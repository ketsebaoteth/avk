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

  // Force Clay to allocate and copy
  Clay_String src{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(text.size()),
                  .chars = text.data()};
  return Clay__WriteStringToCharBuffer(&context->dynamicStringData, src);
}

} // namespace atomic
