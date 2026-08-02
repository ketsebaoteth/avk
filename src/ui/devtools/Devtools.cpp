#include "ui/devtools/Devtools.h"
#include "ui/components.h"
#include "ui/core/frame.h"
#include "ui/internal/context.h"
#include "ui/utils/color.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace atomic {

enum class DockEdge { Left, Right, Top, Bottom };

struct DevToolsState {
  bool initialized = false;
  bool isOpened = false;
  bool isMouseDown = false;
  bool isDragging = false;
  bool hasDragged = false; // Tracks if movement exceeded the drag threshold

  glm::vec2 currentPos{16.0f, 100.0f};
  glm::vec2 targetPos{16.0f, 100.0f};
  glm::vec2 dragStartMousePos{0.0f};
  glm::vec2 dragOffset{0.0f};

  DockEdge activeEdge = DockEdge::Top;
};

static DevToolsState s_dockState;

void drawDevToolsDock(VeraWindow *window) {
  auto *uiState = getUiState();
  auto *app = getVeraApp();
  if (!uiState || !app)
    return;

  uint32_t dockHashId = hashLabel("AtomicDevToolsDock");

  // Get screen bounds
  float screenW = static_cast<float>(getWidth(window));
  float screenH = static_cast<float>(getHeight(window));

  if (screenW <= 100.0f || screenH <= 100.0f) {
    screenW = 1920.0f;
    screenH = 1080.0f;
  }

  constexpr float dockW = 120.0f;
  constexpr float dockH = 38.0f;

  // 1. Initial State: Attached to Top Center
  if (!s_dockState.initialized) {
    s_dockState.currentPos = glm::vec2((screenW - dockW) * 0.5f, 16.0f);
    s_dockState.targetPos = s_dockState.currentPos;
    s_dockState.activeEdge = DockEdge::Top;
    s_dockState.initialized = true;
  }

  // --------------------------------------------------------------------------
  // 1. Button Scale Spring
  // --------------------------------------------------------------------------
  using motion::SpringConfig;

  SpringConfig btnScaleSpring{.mass = 1.0f,
                              .stiffness = 260.0f,
                              .damping = 18.0f,
                              .initialVelocity = 0.0f};

  float targetBtnScale =
      (s_dockState.isMouseDown && !s_dockState.isDragging) ? 0.92f : 1.0f;

  float btnScale = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "btnScale"), targetBtnScale,
      btnScaleSpring);

  // --------------------------------------------------------------------------
  // 2. Apple Asymmetric Panel Open/Close Morph Spring
  // --------------------------------------------------------------------------
  SpringConfig panelMorphSpring =
      s_dockState.isOpened
          ? SpringConfig{.mass = 1.0f,
                         .stiffness = 180.0f, // Smooth open launch
                         .damping = 20.0f, // ζ ≈ 0.74 → Un-rushed Apple spring
                         .initialVelocity = 0.0f}
          : SpringConfig{.mass = 1.0f,
                         .stiffness = 150.0f, // Smooth, heavy close dismissal
                         .damping = 24.0f,    // ζ ≈ 0.98 → Clean landing
                         .initialVelocity = 0.0f};

  float targetProgress = s_dockState.isOpened ? 1.0f : 0.0f;

  float progress = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "panelProgress"), targetProgress,
      panelMorphSpring);

  // --------------------------------------------------------------------------
  // 3. Intersecting Asymmetric Recoil Physics
  // --------------------------------------------------------------------------
  glm::vec2 recoilDir{0.0f};

  if (s_dockState.isOpened) {
    // OPENING: Recoil pushes in the opening direction (towards screen center)
    switch (s_dockState.activeEdge) {
    case DockEdge::Top:
      recoilDir = glm::vec2(0.0f, 1.0f); // Push DOWN (inside)
      break;
    case DockEdge::Bottom:
      recoilDir = glm::vec2(0.0f, -1.0f); // Push UP (inside)
      break;
    case DockEdge::Left:
      recoilDir = glm::vec2(1.0f, 0.0f); // Push RIGHT (inside)
      break;
    case DockEdge::Right:
      recoilDir = glm::vec2(-1.0f, 0.0f); // Push LEFT (inside)
      break;
    }
  } else {
    // CLOSING: Recoil pushes in the closing direction (towards screen border)
    switch (s_dockState.activeEdge) {
    case DockEdge::Top:
      recoilDir = glm::vec2(0.0f, -1.0f); // Push UP (border)
      break;
    case DockEdge::Bottom:
      recoilDir = glm::vec2(0.0f, 1.0f); // Push DOWN (border)
      break;
    case DockEdge::Left:
      recoilDir = glm::vec2(-1.0f, 0.0f); // Push LEFT (border)
      break;
    case DockEdge::Right:
      recoilDir = glm::vec2(1.0f, 0.0f); // Push RIGHT (border)
      break;
    }
  }

  constexpr float kPI = 3.1415926535f;
  float recoilImpulse = std::sin(std::clamp(progress, 0.0f, 1.0f) * kPI);
  float maxRecoilDist = s_dockState.isOpened ? 12.0f : 16.0f;
  float recoilMagnitude = recoilImpulse * maxRecoilDist;

  glm::vec2 bumpOffset = recoilDir * recoilMagnitude;

  // --------------------------------------------------------------------------
  // 4. Dock Position: Instant while Dragging, Smooth Snap Spring on Release
  // --------------------------------------------------------------------------
  SpringConfig dockSpring =
      s_dockState.isDragging
          ? SpringConfig{.mass = 1.0f,
                         .stiffness = 10000.0f,
                         .damping = 200.0f,
                         .initialVelocity = 0.0f} // Instant 1:1 tracking
          : SpringConfig{.mass = 1.0f,
                         .stiffness = 160.0f,
                         .damping = 14.0f,
                         .initialVelocity = 0.0f}; // Edge snap spring

  s_dockState.currentPos.x = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "posX"), s_dockState.targetPos.x,
      dockSpring);

  s_dockState.currentPos.y = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "posY"), s_dockState.targetPos.y,
      dockSpring);

  // Asymmetrical Opacity Logic
  float currentPanelAlpha = 0.0f;
  if (s_dockState.isOpened) {
    currentPanelAlpha = std::clamp((progress - 0.04f) * 6.0f, 0.0f, 1.0f);
  } else {
    currentPanelAlpha = std::clamp((progress - 0.70f) * 4.0f, 0.0f, 1.0f);
  }

  // --------------------------------------------------------------------------
  // RENDER STEP 1: Floating DevTools Panel (Rendered FIRST -> Background in
  // Z-Order)
  // --------------------------------------------------------------------------
  if (currentPanelAlpha > 0.001f) {
    // Target Full Panel Dimensions
    float targetW = std::clamp(screenW * 0.82f, 720.0f, 1200.0f);
    float targetH = std::clamp(screenH * 0.70f, 480.0f, 750.0f);

    float clampedProgress = std::clamp(progress, 0.0f, 1.08f);

    // Axis-Biased Expansion (Sheet vs Drawer Unfolding)
    float panelW = 0.0f;
    float panelH = 0.0f;

    if (s_dockState.activeEdge == DockEdge::Top ||
        s_dockState.activeEdge == DockEdge::Bottom) {
      float startW = std::max(dockW, targetW * 0.88f);
      panelW = (1.0f - clampedProgress) * startW + clampedProgress * targetW;
      panelH = (1.0f - clampedProgress) * dockH + clampedProgress * targetH;
    } else {
      float startH = std::max(dockH, targetH * 0.88f);
      panelW = (1.0f - clampedProgress) * dockW + clampedProgress * targetW;
      panelH = (1.0f - clampedProgress) * startH + clampedProgress * targetH;
    }

    constexpr float kPillRadius = 19.0f;
    constexpr float kCardRadius = 12.0f;
    float currentPanelRadius =
        (1.0f - std::clamp(progress, 0.0f, 1.0f)) * kPillRadius +
        std::clamp(progress, 0.0f, 1.0f) * kCardRadius;

    glm::vec2 pillCenter =
        s_dockState.currentPos + glm::vec2(dockW * 0.5f, dockH * 0.5f);

    // Panel Target Position Movement (EaseOut)
    float desiredPanelX = 0.0f;
    float desiredPanelY = 0.0f;
    constexpr float panelMargin = 12.0f;

    switch (s_dockState.activeEdge) {
    case DockEdge::Top:
      desiredPanelX = pillCenter.x - targetW * 0.5f;
      desiredPanelY = s_dockState.currentPos.y + dockH + panelMargin;
      break;
    case DockEdge::Bottom:
      desiredPanelX = pillCenter.x - targetW * 0.5f;
      desiredPanelY = s_dockState.currentPos.y - targetH - panelMargin;
      break;
    case DockEdge::Left:
      desiredPanelX = s_dockState.currentPos.x + dockW + panelMargin;
      desiredPanelY = pillCenter.y - targetH * 0.5f;
      break;
    case DockEdge::Right:
      desiredPanelX = s_dockState.currentPos.x - targetW - panelMargin;
      desiredPanelY = pillCenter.y - targetH * 0.5f;
      break;
    }

    desiredPanelX = std::clamp(desiredPanelX, 16.0f,
                               std::max(16.0f, screenW - targetW - 16.0f));
    desiredPanelY = std::clamp(desiredPanelY, 16.0f,
                               std::max(16.0f, screenH - targetH - 16.0f));

    float moveDuration = s_dockState.isDragging ? 0.22f : 0.42f;
    float animatedTargetX = uiState->motionManager.animate<float>(
        motion::MotionHandle(dockHashId, "targetPanelX"), desiredPanelX,
        moveDuration, motion::AnimationCurve::EaseOut());

    float animatedTargetY = uiState->motionManager.animate<float>(
        motion::MotionHandle(dockHashId, "targetPanelY"), desiredPanelY,
        moveDuration, motion::AnimationCurve::EaseOut());

    // Morph Position from Pill Center -> Animated Target Position
    glm::vec2 animatedTargetCenter(animatedTargetX + targetW * 0.5f,
                                   animatedTargetY + targetH * 0.5f);
    glm::vec2 currentCenter = (1.0f - clampedProgress) * pillCenter +
                              clampedProgress * animatedTargetCenter;

    float currentPanelX = currentCenter.x - panelW * 0.5f;
    float currentPanelY = currentCenter.y - panelH * 0.5f;

    // Render DevTools Panel
    Div(Modifier()
            .id("AtomicDevToolsPanel")
            .fixed()
            .left(currentPanelX)
            .top(currentPanelY)
            .size(panelW, panelH)
            .background("#ffffff"_hex)
            .subtleShadow(1)
            .border(Colors::gray[200], 1)
            .rounded(currentPanelRadius)
            .color(Colors::black[900])
            .opacity(currentPanelAlpha)
            .column()
            .gap(0),
        [&]() {
          // Panel Header
          Row(Modifier()
                  .padding(20, 12)
                  .background("#fafafa"_hex)
                  .border(Colors::gray[200], {0.0f, 0.0f, 1.0f, 0.0f})
                  .widthGrow()
                  .center(),
              [&]() {
                Row(Modifier().gap(8).center(), [&]() {
                  Icon(LucideIcon::Terminal, Modifier().fontSize(14));
                  Text("Atomic DevTools",
                       Modifier().fontSize(14).fontWeight(600).textWrap(
                           TextWrap::Disabled));
                });

                Div(Modifier().widthGrow());

                if (Button(
                        Modifier()
                            .id("closeDevToolsBtn")
                            .padding(6, 8)
                            .rounded(6.0f),
                        [&]() {
                          Icon(
                              LucideIcon::X,
                              Modifier().color(Colors::gray[400]).fontSize(14));
                        })
                        .clicked) {
                  s_dockState.isOpened = false;
                }
              });

          // ------------------------------------------------------------------
          // DevTools Content Body & Staggered Spring Cards
          // ------------------------------------------------------------------
          Div(Modifier()
                  .background("#ffffff"_hex)
                  .grow()
                  .padding(20)
                  .column()
                  .gap(16),
              [&]() {
                // Card Data Preparation
                char frameTimeBuf[32];
                std::snprintf(frameTimeBuf, sizeof(frameTimeBuf), "%.2f ms",
                              uiState->frameTimeMs);

                std::string fpsStr =
                    std::to_string(static_cast<int>(std::round(uiState->fps)));
                std::string frameTimeStr = frameTimeBuf;
                std::string drawCallsStr = std::to_string(uiState->drawCalls);

                // Row of 3 Stat Cards
                Row(Modifier().widthGrow().gap(16), [&]() {
                  struct CardSpec {
                    LucideIcon icon;
                    std::string label;
                    std::string value;
                    std::string badge;
                    std::string badgeLabel;
                  };

                  std::array<CardSpec, 3> cards = {
                      CardSpec{LucideIcon::Activity, "Performance FPS", fpsStr,
                               frameTimeStr, "Frame Time"},
                      CardSpec{LucideIcon::Layers, "Draw Calls", drawCallsStr,
                               "+12%", "vs last pass"},
                      CardSpec{LucideIcon::Cpu, "Memory Alloc", "18.4 MB",
                               "Normal", "Peak 24.2 MB"}};

                  for (int i = 0; i < 3; ++i) {
                    const auto &c = cards[i];

                    // Staggered Spring Handles (Card 0 pops first, then 1, then
                    // 2)
                    SpringConfig cardSpring{
                        .mass = 1.0f,
                        .stiffness = 240.0f - static_cast<float>(i) * 35.0f,
                        .damping = 18.0f,
                        .initialVelocity = 0.0f};

                    float cardProgress =
                        uiState->motionManager.animateSpring<float>(
                            motion::MotionHandle(dockHashId,
                                                 "cardP_" + std::to_string(i)),
                            targetProgress, cardSpring);

                    float cardAlpha =
                        std::clamp(cardProgress * 2.5f, 0.0f, 1.0f);
                    float cardOffsetY =
                        (1.0f - std::clamp(cardProgress, 0.0f, 1.0f)) * 18.0f;

                    // Render Card
                    Div(Modifier()
                            .grow()
                            .offset(0.0f, cardOffsetY) // Springy Y pop-up
                            .opacity(cardAlpha)        // Fade in
                            .background("#ffffff"_hex)
                            .border(Colors::gray[200], 1)
                            .rounded(12.0f)
                            .padding(16)
                            .column()
                            .gap(12),
                        [&]() {
                          // Top Row: Badge Icon + Label
                          Row(Modifier().widthGrow().center().gap(10), [&]() {
                            Div(Modifier()
                                    .size(32, 32)
                                    .background("#f4f4f5"_hex)
                                    .border(Colors::gray[200], 1)
                                    .rounded(8.0f)
                                    .center(),
                                [&]() {
                                  Icon(c.icon, Modifier().fontSize(14).color(
                                                   Colors::black[900]));
                                });

                            Text(c.label, Modifier()
                                              .fontSize(13)
                                              .fontWeight(600)
                                              .textColor(Colors::black[800])
                                              .textWrap(TextWrap::Disabled));
                          });

                          // Dashed Divider
                          Div(Modifier().widthGrow().height(1).background(
                              Colors::gray[100]));

                          // Big Number Value
                          Text(c.value, Modifier()
                                            .fontSize(28)
                                            .fontWeight(700)
                                            .textColor(Colors::black[950])
                                            .textWrap(TextWrap::Disabled));

                          // Bottom Row: Green Badge + Subtext
                          Row(Modifier().center().gap(6), [&]() {
                            Text(
                                c.badge,
                                Modifier()
                                    .fontSize(12)
                                    .fontWeight(600)
                                    .textColor("#10b981"_hex)); // Emerald green

                            Text(c.badgeLabel,
                                 Modifier()
                                     .fontSize(12)
                                     .fontWeight(400)
                                     .textColor(Colors::gray[400])
                                     .textWrap(TextWrap::Disabled));
                          });
                        });
                  }
                });
              });
        });
  }

  // --------------------------------------------------------------------------
  // RENDER STEP 2: Dock Pill Button (Rendered SECOND -> Foreground in Z-Order)
  // --------------------------------------------------------------------------
  Interaction dockBtn = Div(
      Modifier()
          .id("AtomicDevToolsDockBtn")
          .fixed()
          .left(s_dockState.currentPos.x + bumpOffset.x)
          .top(s_dockState.currentPos.y + bumpOffset.y)
          .scale(btnScale) // Pure scale transformation for touch press
          .background(Colors::black[950])
          .rounded(19.0f) // 38px height / 2 = 19px capsule radius
          .row()
          .center()
          .gap(8)
          .padding(14, 10),
      [&]() {
        Icon(LucideIcon::Monitor, Modifier().color(Colors::white).fontSize(9));
        Text("DevTools",
             Modifier().fontSize(9).fontWeight(400).textColor(Colors::white));
      });

  glm::vec2 mousePos = uiState->pointerPos;

  // Initial Mouse Down on Dock Button
  if (dockBtn.pressed && !s_dockState.isMouseDown) {
    s_dockState.isMouseDown = true;
    s_dockState.hasDragged = false;
    s_dockState.dragStartMousePos = mousePos;
    s_dockState.dragOffset = mousePos - s_dockState.currentPos;
  }

  // Handle Mouse Dragging & Edge Snapping
  if (s_dockState.isMouseDown) {
    if (uiState->pointerDown) {
      constexpr float kDragThreshold = 5.0f;
      if (!s_dockState.isDragging &&
          glm::distance(mousePos, s_dockState.dragStartMousePos) >
              kDragThreshold) {
        s_dockState.isDragging = true;
        s_dockState.hasDragged = true;
      }

      if (s_dockState.isDragging) {
        s_dockState.targetPos.x =
            std::clamp(mousePos.x - s_dockState.dragOffset.x, 8.0f,
                       screenW - dockW - 8.0f);
        s_dockState.targetPos.y =
            std::clamp(mousePos.y - s_dockState.dragOffset.y, 8.0f,
                       screenH - dockH - 8.0f);
      }
    } else {
      // Releasing Pointer: Snap to nearest edge
      if (s_dockState.isDragging) {
        s_dockState.isDragging = false;

        float dLeft = s_dockState.targetPos.x;
        float dRight = screenW - (s_dockState.targetPos.x + dockW);
        float dTop = s_dockState.targetPos.y;
        float dBottom = screenH - (s_dockState.targetPos.y + dockH);

        float minD = std::min({dLeft, dRight, dTop, dBottom});

        if (minD == dLeft) {
          s_dockState.activeEdge = DockEdge::Left;
          s_dockState.targetPos.x = 16.0f;
        } else if (minD == dRight) {
          s_dockState.activeEdge = DockEdge::Right;
          s_dockState.targetPos.x = screenW - dockW - 16.0f;
        } else if (minD == dTop) {
          s_dockState.activeEdge = DockEdge::Top;
          s_dockState.targetPos.y = 16.0f;
        } else {
          s_dockState.activeEdge = DockEdge::Bottom;
          s_dockState.targetPos.y = screenH - dockH - 16.0f;
        }
      }
      s_dockState.isMouseDown = false;
    }
  }

  // Toggle Popup Panel on Click (Only if user did NOT drag)
  if (dockBtn.clicked) {
    if (!s_dockState.hasDragged) {
      s_dockState.isOpened = !s_dockState.isOpened;
    }
    s_dockState.hasDragged = false;
  }
}

} // namespace atomic
