#ifndef WOVED_UTIL_HASH_H
#define WOVED_UTIL_HASH_H

#include <string_view>
#include <cstdint>
#include "core/types.h"
#include "xxhash.h"

namespace woved::util {

/**
 * @brief Computes the 64-bit xxHash of a vector ID.
 * * This is the canonical hash function used for routing entries within the B-epsilon tree.
 * * @param id The vector ID to hash.
 * @return The 64-bit hash value.
 */
inline VectorIdHash hash_id(std::string_view id) {
    // Seed is 0, as per standard xxHash64 usage.
    return XXH64(id.data(), id.length(), 0);
}

} // namespace woved::util

#endif // WOVED_UTIL_HASH_H