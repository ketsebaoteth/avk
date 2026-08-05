#pragma once

#include <cstdint>
#include <expected>
#include <glm/glm.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ui/utils/color.h"

namespace atomic {

// Built-In Core Library Tokens (Hot Path Enum Keys)
enum class ThemeVarId : uint32_t {
  // Scalers & Multipliers
  ScaleMultiplier = 0,
  SpacingMultiplier,
  FontSizeMultiplier,
  BorderRadiusMultiplier,
  BorderWidthMultiplier,
  BorderWidthNone,

  // Surface & Background Colors
  ColorBgApp,
  ColorBgSurface,
  ColorBgSurfaceHover,
  ColorBgSurfaceActive,
  ColorBgSurfaceDisabled,

  // Primary & Text Colors
  ColorPrimary,
  ColorPrimaryHover,
  ColorPrimaryActive,
  ColorAccent,
  ColorTextPrimary,
  ColorTextSecondary,
  ColorTextTertiary,
  ColorTextInverse,
  ColorTextDisabled,

  // Borders
  ColorBorderNormal,
  ColorBorderHover,
  ColorBorderFocus,

  // Component Specific Defaults
  ButtonPadX,
  ButtonPadY,
  ButtonFontSize,
  ButtonFontWeight,
  TextInputHeight,
  TextInputPadX,
  TextInputPadY,
  BorderRadiusLg,
  BorderRadiusXl,
  BorderWidthThin,
  TransitionDurationNormal,

  // Code Theme Tokens
  CodeBg,
  CodeTitlebarBg,
  CodeBorder,
  CodeText,
  CodeKeyword,
  CodeType,
  CodeString,
  CodeComment,
  CodeNumber,
  CodeSymbol,

  // System Flags
  UiAnimations,
  DevtoolsDebug,

  BuiltInCount
};

// Lightweight 32-bit Token Handle
struct ThemeVarToken {
  uint32_t id = 0;

  constexpr ThemeVarToken() = default;
  constexpr explicit ThemeVarToken(uint32_t rawId) : id(rawId) {}
  constexpr ThemeVarToken(ThemeVarId builtInId)
      : id(static_cast<uint32_t>(builtInId)) {}
  constexpr operator uint32_t() const { return id; }
};

template <typename EnumT> struct ThemeEnumRegistry {
  static inline uint32_t baseOffset = 0;
  static inline bool registered = false;
};

enum class ThemeVariableType { Color, Float, Boolean, Integer };

struct DenseThemeVariable {
  bool hasValue = false;
  std::string name;
  ThemeVariableType type = ThemeVariableType::Float;
  std::variant<glm::vec4, float, bool, int32_t> value;
};

struct Theme {
  std::string name;
  std::vector<DenseThemeVariable> variables;
};

class ThemeManager {
public:
  static ThemeManager &getInstance() {
    static ThemeManager instance;
    return instance;
  }

  ThemeManager(const ThemeManager &) = delete;
  ThemeManager &operator=(const ThemeManager &) = delete;

  bool loadThemesFromCss(const std::string &filepath);
  void switchTheme(const std::string &themeName);

  // -----------------------------------------------------------------
  // ⚡ HOT PATH ACCESSORS (O(1) Direct Vector Offset)
  // -----------------------------------------------------------------
  template <typename T>
  inline T getVariable(ThemeVarToken token, const T &defaultValue) const {
    uint32_t id = token.id;

    if (m_activeThemePtr && id < m_activeThemePtr->variables.size()) {
      const auto &var = m_activeThemePtr->variables[id];
      if (var.hasValue) {
        if (auto valPtr = std::get_if<T>(&var.value))
          return *valPtr;
      }
    }

    if (m_rootThemePtr && m_rootThemePtr != m_activeThemePtr &&
        id < m_rootThemePtr->variables.size()) {
      const auto &var = m_rootThemePtr->variables[id];
      if (var.hasValue) {
        if (auto valPtr = std::get_if<T>(&var.value))
          return *valPtr;
      }
    }

    return defaultValue;
  }

  template <typename T>
  inline T getVariable(ThemeVarId builtInId, const T &defaultValue) const {
    return getVariable<T>(ThemeVarToken(builtInId), defaultValue);
  }

  template <typename T, typename EnumT>
    requires std::is_enum_v<EnumT> && (!std::is_same_v<EnumT, ThemeVarId>)
  inline T getVariable(EnumT userEnumVal, const T &defaultValue) const {
    uint32_t id = static_cast<uint32_t>(userEnumVal) +
                  ThemeEnumRegistry<EnumT>::baseOffset;
    return getVariable<T>(ThemeVarToken(id), defaultValue);
  }

  template <typename T>
  T getVariable(const std::string &varName, const T &defaultValue) {
    uint32_t id = getOrRegisterVariableId(varName);
    return getVariable<T>(ThemeVarToken(id), defaultValue);
  }

  ThemeVarToken registerToken(const std::string &varName) {
    return ThemeVarToken(getOrRegisterVariableId(varName));
  }

  template <typename EnumT>
    requires std::is_enum_v<EnumT>
  uint32_t registerEnumBlock(
      const std::vector<std::pair<EnumT, std::string>> &mappings) {
    for (const auto &[enumVal, name] : mappings) {
      uint32_t id = getOrRegisterVariableId(name);
      if (!ThemeEnumRegistry<EnumT>::registered) {
        ThemeEnumRegistry<EnumT>::baseOffset =
            id - static_cast<uint32_t>(enumVal);
        ThemeEnumRegistry<EnumT>::registered = true;
      }
    }
    return ThemeEnumRegistry<EnumT>::baseOffset;
  }

  template <typename T>
  std::expected<T, std::string> inspectVariable(const std::string &themeName,
                                                uint32_t varId) const {
    auto themeIt = m_themes.find(themeName);
    if (themeIt == m_themes.end()) {
      return std::unexpected("Theme block context target not found.");
    }

    if (varId >= themeIt->second.variables.size() ||
        !themeIt->second.variables[varId].hasValue) {
      return std::unexpected(
          "Variable token not configured in this theme block.");
    }

    const auto &var = themeIt->second.variables[varId];
    if (auto valPtr = std::get_if<T>(&var.value)) {
      return *valPtr;
    }

    return std::unexpected(
        "Type variant cast mismatch occurred during state extraction.");
  }

  void setThemeVariableValue(
      const std::string &themeName, uint32_t varId,
      const std::variant<glm::vec4, float, bool, int32_t> &newValue);

  void setThemeVariableFromString(const std::string &themeName, uint32_t varId,
                                  ThemeVariableType type,
                                  const std::string &valueStr);

  bool inheritPropertyFromRoot(const std::string &subThemeName, uint32_t varId);
  bool writeThemeToFile(const std::string &themeName,
                        const std::string &filepath);

  uint32_t getOrRegisterVariableId(const std::string &varName);
  std::string getVariableName(uint32_t varId) const;

  const std::unordered_map<std::string, Theme> &getAllThemes() const {
    return m_themes;
  }
  const std::string &getActiveThemeName() const { return m_activeThemeName; }
  size_t getTotalRegisteredVariablesCount() const { return m_idToName.size(); }

private:
  ThemeManager();
  void registerBuiltInTokens();

  std::unordered_map<std::string, Theme> m_themes;
  std::string m_activeThemeName;

  const Theme *m_activeThemePtr = nullptr;
  const Theme *m_rootThemePtr = nullptr;

  std::unordered_map<std::string, uint32_t> m_nameToId;
  std::vector<std::string> m_idToName;
};

template <>
inline AtomicColor
ThemeManager::getVariable<AtomicColor>(ThemeVarToken token,
                                       const AtomicColor &defaultValue) const {
  glm::vec4 defaultVec = defaultValue;
  glm::vec4 resultVec = getVariable<glm::vec4>(token, defaultVec);
  return AtomicColor(resultVec.r, resultVec.g, resultVec.b, resultVec.a);
}

} // namespace atomic
