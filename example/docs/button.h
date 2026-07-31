#pragma once

#include "avk/utils/ui/layout.h"
#include "ui/components.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

#include <functional>
#include <string>

namespace atomic::docs {

/**
 * @brief Renders the complete, interactive documentation suite for the Button
 * component.
 */
inline void drawButtonDoc(
    std::function<void(const std::string &, const std::string &)> drawHeader) {
  using namespace atomic;
  using namespace atomicComponents;
  using namespace atomic::extras;

  Column(DefaultModifier().gap(32).widthGrow(), [&]() {
    // -------------------------------------------------------------------------
    // Header
    // -------------------------------------------------------------------------
    drawHeader("The Button",
               "At its core, a Button component is an interactive UI primitive "
               "designed to translate user intent into digital action. "
               "Supports custom content lambdas, "
               "shadcn-style color states, smooth motion transitions, and "
               "tactile spring feedback.");

    // -------------------------------------------------------------------------
    // 1. Button Variants Showcase
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("1. Variant Palette (Primary, Secondary, Destructive, Ghost)",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Buttons adapt automatically through fluent modifiers to match "
           "design system tokens.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .row()
              .gap(16)
              .center(),
          [&]() {
            // Primary
            Button(DefaultModifier()
                       .id("btnPrimary")
                       .background("#18181b"_hex)
                       .color(Colors::white),
                   [&]() {
                     Text("Primary",
                          DefaultModifier().fontSize(13).fontWeight(500).color(
                              Colors::white));
                   });

            // Secondary
            Button(DefaultModifier()
                       .id("btnSecondary")
                       .background("#f4f4f5"_hex)
                       .color(Colors::black[900]),
                   [&]() {
                     Text("Secondary",
                          DefaultModifier().fontSize(13).fontWeight(500).color(
                              Colors::black[900]));
                   });

            // Destructive
            Button(DefaultModifier()
                       .id("btnDestructive")
                       .background("#ef4444"_hex)
                       .color(Colors::white),
                   [&]() {
                     Text("Destructive",
                          DefaultModifier().fontSize(13).fontWeight(500).color(
                              Colors::white));
                   });

            // Ghost / Outline
            Button(DefaultModifier()
                       .id("btnOutline")
                       .background(Colors::transparent)
                       .border(Colors::gray[300], 1),
                   [&]() {
                     Text("Outline",
                          DefaultModifier().fontSize(13).fontWeight(500).color(
                              Colors::black[800]));
                   });
          });

      CodeBlock(
          "// Primary Dark Zinc Button\n"
          "Button(DefaultModifier().id(\"primary\").background(\"#18181b\"_hex)"
          ", [&]() {\n"
          "    Text(\"Primary\", DefaultModifier().color(Colors::white));\n"
          "});\n\n"
          "// Destructive Red Button\n"
          "Button(DefaultModifier().id(\"destructive\").background(\"#ef4444\"_"
          "hex), [&]() {\n"
          "    Text(\"Destructive\", DefaultModifier().color(Colors::white));\n"
          "});",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 2. Icon & Content Composition
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("2. Rich Content Composition",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Because buttons accept arbitrary lambda children, you can compose "
           "vector icons, badge counts, and typography freely.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .row()
              .gap(16)
              .center(),
          [&]() {
            Button(
                DefaultModifier().id("btnComposer").background("#2563eb"_hex),
                [&]() {
                  Row(DefaultModifier().gap(8).center(), [&]() {
                    Icon(LucideIcon::Sparkles,
                         DefaultModifier().size(14, 14).color(Colors::white));
                    Text("Generate AI Asset",
                         DefaultModifier().fontSize(13).fontWeight(500).color(
                             Colors::white));
                  });
                });

            Button(
                DefaultModifier().id("btnDownload").background("#10b981"_hex),
                [&]() {
                  Row(DefaultModifier().gap(8).center(), [&]() {
                    Icon(LucideIcon::Download,
                         DefaultModifier().size(14, 14).color(Colors::white));
                    Text("Download File",
                         DefaultModifier().fontSize(13).fontWeight(500).color(
                             Colors::white));
                  });
                });
          });

      CodeBlock("Button(DefaultModifier().id(\"aiBtn\").background(\"#2563eb\"_"
                "hex), [&]() {\n"
                "    Row(DefaultModifier().gap(8).center(), [&]() {\n"
                "        Icon(LucideIcon::Sparkles, DefaultModifier().size(14, "
                "14).color(Colors::white));\n"
                "        Text(\"Generate AI Asset\", "
                "DefaultModifier().color(Colors::white));\n"
                "    });\n"
                "});",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 3. Tactile Physical Spring Feedback
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("3. Tactile Spring Press Dynamics",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Combine button click interactions with atomic::motion spring "
           "simulations to create physical push responses.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .center(),
          [&]() {
            auto *uiState = getUiState();
            uint32_t springBtnId = hashLabel("TactileSpringBtn");

            Interaction btn = Button(
                DefaultModifier()
                    .id("TactileSpringBtn")
                    .background("#f59e0b"_hex),
                [&]() {
                  Text("Press For Spring Bounce",
                       DefaultModifier().fontSize(13).fontWeight(600).color(
                           Colors::white));
                });

            // Evaluate spring scale displacement
            float springScale = uiState->motionManager.animateSpring<float>(
                motion::MotionHandle{springBtnId + 0x900},
                btn.pressed ? 0.90f : 1.0f, motion::SpringConfig::Bouncy());

            // Re-render button preview with spring scale
            Button(DefaultModifier()
                       .id("SpringPreview")
                       .scale(springScale)
                       .background("#f59e0b"_hex),
                   [&]() {
                     Text("Spring Tactile Push",
                          DefaultModifier().fontSize(13).fontWeight(600).color(
                              Colors::white));
                   });
          });

      CodeBlock("Interaction btn = Button(DefaultModifier().id(\"tactile\"), "
                "[&]() {\n"
                "    Text(\"Push Me\");\n"
                "});\n\n"
                "// Dynamic tactile spring displacement\n"
                "float scale = uiState->motionManager.animateSpring<float>(\n"
                "    MotionHandle{id + 0x900},\n"
                "    btn.pressed ? 0.90f : 1.0f,\n"
                "    motion::SpringConfig::Bouncy()\n"
                ");",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 4. Loading & Disabled States
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("4. Disabled & Loading States",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Disabled buttons automatically suppress click/hover signals and "
           "apply opacity cascading.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .row()
              .gap(16)
              .center(),
          [&]() {
            Button(DefaultModifier().id("btnDisabled").disabled(true), [&]() {
              Text("Disabled Action",
                   DefaultModifier().fontSize(13).fontWeight(500).color(
                       Colors::black[400]));
            });
          });

      CodeBlock(
          "Button(DefaultModifier().id(\"disabled\").disabled(true), [&]() {\n"
          "    Text(\"Disabled Action\", "
          "DefaultModifier().color(Colors::black[400]));\n"
          "});",
          "cpp");
    });
  });
}

} // namespace atomic::docs
