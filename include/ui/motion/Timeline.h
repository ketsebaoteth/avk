#pragma once

#include "Curves.h"
#include "MotionTypes.h"
#include "Property.h"
#include "Tween.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <vector>

namespace atomic::motion {

/**
 * @brief Multi-track GSAP-style animation timeline for parallel, sequential,
 * and staggered choreography.
 */
class Timeline {
public:
  using CallbackFn = void (*)(void *userData);

  /**
   * @brief Individual scheduled track entry within the timeline.
   */
  struct TrackEntry {
    std::unique_ptr<ITween> tween;
    float startTime{0.0f};
    float duration{0.0f};
    bool started{false};
  };

  constexpr Timeline() noexcept = default;
  ~Timeline() = default;

  Timeline(const Timeline &) = delete;
  Timeline &operator=(const Timeline &) = delete;
  Timeline(Timeline &&) noexcept = default;
  Timeline &operator=(Timeline &&) noexcept = default;

  /**
   * @brief Appends a tween sequentially after the current timeline cursor time.
   */
  template <typename T>
    requires Interpolatable<T>
  Timeline &append(Tween<T> &&tween, float delay = 0.0f) {
    const float scheduledStart = cursorTime + std::max(delay, 0.0f);
    const float dur = tween.duration;

    TrackEntry entry{.tween = std::make_unique<Tween<T>>(std::move(tween)),
                     .startTime = scheduledStart,
                     .duration = dur,
                     .started = false};

    cursorTime = scheduledStart + dur;
    totalDuration = std::max(totalDuration, cursorTime);

    tracks.push_back(std::move(entry));
    return *this;
  }

  /**
   * @brief Schedules a tween in parallel alongside the start of the previous
   * track.
   */
  template <typename T>
    requires Interpolatable<T>
  Timeline &join(Tween<T> &&tween, float offset = 0.0f) {
    float scheduledStart = 0.0f;
    if (!tracks.empty()) {
      scheduledStart = std::max(0.0f, tracks.back().startTime + offset);
    } else {
      scheduledStart = std::max(0.0f, offset);
    }

    const float dur = tween.duration;

    TrackEntry entry{.tween = std::make_unique<Tween<T>>(std::move(tween)),
                     .startTime = scheduledStart,
                     .duration = dur,
                     .started = false};

    cursorTime = std::max(cursorTime, scheduledStart + dur);
    totalDuration = std::max(totalDuration, cursorTime);

    tracks.push_back(std::move(entry));
    return *this;
  }

  /**
   * @brief Inserts a tween at an absolute timestamp offset from timeline start.
   */
  template <typename T>
    requires Interpolatable<T>
  Timeline &insert(float absoluteStartTime, Tween<T> &&tween) {
    const float scheduledStart = std::max(0.0f, absoluteStartTime);
    const float dur = tween.duration;

    TrackEntry entry{.tween = std::make_unique<Tween<T>>(std::move(tween)),
                     .startTime = scheduledStart,
                     .duration = dur,
                     .started = false};

    cursorTime = std::max(cursorTime, scheduledStart + dur);
    totalDuration = std::max(totalDuration, cursorTime);

    tracks.push_back(std::move(entry));
    return *this;
  }

  /**
   * @brief Helper utility to stagger a series of target properties by a fixed
   * interval.
   */
  template <typename T>
    requires Interpolatable<T>
  Timeline &staggerTo(const std::vector<PropertyRef<T>> &targets,
                      const T &endValue, float duration, float staggerInterval,
                      const AnimationCurve &curve = AnimationCurve::EaseOut()) {
    for (size_t i = 0; i < targets.size(); ++i) {
      if (!targets[i].isValid()) {
        continue;
      }
      const uint32_t handleId = static_cast<uint32_t>(tracks.size() + 1);
      Tween<T> tw(MotionHandle{handleId}, targets[i], targets[i].get(),
                  endValue, duration);
      tw.setCurve(curve);

      if (i == 0) {
        append(std::move(tw), 0.0f);
      } else {
        join(std::move(tw), staggerInterval);
      }
    }
    return *this;
  }

  /**
   * @brief Advances the timeline execution state forward by deltaTime seconds.
   */
  void tick(float deltaTime) noexcept {
    if (state != PlayState::Running) {
      return;
    }

    const float scaledDt = deltaTime * timeScale * (isReversed ? -1.0f : 1.0f);
    elapsed += scaledDt;

    if (!hasStartedTriggered && onStartCallback) {
      hasStartedTriggered = true;
      onStartCallback(userData);
    }

    bool allTracksFinished = true;

    for (auto &track : tracks) {
      if (!track.tween) {
        continue;
      }

      if (elapsed >= track.startTime) {
        float dtToApply = scaledDt;

        if (!track.started) {
          track.started = true;
          track.tween->setPlayState(PlayState::Running);
          /**
           * @brief Sub-frame alignment: advance by the exact offset since
           * scheduled start time.
           */
          const float trackLocalOffset =
              std::max(0.0f, elapsed - track.startTime);
          dtToApply = std::min(scaledDt, trackLocalOffset);
        }

        track.tween->tick(dtToApply);
      }

      if (!track.tween->isFinished()) {
        allTracksFinished = false;
      }
    }

    if (onUpdateCallback) {
      onUpdateCallback(userData);
    }

    if (allTracksFinished ||
        (isReversed ? elapsed <= 0.0f : elapsed >= totalDuration)) {
      handleTimelineCompletion();
    }
  }

  /**
   * @brief Controls playback execution state.
   */
  void play() noexcept {
    state = PlayState::Running;
    for (auto &track : tracks) {
      if (track.tween && elapsed >= track.startTime &&
          !track.tween->isFinished()) {
        track.tween->setPlayState(PlayState::Running);
      }
    }
  }

  void pause() noexcept {
    state = PlayState::Paused;
    for (auto &track : tracks) {
      if (track.tween) {
        track.tween->setPlayState(PlayState::Paused);
      }
    }
  }

  void stop() noexcept {
    state = PlayState::Stopped;
    elapsed = 0.0f;
    hasStartedTriggered = false;
    for (auto &track : tracks) {
      if (track.tween) {
        track.tween->reset();
      }
      track.started = false;
    }
  }

  void restart() noexcept {
    stop();
    play();
  }

  /**
   * @brief Seeks directly to a specified timestamp offset in seconds.
   */
  void seek(float timestamp) noexcept {
    elapsed = std::clamp(timestamp, 0.0f, totalDuration);
    for (auto &track : tracks) {
      if (!track.tween) {
        continue;
      }
      if (elapsed >= track.startTime) {
        const float trackElapsed =
            std::clamp(elapsed - track.startTime, 0.0f, track.duration);
        track.tween->tick(trackElapsed);
      } else {
        track.tween->reset();
        track.started = false;
      }
    }
  }

  /**
   * @brief Builder configuration methods.
   */
  constexpr Timeline &setTimeScale(float scale) noexcept {
    timeScale = std::max(scale, 0.001f);
    return *this;
  }

  constexpr Timeline &setLoopMode(LoopMode mode) noexcept {
    loopMode = mode;
    return *this;
  }

  constexpr Timeline &setReversed(bool reversed) noexcept {
    isReversed = reversed;
    return *this;
  }

  constexpr Timeline &setOnStart(CallbackFn cb, void *data = nullptr) noexcept {
    onStartCallback = cb;
    userData = data;
    return *this;
  }

  constexpr Timeline &setOnUpdate(CallbackFn cb,
                                  void *data = nullptr) noexcept {
    onUpdateCallback = cb;
    userData = data;
    return *this;
  }

  constexpr Timeline &setOnComplete(CallbackFn cb,
                                    void *data = nullptr) noexcept {
    onCompleteCallback = cb;
    userData = data;
    return *this;
  }

  [[nodiscard]] PlayState getPlayState() const noexcept { return state; }
  [[nodiscard]] float getTotalDuration() const noexcept {
    return totalDuration;
  }
  [[nodiscard]] float getElapsedTime() const noexcept { return elapsed; }
  [[nodiscard]] bool isFinished() const noexcept {
    return state == PlayState::Completed || state == PlayState::Stopped;
  }

private:
  std::vector<TrackEntry> tracks{};
  float totalDuration{0.0f};
  float cursorTime{0.0f};
  float elapsed{0.0f};
  float timeScale{1.0f};
  PlayState state{PlayState::Stopped};
  LoopMode loopMode{LoopMode::Once};
  bool isReversed{false};
  bool hasStartedTriggered{false};

  CallbackFn onStartCallback{nullptr};
  CallbackFn onUpdateCallback{nullptr};
  CallbackFn onCompleteCallback{nullptr};
  void *userData{nullptr};

  void handleTimelineCompletion() noexcept {
    if (onCompleteCallback) {
      onCompleteCallback(userData);
    }

    switch (loopMode) {
    case LoopMode::Once:
      state = PlayState::Completed;
      break;
    case LoopMode::Infinite:
      restart();
      break;
    case LoopMode::PingPong:
      isReversed = !isReversed;
      elapsed = isReversed ? totalDuration : 0.0f;
      play();
      break;
    }
  }
};

} // namespace atomic::motion
