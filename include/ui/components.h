#pragma once
#include "ui/generated/lucideIcons.generated.h"
#include "ui/internal/context.h"
#include "ui/renderer/interaction.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include <functional>
#include <string>

struct Clay_ElementId;

namespace atomic {

inline const float DEFAULT_HEIGHT = 38.0f;
inline const glm::vec4 DEFAULT_BACKGROUND_NORMAL = "#ffffff"_hex;
inline const glm::vec4 DEFAULT_BORDER_NORMAL = "#e5e5e5"_hex;
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
Interaction Div(Modifier &&modifier = Modifier(),
                const std::function<void()> &content = nullptr);
inline Interaction Div(const std::function<void()> &content = nullptr) {
  return Div(Modifier(), content);
};

/**
 * @brief Convenience inline wrapper for Row direction Div containers.
 */
inline Interaction Row(Modifier &&modifier = Modifier(),
                       const std::function<void()> &content = nullptr) {
  return Div(std::move(modifier).row(), content);
}

/**
 * @brief Convenience inline wrapper for Column direction Div containers.
 */
inline Interaction Column(Modifier &&modifier = Modifier(),
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
                 Modifier &&modifier = Modifier());

/**
 * @brief Overload for Text using default or inherited font ID.
 */
Interaction Text(const std::string &text, Modifier &&modifier = Modifier());

/**
 * @brief Internal overload for Text with explicit element ID.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId, Modifier &&modifier = Modifier());

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
 * @brief Input filtering policy enum for TextInput.
 */
enum class TextInputType { Text, NumberOnly, AlphaOnly, Alphanumeric, Custom };

/**
 * @brief Custom render callback hook signature for TextInput presentation
 * overrides.
 */
using CustomTextRendererFn = std::function<void(
    const std::string &displayString, float x, float y, float fontSize,
    avk::Font *font, const glm::vec4 &textColor)>;

/**
 * @brief Comprehensive configuration descriptor for TextInput validation,
 * rendering, and behavior.
 */
struct TextConfig {
  TextInputType type = TextInputType::Text;
  std::function<bool(uint32_t codepoint, const std::string &currentText,
                     uint32_t cursorIdx)>
      customFilter{nullptr};
  CustomTextRendererFn customRenderer{
      nullptr};         // Custom SVG/Glyph rendering hook
  size_t maxLength = 0; // 0 = Unlimited
  bool isPassword = false;
  float customCharAdvance = 0.0f;
};

/**
 * @brief Interactive text input box component with full config validation,
 * clipboard, and selection.
 */
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, const TextConfig &config,
                      uint32_t fontId = 0);

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder = "", uint32_t fontId = 0);

Interaction TextInput(std::string &textBuffer, const std::string &placeholder,
                      Modifier &&modifier); /**
                                             * @brief Placement positioning
                                             * modes for Select dropdown menus.
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
  float scrollSpeed = 105.0f;

  bool showVerticalBar = true;
  bool showHorizontalBar = false;

  float scrollbarWidth = 6.0f;
  float scrollbarRadius = 3.0f;
  float scrollbarMarginRight = 4.0f;
  float scrollbarMarginBottom = 4.0f;
  float scrollbarMinThumbSize = 24.0f;

  // Optimized for light themes (Neutral light-gray tones)
  glm::vec4 scrollbarColor = {0.82f, 0.82f, 0.85f,
                              0.80f}; // Light gray (approx. gray[200])
  glm::vec4 scrollbarColorHover = {0.65f, 0.65f, 0.68f,
                                   0.90f}; // Medium gray on hover
  glm::vec4 scrollbarColorPressed = {0.50f, 0.50f, 0.54f,
                                     1.0f}; // Darker gray when pressed
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
  ScrollView(Modifier(), ScrollViewConfig{}, std::move(contentCallback));
}

/**
 * @brief Animated sliding toggle switch component.
 */
Interaction Switch(Modifier &&modifier, bool &checked);

/**
 * @brief Vector icon rendering component using Lucide glyph codepoints.
 */
Interaction Icon(LucideIcon icon, Modifier &&modifier = Modifier());

/**
 * @brief Animated checkbox component with vector checkmark.
 */
Interaction Checkbox(Modifier &&modifier, bool &checked);

} // namespace atomic

namespace atomicComponents {

enum class ToastDirection { Top, Bottom, Left, Right };

struct ToastConfig {
  float delay = 0.3f; // Delay in seconds before the toast pops up
  ToastDirection direction =
      ToastDirection::Top; // Pop-up direction relative to the trigger
  float distance =
      10.0f; // Slide distance in pixels for the smooth entrance animation
  float duration = 0.2f; // Animation duration in seconds
};

/**
 * @brief Floating animated toast component.
 */
void Toast(std::function<void()> triggerCallback,
           std::function<void()> toastContentCallback,
           atomic::Modifier &&modifier = atomic::Modifier{},
           ToastConfig config = ToastConfig{});

/**
 * @brief Floating popover overlay component with automatic outside-click
 * dismissal.
 */
void Popover(bool &isOpen, std::function<void()> triggerCallback,
             std::function<void()> popupCallback,
             atomic::Modifier &&popupModifier = atomic::Modifier());

/**
 * @brief Modern Shadcn-style Tab Button with smooth hover effects, active
 * indicators, and click handling.
 */
inline bool TabButton(const std::string &tabId, const std::string &label,
                      std::string &activeTab) {
  using namespace atomic;

  bool isActive = (activeTab == tabId);

  // Modern colors
  glm::vec4 activeBg = "#f4f4f5"_hex; // Target surface color

  glm::vec4 inactiveBg = glm::vec4(activeBg.r, activeBg.g, activeBg.b, 0.0f);
  glm::vec4 hoverBg = "#e4e4e7"_hex;

  glm::vec4 activeTextColor = "#09090b"_hex;   // Dark zinc text
  glm::vec4 inactiveTextColor = "#71717a"_hex; // Muted gray text

  // Determine target background
  bool isBtnHovered = !isActive && isHovered(hashLabel(tabId));
  glm::vec4 targetBg =
      isActive ? activeBg : (isBtnHovered ? hoverBg : inactiveBg);

  // Build tab modifier with smooth 0.15s ease-out transitions
  Modifier style = Modifier()
                       .id(tabId)
                       .background(targetBg)
                       .color(isActive ? activeTextColor : inactiveTextColor)
                       .fontSize(13.0f)
                       .fontWeight(isActive ? 500.0f : 400.0f)
                       .padding(isBtnHovered ? 24 : 14, 8)
                       .rounded(6.0f)
                       .borderless()
                       .transition(0.4f, AnimationCurve::Ease());

  Interaction result = Button(label, std::move(style));

  if (result.clicked) {
    activeTab = tabId;
  }

  return result.clicked;
}
} // namespace atomicComponents
