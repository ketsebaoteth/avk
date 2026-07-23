#pragma once

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <string_view>

/**
 * @brief Global, lightweight, compile-time RGBA color token.
 * Implicitly converts to glm::vec4 for direct use with your Modifier code.
 */
struct AtomicColor {
  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

  constexpr AtomicColor() = default;
  constexpr AtomicColor(float r, float g, float b, float a = 1.0f)
      : r(r), g(g), b(b), a(a) {}

  // Implicit conversion operator directly to glm::vec4
  operator glm::vec4() const { return glm::vec4(r, g, b, a); }

  // Brightness multipliers / programmatic tint modifications
  constexpr AtomicColor operator*(float scalar) const {
    return AtomicColor(std::min(r * scalar, 1.0f), std::min(g * scalar, 1.0f),
                       std::min(b * scalar, 1.0f), a);
  }

  // Returns a modified alpha opacity duplicate (0.0 to 1.0)
  constexpr AtomicColor alpha(float newAlpha) const {
    return AtomicColor(r, g, b, newAlpha);
  }

  // Linear interpolation (mix) evaluated completely at compile time
  constexpr AtomicColor mix(const AtomicColor &other, float t) const {
    return AtomicColor(r + (other.r - r) * t, g + (other.g - g) * t,
                       b + (other.b - b) * t, a + (other.a - a) * t);
  }
};

// -----------------------------------------------------------------
// 1. DYNAMIC COMPILE-TIME PALETTE GENERATOR
// -----------------------------------------------------------------
struct AtomicPalette {
  std::array<AtomicColor, 10>
      shades; // 50, 100, 200, 300, 400, 500, 600, 700, 800, 900

  constexpr AtomicPalette() = default;

  // Auto-generates all 10 shades at compile-time from a single base color!
  constexpr AtomicPalette(const AtomicColor &base) {
    shades[0] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.92f); // 50
    shades[1] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.80f); // 100
    shades[2] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.60f); // 200
    shades[3] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.40f); // 300
    shades[4] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.20f); // 400
    shades[5] = base;                                           // 500 (Base)
    shades[6] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.20f); // 600
    shades[7] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.40f); // 700
    shades[8] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.60f); // 800
    shades[9] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.85f); // 900
  }

  // Allows manual hardcoded overrides if needed
  constexpr AtomicPalette(const std::array<AtomicColor, 10> &customShades)
      : shades(customShades) {}

  // Array index lookup mapping weight values (e.g. gray[900])
  constexpr AtomicColor operator[](size_t weight) const {
    switch (weight) {
    case 50:
      return shades[0];
    case 100:
      return shades[1];
    case 200:
      return shades[2];
    case 300:
      return shades[3];
    case 400:
      return shades[4];
    case 500:
      return shades[5];
    case 600:
      return shades[6];
    case 700:
      return shades[7];
    case 800:
      return shades[8];
    case 900:
      return shades[9];
    default:
      return shades[5]; // Baseline fallback
    }
  }
};

// -----------------------------------------------------------------
// 2. PALETTE INSTANCES (Completely compiled at compile-time)
// -----------------------------------------------------------------
namespace Colors {
inline constexpr AtomicColor white = AtomicColor(1.0f, 1.0f, 1.0f, 1.0f);
inline constexpr AtomicColor transparent = AtomicColor(0.0f, 0.0f, 0.0f, 0.0f);
inline constexpr AtomicColor orange = AtomicColor(1.0f, 0.5f, 0.0f, 1.0f);

// Dynamic Compile-Time Palettes (Just pass the base 500 color!)
inline constexpr AtomicPalette gray =
    AtomicPalette(AtomicColor(0.25f, 0.25f, 0.25f));
inline constexpr AtomicPalette black =
    AtomicPalette(AtomicColor(0.10f, 0.10f, 0.11f));

inline constexpr AtomicPalette blue =
    AtomicPalette(AtomicColor(0.20f, 0.50f, 0.90f));
inline constexpr AtomicPalette red =
    AtomicPalette(AtomicColor(0.90f, 0.20f, 0.20f));

// Custom brand colors
inline constexpr AtomicColor bajajGreen = AtomicColor(0.1f, 0.6f, 0.2f, 1.0f);
inline constexpr AtomicColor soapBlue = AtomicColor(0.2f, 0.5f, 0.9f, 1.0f);
} // namespace Colors

// -----------------------------------------------------------------
// 3. UNIFIED APP DESIGN THEME STRUCT
// -----------------------------------------------------------------
struct AtomicTheme {
  AtomicColor background;
  AtomicColor surface;
  AtomicColor text;
  AtomicColor accent;
  AtomicColor buttonPressed;
  AtomicColor buttonHover;

  static constexpr AtomicTheme Dark() {
    return AtomicTheme{.background = Colors::gray[900],
                       .surface = Colors::gray[800],
                       .text = Colors::white,
                       .accent = Colors::orange,
                       .buttonPressed = Colors::gray[700],
                       .buttonHover = Colors::gray[600]};
  }

  static constexpr AtomicTheme Light() {
    return AtomicTheme{.background = Colors::white,
                       .surface = Colors::gray[100],
                       .text = Colors::gray[900],
                       .accent = Colors::soapBlue,
                       .buttonPressed = Colors::gray[300],
                       .buttonHover = Colors::gray[200]};
  }
};

// -----------------------------------------------------------------
// 4. GLOBAL COMPILE-TIME HEX LITERAL PARSER
// -----------------------------------------------------------------
namespace detail_color {
constexpr int charToHex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return 0;
}
} // namespace detail_color

constexpr AtomicColor operator""_hex(const char *str, size_t len) {
  std::string_view sv(str, len);
  if (!sv.empty() && sv[0] == '#') {
    sv.remove_prefix(1);
  }

  if (sv.length() == 6) {
    int r =
        (detail_color::charToHex(sv[0]) << 4) | detail_color::charToHex(sv[1]);
    int g =
        (detail_color::charToHex(sv[2]) << 4) | detail_color::charToHex(sv[3]);
    int b =
        (detail_color::charToHex(sv[4]) << 4) | detail_color::charToHex(sv[5]);
    return AtomicColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
  } else if (sv.length() == 8) {
    int r =
        (detail_color::charToHex(sv[0]) << 4) | detail_color::charToHex(sv[1]);
    int g =
        (detail_color::charToHex(sv[2]) << 4) | detail_color::charToHex(sv[3]);
    int b =
        (detail_color::charToHex(sv[4]) << 4) | detail_color::charToHex(sv[5]);
    int a =
        (detail_color::charToHex(sv[6]) << 4) | detail_color::charToHex(sv[7]);
    return AtomicColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }
  return AtomicColor(0.0f, 0.0f, 0.0f, 1.0f);
}
