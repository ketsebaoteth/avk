#pragma once
#include "avk/atomic_ui.h"
#include "clay.h"
#include <cstring>
#include <print>

namespace utils::layout {

inline void handleClayError(Clay_ErrorData error) {
  std::println("[Clay Layout]: {}", error.errorText.chars);
}

atomic::UIState *getUiState();
uint32_t &getElementIdCounter();

inline Clay_ElementId getNextId(const char *label) {
  char buffer[64];

  // Fetch the centralized global reference and post-increment it
  uint32_t currentId = getElementIdCounter()++;
  std::snprintf(buffer, sizeof(buffer), "%s_%u", label, currentId);

  return Clay_GetElementId(
      Clay_String{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(std::strlen(buffer)),
                  .chars = buffer});
}
} // namespace utils::layout
