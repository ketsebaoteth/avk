#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "ui/style/themeManager.h"
#include "ui/utils/clayUtils.h"

#include <cmath>
#include <cstdint>
#include <string_view>

namespace atomic {

Interaction Text(std::string_view text, Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  Clay_ElementId textId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Text");
  return Text(text, textId, std::move(modifier));
}

Interaction Text(std::string_view text, Clay_ElementId textId,
                 Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();
  auto &tm = ThemeManager::getInstance();

  // ⚡ ENUM O(1) LOOKUPS
  float fontSizeMult =
      tm.getVariable<float>(ThemeVarId::FontSizeMultiplier, 1.0f);
  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);

  Style style = utils::layout::resolveTransitions(textId.id, rawStyle);

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  Clay_ElementId outerId = textId;
  outerId.id += 0x6D417267;

  constexpr float BASE_UI_SCALE = 2.0f;

  float monitorDpi =
      (getVeraApp() && getVeraApp()->getPrimaryMonitor().dpiScale > 0.0f)
          ? getVeraApp()->getPrimaryMonitor().dpiScale
          : 1.0f;

  float effectiveScale = monitorDpi * BASE_UI_SCALE;

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = style.marginLeft.value_or(0.0f) * spacingMult * effectiveScale;
    float mr = style.marginRight.value_or(0.0f) * spacingMult * effectiveScale;
    float mt = style.marginTop.value_or(0.0f) * spacingMult * effectiveScale;
    float mb = style.marginBottom.value_or(0.0f) * spacingMult * effectiveScale;

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

  uint32_t fontId = getActiveFont();
  uint32_t finalFontId = (fontId != INVALID_FONT_ID)
                             ? fontId
                             : (inherited.fontId.value_or(getDefaultFontId()));

  avk::Font *font = getFont(finalFontId);

  float defaultLogicalFontSize = (font && font->getFontSize() > 0)
                                     ? static_cast<float>(font->getFontSize())
                                     : 16.0f;

  float unscaledLogicalFontSize =
      (style.fontSize.value_or(
          inherited.fontSize.value_or(defaultLogicalFontSize))) *
      fontSizeMult;

  float finalPhysicalFontSize = unscaledLogicalFontSize * effectiveScale;

  glm::vec4 themeTextColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorTextPrimary, Colors::black[900]);
  glm::vec4 textColor =
      style.textColor.value_or(inherited.textColor.value_or(themeTextColor));

  textColor.a *= effectiveOpacity;
  float textOffset =
      style.textOffset.value_or(inherited.textOffset.value_or(0.0f));

  // ⚡ Fix: Explicitly construct std::string for copyStringToClayBuffer
  Clay_String allocatedString = copyStringToClayBuffer(std::string(text));

  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  if (style.textAlign.has_value()) {
    switch (style.textAlign.value()) {
    case TextAlign::Center:
      clayTextAlign = CLAY_TEXT_ALIGN_CENTER;
      break;
    case TextAlign::Right:
      clayTextAlign = CLAY_TEXT_ALIGN_RIGHT;
      break;
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
    glm::vec4 themeRadius =
        glm::vec4(tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 0.0f));
    glm::vec4 radius = style.borderRadius.value_or(themeRadius);
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
    resolved.fontWeight =
        style.fontWeight.value_or(inherited.fontWeight.value_or(400.0f));
    resolved.letterSpacing =
        style.letterSpacing.value_or(inherited.letterSpacing.value_or(0.0f));
    resolved.lineHeight =
        style.lineHeight.value_or(inherited.lineHeight.value_or(0.0f));

    uiState->computedStyleMap[textId.id] = resolved;
  }

  return result;
}

} // namespace atomic
