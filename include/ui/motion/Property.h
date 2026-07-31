#pragma once

#include "MotionTypes.h"
#include <concepts>
#include <type_traits>

namespace atomic::motion {

/**
 * @brief Performs type-safe interpolation between start and end using
 * normalized time t. Automatically selects subtractive vs additive linear
 * combinations based on type capabilities.
 */
template <typename T>
  requires Interpolatable<T>
[[nodiscard]] constexpr T lerp(const T &start, const T &end, float t) noexcept {
  if constexpr (requires { start + (end - start) * t; }) {
    return start + (end - start) * t;
  } else {
    return start * (1.0f - t) + end * t;
  }
}

/**
 * @brief Lightweight, non-owning reference to an interpolatable property target
 * in memory. Enforces zero-heap allocation and direct memory updates during
 * high-frequency tick loops.
 */
template <typename T>
  requires Interpolatable<T>
class PropertyRef {
public:
  constexpr PropertyRef() noexcept = default;
  constexpr explicit PropertyRef(T *targetPtr) noexcept : ptr(targetPtr) {}

  /**
   * @brief Checks if the property reference points to valid target memory.
   */
  [[nodiscard]] constexpr bool isValid() const noexcept {
    return ptr != nullptr;
  }

  /**
   * @brief Explicit boolean check operator for valid memory binding.
   */
  constexpr explicit operator bool() const noexcept { return ptr != nullptr; }

  /**
   * @brief Retrieves the current value at target memory.
   */
  [[nodiscard]] constexpr T get() const noexcept { return ptr ? *ptr : T{}; }

  /**
   * @brief Writes a new value directly to target memory.
   */
  constexpr void set(const T &value) const noexcept {
    if (ptr) {
      *ptr = value;
    }
  }

  /**
   * @brief Evaluates lerp(start, end, t) and writes the result to target
   * memory.
   */
  constexpr void applyInterpolation(const T &start, const T &end,
                                    float t) const noexcept {
    if (ptr) {
      *ptr = atomic::motion::lerp(start, end, t);
    }
  }

  /**
   * @brief Returns the underlying raw memory pointer.
   */
  [[nodiscard]] constexpr T *raw() const noexcept { return ptr; }

private:
  T *ptr{nullptr};
};

/**
 * @brief Function-pointer accessor binding for objects exposing getter/setter
 * methods.
 */
template <typename T, typename Context>
  requires Interpolatable<T>
struct PropertyAccessor {
  Context *instance{nullptr};
  T (*getter)(const Context *){nullptr};
  void (*setter)(Context *, const T &){nullptr};

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return instance && getter && setter;
  }

  [[nodiscard]] constexpr T get() const noexcept {
    return isValid() ? getter(instance) : T{};
  }

  constexpr void set(const T &value) const noexcept {
    if (isValid()) {
      setter(instance, value);
    }
  }

  constexpr void applyInterpolation(const T &start, const T &end,
                                    float t) const noexcept {
    if (isValid()) {
      setter(instance, atomic::motion::lerp(start, end, t));
    }
  }
};

/**
 * @brief Helper factory function to cleanly deduce types when creating property
 * references.
 */
template <typename T>
  requires Interpolatable<T>
[[nodiscard]] constexpr PropertyRef<T> makeProperty(T &target) noexcept {
  return PropertyRef<T>(&target);
}

} // namespace atomic::motion
