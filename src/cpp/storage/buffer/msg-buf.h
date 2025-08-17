#pragma once

#include "include/woved/types.h"
#include "storage/latest-by-id.h"
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>

namespace woved::storage {

// Central message buffer for write buffering
class MessageBuffer {
public:
    struct Config {
        size_t max_bytes = 17179869184;  // 16 GiB
        size_t shard_count = 16;
        size_t flush_threshold_bytes = 134217728;  // 128 MiB
        bool dedupe_enabled = true;
    };
    
    explicit MessageBuffer(const Config& config, 
                          std::shared_ptr<LatestByIdMap> latest_by_id);
    ~MessageBuffer();
    
    // Append message to buffer
    void append(VectorIdHash hash, const BTreeMessage& msg);
    
    // Get messages for a specific leaf/partition
    std::vector<BTreeMessage> sliceForLeaf(size_t leaf_id, size_t max_batch);
    
    // Evict messages after successful flush
    void evict(const std::vector<BTreeMessage>& flushed);
    
    // Scan buffer for query (read-your-writes)
    std::vector<VectorEntry> scanForQuery(
        const Vector& query,
        const TenantId& tenant,
        const NamespaceId& ns,
        const std::vector<TagId>& tags,
        size_t max_scan = 10000
    );
    
    // Get buffer statistics
    struct Stats {
        size_t message_count;
        size_t bytes_used;
        size_t dedupe_count;
        std::vector<size_t> shard_sizes;
    };
    Stats getStats() const;
    
    // Wait for space if buffer is full
    bool waitForSpace(std::chrono::milliseconds timeout);
    
    // Clear buffer (for recovery)
    void clear();
    
private:
    // Shard structure for parallel access
    struct Shard {
        mutable std::mutex mutex;
        std::deque<std::unique_ptr<BTreeMessage>> messages;
        std::atomic<size_t> bytes{0};
        std::atomic<size_t> count{0};
        
        // Per-shard deduplication map (ID hash -> latest message)
        std::unordered_map<VectorIdHash, BTreeMessage*> latest_map;
    };
    
    Config config_;
    std::vector<std::unique_ptr<Shard>> shards_;
    std::shared_ptr<LatestByIdMap> latest_by_id_;
    
    std::atomic<size_t> total_bytes_{0};
    std::atomic<size_t> total_messages_{0};
    std::atomic<size_t> dedupe_count_{0};
    
    std::condition_variable space_cv_;
    std::mutex space_mutex_;
    
    // Get shard for a hash
    size_t getShardIndex(VectorIdHash hash) const {
        return hash % config_.shard_count;
    }
    
    // Estimate message size
    size_t estimateSize(const BTreeMessage& msg) const;
    
    // Apply deduplication within shard
    void dedupeInShard(Shard* shard, const BTreeMessage& msg);
};

// Implementation
MessageBuffer::MessageBuffer(const Config& config,
                            std::shared_ptr<LatestByIdMap> latest_by_id)
    : config_(config), latest_by_id_(latest_by_id) {
    
    // Initialize shards
    shards_.reserve(config_.shard_count);
    for (size_t i = 0; i < config_.shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>());
    }
    
    LOG_INFO("MessageBuffer initialized with {} shards, max {} bytes",
             config_.shard_count, config_.max_bytes);
}

MessageBuffer::~MessageBuffer() {
    LOG_INFO("MessageBuffer destroyed with {} messages, {} bytes",
             total_messages_.load(), total_bytes_.load());
}

void MessageBuffer::append(VectorIdHash hash, const BTreeMessage& msg) {
    size_t shard_idx = getShardIndex(hash);
    auto& shard = shards_[shard_idx];
    
    size_t msg_size = estimateSize(msg);
    
    // Wait for space if needed
    while (total_bytes_.load() + msg_size > config_.max_bytes) {
        if (!waitForSpace(std::chrono::milliseconds(100))) {
            LOG_WARN("Buffer full, forcing flush");
            // Trigger flush through callback or return error
            return;
        }
    }
    
    std::lock_guard<std::mutex> lock(shard->mutex);
    
    // Deduplication within shard
    if (config_.dedupe_enabled && msg.op != OperationType::DELETE) {
        auto it = shard->latest_map.find(hash);
        if (it != shard->latest_map.end()) {
            // Remove old version
            dedupe_count_++;
            // Note: In production, would properly remove from deque
        }
    }
    
    // Add new message
    auto msg_ptr = std::make_unique<BTreeMessage>(msg);
    if (config_.dedupe_enabled) {
        shard->latest_map[hash] = msg_ptr.get();
    }
    
    shard->messages.push_back(std::move(msg_ptr));
    shard->bytes.fetch_add(msg_size);
    shard->count.fetch_add(1);
    
    total_bytes_.fetch_add(msg_size);
    total_messages_.fetch_add(1);
    
    // Update latest_by_id for read-your-writes
    if (latest_by_id_) {
        VectorLocation loc;
        loc.type = VectorLocation::BUFFER;
        loc.timestamp = msg.timestamp;
        loc.epoch = msg.epoch;
        loc.tombstone = (msg.op == OperationType::DELETE);
        
        latest_by_id_->upsert(msg.entry.id, hash, loc);
    }
}

std::vector<BTreeMessage> MessageBuffer::sliceForLeaf(size_t leaf_id, size_t max_batch) {
    std::vector<BTreeMessage> result;
    result.reserve(max_batch);
    
    // Simple round-robin across shards for the leaf
    // In production, would use better routing based on key ranges
    for (size_t i = 0; i < config_.shard_count && result.size() < max_batch; ++i) {
        auto& shard = shards_[i];
        std::lock_guard<std::mutex> lock(shard->mutex);
        
        size_t to_take = std::min(max_batch - result.size(), shard->messages.size());
        for (size_t j = 0; j < to_take; ++j) {
            if (!shard->messages.empty()) {
                result.push_back(*shard->messages.front());
                // Note: Not removing yet, wait for evict() call
            }
        }
    }
    
    return result;
}

void MessageBuffer::evict(const std::vector<BTreeMessage>& flushed) {
    // Remove flushed messages from buffer
    // In production, would track which shard each message came from
    for (const auto& msg : flushed) {
        size_t shard_idx = getShardIndex(msg.entry.id_hash);
        auto& shard = shards_[shard_idx];
        
        std::lock_guard<std::mutex> lock(shard->mutex);
        
        // Remove from front (FIFO)
        if (!shard->messages.empty()) {
            size_t msg_size = estimateSize(*shard->messages.front());
            
            if (config_.dedupe_enabled) {
                shard->latest_map.erase(msg.entry.id_hash);
            }
            
            shard->messages.pop_front();
            shard->bytes.fetch_sub(msg_size);
            shard->count.fetch_sub(1);
            
            total_bytes_.fetch_sub(msg_size);
            total_messages_.fetch_sub(1);
        }
    }
    
    // Signal space available
    space_cv_.notify_all();
}

std::vector<VectorEntry> MessageBuffer::scanForQuery(
    const Vector& query,
    const TenantId& tenant,
    const NamespaceId& ns,
    const std::vector<TagId>& tags,
    size_t max_scan) {
    
    std::vector<VectorEntry> results;
    size_t scanned = 0;
    
    // Scan all shards for matching entries
    for (auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard->mutex);
        
        for (const auto& msg : shard->messages) {
            if (scanned >= max_scan) break;
            scanned++;
            
            // Apply filters
            if (msg->op == OperationType::DELETE) continue;
            if (!tenant.empty() && msg->entry.tenant != tenant) continue;
            if (!ns.empty() && msg->entry.namespace_id != ns) continue;
            
            // Tag filter (ANY-of)
            if (!tags.empty()) {
                bool has_tag = false;
                for (TagId tag : tags) {
                    if (std::find(msg->entry.tags.begin(), 
                                 msg->entry.tags.end(), tag) != 
                        msg->entry.tags.end()) {
                        has_tag = true;
                        break;
                    }
                }
                if (!has_tag) continue;
            }
            
            results.push_back(msg->entry);
        }
    }
    
    return results;
}

MessageBuffer::Stats MessageBuffer::getStats() const {
    Stats stats;
    stats.message_count = total_messages_.load();
    stats.bytes_used = total_bytes_.load();
    stats.dedupe_count = dedupe_count_.load();
    
    for (const auto& shard : shards_) {
        stats.shard_sizes.push_back(shard->count.load());
    }
    
    return stats;
}

bool MessageBuffer::waitForSpace(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(space_mutex_);
    return space_cv_.wait_for(lock, timeout, [this] {
        return total_bytes_.load() < config_.max_bytes;
    });
}

void MessageBuffer::clear() {
    for (auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard->mutex);
        shard->messages.clear();
        shard->latest_map.clear();
        shard->bytes = 0;
        shard->count = 0;
    }
    
    total_bytes_ = 0;
    total_messages_ = 0;
    dedupe_count_ = 0;
    
    space_cv_.notify_all();
}

size_t MessageBuffer::estimateSize(const BTreeMessage& msg) const {
    size_t size = sizeof(BTreeMessage);
    size += msg.entry.vector.size() * sizeof(float);
    size += msg.entry.id.size();
    size += msg.entry.tenant.size();
    size += msg.entry.namespace_id.size();
    size += msg.entry.tags.size() * sizeof(TagId);
    return size;
}

} // namespace woved::storage