#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/utils/clayUtils.h"

#include <cmath>

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
 * @brief Core Text primitive resolving cascading style inheritance, margins,
 * and multiline wrapped height.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId, Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();

  Style style = utils::layout::resolveTransitions(textId.id, rawStyle);

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  Clay_ElementId outerId = textId;
  outerId.id += 0x6D417267;

  // Global base scale factor so developers can use intuitive logical font sizes
  constexpr float BASE_UI_SCALE = 2.0f;

  float monitorDpi =
      (getVeraApp() && getVeraApp()->getPrimaryMonitor().dpiScale > 0.0f)
          ? getVeraApp()->getPrimaryMonitor().dpiScale
          : 1.0f;

  float effectiveScale = monitorDpi * BASE_UI_SCALE;

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = style.marginLeft.value_or(0.0f) * effectiveScale;
    float mr = style.marginRight.value_or(0.0f) * effectiveScale;
    float mt = style.marginTop.value_or(0.0f) * effectiveScale;
    float mb = style.marginBottom.value_or(0.0f) * effectiveScale;

    Clay_ElementDeclaration outerDecl{};
    utils::layout::applyStyleToLayout(outerDecl, style);

    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};

    outerDecl.backgroundColor = {0, 0, 0, 0};
    Clay__ConfigureOpenElement(outerDecl);
  }

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  uint32_t finalFontId =
      (fontId != 0)
          ? fontId
          : (inherited.fontId != 0 ? inherited.fontId : getDefaultFontId());

  avk::Font *font = getFont(finalFontId);
  float defaultLogicalFontSize = (font && font->getFontSize() > 0)
                                     ? static_cast<float>(font->getFontSize())
                                     : 16.0f;

  float unscaledLogicalFontSize = style.fontSize.value_or(
      inherited.fontSize > 0.0f ? inherited.fontSize : defaultLogicalFontSize);

  float finalPhysicalFontSize = unscaledLogicalFontSize * effectiveScale;

  glm::vec4 textColor = style.textColor.value_or(
      inherited.textColor.a > 0.01f ? inherited.textColor : Colors::black[900]);

  if (textColor.a < 0.01f) {
    textColor = glm::vec4(0.10f, 0.10f, 0.11f, 1.0f);
  }

  textColor.a *= effectiveOpacity;
  float textOffset = style.textOffset.value_or(inherited.textOffset);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  // Map style.textAlign to Clay_TextAlignment
  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  if (style.textAlign.has_value()) {
    switch (style.textAlign.value()) {
    case TextAlign::Center:
      clayTextAlign = CLAY_TEXT_ALIGN_CENTER;
      break;
    case TextAlign::Right:
      clayTextAlign = CLAY_TEXT_ALIGN_RIGHT;
      break;
    // case TextAlign::Justify:
    // Justified text needs full container width from Clay;
    // payload->textAlign carries Justify to TextLayout::ShapeString!
    // clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
    // break;
    default:
      clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
      break;
    }
  }

  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(finalFontId);
  config.fontSize = static_cast<uint16_t>(std::round(finalPhysicalFontSize));
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData = payload;

  Clay__OpenTextElement(allocatedString, config);

  if (hasMargin) {
    Clay__CloseElement();
  }

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
    resolved.fontSize = unscaledLogicalFontSize;
    uiState->computedStyleMap[textId.id] = resolved;
  }

  return result;
}

} // namespace atomic
