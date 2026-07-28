#define CLAY_IMPLEMENTATION
#include "clay.h"
#include <string>

namespace atomic {

Clay_String copyStringToClayBuffer(const std::string &text) {
  Clay_Context *context = Clay_GetCurrentContext();
  if (!context) {
    return Clay_String{
        .isStaticallyAllocated = false, .length = 0, .chars = nullptr};
  }

  Clay_String clayString{.isStaticallyAllocated = false,
                         .length = static_cast<int32_t>(text.size()),
                         .chars = text.c_str()};

  // Safely copies the string data into Clay's frame-allocated dynamic arena
  return Clay__WriteStringToCharBuffer(&context->dynamicStringData, clayString);
}

} // namespace atomic
