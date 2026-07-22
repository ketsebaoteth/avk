
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

  // Implicit conversion operator directly to your layout library's glm::vec4
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
};

// -----------------------------------------------------------------
// 1. EXTENDABLE FIXED PALETTES (Fixed Order: AtomicColor is now fully
// complete!)
// -----------------------------------------------------------------
struct AtomicPalette {
  std::array<AtomicColor, 10>
      shades; // 50, 100, 200, 300, 400, 500, 600, 700, 800, 900

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
// 2. EASILY ADD MORE COLORS HERE
// -----------------------------------------------------------------
namespace Colors {
inline constexpr AtomicColor white = AtomicColor(1.0f, 1.0f, 1.0f, 1.0f);
inline constexpr AtomicColor transparent = AtomicColor(0.0f, 0.0f, 0.0f, 0.0f);
inline constexpr AtomicColor orange = AtomicColor(1.0f, 0.5f, 0.0f, 1.0f);

// Easy to add your specific business brand shades directly
inline constexpr AtomicColor bajajGreen = AtomicColor(0.1f, 0.6f, 0.2f, 1.0f);
inline constexpr AtomicColor soapBlue = AtomicColor(0.2f, 0.5f, 0.9f, 1.0f);

inline constexpr AtomicPalette gray = AtomicPalette{{
    AtomicColor(0.95f, 0.95f, 0.95f), // 50
    AtomicColor(0.88f, 0.88f, 0.88f), // 100
    AtomicColor(0.75f, 0.75f, 0.75f), // 200
    AtomicColor(0.60f, 0.60f, 0.60f), // 300
    AtomicColor(0.45f, 0.45f, 0.45f), // 400
    AtomicColor(0.25f, 0.25f, 0.25f), // 500
    AtomicColor(0.18f, 0.18f, 0.18f), // 600
    AtomicColor(0.12f, 0.12f, 0.12f), // 700
    AtomicColor(0.06f, 0.06f, 0.06f), // 800
    AtomicColor(0.02f, 0.02f, 0.02f)  // 900
}};
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

  // Built-in preset themes configured completely at compile time
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

// User-Defined Literal in global scope returns AtomicColor completely at
// compile time
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
