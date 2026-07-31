#pragma once

#include "Curves.h"
#include "MotionManager.h"
#include "MotionTypes.h"
#include "Property.h"
#include "Timeline.h"
#include "Tween.h"

namespace atomic::motion {

/**
 * @brief Convenience inline helper to animate a scalar float target in
 * immediate-mode UI contexts.
 */
inline float
animateFloat(MotionManager &mgr, MotionHandle handle, float target,
             float duration = 0.2f,
             const AnimationCurve &curve = AnimationCurve::EaseOut()) {
  return mgr.animate<float>(handle, target, duration, curve);
}

/**
 * @brief Convenience inline helper to animate a 4D vector or color target in
 * immediate-mode UI contexts.
 */
template <typename T>
  requires Interpolatable<T>
inline T
animateVector(MotionManager &mgr, MotionHandle handle, const T &target,
              float duration = 0.2f,
              const AnimationCurve &curve = AnimationCurve::EaseOut()) {
  return mgr.animate<T>(handle, target, duration, curve);
}

} // namespace atomic::motion

/**
 * @brief Unifies atomic::AnimationCurve and atomic::SpringConfig with the
 * atomic::motion subsystem.
 */
namespace atomic {
using AnimationCurve = motion::AnimationCurve;
using SpringConfig = motion::SpringConfig;
} // namespace atomic
