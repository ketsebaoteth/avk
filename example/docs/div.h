#pragma once

#include "ui/components.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

#include <functional>
#include <string>

namespace atomic::docs {

/**
 * @brief Renders the complete, interactive documentation suite for the Div
 * component.
 */
inline void drawDivDoc(
    std::function<void(const std::string &, const std::string &)> drawHeader) {
  using namespace atomic;
  using namespace atomicComponents;
  using namespace atomic::extras;

  Column(DefaultModifier().gap(32).widthGrow(), [&]() {
    drawHeader(
        "The Div",
        "A structural layout primitive that manages child alignment, "
        "spacing, and direction in flexible cross-axis configurations. "
        "Supports standard box model properties (margin, padding), background "
        "fills, borders, and built-in property interpolation for smooth "
        "layout updates.");

    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("1. Layout Direction & Spacing (Row vs Column)",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Control child distribution using flex directions (.row(), "
           ".column()) and child gap spacing (.gap(12)).",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(20)
              .center(),
          [&]() {
            // Row Layout
            Div(DefaultModifier().row().gap(12).center(), [&]() {
              Div(DefaultModifier()
                      .size(40, 40)
                      .background("#3b82f6"_hex)
                      .rounded(8.0f));
              Div(DefaultModifier()
                      .size(40, 40)
                      .background("#10b981"_hex)
                      .rounded(8.0f));
              Div(DefaultModifier()
                      .size(40, 40)
                      .background("#f59e0b"_hex)
                      .rounded(8.0f));
            });

            // Column Layout
            Div(DefaultModifier().column().gap(8).center(), [&]() {
              Div(DefaultModifier()
                      .size(120, 24)
                      .background("#e4e4e7"_hex)
                      .rounded(6.0f));
              Div(DefaultModifier()
                      .size(120, 24)
                      .background("#d4d4d8"_hex)
                      .rounded(6.0f));
            });
          });

      CodeBlock("// Row Layout with 12px gap\n"
                "Div(DefaultModifier().row().gap(12).center(), [&]() {\n"
                "    Div(DefaultModifier().size(40, "
                "40).background(\"#3b82f6\"_hex));\n"
                "    Div(DefaultModifier().size(40, "
                "40).background(\"#10b981\"_hex));\n"
                "});\n\n"
                "// Column Layout with 8px gap\n"
                "Div(DefaultModifier().column().gap(8).center(), [&]() {\n"
                "    Div(DefaultModifier().size(120, "
                "24).background(\"#e4e4e7\"_hex));\n"
                "});",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 2. Outer Margins & Inner Padding
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("2. Outer Margins vs Inner Padding",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Div transparently wraps elements with an outer margin padding "
           "container when margins are specified (.margin(20)), cleanly "
           "separating outer spacing from inner padding (.padding(16)).",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .center(),
          [&]() {
            Div(DefaultModifier()
                    .margin(20)
                    .padding(24)
                    .background("#18181b"_hex)
                    .border("#27272a"_hex, 1)
                    .rounded(10.0f)
                    .center(),
                [&]() {
                  Text("Outer Margin (20px) & Inner Padding (24px)",
                       DefaultModifier().fontSize(13).fontWeight(500).color(
                           Colors::white));
                });
          });

      CodeBlock(
          "Div(DefaultModifier()\n"
          "    .margin(20)              // Creates transparent outer spacing "
          "wrapper\n"
          "    .padding(24)             // Applies internal content padding\n"
          "    .background(\"#18181b\"_hex)\n"
          "    .rounded(10.0f),\n"
          "    [&]() {\n"
          "        Text(\"Content inside Div\");\n"
          "    }\n"
          ");",
          "cpp");
    });

    // -------------------------------------------------------------------------
    // 3. Modern Fills (Gradients & Glassmorphism)
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("3. Modern Fills (Gradients & Glassmorphism)",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Apply multi-stop OKLab linear or radial gradients, borders, and "
           "backdrop blurs directly to containers.",
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
            // Linear Gradient Box
            Div(DefaultModifier()
                    .size(160, 80)
                    .rounded(12.0f)
                    .linearGradient(135.0f,
                                    {
                                        {"#2563eb"_hex, 0.0f},
                                        {"#7c3aed"_hex, 1.0f},
                                    })
                    .center(),
                [&]() {
                  Text("Linear Gradient",
                       DefaultModifier().fontSize(12).fontWeight(600).color(
                           Colors::white));
                });

            // Glassmorphic Card
            Div(DefaultModifier()
                    .size(160, 80)
                    .rounded(12.0f)
                    .background(glm::vec4(0.09f, 0.09f, 0.11f, 0.85f))
                    .blur(16.0f)
                    .border(glm::vec4(1.0f, 1.0f, 1.0f, 0.15f), 1)
                    .center(),
                [&]() {
                  Text("Glassmorphism",
                       DefaultModifier().fontSize(12).fontWeight(600).color(
                           Colors::white));
                });
          });

      CodeBlock("// Vibrant 2-Stop Linear Gradient\n"
                "Div(DefaultModifier()\n"
                "    .size(160, 80)\n"
                "    .linearGradient(135.0f, {{\"#2563eb\"_hex, 0.0f}, "
                "{\"#7c3aed\"_hex, 1.0f}})\n"
                ");\n\n"
                "// Glassmorphic Card with Blur\n"
                "Div(DefaultModifier()\n"
                "    .size(160, 80)\n"
                "    .background(glm::vec4(0.09f, 0.09f, 0.11f, 0.85f))\n"
                "    .blur(16.0f)\n"
                "    .border(glm::vec4(1.0f, 1.0f, 1.0f, 0.15f), 1)\n"
                ");",
                "cpp");
    });
  });
}

} // namespace atomic::docs
