#pragma once

#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "clay.h"
#include "core/app/Types.h"
#include "window/session.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class VeraWindow;

namespace atomic {

/**
 * @brief Persistent state block tracking text input cursor and selection
 * metrics.
 */
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
  bool isDraggingY = false;
  float dragStartY = 0.0f;
  float dragStartScrollY = 0.0f;
};

/**
 * @brief Specifies CSS object-fit layout behaviors for images and textures.
 */
enum class ObjectFit : uint8_t { Fill, Contain, Cover, Custom };

/**
 * @brief Layout direction for generic container elements (Div).
 */
enum class LayoutDirection : uint8_t { Row, Column };

/**
 * @brief Horizontal alignment modes for container child layout.
 */
enum class AlignmentX : uint8_t { Left, Center, Right, SpaceBetween };

/**
 * @brief Vertical alignment modes for container child layout.
 */
enum class AlignmentY : uint8_t { Top, Center, Bottom, SpaceBetween };

/**
 * @brief Combined 2D alignment configuration block.
 */
struct Alignment {
  AlignmentX x = AlignmentX::Left;
  AlignmentY y = AlignmentY::Top;
};

/**
 * @brief Position context models for layout elements.
 */
enum class Position : uint8_t { Normal, Relative, Absolute, Fixed };

/**
 * @brief Attachment anchor points for floating and overlay elements.
 */
enum class AttachPoint : uint8_t {
  TopLeft,
  TopCenter,
  TopRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
  CenterLeft,
  Center,
  CenterRight
};

/**
 * @brief Per-frame instance payload generated during layout traversal.
 */
struct RenderPayload {
  uint32_t textureIndex = 0;
  glm::vec4 tintColor{1.0f};
  glm::vec4 uvBounds{0.0f, 0.0f, 1.0f, 1.0f};
  ObjectFit objectFit = ObjectFit::Fill;

  float scale = 1.0f;
  float rotation = 0.0f;
  float blur = 0.0f;
  glm::vec2 transformOrigin{0.5f, 0.5f};
  glm::vec2 translate{0.0f, 0.0f};
  float textOffset = 0.0f;
};

/**
 * @brief Styles that cascade and propagate down the layout hierarchy.
 */
struct CascadingStyle {
  glm::vec4 textColor = glm::vec4(1.0f);
  uint32_t fontId = 0;
  float textOffset = 0.0f;
  float inheritedOpacity = 1.0f;
  bool pointerEvents = true;
  bool disabled = false;
};

/**
 * @brief Resolved pixel geometry computed post-layout by the layout engine.
 */
struct ComputedLayout {
  glm::vec2 position = glm::vec2(0.0f);
  glm::vec2 size = glm::vec2(0.0f);
  glm::vec4 padding = glm::vec4(0.0f);
  bool found = false;

  [[nodiscard]] float width() const { return size.x; }
  [[nodiscard]] float height() const { return size.y; }
  [[nodiscard]] float x() const { return position.x; }
  [[nodiscard]] float y() const { return position.y; }
  [[nodiscard]] glm::vec2 center() const { return position + (size * 0.5f); }
};

/**
 * @brief Master runtime UI engine context state.
 */
struct UIState {
  std::unique_ptr<avk::VulkanContext> context;
  std::unique_ptr<avk::Renderer> renderer;
  void *clayArenaMemory = nullptr;
  std::vector<window::WindowSession> sessions;
  std::vector<std::unique_ptr<RenderPayload>> framePayloads;
  std::vector<uint32_t> positioningContextStack;
  std::vector<CascadingStyle> cascadingStyleStack;
  std::unordered_map<uint32_t, CascadingStyle> computedStyleMap;

  glm::vec2 pointerPos = glm::vec2(0.0f);
  bool pointerPressed = false;
  bool pointerDown = false;
  std::vector<std::unique_ptr<avk::Font>> fonts;
  std::unordered_map<uint32_t, InputState> inputStateMap;
  std::unordered_map<uint32_t, ScrollViewState> scrollViewStates;

  uint32_t defaultFontId = 0;
  std::array<uint32_t, 5> defaultIconFontIds;
  std::unordered_map<std::string, uint32_t> iconMap;

  uint32_t focusedElementId = 0;
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

  [[nodiscard]] CascadingStyle getActiveCascadingStyle() const {
    if (!cascadingStyleStack.empty()) {
      return cascadingStyleStack.back();
    }
    return CascadingStyle{};
  }

  window::WindowSession *findSession(VeraWindow *window) {
    auto result = std::find_if(sessions.begin(), sessions.end(),
                               [window](const window::WindowSession &session) {
                                 return session.window == window;
                               });
    return (result != sessions.end()) ? &(*result) : nullptr;
  }
};

/**
 * @brief Comprehensive styling definition block backing fluent modifiers.
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
  std::optional<bool> disabled;
  std::optional<float> opacity;

  std::optional<uint16_t> childGap = 5.0f;
  std::optional<AlignmentX> alignX;
  std::optional<AlignmentY> alignY;
  std::optional<LayoutDirection> direction;

  std::optional<float> scale;
  std::optional<float> rotation;
  std::optional<float> blur;

  std::optional<Position> position;
  std::optional<float> left;
  std::optional<float> right;
  std::optional<float> top;
  std::optional<float> bottom;

  std::optional<glm::vec2> transformOrigin;
  std::optional<glm::vec2> translate;
  std::optional<uint32_t> parentId;

  std::optional<AttachPoint> elementAttach;
  std::optional<AttachPoint> parentAttach;
  std::optional<glm::vec2> offset;
  std::optional<ObjectFit> objectFit;
  std::optional<glm::vec4> uvBounds;
};

/**
 * @brief Interaction result payload returned by interactive primitives.
 */
struct Interaction {
  bool clicked = false;
  bool hovered = false;
  bool pressed = false;
  bool changed = false;

  explicit operator bool() const { return clicked || changed; }
};

/**
 * @brief Fluent chaining modifier interface holding layout and visual styles.
 */
class Modifier {
public:
  Modifier() = default;

  Modifier background(const glm::vec4 &color) && {
    m_style.backgroundColor = color;
    return std::move(*this);
  }

  Modifier color(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  Modifier textColor(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  Modifier textOffset(float y) && {
    m_style.textOffset = y;
    return std::move(*this);
  }

  Modifier opacity(float alpha) && {
    m_style.opacity = alpha;
    return std::move(*this);
  }

  Modifier disabled(bool isDisabled = true) && {
    m_style.disabled = isDisabled;
    return std::move(*this);
  }

  Modifier direction(LayoutDirection dir) && {
    m_style.direction = dir;
    return std::move(*this);
  }

  Modifier row() && {
    m_style.direction = LayoutDirection::Row;
    return std::move(*this);
  }

  Modifier column() && {
    m_style.direction = LayoutDirection::Column;
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

  Modifier blur(float b) && {
    m_style.blur = b;
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

  Modifier fixed() && {
    m_style.position = Position::Fixed;
    return std::move(*this);
  }

  Modifier parentId(uint32_t id) && {
    m_style.parentId = id;
    return std::move(*this);
  }

  Modifier attach(AttachPoint elementPt, AttachPoint parentPt) && {
    m_style.elementAttach = elementPt;
    m_style.parentAttach = parentPt;
    return std::move(*this);
  }

  Modifier offset(float x, float y) && {
    m_style.offset = glm::vec2(x, y);
    return std::move(*this);
  }

  Modifier offset(const glm::vec2 &off) && {
    m_style.offset = off;
    return std::move(*this);
  }

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

  Modifier size(float width, float height) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  Modifier width(float width) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    return std::move(*this);
  }

  Modifier height(float height) && {
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  Modifier rounded(float radius) && {
    m_style.borderRadius = glm::vec4(radius);
    return std::move(*this);
  }

  Modifier rounded(const glm::vec4 &radii) && {
    m_style.borderRadius = radii;
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

  Modifier padding(uint16_t left, uint16_t right, uint16_t top,
                   uint16_t bottom) && {
    m_style.padLeft = left;
    m_style.padRight = right;
    m_style.padTop = top;
    m_style.padBottom = bottom;
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

  Modifier childAlignment(Alignment alignment) && {
    m_style.alignX = alignment.x;
    m_style.alignY = alignment.y;
    return std::move(*this);
  }

  Modifier objectFit(ObjectFit fit) && {
    m_style.objectFit = fit;
    return std::move(*this);
  }

  Modifier cover() && {
    m_style.objectFit = ObjectFit::Cover;
    return std::move(*this);
  }

  Modifier contain() && {
    m_style.objectFit = ObjectFit::Contain;
    return std::move(*this);
  }

  Modifier uv(float minU, float minV, float maxU, float maxV) && {
    m_style.uvBounds = glm::vec4(minU, minV, maxU, maxV);
    return std::move(*this);
  }

  Modifier uv(const glm::vec4 &bounds) && {
    m_style.uvBounds = bounds;
    return std::move(*this);
  }

  [[nodiscard]] const Style &getStyle() const { return m_style; }

private:
  Style m_style;
};

inline Modifier DefaultModifier() { return Modifier{}; }

/** @brief Initializes the atomicUI engine context and renderer. */
void initialize(std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation = false);

/** @brief Shuts down the engine and releases all Vulkan resources. */
void shutdown();

/** @brief Maps a native Vera window session into the UI context. */
void registerWindow(VeraWindow *window);

/** @brief Unregisters and destroys a window session context. */
void unregisterWindow(VeraWindow *window);

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

/** @brief Loads an image texture into bindless descriptor slot memory. */
uint32_t loadTexture(const std::string &path);

/** @brief Unloads a texture slot from descriptor memory. */
void unloadTexture(uint32_t textureIndex);

/** @brief Fetches raw font instance by font ID. */
avk::Font *getFont(uint32_t fontId);

/** @brief Returns the default system font ID. */
uint32_t getDefaultFontId();

/** @brief Loads a TrueType font file at a specific pixel size. */
uint32_t loadFont(const std::string &path, uint32_t fontSize);

/** @brief Loads a TrueType font file with explicit unicode codepoint targets.
 */
uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints);

/** @brief Finds closest icon font size tier for rasterization. */
uint32_t getClosestIconFontId(float requestedSize);

/** @brief Returns true if a text input box currently holds keyboard focus. */
bool isKeyboardCaptured();

/** @brief Clears global keyboard input focus. */
void clearKeyboardFocus();

/** @brief Safely copies a C++ string into Clay's frame scratchpad. */
Clay_String copyStringToClayBuffer(const std::string &text);

} // namespace atomic
