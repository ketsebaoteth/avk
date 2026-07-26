#include "animation/animation.h"
#include "avk/atomic_ui.h"
#include "ui/color.h"
#include "ui/lucide-icons.generated.h"

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

/*
 * @brief A basic Column component
 * */
void Column(Modifier &&modifier, const std::function<void()> &content);

/*
 * @brief A basic Row component
 * */
void Row(Modifier &&modifier, const std::function<void()> &content);

/**
 * @brief Renders a styled vector image component.
 * @param textureIndex Index of the GPU-uploaded boundless texture.
 * @param tint Color multiplier to tint the image (Defaults to white).
 */
Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint = glm::vec4(1.0f));

/**
 * @brief Renders a styled, interactive, layout-integrated text component.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier = DefaultModifier());

// overload with no fontID
Interaction Text(const std::string &text,
                 Modifier &&modifier = DefaultModifier());

// notRecommendedForUse: overloaded for internal use only
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId,
                 Modifier &&modifier = DefaultModifier());

/**
 * @brief Clean interactive Button layout component with optional child
 * composition.
 */
Interaction Button(Modifier &&modifier,
                   const std::function<void()> &content = nullptr);

// Convenience overload that wraps your layout Button
inline Interaction Button(const std::string &label,
                          Modifier &&modifier = Modifier{},
                          glm::vec4 textColor = "#ffffff"_hex) {
  return Button(std::move(modifier), [label, textColor]() {
    Text(label, DefaultModifier().color(textColor));
  });
}
/**
 * @brief Renders a highly interactive immediate-mode text input box with full
 * selection and controls.
 * @param textBuffer Reference to the std::string that will hold the typed
 * characters.
 * @param placeholder Fallback placeholder text shown when the buffer is empty.
 */
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId);

Interaction TextInput(std::string &textBuffer, const std::string &placeholder,
                      Modifier &&modifier = DefaultModifier());

enum class DropdownPlacement {
  Smart,  // Radix UI / macOS style: centers dropdown over header at selected
          // item
  Bottom, // Standard dropdown below header
  Top,    // Popup above header
  Left,   // Popup to the left
  Right   // Popup to the right
};

Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options, uint32_t fontId,
                   DropdownPlacement placement = DropdownPlacement::Smart);

Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement = DropdownPlacement::Smart);

/**
 * @brief Rich styling and behavior configuration for ScrollViews.
 */
struct ScrollViewConfig {
  bool smoothScrolling = true;
  float smoothFactor = 0.05f; // more smother when this number is lower

  // Visibility toggles
  bool showVerticalBar = true;
  bool showHorizontalBar = false;

  // Detailed Scrollbar Customizations
  float scrollbarWidth = 6.0f;  // Width of the vertical bar handle
  float scrollbarRadius = 3.0f; // Corner rounding of the handle
  float scrollbarMarginRight =
      4.0f; // Spacing gap between handle and right container edge
  float scrollbarMarginBottom =
      4.0f; // Spacing gap between handle and bottom container edge
  float scrollbarMinThumbSize = 24.0f; // Minimum physical height of the handle

  // Dynamic, interactive scrollbar handle colors
  glm::vec4 scrollbarColor =
      glm::vec4(1.0f, 1.0f, 1.0f, 0.20f); // Default subtle white
  glm::vec4 scrollbarColorHover =
      glm::vec4(1.0f, 1.0f, 1.0f, 0.45f); // Brightens on hover
  glm::vec4 scrollbarColorPressed =
      glm::vec4(1.0f, 1.0f, 1.0f, 0.65f); // Glows while dragged
};

// Full version with explicit config
void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback);

// Convenience version using default config (most common usage)
inline void ScrollView(Modifier &&modifier,
                       std::function<void()> contentCallback) {
  ScrollView(std::move(modifier), ScrollViewConfig{},
             std::move(contentCallback));
}

// Convenience version with default modifier and default config
inline void ScrollView(std::function<void()> contentCallback) {
  ScrollView(DefaultModifier(), ScrollViewConfig{}, std::move(contentCallback));
}

/**
 * @brief Renders a beautiful, animated shadcn-style sliding switch/toggle.
 * @param checked Reference to the boolean tracking if the switch is ON or OFF.
 * @return An Interaction state block.
 */
Interaction Switch(Modifier &&modifier, bool &checked);

/**
 * @brief Renders a razor-sharp, point-filtered vector Lucide icon.
 * @param name The official Lucide icon name (e.g. "search", "settings",
 * "chevron-down").
 */
Interaction Icon(LucideIcon icon, Modifier &&modifier);
/**
 * @brief Renders a beautiful, animated shadcn-style checkbox with a vector
 * checkmark.
 * @param checked Reference to the boolean tracking the checked state.
 * @return An Interaction state block.
 */
Interaction Checkbox(Modifier &&modifier, bool &checked);
} // namespace atomic
