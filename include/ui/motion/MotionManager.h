#pragma once

#include "Curves.h"
#include "MotionTypes.h"
#include "Property.h"
#include "Timeline.h"
#include "Timer.h"
#include "Tween.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <unordered_map>
#include <vector>

namespace atomic::motion {

/**
 * @brief Central manager and scheduler for all active tweens, timelines, and
 * immediate-mode state bindings.
 */
class MotionManager {
public:
  MotionManager() noexcept = default;
  ~MotionManager() = default;

  MotionManager(const MotionManager &) = delete;
  MotionManager &operator=(const MotionManager &) = delete;
  MotionManager(MotionManager &&) noexcept = default;
  MotionManager &operator=(MotionManager &&) noexcept = default;

  /**
   * @brief Generates a unique stable MotionHandle.
   */
  [[nodiscard]] MotionHandle createHandle() noexcept;

  /**
   * @brief Creates and registers a retained property tween.
   */
  template <typename T>
    requires Interpolatable<T>
  MotionHandle createTween(PropertyRef<T> target, const T &start, const T &end,
                           float duration) {
    MotionHandle handle = createHandle();
    auto tween =
        std::make_unique<Tween<T>>(handle, target, start, end, duration);
    tween->setPlayState(PlayState::Running);

    tweens.push_back(std::move(tween));
    return handle;
  }

  /**
   * @brief Registers an existing timeline instance into the master manager tick
   * loop.
   */
  Timeline &createTimeline();

  /**
   * @brief Immediate-Mode Animation Entry Point: Animates a property to
   * targetValue over duration. Smoothly handles target changes mid-flight
   * without visual popping.
   */
  template <typename T>
    requires Interpolatable<T>
  T animate(MotionHandle handle, const T &targetValue, float duration,
            const AnimationCurve &curve = AnimationCurve::EaseOut()) {
    if (!handle.isValid()) {
      return targetValue;
    }

    auto it = immediateStates.find(handle.id);
    if (it == immediateStates.end()) {
      /**
       * @brief First initialization: set current value immediately to target.
       */
      ImmediateState<T> state{.currentValue = targetValue,
                              .targetValue = targetValue,
                              .tween =
                                  Tween<T>(handle, PropertyRef<T>{},
                                           targetValue, targetValue, duration)};
      state.tween.setCurve(curve);
      state.tween.setPlayState(PlayState::Stopped);
      state.lastFrameTouched = currentFrame;

      immediateStates[handle.id] =
          std::make_unique<ImmediateHolder<T>>(std::move(state));
      return targetValue;
    }

    auto *holder = static_cast<ImmediateHolder<T> *>(it->second.get());
    holder->state.lastFrameTouched = currentFrame;

    if (holder->state.targetValue != targetValue) {
      /**
       * @brief Target changed mid-flight: retarget smooth transition without
       * popping.
       */
      holder->state.targetValue = targetValue;
      holder->state.tween.property =
          PropertyRef<T>(&holder->state.currentValue);
      holder->state.tween.retarget(targetValue, duration);
      holder->state.tween.setCurve(curve);
    }

    if (holder->state.tween.getPlayState() == PlayState::Running) {
      holder->state.tween.property =
          PropertyRef<T>(&holder->state.currentValue);
      holder->state.tween.tick(lastDeltaTime);
    }

    return holder->state.currentValue;
  }
  /**
   * @brief Registers a fully-configured Tween instance (with custom LoopMode,
   * Spring, or Callbacks) directly into the manager execution loop.
   */
  template <typename T>
    requires Interpolatable<T>
  MotionHandle registerTween(Tween<T> &&tween) {
    MotionHandle handle = tween.getHandle();
    if (!handle.isValid()) {
      handle = createHandle();
      tween.handle = handle;
    }
    tween.setPlayState(PlayState::Running);
    tweens.push_back(std::make_unique<Tween<T>>(std::move(tween)));
    return handle;
  }

  /**
   * @brief Immediate-Mode Spring Animation Entry Point.
   */
  template <typename T>
    requires Interpolatable<T>
  T animateSpring(MotionHandle handle, const T &targetValue,
                  const SpringConfig &config = SpringConfig::Default()) {
    if (!handle.isValid()) {
      return targetValue;
    }

    auto it = immediateStates.find(handle.id);
    if (it == immediateStates.end()) {
      ImmediateState<T> state{.currentValue = targetValue,
                              .targetValue = targetValue,
                              .tween = Tween<T>(handle, PropertyRef<T>{},
                                                targetValue, targetValue,
                                                config.estimateDuration())};
      state.tween.setSpring(config);
      state.tween.setPlayState(PlayState::Stopped);
      state.lastFrameTouched = currentFrame;

      immediateStates[handle.id] =
          std::make_unique<ImmediateHolder<T>>(std::move(state));
      return targetValue;
    }

    auto *holder = static_cast<ImmediateHolder<T> *>(it->second.get());
    holder->state.lastFrameTouched = currentFrame;

    if (holder->state.targetValue != targetValue) {
      holder->state.targetValue = targetValue;
      holder->state.tween.setSpring(config);
      holder->state.tween.property =
          PropertyRef<T>(&holder->state.currentValue);
      holder->state.tween.retarget(targetValue, config.estimateDuration());
    }

    if (holder->state.tween.getPlayState() == PlayState::Running) {
      holder->state.tween.property =
          PropertyRef<T>(&holder->state.currentValue);
      holder->state.tween.tick(lastDeltaTime);
    }

    return holder->state.currentValue;
  }

  void clearTimelines() noexcept {
    for (auto &tl : timelines) {
      if (tl) {
        tl->stop();
      }
    }
    timelines.clear();
  }
  /**
   * @brief Advances all active tweens, timelines, and immediate-mode animations
   * by deltaTime.
   */
  void tick(float deltaTime) noexcept;

  /**
   * @brief Cancels an active tween or timeline matching handle.
   */
  void cancel(MotionHandle handle) noexcept;

  /**
   * @brief Clears all active animations and resets internal states.
   */
  void clear() noexcept;

  /**
   * @brief Purges immediate-mode states untouched for longer than
   * maxUnusedFrames.
   */
  void gc(uint64_t maxUnusedFrames = 120) noexcept;

  [[nodiscard]] size_t activeTweenCount() const noexcept;
  [[nodiscard]] uint64_t getCurrentFrame() const noexcept {
    return currentFrame;
  }

  /**
   * @brief Creates and registers a new MotionTimer instance.
   */
  MotionHandle createTimer(const TimerConfig &config) {
    MotionHandle handle = createHandle();
    auto timer = std::make_unique<MotionTimer>(handle, config);
    timer->play();
    timers.push_back(std::move(timer));
    return handle;
  }

  [[nodiscard]] MotionTimer *getTimer(MotionHandle handle) noexcept {
    if (!handle.isValid())
      return nullptr;
    for (auto &t : timers) {
      if (t && t->getHandle() == handle)
        return t.get();
    }
    return nullptr;
  }

  void clearTimers() noexcept { timers.clear(); }

private:
  std::vector<std::unique_ptr<MotionTimer>> timers{};
  struct IImmediateHolder {
    virtual ~IImmediateHolder() = default;
    virtual void tick(float dt) = 0;
    [[nodiscard]] virtual uint64_t getLastFrameTouched() const = 0;
  };

  template <typename T>
    requires Interpolatable<T>
  struct ImmediateState {
    T currentValue{};
    T targetValue{};
    Tween<T> tween{};
    uint64_t lastFrameTouched{0};
  };

  template <typename T>
    requires Interpolatable<T>
  struct ImmediateHolder : public IImmediateHolder {
    ImmediateState<T> state;
    explicit ImmediateHolder(ImmediateState<T> &&s) : state(std::move(s)) {}

    void tick(float dt) override {
      if (state.tween.getPlayState() == PlayState::Running) {
        state.tween.property = PropertyRef<T>(&state.currentValue);
        state.tween.tick(dt);
      }
    }

    [[nodiscard]] uint64_t getLastFrameTouched() const override {
      return state.lastFrameTouched;
    }
  };

  std::vector<std::unique_ptr<ITween>> tweens{};
  std::vector<std::unique_ptr<Timeline>> timelines{};
  std::unordered_map<uint32_t, std::unique_ptr<IImmediateHolder>>
      immediateStates{};

  uint32_t nextHandleId{1};
  uint64_t currentFrame{0};
  float lastDeltaTime{0.016f};

  void purgeFinished() noexcept;
};

} // namespace atomic::motion
