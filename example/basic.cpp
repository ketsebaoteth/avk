#include "avk/atomic_ui.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "ui/color.h"
#include "ui/components.h"
#include <print>

std::string myTextBuffer = "";

void drawUI(VeraWindow *window) {
  auto width = static_cast<float>(atomic::getWidth(window));
  auto height = static_cast<float>(atomic::getHeight(window));

  using namespace atomic;

  Column(DefaultModifier()
             .background("#050505"_hex)
             .size(width, height)
             .padding(20, 20)
             .gap(15),
         [&]() {
           Row(DefaultModifier()
                   .background("#171717"_hex)
                   .size(width - 40.0f, 100.0f)
                   .rounded(12.0f)
                   .padding(10, 10)
                   .gap(10),
               [&]() {
                 auto mybtn = Button("mybtn", DefaultModifier());
                 if (mybtn.hovered) {
                   // std::println("Hovered!\n");
                 }
                 TextInput(DefaultModifier(), myTextBuffer, "placeholder...");
               });

           Column(DefaultModifier()
                      .background("#171717"_hex)
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
