#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <string_view>

/**
 * @brief Global, lightweight RGBA color token.
 * Implicitly converts to glm::vec4 for direct use with your Modifier code.
 */
struct AtomicColor {
  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

  constexpr AtomicColor() = default;
  constexpr AtomicColor(float r, float g, float b, float a = 1.0f)
      : r(r), g(g), b(b), a(a) {}

  operator glm::vec4() const { return glm::vec4(r, g, b, a); }

  constexpr AtomicColor operator*(float scalar) const {
    return AtomicColor(std::min(r * scalar, 1.0f), std::min(g * scalar, 1.0f),
                       std::min(b * scalar, 1.0f), a);
  }

  constexpr AtomicColor alpha(float newAlpha) const {
    return AtomicColor(r, g, b, newAlpha);
  }

  constexpr AtomicColor mix(const AtomicColor &other, float t) const {
    return AtomicColor(r + (other.r - r) * t, g + (other.g - g) * t,
                       b + (other.b - b) * t, a + (other.a - a) * t);
  }
};

// -----------------------------------------------------------------
// AUTO-COMPENSATION LOGIC
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

inline float autoCompensate(float c) {
  // We bracketed the target!
  // std::powf(c, 1.0f)  -> resulted in #12
  // std::powf(c, 2.4f)  -> resulted in #06
  // std::powf(c, 1.28f) -> perfectly interpolates to #0a
  return std::powf(c, 1.42f);
}

// Converts 0-255 inputs and automatically applies the tuned compensation curve
inline AtomicColor srgbColor(float r255, float g255, float b255,
                             float a = 1.0f) {
  return AtomicColor(autoCompensate(r255 / 255.0f),
                     autoCompensate(g255 / 255.0f),
                     autoCompensate(b255 / 255.0f), a);
}
} // namespace detail_color

// -----------------------------------------------------------------
// 1. DYNAMIC PALETTE GENERATOR
// -----------------------------------------------------------------
struct AtomicPalette {
  std::array<AtomicColor, 10> shades;

  constexpr AtomicPalette() = default;

  constexpr AtomicPalette(const AtomicColor &base) {
    shades[0] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.92f); // 50
    shades[1] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.80f); // 100
    shades[2] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.60f); // 200
    shades[3] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.40f); // 300
    shades[4] = base.mix(AtomicColor(1.0f, 1.0f, 1.0f), 0.20f); // 400
    shades[5] = base;                                           // 500
    shades[6] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.20f); // 600
    shades[7] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.40f); // 700
    shades[8] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.60f); // 800
    shades[9] = base.mix(AtomicColor(0.0f, 0.0f, 0.0f), 0.85f); // 900
  }

  constexpr AtomicPalette(const std::array<AtomicColor, 10> &customShades)
      : shades(customShades) {}

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
      return shades[5];
    }
  }
};

// -----------------------------------------------------------------
// 2. PALETTE INSTANCES
// Evaluated at runtime startup due to the std::powf compensation curve.
// -----------------------------------------------------------------
namespace Colors {
inline const AtomicColor white = AtomicColor(1.0f, 1.0f, 1.0f, 1.0f);
inline const AtomicColor transparent = AtomicColor(0.0f, 0.0f, 0.0f, 0.0f);
inline const AtomicColor orange = detail_color::srgbColor(255, 128, 0);

inline const AtomicPalette gray =
    AtomicPalette(detail_color::srgbColor(64, 64, 64));
inline const AtomicPalette black =
    AtomicPalette(detail_color::srgbColor(26, 26, 28));
inline const AtomicPalette blue =
    AtomicPalette(detail_color::srgbColor(51, 128, 230));
inline const AtomicPalette red =
    AtomicPalette(detail_color::srgbColor(230, 51, 51));

inline const AtomicColor bajajGreen = detail_color::srgbColor(26, 153, 51);
inline const AtomicColor soapBlue = detail_color::srgbColor(51, 128, 230);
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

  static AtomicTheme Dark() {
    return AtomicTheme{.background = Colors::gray[900],
                       .surface = Colors::gray[800],
                       .text = Colors::white,
                       .accent = Colors::orange,
                       .buttonPressed = Colors::gray[700],
                       .buttonHover = Colors::gray[600]};
  }

  static AtomicTheme Light() {
    return AtomicTheme{.background = Colors::white,
                       .surface = Colors::gray[100],
                       .text = Colors::gray[900],
                       .accent = Colors::soapBlue,
                       .buttonPressed = Colors::gray[300],
                       .buttonHover = Colors::gray[200]};
  }
};

// -----------------------------------------------------------------
// 4. GLOBAL HEX LITERAL PARSER
// Uses autoCompensate() so inline hex codes are crushed to the proper target.
// -----------------------------------------------------------------
inline AtomicColor operator""_hex(const char *str, size_t len) {
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
    return detail_color::srgbColor(static_cast<float>(r), static_cast<float>(g),
                                   static_cast<float>(b));
  } else if (sv.length() == 8) {
    int r =
        (detail_color::charToHex(sv[0]) << 4) | detail_color::charToHex(sv[1]);
    int g =
        (detail_color::charToHex(sv[2]) << 4) | detail_color::charToHex(sv[3]);
    int b =
        (detail_color::charToHex(sv[4]) << 4) | detail_color::charToHex(sv[5]);
    int a =
        (detail_color::charToHex(sv[6]) << 4) | detail_color::charToHex(sv[7]);

    // Alpha is linear transparency; it doesn't get color compensated.
    return detail_color::srgbColor(static_cast<float>(r), static_cast<float>(g),
                                   static_cast<float>(b), a / 255.0f);
  }
  return AtomicColor(0.0f, 0.0f, 0.0f, 1.0f);
}
