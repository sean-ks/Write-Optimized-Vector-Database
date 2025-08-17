#include "include/woved/types.h"
#include <memory>
#include <functional>

namespace woved::storage {

class BEpsilonNode;
class MessageBuffer;
class SegmentManager;

struct BTreeConfig {
    size_t node_size_bytes = 65536;  // 64KB
    size_t fanout = 256;
    float epsilon = 0.5f;
    bool adaptive_epsilon = true;
    float hot_partition_threshold = 0.5f;
    float direct_flush_threshold = 0.8f;
};

class BEpsilonTree {
public:
    explicit BEpsilonTree(const BTreeConfig& config, 
                          std::shared_ptr<SegmentManager> segment_mgr);
    ~BEpsilonTree();

    // Write operations
    void insert(const VectorEntry& entry);
    void upsert(const VectorEntry& entry);
    void remove(const VectorId& id, const VectorIdHash& id_hash);
    
    // Flush control
    void flush(bool force = false);
    void flushNode(BEpsilonNode* node);
    
    // Recovery and persistence
    void checkpoint();
    void recover(const std::string& manifest_path);
    
    // Statistics
    struct Stats {
        size_t total_nodes;
        size_t leaf_nodes;
        size_t messages_buffered;
        size_t bytes_buffered;
        float avg_fill_ratio;
        size_t flush_count;
    };
    Stats getStats() const;
    
    // Adaptive tuning
    void adjustEpsilon(float new_epsilon);
    void enableAdaptiveMode(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}