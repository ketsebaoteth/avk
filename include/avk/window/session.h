#pragma once
#include "avk/avk_canvas.h"
#include "core/app/Types.h"
#include "volk.h"
#include <chrono>
#include <memory>

namespace window {
struct WindowSession {
  VeraWindow *window = nullptr;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  std::unique_ptr<avk::WindowCanvas> canvas;
  std::chrono::high_resolution_clock::time_point lastTime;
  float lastDeltaTime = 0.016f;
};
} // namespace window
