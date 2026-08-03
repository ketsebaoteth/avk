#pragma once

#include "ui/components.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

#include <functional>
#include <string>

namespace atomic::docs {

static std::string pinCode = "";
/**
 * @brief Renders the complete, interactive documentation and test harness
 * for the TextInput component.
 */
inline void drawInputDoc(
    std::function<void(const std::string &, const std::string &)> drawHeader) {
  using namespace atomic;
  using namespace atomicComponents;
  using namespace atomic::extras;

  // Persistent test buffers for live interaction
  static std::string standardBuffer = "";
  static std::string passwordBuffer = "";
  static std::string prefilledBuffer = "Hello, Atomic UI!";
  static bool showRawPassword = false;

  Column(Modifier().gap(32).widthGrow(), [&]() {
    drawHeader(
        "TextInput",
        "An interactive single-line text entry component supporting live UTF-8 "
        "editing, dynamic caret positioning, text selection, placeholder "
        "rendering, and password masking.");

    // -------------------------------------------------------------------------
    // 1. Standard Input & Live Data Binding
    // -------------------------------------------------------------------------
    Column(Modifier().gap(12).widthGrow(), []() {
      Text("1. Standard Text Input & Live State Sync",
           Modifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Tests real-time keystroke processing, caret positioning, backspace "
           "handling, and state reflection across frames.",
           Modifier().fontSize(13).textColor(Colors::black[500]));

      Div(Modifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            TextInput(Modifier().id("stdInputTest").width(400), standardBuffer,
                      "Type something here...");

            // Live state output indicator
            Div(Modifier()
                    .padding(10, 14)
                    .background("#f4f4f5"_hex)
                    .rounded(8.0f)
                    .row()
                    .gap(8)
                    .center(),
                [&]() {
                  Text("Buffer Value:",
                       Modifier().fontSize(13).fontWeight(600).textColor(
                           Colors::gray[700]));
                  Text(standardBuffer.empty() ? "(empty)" : standardBuffer,
                       Modifier().fontSize(13).textColor(
                           standardBuffer.empty() ? Colors::gray[400]
                                                  : Colors::black[1000]));
                });
          });

      CodeBlock("static std::string textBuffer = \"\";\n\n"
                "// Standard TextInput bound to a std::string buffer\n"
                "TextInput(textBuffer, \"Type something here...\",\n"
                "          Modifier().id(\"stdInput\").width(400));",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 2. Password Obfuscation (isPassword = true)
    // -------------------------------------------------------------------------
    Column(Modifier().gap(12).widthGrow(), []() {
      Text("2. Password Masking (isPassword = true)",
           Modifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Replaces rendered codepoints with bullet glyphs (•) while "
           "preserving "
           "exact UTF-8 byte offset tracking for caret placement.",
           Modifier().fontSize(13).textColor(Colors::black[500]));

      Div(Modifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            Row(Modifier().gap(12).center(), [&]() {
              TextInput(Modifier().id("passInputTest").width(350),
                        passwordBuffer, "Enter secret key...",
                        TextConfig{.isPassword = true});

              if (Button(Modifier().id("togglePassBtn").padding(8, 12), [&]() {
                    Text(showRawPassword ? "Hide Raw" : "Show Raw",
                         Modifier().fontSize(12).fontWeight(500));
                  }).clicked) {
                showRawPassword = !showRawPassword;
              }
            });

            if (showRawPassword) {
              Div(Modifier()
                      .padding(10, 14)
                      .background("#fef2f2"_hex)
                      .border("#fecaca"_hex, 1)
                      .rounded(6.0f),
                  [&]() {
                    Text("Raw Buffer: " + passwordBuffer,
                         Modifier().fontSize(12).textColor("#dc2626"_hex));
                  });
            }
          });

      CodeBlock("static std::string passwordBuffer = \"\";\n\n"
                "// Password field using custom configuration\n"
                "TextInput(passwordBuffer, \"Enter secret key...\",\n"
                "          Modifier().id(\"passInput\").width(350),\n"
                "          TextInputConfig{ .isPassword = true });",
                "cpp");
    });
    Column(Modifier().gap(12).widthGrow(), []() {
      Text("Custom Renderer Function",
           Modifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("render with a red div in the back",
           Modifier().fontSize(13).textColor(Colors::black[500]));

      Div(Modifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            TextConfig pinConfig{};
            pinConfig.maxLength = 6;
            pinConfig.customCharAdvance = 20.0f;
            pinConfig.customRenderer = [](const std::string &displayString,
                                          float localX, float localY,
                                          float fontSize, avk::Font *font,
                                          [[maybe_unused]] const glm::vec4
                                              &textColor) {
              float curX = localX;

              for (size_t i = 0; i < displayString.size(); ++i) {
                std::string ch(1, displayString[i]);
                float charWidth =
                    font ? font->measureText(ch, fontSize).x : 14.0f;

                // Render each typed character inside a red Div badge
                Div(Modifier()
                        .absolute()
                        .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
                        .offset(curX, localY - 4.0f)
                        .padding(8, 4)
                        .rounded(6.0f)
                        .background(Colors::red[600]),
                    [&]() {
                      Text(ch,
                           Modifier().fontSize(fontSize).fontWeight(600).color(
                               Colors::white));
                    });

                curX += charWidth + 20.0f; // 20px gap
              };
            };

            // Render TextInput with custom red character renderer
            TextInput(
                Modifier().id("customRendererTest").width(320).padding(12, 10),
                pinCode, "Enter PIN", pinConfig);
          });
    });

    // -------------------------------------------------------------------------
    // 3. Pre-filled Content & Flex Layout (.widthGrow())
    // -------------------------------------------------------------------------
    Column(Modifier().gap(12).widthGrow(), []() {
      Text("3. Pre-filled Content & Responsive Flex",
           Modifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Tests full container flex growing (.widthGrow()), pre-populated "
           "string "
           "seeding, and programmatically mutated buffers.",
           Modifier().fontSize(13).textColor(Colors::black[500]));

      Div(Modifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            TextInput(Modifier().id("prefilledInputTest").widthGrow(),
                      prefilledBuffer, "Flex input...");

            Row(Modifier().gap(10).center(), [&]() {
              if (Button(Modifier().id("clearInputBtn").padding(6, 12), [&]() {
                    Text("Clear Buffer", Modifier().fontSize(12));
                  }).clicked) {
                prefilledBuffer.clear();
              }

              if (Button(Modifier().id("resetInputBtn").padding(6, 12), [&]() {
                    Text("Reset Default", Modifier().fontSize(12));
                  }).clicked) {
                prefilledBuffer = "Hello, Atomic UI!";
              }
            });
          });

      CodeBlock("static std::string buffer = \"Hello, Atomic UI!\";\n\n"
                "// Full-width flexible text field\n"
                "TextInput(buffer, \"Flex input...\",\n"
                "          Modifier().id(\"flexInput\").widthGrow());",
                "cpp");
    });
  });
}

} // namespace atomic::docs
