#pragma once

#include "ui/renderer/interaction.h"
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <optional>

namespace atomic {

enum class ResizeEdge : uint8_t {
  NoEdge = 0,
  Top = 1 << 0,
  Bottom = 1 << 1,
  Left = 1 << 2,
  Right = 1 << 3,
  TopLeft = Top | Left,
  TopRight = Top | Right,
  BottomLeft = Bottom | Left,
  BottomRight = Bottom | Right
};

inline ResizeEdge operator|(ResizeEdge a, ResizeEdge b) {
  return static_cast<ResizeEdge>(static_cast<uint8_t>(a) |
                                 static_cast<uint8_t>(b));
}

inline bool hasEdge(ResizeEdge mask, ResizeEdge flag) {
  return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(flag)) ==
         static_cast<uint8_t>(flag);
}

struct HandleState {
  ResizeEdge edge = ResizeEdge::NoEdge;
  bool isHovered = false;
  bool isDragging = false;
  glm::vec2 currentSize{0.0f};
  glm::vec2 currentPosition{0.0f};
};

// ⚡ Callback now returns atomic::Interaction from the rendered handle element
using HandleRenderCallback = std::function<void(const HandleState &state)>;

struct ResizeConfig {
  bool enabled = true;
  // Set to TRUE if you want to allow Left/Top resizing (which converts Div to
  // Position::Absolute).
  bool allowAbsolutePositioning = false;
  // 1 & 2. Allowed sides and corners (e.g. Top | Right | BottomRight)
  ResizeEdge allowedEdges =
      ResizeEdge::Right | ResizeEdge::Bottom | ResizeEdge::BottomRight;

  // 4. Proximity thresholds
  float sideProximity = 8.0f;    // Thickness threshold for 4 sides
  float cornerProximity = 12.0f; // Size threshold for 4 corners (12x12)

  // 5. Bounds
  glm::vec2 minSize{40.0f, 40.0f};
  glm::vec2 maxSize{4000.0f, 4000.0f};

  // 3. Optional Custom Callbacks for 8 handles (4 sides + 4 corners)
  HandleRenderCallback onRenderTop;
  HandleRenderCallback onRenderBottom;
  HandleRenderCallback onRenderLeft;
  HandleRenderCallback onRenderRight;
  HandleRenderCallback onRenderTopLeft;
  HandleRenderCallback onRenderTopRight;
  HandleRenderCallback onRenderBottomLeft;
  HandleRenderCallback onRenderBottomRight;

  // Size change event callback
  std::function<void(const glm::vec2 &newSize, const glm::vec2 &newPos)>
      onResize;
};

} // namespace atomic
