#ifndef UI_ID_HPP
#define UI_ID_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ui_types.hpp"

namespace ui {

inline Id HashBytes(Id seed, const void* bytes, std::size_t len) {
    // FNV-1a 64-bit.
    constexpr std::uint64_t fnv_offset = 1469598103934665603ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;

    std::uint64_t h = seed ^ fnv_offset;
    const std::uint8_t* at = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<std::uint64_t>(at[i]);
        h *= fnv_prime;
    }
    return h;
}

inline Id HashId(Id parent, std::string_view label) {
    return HashBytes(parent, label.data(), label.size());
}

inline Id HashId(Id parent, const void* ptr) {
    return HashBytes(parent, &ptr, sizeof(ptr));
}

inline Id HashId(Id parent, std::uint64_t integer_id) {
    return HashBytes(parent, &integer_id, sizeof(integer_id));
}

}  // namespace ui

#endif  // UI_ID_HPP
