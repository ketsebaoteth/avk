#pragma once

#include "ui/components.h"
#include "ui/style/modifier.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

#include <functional>
#include <string>

namespace atomic::docs {

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

  Column(DefaultModifier().gap(32).widthGrow(), [&]() {
    drawHeader(
        "TextInput",
        "An interactive single-line text entry component supporting live UTF-8 "
        "editing, dynamic caret positioning, text selection, placeholder "
        "rendering, and password masking.");

    // -------------------------------------------------------------------------
    // 1. Standard Input & Live Data Binding
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("1. Standard Text Input & Live State Sync",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Tests real-time keystroke processing, caret positioning, backspace "
           "handling, and state reflection across frames.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            TextInput(standardBuffer, "Type something here...",
                      DefaultModifier().id("stdInputTest").width(400));

            // Live state output indicator
            Div(DefaultModifier()
                    .padding(10, 14)
                    .background("#f4f4f5"_hex)
                    .rounded(8.0f)
                    .row()
                    .gap(8)
                    .center(),
                [&]() {
                  Text("Buffer Value:",
                       DefaultModifier().fontSize(13).fontWeight(600).textColor(
                           Colors::gray[700]));
                  Text(standardBuffer.empty() ? "(empty)" : standardBuffer,
                       DefaultModifier().fontSize(13).textColor(
                           standardBuffer.empty() ? Colors::gray[400]
                                                  : Colors::black[1000]));
                });
          });

      CodeBlock("static std::string textBuffer = \"\";\n\n"
                "// Standard TextInput bound to a std::string buffer\n"
                "TextInput(textBuffer, \"Type something here...\",\n"
                "          DefaultModifier().id(\"stdInput\").width(400));",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 2. Password Obfuscation (isPassword = true)
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("2. Password Masking (isPassword = true)",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Replaces rendered codepoints with bullet glyphs (•) while "
           "preserving "
           "exact UTF-8 byte offset tracking for caret placement.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            Row(DefaultModifier().gap(12).center(), [&]() {
              TextInput(DefaultModifier().id("passInputTest").width(350),
                        passwordBuffer, "Enter secret key...",
                        TextConfig{.isPassword = true});

              if (Button(DefaultModifier().id("togglePassBtn").padding(8, 12),
                         [&]() {
                           Text(showRawPassword ? "Hide Raw" : "Show Raw",
                                DefaultModifier().fontSize(12).fontWeight(500));
                         })
                      .clicked) {
                showRawPassword = !showRawPassword;
              }
            });

            if (showRawPassword) {
              Div(DefaultModifier()
                      .padding(10, 14)
                      .background("#fef2f2"_hex)
                      .border("#fecaca"_hex, 1)
                      .rounded(6.0f),
                  [&]() {
                    Text("Raw Buffer: " + passwordBuffer,
                         DefaultModifier().fontSize(12).textColor(
                             "#dc2626"_hex));
                  });
            }
          });

      CodeBlock("static std::string passwordBuffer = \"\";\n\n"
                "// Password field using custom configuration\n"
                "TextInput(passwordBuffer, \"Enter secret key...\",\n"
                "          DefaultModifier().id(\"passInput\").width(350),\n"
                "          TextInputConfig{ .isPassword = true });",
                "cpp");
    });

    // -------------------------------------------------------------------------
    // 3. Pre-filled Content & Flex Layout (.widthGrow())
    // -------------------------------------------------------------------------
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("3. Pre-filled Content & Responsive Flex",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Tests full container flex growing (.widthGrow()), pre-populated "
           "string "
           "seeding, and programmatically mutated buffers.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      Div(DefaultModifier()
              .widthGrow()
              .padding(30)
              .background("#ffffff"_hex)
              .border(Colors::gray[200], 1)
              .rounded(12.0f)
              .column()
              .gap(16),
          [&]() {
            TextInput(prefilledBuffer, "Flex input...",
                      DefaultModifier().id("prefilledInputTest").widthGrow());

            Row(DefaultModifier().gap(10).center(), [&]() {
              if (Button(DefaultModifier().id("clearInputBtn").padding(6, 12),
                         [&]() {
                           Text("Clear Buffer", DefaultModifier().fontSize(12));
                         })
                      .clicked) {
                prefilledBuffer.clear();
              }

              if (Button(DefaultModifier().id("resetInputBtn").padding(6, 12),
                         [&]() {
                           Text("Reset Default",
                                DefaultModifier().fontSize(12));
                         })
                      .clicked) {
                prefilledBuffer = "Hello, Atomic UI!";
              }
            });
          });

      CodeBlock("static std::string buffer = \"Hello, Atomic UI!\";\n\n"
                "// Full-width flexible text field\n"
                "TextInput(buffer, \"Flex input...\",\n"
                "          DefaultModifier().id(\"flexInput\").widthGrow());",
                "cpp");
    });
  });
}

} // namespace atomic::docs
