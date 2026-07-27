#include "avk/atomic_ui.h"
#include "avk/utils/ui/layout.h"
#include "ui/components.h"
#include <unordered_map>

namespace atomicComponents {

/**
 * @brief Floating animated toast component anchored to a trigger callback.
 */
void Toast(std::function<void()> triggerCallback,
           std::function<void()> toastContentCallback,
           atomic::Modifier &&modifier) {

  Clay_ElementId triggerId = utils::layout::getNextId("ToastTrigger");
  uint32_t toastAnimId = triggerId.id;

  static std::unordered_map<uint32_t, bool> hoverStates;
  bool isHovered = hoverStates[toastAnimId];

  float opacity =
      atomic::AnimateFloat(toastAnimId, isHovered ? 1.0f : 0.0f, 0.15f);

  atomic::Interaction trigger =
      atomic::Row(atomic::Modifier().relative(), [&]() {
        if (triggerCallback)
          triggerCallback();

        if (opacity > 0.01f) {
          atomic::Modifier toastStyle =
              std::move(modifier)
                  .absolute()
                  .attach(atomic::AttachPoint::BottomCenter,
                          atomic::AttachPoint::TopCenter)
                  .offset(0.0f, -5.0f)
                  .padding(12, 5)
                  .background(glm::vec4(0.12f, 0.12f, 0.12f, 0.95f * opacity));

          atomic::Row(std::move(toastStyle), [&]() {
            if (toastContentCallback)
              toastContentCallback();
          });
        }
      });

  hoverStates[toastAnimId] = trigger.hovered;
}

} // namespace atomicComponents
