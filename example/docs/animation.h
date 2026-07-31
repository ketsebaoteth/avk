#pragma once

#include "avk/utils/ui/layout.h"
#include "showcase.h"
#include "ui/components.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/style/modifier.h"
#include "ui/style/style.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace atomic::docs {

/**
 * @brief Renders the master interactive documentation suite for the
 * Atomic.Motion engine.
 */
inline void drawAnimationDoc(
    std::function<void(const std::string &, const std::string &)> drawHeader) {
  using namespace atomic;
  using namespace atomicComponents;
  using namespace atomic::extras;
  using atomic::motion::MotionHandle;

  Column(DefaultModifier().gap(36).widthGrow(), [&]() {
    drawHeader(
        "Atomic.Motion Subsystem",
        "A zero-allocation animation engine built for immediate-mode C++ "
        "UI architectures. Decouples layout states from execution loops to "
        "provide declarative properties for single tweens, analytical "
        "mass-spring "
        "dynamics, synchronized multi-track timelines, staggered container "
        "layouts, "
        "and standard Robert Penner easing equations.");

    auto *uiState = getUiState();
    uint32_t headerCardId = hashLabel("HeaderBannerCard");

    static float translationX = 0.0f;
    static bool tweenInitialized = false;

    if (!tweenInitialized && uiState) {
      tweenInitialized = true;

      using namespace atomic::motion;

      auto prop = makeProperty(translationX);

      // Create pre-configured ping-pong tween
      Tween<float> loopTween(uiState->motionManager.createHandle(), prop, 0.0f,
                             200.0f, 1.0f);
      loopTween.setCurve(AnimationCurve::EaseInOut());
      loopTween.setLoopMode(LoopMode::PingPong);

      // Register the configured tween directly!
      uiState->motionManager.registerTween(std::move(loopTween));
    }

    // Exact Modifier API matching modifier.h
    Div(Modifier()
            .id("HeaderBannerCard")
            .width(280)
            .height(80)
            .background(glm::vec4(0.12f, 0.14f, 0.18f, 1.0f))
            .rounded(12.0f)
            .border(Colors::gray[200], 1.0f)
            .alignX(AlignmentX::Center)
            .alignY(AlignmentY::Center)
            .translate(translationX, 0.0f),
        [&]() {
          Text("Ping-Pong",
               DefaultModifier().textColor("#ffffff"_hex).fontSize(18));
        });

    Column(DefaultModifier().gap(14).widthGrow(), []() {
      Text("1. Declarative Immediate-Mode Tweens (Sub-Frame Interpolation)",
           DefaultModifier().fontSize(20).fontWeight(600).textColor(
               Colors::black[900]));

      Text(
          "In immediate-mode rendering, component functions supply target "
          "values to animate<T>() on every frame loop. "
          "If a target shifts mid-animation (e.g., during high-frequency "
          "pointer input), the engine intercepts the trajectory, captures "
          "the current instantaneous velocity, and recalculates the path "
          "from the current runtime state to prevent position discontinuities.",
          DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(32, 250)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .center(),
          [&]() {
            auto *uiState = getUiState();
            uint32_t cardId = hashLabel("DeclarativeMorphCard");
            bool hovered = isHovered(cardId);
            bool pressed = isPressed(cardId);

            float targetScale = pressed ? 0.92f : (hovered ? 1.08f : 1.0f);
            glm::vec4 targetBg =
                pressed ? "#0f172a"_hex
                        : (hovered ? "#2563eb"_hex : "#1e293b"_hex);
            glm::vec4 targetBorder = hovered ? "#60a5fa"_hex : "#334155"_hex;

            float scale = uiState->motionManager.animate<float>(
                MotionHandle(cardId, "scale"), targetScale, 0.22f,
                motion::AnimationCurve::BackOut());

            glm::vec4 bg = uiState->motionManager.animate<glm::vec4>(
                MotionHandle(cardId, "background"), targetBg, 0.22f,
                motion::AnimationCurve::EaseOut());

            glm::vec4 border = uiState->motionManager.animate<glm::vec4>(
                MotionHandle(cardId, "border"), targetBorder, 0.22f,
                motion::AnimationCurve::EaseOut());

            Div(DefaultModifier()
                    .id("DeclarativeMorphCard")
                    .padding(30)
                    .scale(scale)
                    .background(bg)
                    .border(border, 2.0f)
                    .rounded(12.0f)
                    .center(),
                [&]() {
                  Column(DefaultModifier().gap(4).center(), [&]() {
                    Text(pressed ? "State: Pressed"
                                 : (hovered ? "State: Hovered" : "State: Idle"),
                         DefaultModifier().fontSize(14).fontWeight(600).color(
                             Colors::white));
                    Text("Hover & click to test rapid re-baselining",
                         DefaultModifier().fontSize(11).color("#94a3b8"_hex));
                  });
                });
          });

      CodeBlock(
          "auto* uiState = getUiState();\n"
          "uint32_t cardId = hashLabel(\"DeclarativeMorphCard\");\n"
          "bool hovered = isHovered(cardId);\n"
          "bool pressed = isPressed(cardId);\n\n"
          "// 1. Resolve declarative target state\n"
          "float targetScale = pressed ? 0.92f : (hovered ? 1.08f : 1.0f);\n"
          "glm::vec4 targetBg = hovered ? \"#2563eb\"_hex : \"#1e293b\"_hex;\n"
          "glm::vec4 targetBorder = hovered ? \"#60a5fa\"_hex : "
          "\"#334155\"_hex;\n\n"
          "// 2. Named composite handles (Zero magic hex offset additions!)\n"
          "float scale = uiState->motionManager.animate<float>(\n"
          "    MotionHandle(cardId, \"scale\"), targetScale, 0.22f, "
          "motion::AnimationCurve::EaseOut());\n\n"
          "glm::vec4 bg = uiState->motionManager.animate<glm::vec4>(\n"
          "    MotionHandle(cardId, \"bg\"), targetBg, 0.22f, "
          "motion::AnimationCurve::EaseOut());\n\n"
          "glm::vec4 border = uiState->motionManager.animate<glm::vec4>(\n"
          "    MotionHandle(cardId, \"border\"), targetBorder, 0.22f, "
          "motion::AnimationCurve::EaseOut());\n\n"
          "Div(DefaultModifier().id(\"DeclarativeMorphCard\").scale(scale)."
          "background(bg));",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 2. Analytical Mass-Spring-Damper Dynamics
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(14).widthGrow().padding(24, 0), [&]() {
      Text("2. Mass-Spring-Damper Physics (Closed-Form Analytical Evaluation)",
           DefaultModifier().fontSize(20).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Springs in Atomic.Motion evaluate displacement analytically "
           "against elapsed seconds t: "
           "y(t) = 1 - e^(-alpha * t) * (cos(omega_d * t) + c2 * sin(omega_d * "
           "t)). "
           "Because it is an exact closed-form solution to the mass-spring "
           "differential equation m*x'' + c*x' + k*x = 0, "
           "it evaluates with 100% precision regardless of frame rate "
           "fluctuations.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(40, 32)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(32)
              .center(),
          [&]() {
            auto *uiState = getUiState();
            uint32_t springId = hashLabel("SpringDemoSuite");

            static bool springActive = false;
            if (Button(
                    DefaultModifier()
                        .id("ToggleSpringBtn")
                        .background("#2563eb"_hex),
                    [&]() {
                      Text(springActive ? "Reset Springs"
                                        : "Trigger Horizontal Showcase",
                           DefaultModifier().fontSize(12).fontWeight(600).color(
                               Colors::white));
                    })
                    .clicked) {
              springActive = !springActive;
            }

            Column(DefaultModifier().gap(28).margin(24, 0), [&]() {
              // 1. Default Spring
              float defVal = uiState->motionManager.animateSpring<float>(
                  MotionHandle{springId + 1}, springActive ? 85.0f : 0.0f,
                  motion::SpringConfig::Default());

              Div(DefaultModifier().row().gap(46).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Default",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("k=120, c=14",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(defVal, 0.0f)
                            .background("#0284c7"_hex)
                            .rounded(10.0f));
                  });

              // 2. Bouncy Spring
              float bounceVal = uiState->motionManager.animateSpring<float>(
                  MotionHandle{springId + 2}, springActive ? 85.0f : 0.0f,
                  motion::SpringConfig::Bouncy());

              Div(DefaultModifier().row().gap(46).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Bouncy",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("k=180, c=8",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(bounceVal, 0.0f)
                            .background("#dc2626"_hex)
                            .rounded(10.0f));
                  });

              // 3. Snappy Spring
              float snappyVal = uiState->motionManager.animateSpring<float>(
                  MotionHandle{springId + 3}, springActive ? 85.0f : 0.0f,
                  motion::SpringConfig::Snappy());

              Div(DefaultModifier().row().gap(46).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Snappy",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("k=300, c=22",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(snappyVal, 0.0f)
                            .background("#059669"_hex)
                            .rounded(10.0f));
                  });

              // 4. Heavy Spring
              motion::SpringConfig heavyConfig{
                  .mass = 3.0f, .stiffness = 80.0f, .damping = 12.0f};
              float heavyVal = uiState->motionManager.animateSpring<float>(
                  MotionHandle{springId + 4}, springActive ? 85.0f : 0.0f,
                  heavyConfig);

              Div(DefaultModifier().row().gap(46).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Heavy",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("m=3, k=80",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(heavyVal, 0.0f)
                            .background("#7c3aed"_hex)
                            .rounded(10.0f));
                  });
            });
          });

      CodeBlock(
          "// 1. Default Spring: Balanced stiffness (120) & damping (14)\n"
          "float y1 = uiState->motionManager.animateSpring<float>(\n"
          "    MotionHandle{id + 1}, targetY, "
          "motion::SpringConfig::Default());\n\n"
          "// 2. Bouncy Spring: High stiffness (180), low damping (8) for "
          "overshoot\n"
          "float y2 = uiState->motionManager.animateSpring<float>(\n"
          "    MotionHandle{id + 2}, targetY, "
          "motion::SpringConfig::Bouncy());\n\n"
          "// 3. Custom Heavy Spring: Higher mass (3.0kg) for low-frequency "
          "inertia\n"
          "motion::SpringConfig heavy{.mass = 3.0f, .stiffness = 80.0f, "
          ".damping = 12.0f};\n"
          "float y3 = uiState->motionManager.animateSpring<float>(\n"
          "    MotionHandle{id + 3}, targetY, heavy);\n\n"
          "// Render with .translate(0.0f, -y) for visual GPU displacement\n"
          "Div(DefaultModifier().translate(0.0f, "
          "-y1).background(\"#0284c7\"_hex));",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 3. Multi-Track GSAP-Style Timelines
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(14).widthGrow(), []() {
      Text("3. GSAP-Style Multi-Track Timeline Orchestration",
           DefaultModifier().fontSize(20).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Timelines allow you to chain, overlap, and control multi-element "
           "choreographies. "
           "Full playback controls are supported: play(), pause(), stop(), "
           "restart(), seek(timestamp), and timeScale(speed).",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(32)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(20)
              .center(),
          [&]() {
            auto *uiState = getUiState();
            static float box1X = 0.0f;
            static float box2X = 0.0f;
            static float box3X = 0.0f;

            Column(DefaultModifier().gap(28).margin(24, 0), [&]() {
              // Timeline Control Buttons
              Row(DefaultModifier().gap(12), [&]() {
                if (Button(DefaultModifier()
                               .id("TlPlayBtn")
                               .background("#10b981"_hex),
                           [&]() {
                             Text("Play Timeline", DefaultModifier()
                                                       .fontSize(12)
                                                       .fontWeight(600)
                                                       .color(Colors::white));
                           })
                        .clicked) {

                  auto &tl = uiState->motionManager.createTimeline();
                  tl.stop();

                  using namespace atomic::motion;
                  Tween<float> tw1(uiState->motionManager.createHandle(),
                                   makeProperty(box1X), 0.0f, 220.0f, 0.4f);
                  tw1.setCurve(AnimationCurve::ExpoOut());
                  tl.append(std::move(tw1));

                  Tween<float> tw2(uiState->motionManager.createHandle(),
                                   makeProperty(box2X), 0.0f, 220.0f, 0.4f);
                  tw2.setCurve(AnimationCurve::EaseOut());
                  tl.join(std::move(tw2), -0.1f);

                  Tween<float> tw3(uiState->motionManager.createHandle(),
                                   makeProperty(box3X), 0.0f, 220.0f, 0.4f);
                  tw3.setSpring(SpringConfig::Bouncy());
                  tl.append(std::move(tw3));

                  tl.play();
                }

                if (Button(DefaultModifier()
                               .id("TlResetBtn")
                               .background("#64748b"_hex),
                           [&]() {
                             Text("Reset", DefaultModifier()
                                               .fontSize(12)
                                               .fontWeight(600)
                                               .color(Colors::white));
                           })
                        .clicked) {
                  uiState->motionManager.clearTimelines();
                  box1X = 0.0f;
                  box2X = 0.0f;
                  box3X = 0.0f;
                }
              });

              // Box 1 (ExpoOut)
              Div(DefaultModifier().row().gap(16).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Box 1",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("ExpoOut",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(box1X, 0.0f)
                            .background("#0284c7"_hex)
                            .rounded(10.0f));
                  });

              // Box 2 (EaseOut)
              Div(DefaultModifier().row().gap(16).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Box 2",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("EaseOut",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(box2X, 0.0f)
                            .background("#d97706"_hex)
                            .rounded(10.0f));
                  });

              // Box 3 (Bouncy)
              Div(DefaultModifier().row().gap(16).alignY(AlignmentY::Center),
                  [&]() {
                    Column(DefaultModifier().gap(4), [&]() {
                      Text("Box 3",
                           DefaultModifier().fontSize(11).fontWeight(600).color(
                               Colors::black[900]));
                      Text("Bouncy",
                           DefaultModifier().fontSize(9).color("#475569"_hex));
                    });
                    Div(DefaultModifier()
                            .size(90, 80)
                            .translate(box3X, 0.0f)
                            .background("#4f46e5"_hex)
                            .rounded(10.0f));
                  });
            });
          });

      CodeBlock("using namespace atomic::motion;\n\n"
                "Timeline& tl = uiState->motionManager.createTimeline();\n\n"
                "// Track 1: Append sequential track\n"
                "Tween<float> tw1(mgr.createHandle(), makeProperty(box1X), "
                "0.0f, 220.0f, 0.4f);\n"
                "tw1.setCurve(AnimationCurve::ExpoOut());\n"
                "tl.append(std::move(tw1));\n\n"
                "// Track 2: Join parallel track with -100ms offset\n"
                "Tween<float> tw2(mgr.createHandle(), makeProperty(box2X), "
                "0.0f, 220.0f, 0.4f);\n"
                "tl.join(std::move(tw2), -0.1f);\n\n"
                "// Track 3: Append spring bounce track\n"
                "Tween<float> tw3(mgr.createHandle(), makeProperty(box3X), "
                "0.0f, 220.0f, 0.4f);\n"
                "tw3.setSpring(SpringConfig::Bouncy());\n"
                "tl.append(std::move(tw3));\n\n"
                "tl.play();",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 4. Container Stagger Choreography (staggerTo)
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(14).padding(10, 100).widthGrow(), []() {
      Text("4. Container Stagger Choreography (staggerTo)",
           DefaultModifier().fontSize(20).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Automatically cascade entrance animations across arrays or lists "
           "of elements with fixed delay offsets.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(32)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(20)
              .center(),
          [&]() {
            auto *uiState = getUiState();
            static std::vector<float> itemScales = {1.0f, 1.0f, 1.0f,
                                                    1.0f, 1.0f, 1.0f};

            if (Button(
                    DefaultModifier()
                        .id("TriggerStaggerBtn")
                        .background("#8b5cf6"_hex),
                    [&]() {
                      Text("Cascade Stagger Entrance",
                           DefaultModifier().fontSize(12).fontWeight(600).color(
                               Colors::white));
                    })
                    .clicked) {

              for (float &s : itemScales)
                s = 0.2f;

              using namespace atomic::motion;
              Timeline &tl = uiState->motionManager.createTimeline();

              std::vector<PropertyRef<float>> targets;
              for (float &s : itemScales) {
                targets.push_back(makeProperty(s));
              }

              tl.staggerTo<float>(targets,
                                  1.0f,  // Target scale
                                  0.30f, // Duration per item
                                  0.05f, // 50ms stagger interval offset
                                  AnimationCurve::BackOut() // Overshoot curve
              );

              tl.play();
            }

            Row(DefaultModifier().gap(12).margin(10, 0).center(), [&]() {
              for (size_t i = 0; i < itemScales.size(); ++i) {
                Div(DefaultModifier()
                        .size(90, 80)
                        .scale(itemScales[i])
                        .background("#a855f7"_hex)
                        .rounded(8.0f)
                        .center(),
                    [&]() {
                      Text(std::to_string(i + 1),
                           DefaultModifier().fontSize(15).fontWeight(600).color(
                               Colors::white));
                    });
              }
            });
          });

      CodeBlock(
          "using namespace atomic::motion;\n\n"
          "std::vector<PropertyRef<float>> targets;\n"
          "for (float& scale : itemScales) {\n"
          "    targets.push_back(makeProperty(scale));\n"
          "}\n\n"
          "// Automatically cascade scale animations with 50ms interval "
          "offset\n"
          "timeline.staggerTo<float>(\n"
          "    targets,\n"
          "    1.0f,                       // Target scale\n"
          "    0.30f,                      // Duration per item\n"
          "    0.05f,                      // 50ms interval delay offset\n"
          "    AnimationCurve::BackOut()   // Overshoot back-curve\n"
          ");\n\n"
          "timeline.play();",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 5. Type-Safe Retained Tweens & Property References
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(14).widthGrow(), []() {
      Text(
          "5. Type-Safe Retained Tweens & Property References (PropertyRef<T>)",
          DefaultModifier().fontSize(20).fontWeight(600).textColor(
              Colors::black[900]));
      Text("In addition to immediate-mode calls, Atomic.Motion allows binding "
           "memory addresses directly to Tween<T> instances. "
           "Supports builder-style configurations, custom spring settings, and "
           "zero-allocation lifecycle callbacks.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      CodeBlock(
          "using namespace atomic::motion;\n\n"
          "struct CardState {\n"
          "    float opacity{0.0f};\n"
          "    float posY{-50.0f};\n"
          "};\n\n"
          "CardState state;\n"
          "PropertyRef<float> opacityProp = makeProperty(state.opacity);\n"
          "PropertyRef<float> posProp     = makeProperty(state.posY);\n\n"
          "// Create and configure standalone tween\n"
          "Tween<float> tween(mgr.createHandle(), posProp, -50.0f, 0.0f, "
          "0.35f);\n"
          "tween.setSpring(SpringConfig::Bouncy());\n"
          "tween.setOnComplete([](void* data) {\n"
          "    std::println(\"Card entrance spring finished!\");\n"
          "});\n\n"
          "mgr.createTween(opacityProp, 0.0f, 1.0f, 0.25f);",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 6. Complete Robert Penner Easing Suite
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(14).widthGrow(), []() {
      Text("6. Robert Penner Easing Curves Gallery (Curves.h & Easing::*)",
           DefaultModifier().fontSize(20).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Atomic.Motion provides both AnimationCurve presets (evaluated via "
           "Newton-Raphson Bezier solvers) "
           "and pure analytical scalar easing functions inside namespace "
           "atomic::motion::Easing.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(24)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(12),
          [&]() {
            Row(DefaultModifier().gap(16).center(), [&]() {
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("AnimationCurve::EaseOut()",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("AnimationCurve::ExpoOut()",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("AnimationCurve::BackOut(1.5f)",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
            });

            Row(DefaultModifier().gap(16).center(), [&]() {
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("Easing::BounceOut(t)",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("Easing::ElasticOut(t)",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
              Div(DefaultModifier()
                      .padding(8, 14)
                      .background("#f1f5f9"_hex)
                      .rounded(6.0f),
                  [&]() {
                    Text("Easing::SineInOut(t)",
                         DefaultModifier().fontSize(11).color("#334155"_hex));
                  });
            });
          });

      CodeBlock("using namespace atomic::motion;\n"
                "using namespace atomic::motion::Easing;\n\n"
                "// 1. Bezier AnimationCurve Presets\n"
                "auto c1 = AnimationCurve::EaseOut();\n"
                "auto c2 = AnimationCurve::ExpoOut();\n"
                "auto c3 = AnimationCurve::BackOut(1.5f);  // GSAP-style "
                "configurable overshoot\n\n"
                "// 2. Pure Analytical Easing Functions\n"
                "float v1 = BackOut(t);      // Overshoot displacement\n"
                "float v2 = BounceOut(t);    // Physical collision bounce\n"
                "float v3 = ElasticOut(t);   // Damped harmonic wave\n"
                "float v4 = ExpoOut(t);      // Ultra-fast exponential decay",
                "cpp");
    });
    drawHeroAnimationShowcase();
  });
}

} // namespace atomic::docs
