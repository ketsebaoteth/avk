#include "avk/utils/ui/layout.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "ui/animation/animation.h"
#include "ui/components.h"
#include "ui/core/frame.h"
#include "ui/core/resources.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/style/modifier.h"
#include "ui/style/style.h"
#include "ui/utils/color.h"
#include "ui/utils/extraComponents.h"
#include <print>
#include <string>
#include <vector>

// History navigation state manager ("React at home" router state)
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

  std::string current() const { return history[pointer]; }

  bool canBack() const { return pointer > 0; }
  bool canForward() const { return pointer < history.size() - 1; }
};

inline void MacosWindow(const std::function<void()> &content,
                        std::string title) {
  using namespace atomic;

  Div(DefaultModifier()
          .widthGrow()
          .margin(0, 40)
          .column()
          .background("#ffffff"_hex)
          .border(Colors::gray[100], 1)
          .rounded(10.0f),
      [&]() {
        Div(DefaultModifier().widthGrow().border(Colors::gray[100],
                                                 {0.0f, 0.0f, 0.0f, 1.0f}),
            [&]() {
              Div(DefaultModifier()
                      .row()
                      .alignY(AlignmentY::Center)
                      .widthGrow()
                      .padding(0, 24)
                      .gap(8),
                  [&]() {
                    Div(DefaultModifier()
                            .gap(26)
                            .alignY(AlignmentY::Center)
                            .margin(30, 0),
                        [&]() {
                          Div([]() {
                            Div(DefaultModifier()
                                    .size(12, 12)
                                    .rounded(6.0f)
                                    .background("#ff5f56"_hex));
                            Div(DefaultModifier()
                                    .size(12, 12)
                                    .rounded(6.0f)
                                    .background("#ffbd2e"_hex));
                            Div(DefaultModifier()
                                    .size(12, 12)
                                    .rounded(6.0f)
                                    .background("#27c93f"_hex));
                          });
                          Text(title, DefaultModifier()
                                          .fontSize(12)
                                          .fontWeight(400)
                                          .color(Colors::gray[300]));
                        });
                  });
            });

        Div(DefaultModifier()
                .widthGrow()
                .heightGrow()
                .alignX(AlignmentX::Center)
                .alignY(AlignmentY::Center)
                .padding(200),
            [&]() {
              if (content) {
                content();
              }
            });
      });
}

// Reusable component helper using the button interaction struct's .clicked
// property
// Updated helper taking an ID prefix
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

      // Unique ID per tab using the prefix
      std::string backId = idPrefix + "_goBackBtn";
      std::string fwdId = idPrefix + "_goForwardBtn";

      // Back Button with Interaction Check
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

      // Forward Button with Interaction Check
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

void drawButtonTab(DocNavigation &nav, std::string &activeTab) {
  using namespace atomic;
  using namespace atomicComponents;
  using namespace atomic::extras;

  Column(DefaultModifier().gap(24).widthGrow(), [&]() {
    drawDocHeader(
        "The Button",
        "At its core, a Button component is an interactive UI primitive "
        "designed to translate a physical user intent into a digital action.",
        nav, activeTab, "buttonTab");

    // 2. Default Button Example
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("Default Button",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      MacosWindow(
          []() {
            Button(DefaultModifier().id("defaultButtonInstance"), [&]() {
              Text("Button", DefaultModifier().fontSize(13).fontWeight(500));
            });
          },
          "Default button example");

      CodeBlock(
          "Button(DefaultModifier().id(\"defaultButtonInstance\"), [&]() {\n"
          "    Text(\"Button\", "
          "DefaultModifier().fontSize(13).fontWeight(500));\n"
          "});",
          "cpp");
    });

    // 3. Button with Icon & Text Content
    Column(DefaultModifier().gap(12).widthGrow(), []() {
      Text("Icon & Text Composition",
           DefaultModifier().fontSize(18).fontWeight(600).textColor(
               Colors::black[900]));
      Text("Buttons accept custom layout children via content lambdas, "
           "allowing you to compose icons and text seamlessly inside a Row.",
           DefaultModifier().fontSize(13).textColor(Colors::black[500]));

      MacosWindow(
          []() {
            Button(DefaultModifier().id("iconTextButtonInstance"), [&]() {
              Row(DefaultModifier().gap(8).center(), [&]() {
                Icon(LucideIcon::Sparkles,
                     DefaultModifier().size(14, 14).color(Colors::black[800]));
                Text("Generate Asset",
                     DefaultModifier().fontSize(13).fontWeight(500));
              });
            });
          },
          "Button with custom content layout");

      CodeBlock(
          "Button(DefaultModifier().id(\"iconTextButtonInstance\"), [&]() {\n"
          "    Row(DefaultModifier().gap(8).center(), [&]() {\n"
          "        Icon(LucideIcon::Sparkles, DefaultModifier().size(14, "
          "14).color(Colors::black[800]));\n"
          "        Text(\"Generate Asset\", "
          "DefaultModifier().fontSize(13).fontWeight(500));\n"
          "    });\n"
          "});",
          "cpp");
    });
  });
}

void drawDivTab(DocNavigation &nav, std::string &activeTab) {
  using namespace atomic;
  using namespace atomicComponents;

  Column(DefaultModifier().gap(24).widthGrow(), [&]() {
    drawDocHeader("The Div",
                  "A foundational layout primitive used to structure and space "
                  "child elements in flexible column or row arrangements.",
                  nav, activeTab, "divTab");
  });
}

void drawProfileMenuScene(VeraWindow *window, uint32_t banner) {
  using namespace atomic;
  using namespace atomicComponents;

  (void)banner; // Silence unused warning if banner isn't rendered here yet

  auto width = static_cast<float>(getWidth(window));
  auto height = static_cast<float>(getHeight(window));
  static std::string searchInput = "";
  static std::string activeTab = "buttonTab";
  static DocNavigation docNav;

  Row(DefaultModifier().background("#ffffff"_hex).size(width, height), [&]() {
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

          Text("Gallary", DefaultModifier().fontSize(20).fontWeight(600).color(
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
              });
        });

    ScrollView(DefaultModifier().heightGrow().id("DocsScrollView").widthGrow(),
               [&]() {
                 Div(DefaultModifier()
                         .padding(200, 150)
                         .alignX(AlignmentX::Center)
                         .column()
                         .widthGrow()
                         .heightGrow(),
                     [&]() {
                       if (activeTab == "buttonTab") {
                         drawButtonTab(docNav, activeTab);
                       } else if (activeTab == "divTab") {
                         drawDivTab(docNav, activeTab);
                       }
                     });
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

        // Fixed the duplicate .maximizeButton designation to .minimizeButton
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
