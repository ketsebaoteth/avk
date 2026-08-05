#include "ui/components.h"
#include "ui/internal/context.h"
#include "ui/utils/color.h"

namespace atomic {

namespace Curves {
const AnimationCurve &AppleEaseOut() {
  static const AnimationCurve c =
      AnimationCurve::Custom(0.16f, 1.00f, 0.30f, 1.00f);
  return c;
}
const AnimationCurve &AppleSnappy() {
  static const AnimationCurve c =
      AnimationCurve::Custom(0.19f, 1.00f, 0.22f, 1.00f);
  return c;
}
const AnimationCurve &AppleEaseInOut() {
  static const AnimationCurve c =
      AnimationCurve::Custom(0.42f, 0.00f, 0.58f, 1.00f);
  return c;
}
const AnimationCurve &Emphasized() {
  static const AnimationCurve c =
      AnimationCurve::Custom(0.05f, 0.70f, 0.10f, 1.00f);
  return c;
}
const AnimationCurve &SmoothSwift() {
  static const AnimationCurve c =
      AnimationCurve::Custom(0.40f, 0.00f, 0.20f, 1.00f);
  return c;
}
} // namespace Curves

Interaction Div(const std::function<void()> &content) {
  return Div(Modifier(), content);
}

Interaction Row(Modifier &&modifier, const std::function<void()> &content) {
  return Div(std::move(modifier).row(), content);
}

Interaction Column(Modifier &&modifier, const std::function<void()> &content) {
  return Div(std::move(modifier).column(), content);
}

Interaction Text(std::string_view text) { return Text(text, Modifier()); }

Interaction Button(const std::string &label, Modifier &&modifier) {
  return Button(std::move(modifier), [label]() { Text(label, Modifier()); });
}

void ScrollView(Modifier &&modifier, std::function<void()> contentCallback) {
  ScrollView(std::move(modifier), ScrollViewConfig{},
             std::move(contentCallback));
}

void ScrollView(std::function<void()> contentCallback) {
  ScrollView(Modifier(), ScrollViewConfig{}, std::move(contentCallback));
}

} // namespace atomic

namespace atomicComponents {

bool TabButton(const std::string &tabId, const std::string &label,
               std::string &activeTab) {
  using namespace atomic;

  bool isActive = (activeTab == tabId);

  glm::vec4 activeBg = "#f4f4f5"_hex;
  glm::vec4 inactiveBg = glm::vec4(activeBg.r, activeBg.g, activeBg.b, 0.0f);
  glm::vec4 hoverBg = "#e4e4e7"_hex;

  glm::vec4 activeTextColor = "#09090b"_hex;
  glm::vec4 inactiveTextColor = "#71717a"_hex;

  bool isBtnHovered = !isActive && isHovered(hashLabel(tabId));
  glm::vec4 targetBg =
      isActive ? activeBg : (isBtnHovered ? hoverBg : inactiveBg);

  Modifier style = Modifier()
                       .id(tabId)
                       .background(targetBg)
                       .color(isActive ? activeTextColor : inactiveTextColor)
                       .fontSize(13.0f)
                       .fontWeight(isActive ? 500.0f : 400.0f)
                       .padding(isBtnHovered ? 24 : 14, 8)
                       .rounded(6.0f)
                       .borderless()
                       .transition(0.4f, Curves::AppleEaseOut());

  Interaction result = Button(label, std::move(style));

  if (result.clicked) {
    activeTab = tabId;
  }

  return result.clicked;
}

} // namespace atomicComponents
