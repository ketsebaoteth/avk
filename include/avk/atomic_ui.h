#pragma once

#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "clay.h"
#include "core/app/Types.h"
#include "glm/fwd.hpp"
#include "window/session.h"
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>

// Forward declare Vera structures
class VeraWindow;

namespace atomic {

struct UIState {
  std::unique_ptr<avk::VulkanContext> context;
  std::unique_ptr<avk::Renderer> renderer;
  void *clayArenaMemory = nullptr;
  std::vector<window::WindowSession> sessions;

  glm::vec2 pointerPos = glm::vec2(0.0f);
  bool pointerPressed = false;
  std::vector<std::unique_ptr<avk::Font>> fonts;

  window::WindowSession *findSession(VeraWindow *window) {
    auto result = std::find_if(sessions.begin(), sessions.end(),
                               [window](const window::WindowSession &session) {
                                 return session.window == window;
                               });
    return (result != sessions.end()) ? &(*result) : nullptr;
  }
};

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

/**
 * @brief Chaining Modifier holding rendering and alignment styles.
 */
struct Style {
  glm::vec4 backgroundColor = glm::vec4(0.0f);
  glm::vec4 borderRadius = glm::vec4(0.0f);
  glm::vec4 strokeColor = glm::vec4(0.0f);
  float strokeThickness = 0.0f;

  float width = 0.0f;
  float height = 0.0f;
  bool hasWidth = false;
  bool hasHeight = false;

  glm::vec4 textColor = glm::vec4(1.0f);
  float textOffset = -3.0f;

  uint16_t padLeft = 0;
  uint16_t padRight = 0;
  uint16_t padTop = 0;
  uint16_t padBottom = 0;

  uint16_t childGap = 0;

  AlignmentX alignX = AlignmentX::Left;
  AlignmentY alignY = AlignmentY::Top;
};

/**
 * @brief Lightweight, implicit-bool convertible window interaction state block.
 */
struct Interaction {
  bool clicked = false;
  bool hovered = false;
  bool pressed = false;

  explicit operator bool() const { return clicked; }
};

class Modifier {
public:
  Modifier() = default;

  Modifier background(const glm::vec4 &color) && {
    m_style.backgroundColor = color;
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
    m_style.width = width;
    m_style.height = height;
    // if has a value of -1.0f a grow is assumed instead of it being treated
    // like actuall dimensions
    if (width != -1.0f) {
      m_style.hasWidth = true;
    }
    if (height != -1.0f) {
      m_style.hasHeight = true;
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

void Column(Modifier &&modifier, const std::function<void()> &content);
void Row(Modifier &&modifier, const std::function<void()> &content);
uint32_t loadTexture(const std::string &path);
void unloadTexture(uint32_t textureIndex);
avk::Font *getFont(uint32_t fontId);
uint32_t loadFont(const std::string &path, uint32_t fontSize);

} // namespace atomic
