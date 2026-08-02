#pragma once

#include "ui/components.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/motion/Curves.h"
#include "ui/motion/MotionTypes.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace atomic::docs {

/**
 * @brief Showcase 1: Equalizer Wave
 */
inline void drawEqualizerWaveShowcase() {
  using namespace atomic;
  using namespace atomicComponents;
  using atomic::motion::MotionHandle;

  Column(Modifier().gap(24).widthGrow(), []() {
    Text("Equalizer Wave", Modifier().fontSize(18).fontWeight(700));
    Text("Spring wave animation test.", Modifier().fontSize(13));

    Div(Modifier()
            .widthGrow()
            .padding(24)
            .border(Colors::gray[200], 1.0f)
            .rounded(12.0f)
            .column()
            .gap(24),
        [&]() {
          auto *uiState = getUiState();
          uint32_t baseId = hashLabel("EqualizerShowcase");

          static bool waveTriggered = false;
          if (Button(Modifier().id("TriggerWaveBtn"), [&]() {
                Text(waveTriggered ? "Reset" : "Play", Modifier().fontSize(12));
              }).clicked) {
            waveTriggered = !waveTriggered;
          }

          Row(Modifier().gap(12).margin(10, 0).widthGrow().center(), [&]() {
            constexpr int numBars = 14;
            for (int i = 0; i < numBars; ++i) {
              uint32_t barId = baseId + 0x1000 + i;

              float phase = static_cast<float>(i) * 0.40f;
              float targetH = waveTriggered
                                  ? (45.0f + std::sin(phase) * 50.0f + 35.0f)
                                  : 14.0f;

              motion::SpringConfig waveSpring{.mass = 1.0f + i * 0.08f,
                                              .stiffness = 150.0f,
                                              .damping = 12.0f};

              float barHeight = uiState->motionManager.animateSpring<float>(
                  MotionHandle{barId}, targetH, waveSpring);

              glm::vec4 barColor = glm::mix(
                  glm::vec4(0.31f, 0.27f, 0.90f, 1.0f),
                  glm::vec4(0.14f, 0.58f, 0.45f, 1.0f),
                  static_cast<float>(i) / static_cast<float>(numBars - 1));

              Div(Modifier()
                      .size(14, barHeight)
                      .background(barColor)
                      .rounded(6.0f));
            }
          });
        });
  });
}

/**
 * @brief Showcase 2: Card Morph Matrix
 */
inline void drawCardMorphShowcase() {
  using namespace atomic;
  using namespace atomicComponents;
  using atomic::motion::MotionHandle;

  Column(Modifier().gap(24).widthGrow(), []() {
    Text("Card Morph", Modifier().fontSize(18).fontWeight(700));
    Text("Interactive card hover and press states.", Modifier().fontSize(13));

    Div(Modifier()
            .widthGrow()
            .padding(24)
            .border(Colors::gray[200], 1.0f)
            .rounded(12.0f)
            .column()
            .gap(24),
        [&]() {
          auto *uiState = getUiState();

          Row(Modifier().gap(16).widthGrow(), [&]() {
            constexpr int cardCount = 3;
            const char *cardTitles[cardCount] = {"Card 1", "Card 2", "Card 3"};
            const char *cardSubtitles[cardCount] = {"Subtitle 1", "Subtitle 2",
                                                    "Subtitle 3"};
            glm::vec4 cardAccents[cardCount] = {"#4f46e5"_hex, "#db2777"_hex,
                                                "#059669"_hex};

            for (int i = 0; i < cardCount; ++i) {
              std::string label = "CyberCard_" + std::to_string(i);
              uint32_t cid = hashLabel(label);

              bool hovered = isHovered(cid);
              bool pressed = isPressed(cid);

              float targetScale = pressed ? 0.97f : (hovered ? 1.03f : 1.0f);
              glm::vec4 targetBorder =
                  hovered ? cardAccents[i] : Colors::gray[200];
              glm::vec4 targetBg = Colors::white;

              float scale = uiState->motionManager.animate<float>(
                  MotionHandle{cid + 0x100}, targetScale, 0.2f,
                  motion::AnimationCurve::EaseOut());

              glm::vec4 border = uiState->motionManager.animate<glm::vec4>(
                  MotionHandle{cid + 0x300}, targetBorder, 0.2f,
                  motion::AnimationCurve::EaseOut());

              glm::vec4 bg = uiState->motionManager.animate<glm::vec4>(
                  MotionHandle{cid + 0x400}, targetBg, 0.2f,
                  motion::AnimationCurve::EaseOut());

              Div(Modifier()
                      .id(label)
                      .widthGrow()
                      .scale(scale)
                      .background(bg)
                      .border(border, 1.2f)
                      .rounded(12.0f)
                      .padding(16)
                      .column()
                      .gap(8),
                  [&]() {
                    Row(Modifier().widthGrow().alignY(AlignmentY::Center),
                        [&]() {
                          Div(Modifier().size(8, 8).rounded(4.0f).background(
                              cardAccents[i]));
                          Div(Modifier().widthGrow());
                          Icon(LucideIcon::IdCard,
                               Modifier().size(14, 14).color(
                                   hovered ? cardAccents[i]
                                           : Colors::gray[400]));
                        });

                    Text(cardTitles[i],
                         Modifier().fontSize(13).fontWeight(600));
                    Text(cardSubtitles[i], Modifier().fontSize(11));
                  });
            }
          });
        });
  });
}

/**
 * @brief Showcase 3: Staggered Launchpad Square Burst with Rainbow Background
 * Stagger
 */
inline void drawLaunchpadBurstShowcase() {
  using namespace atomic;
  using namespace atomicComponents;
  using atomic::motion::MotionHandle;

  Column(Modifier().gap(24).widthGrow(), []() {
    Text("Launchpad Burst", Modifier().fontSize(18).fontWeight(700));
    Text("Staggered square scale and rainbow background cascade.",
         Modifier().fontSize(13));

    Div(Modifier()
            .widthGrow()
            .padding(24)
            .border(Colors::gray[200], 1.0f)
            .rounded(12.0f)
            .column()
            .gap(24),
        [&]() {
          auto *uiState = getUiState();

          static std::vector<float> nodeScales = {1.0f, 1.0f, 1.0f,
                                                  1.0f, 1.0f, 1.0f};
          static std::vector<float> nodeColorWeights = {0.0f, 0.0f, 0.0f,
                                                        0.0f, 0.0f, 0.0f};

          if (Button(Modifier().id("LaunchpadBtn"), [&]() {
                Text("Trigger Burst", Modifier().fontSize(12));
              }).clicked) {

            for (size_t i = 0; i < nodeScales.size(); ++i) {
              nodeScales[i] = 0.1f;
              nodeColorWeights[i] = 0.0f;
            }

            using namespace atomic::motion;
            Timeline &tl = uiState->motionManager.createTimeline();

            std::vector<PropertyRef<float>> scaleTargets;
            for (float &s : nodeScales) {
              scaleTargets.push_back(makeProperty(s));
            }

            std::vector<PropertyRef<float>> colorTargets;
            for (float &w : nodeColorWeights) {
              colorTargets.push_back(makeProperty(w));
            }

            tl.staggerTo<float>(scaleTargets, 1.0f, 0.35f, 0.04f,
                                motion::AnimationCurve::BackOut());
            tl.staggerTo<float>(colorTargets, 1.0f, 0.35f, 0.04f,
                                motion::AnimationCurve::EaseOut());

            tl.play();
          }

          Row(Modifier().gap(12).margin(10, 0).widthGrow().center(), [&]() {
            glm::vec4 rainbowPalette[6] = {
                "#ef4444"_hex, // Red
                "#f97316"_hex, // Orange
                "#eab308"_hex, // Yellow
                "#10b981"_hex, // Green
                "#3b82f6"_hex, // Blue
                "#8b5cf6"_hex  // Purple
            };

            for (size_t i = 0; i < nodeScales.size(); ++i) {
              glm::vec4 boxBg =
                  glm::mix(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), rainbowPalette[i],
                           nodeColorWeights[i]);

              Div(Modifier()
                      .size(68, 68)
                      .scale(nodeScales[i])
                      .background(boxBg)
                      .border(Colors::gray[200], 1.2f)
                      .rounded(12.0f)
                      .center(),
                  [&]() {
                    Text(std::to_string(i + 1),
                         Modifier().fontSize(13).fontWeight(700));
                  });
            }
          });
        });
  });
}
/**
 * @brief Renders a retro 7-segment amber digital LCD display panel.
 */
inline void drawDigitalLcdPanel(const std::string &headerLabel,
                                const std::string &digitReadout) {
  using namespace atomic;

  // Exact Amber/Espresso color palette
  const glm::vec4 labelColor = "#d87236"_hex;  // Muted warm amber
  const glm::vec4 digitColor = "#ff832b"_hex;  // Bright amber 7-segment neon
  const glm::vec4 panelBg = "#46271a"_hex;     // Dark cocoa LCD bevel
  const glm::vec4 panelBorder = "#5a3121"_hex; // Subtle panel edge stroke

  Column(Modifier().gap(12), [&]() {
    // Top Muted Header Label
    Text(headerLabel,
         Modifier().fontSize(16).fontWeight(600).textColor(labelColor));

    // Digital LCD Display Bevel Box
    Div(Modifier()
            .width(260)
            .height(85)
            .background(panelBg)
            .border(panelBorder, 1.0f)
            .rounded(12.0f)
            .padding(16, 24)
            .row()
            .alignY(AlignmentY::Center)
            .alignX(AlignmentX::Right),
        [&]() {
          Text(digitReadout,
               Modifier().fontSize(34).fontWeight(700).textColor(digitColor));
        });
  });
}

/**
 * @brief Complete interactive Retro LCD Timer Showcase with live callback
 * fires.
 */
inline void drawRetroDigitalTimerShowcase() {
  using namespace atomic;
  using namespace atomicComponents;
  using atomic::motion::MotionHandle;

  const glm::vec4 canvasBg = "#382015"_hex;

  Div(Modifier()
          .widthGrow()
          .padding(40, 48)
          .background(canvasBg)
          .rounded(16.0f)
          .column()
          .gap(24)
          .center(),
      [&]() {
        auto *uiState = getUiState();
        static MotionHandle timerHandle = MotionHandle::Invalid();

        // Interactive Start / Pause / Reset Buttons
        Row(Modifier().widthGrow().center(), [&]() {
          if (Button(Modifier().id("ToggleLcdBtn").background("#5a3121"_hex),
                     [&]() {
                       Text(timerHandle.isValid() ? "Pause / Resume Timer"
                                                  : "Start Digital Timer",
                            Modifier().fontSize(12).fontWeight(600).color(
                                "#ff832b"_hex));
                     })
                  .clicked) {

            if (!timerHandle.isValid()) {
              // Anime.js-style timer creation: 10s duration per loop, infinite
              // iterations
              timerHandle = uiState->motionManager.createTimer(
                  {.duration = 10.0f,
                   .loopMode = motion::LoopMode::Infinite,
                   .timeScale = 1.0f});
            } else {
              auto *timer = uiState->motionManager.getTimer(timerHandle);
              if (timer) {
                if (timer->getPlayState() == motion::PlayState::Running) {
                  timer->pause();
                } else {
                  timer->play();
                }
              }
            }
          }

          if (Button(Modifier().id("ResetLcdBtn").background("#46271a"_hex),
                     [&]() {
                       Text("Reset Readouts",
                            Modifier().fontSize(12).fontWeight(600).color(
                                "#d87236"_hex));
                     })
                  .clicked) {
            uiState->motionManager.clearTimers();
            timerHandle = MotionHandle::Invalid();
          }
        });

        // 1. Fetch live timer values directly from Atomic.Motion
        auto *timer = uiState->motionManager.getTimer(timerHandle);

        // 2. Format displays
        float totalSec = timer ? timer->getTotalElapsedTime() : 0.0f;
        uint32_t loops = timer ? timer->getIterationCount() : 0;

        int currentMs = static_cast<int>(std::round(totalSec * 100.0f));
        std::string timeStr = std::to_string(currentMs);
        std::string callbackStr = std::to_string(loops);

        // Render side-by-side LCD panels
        Row(Modifier().gap(24).center(), [&]() {
          drawDigitalLcdPanel("current time", timeStr);
          drawDigitalLcdPanel("callback fired", callbackStr);
        });
      });
}

/**
 * @brief Master showcase wrapper rendering all 3 split containers.
 */
inline void drawHeroAnimationShowcase() {
  using namespace atomic;
  using namespace atomicComponents;

  Column(Modifier().gap(40).widthGrow(), []() {
    Text("Atomic Motion Showcases", Modifier().fontSize(24).fontWeight(700));
    Text("Individual interactive animation modules.", Modifier().fontSize(14));

    drawEqualizerWaveShowcase();
    drawCardMorphShowcase();
    drawLaunchpadBurstShowcase();
    drawRetroDigitalTimerShowcase();
  });
}

} // namespace atomic::docs
