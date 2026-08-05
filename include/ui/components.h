#pragma once

#include "ui/generated/lucideIcons.generated.h"
#include "ui/renderer/interaction.h"
#include "ui/style/modifier.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// ⚡ Forward Declarations (Zero Includes Required)
struct Clay_ElementId;
namespace avk {
class Font;
}

namespace atomic {

// Constants
constexpr float DEFAULT_HEIGHT = 38.0f;
constexpr float DEFAULT_BORDER_WIDTH = 1.0f;
constexpr uint32_t INVALID_FONT_ID = 0xFFFFFFFF;

inline const glm::vec4 DEFAULT_BACKGROUND_NORMAL{1.0f, 1.0f, 1.0f,
                                                 1.0f}; // #ffffff
inline const glm::vec4 DEFAULT_BORDER_NORMAL{0.8980392f, 0.8980392f, 0.8980392f,
                                             1.0f}; // #e5e5e5
inline const glm::vec4 DEFAULT_BORDER_RADIUS{6.0f, 6.0f, 6.0f, 6.0f};

namespace Curves {
const AnimationCurve &AppleEaseOut();
const AnimationCurve &AppleSnappy();
const AnimationCurve &AppleEaseInOut();
const AnimationCurve &Emphasized();
const AnimationCurve &SmoothSwift();
} // namespace Curves

// ----------------------------------------------------------------------------
// Primitives
// ----------------------------------------------------------------------------
Interaction Div(Modifier &&modifier = Modifier(),
                const std::function<void()> &content = nullptr);

Interaction Div(const std::function<void()> &content);

Interaction Row(Modifier &&modifier = Modifier(),
                const std::function<void()> &content = nullptr);

Interaction Column(Modifier &&modifier = Modifier(),
                   const std::function<void()> &content = nullptr);

Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint = glm::vec4(1.0f));

Interaction Text(std::string_view text, Modifier &&modifier);
Interaction Text(std::string_view text);
Interaction Text(std::string_view text, Clay_ElementId textId,
                 Modifier &&modifier = Modifier());

Interaction Button(Modifier &&modifier, const std::function<void()> &content);
Interaction Button(const std::string &label, Modifier &&modifier = Modifier{});

// ----------------------------------------------------------------------------
// Inputs & Select
// ----------------------------------------------------------------------------
enum class TextInputType { Text, NumberOnly, AlphaOnly, Alphanumeric, Custom };

using CustomTextRendererFn = std::function<void(
    const std::string &displayString, float x, float y, float fontSize,
    avk::Font *font, const glm::vec4 &textColor)>;

struct TextConfig {
  TextInputType type = TextInputType::Text;
  std::function<bool(uint32_t codepoint, const std::string &currentText,
                     uint32_t cursorIdx)>
      customFilter{nullptr};
  CustomTextRendererFn customRenderer{nullptr};
  size_t maxLength = 0;
  bool isPassword = false;
  float customCharAdvance = 0.0f;
};

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, const TextConfig &config);

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder = "");

enum class DropdownPlacement { Smart, Bottom, Top, Left, Right };

Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement = DropdownPlacement::Smart);

// ----------------------------------------------------------------------------
// ScrollView
// ----------------------------------------------------------------------------
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

  glm::vec4 scrollbarColor = {0.82f, 0.82f, 0.85f, 0.80f};
  glm::vec4 scrollbarColorHover = {0.65f, 0.65f, 0.68f, 0.90f};
  glm::vec4 scrollbarColorPressed = {0.50f, 0.50f, 0.54f, 1.0f};
};

void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback);

void ScrollView(Modifier &&modifier, std::function<void()> contentCallback);

void ScrollView(std::function<void()> contentCallback);

Interaction Switch(Modifier &&modifier, bool &checked);

Interaction Icon(LucideIcon icon, Modifier &&modifier = Modifier());

Interaction Checkbox(Modifier &&modifier, bool &checked);

} // namespace atomic

namespace atomicComponents {

enum class ToastDirection { Top, Bottom, Left, Right };

struct ToastConfig {
  float delay = 0.3f;
  ToastDirection direction = ToastDirection::Top;
  float distance = 10.0f;
  float duration = 0.2f;
};

void Toast(std::function<void()> triggerCallback,
           std::function<void()> toastContentCallback,
           atomic::Modifier &&modifier = atomic::Modifier{},
           ToastConfig config = ToastConfig{});

void Popover(bool &isOpen, std::function<void()> triggerCallback,
             std::function<void()> popupCallback,
             atomic::Modifier &&popupModifier = atomic::Modifier());

// ⚡ Declarations only (Implementation in components.cpp)
bool TabButton(const std::string &tabId, const std::string &label,
               std::string &activeTab);

} // namespace atomicComponents
