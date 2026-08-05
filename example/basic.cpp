#include "core/app/App.h"
#include "core/app/Types.h"
#include "ui/components.h"
#include "ui/core/frame.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "ui/style/modifier.h"
#include "ui/style/style.h"
#include "ui/style/themeManager.h"
#include "ui/utils/color.h"

#include <print>
#include <string>

void drawTextBenchmarkScene(VeraWindow *window) {
  using namespace atomic;

  auto width = static_cast<float>(getWidth(window));
  auto height = static_cast<float>(getHeight(window));
  auto &tm = ThemeManager::getInstance();
  static auto val = 0;
  // Root background container
  Div(Modifier()
          .background(
              tm.getVariable<glm::vec4>(ThemeVarId::ColorBgApp, "#ffffff"_hex))
          .row()
          .gap(30)
          .size(width, height),
      [&]() {
        // decrease by one
        if (Button(Modifier().id("MinusI"), []() { Icon(LucideIcon::Minus); }))
          val--;

        // show value
        Text(std::format("{}", val), Modifier());

        // increase by one
        if (Button(Modifier().id("plusB"), []() { Icon(LucideIcon::Plus); }))
          val++;
        Div(Modifier()
                .width(360)
                .height(240)
                .background("#17191e"_hex)
                .border("#2a2d36"_hex, 1)
                .rounded(12)
                .padding(20)
                .column()
                .resize(ResizeConfig{
                    .allowedEdges = ResizeEdge::BottomRight,
                    .minSize = glm::vec2(200, 150),

                    // ⚡ Handle lambda now returns Interaction!
                    .onRenderBottomRight =
                        [](const HandleState &state) {
                          glm::vec4 knobBg =
                              state.isDragging
                                  ? "#3b82f6"_hex
                                  : (state.isHovered ? "#60a5fa"_hex
                                                     : "#3f3f46"_hex);

                          // Render knob Div and RETURN its Interaction struct
                          // directly
                          Div(Modifier()
                                  .size(12, 12)
                                  .background(knobBg)
                                  .rounded(3)
                                  .onHover(
                                      Modifier().background("#93c5fd"_hex)));
                        }}),
            [&]() { Text("Interactive Handle Knob Example!"); });
      });
}

int main() {
  VeraApp app(VeraAppInfo{.enablePlatformDebugging = false,
                          .preferedLinuxProtocol = VeraLinuxProtocol::Wayland});

  auto windowResult = app.createWindow({.width = 800,
                                        .height = 600,
                                        .title = "atomicUI Text Benchmark",
                                        .customTitleBar = true});
  if (!windowResult) {
    std::println("window creation failed: {}", windowResult.error().info);
    return 1;
  }
  VeraWindow *window = windowResult.value();

  VeraNativeHandle handle = window->getNativeHandle();
  atomic::initialize(app, handle, true);
  atomic::registerWindow(window);

  window->setTitlebarHitTestRegions({.dragRegion = VeraRect{0, 0, 800, 30},
                                     .minimizeButton = VeraRect{740, 5, 30, 20},
                                     .maximizeButton = VeraRect{770, 5, 30, 20},
                                     .closeButton = VeraRect{800, 5, 30, 20}});

  bool isClosing = false;
  window->setCloseRequestCallback([&]() -> bool {
    isClosing = true;
    atomic::unregisterWindow(window);
    app.destroyWindow(window);
    return true;
  });

  window->setResizeCallback(
      [&isClosing, window](uint32_t newWidth, uint32_t newHeight) {
        if (isClosing)
          return;

        atomic::resizeWindow(window, newWidth, newHeight);

        window->setTitlebarHitTestRegions(
            {.dragRegion = VeraRect{0, 0, newWidth, 30},
             .minimizeButton = VeraRect{newWidth - 60, 5, 30, 20},
             .maximizeButton = VeraRect{newWidth - 30, 5, 30, 20},
             .closeButton = VeraRect{newWidth, 5, 30, 20}});

        if (atomic::beginFrame(window)) {
          drawTextBenchmarkScene(window);
          atomic::endFrame(window);
        }
      });

  while (app.getWindowCount() > 0) {
    app.pollEvents();
    if (app.getWindowCount() == 0) {
      break;
    }

    if (atomic::beginFrame(window)) {
      drawTextBenchmarkScene(window);
      atomic::endFrame(window);
    }
  }

  atomic::shutdown();

  return 0;
}
