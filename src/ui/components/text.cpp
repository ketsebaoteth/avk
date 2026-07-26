#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

Interaction Text(const std::string &text, Modifier &&modifier) {
  return Text(text, getDefaultFontId(), std::move(modifier));
}

Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier) {
  const auto &style = modifier.getStyle();

  Clay_ElementId textId = utils::layout::getNextId("Text");
  Clay__OpenElementWithId(textId);

  // 1. Resolve safe optional values
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 textColor = style.textColor.value_or(glm::vec4(1.0f));
  float textOffset = style.textOffset.value_or(-0.0f);

  avk::Font *font = getFont(fontId);
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  Clay_ElementDeclaration decl{};
  decl.layout = {.sizing = {.width = CLAY_SIZING_FIT(),
                            .height = CLAY_SIZING_FIXED(fontHeight)},
                 .padding = {0, 0, 0, 0}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // 1-LINE SYSTEM HOOK: Apply width/height overrides and absolute floating
  // containers!
  utils::layout::applyStyleToLayout(decl, style);

  // Safe frame allocation - Zero memory leaks!
  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);
  decl.userData = payload;

  Clay__ConfigureOpenElement(decl);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  // Resolve text alignment
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

  uint32_t nativeSize = font ? font->getFontSize() : 16;

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(fontId);
  config.fontSize = static_cast<uint16_t>(nativeSize);
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData =
      payload; // Pass unified payload directly to the render command!

  Clay__OpenTextElement(allocatedString, config);
  Clay__CloseElement();

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId, Modifier &&modifier) {
  const auto &style = modifier.getStyle();

  Clay__OpenElementWithId(textId);

  // Resolve safe optionals
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 textColor = style.textColor.value_or(glm::vec4(1.0f));
  float textOffset = style.textOffset.value_or(-4.0f);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()},
      .padding = {0, 0, 0, 0}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // 1-LINE SYSTEM HOOK: Apply styling overrides and absolute positions!
  utils::layout::applyStyleToLayout(decl, style);

  // Safe frame allocation - Zero memory leaks!
  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);
  decl.userData = payload;

  Clay__ConfigureOpenElement(decl);

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

  avk::Font *font = getFont(fontId);
  uint32_t nativeSize = font ? font->getFontSize() : 16;

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(fontId);
  config.fontSize = static_cast<uint16_t>(nativeSize);
  config.textColor = Clay_Color{textColor.r * 255.0f, textColor.g * 255.0f,
                                textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData =
      payload; // Pass unified payload directly to the render command!

  Clay__OpenTextElement(allocatedString, config);
  Clay__CloseElement();

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

} // namespace atomic
