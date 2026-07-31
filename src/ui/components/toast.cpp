#include "avk/utils/ui/layout.h"
#include "ui/components.h"
#include "ui/motion/AtomicMotion.h"

#include <chrono>
#include <functional>
#include <unordered_map>

namespace atomicComponents {

/**
 * @brief Floating animated toast component with configurable delay, direction,
 * and a polished light theme style powered by Atomic.Motion.
 */
void Toast(std::function<void()> triggerCallback,
           std::function<void()> toastContentCallback,
           atomic::Modifier &&modifier, ToastConfig config) {

  Clay_ElementId triggerId = utils::layout::getNextId("ToastTrigger");
  uint32_t toastAnimId = triggerId.id;

  static std::unordered_map<uint32_t, bool> hoverStates;
  static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point>
      hoverStartTimes;

  bool isHovered = hoverStates[toastAnimId];
  auto now = std::chrono::steady_clock::now();

  if (isHovered) {
    if (hoverStartTimes.find(toastAnimId) == hoverStartTimes.end()) {
      hoverStartTimes[toastAnimId] = now;
    }
  } else {
    hoverStartTimes.erase(toastAnimId);
  }

  bool showToast = false;
  if (isHovered && config.delay > 0.0f) {
    std::chrono::duration<float> elapsed = now - hoverStartTimes[toastAnimId];
    if (elapsed.count() >= config.delay) {
      showToast = true;
    }
  } else {
    showToast = isHovered;
  }

  auto *uiState = atomic::getUiState();
  using atomic::motion::MotionHandle;

  float targetOpacity = showToast ? 1.0f : 0.0f;
  float opacity = targetOpacity;
  float scale = showToast ? 1.0f : 0.95f;

  if (uiState) {
    opacity = uiState->motionManager.animate<float>(
        MotionHandle{toastAnimId}, targetOpacity, config.duration,
        atomic::motion::AnimationCurve::EaseOut());

    scale = uiState->motionManager.animate<float>(
        MotionHandle{toastAnimId + 0x1000}, showToast ? 1.0f : 0.95f,
        config.duration, atomic::motion::AnimationCurve::EaseOut());
  }

  atomic::Interaction trigger =
      atomic::Row(atomic::Modifier().relative(), [&]() {
        if (triggerCallback)
          triggerCallback();

        if (opacity > 0.01f) {
          atomic::AttachPoint attachFrom = atomic::AttachPoint::BottomCenter;
          atomic::AttachPoint attachTo = atomic::AttachPoint::TopCenter;
          float offsetX = 0.0f;
          float offsetY = 0.0f;
          float slideOffset = (1.0f - opacity) * config.distance;

          switch (config.direction) {
          case ToastDirection::Top:
            attachFrom = atomic::AttachPoint::BottomCenter;
            attachTo = atomic::AttachPoint::TopCenter;
            offsetY = -6.0f - slideOffset;
            break;
          case ToastDirection::Bottom:
            attachFrom = atomic::AttachPoint::TopCenter;
            attachTo = atomic::AttachPoint::BottomCenter;
            offsetY = 6.0f + slideOffset;
            break;
          case ToastDirection::Left:
            attachFrom = atomic::AttachPoint::CenterRight;
            attachTo = atomic::AttachPoint::CenterLeft;
            offsetX = -6.0f - slideOffset;
            break;
          case ToastDirection::Right:
            attachFrom = atomic::AttachPoint::CenterLeft;
            attachTo = atomic::AttachPoint::CenterRight;
            offsetX = 6.0f + slideOffset;
            break;
          }

          glm::vec4 lightBg = {0.98f, 0.98f, 0.99f, 0.95f * opacity};
          glm::vec4 lightBorder = {0.85f, 0.85f, 0.88f, 1.0f * opacity};

          atomic::Modifier toastStyle = std::move(modifier)
                                            .absolute()
                                            .attach(attachFrom, attachTo)
                                            .offset(offsetX, offsetY)
                                            .padding(12, 16)
                                            .rounded(8.0f)
                                            .color(Colors::gray[500])
                                            .fontSize(10)
                                            .scale(scale)
                                            .background(lightBg)
                                            .border(lightBorder, 1.0f);

          atomic::Row(std::move(toastStyle), [&]() {
            if (toastContentCallback)
              toastContentCallback();
          });
        }
      });

  hoverStates[toastAnimId] = trigger.hovered;
}

} // namespace atomicComponents
