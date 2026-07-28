#pragma once

#include "avk/window/session.h"
#include "ui/internal/context.h"
namespace atomic {
inline void calcFrameDeltaTime(window::WindowSession *session) {
  if (!session)
    return;

  auto currentTime = std::chrono::high_resolution_clock::now();
  session->lastDeltaTime =
      std::chrono::duration<float>(currentTime - session->lastTime).count();
  session->lastTime = currentTime;
}

inline uint32_t getClosestIconFontId(float requestedSize) {
  if (!getUiState())
    return 0;

  static constexpr std::array<float, 5> TIERS = {12.0f, 16.0f, 24.0f, 32.0f,
                                                 48.0f};

  uint32_t closestIndex = 1;
  float minDiff = std::abs(requestedSize - TIERS[1]);

  for (size_t i = 0; i < TIERS.size(); ++i) {
    float diff = std::abs(requestedSize - TIERS[i]);
    if (diff < minDiff) {
      minDiff = diff;
      closestIndex = static_cast<uint32_t>(i);
    }
  }

  return getUiState()->defaultIconFontIds[closestIndex];
}

inline static uint32_t decodeNextUtf8(const char *chars, int32_t length,
                                      uint32_t &index) {
  if (index >= static_cast<uint32_t>(length))
    return 0;

  unsigned char c = chars[index++];

  if (c < 0x80)
    return c;

  if ((c & 0xE0) == 0xC0) {
    if (index >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x1F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  if ((c & 0xF0) == 0xE0) {
    if (index + 1 >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x0F) << 12;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  if ((c & 0xF8) == 0xF0) {
    if (index + 2 >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x07) << 18;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 12;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  return c;
}

} // namespace atomic
