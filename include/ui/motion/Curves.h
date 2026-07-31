#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atomic::motion {

/**
 * @brief Evaluates cubic Bézier curves for arbitrary scalar inputs using
 * Newton-Raphson iteration with bounded binary subdivision fallbacks. Zero heap
 * footprint.
 */
class AnimationCurve {
public:
  constexpr AnimationCurve() noexcept = default;
  constexpr AnimationCurve(float x1, float y1, float x2, float y2) noexcept
      : p1x(x1), p1y(y1), p2x(x2), p2y(y2) {}

  /**
   * @brief Evaluates the curve output for a normalized time parameter t in
   * [0.0, 1.0].
   */
  [[nodiscard]] float evaluate(float t) const noexcept {
    t = std::clamp(t, 0.0f, 1.0f);

    if (p1x == p1y && p2x == p2y) {
      return t;
    }

    float u = t;

    for (int i = 0; i < 8; ++i) {
      float x = sampleCurveX(u) - t;
      if (std::abs(x) < 1e-5f) {
        break;
      }
      float dx = sampleCurveDerivativeX(u);
      if (std::abs(dx) < 1e-5f) {
        break;
      }
      u -= x / dx;
    }

    if (u < 0.0f || u > 1.0f) {
      float low = 0.0f;
      float high = 1.0f;
      u = t;
      for (int i = 0; i < 16; ++i) {
        float x = sampleCurveX(u);
        if (std::abs(x - t) < 1e-4f) {
          break;
        }
        if (t > x) {
          low = u;
        } else {
          high = u;
        }
        u = (high + low) * 0.5f;
      }
    }

    return sampleCurveY(std::clamp(u, 0.0f, 1.0f));
  }

  // --- Core Presets ---
  static constexpr AnimationCurve Linear() noexcept {
    return {0.0f, 0.0f, 1.0f, 1.0f};
  }
  static constexpr AnimationCurve Ease() noexcept {
    return {0.25f, 0.1f, 0.25f, 1.0f};
  }
  static constexpr AnimationCurve EaseIn() noexcept {
    return {0.42f, 0.0f, 1.0f, 1.0f};
  }
  static constexpr AnimationCurve EaseOut() noexcept {
    return {0.0f, 0.0f, 0.58f, 1.0f};
  }
  static constexpr AnimationCurve EaseInOut() noexcept {
    return {0.42f, 0.0f, 0.58f, 1.0f};
  }

  // --- Exponential ---
  static constexpr AnimationCurve ExpoIn() noexcept {
    return {0.7f, 0.0f, 0.84f, 0.0f};
  }
  static constexpr AnimationCurve ExpoOut() noexcept {
    return {0.16f, 1.0f, 0.3f, 1.0f};
  }
  static constexpr AnimationCurve ExpoInOut() noexcept {
    return {0.87f, 0.0f, 0.13f, 1.0f};
  }

  // --- Overshoot Back Curves ---
  static constexpr AnimationCurve BackIn() noexcept {
    return {0.36f, -0.56f, 0.64f, 1.0f};
  }
  static constexpr AnimationCurve BackOut() noexcept {
    return {0.34f, 1.56f, 0.64f, 1.0f};
  }
  static constexpr AnimationCurve BackInOut() noexcept {
    return {0.68f, -0.55f, 0.265f, 1.55f};
  }

  // --- Sine Curves ---
  static constexpr AnimationCurve SineIn() noexcept {
    return {0.12f, 0.0f, 0.39f, 0.0f};
  }
  static constexpr AnimationCurve SineOut() noexcept {
    return {0.61f, 1.0f, 0.88f, 1.0f};
  }
  static constexpr AnimationCurve SineInOut() noexcept {
    return {0.37f, 0.0f, 0.63f, 1.0f};
  }

  // --- Cubic Curves ---
  static constexpr AnimationCurve CubicIn() noexcept {
    return {0.32f, 0.0f, 0.67f, 0.0f};
  }
  static constexpr AnimationCurve CubicOut() noexcept {
    return {0.33f, 1.0f, 0.68f, 1.0f};
  }
  static constexpr AnimationCurve CubicInOut() noexcept {
    return {0.65f, 0.0f, 0.35f, 1.0f};
  }

  // --- Circular Curves ---
  static constexpr AnimationCurve CircIn() noexcept {
    return {0.55f, 0.0f, 1.0f, 0.45f};
  }
  static constexpr AnimationCurve CircOut() noexcept {
    return {0.0f, 0.55f, 0.45f, 1.0f};
  }
  static constexpr AnimationCurve CircInOut() noexcept {
    return {0.85f, 0.0f, 0.15f, 1.0f};
  }

  static constexpr AnimationCurve Custom(float x1, float y1, float x2,
                                         float y2) noexcept {
    return {x1, y1, x2, y2};
  }

private:
  float p1x{0.0f};
  float p1y{0.0f};
  float p2x{1.0f};
  float p2y{1.0f};

  [[nodiscard]] constexpr float sampleCurveX(float t) const noexcept {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * p1x +
           3.0f * (1.0f - t) * t * t * p2x + t * t * t;
  }

  [[nodiscard]] constexpr float sampleCurveY(float t) const noexcept {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * p1y +
           3.0f * (1.0f - t) * t * t * p2y + t * t * t;
  }

  [[nodiscard]] constexpr float sampleCurveDerivativeX(float t) const noexcept {
    return 3.0f * (1.0f - t) * (1.0f - t) * p1x +
           6.0f * (1.0f - t) * t * (p2x - p1x) + 3.0f * t * t * (1.0f - p2x);
  }
};

/**
 * @brief Mass-spring-damper physical parameter descriptor.
 */
struct SpringConfig {
  float mass{1.0f};
  float stiffness{100.0f};
  float damping{10.0f};
  float initialVelocity{0.0f};

  static constexpr SpringConfig Default() noexcept {
    return {1.0f, 120.0f, 14.0f, 0.0f};
  }
  static constexpr SpringConfig Bouncy() noexcept {
    return {1.0f, 180.0f, 8.0f, 0.0f};
  }
  static constexpr SpringConfig Snappy() noexcept {
    return {1.0f, 300.0f, 22.0f, 0.0f};
  }

  constexpr bool operator==(const SpringConfig &) const noexcept = default;

  [[nodiscard]] float
  estimateDuration(float threshold = 0.001f) const noexcept {
    const float m = std::max(mass, 0.001f);
    const float k = std::max(stiffness, 0.001f);
    const float c = std::max(damping, 0.0f);

    const float omegaN = std::sqrt(k / m);
    const float zeta = c / (2.0f * std::sqrt(k * m));

    const float alpha = (zeta < 1.0f) ? (zeta * omegaN) : omegaN;
    if (alpha <= 1e-5f) {
      return 1.0f;
    }

    return std::clamp(-std::log(threshold) / alpha, 0.05f, 10.0f);
  }
};

[[nodiscard]] inline float evaluateSpring(float t,
                                          const SpringConfig &config) noexcept {
  if (t <= 0.0f) {
    return 0.0f;
  }

  const float m = std::max(config.mass, 0.001f);
  const float k = std::max(config.stiffness, 0.001f);

  const float omegaN = std::sqrt(k / m);
  const float zeta = config.damping / (2.0f * std::sqrt(k * m));

  if (zeta < 1.0f) {
    const float omegaD = omegaN * std::sqrt(1.0f - zeta * zeta);
    if (omegaD < 1e-5f) {
      return 1.0f;
    }
    const float alpha = zeta * omegaN;
    if (std::exp(-alpha * t) < 1e-5f) {
      return 1.0f;
    }
    const float c2 = (config.initialVelocity + alpha) / omegaD;
    return 1.0f - std::exp(-alpha * t) *
                      (std::cos(omegaD * t) + c2 * std::sin(omegaD * t));
  } else {
    const float alpha = omegaN;
    if (std::exp(-alpha * t) < 1e-5f) {
      return 1.0f;
    }
    const float c = config.initialVelocity + alpha;
    return 1.0f - std::exp(-alpha * t) * (1.0f + c * t);
  }
}

namespace Easing {

[[nodiscard]] inline float SineIn(float t) noexcept {
  return 1.0f - std::cos((t * std::numbers::pi_v<float>)*0.5f);
}

[[nodiscard]] inline float SineOut(float t) noexcept {
  return std::sin((t * std::numbers::pi_v<float>)*0.5f);
}

[[nodiscard]] inline float SineInOut(float t) noexcept {
  return -0.5f * (std::cos(std::numbers::pi_v<float> * t) - 1.0f);
}

[[nodiscard]] inline float QuadIn(float t) noexcept { return t * t; }

[[nodiscard]] inline float QuadOut(float t) noexcept { return t * (2.0f - t); }

[[nodiscard]] inline float QuadInOut(float t) noexcept {
  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

[[nodiscard]] inline float CubicIn(float t) noexcept { return t * t * t; }

[[nodiscard]] inline float CubicOut(float t) noexcept {
  const float f = t - 1.0f;
  return f * f * f + 1.0f;
}

[[nodiscard]] inline float CubicInOut(float t) noexcept {
  return t < 0.5f ? 4.0f * t * t * t
                  : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

[[nodiscard]] inline float BackIn(float t) noexcept {
  constexpr float c1 = 1.70158f;
  constexpr float c3 = c1 + 1.0f;
  return c3 * t * t * t - c1 * t * t;
}

[[nodiscard]] inline float BackOut(float t) noexcept {
  constexpr float c1 = 1.70158f;
  constexpr float c3 = c1 + 1.0f;
  const float f = t - 1.0f;
  return 1.0f + c3 * f * f * f + c1 * f * f;
}

[[nodiscard]] inline float BounceOut(float t) noexcept {
  constexpr float n1 = 7.5625f;
  constexpr float d1 = 2.75f;
  if (t < 1.0f / d1) {
    return n1 * t * t;
  } else if (t < 2.0f / d1) {
    const float f = t - 1.5f / d1;
    return n1 * f * f + 0.75f;
  } else if (t < 2.5f / d1) {
    const float f = t - 2.25f / d1;
    return n1 * f * f + 0.9375f;
  } else {
    const float f = t - 2.625f / d1;
    return n1 * f * f + 0.984375f;
  }
}

} // namespace Easing

} // namespace atomic::motion
