#include "avk/atomic.h"
#include <print>
#include <string>

void drawTextBenchmarkScene(VeraWindow *window) {
  using namespace atomic;

  auto width = static_cast<float>(getWidth(window));
  auto height = static_cast<float>(getHeight(window));

  // Root background container
  Div(Modifier().background("#1e1e1e"_hex).size(width, height), [&]() {
    // Scrollable viewport
    ScrollView(
        Modifier().id("BenchmarkScrollView").widthGrow().heightGrow(), [&]() {
          // Column wrapper for the 1,300 text items
          Div(Modifier().column().padding(8).gap(2).widthGrow(), [&]() {
            for (int i = 0; i < 1300; ++i) {
              std::string textStr = "Text element #" + std::to_string(i) +
                                    " - Sample text for performance testing";

              Div(Modifier().padding(4).background("#2d2d2d"_hex).widthGrow(),
                  [&]() {
                    Text(textStr,
                         Modifier().fontSize(14).textColor(Colors::white));
                  });
            }
          });
        });
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
