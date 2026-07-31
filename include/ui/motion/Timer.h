#pragma once

#include "MotionTypes.h"
#include <algorithm>

namespace atomic::motion {

/**
 * @brief Configuration descriptor for high-precision motion timers.
 */
struct TimerConfig {
  float duration{1.0f};
  LoopMode loopMode{LoopMode::Infinite};
  float timeScale{1.0f};
};

/**
 * @brief Standalone timer container for time-based iteration counting, looping,
 * and callbacks.
 */
class MotionTimer {
public:
  using UpdateCallbackFn = void (*)(float elapsed, float progress,
                                    uint32_t iteration, void *userData);
  using LoopCallbackFn = void (*)(uint32_t iteration, void *userData);

  constexpr MotionTimer() noexcept = default;

  constexpr MotionTimer(MotionHandle h, const TimerConfig &config) noexcept
      : handle(h), duration(config.duration > 0.0f ? config.duration : 0.001f),
        timeScale(config.timeScale), loopMode(config.loopMode) {}

  [[nodiscard]] MotionHandle getHandle() const noexcept { return handle; }
  [[nodiscard]] PlayState getPlayState() const noexcept { return state; }
  void setPlayState(PlayState newState) noexcept { state = newState; }

  [[nodiscard]] float getElapsedTime() const noexcept { return elapsed; }
  [[nodiscard]] float getProgress() const noexcept {
    return std::clamp(elapsed / duration, 0.0f, 1.0f);
  }

  [[nodiscard]] float getTotalElapsedTime() const noexcept {
    return (static_cast<float>(currentIteration) * duration) + elapsed;
  }
  [[nodiscard]] uint32_t getIterationCount() const noexcept {
    return currentIteration;
  }
  [[nodiscard]] bool isFinished() const noexcept {
    return state == PlayState::Completed || state == PlayState::Stopped;
  }

  void play() noexcept { state = PlayState::Running; }
  void pause() noexcept { state = PlayState::Paused; }
  void stop() noexcept {
    state = PlayState::Stopped;
    elapsed = 0.0f;
    currentIteration = 0;
  }
  void restart() noexcept {
    stop();
    play();
  }

  void tick(float deltaTime) noexcept {
    if (state != PlayState::Running) {
      return;
    }

    elapsed += deltaTime * timeScale;
    const float progress = getProgress();

    if (onUpdateCallback) {
      onUpdateCallback(elapsed, progress, currentIteration, userData);
    }

    if (elapsed >= duration) {
      currentIteration++;

      if (onLoopCallback) {
        onLoopCallback(currentIteration, userData);
      }

      switch (loopMode) {
      case LoopMode::Once:
        state = PlayState::Completed;
        break;
      case LoopMode::Infinite:
        elapsed = 0.0f;
        break;
      case LoopMode::PingPong:
        elapsed = 0.0f;
        timeScale *= -1.0f; // Reverse playback direction
        break;
      }
    }
  }

  constexpr MotionTimer &setTimeScale(float scale) noexcept {
    timeScale = scale;
    return *this;
  }

  constexpr MotionTimer &setOnUpdate(UpdateCallbackFn cb,
                                     void *data = nullptr) noexcept {
    onUpdateCallback = cb;
    userData = data;
    return *this;
  }

  constexpr MotionTimer &setOnLoop(LoopCallbackFn cb,
                                   void *data = nullptr) noexcept {
    onLoopCallback = cb;
    userData = data;
    return *this;
  }

  MotionHandle handle{MotionHandle::Invalid()};
  float duration{1.0f};
  float elapsed{0.0f};
  float timeScale{1.0f};
  uint32_t currentIteration{0};
  PlayState state{PlayState::Stopped};
  LoopMode loopMode{LoopMode::Infinite};

  UpdateCallbackFn onUpdateCallback{nullptr};
  LoopCallbackFn onLoopCallback{nullptr};
  void *userData{nullptr};
};

} // namespace atomic::motion
