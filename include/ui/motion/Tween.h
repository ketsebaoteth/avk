#pragma once

#include "Curves.h"
#include "MotionTypes.h"
#include "Property.h"

#include <algorithm>
#include <concepts>
#include <utility>

namespace atomic::motion {

/**
 * @brief Discriminator defining whether a tween evaluates via duration-curve or
 * physical spring.
 */
enum class TweenMode : uint8_t { Curve = 0, Spring };

/**
 * @brief Abstract polymorphic base interface for uniform heterogeneous tween
 * ticking.
 */
class ITween {
public:
  virtual ~ITween() = default;

  [[nodiscard]] virtual MotionHandle getHandle() const noexcept = 0;
  [[nodiscard]] virtual PlayState getPlayState() const noexcept = 0;
  virtual void setPlayState(PlayState newState) noexcept = 0;

  virtual void tick(float deltaTime) noexcept = 0;
  [[nodiscard]] virtual bool isFinished() const noexcept = 0;
  virtual void reset() noexcept = 0;
};

/**
 * @brief Concrete template class managing type-safe property interpolation over
 * time.
 */
template <typename T>
  requires Interpolatable<T>
class Tween : public ITween {
public:
  using CallbackFn = void (*)(void *userData);

  constexpr Tween() noexcept = default;

  constexpr Tween(MotionHandle h, PropertyRef<T> prop, const T &start,
                  const T &end, float dur) noexcept
      : handle(h), property(prop), startValue(start), endValue(end),
        duration(dur > 0.0f ? dur : 0.001f) {}

  [[nodiscard]] MotionHandle getHandle() const noexcept override {
    return handle;
  }
  [[nodiscard]] PlayState getPlayState() const noexcept override {
    return state;
  }
  void setPlayState(PlayState newState) noexcept override { state = newState; }

  [[nodiscard]] bool isFinished() const noexcept override {
    return state == PlayState::Completed || state == PlayState::Stopped;
  }

  /**
   * @brief Resets the elapsed time and state to initial start conditions.
   */
  void reset() noexcept override {
    elapsed = 0.0f;
    state = PlayState::Stopped;
    property.set(startValue);
  }

  /**
   * @brief Re-targets the tween mid-flight, capturing current position as
   * startValue to prevent popping.
   */
  void retarget(const T &newEndValue, float newDuration = -1.0f) noexcept {
    if (property.isValid()) {
      startValue = property.get();
    } else {
      startValue = lerp(startValue, endValue, getEasedProgress());
    }
    endValue = newEndValue;
    elapsed = 0.0f;

    if (newDuration > 0.0f) {
      duration = newDuration;
    } else if (mode == TweenMode::Spring) {
      duration = springConfig.estimateDuration();
    }

    state = PlayState::Running;
  }

  /**
   * @brief Steps the animation forward by deltaTime seconds.
   */
  void tick(float deltaTime) noexcept override {
    if (state != PlayState::Running) {
      return;
    }

    elapsed += deltaTime;

    const float easedProgress = getEasedProgress();
    property.applyInterpolation(startValue, endValue, easedProgress);

    if (onUpdateCallback) {
      onUpdateCallback(userData);
    }

    if (elapsed >= duration) {
      /**
       * @brief Ensure strict endpoint convergence on final frame.
       */
      property.set(endValue);
      handleLoopCompletion();
    }
  }

  /**
   * @brief Builder pattern modifiers (zero heap allocations).
   */
  constexpr Tween &setCurve(const AnimationCurve &c) noexcept {
    curve = c;
    mode = TweenMode::Curve;
    return *this;
  }

  constexpr Tween &setSpring(const SpringConfig &config) noexcept {
    springConfig = config;
    mode = TweenMode::Spring;
    duration = springConfig.estimateDuration();
    return *this;
  }

  constexpr Tween &setLoopMode(LoopMode m) noexcept {
    loopMode = m;
    return *this;
  }

  constexpr Tween &setAutoreverse(bool enable) noexcept {
    flags.autoreverse = enable;
    return *this;
  }

  constexpr Tween &setOnComplete(CallbackFn cb, void *data = nullptr) noexcept {
    onCompleteCallback = cb;
    userData = data;
    return *this;
  }

  constexpr Tween &setOnUpdate(CallbackFn cb, void *data = nullptr) noexcept {
    onUpdateCallback = cb;
    userData = data;
    return *this;
  }

  MotionHandle handle{MotionHandle::Invalid()};
  PropertyRef<T> property{};
  T startValue{};
  T endValue{};
  float duration{1.0f};
  float elapsed{0.0f};
  PlayState state{PlayState::Stopped};
  LoopMode loopMode{LoopMode::Once};
  MotionFlags flags{};
  TweenMode mode{TweenMode::Curve};
  AnimationCurve curve{AnimationCurve::Linear()};
  SpringConfig springConfig{};

  CallbackFn onUpdateCallback{nullptr};
  CallbackFn onCompleteCallback{nullptr};
  void *userData{nullptr};

private:
  [[nodiscard]] float getEasedProgress() const noexcept {
    if (mode == TweenMode::Spring) {
      return evaluateSpring(elapsed, springConfig);
    }
    const float rawProgress = std::clamp(elapsed / duration, 0.0f, 1.0f);
    return curve.evaluate(rawProgress);
  }

  void handleLoopCompletion() noexcept {
    if (onCompleteCallback) {
      onCompleteCallback(userData);
    }

    switch (loopMode) {
    case LoopMode::Once:
      state = PlayState::Completed;
      break;
    case LoopMode::Infinite:
      elapsed = 0.0f;
      if (flags.autoreverse) {
        std::swap(startValue, endValue);
      }
      break;
    case LoopMode::PingPong:
      elapsed = 0.0f;
      std::swap(startValue, endValue);
      break;
    }
  }
};

} // namespace atomic::motion
