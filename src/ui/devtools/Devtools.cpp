#include "ui/devtools/Devtools.h"
#include "ui/components.h"
#include "ui/core/frame.h"
#include "ui/internal/context.h"
#include "ui/style/themeManager.h"
#include "ui/utils/color.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace atomic {

enum class DockEdge { Left, Right, Top, Bottom };
enum class DevToolsMainTab { Profiling, Styling };

struct DevToolsState {
  bool initialized = false;
  bool isOpened = false;
  bool isMouseDown = false;
  bool isDragging = false;
  bool hasDragged = false;

  glm::vec2 currentPos{16.0f, 100.0f};
  glm::vec2 targetPos{16.0f, 100.0f};
  glm::vec2 dragStartMousePos{0.0f};
  glm::vec2 dragOffset{0.0f};

  DockEdge activeEdge = DockEdge::Top;

  // Tab State
  DevToolsMainTab activeMainTab = DevToolsMainTab::Profiling;
  std::string activeStyleSubTab = ":root";
  std::string cssFilePath = "assets/styles/default_theme.css";
  std::unordered_map<std::string, std::string> propertyBuffers;
};

static DevToolsState s_dockState;

static std::string formatVarValue(const DenseThemeVariable &var) {
  switch (var.type) {
  case ThemeVariableType::Color: {
    auto col = std::get<glm::vec4>(var.value);
    std::stringstream ss;
    ss << "#" << std::hex << std::setfill('0') << std::setw(2)
       << static_cast<int>(col.r * 255.0f) << std::setw(2)
       << static_cast<int>(col.g * 255.0f) << std::setw(2)
       << static_cast<int>(col.b * 255.0f) << std::setw(2)
       << static_cast<int>(col.a * 255.0f);
    return ss.str();
  }
  case ThemeVariableType::Float: {
    auto val = std::get<float>(var.value);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << val;
    return ss.str();
  }
  case ThemeVariableType::Boolean: {
    return std::get<bool>(var.value) ? "true" : "false";
  }
  case ThemeVariableType::Integer: {
    return std::to_string(std::get<int32_t>(var.value));
  }
  }
  return "";
}

void drawDevToolsDock(VeraWindow *window) {
  auto *uiState = getUiState();
  auto *app = getVeraApp();
  if (!uiState || !app)
    return;

  uint32_t dockHashId = hashLabel("AtomicDevToolsDock");

  float screenW = static_cast<float>(getWidth(window));
  float screenH = static_cast<float>(getHeight(window));

  if (screenW <= 100.0f || screenH <= 100.0f) {
    screenW = 1920.0f;
    screenH = 1080.0f;
  }

  constexpr float dockW = 120.0f;
  constexpr float dockH = 38.0f;

  if (!s_dockState.initialized) {
    s_dockState.currentPos = glm::vec2((screenW - dockW) * 0.5f, 16.0f);
    s_dockState.targetPos = s_dockState.currentPos;
    s_dockState.activeEdge = DockEdge::Top;
    s_dockState.initialized = true;
  }

  // 1. Button Scale Spring
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

  // 2. Panel Morph Spring
  SpringConfig panelMorphSpring = s_dockState.isOpened
                                      ? SpringConfig{.mass = 1.0f,
                                                     .stiffness = 180.0f,
                                                     .damping = 20.0f,
                                                     .initialVelocity = 0.0f}
                                      : SpringConfig{.mass = 1.0f,
                                                     .stiffness = 150.0f,
                                                     .damping = 24.0f,
                                                     .initialVelocity = 0.0f};

  float targetProgress = s_dockState.isOpened ? 1.0f : 0.0f;
  float progress = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "panelProgress"), targetProgress,
      panelMorphSpring);

  // 3. Recoil Impulse
  glm::vec2 recoilDir{0.0f};
  if (s_dockState.isOpened) {
    switch (s_dockState.activeEdge) {
    case DockEdge::Top:
      recoilDir = glm::vec2(0.0f, 1.0f);
      break;
    case DockEdge::Bottom:
      recoilDir = glm::vec2(0.0f, -1.0f);
      break;
    case DockEdge::Left:
      recoilDir = glm::vec2(1.0f, 0.0f);
      break;
    case DockEdge::Right:
      recoilDir = glm::vec2(-1.0f, 0.0f);
      break;
    }
  } else {
    switch (s_dockState.activeEdge) {
    case DockEdge::Top:
      recoilDir = glm::vec2(0.0f, -1.0f);
      break;
    case DockEdge::Bottom:
      recoilDir = glm::vec2(0.0f, 1.0f);
      break;
    case DockEdge::Left:
      recoilDir = glm::vec2(-1.0f, 0.0f);
      break;
    case DockEdge::Right:
      recoilDir = glm::vec2(1.0f, 0.0f);
      break;
    }
  }

  constexpr float kPI = 3.1415926535f;
  float recoilImpulse = std::sin(std::clamp(progress, 0.0f, 1.0f) * kPI);
  float maxRecoilDist = s_dockState.isOpened ? 12.0f : 16.0f;
  glm::vec2 bumpOffset = recoilDir * (recoilImpulse * maxRecoilDist);

  // 4. Dock Position tracking
  SpringConfig dockSpring = s_dockState.isDragging
                                ? SpringConfig{.mass = 1.0f,
                                               .stiffness = 10000.0f,
                                               .damping = 200.0f,
                                               .initialVelocity = 0.0f}
                                : SpringConfig{.mass = 1.0f,
                                               .stiffness = 160.0f,
                                               .damping = 14.0f,
                                               .initialVelocity = 0.0f};

  s_dockState.currentPos.x = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "posX"), s_dockState.targetPos.x,
      dockSpring);
  s_dockState.currentPos.y = uiState->motionManager.animateSpring<float>(
      motion::MotionHandle(dockHashId, "posY"), s_dockState.targetPos.y,
      dockSpring);

  float currentPanelAlpha =
      s_dockState.isOpened ? std::clamp((progress - 0.04f) * 6.0f, 0.0f, 1.0f)
                           : std::clamp((progress - 0.70f) * 4.0f, 0.0f, 1.0f);

  // --------------------------------------------------------------------------
  // RENDER STEP 1: Floating DevTools Panel
  // --------------------------------------------------------------------------
  if (currentPanelAlpha > 0.001f) {
    float targetW = std::clamp(screenW * 0.82f, 780.0f, 1200.0f);
    float targetH = std::clamp(screenH * 0.72f, 520.0f, 800.0f);
    float clampedProgress = std::clamp(progress, 0.0f, 1.08f);

    float panelW = (1.0f - clampedProgress) * dockW + clampedProgress * targetW;
    float panelH = (1.0f - clampedProgress) * dockH + clampedProgress * targetH;

    glm::vec2 pillCenter =
        s_dockState.currentPos + glm::vec2(dockW * 0.5f, dockH * 0.5f);

    float desiredPanelX = 0.0f, desiredPanelY = 0.0f;
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

    glm::vec2 animatedTargetCenter(animatedTargetX + targetW * 0.5f,
                                   animatedTargetY + targetH * 0.5f);
    glm::vec2 currentCenter = (1.0f - clampedProgress) * pillCenter +
                              clampedProgress * animatedTargetCenter;

    float currentPanelX = currentCenter.x - panelW * 0.5f;
    float currentPanelY = currentCenter.y - panelH * 0.5f;

    Div(Modifier()
            .id("AtomicDevToolsPanel")
            .fixed()
            .left(currentPanelX)
            .top(currentPanelY)
            .size(panelW, panelH)
            .background("#ffffff"_hex)
            .subtleShadow(1)
            .border(Colors::gray[200], 1)
            .rounded(14.0f)
            .color(Colors::black[900])
            .opacity(currentPanelAlpha)
            .column()
            .gap(0),
        [&]() {
          // ------------------------------------------------------------------
          // Header Bar
          // ------------------------------------------------------------------
          Row(Modifier()
                  .padding(16, 10)
                  .background("#fafafa"_hex)
                  .border(Colors::gray[200], {0.0f, 0.0f, 1.0f, 0.0f})
                  .widthGrow()
                  .center(),
              [&]() {
                Row(Modifier().gap(8).center(), [&]() {
                  Icon(LucideIcon::Terminal, Modifier().fontSize(14));
                  Text("Atomic DevTools",
                       Modifier().fontSize(14).fontWeight(600));
                });

                Div(Modifier().widthGrow());

                if (Button(
                        Modifier()
                            .id("closeDevToolsBtn")
                            .padding(6, 6)
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
          // Tier 1 Main Tab Bar (Profiling | Styling)
          // ------------------------------------------------------------------
          Row(Modifier()
                  .widthGrow()
                  .padding(16, 8)
                  .background("#f4f4f5"_hex)
                  .border(Colors::gray[200], {0.0f, 0.0f, 1.0f, 0.0f})
                  .gap(8),
              [&]() {
                bool isProfiling =
                    (s_dockState.activeMainTab == DevToolsMainTab::Profiling);
                bool isStyling =
                    (s_dockState.activeMainTab == DevToolsMainTab::Styling);

                glm::vec4 profBg =
                    isProfiling ? "#ffffff"_hex : Colors::transparent;
                glm::vec4 profText =
                    isProfiling ? Colors::black[900] : Colors::gray[500];

                if (Button(Modifier()
                               .id("mainTabProfiling")
                               .background(profBg)
                               .color(profText)
                               .padding(12, 6)
                               .rounded(6.0f),
                           [&]() {
                             Row(Modifier().gap(6).center(), [&]() {
                               Icon(LucideIcon::Activity,
                                    Modifier().fontSize(13));
                               Text("Profiling",
                                    Modifier().fontSize(13).fontWeight(600));
                             });
                           })
                        .clicked) {
                  s_dockState.activeMainTab = DevToolsMainTab::Profiling;
                }

                glm::vec4 styleBg =
                    isStyling ? "#ffffff"_hex : Colors::transparent;
                glm::vec4 styleText =
                    isStyling ? Colors::black[900] : Colors::gray[500];

                if (Button(Modifier()
                               .id("mainTabStyling")
                               .background(styleBg)
                               .color(styleText)
                               .padding(12, 6)
                               .rounded(6.0f),
                           [&]() {
                             Row(Modifier().gap(6).center(), [&]() {
                               Icon(LucideIcon::Palette,
                                    Modifier().fontSize(13));
                               Text("Styling & Themes",
                                    Modifier().fontSize(13).fontWeight(600));
                             });
                           })
                        .clicked) {
                  s_dockState.activeMainTab = DevToolsMainTab::Styling;
                }
              });

          // ------------------------------------------------------------------
          // Main Tab Body Content
          // ------------------------------------------------------------------
          Div(Modifier()
                  .background("#ffffff"_hex)
                  .grow()
                  .padding(16)
                  .column()
                  .gap(12),
              [&]() {
                if (s_dockState.activeMainTab == DevToolsMainTab::Profiling) {
                  // --------------------------------------------------------------
                  // TAB 1: PROFILING METRICS CARDS
                  // --------------------------------------------------------------
                  char frameTimeBuf[32];
                  std::snprintf(frameTimeBuf, sizeof(frameTimeBuf), "%.2f ms",
                                uiState->frameTimeMs);

                  std::string fpsStr = std::to_string(
                      static_cast<int>(std::round(uiState->fps)));
                  std::string frameTimeStr = frameTimeBuf;
                  std::string drawCallsStr = std::to_string(uiState->drawCalls);

                  ScrollView(Modifier().grow().widthGrow(), [&]() {
                    Row(Modifier().widthGrow().gap(12), [&]() {
                      struct CardSpec {
                        LucideIcon icon;
                        std::string label;
                        std::string value;
                        std::string badge;
                        std::string badgeLabel;
                      };

                      std::array<CardSpec, 3> cards = {
                          CardSpec{LucideIcon::Activity, "Performance FPS",
                                   fpsStr, frameTimeStr, "Frame Time"},
                          CardSpec{LucideIcon::Layers, "Draw Calls",
                                   drawCallsStr, "+12%", "vs last pass"},
                          CardSpec{LucideIcon::Cpu, "Memory Alloc", "18.4 MB",
                                   "Normal", "Peak 24.2 MB"}};

                      for (int i = 0; i < 3; ++i) {
                        const auto &c = cards[i];

                        SpringConfig cardSpring{
                            .mass = 1.0f,
                            .stiffness = 240.0f - static_cast<float>(i) * 35.0f,
                            .damping = 18.0f,
                            .initialVelocity = 0.0f};
                        float cardProgress =
                            uiState->motionManager.animateSpring<float>(
                                motion::MotionHandle(
                                    dockHashId, "cardP_" + std::to_string(i)),
                                targetProgress, cardSpring);

                        float cardAlpha =
                            std::clamp(cardProgress * 2.5f, 0.0f, 1.0f);
                        float cardOffsetY =
                            (1.0f - std::clamp(cardProgress, 0.0f, 1.0f)) *
                            18.0f;

                        Div(Modifier()
                                .grow()
                                .offset(0.0f, cardOffsetY)
                                .opacity(cardAlpha)
                                .background("#ffffff"_hex)
                                .border(Colors::gray[200], 1)
                                .rounded(10.0f)
                                .padding(14)
                                .column()
                                .gap(10),
                            [&]() {
                              Row(Modifier().widthGrow().center().gap(8),
                                  [&]() {
                                    Div(Modifier()
                                            .size(28, 28)
                                            .background("#f4f4f5"_hex)
                                            .border(Colors::gray[200], 1)
                                            .rounded(6.0f)
                                            .center(),
                                        [&]() {
                                          Icon(c.icon,
                                               Modifier().fontSize(13).color(
                                                   Colors::black[900]));
                                        });
                                    Text(c.label,
                                         Modifier()
                                             .fontSize(12)
                                             .fontWeight(600)
                                             .textColor(Colors::black[800]));
                                  });

                              Div(Modifier().widthGrow().height(1).background(
                                  Colors::gray[100]));
                              Text(c.value, Modifier()
                                                .fontSize(24)
                                                .fontWeight(700)
                                                .textColor(Colors::black[950]));

                              Row(Modifier().center().gap(6), [&]() {
                                Text(c.badge, Modifier()
                                                  .fontSize(11)
                                                  .fontWeight(600)
                                                  .textColor("#10b981"_hex));
                                Text(c.badgeLabel,
                                     Modifier()
                                         .fontSize(11)
                                         .fontWeight(400)
                                         .textColor(Colors::gray[400]));
                              });
                            });
                      }
                    });
                  });
                } else {
                  // --------------------------------------------------------------
                  // TAB 2: STYLING & THEME MANAGER INSPECTOR
                  // --------------------------------------------------------------
                  auto &tm = ThemeManager::getInstance();
                  const auto &allThemes = tm.getAllThemes();

                  // Tier 2 Sub-Tabs (Theme Selectors: :root, dark, etc.) + Set
                  // Active Theme Control
                  Row(Modifier()
                          .widthGrow()
                          .padding(8, 6)
                          .background("#f4f4f5"_hex)
                          .rounded(8.0f)
                          .center()
                          .gap(6),
                      [&]() {
                        for (const auto &[themeName, theme] : allThemes) {
                          bool isSubActive =
                              (s_dockState.activeStyleSubTab == themeName);
                          glm::vec4 subBg =
                              isSubActive ? "#ffffff"_hex : Colors::transparent;
                          glm::vec4 subText = isSubActive ? Colors::black[900]
                                                          : Colors::gray[500];

                          if (Button(Modifier()
                                         .id("subTab_" + themeName)
                                         .background(subBg)
                                         .color(subText)
                                         .padding(10, 4)
                                         .rounded(6.0f),
                                     [&]() {
                                       Text(themeName,
                                            Modifier().fontSize(12).fontWeight(
                                                600));
                                     })
                                  .clicked) {
                            s_dockState.activeStyleSubTab = themeName;
                          }
                        }

                        Div(Modifier().widthGrow());

                        // Active Theme Status / Switcher Button
                        bool isInspectedThemeActive =
                            (tm.getActiveThemeName() ==
                             s_dockState.activeStyleSubTab);
                        if (isInspectedThemeActive) {
                          Row(Modifier()
                                  .padding(8, 4)
                                  .background("#10b981"_hex)
                                  .rounded(4.0f)
                                  .center()
                                  .gap(4),
                              [&]() {
                                Icon(LucideIcon::Check,
                                     Modifier().fontSize(11).color(
                                         Colors::white));
                                Text("Active Theme", Modifier()
                                                         .fontSize(11)
                                                         .fontWeight(600)
                                                         .color(Colors::white));
                              });
                        } else {
                          if (Button(Modifier()
                                         .id("setActiveThemeBtn")
                                         .background("#3b82f6"_hex)
                                         .color(Colors::white)
                                         .padding(10, 4)
                                         .rounded(4.0f),
                                     [&]() {
                                       Row(Modifier().gap(4).center(), [&]() {
                                         Icon(LucideIcon::Play,
                                              Modifier().fontSize(11).color(
                                                  Colors::white));
                                         Text("Set as Active Theme",
                                              Modifier()
                                                  .fontSize(11)
                                                  .fontWeight(600)
                                                  .color(Colors::white));
                                       });
                                     })
                                  .clicked) {
                            tm.switchTheme(s_dockState.activeStyleSubTab);
                          }
                        }
                      });

                  // Scrollable Variable Inspection List (O(1) Vector Indexing)
                  ScrollView(Modifier().grow().widthGrow(), [&]() {
                    Div(Modifier().widthGrow().column().gap(8), [&]() {
                      auto themeIt =
                          allThemes.find(s_dockState.activeStyleSubTab);
                      if (themeIt != allThemes.end()) {
                        for (uint32_t varId = 0;
                             varId < themeIt->second.variables.size();
                             ++varId) {
                          const auto &var = themeIt->second.variables[varId];
                          if (!var.hasValue)
                            continue;

                          std::string varName = var.name.empty()
                                                    ? tm.getVariableName(varId)
                                                    : var.name;
                          std::string bufKey = s_dockState.activeStyleSubTab +
                                               "::" + std::to_string(varId);

                          if (!s_dockState.propertyBuffers.contains(bufKey)) {
                            s_dockState.propertyBuffers[bufKey] =
                                formatVarValue(var);
                          }

                          std::string &currentBuf =
                              s_dockState.propertyBuffers[bufKey];
                          std::string previousBuf = currentBuf;

                          // Variable Property Row
                          Row(Modifier()
                                  .widthGrow()
                                  .padding(10, 8)
                                  .background("#fafafa"_hex)
                                  .border(Colors::gray[200], 1)
                                  .rounded(6.0f)
                                  .center(),
                              [&]() {
                                // Left: Property Token Name
                                Text(varName,
                                     Modifier()
                                         .fontSize(12)
                                         .fontWeight(600)
                                         .textColor(Colors::black[800]));

                                Div(Modifier().widthGrow());

                                // Right: Type Control / Input Editor
                                if (var.type == ThemeVariableType::Boolean) {
                                  bool boolVal = std::get<bool>(var.value);
                                  std::string toggleLabel =
                                      boolVal ? "true" : "false";
                                  glm::vec4 toggleBg = boolVal
                                                           ? "#10b981"_hex
                                                           : Colors::gray[300];

                                  if (Button(Modifier()
                                                 .id("toggle_" + bufKey)
                                                 .background(toggleBg)
                                                 .color(Colors::white)
                                                 .padding(10, 4)
                                                 .rounded(4.0f),
                                             [&]() {
                                               Text(toggleLabel,
                                                    Modifier()
                                                        .fontSize(11)
                                                        .fontWeight(600));
                                             })
                                          .clicked) {
                                    bool newBool = !boolVal;
                                    tm.setThemeVariableValue(
                                        s_dockState.activeStyleSubTab, varId,
                                        newBool);
                                    currentBuf = newBool ? "true" : "false";
                                  }
                                } else {
                                  // Color / Float / Integer Input Box
                                  TextInput(Modifier()
                                                .id("input_" + bufKey)
                                                .width(160.0f)
                                                .fontSize(12.0f),
                                            currentBuf, "Value...");

                                  // INSTANT APPLY: On every character
                                  // typed/edited in DevTools
                                  if (currentBuf != previousBuf) {
                                    tm.setThemeVariableFromString(
                                        s_dockState.activeStyleSubTab, varId,
                                        var.type, currentBuf);
                                  }

                                  // Small Color Preview Indicator for Color
                                  // Variables
                                  if (var.type == ThemeVariableType::Color) {
                                    auto col = std::get<glm::vec4>(var.value);
                                    Div(Modifier()
                                            .size(18, 18)
                                            .background(col)
                                            .border(Colors::gray[300], 1)
                                            .rounded(4.0f));
                                  }
                                }
                              });
                        }
                      }
                    });
                  });

                  // --------------------------------------------------------------
                  // Bottom Footer Bar: Disk Actions (Save to File | Reload Disk
                  // CSS)
                  // --------------------------------------------------------------
                  Row(Modifier()
                          .widthGrow()
                          .padding(12, 8)
                          .background("#fafafa"_hex)
                          .border(Colors::gray[200], {1.0f, 0.0f, 0.0f, 0.0f})
                          .center()
                          .gap(10),
                      [&]() {
                        TextInput(Modifier()
                                      .id("cssPathInput")
                                      .grow()
                                      .fontSize(12.0f),
                                  s_dockState.cssFilePath, "CSS Filepath...");

                        // 1. Save Theme to File Button
                        if (Button(Modifier()
                                       .id("saveCssBtn")
                                       .background("#10b981"_hex)
                                       .color(Colors::white)
                                       .padding(12, 8)
                                       .rounded(6.0f),
                                   [&]() {
                                     Row(Modifier().gap(6).center(), [&]() {
                                       Icon(LucideIcon::Save,
                                            Modifier().fontSize(12).color(
                                                Colors::white));
                                       Text("Save to Disk",
                                            Modifier()
                                                .fontSize(12)
                                                .fontWeight(600)
                                                .color(Colors::white));
                                     });
                                   })
                                .clicked) {
                          tm.writeThemeToFile(s_dockState.activeStyleSubTab,
                                              s_dockState.cssFilePath);
                        }

                        // 2. Reload Disk CSS Button
                        if (Button(Modifier()
                                       .id("reloadDiskCssBtn")
                                       .background("#6b7280"_hex)
                                       .color(Colors::white)
                                       .padding(12, 8)
                                       .rounded(6.0f),
                                   [&]() {
                                     Row(Modifier().gap(6).center(), [&]() {
                                       Icon(LucideIcon::RotateCcw,
                                            Modifier().fontSize(12).color(
                                                Colors::white));
                                       Text("Reload Disk CSS",
                                            Modifier()
                                                .fontSize(12)
                                                .fontWeight(600)
                                                .color(Colors::white));
                                     });
                                   })
                                .clicked) {
                          tm.loadThemesFromCss(s_dockState.cssFilePath);
                          s_dockState.propertyBuffers.clear();
                        }
                      });
                }
              });
        });
  }

  // --------------------------------------------------------------------------
  // RENDER STEP 2: Dock Pill Button
  // --------------------------------------------------------------------------
  Interaction dockBtn = Div(
      Modifier()
          .id("AtomicDevToolsDockBtn")
          .fixed()
          .left(s_dockState.currentPos.x + bumpOffset.x)
          .top(s_dockState.currentPos.y + bumpOffset.y)
          .scale(btnScale)
          .background(Colors::black[950])
          .rounded(19.0f)
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

  if (dockBtn.pressed && !s_dockState.isMouseDown) {
    s_dockState.isMouseDown = true;
    s_dockState.hasDragged = false;
    s_dockState.dragStartMousePos = mousePos;
    s_dockState.dragOffset = mousePos - s_dockState.currentPos;
  }

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

  if (dockBtn.clicked) {
    if (!s_dockState.hasDragged) {
      s_dockState.isOpened = !s_dockState.isOpened;
    }
    s_dockState.hasDragged = false;
  }
}

} // namespace atomic
