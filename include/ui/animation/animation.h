
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace atomic {

// ============================================================================
// 1. Cubic Bezier & Easing Presets
// ============================================================================

class AnimationCurve {
public:
  constexpr AnimationCurve() : p1x(0.0f), p1y(0.0f), p2x(1.0f), p2y(1.0f) {}
  constexpr AnimationCurve(float x1, float y1, float x2, float y2)
      : p1x(x1), p1y(y1), p2x(x2), p2y(y2) {}

  // Evaluate curve at time progress t in [0.0, 1.0]
  [[nodiscard]] float evaluate(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    if (p1x == p1y && p2x == p2y) {
      return t; // Linear optimization
    }

    // Solve for parameter u given x = t using Newton-Raphson
    float u = t;
    for (int i = 0; i < 8; ++i) {
      float x = sampleCurveX(u) - t;
      if (std::abs(x) < 1e-5f)
        break;
      float dx = sampleCurveDerivativeX(u);
      if (std::abs(dx) < 1e-5f)
        break;
      u -= x / dx;
    }

    // Fallback binary search if Newton failed out of bounds
    if (u < 0.0f || u > 1.0f) {
      float low = 0.0f, high = 1.0f;
      u = t;
      while (low < high) {
        float x = sampleCurveX(u);
        if (std::abs(x - t) < 1e-4f)
          break;
        if (t > x)
          low = u;
        else
          high = u;
        u = (high + low) * 0.5f;
      }
    }

    return sampleCurveY(u);
  }

  // Preset Factory Functions (GSAP Equivalents)
  static AnimationCurve Linear() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
  static AnimationCurve Ease() { return {0.25f, 0.1f, 0.25f, 1.0f}; }
  static AnimationCurve EaseIn() { return {0.42f, 0.0f, 1.0f, 1.0f}; }
  static AnimationCurve EaseOut() { return {0.0f, 0.0f, 0.58f, 1.0f}; }
  static AnimationCurve EaseInOut() { return {0.42f, 0.0f, 0.58f, 1.0f}; }
  static AnimationCurve ExpoIn() { return {0.7f, 0.0f, 0.84f, 0.0f}; }
  static AnimationCurve ExpoOut() { return {0.16f, 1.0f, 0.3f, 1.0f}; }
  static AnimationCurve Custom(float x1, float y1, float x2, float y2) {
    return {x1, y1, x2, y2};
  }

private:
  float p1x, p1y, p2x, p2y;

  [[nodiscard]] inline float sampleCurveX(float t) const {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * p1x +
           3.0f * (1.0f - t) * t * t * p2x + t * t * t;
  }

  [[nodiscard]] inline float sampleCurveY(float t) const {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * p1y +
           3.0f * (1.0f - t) * t * t * p2y + t * t * t;
  }

  [[nodiscard]] inline float sampleCurveDerivativeX(float t) const {
    return 3.0f * (1.0f - t) * (1.0f - t) * p1x +
           6.0f * (1.0f - t) * t * (p2x - p1x) + 3.0f * t * t * (1.0f - p2x);
  }
};

// ============================================================================
// 2. Global / Immediate-Mode Animation Manager
// ============================================================================

struct AnimStateFloat {
  bool initialized{false};
  float currentValue{0.0f};
  float startValue{0.0f};
  float targetValue{0.0f};
  float elapsedTime{0.0f};
  float duration{0.2f};
  AnimationCurve curve{AnimationCurve::EaseOut()};
  bool isAnimating{false};
  uint64_t lastFrameTouched{0};
};

struct AnimStateVec4 {
  bool initialized{false};
  glm::vec4 currentValue{0.0f};
  glm::vec4 startValue{0.0f};
  glm::vec4 targetValue{0.0f};
  float elapsedTime{0.0f};
  float duration{0.2f};
  AnimationCurve curve{AnimationCurve::EaseOut()};
  bool isAnimating{false};
  uint64_t lastFrameTouched{0};
};

class AnimationManager {
public:
  static AnimationManager &instance() {
    static AnimationManager mgr;
    return mgr;
  }

  void update(float deltaTimeSeconds) {
    m_deltaTime = deltaTimeSeconds;
    m_currentFrame++;
  }

  float animate(uint32_t elementId, float targetValue, float duration,
                const AnimationCurve &curve, float speed = 1.0f) {
    auto &state = m_floatStates[elementId];
    state.lastFrameTouched = m_currentFrame;

    // Fix 1: Snap to target on very first frame (prevents 0.0f pop)
    if (!state.initialized) {
      state.currentValue = targetValue;
      state.targetValue = targetValue;
      state.initialized = true;
      return state.currentValue;
    }

    if (state.targetValue != targetValue) {
      state.startValue = state.currentValue;
      state.targetValue = targetValue;
      state.elapsedTime = 0.0f;
      state.duration = std::max(duration / std::max(speed, 0.001f), 0.001f);
      state.curve = curve;
      state.isAnimating = true;
    }

    if (state.isAnimating) {
      state.elapsedTime += m_deltaTime;
      float progress =
          std::clamp(state.elapsedTime / state.duration, 0.0f, 1.0f);
      float easedProgress = state.curve.evaluate(progress);

      state.currentValue =
          state.startValue +
          (state.targetValue - state.startValue) * easedProgress;

      if (progress >= 1.0f) {
        state.currentValue = state.targetValue;
        state.isAnimating = false;
      }
    }

    return state.currentValue;
  }

  // Fix 2: Native Vec4 interpolation (prevents ID collisions)
  glm::vec4 animateVec4(uint32_t elementId, const glm::vec4 &targetValue,
                        float duration, const AnimationCurve &curve,
                        float speed = 1.0f) {
    auto &state = m_vec4States[elementId];
    state.lastFrameTouched = m_currentFrame;

    if (!state.initialized) {
      state.currentValue = targetValue;
      state.targetValue = targetValue;
      state.initialized = true;
      return state.currentValue;
    }

    if (state.targetValue != targetValue) {
      state.startValue = state.currentValue;
      state.targetValue = targetValue;
      state.elapsedTime = 0.0f;
      state.duration = std::max(duration / std::max(speed, 0.001f), 0.001f);
      state.curve = curve;
      state.isAnimating = true;
    }

    if (state.isAnimating) {
      state.elapsedTime += m_deltaTime;
      float progress =
          std::clamp(state.elapsedTime / state.duration, 0.0f, 1.0f);
      float easedProgress = state.curve.evaluate(progress);

      // glm::mix is the GLM equivalent of Lerp
      state.currentValue =
          glm::mix(state.startValue, state.targetValue, easedProgress);

      if (progress >= 1.0f) {
        state.currentValue = state.targetValue;
        state.isAnimating = false;
      }
    }

    return state.currentValue;
  }

  void gc(uint64_t maxUnusedFrames = 120) {
    for (auto it = m_floatStates.begin(); it != m_floatStates.end();) {
      if (m_currentFrame - it->second.lastFrameTouched > maxUnusedFrames) {
        it = m_floatStates.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = m_vec4States.begin(); it != m_vec4States.end();) {
      if (m_currentFrame - it->second.lastFrameTouched > maxUnusedFrames) {
        it = m_vec4States.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  std::unordered_map<uint32_t, AnimStateFloat> m_floatStates;
  std::unordered_map<uint32_t, AnimStateVec4> m_vec4States;
  float m_deltaTime{0.016f};
  uint64_t m_currentFrame{0};
};

// ============================================================================
// 3. Convenience Component Hooks (Supports both integer IDs and String Labels!)
// ============================================================================

inline uint32_t hashLabel(const std::string &label) {
  uint32_t hash = 2166136261u;
  for (char c : label) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

// String Label Overload for AnimateFloat
inline float AnimateFloat(const std::string &label, float target,
                          float duration = 0.2f,
                          AnimationCurve curve = AnimationCurve::EaseOut(),
                          float speed = 1.0f) {
  return AnimationManager::instance().animate(hashLabel(label), target,
                                              duration, curve, speed);
}

// String Label Overload for AnimateVec4
inline glm::vec4 AnimateVec4(const std::string &label, const glm::vec4 &target,
                             float duration = 0.2f,
                             AnimationCurve curve = AnimationCurve::EaseOut()) {
  return AnimationManager::instance().animateVec4(hashLabel(label), target,
                                                  duration, curve);
}

// Integer ID Overloads (Existing)
inline float AnimateFloat(uint32_t id, float target, float duration = 0.2f,
                          AnimationCurve curve = AnimationCurve::EaseOut(),
                          float speed = 1.0f) {
  return AnimationManager::instance().animate(id, target, duration, curve,
                                              speed);
}

inline glm::vec4 AnimateVec4(uint32_t id, const glm::vec4 &target,
                             float duration = 0.2f,
                             AnimationCurve curve = AnimationCurve::EaseOut()) {
  return AnimationManager::instance().animateVec4(id, target, duration, curve);
}
} // namespace atomic
