#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <variant>
#include <optional>
#include <span>
#include <atomic>

namespace woved {

// Core type definitions
using VectorId = std::string;
using VectorIdHash = uint64_t;
using Dimension = uint32_t;
using Score = float;
using Timestamp = std::chrono::microseconds;
using Epoch = uint64_t;

// Vector data types
using Vector = std::vector<float>;
using VectorView = std::span<const float>;

// Metadata types
using TenantId = std::string;
using TenantHash = uint64_t;
using NamespaceId = std::string;
using NamespaceHash = uint64_t;
using TagId = uint32_t;
using TagSet = std::vector<TagId>;

// Centroid assignment for flush coherence
using CentroidId = uint16_t;

enum class Metric {
    INNER_PRODUCT,  // Cosine via L2-norm at ingest
    L2,
    COSINE
};

enum class OperationType {
    INSERT,
    UPSERT,
    DELETE
};

// Core vector entry structure
struct VectorEntry {
    VectorId id;
    VectorIdHash id_hash;
    Vector vector;
    TenantId tenant;
    TenantHash tenant_hash;
    NamespaceId namespace_id;
    NamespaceHash namespace_hash;
    TagSet tags;
    Timestamp created_at;
    Timestamp updated_at;
    CentroidId centroid_id = 0;  // Pre-computed for flush coherence
    bool deleted = false;
};

// Query structures
struct QueryRequest {
    Vector query;
    uint32_t top_k = 10;
    TenantId tenant;
    NamespaceId namespace_id;
    std::vector<std::string> tags_any;
    std::optional<uint32_t> nprobe;
    std::optional<float> sample_p;
};

struct QueryResult {
    VectorId id;
    Score score;
    TagSet tags;
    std::string segment_id;
};

// WAL record structure
struct WALRecord {
    uint32_t length;
    uint32_t crc32c;
    Epoch epoch;
    OperationType type;
    VectorEntry entry;
};

// Segment descriptor
struct SegmentDescriptor {
    std::string segment_id;
    std::string file_path;
    uint64_t num_vectors;
    VectorIdHash min_id_hash;
    VectorIdHash max_id_hash;
    Epoch min_epoch;
    Epoch max_epoch;
    float tombstone_ratio;
    Timestamp created_at;
    bool is_stable = false;  // false = delta, true = stable
};

// Message for B-epsilon tree
struct BTreeMessage {
    OperationType op;
    VectorEntry entry;
    Epoch epoch;
    Timestamp timestamp;
};

// Constants from specification
namespace constants {
    constexpr uint32_t DEFAULT_DIMENSION = 768;
    constexpr uint32_t DEFAULT_TOP_K = 10;
    constexpr uint32_t MAX_TOP_K = 100;
    
    // Segment sizing
    constexpr uint64_t DEFAULT_VECTORS_PER_SEGMENT = 2000000;  // 2M vectors
    constexpr uint64_t SEGMENT_CHUNK_SIZE = 1048576;  // 1 MiB chunks
    
    // WAL settings
    constexpr uint32_t WAL_GROUP_COMMIT_MS = 8;
    constexpr uint64_t WAL_FILE_SIZE = 4294967296;  // 4 GiB
    
    // Buffer settings
    constexpr uint64_t MAX_BUFFER_BYTES = 17179869184;  // 16 GiB
    
    // Index parameters
    constexpr uint32_t GLOBAL_IVF_NLIST = 1024;
    constexpr uint32_t DELTA_IVF_NLIST = 1024;
    constexpr uint32_t STABLE_IVF_NLIST = 4096;
    constexpr uint32_t STABLE_PQ_M = 96;
    constexpr uint32_t STABLE_PQ_NBITS = 8;
    
    // Performance targets
    constexpr float TARGET_RECALL = 0.95f;
    constexpr uint32_t TARGET_P99_MS = 150;
    constexpr uint32_t TARGET_INGEST_QPS = 50000;
    constexpr float MAX_DELTA_FRACTION = 0.05f;
    constexpr float MAX_WRITE_AMP_P50 = 2.3f;
    constexpr float MAX_WRITE_AMP_P95 = 2.6f;
}

}