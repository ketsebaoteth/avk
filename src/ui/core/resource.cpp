#include "avk/utils/ui/layout.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"

namespace atomic {

uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;

  auto font = std::make_unique<avk::Font>(uiState->context.get(), getPath(path),
                                          fontSize, codepoints);
  uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

uint32_t loadFont(const std::string &path, uint32_t fontSize) {
  return loadFont(path, fontSize, {});
}

uint32_t loadTexture(const std::string &path) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;
  return uiState->context->getTextureManager()->loadTexture(getPath(path));
}

void unloadTexture(uint32_t textureIndex) {
  auto uiState = getUiState();
  if (!uiState)
    return;
  uiState->context->getTextureManager()->unloadTexture(textureIndex);
}

avk::Font *getFont(uint32_t fontId) {
  auto uiState = getUiState();
  if (!uiState || fontId >= uiState->fonts.size()) {
    return nullptr;
  }
  return uiState->fonts[fontId].get();
}

bool isKeyboardCaptured() {
  auto uiState = getUiState();
  return uiState && uiState->focusedElementId != 0;
}

void clearKeyboardFocus() {
  auto uiState = getUiState();
  if (uiState) {
    uiState->focusedElementId = 0;
  }
}
} // namespace atomic
