#pragma once
#include "clay.h"
#include "glm/glm.hpp"
#include <cstdint>

class VeraWindow;
namespace atomic {

/** @brief Begins a new UI frame pass for a window. */
bool beginFrame(VeraWindow *window);

/** @brief Ends layout evaluation and submits rendering instances to GPU. */
void endFrame(VeraWindow *window);

/** @brief Resizes swapchain and dynamic viewport allocations. */
void resizeWindow(VeraWindow *window, uint32_t width, uint32_t height);

/** @brief Returns active session viewport width in physical pixels. */
uint32_t getWidth(VeraWindow *window);

/** @brief Returns active session viewport height in physical pixels. */
uint32_t getHeight(VeraWindow *window);

} // namespace atomic
