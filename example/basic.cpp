#include "animation/animation.h"
#include "avk/atomic_ui.h"
#include "avk/utils/ui/layout.h"
#include "core/app/App.h"
#include "core/app/Types.h"
#include "ui/color.h"
#include "ui/components.h"
#include "ui/lucide-icons.generated.h"
#include <iostream>
#include <print>
#include <string>
#include <vector>

/**
 * @brief Menu item component with pure alpha fade-in/out on hover.
 */
void MenuItem(LucideIcon icon, const std::string &label,
              std::function<void()> rightBadge = nullptr) {
  using namespace atomic;

  uint32_t itemId = utils::layout::getNextId("MenuItem").id;

  static std::unordered_map<uint32_t, bool> hoverMap;
  bool wasHovered = hoverMap[itemId];

  glm::vec4 targetBg = "#f4f4f5"_hex;
  targetBg.a = wasHovered ? 1.0f : 0.0f;

  glm::vec4 animatedBg =
      AnimateVec4(itemId, targetBg, 0.12f, Curves::AppleEaseOut);

  Interaction result = Button(
      DefaultModifier()
          .background(animatedBg)
          .width(244.0f)
          .padding(12, 10)
          .rounded(12.0f)
          .alignY(AlignmentY::Center)
          .gap(12)
          .row(),
      [&]() {
        Icon(icon, DefaultModifier().size(21.0f, 21.0f).color("#18181b"_hex));
        Text(label, DefaultModifier().color("#18181b"_hex));

        if (rightBadge) {
          Div(DefaultModifier().row().alignX(AlignmentX::Right).width(-1.0f),
              [&]() { rightBadge(); });
        }
      });

  hoverMap[itemId] = result.hovered;
}

void drawProfileMenuScene(VeraWindow *window, uint32_t avatarTextureId) {
  using namespace atomic;
  using namespace atomicComponents;

  static bool isMenuOpen = true;
  auto width = static_cast<float>(atomic::getWidth(window));
  auto height = static_cast<float>(atomic::getHeight(window));

  // Root Page Canvas
  Column(
      DefaultModifier()
          .background("#f4f4f5"_hex)
          .center()
          .size(width, height)
          .padding(24, 24),
      [&]() {
        // Top Bar Container
        Div(DefaultModifier()
                .row()
                .center()
                .gap(12)
                .alignX(AlignmentX::Right)
                .relative(),
            [&]() {
              // 1. "Share +" Button
              Button(DefaultModifier()
                         .background("#e4e4e7"_hex)
                         .rounded(20.0f)
                         .padding(16, 8)
                         .gap(6),
                     [&]() {
                       Text("Share", DefaultModifier().color("#18181b"_hex));
                       Icon(LucideIcon::Plus, DefaultModifier()
                                                  .size(14.0f, 14.0f)
                                                  .color("#18181b"_hex));
                     });

              // 2. Avatar Trigger Button (With Ring Border)
              Interaction avatar =
                  Div(DefaultModifier()
                          .size(44.0f, 44.0f)
                          .rounded(22.0f)
                          .border("#f97316"_hex, 2.0f) // Gradient Orange Ring
                          .padding(2, 2)
                          .center(),
                      [&]() {
                        Image(DefaultModifier()
                                  .size(36.0f, 36.0f)
                                  .rounded(18.0f)
                                  .cover(),
                              avatarTextureId);
                      });

              if (avatar.clicked) {
                isMenuOpen = !isMenuOpen;
              }

              // 3. Smooth Fade-In/Out Opacity Animation
              float opacity =
                  AnimateFloat("MenuFade", isMenuOpen ? 1.0f : 0.0f, 0.15f);

              if (opacity > 0.01f) {
                glm::vec4 popupBg = Colors::white;
                popupBg.a = opacity;

                Modifier popupStyle =
                    DefaultModifier()
                        .absolute()
                        .attach(AttachPoint::TopRight, AttachPoint::BottomRight)
                        .offset(-4.0f, 8.0f) // 8px vertical gap below avatar
                        .width(260.0f)
                        .background(popupBg)
                        .border("#e4e4e7"_hex, 1.0f)
                        .rounded(18.0f)
                        .padding(8, 8)
                        .column()
                        .gap(2);

                // 4. Floating Menu Card
                Interaction popup = Div(std::move(popupStyle), [&]() {
                  MenuItem(LucideIcon::User, "Profile");
                  MenuItem(LucideIcon::MessageCircle, "Community");

                  // Subscription + PRO Badge
                  MenuItem(LucideIcon::CreditCard, "Subscription", [&]() {
                    Div(DefaultModifier()
                            .row()
                            .center()
                            .gap(4)
                            .background(
                                "#fae8ff"_hex) // Light purple/pink badge
                            .border("#f0abfc"_hex, 1.0f)
                            .padding(8, 3)
                            .rounded(8.0f),
                        [&]() {
                          Icon(LucideIcon::Zap, DefaultModifier()
                                                    .size(12.0f, 12.0f)
                                                    .color("#a855f7"_hex));
                          Text("PRO", DefaultModifier().color("#a855f7"_hex));
                        });
                  });

                  MenuItem(LucideIcon::Sliders, "Settings");

                  // Divider Line
                  Div(DefaultModifier().width(244.0f).height(1.0f).background(
                      "#f4f4f5"_hex));

                  MenuItem(LucideIcon::Info, "Help center");
                  MenuItem(LucideIcon::LogOut, "Sign out");
                });

                // 5. Automatic Click Outside to Dismiss
                if (utils::layout::getUiState()->pointerPressed &&
                    !avatar.hovered && !popup.hovered) {
                  isMenuOpen = false;
                }
              }
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
  auto id = atomic::loadTexture("/home/k/Pictures/wallpapers/jjk.png");

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
