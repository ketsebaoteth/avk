#include "avk/atomic_ui.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/lucide-icons.generated.h"
#include <unordered_map>

namespace {

std::string codepointToUtf8(char32_t codepoint) {
  std::string out;
  if (codepoint <= 0x7F) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return out;
}

} // namespace

namespace atomic {

/**
 * @brief Renders a vector Lucide icon with cascading style inheritance.
 */
Interaction Icon(LucideIcon icon, Modifier &&modifier) {
  static std::unordered_map<LucideIcon, std::string> iconStringCache;

  if (iconStringCache.find(icon) == iconStringCache.end()) {
    const auto codepoint = static_cast<char32_t>(icon);
    iconStringCache[icon] = codepointToUtf8(codepoint);
  }

  const std::string &iconString = iconStringCache[icon];
  const auto &style = modifier.getStyle();
  const float requestedSize =
      style.height.value_or(style.width.value_or(16.0f));
  const uint32_t closestFontId = getClosestIconFontId(requestedSize);

  return Text(iconString, closestFontId,
              std::move(modifier).size(requestedSize, requestedSize));
}

} // namespace atomic
