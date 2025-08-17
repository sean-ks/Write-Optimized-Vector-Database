#ifndef WOVED_UTIL_UUID_H
#define WOVED_UTIL_UUID_H

#include <string>
#include <cstdint>
#include <random>

namespace woved::util {

/**
 * @brief A class for generating version 7 UUIDs (time-ordered).
 */
class UuidV7Generator {
public:
    UuidV7Generator();

    /**
     * @brief Generates a new UUIDv7.
     * @return A string representation of the UUID.
     */
    std::string generate();

private:
    uint64_t last_ms_ = 0;
    uint16_t sequence_ = 0;
    std::mt19937_64 rng_;
    std::uniform_int_distribution<uint64_t> dist_;
};

} // namespace woved::util

#endif // WOVED_UTIL_UUID_H
