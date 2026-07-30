#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/utils/clayUtils.h"

namespace atomic {

/**
 * @brief Default font overload for Text.
 */
Interaction Text(const std::string &text, Modifier &&modifier) {
  return Text(text, 0, std::move(modifier));
}

/**
 * @brief Auto-generating ID overload for Text.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  Clay_ElementId textId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Text");
  return Text(text, fontId, textId, std::move(modifier));
}

/**
 * @brief Core Text primitive resolving cascading style inheritance and
 * multiline wrapped height.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId, Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();

  Style style = utils::layout::resolveTransitions(textId.id, rawStyle);

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  uint32_t finalFontId =
      (fontId != 0)
          ? fontId
          : (inherited.fontId != 0 ? inherited.fontId : getDefaultFontId());

  avk::Font *font = getFont(finalFontId);
  float baseFontSize = (font && font->getFontSize() > 0)
                           ? static_cast<float>(font->getFontSize())
                           : 32.0f;

  float finalFontSize = style.fontSize.value_or(
      inherited.fontSize > 0.0f ? inherited.fontSize : baseFontSize);

  glm::vec4 textColor = style.textColor.value_or(inherited.textColor);
  textColor.a *= effectiveOpacity;
  float textOffset = style.textOffset.value_or(inherited.textOffset);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  if (style.alignX.has_value()) {
    switch (style.alignX.value()) {
    case AlignmentX::Center:
      clayTextAlign = CLAY_TEXT_ALIGN_CENTER;
      break;
    case AlignmentX::Right:
      clayTextAlign = CLAY_TEXT_ALIGN_RIGHT;
      break;
    default:
      clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
      break;
    }
  }

  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);

  // Directly open Clay's Text Element (No redundant outer Div wrapper!)
  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(finalFontId);
  config.fontSize = static_cast<uint16_t>(std::round(finalFontSize));
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData = payload;

  Clay__OpenTextElement(allocatedString, config);

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool isPressed = isHovered && uiState->pointerDown;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = isPressed;
    uiState->currentLifecycleMap[textId.id] = lifecycle;

    CascadingStyle resolved = inherited;
    resolved.textColor = textColor;
    resolved.textOffset = textOffset;
    resolved.fontId = finalFontId;
    resolved.fontSize = finalFontSize;
    uiState->computedStyleMap[textId.id] = resolved;
  }

  return result;
}

} // namespace atomic
