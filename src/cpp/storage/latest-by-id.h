// src/cpp/storage/latest_by_id.h
#pragma once

#include "include/woved/types.h"
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <optional>

namespace woved::storage {

// Entry location tracking
struct VectorLocation {
    enum LocationType {
        BUFFER,      // In memory buffer
        SEGMENT,     // In on-disk segment
        DELETED      // Tombstoned
    };
    
    LocationType type;
    std::string segment_id;  // Empty if in buffer
    uint32_t local_id;        // Local ID within segment
    Timestamp timestamp;
    Epoch epoch;
    bool tombstone = false;
};

// Thread-safe latest-by-id map for deduplication and version tracking
class LatestByIdMap {
public:
    LatestByIdMap();
    ~LatestByIdMap();
    
    // Update location for a vector ID
    void upsert(const VectorId& id, const VectorIdHash& id_hash, 
                const VectorLocation& location);
    
    // Mark as deleted (tombstone)
    void markDeleted(const VectorId& id, const VectorIdHash& id_hash,
                     Timestamp timestamp, Epoch epoch);
    
    // Get latest location for an ID
    std::optional<VectorLocation> getLatest(const VectorId& id) const;
    std::optional<VectorLocation> getLatestByHash(VectorIdHash id_hash) const;
    
    // Check if ID exists and is not deleted
    bool exists(const VectorId& id) const;
    bool existsByHash(VectorIdHash id_hash) const;
    
    // Remove entries for a segment (after compaction)
    void removeSegmentEntries(const std::string& segment_id);
    
    // Move entries from buffer to segment (after flush)
    void moveToSegment(const std::vector<VectorId>& ids,
                       const std::string& segment_id,
                       Epoch epoch);
    
    // Get statistics
    struct Stats {
        size_t total_entries;
        size_t buffer_entries;
        size_t segment_entries;
        size_t tombstone_entries;
    };
    Stats getStats() const;
    
    // Clear all entries (for testing/recovery)
    void clear();
    
    // Rebuild from segments (recovery)
    void rebuild(const std::vector<SegmentDescriptor>& segments);
    
private:
    // Internal entry structure
    struct Entry {
        VectorId id;
        VectorIdHash id_hash;
        VectorLocation location;
        std::atomic<uint64_t> version;  // For optimistic concurrency
    };
    
    // Primary map: ID hash -> Entry
    mutable std::shared_mutex map_mutex_;
    std::unordered_map<VectorIdHash, std::unique_ptr<Entry>> id_map_;
    
    // Secondary index: ID string -> hash (for exact lookups)
    std::unordered_map<VectorId, VectorIdHash> id_to_hash_;
    
    // Statistics counters
    std::atomic<size_t> buffer_count_{0};
    std::atomic<size_t> segment_count_{0};
    std::atomic<size_t> tombstone_count_{0};
    
    // Version counter for optimistic concurrency
    std::atomic<uint64_t> global_version_{0};
    
    // Helper to update an entry
    void updateEntry(Entry* entry, const VectorLocation& location);
};

// Implementation
LatestByIdMap::LatestByIdMap() {
    LOG_DEBUG("LatestByIdMap initialized");
}

LatestByIdMap::~LatestByIdMap() {
    LOG_DEBUG("LatestByIdMap destroyed with {} entries", id_map_.size());
}

void LatestByIdMap::upsert(const VectorId& id, const VectorIdHash& id_hash,
                           const VectorLocation& location) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    auto it = id_map_.find(id_hash);
    if (it != id_map_.end()) {
        // Update existing entry
        auto& entry = it->second;
        
        // Update counters
        if (entry->location.type == VectorLocation::BUFFER) buffer_count_--;
        if (entry->location.type == VectorLocation::SEGMENT) segment_count_--;
        if (entry->location.tombstone) tombstone_count_--;
        
        updateEntry(entry.get(), location);
    } else {
    void LatestByIdMap::upsert(const VectorId& id, const VectorIdHash& id_hash,
                           const VectorLocation& location) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    auto it = id_map_.find(id_hash);
    if (it != id_map_.end()) {
        // Update existing entry
        auto& entry = it->second;
        
        // Update counters
        if (entry->location.type == VectorLocation::BUFFER) buffer_count_--;
        if (entry->location.type == VectorLocation::SEGMENT) segment_count_--;
        if (entry->location.tombstone) tombstone_count_--;
        
        updateEntry(entry.get(), location);
    } else {
        // Create new entry
        auto entry = std::make_unique<Entry>();
        entry->id = id;
        entry->id_hash = id_hash;
        entry->location = location;
        entry->version = global_version_.fetch_add(1);
        
        id_map_[id_hash] = std::move(entry);
        id_to_hash_[id] = id_hash;
    }
    
    // Update counters for new location
    if (location.type == VectorLocation::BUFFER) buffer_count_++;
    if (location.type == VectorLocation::SEGMENT) segment_count_++;
    if (location.tombstone) tombstone_count_++;
}

void LatestByIdMap::markDeleted(const VectorId& id, const VectorIdHash& id_hash,
                                Timestamp timestamp, Epoch epoch) {
    VectorLocation location;
    location.type = VectorLocation::DELETED;
    location.timestamp = timestamp;
    location.epoch = epoch;
    location.tombstone = true;
    
    upsert(id, id_hash, location);
}

std::optional<VectorLocation> LatestByIdMap::getLatest(const VectorId& id) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex_);
    
    auto hash_it = id_to_hash_.find(id);
    if (hash_it == id_to_hash_.end()) {
        return std::nullopt;
    }
    
    return getLatestByHash(hash_it->second);
}

std::optional<VectorLocation> LatestByIdMap::getLatestByHash(VectorIdHash id_hash) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex_);
    
    auto it = id_map_.find(id_hash);
    if (it != id_map_.end()) {
        return it->second->location;
    }
    
    return std::nullopt;
}

bool LatestByIdMap::exists(const VectorId& id) const {
    auto location = getLatest(id);
    return location.has_value() && !location->tombstone;
}

bool LatestByIdMap::existsByHash(VectorIdHash id_hash) const {
    auto location = getLatestByHash(id_hash);
    return location.has_value() && !location->tombstone;
}

void LatestByIdMap::removeSegmentEntries(const std::string& segment_id) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    for (auto it = id_map_.begin(); it != id_map_.end(); ) {
        if (it->second->location.type == VectorLocation::SEGMENT &&
            it->second->location.segment_id == segment_id) {
            
            id_to_hash_.erase(it->second->id);
            segment_count_--;
            if (it->second->location.tombstone) tombstone_count_--;
            
            it = id_map_.erase(it);
        } else {
            ++it;
        }
    }
}

void LatestByIdMap::moveToSegment(const std::vector<VectorId>& ids,
                                  const std::string& segment_id,
                                  Epoch epoch) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    for (const auto& id : ids) {
        auto hash_it = id_to_hash_.find(id);
        if (hash_it != id_to_hash_.end()) {
            auto entry_it = id_map_.find(hash_it->second);
            if (entry_it != id_map_.end()) {
                auto& entry = entry_it->second;
                
                // Update counters
                if (entry->location.type == VectorLocation::BUFFER) {
                    buffer_count_--;
                    segment_count_++;
                }
                
                // Update location
                entry->location.type = VectorLocation::SEGMENT;
                entry->location.segment_id = segment_id;
                entry->location.epoch = epoch;
                entry->version = global_version_.fetch_add(1);
            }
        }
    }
}

LatestByIdMap::Stats LatestByIdMap::getStats() const {
    std::shared_lock<std::shared_mutex> lock(map_mutex_);
    
    Stats stats;
    stats.total_entries = id_map_.size();
    stats.buffer_entries = buffer_count_.load();
    stats.segment_entries = segment_count_.load();
    stats.tombstone_entries = tombstone_count_.load();
    
    return stats;
}

void LatestByIdMap::clear() {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    id_map_.clear();
    id_to_hash_.clear();
    buffer_count_ = 0;
    segment_count_ = 0;
    tombstone_count_ = 0;
}

void LatestByIdMap::rebuild(const std::vector<SegmentDescriptor>& segments) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    
    // Clear existing entries
    clear();
    
    // Rebuild from segments (would scan segment headers in production)
    for (const auto& seg : segments) {
        LOG_DEBUG("Rebuilding latest_by_id from segment {}", seg.segment_id);
        // In production, would scan segment's row table to get all IDs
        // and their timestamps, keeping only the latest for each ID
    }
}

void LatestByIdMap::updateEntry(Entry* entry, const VectorLocation& location) {
    entry->location = location;
    entry->version = global_version_.fetch_add(1);
}

} // namespace woved::storage