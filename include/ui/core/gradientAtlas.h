#pragma once
#include "avk/avk_core.h"
#include "avk/avk_texture.h"
#include "glm/glm.hpp"
#include "ui/style/style.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace atomic {

// OKLab Perceptual Mixing Helper for CPU Baking
inline glm::vec3 srgbToOklab(glm::vec3 c) {
  glm::mat3 m1 = glm::mat3(0.4122214708f, 0.5363325363f, 0.0514459929f,
                           0.2119034982f, 0.6806995451f, 0.1073969566f,
                           0.0883024619f, 0.2817188376f, 0.6299787005f);
  glm::vec3 lms = m1 * c;
  lms = glm::pow(glm::max(lms, glm::vec3(0.0f)), glm::vec3(1.0f / 3.0f));
  glm::mat3 m2 = glm::mat3(0.2104542553f, 0.7936177850f, -0.0040720468f,
                           1.9779984951f, -2.4285922050f, 0.4505937099f,
                           0.0259040371f, 0.7827717662f, -0.8086757660f);
  return m2 * lms;
}

inline glm::vec3 oklabToSrgb(glm::vec3 c) {
  glm::mat3 m1 =
      glm::mat3(1.0f, 0.3963377774f, 0.2158037573f, 1.0f, -0.1055613458f,
                -0.0638541728f, 1.0f, -0.0894841775f, -1.2914855480f);
  glm::vec3 lms = m1 * c;
  lms = lms * lms * lms;
  glm::mat3 m2 = glm::mat3(4.0767416621f, -3.3077115913f, 0.2309699292f,
                           -1.2684380046f, 2.6097574011f, -0.3413193965f,
                           -0.0041960863f, -0.7034186147f, 1.7076147010f);
  return m2 * lms;
}

/**
 * @brief Interpolates across multi-stop gradients (supports 2, 7, 20+ stops) in
 * OKLab space.
 */
inline glm::vec4 sampleMultiStopGradient(const std::vector<GradientStop> &stops,
                                         float t) {
  if (stops.empty())
    return glm::vec4(1.0f);
  if (stops.size() == 1 || t <= stops.front().position)
    return stops.front().color;
  if (t >= stops.back().position)
    return stops.back().color;

  // Find surrounding stops
  for (size_t i = 0; i < stops.size() - 1; ++i) {
    if (t >= stops[i].position && t <= stops[i + 1].position) {
      float range = stops[i + 1].position - stops[i].position;
      float localT = (range > 0.0001f) ? (t - stops[i].position) / range : 0.0f;

      const glm::vec4 &c1 = stops[i].color;
      const glm::vec4 &c2 = stops[i + 1].color;

      // OKLab Perceptual Color Mixing
      glm::vec3 lab1 = srgbToOklab(glm::vec3(c1));
      glm::vec3 lab2 = srgbToOklab(glm::vec3(c2));
      glm::vec3 labMix = glm::mix(lab1, lab2, localT);
      glm::vec3 srgbMix = oklabToSrgb(labMix);

      float alphaMix = glm::mix(c1.a, c2.a, localT);
      return glm::vec4(srgbMix, alphaMix);
    }
  }
  return stops.back().color;
}

/**
 * @brief Bakes and caches multi-stop gradient textures for Vulkan rendering.
 */
class GradientAtlasManager {
public:
  static GradientAtlasManager &instance() {
    static GradientAtlasManager mgr;
    return mgr;
  }

  /**
   * @brief Bakes a multi-stop gradient into a 256x1 1D Vulkan Texture and
   * returns its textureIndex.
   */
  uint32_t getOrCreateGradientTexture(avk::VulkanContext *context,
                                      const Gradient &grad) {
    if (!context || grad.stops.empty())
      return 0;

    // Generate cache key
    std::string key;
    for (const auto &s : grad.stops) {
      key += std::to_string(s.color.r) + "," + std::to_string(s.color.g) + "," +
             std::to_string(s.color.b) + "," + std::to_string(s.color.a) + "@" +
             std::to_string(s.position) + "|";
    }

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      return it->second;
    }

    // Bake 256x1 pixel RGBA buffer
    constexpr uint32_t width = 256;
    std::vector<uint8_t> pixels(width * 4);

    for (uint32_t i = 0; i < width; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(width - 1);
      glm::vec4 color = sampleMultiStopGradient(grad.stops, t);

      pixels[i * 4 + 0] =
          static_cast<uint8_t>(glm::clamp(color.r * 255.0f, 0.0f, 255.0f));
      pixels[i * 4 + 1] =
          static_cast<uint8_t>(glm::clamp(color.g * 255.0f, 0.0f, 255.0f));
      pixels[i * 4 + 2] =
          static_cast<uint8_t>(glm::clamp(color.b * 255.0f, 0.0f, 255.0f));
      pixels[i * 4 + 3] =
          static_cast<uint8_t>(glm::clamp(color.a * 255.0f, 0.0f, 255.0f));
    }

    uint32_t texIdx =
        context->getTextureManager()->loadRawPixels(pixels.data(), width, 1);
    m_cache[key] = texIdx;
    return texIdx;
  }

  void clearCache() { m_cache.clear(); }

private:
  GradientAtlasManager() = default;
  std::unordered_map<std::string, uint32_t> m_cache;
};

} // namespace atomic
