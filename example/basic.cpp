#include "avk/atomic_ui.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "glm/fwd.hpp"
#include "ui/components.h"
#include <print>

void drawUI(VeraWindow *window) {
  auto width = static_cast<float>(atomic::getWidth(window));
  auto height = static_cast<float>(atomic::getHeight(window));

  atomic::Column(atomic::DefaultModifier()
                     .background({0.05f, 0.05f, 0.05f, 1.0f})
                     .size(width, height)
                     .padding(20, 20)
                     .gap(15),
                 [&]() {
                   atomic::Row(atomic::DefaultModifier()
                                   .background({0.1f, 0.1f, 0.12f, 1.0f})
                                   .size(width - 40.0f, 100.0f)
                                   .rounded(12.0f)
                                   .padding(10, 10)
                                   .gap(10),
                               [&]() {
                                 atomic::Button(
                                     atomic::DefaultModifier()
                                         .background({1.0f, 0.5f, 0.0f, 1.0f})
                                         .size(80.0f, 40.0f)
                                         .rounded(6.0f));
                                 auto mybtn = atomic::Button(
                                     atomic::DefaultModifier()
                                         .background({1.0f, 0.0f, 0.0f, 1.0f})
                                         .size(-1.0f, 40.0f)
                                         .rounded(6.0f));
                                 if (mybtn.hovered) {
                                   std::println("Hovered!\n");
                                 }
                               });

                   atomic::Column(atomic::DefaultModifier()
                                      .background({0.08f, 0.08f, 0.1f, 1.0f})
                                      .size(width - 40.0f, height - 180.0f)
                                      .rounded(12.0f),
                                  [&]() {});
                 });
}

int main() {
  VeraApp app(VeraAppInfo{});

  const uint32_t testWindowWidth = 800;
  uint32_t testWindowHeight = 600;
  auto windowResult = app.createWindow({.width = testWindowWidth,
                                        .height = testWindowHeight,
                                        .title = "atomicUI Modern Composer",
                                        .customTitleBar = true});
  if (!windowResult) {
    std::println("window creation failed: {}", windowResult.error().info);
    return 1;
  }
  VeraWindow *window = windowResult.value();

  VeraNativeHandle handle = window->getNativeHandle();
  atomic::initialize(handle, false);
  atomic::registerWindow(window);

  window->setTitlebarHitTestRegions({.dragRegion = VeraRect{0, 0, 800, 30},
                                     .minimizeButton = VeraRect{740, 5, 30, 20},
                                     .maximizeButton = VeraRect{770, 5, 30, 20},
                                     .closeButton = VeraRect{800, 5, 30, 20}});

  bool isClosing = false;
  window->setCloseRequestCallback([&]() -> bool {
    isClosing = true;
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
          drawUI(window);
          atomic::endFrame(window);
        }
      });

  while (app.getWindowCount() > 0) {
    app.pollEvents();
    if (app.getWindowCount() == 0) {
      break;
    }

    if (atomic::beginFrame(window)) {
      drawUI(window);
      atomic::endFrame(window);
    }
  }

  atomic::shutdown();

  return 0;
}
