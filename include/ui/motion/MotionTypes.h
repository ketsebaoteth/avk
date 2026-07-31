#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>

namespace atomic::motion {

/**
 * @brief Stable 32-bit identifier for animation targets and control bindings.
 * Replaces dynamic string allocations and fragile layout index lookups in
 * immediate-mode loops.
 */
struct MotionHandle {
  uint32_t id{0};

  constexpr MotionHandle() noexcept = default;
  constexpr explicit MotionHandle(uint32_t rawId) noexcept : id(rawId) {}

  /**
   * @brief Generates a stable composite handle for an element ID + property
   * name. Completely eliminates manual magic hex offset additions (+ 0x100).
   */
  constexpr MotionHandle(uint32_t elementId,
                         std::string_view propertyName) noexcept {
    uint32_t hash = elementId;
    for (char c : propertyName) {
      hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u; // FNV-1a
    }
    id = hash;
  }

  [[nodiscard]] constexpr bool isValid() const noexcept { return id != 0; }
  [[nodiscard]] constexpr uint32_t value() const noexcept { return id; }

  static constexpr MotionHandle Invalid() noexcept { return MotionHandle{0}; }

  constexpr bool operator==(const MotionHandle &) const noexcept = default;
  constexpr auto operator<=>(const MotionHandle &) const noexcept = default;
};

static_assert(sizeof(MotionHandle) == sizeof(uint32_t),
              "MotionHandle must retain a strict 32-bit footprint.");

/**
 * @brief Runtime execution state of a tween or timeline instance.
 * Packed into a single byte for vector and cache efficiency.
 */
enum class PlayState : uint8_t { Stopped = 0, Running, Paused, Completed };

/**
 * @brief Defines playback repetition behavior upon reaching target completion.
 */
enum class LoopMode : uint8_t { Once = 0, Infinite, PingPong };

/**
 * @brief Concept defining types that support continuous scalar interpolation.
 * Validates arithmetic compatibility at compile time without virtual dispatch.
 */
template <typename T>
concept Interpolatable = requires(T a, T b, float t) {
  { a + (b - a) * t } -> std::convertible_to<T>;
} || requires(T a, T b, float t) {
  { a * (1.0f - t) + b * t } -> std::convertible_to<T>;
};

/**
 * @brief Bit-packed modifier flags for fine-grained runtime behavior.
 */
struct MotionFlags {
  bool autoreverse : 1 {false};
  bool useUnscaledTime : 1 {false};
  uint8_t reserved : 6 {0};

  constexpr bool operator==(const MotionFlags &) const noexcept = default;
};

static_assert(sizeof(MotionFlags) == sizeof(uint8_t),
              "MotionFlags must pack strictly into 1 byte.");

} // namespace atomic::motion

/**
 * @brief Standard library hash specialization for MotionHandle keys.
 * Uses explicit global scope resolution ::atomic::motion::MotionHandle.
 */
template <> struct std::hash<::atomic::motion::MotionHandle> {
  [[nodiscard]] size_t
  operator()(::atomic::motion::MotionHandle handle) const noexcept {
    return std::hash<uint32_t>{}(handle.id);
  }
};
