#include "ui/motion/MotionManager.h"

namespace atomic::motion {

MotionHandle MotionManager::createHandle() noexcept {
  return MotionHandle{nextHandleId++};
}

Timeline &MotionManager::createTimeline() {
  timelines.push_back(std::make_unique<Timeline>());
  return *timelines.back();
}

void MotionManager::tick(float deltaTime) noexcept {
  if (deltaTime <= 0.0f)
    return;

  lastDeltaTime = deltaTime;
  currentFrame++;

  // Step active timers automatically
  for (auto &timer : timers) {
    if (timer && !timer->isFinished()) {
      timer->tick(deltaTime);
    }
  }

  for (auto &tween : tweens) {
    if (tween && !tween->isFinished()) {
      tween->tick(deltaTime);
    }
  }

  for (auto &timeline : timelines) {
    if (timeline && !timeline->isFinished()) {
      timeline->tick(deltaTime);
    }
  }

  purgeFinished();
}

void MotionManager::cancel(MotionHandle handle) noexcept {
  if (!handle.isValid()) {
    return;
  }

  tweens.erase(std::remove_if(tweens.begin(), tweens.end(),
                              [handle](const auto &tween) {
                                return tween && tween->getHandle() == handle;
                              }),
               tweens.end());

  immediateStates.erase(handle.id);
}

void MotionManager::clear() noexcept {
  tweens.clear();
  timelines.clear();
  immediateStates.clear();
}

void MotionManager::gc(uint64_t maxUnusedFrames) noexcept {
  for (auto it = immediateStates.begin(); it != immediateStates.end();) {
    if (currentFrame - it->second->getLastFrameTouched() > maxUnusedFrames) {
      it = immediateStates.erase(it);
    } else {
      ++it;
    }
  }
}

size_t MotionManager::activeTweenCount() const noexcept {
  return std::count_if(tweens.begin(), tweens.end(),
                       [](const auto &t) { return t && !t->isFinished(); });
}

void MotionManager::purgeFinished() noexcept {
  tweens.erase(std::remove_if(tweens.begin(), tweens.end(),
                              [](const auto &tween) {
                                return !tween || tween->isFinished();
                              }),
               tweens.end());

  timelines.erase(std::remove_if(timelines.begin(), timelines.end(),
                                 [](const auto &timeline) {
                                   return !timeline || timeline->isFinished();
                                 }),
                  timelines.end());
}

} // namespace atomic::motion
