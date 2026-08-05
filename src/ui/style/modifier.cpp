#include "ui/style/modifier.h"
#include "ui/internal/context.h"

namespace atomic {

Modifier Modifier::animateScale(float targetScale, float duration,
                                AnimationCurve curve) && {
  auto *uiState = getUiState();
  if (!uiState)
    return std::move(*this).scale(targetScale);

  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);

  float animatedScale = uiState->motionManager.animate<float>(
      motion::MotionHandle(rawId, "scale"), targetScale, duration, curve);

  return std::move(*this).scale(animatedScale);
}

Modifier Modifier::animateScale(const MotionState<float> &state, float duration,
                                AnimationCurve curve) && {
  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);
  bool hov = isHovered(rawId);
  bool prs = isPressed(rawId);

  float target = state.resolve(hov, prs);
  return std::move(*this).animateScale(target, duration, curve);
}

Modifier Modifier::animateBackground(const glm::vec4 &targetBg, float duration,
                                     AnimationCurve curve) && {
  auto *uiState = getUiState();
  if (!uiState)
    return std::move(*this).background(targetBg);

  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);

  glm::vec4 animatedBg = uiState->motionManager.animate<glm::vec4>(
      motion::MotionHandle(rawId, "bg"), targetBg, duration, curve);

  return std::move(*this).background(animatedBg);
}

Modifier Modifier::animateBackground(const MotionState<glm::vec4> &state,
                                     float duration, AnimationCurve curve) && {
  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);
  bool hov = isHovered(rawId);
  bool prs = isPressed(rawId);

  glm::vec4 target = state.resolve(hov, prs);
  return std::move(*this).animateBackground(target, duration, curve);
}

Modifier Modifier::animateBorder(const glm::vec4 &targetColor, float thickness,
                                 float duration, AnimationCurve curve) && {
  auto *uiState = getUiState();
  if (!uiState)
    return std::move(*this).border(targetColor, thickness);

  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);

  glm::vec4 animatedColor = uiState->motionManager.animate<glm::vec4>(
      motion::MotionHandle(rawId, "border"), targetColor, duration, curve);

  return std::move(*this).border(animatedColor, thickness);
}

Modifier Modifier::animateBorder(const MotionState<glm::vec4> &state,
                                 float thickness, float duration,
                                 AnimationCurve curve) && {
  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);
  bool hov = isHovered(rawId);
  bool prs = isPressed(rawId);

  glm::vec4 target = state.resolve(hov, prs);
  return std::move(*this).animateBorder(target, thickness, duration, curve);
}

Modifier Modifier::animateTranslate(const glm::vec2 &targetTranslate,
                                    float duration, AnimationCurve curve) && {
  auto *uiState = getUiState();
  if (!uiState)
    return std::move(*this).translate(targetTranslate.x, targetTranslate.y);

  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);

  float tx = uiState->motionManager.animate<float>(
      motion::MotionHandle(rawId, "tx"), targetTranslate.x, duration, curve);
  float ty = uiState->motionManager.animate<float>(
      motion::MotionHandle(rawId, "ty"), targetTranslate.y, duration, curve);

  return std::move(*this).translate(tx, ty);
}

Modifier Modifier::animateTranslate(const MotionState<glm::vec2> &state,
                                    float duration, AnimationCurve curve) && {
  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);
  bool hov = isHovered(rawId);
  bool prs = isPressed(rawId);

  glm::vec2 target = state.resolve(hov, prs);
  return std::move(*this).animateTranslate(target, duration, curve);
}

Modifier Modifier::animateOpacity(float targetOpacity, float duration,
                                  AnimationCurve curve) && {
  auto *uiState = getUiState();
  if (!uiState)
    return std::move(*this).opacity(targetOpacity);

  std::string label = m_style.elementLabel.value_or("AnonModifier");
  uint32_t rawId = hashLabel(label);

  float animatedAlpha = uiState->motionManager.animate<float>(
      motion::MotionHandle(rawId, "opacity"), targetOpacity, duration, curve);

  return std::move(*this).opacity(animatedAlpha);
}

} // namespace atomic
