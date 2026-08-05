#include "ui/style/themeManager.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string_view>

namespace atomic {

namespace detail_parser {

static void skipWhitespaceAndComments(std::string_view &sv) {
  while (!sv.empty()) {
    if (std::isspace(static_cast<unsigned char>(sv.front()))) {
      sv.remove_prefix(1);
      continue;
    }
    if (sv.starts_with("//")) {
      auto pos = sv.find('\n');
      if (pos == std::string_view::npos)
        sv = {};
      else
        sv.remove_prefix(pos + 1);
      continue;
    }
    if (sv.starts_with("/*")) {
      auto pos = sv.find("*/");
      if (pos == std::string_view::npos)
        sv = {};
      else
        sv.remove_prefix(pos + 2);
      continue;
    }
    break;
  }
}

static std::string_view trim(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
    sv.remove_prefix(1);
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
    sv.remove_suffix(1);
  return sv;
}

static AtomicColor parseHexColor(std::string_view sv) {
  if (sv.starts_with('#'))
    sv.remove_prefix(1);

  if (sv.length() == 3 || sv.length() == 4) {
    int r = detail_color::charToHex(sv[0]);
    int g = detail_color::charToHex(sv[1]);
    int b = detail_color::charToHex(sv[2]);
    int a = (sv.length() == 4) ? detail_color::charToHex(sv[3]) : 15;

    return detail_color::srgbColor(static_cast<float>((r << 4) | r),
                                   static_cast<float>((g << 4) | g),
                                   static_cast<float>((b << 4) | b),
                                   static_cast<float>((a << 4) | a) / 255.0f);
  }

  if (sv.length() == 6 || sv.length() == 8) {
    int r =
        (detail_color::charToHex(sv[0]) << 4) | detail_color::charToHex(sv[1]);
    int g =
        (detail_color::charToHex(sv[2]) << 4) | detail_color::charToHex(sv[3]);
    int b =
        (detail_color::charToHex(sv[4]) << 4) | detail_color::charToHex(sv[5]);
    int a = (sv.length() == 8) ? ((detail_color::charToHex(sv[6]) << 4) |
                                  detail_color::charToHex(sv[7]))
                               : 255;

    return detail_color::srgbColor(static_cast<float>(r), static_cast<float>(g),
                                   static_cast<float>(b),
                                   static_cast<float>(a) / 255.0f);
  }

  return AtomicColor(0.0f, 0.0f, 0.0f, 1.0f);
}

static AtomicColor parseRgbaColor(std::string_view sv) {
  auto openParen = sv.find('(');
  auto closeParen = sv.find(')');
  if (openParen == std::string_view::npos ||
      closeParen == std::string_view::npos || closeParen <= openParen) {
    return AtomicColor(0.0f, 0.0f, 0.0f, 1.0f);
  }

  std::string_view inner = sv.substr(openParen + 1, closeParen - openParen - 1);

  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
  int compIndex = 0;

  while (!inner.empty() && compIndex < 4) {
    while (!inner.empty() &&
           (std::isspace(static_cast<unsigned char>(inner.front())) ||
            inner.front() == ',')) {
      inner.remove_prefix(1);
    }
    if (inner.empty())
      break;

    float val = 0.0f;
    auto [ptr, ec] =
        std::from_chars(inner.data(), inner.data() + inner.size(), val);
    if (ec == std::errc{}) {
      if (compIndex == 0)
        r = val;
      else if (compIndex == 1)
        g = val;
      else if (compIndex == 2)
        b = val;
      else if (compIndex == 3)
        a = val;
      compIndex++;

      size_t parsedLen = ptr - inner.data();
      inner.remove_prefix(parsedLen);
    } else {
      break;
    }
  }

  return detail_color::srgbColor(r, g, b, a);
}

static DenseThemeVariable parseValueToVariable(const std::string &varName,
                                               std::string_view valSv) {
  DenseThemeVariable var;
  var.hasValue = true;
  var.name = varName;

  if (valSv == "true") {
    var.type = ThemeVariableType::Boolean;
    var.value = true;
    return var;
  }
  if (valSv == "false") {
    var.type = ThemeVariableType::Boolean;
    var.value = false;
    return var;
  }

  if (valSv.starts_with('#')) {
    var.type = ThemeVariableType::Color;
    AtomicColor col = parseHexColor(valSv);
    var.value = static_cast<glm::vec4>(col);
    return var;
  }

  if (valSv.starts_with("rgba(") || valSv.starts_with("rgb(")) {
    var.type = ThemeVariableType::Color;
    AtomicColor col = parseRgbaColor(valSv);
    var.value = static_cast<glm::vec4>(col);
    return var;
  }

  std::string_view numSv = valSv;
  bool hadPxUnit = false;

  if (numSv.ends_with("px") || numSv.ends_with("em")) {
    hadPxUnit = true;
    numSv.remove_suffix(2);
  } else if (numSv.ends_with("rem") || numSv.ends_with("deg")) {
    hadPxUnit = true;
    numSv.remove_suffix(3);
  } else if (numSv.ends_with('%')) {
    numSv.remove_suffix(1);
  }
  numSv = trim(numSv);

  bool hasDecimal = (numSv.find('.') != std::string_view::npos);

  if (hasDecimal || hadPxUnit) {
    float floatVal = 0.0f;
    std::from_chars(numSv.data(), numSv.data() + numSv.size(), floatVal);
    var.type = ThemeVariableType::Float;
    var.value = floatVal;
  } else {
    int32_t intVal = 0;
    auto [ptr, ec] =
        std::from_chars(numSv.data(), numSv.data() + numSv.size(), intVal);
    if (ec == std::errc{}) {
      var.type = ThemeVariableType::Integer;
      var.value = intVal;
    } else {
      float floatVal = 0.0f;
      std::from_chars(numSv.data(), numSv.data() + numSv.size(), floatVal);
      var.type = ThemeVariableType::Float;
      var.value = floatVal;
    }
  }

  return var;
}

} // namespace detail_parser

ThemeManager::ThemeManager() : m_activeThemeName(":root") {
  registerBuiltInTokens();
  m_themes[":root"].name = ":root";
  m_rootThemePtr = &m_themes[":root"];
  m_activeThemePtr = m_rootThemePtr;
}

void ThemeManager::registerBuiltInTokens() {
  auto registerToken = [this](ThemeVarId id, const std::string &name) {
    uint32_t idx = static_cast<uint32_t>(id);
    m_nameToId[name] = idx;
    if (m_idToName.size() <= idx) {
      m_idToName.resize(idx + 1);
    }
    m_idToName[idx] = name;
  };

  registerToken(ThemeVarId::ScaleMultiplier, "--scale-multiplier");
  registerToken(ThemeVarId::SpacingMultiplier, "--spacing-multiplier");
  registerToken(ThemeVarId::FontSizeMultiplier, "--font-size-multiplier");
  registerToken(ThemeVarId::BorderRadiusMultiplier,
                "--border-radius-multiplier");
  registerToken(ThemeVarId::BorderWidthMultiplier, "--border-width-multiplier");

  registerToken(ThemeVarId::ColorBgApp, "--color-bg-app");
  registerToken(ThemeVarId::ColorBgSurface, "--color-bg-surface");
  registerToken(ThemeVarId::ColorBgSurfaceHover, "--color-bg-surface-hover");
  registerToken(ThemeVarId::ColorBgSurfaceActive, "--color-bg-surface-active");
  registerToken(ThemeVarId::ColorBgSurfaceDisabled,
                "--color-bg-surface-disabled");

  registerToken(ThemeVarId::ColorPrimary, "--color-primary");
  registerToken(ThemeVarId::ColorPrimaryHover, "--color-primary-hover");
  registerToken(ThemeVarId::ColorPrimaryActive, "--color-primary-active");
  registerToken(ThemeVarId::ColorAccent, "--color-accent");
  registerToken(ThemeVarId::ColorTextPrimary, "--color-text-primary");
  registerToken(ThemeVarId::ColorTextSecondary, "--color-text-secondary");
  registerToken(ThemeVarId::ColorTextTertiary, "--color-text-tertiary");
  registerToken(ThemeVarId::ColorTextDisabled, "--color-text-disabled");

  registerToken(ThemeVarId::ColorBorderNormal, "--color-border-normal");
  registerToken(ThemeVarId::ColorBorderHover, "--color-border-hover");
  registerToken(ThemeVarId::ColorBorderFocus, "--color-border-focus");

  registerToken(ThemeVarId::ButtonPadX, "--button-pad-x");
  registerToken(ThemeVarId::ButtonPadY, "--button-pad-y");
  registerToken(ThemeVarId::ButtonFontSize, "--button-font-size");
  registerToken(ThemeVarId::ButtonFontWeight, "--button-font-weight");
  registerToken(ThemeVarId::TextInputHeight, "--text-input-height");
  registerToken(ThemeVarId::TextInputPadX, "--text-input-pad-x");
  registerToken(ThemeVarId::TextInputPadY, "--text-input-pad-y");
  registerToken(ThemeVarId::BorderRadiusLg, "--border-radius-lg");
  registerToken(ThemeVarId::BorderRadiusXl, "--border-radius-xl");
  registerToken(ThemeVarId::BorderWidthThin, "--border-width-thin");
  registerToken(ThemeVarId::BorderWidthNone, "--border-width-none");
  registerToken(ThemeVarId::ColorTextInverse, "--color-text-inverse");
  registerToken(ThemeVarId::TransitionDurationNormal,
                "--transition-duration-normal");

  // Code Theme Tokens
  registerToken(ThemeVarId::CodeBg, "--code-bg");
  registerToken(ThemeVarId::CodeTitlebarBg, "--code-titlebar-bg");
  registerToken(ThemeVarId::CodeBorder, "--code-border");
  registerToken(ThemeVarId::CodeText, "--code-text");
  registerToken(ThemeVarId::CodeKeyword, "--code-keyword");
  registerToken(ThemeVarId::CodeType, "--code-type");
  registerToken(ThemeVarId::CodeString, "--code-string");
  registerToken(ThemeVarId::CodeComment, "--code-comment");
  registerToken(ThemeVarId::CodeNumber, "--code-number");
  registerToken(ThemeVarId::CodeSymbol, "--code-symbol");

  registerToken(ThemeVarId::UiAnimations, "--ui-animations");
  registerToken(ThemeVarId::DevtoolsDebug, "--devtools-debug");
}

uint32_t ThemeManager::getOrRegisterVariableId(const std::string &varName) {
  auto it = m_nameToId.find(varName);
  if (it != m_nameToId.end()) {
    return it->second;
  }

  uint32_t newId = static_cast<uint32_t>(m_idToName.size());
  m_nameToId[varName] = newId;
  m_idToName.push_back(varName);
  return newId;
}

std::string ThemeManager::getVariableName(uint32_t varId) const {
  if (varId < m_idToName.size()) {
    return m_idToName[varId];
  }
  return "";
}

bool ThemeManager::loadThemesFromCss(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    return false;

  auto fileSize = file.tellg();
  if (fileSize <= 0)
    return true;

  std::string content(static_cast<size_t>(fileSize), '\0');
  file.seekg(0);
  file.read(content.data(), fileSize);

  std::string_view sv = content;

  while (!sv.empty()) {
    detail_parser::skipWhitespaceAndComments(sv);
    if (sv.empty())
      break;

    auto openBrace = sv.find('{');
    if (openBrace == std::string_view::npos)
      break;

    std::string_view rawSelector = detail_parser::trim(sv.substr(0, openBrace));
    sv.remove_prefix(openBrace + 1);

    std::string themeName(rawSelector);
    if (themeName != ":root") {
      if (themeName.starts_with('.'))
        themeName = themeName.substr(1);
      if (themeName.starts_with(':'))
        themeName = themeName.substr(1);
    }

    auto &theme = m_themes[themeName];
    theme.name = themeName;

    while (!sv.empty()) {
      detail_parser::skipWhitespaceAndComments(sv);
      if (sv.empty())
        break;

      if (sv.front() == '}') {
        sv.remove_prefix(1);
        break;
      }

      auto semiPos = sv.find(';');
      auto closeBracePos = sv.find('}');

      size_t declEnd = std::min(semiPos, closeBracePos);
      if (declEnd == std::string_view::npos)
        break;

      std::string_view decl = detail_parser::trim(sv.substr(0, declEnd));

      if (semiPos != std::string_view::npos && semiPos <= declEnd) {
        sv.remove_prefix(semiPos + 1);
      } else if (closeBracePos != std::string_view::npos &&
                 closeBracePos == declEnd) {
        sv.remove_prefix(closeBracePos);
      }

      if (decl.empty())
        continue;

      auto colonPos = decl.find(':');
      if (colonPos == std::string_view::npos)
        continue;

      std::string_view varNameSv =
          detail_parser::trim(decl.substr(0, colonPos));
      std::string_view valSv = detail_parser::trim(decl.substr(colonPos + 1));

      if (varNameSv.empty() || valSv.empty())
        continue;

      std::string varName(varNameSv);
      uint32_t varId = getOrRegisterVariableId(varName);

      if (theme.variables.size() <= varId) {
        theme.variables.resize(varId + 1);
      }

      DenseThemeVariable var =
          detail_parser::parseValueToVariable(varName, valSv);
      theme.variables[varId] = std::move(var);
    }
  }

  switchTheme(m_activeThemeName);
  return true;
}

void ThemeManager::switchTheme(const std::string &themeName) {
  m_activeThemeName = themeName;
  auto it = m_themes.find(m_activeThemeName);
  if (it != m_themes.end()) {
    m_activeThemePtr = &it->second;
  } else {
    m_activeThemePtr = m_rootThemePtr;
  }

  auto rootIt = m_themes.find(":root");
  if (rootIt != m_themes.end()) {
    m_rootThemePtr = &rootIt->second;
  }
}

void ThemeManager::setThemeVariableValue(
    const std::string &themeName, uint32_t varId,
    const std::variant<glm::vec4, float, bool, int32_t> &newValue) {
  auto &theme = m_themes[themeName];
  theme.name = themeName;

  if (theme.variables.size() <= varId) {
    theme.variables.resize(varId + 1);
  }

  DenseThemeVariable var;
  var.hasValue = true;
  var.name = getVariableName(varId);
  var.value = newValue;

  if (std::holds_alternative<glm::vec4>(newValue)) {
    var.type = ThemeVariableType::Color;
  } else if (std::holds_alternative<float>(newValue)) {
    var.type = ThemeVariableType::Float;
  } else if (std::holds_alternative<bool>(newValue)) {
    var.type = ThemeVariableType::Boolean;
  } else if (std::holds_alternative<int32_t>(newValue)) {
    var.type = ThemeVariableType::Integer;
  }

  theme.variables[varId] = std::move(var);

  switchTheme(m_activeThemeName);
}

void ThemeManager::setThemeVariableFromString(const std::string &themeName,
                                              uint32_t varId,
                                              ThemeVariableType type,
                                              const std::string &valueStr) {
  std::string varName = getVariableName(varId);
  DenseThemeVariable parsedVar =
      detail_parser::parseValueToVariable(varName, valueStr);
  parsedVar.type = type;
  setThemeVariableValue(themeName, varId, parsedVar.value);
}

bool ThemeManager::inheritPropertyFromRoot(const std::string &subThemeName,
                                           uint32_t varId) {
  if (!m_rootThemePtr || varId >= m_rootThemePtr->variables.size() ||
      !m_rootThemePtr->variables[varId].hasValue) {
    return false;
  }

  auto &subTheme = m_themes[subThemeName];
  subTheme.name = subThemeName;
  if (subTheme.variables.size() <= varId) {
    subTheme.variables.resize(varId + 1);
  }
  subTheme.variables[varId] = m_rootThemePtr->variables[varId];

  switchTheme(m_activeThemeName);
  return true;
}

bool ThemeManager::writeThemeToFile(const std::string &themeName,
                                    const std::string &filepath) {
  auto it = m_themes.find(themeName);
  if (it == m_themes.end())
    return false;

  std::ofstream file(filepath);
  if (!file.is_open())
    return false;

  file << (themeName == ":root" ? ":root" : "." + themeName) << " {\n";
  for (size_t id = 0; id < it->second.variables.size(); ++id) {
    const auto &var = it->second.variables[id];
    if (!var.hasValue)
      continue;

    std::string varName = var.name.empty()
                              ? getVariableName(static_cast<uint32_t>(id))
                              : var.name;
    file << "  " << varName << ": ";

    switch (var.type) {
    case ThemeVariableType::Color: {
      auto col = std::get<glm::vec4>(var.value);
      file << "rgba(" << static_cast<int>(col.r * 255.0f) << ", "
           << static_cast<int>(col.g * 255.0f) << ", "
           << static_cast<int>(col.b * 255.0f) << ", " << col.a << ");\n";
      break;
    }
    case ThemeVariableType::Float: {
      auto val = std::get<float>(var.value);
      file << val << "px;\n";
      break;
    }
    case ThemeVariableType::Boolean: {
      auto val = std::get<bool>(var.value);
      file << (val ? "true" : "false") << ";\n";
      break;
    }
    case ThemeVariableType::Integer: {
      auto val = std::get<int32_t>(var.value);
      file << val << ";\n";
      break;
    }
    }
  }
  file << "}\n";
  return true;
}

} // namespace atomic
