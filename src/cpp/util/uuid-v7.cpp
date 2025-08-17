#include "uuid-v7.h"
#include <chrono>
#include <stdexcept>
#include <array>
#include <fmt/core.h>

namespace woved::util {

UuidV7Generator::UuidV7Generator() {
    std::random_device rd;
    std::seed_seq ss{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    rng_.seed(ss);
}

std::string UuidV7Generator::generate() {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto epoch_ms = duration_cast<milliseconds>(now.time_since_epoch()).count();

    if (epoch_ms == last_ms_) {
        sequence_++;
    } else {
        last_ms_ = epoch_ms;
        sequence_ = 0;
    }

    // UUIDv7 structure:
    // 48 bits: unix_ts_ms
    //  4 bits: version (0111)
    // 12 bits: rand_a (sequence counter for monotonicity)
    //  2 bits: variant (10)
    // 62 bits: rand_b

    uint64_t unix_ts_ms = last_ms_;

    // rand_a contains the sequence for monotonicity within the same millisecond
    uint16_t rand_a = sequence_;

    // rand_b is random
    uint64_t rand_b = dist_(rng_);

    std::array<uint8_t, 16> bytes;

    bytes[0] = (unix_ts_ms >> 40) & 0xFF;
    bytes[1] = (unix_ts_ms >> 32) & 0xFF;
    bytes[2] = (unix_ts_ms >> 24) & 0xFF;
    bytes[3] = (unix_ts_ms >> 16) & 0xFF;
    bytes[4] = (unix_ts_ms >> 8) & 0xFF;
    bytes[5] = unix_ts_ms & 0xFF;

    // Version and rand_a
    bytes[6] = 0x70 | ((rand_a >> 8) & 0x0F);
    bytes[7] = rand_a & 0xFF;

    // Variant and rand_b
    bytes[8] = 0x80 | ((rand_b >> 56) & 0x3F);
    bytes[9] = (rand_b >> 48) & 0xFF;
    bytes[10] = (rand_b >> 40) & 0xFF;
    bytes[11] = (rand_b >> 32) & 0xFF;
    bytes[12] = (rand_b >> 24) & 0xFF;
    bytes[13] = (rand_b >> 16) & 0xFF;
    bytes[14] = (rand_b >> 8) & 0xFF;
    bytes[15] = rand_b & 0xFF;

    return fmt::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

} // namespace woved::util
