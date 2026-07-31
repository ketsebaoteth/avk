#include "avk/utils/ui/layout.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "ui/components.h"
#include "ui/core/frame.h"
#include "ui/core/resources.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/style/modifier.h"
#include "ui/style/style.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"

// Modular documentation headers
#include "docs/animation.h"
#include "docs/button.h"
#include "docs/div.h"

#include <print>
#include <string>
#include <vector>

/**
 * @brief History navigation state manager ("React at home" router state).
 */
struct DocNavigation {
  std::vector<std::string> history = {"buttonTab"};
  size_t pointer = 0;

  void push(const std::string &tab) {
    if (pointer < history.size() - 1) {
      history.erase(history.begin() + pointer + 1, history.end());
    }
    if (history[pointer] != tab) {
      history.push_back(tab);
      pointer = history.size() - 1;
    }
  }

  void back() {
    if (pointer > 0)
      pointer--;
  }

  void forward() {
    if (pointer < history.size() - 1)
      pointer++;
  }

  [[nodiscard]] std::string current() const { return history[pointer]; }
  [[nodiscard]] bool canBack() const { return pointer > 0; }
  [[nodiscard]] bool canForward() const { return pointer < history.size() - 1; }
};

void drawDocHeader(const std::string &title, const std::string &subtitle,
                   DocNavigation &nav, std::string &activeTab,
                   const std::string &idPrefix) {
  using namespace atomic;
  using namespace atomicComponents;

  Column(DefaultModifier().gap(10).widthGrow(), [&]() {
    Row(DefaultModifier().widthGrow().center(), [&]() {
      Text(title, DefaultModifier().fontSize(40).fontWeight(700).textColor(
                      Colors::black[1000]));
      Div(DefaultModifier().widthGrow());

      std::string backId = idPrefix + "_goBackBtn";
      std::string fwdId = idPrefix + "_goForwardBtn";

      Toast(
          [&]() {
            if (Button(DefaultModifier().id(backId),
                       [&]() { Icon(LucideIcon::CornerUpLeft); })
                    .clicked &&
                nav.canBack()) {
              nav.back();
              activeTab = nav.current();
            }
          },
          [&]() { Text("Go back to prev page"); });

      Toast(
          [&]() {
            if (Button(DefaultModifier().id(fwdId),
                       [&]() { Icon(LucideIcon::CornerUpRight); })
                    .clicked &&
                nav.canForward()) {
              nav.forward();
              activeTab = nav.current();
            }
          },
          [&]() { Text("Go to next page"); });
    });
    Text(subtitle, DefaultModifier().fontSize(14).fontWeight(400).textColor(
                       Colors::black[500]));
  });
}

void drawProfileMenuScene(VeraWindow *window, uint32_t banner) {
  using namespace atomic;
  using namespace atomicComponents;

  (void)banner;

  auto width = static_cast<float>(getWidth(window));
  auto height = static_cast<float>(getHeight(window));
  static std::string searchInput = "";
  static std::string activeTab = "buttonTab";
  static DocNavigation docNav;

  Row(DefaultModifier().background("#ffffff"_hex).size(width, height), [&]() {
    // -------------------------------------------------------------------------
    // Sidebar Navigation
    // -------------------------------------------------------------------------
    Div(DefaultModifier()
            .heightGrow()
            .padding(30, 50)
            .column()
            .gap(10)
            .relative(),
        [&]() {
          Div(DefaultModifier()
                  .absolute()
                  .right(0)
                  .width(1.0f)
                  .heightGrow()
                  .linearGradient(180.0f, {
                                              {Colors::transparent, 0.2f},
                                              {Colors::gray[200], 0.5f},
                                              {Colors::transparent, 0.8f},
                                          }));

          Text("Gallery", DefaultModifier().fontSize(20).fontWeight(600).color(
                              "#000000"_hex));
          TextInput(searchInput, "search ...",
                    DefaultModifier().id("searchField").width(500));

          Div(DefaultModifier().column().heightGrow().widthGrow().padding(5,
                                                                          50),
              [&]() {
                if (TabButton("buttonTab", "Button", activeTab)) {
                  activeTab = "buttonTab";
                  docNav.push("buttonTab");
                }
                if (TabButton("divTab", "Div", activeTab)) {
                  activeTab = "divTab";
                  docNav.push("divTab");
                }
                if (TabButton("animationTab", "Animation Engine", activeTab)) {
                  activeTab = "animationTab";
                  docNav.push("animationTab");
                }
              });
        });

    // -------------------------------------------------------------------------
    // Documentation Content Viewport
    // -------------------------------------------------------------------------
    ScrollView(
        DefaultModifier().heightGrow().id("DocsScrollView").widthGrow(), [&]() {
          Div(DefaultModifier()
                  .padding(200, 150)
                  .alignX(AlignmentX::Center)
                  .column()
                  .widthGrow()
                  .heightGrow(),
              [&]() {
                auto headerBinder = [&](const std::string &title,
                                        const std::string &subtitle) {
                  drawDocHeader(title, subtitle, docNav, activeTab, activeTab);
                };

                if (activeTab == "buttonTab") {
                  atomic::docs::drawButtonDoc(headerBinder);
                } else if (activeTab == "divTab") {
                  atomic::docs::drawDivDoc(headerBinder);
                } else if (activeTab == "animationTab") {
                  atomic::docs::drawAnimationDoc(headerBinder);
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
                                        .title = "atomicUI Modern Composer",
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
  auto id = atomic::loadTexture("images/Banner.png");

  bool isClosing = false;
  window->setCloseRequestCallback([&]() -> bool {
    isClosing = true;
    atomic::unregisterWindow(window);
    app.destroyWindow(window);
    return true;
  });

  window->setResizeCallback(
      [&isClosing, window, id](uint32_t newWidth, uint32_t newHeight) {
        if (isClosing)
          return;

        atomic::resizeWindow(window, newWidth, newHeight);

        window->setTitlebarHitTestRegions(
            {.dragRegion = VeraRect{0, 0, newWidth, 30},
             .minimizeButton = VeraRect{newWidth - 60, 5, 30, 20},
             .maximizeButton = VeraRect{newWidth - 30, 5, 30, 20},
             .closeButton = VeraRect{newWidth, 5, 30, 20}});

        if (atomic::beginFrame(window)) {
          drawProfileMenuScene(window, id);
          atomic::endFrame(window);
        }
      });

  while (app.getWindowCount() > 0) {
    app.pollEvents();
    if (app.getWindowCount() == 0) {
      break;
    }

    if (atomic::beginFrame(window)) {
      drawProfileMenuScene(window, id);
      atomic::endFrame(window);
    }
  }

  atomic::shutdown();

  return 0;
}
