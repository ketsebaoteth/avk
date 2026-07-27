#pragma once

#include "animation/animation.h"
#include "avk/atomic_ui.h"
#include "ui/color.h"
#include "ui/lucide-icons.generated.h"
#include <functional>
#include <string>
#include <vector>

namespace atomic {

inline const float DEFAULT_HEIGHT = 38.0f;
inline const glm::vec4 DEFAULT_BACKGROUND_NORMAL = "#151515"_hex;
inline const glm::vec4 DEFAULT_BORDER_NORMAL = "#2f2f2f"_hex;
inline const glm::vec4 DEFAULT_BORDER_RADIUS = glm::vec4(6.0f);
inline const float DEFAULT_BORDER_WIDTH = 1.0f;

namespace Curves {
inline const AnimationCurve AppleEaseOut =
    AnimationCurve::Custom(0.16f, 1.00f, 0.30f, 1.00f);
inline const AnimationCurve AppleSnappy =
    AnimationCurve::Custom(0.19f, 1.00f, 0.22f, 1.00f);
inline const AnimationCurve AppleEaseInOut =
    AnimationCurve::Custom(0.42f, 0.00f, 0.58f, 1.00f);
inline const AnimationCurve Emphasized =
    AnimationCurve::Custom(0.05f, 0.70f, 0.10f, 1.00f);
inline const AnimationCurve SmoothSwift =
    AnimationCurve::Custom(0.40f, 0.00f, 0.20f, 1.00f);
} // namespace Curves

/**
 * @brief Universal layout container element (Flexbox box model).
 */
Interaction Div(Modifier &&modifier = DefaultModifier(),
                const std::function<void()> &content = nullptr);

/**
 * @brief Convenience inline wrapper for Row direction Div containers.
 */
inline Interaction Row(Modifier &&modifier = DefaultModifier(),
                       const std::function<void()> &content = nullptr) {
  return Div(std::move(modifier).row(), content);
}

/**
 * @brief Convenience inline wrapper for Column direction Div containers.
 */
inline Interaction Column(Modifier &&modifier = DefaultModifier(),
                          const std::function<void()> &content = nullptr) {
  return Div(std::move(modifier).column(), content);
}

/**
 * @brief Renders a styled vector image component.
 */
Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint = glm::vec4(1.0f));

/**
 * @brief Renders a styled, layout-integrated text component.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier = DefaultModifier());

/**
 * @brief Overload for Text using default or inherited font ID.
 */
Interaction Text(const std::string &text,
                 Modifier &&modifier = DefaultModifier());

/**
 * @brief Internal overload for Text with explicit element ID.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId,
                 Modifier &&modifier = DefaultModifier());

/**
 * @brief Interactive composable Button component.
 */
Interaction Button(Modifier &&modifier, const std::function<void()> &content);

/**
 * @brief Convenience string label overload for Button.
 * Text automatically inherits button text color via Style Cascade Engine.
 */
inline Interaction Button(const std::string &label,
                          Modifier &&modifier = Modifier{}) {
  return Button(std::move(modifier), [label]() { Text(label); });
}

/**
 * @brief Interactive immediate-mode text input component.
 */
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId);

/**
 * @brief Convenience overload for TextInput using default font.
 */
Interaction TextInput(std::string &textBuffer,
                      const std::string &placeholder = "",
                      Modifier &&modifier = DefaultModifier());

/**
 * @brief Placement positioning modes for Select dropdown menus.
 */
enum class DropdownPlacement { Smart, Bottom, Top, Left, Right };

/**
 * @brief Dropdown select menu component with explicit font ID.
 */
Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options, uint32_t fontId,
                   DropdownPlacement placement = DropdownPlacement::Smart);

/**
 * @brief Dropdown select menu component using default font.
 */
Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement = DropdownPlacement::Smart);

/**
 * @brief Configuration parameters for ScrollView styling and behavior.
 */
struct ScrollViewConfig {
  bool smoothScrolling = true;
  float smoothFactor = 0.05f;

  bool showVerticalBar = true;
  bool showHorizontalBar = false;

  float scrollbarWidth = 6.0f;
  float scrollbarRadius = 3.0f;
  float scrollbarMarginRight = 4.0f;
  float scrollbarMarginBottom = 4.0f;
  float scrollbarMinThumbSize = 24.0f;

  glm::vec4 scrollbarColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.20f);
  glm::vec4 scrollbarColorHover = glm::vec4(1.0f, 1.0f, 1.0f, 0.45f);
  glm::vec4 scrollbarColorPressed = glm::vec4(1.0f, 1.0f, 1.0f, 0.65f);
};

/**
 * @brief Scrollable viewport container with custom configuration.
 */
void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback);

/**
 * @brief Scrollable viewport container with default configuration.
 */
inline void ScrollView(Modifier &&modifier,
                       std::function<void()> contentCallback) {
  ScrollView(std::move(modifier), ScrollViewConfig{},
             std::move(contentCallback));
}

/**
 * @brief Scrollable viewport container with default modifier and config.
 */
inline void ScrollView(std::function<void()> contentCallback) {
  ScrollView(DefaultModifier(), ScrollViewConfig{}, std::move(contentCallback));
}

/**
 * @brief Animated sliding toggle switch component.
 */
Interaction Switch(Modifier &&modifier, bool &checked);

/**
 * @brief Vector icon rendering component using Lucide glyph codepoints.
 */
Interaction Icon(LucideIcon icon, Modifier &&modifier = DefaultModifier());

/**
 * @brief Animated checkbox component with vector checkmark.
 */
Interaction Checkbox(Modifier &&modifier, bool &checked);

} // namespace atomic

namespace atomicComponents {

/**
 * @brief Floating animated toast component.
 */
void Toast(std::function<void()> triggerCallback,
           std::function<void()> toastContentCallback,
           atomic::Modifier &&modifier = atomic::Modifier());

/**
 * @brief Floating popover overlay component with automatic outside-click
 * dismissal.
 */
void Popover(bool &isOpen, std::function<void()> triggerCallback,
             std::function<void()> popupCallback,
             atomic::Modifier &&popupModifier = atomic::Modifier());

} // namespace atomicComponents
