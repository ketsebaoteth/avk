#pragma once

#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "clay.h"
#include "core/app/Types.h"
#include "glm/ext/vector_float2.hpp"
#include "glm/fwd.hpp"
#include "window/session.h"
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>

// Forward declare Vera structures
class VeraWindow;

namespace atomic {

struct InputState {
  uint32_t cursorPosition = 0;
  uint32_t selectionStart = 0;
  uint32_t selectionEnd = 0;
  uint32_t selectionAnchor = 0;
  bool isDraggingText = false;
};
/**
 * @brief Persistent state block tracking scroll offsets and drag interactions.
 */
struct ScrollViewState {
  float scrollOffsetX = 0.0f;
  float scrollOffsetY = 0.0f;
  float targetScrollOffsetX = 0.0f;
  float targetScrollOffsetY = 0.0f;

  // Interactive drag-to-scroll metrics
  bool isDraggingY = false;
  float dragStartY = 0.0f;
  float dragStartScrollY = 0.0f;
};
struct RenderPayload {
  uint32_t textureIndex = 0;
  glm::vec4 tintColor = glm::vec4(1.0f);
  float scale = 1.0f;
  float rotation = 0.0f;
  float blur = 0.0f;
  glm::vec2 transformOrigin = glm::vec2(0.5f, 0.5f);
  glm::vec2 translate = glm::vec2(0.0f, 0.0f);
  float textOffset = 0.0f;
};
struct UIState {
  std::unique_ptr<avk::VulkanContext> context;
  std::unique_ptr<avk::Renderer> renderer;
  void *clayArenaMemory = nullptr;
  std::vector<window::WindowSession> sessions;
  std::vector<std::unique_ptr<RenderPayload>> framePayloads;
  std::vector<uint32_t> positioningContextStack;

  glm::vec2 pointerPos = glm::vec2(0.0f);
  bool pointerPressed = false;
  bool pointerDown = false;
  std::vector<std::unique_ptr<avk::Font>> fonts;
  std::unordered_map<uint32_t, InputState> inputStateMap;
  std::unordered_map<uint32_t, ScrollViewState> scrollViewStates;

  uint32_t defaultFontId = 0;
  std::array<uint32_t, 5> defaultIconFontIds;
  std::unordered_map<std::string, uint32_t> iconMap;

  uint32_t focusedElementId = 0; // 0 means no active focus
  std::vector<uint32_t> capturedChars;
  bool backspacePressed = false;
  bool enterPressed = false;
  bool anyInputBoxHovered = false;
  float mouseWheelDeltaX = 0.0f;
  float mouseWheelDeltaY = 0.0f;

  bool selectAll = false;
  bool doingShiftSelect = false;

  bool deletePressed = false;
  bool leftArrowPressed = false;
  bool rightArrowPressed = false;
  bool ctrlPressed = false;
  bool shiftPressed = false;

  window::WindowSession *findSession(VeraWindow *window) {
    auto result = std::find_if(sessions.begin(), sessions.end(),
                               [window](const window::WindowSession &session) {
                                 return session.window == window;
                               });
    return (result != sessions.end()) ? &(*result) : nullptr;
  }
};
uint32_t getClosestIconFontId(float requestedSize);
bool isKeyboardCaptured();

void clearKeyboardFocus();
/**
 * @brief Internal helper to safely copy C++ strings into Clay's frame-allocated
 * scratchpad. Prevents dangling pointers during immediate-mode text layouts.
 */
Clay_String copyStringToClayBuffer(const std::string &text);

struct ImagePayload {
  uint32_t textureIndex;
  glm::vec4 tintColor;
};

enum class AlignmentX : uint8_t { Left, Center, Right, SpaceBetween };
enum class AlignmentY : uint8_t { Top, Center, Bottom, SpaceBetween };
struct Alignment {
  AlignmentX x = AlignmentX::Left;
  AlignmentY y = AlignmentY::Top;
};

enum class Position : uint8_t {
  Normal,   // Standard layout flow (like CSS static)
  Relative, // Layout flow, but establishes positioning context for absolute
            // children
  Absolute, // out-of-flow, positioned relative to nearest Relative/Absolute
            // parent or window
  Fixed     // out-of-flow, strictly positioned relative to the window viewport
            // (0,0)
};

/**
 * @brief Chaining Modifier holding rendering and alignment styles.
 */
struct Style {
  std::optional<glm::vec4> backgroundColor;
  std::optional<glm::vec4> borderRadius;
  std::optional<glm::vec4> strokeColor;
  std::optional<float> strokeThickness;

  std::optional<float> width;
  std::optional<float> height;

  std::optional<glm::vec4> textColor;
  std::optional<float> textOffset;

  std::optional<uint16_t> padLeft;
  std::optional<uint16_t> padRight;
  std::optional<uint16_t> padTop;
  std::optional<uint16_t> padBottom;
  std::optional<bool> pointerEvents;

  std::optional<uint16_t> childGap = 5.0f;
  std::optional<AlignmentX> alignX;
  std::optional<AlignmentY> alignY;

  std::optional<float> scale;
  std::optional<float> rotation;
  std::optional<float> blur;

  std::optional<Position> position;
  std::optional<float> left;
  std::optional<float> right;
  std::optional<float> top;
  std::optional<float> bottom;

  // NEW: Transform modifications
  std::optional<glm::vec2> transformOrigin; // Default (0.5, 0.5) for Center
  std::optional<glm::vec2> translate;       // post-layout pixel offsets (x, y)
  std::optional<uint32_t> parentId;
};

/**
 * @brief Lightweight, implicit-bool convertible window interaction state block.
 */
struct Interaction {
  bool clicked = false;
  bool hovered = false;
  bool pressed = false;
  // for select
  bool changed = false;

  explicit operator bool() const { return clicked || changed; }
};

class Modifier {
public:
  Modifier() = default;

  Modifier background(const glm::vec4 &color) && {
    m_style.backgroundColor = color;
    return std::move(*this);
  }
  Modifier scale(float s) && {
    m_style.scale = s;
    return std::move(*this);
  }

  Modifier rotation(float r) && {
    m_style.rotation = r;
    return std::move(*this);
  }
  Modifier pointerEvents(bool capture) && {
    m_style.pointerEvents = capture;
    return std::move(*this);
  }

  Modifier relative() && {
    m_style.position = Position::Relative;
    return std::move(*this);
  }
  Modifier absolute() && {
    m_style.position = Position::Absolute;
    return std::move(*this);
  }

  // Sets position mode to Fixed (relative strictly to the screen viewport)
  Modifier fixed() && {
    m_style.position = Position::Fixed;
    return std::move(*this);
  }

  // Individual CSS positioning properties
  Modifier left(float val) && {
    m_style.left = val;
    return std::move(*this);
  }

  Modifier right(float val) && {
    m_style.right = val;
    return std::move(*this);
  }

  Modifier top(float val) && {
    m_style.top = val;
    return std::move(*this);
  }

  Modifier bottom(float val) && {
    m_style.bottom = val;
    return std::move(*this);
  }

  Modifier transformOrigin(float x, float y) && {
    m_style.transformOrigin = glm::vec2(x, y);
    return std::move(*this);
  }

  Modifier translate(float x, float y) && {
    m_style.translate = glm::vec2(x, y);
    return std::move(*this);
  }
  Modifier blur(float b) && {
    m_style.blur = b;
    return std::move(*this);
  }

  Modifier color(const glm::vec4 &color) && {
    m_style.textColor = color;
    return std::move(*this);
  }
  Modifier textOffset(float y) && {
    m_style.textOffset = y;
    return std::move(*this);
  }

  Modifier size(float width, float height) && {
    if (width != -1.0f) {
      m_style.width = width;
    } else {
      m_style.width = std::nullopt;
    }

    if (height != -1.0f) {
      m_style.height = height;
    } else {
      m_style.height = std::nullopt;
    }

    return std::move(*this);
  }

  Modifier rounded(float radius) && {
    m_style.borderRadius = glm::vec4(radius);
    return std::move(*this);
  }

  Modifier border(const glm::vec4 &color, float thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = thickness;
    return std::move(*this);
  }

  Modifier padding(uint16_t horizontal, uint16_t vertical) && {
    m_style.padLeft = horizontal;
    m_style.padRight = horizontal;
    m_style.padTop = vertical;
    m_style.padBottom = vertical;
    return std::move(*this);
  }

  Modifier gap(uint16_t spacing) && {
    m_style.childGap = spacing;
    return std::move(*this);
  }
  Modifier alignX(AlignmentX alignment) && {
    m_style.alignX = alignment;
    return std::move(*this);
  }

  Modifier alignY(AlignmentY alignment) && {
    m_style.alignY = alignment;
    return std::move(*this);
  }

  Modifier center() && {
    m_style.alignX = AlignmentX::Center;
    m_style.alignY = AlignmentY::Center;
    return std::move(*this);
  }
  Modifier childAlignment(Alignment alignement) {
    m_style.alignX = alignement.x;
    m_style.alignY = alignement.y;
    return std::move(*this);
  }

  const Style &getStyle() const { return m_style; }

private:
  Style m_style;
};

inline Modifier DefaultModifier() { return Modifier{}; }

void initialize(std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation = false);
void shutdown();

void registerWindow(VeraWindow *window);
void unregisterWindow(VeraWindow *window);

bool beginFrame(VeraWindow *window);
void endFrame(VeraWindow *window);

void resizeWindow(VeraWindow *window, uint32_t width, uint32_t height);

uint32_t getWidth(VeraWindow *window);
uint32_t getHeight(VeraWindow *window);

uint32_t loadTexture(const std::string &path);
void unloadTexture(uint32_t textureIndex);
avk::Font *getFont(uint32_t fontId);
uint32_t getDefaultFontId();
uint32_t loadFont(const std::string &path, uint32_t fontSize);
// overload to load with codepoints
uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints);

} // namespace atomic
