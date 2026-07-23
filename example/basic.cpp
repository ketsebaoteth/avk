#include "avk/atomic_ui.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "glm/fwd.hpp"
#include "ui/color.h"
#include "ui/components.h"
#include <print>

std::string myTextBuffer = "";

void drawUI(VeraWindow *window, uint32_t iconindex) {
  auto width = static_cast<float>(atomic::getWidth(window));
  auto height = static_cast<float>(atomic::getHeight(window));

  using namespace atomic;

  Column(
      DefaultModifier()
          .background({0.05f, 0.05f, 0.05f, 1.0f})
          .size(width, height)
          .padding(20, 20)
          .gap(15),
      [&]() {
        Row(DefaultModifier()
                .background({0.1f, 0.1f, 0.12f, 1.0f})
                .size(width - 40.0f, 100.0f)
                .rounded(12.0f)
                .padding(10, 10)
                .gap(10),
            [&]() {
              Button(DefaultModifier(), [&]() {
                Image(DefaultModifier().size(24.0f, 24.0f), iconindex,
                      Colors::white);
              });
              auto mybtn =
                  Button(DefaultModifier(), [&]() { atomic::Text("button"); });
              if (mybtn.hovered) {
                // std::println("Hovered!\n");
              }
              TextInput(DefaultModifier(), myTextBuffer, "sample input field");
            });

        Column(DefaultModifier()
                   .background({0.08f, 0.08f, 0.1f, 1.0f})
                   .size(width - 40.0f, height - 180.0f)
                   .rounded(12.0f)
                   .padding(15.0f, 15.0f),
               [&]() {
                 atomic::Text("Some extra content to test weather things "
                              "render properly for text");
               });
      });
}

int main() {
  VeraApp app(VeraAppInfo{});

  auto windowResult = app.createWindow({.width = 800,
                                        .height = 600,
                                        .title = "atomicUI Modern Composer",
                                        .customTitleBar = true});
  if (!windowResult) {
    std::println("window creation failed: {}", windowResult.error().info);
    return 1;
  }
  VeraWindow *window = windowResult.value();

  VeraNativeHandle handle = window->getNativeHandle();
  atomic::initialize(handle, true);
  atomic::registerWindow(window);
  auto index = atomic::loadTexture("./icons/move3d.png");

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
      [&isClosing, window, index](uint32_t newWidth, uint32_t newHeight) {
        if (isClosing)
          return;

        atomic::resizeWindow(window, newWidth, newHeight);

        window->setTitlebarHitTestRegions(
            {.dragRegion = VeraRect{0, 0, newWidth, 30},
             .minimizeButton = VeraRect{newWidth - 60, 5, 30, 20},
             .maximizeButton = VeraRect{newWidth - 30, 5, 30, 20},
             .closeButton = VeraRect{newWidth, 5, 30, 20}});

        if (atomic::beginFrame(window)) {
          drawUI(window, index);
          atomic::endFrame(window);
        }
      });

  while (app.getWindowCount() > 0) {
    app.pollEvents();
    if (app.getWindowCount() == 0) {
      break;
    }

    if (atomic::beginFrame(window)) {
      drawUI(window, index);
      atomic::endFrame(window);
    }
  }

  atomic::shutdown();

  return 0;
}
