#pragma once

#include <string>
#include <cstdint>

namespace woved {

// Configuration structures matching the YAML spec
struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    uint16_t grpc_port = 9090;
    uint16_t http_port = 8080;
    uint16_t metrics_port = 9091;
    uint32_t max_connections = 1000;
    uint32_t worker_threads = 0;  // 0 = auto-detect
};

struct CollectionConfig {
    uint32_t dim = 768;
    std::string metric = "inner_product";  // cosine via normalization
    uint64_t max_vectors = 100000000;  // 100M
    std::string id_type = "uuidv7";
};

struct BTreeConfig {
    float epsilon = 0.5f;
    size_t node_size_kb = 64;
    size_t fanout = 256;
    bool adaptive_epsilon = true;
    float hot_partition_threshold = 0.5f;
    float direct_flush_threshold = 0.8f;
};

struct BufferConfig {
    std::string type = "nvm";  // nvm, mmap, memory
    uint64_t size_bytes = 17179869184;  // 16 GiB
    uint32_t shard_count = 16;
    uint64_t flush_threshold_bytes = 134217728;  // 128 MiB (Lmax)
    uint32_t flush_interval_ms = 100;
    bool dedupe_enabled = true;
};

struct WALConfig {
    bool framed_records = true;
    uint32_t fence_len = 0;  // Zero-length fence
    uint32_t group_commit_ms = 8;
    uint32_t fence_every_ms = 5;
    uint32_t fsync_every_fences = 50;
    uint64_t rotate_bytes = 3221225472;  // 3 GiB
    uint32_t max_files = 10;
    std::string compression = "none";  // none, lz4, zstd
};

struct SegmentConfig {
    uint64_t target_size_vectors = 2000000;  // 2M vectors
    uint32_t max_segments_per_leaf = 8;
    float tombstone_ratio_threshold = 0.2f;
    float merge_bandwidth_limit = 0.3f;  // 30% of device bandwidth
    bool enable_compression = false;
    std::string compression_type = "zstd";
};

struct StorageConfig {
    std::string data_dir = "/var/lib/woved";
    std::string wal_dir = "/var/lib/woved/wal";
    std::string segment_dir = "/var/lib/woved/segments";
    
    BTreeConfig btree;
    BufferConfig buffer;
    WALConfig wal;
    SegmentConfig segment;
};

struct DeltaIndexConfig {
    std::string type = "ivf_flat";
    uint32_t nlist = 1024;
    uint32_t nprobe = 6;
    float sample_p = 0.25f;
    uint32_t list_cap = 2000;
    bool global_centroids = true;
    uint32_t rebuild_interval_hours = 24;
};

struct StableIndexConfig {
    std::string type = "ivf_pq";
    uint32_t nlist = 4096;
    struct PQConfig {
        uint32_t m = 96;
        uint32_t nbits = 8;
        bool use_opq = true;
    } pq;
    uint32_t nprobe = 12;
    uint32_t rerank_factor = 4;
};

struct GlobalIndexConfig {
    std::string type = "ivf";
    uint32_t nlist = 1024;
    uint32_t memory_cache_mb = 512;
};

struct HNSWCacheConfig {
    bool enabled = false;
    uint32_t max_elements = 1000000;
    uint32_t m = 16;
    uint32_t ef_construction = 200;
    uint32_t ef = 50;
};

struct IndexConfig {
    DeltaIndexConfig delta;
    StableIndexConfig stable;
    GlobalIndexConfig global;
    HNSWCacheConfig hnsw_cache;
};

struct FilteringConfig {
    uint64_t bitmap_cache_bytes = 1073741824;  // 1 GiB
    uint64_t per_segment_soft_cap_bytes = 134217728;  // 128 MiB
    bool bloom_filter_enabled = true;
    float bloom_filter_fpp = 0.01f;
    uint32_t tag_dict_size = 50000;
    uint32_t max_tags_per_vector = 16;
    float dense_bitmap_threshold = 0.2f;
};

struct QueryConfig {
    uint32_t timeout_ms = 5000;
    uint32_t max_candidates = 10000;
    uint32_t default_top_k = 10;
    uint32_t max_top_k = 100;
    bool two_phase_enabled = true;
    bool buffer_scan_enabled = true;
    bool prefetch_enabled = true;
    uint32_t prefetch_depth = 2;
};

struct TuningConfig {
    float recall_target = 0.95f;
    bool auto_tune_enabled = true;
    uint32_t nprobe_delta_min = 4;
    uint32_t nprobe_delta_max = 8;
    uint32_t nprobe_stable_min = 8;
    uint32_t nprobe_stable_max = 16;
    bool persist_decisions = true;
    uint32_t decision_window_hours = 1;
};

struct IOConfig {
    bool use_iouring = true;
    struct IOUringConfig {
        bool sqpoll = true;
        uint32_t queue_depth = 32;
        bool register_files = true;
        uint32_t link_timeout_ms = 5;
    } iouring;
    bool use_direct_io = false;
    uint32_t prefetch_distance = 4;
    uint32_t merge_bandwidth_limit_mbps = 500;
    uint32_t read_ahead_kb = 8192;
};

struct NUMAConfig {
    bool enabled = true;
    bool bind_threads = true;
    bool replicate_centroids = true;
    std::string memory_policy = "interleave";  // bind, interleave, preferred
};

struct MonitoringConfig {
    struct PrometheusConfig {
        bool enabled = true;
        uint32_t scrape_interval_s = 15;
    } prometheus;
    // Metrics list would be handled separately
};

struct LimitsConfig {
    uint32_t max_upsert_batch = 10000;
    uint32_t max_query_batch = 100;
    uint64_t max_request_size_bytes = 104857600;  // 100 MiB
    uint32_t max_memory_gb = 64;
    uint32_t max_cpu_percent = 85;
    uint32_t max_disk_usage_percent = 90;
};

struct RecoveryConfig {
    uint32_t checkpoint_interval_s = 60;
    uint32_t max_recovery_time_s = 30;
    uint32_t parallel_recovery_threads = 4;
    bool verify_checksums = true;
};

struct ExperimentalConfig {
    bool gpu_acceleration = false;
    uint32_t gpu_device_id = 0;
    bool learned_index = false;
    bool adaptive_sampling = true;
    bool connectivity_aware_layout = true;
    bool vector_compression = false;
};

struct LoggingConfig {
    std::string level = "info";  // debug, info, warn, error
    std::string file = "/var/log/woved/woved.log";
    uint32_t max_size_mb = 100;
    uint32_t max_files = 10;
    bool console = true;
    bool structured = true;
};

// Main configuration structure
struct Config {
    ServerConfig server;
    CollectionConfig collection;
    StorageConfig storage;
    IndexConfig index;
    FilteringConfig filtering;
    QueryConfig query;
    TuningConfig tuning;
    IOConfig io;
    NUMAConfig numa;
    MonitoringConfig monitoring;
    LimitsConfig limits;
    RecoveryConfig recovery;
    ExperimentalConfig experimental;
    LoggingConfig logging;
    
    // Version info
    std::string version = "1.0";
};

// Global configuration instance
extern Config g_config;

// Configuration loading from YAML
bool loadConfig(const std::string& path);
bool validateConfig(const Config& config);
void applyDefaults(Config& config);

} // namespace woved
