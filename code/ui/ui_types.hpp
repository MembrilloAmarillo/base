#ifndef UI_TYPES_HPP
#define UI_TYPES_HPP

#include <cmath>
#include <cstdint>

namespace ui {

using Id = std::uint64_t;
using TextureId = std::uint64_t;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(Vec2 p) const {
        return p.x >= x && p.x <= (x + w) && p.y >= y && p.y <= (y + h);
    }
};

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;

    static Color RGBA(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_) {
        return Color{r_, g_, b_, a_};
    }

    static Color Hex(std::uint32_t rgb, std::uint8_t alpha = 255) {
        return Color{
            static_cast<std::uint8_t>((rgb >> 16U) & 0xffU),
            static_cast<std::uint8_t>((rgb >> 8U) & 0xffU),
            static_cast<std::uint8_t>(rgb & 0xffU),
            alpha
        };
    }

    // Packed as ABGR8 for direct upload.
    std::uint32_t PackABGR() const {
        return (static_cast<std::uint32_t>(a) << 24U) |
               (static_cast<std::uint32_t>(b) << 16U) |
               (static_cast<std::uint32_t>(g) << 8U) |
               static_cast<std::uint32_t>(r);
    }
};

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float Clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

inline float Approach(float current, float target, float dt, float speed) {
    const float k = 1.0f - std::exp(-speed * dt);
    return Lerp(current, target, Clamp01(k));
}

}  // namespace ui

#endif  // UI_TYPES_HPP
