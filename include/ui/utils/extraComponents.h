#pragma once

#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/generated/lucideIcons.generated.h"
#include "ui/internal/context.h"
#include "ui/style/modifier.h"
#include "ui/style/style.h"
#include "ui/utils/color.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atomic::extras {

struct CodeTheme {
  glm::vec4 background = {0.98f, 0.98f, 0.99f,
                          1.0f}; // Clean white/light background
  glm::vec4 titlebarBg = {0.93f, 0.93f, 0.95f, 1.0f}; // Subtle gray titlebar
  glm::vec4 border = {0.85f, 0.85f, 0.88f, 1.0f};     // Soft border
  glm::vec4 text = {0.15f, 0.15f, 0.18f, 1.0f};       // Dark charcoal text
  glm::vec4 keyword = {0.75f, 0.15f, 0.50f, 1.0f};    // Rich magenta/purple
  glm::vec4 typeName = {0.05f, 0.45f, 0.75f, 1.0f};   // Vibrant blue
  glm::vec4 stringLit = {0.10f, 0.55f, 0.20f, 1.0f};  // Deep green
  glm::vec4 comment = {0.45f, 0.48f, 0.52f, 1.0f};    // Muted slate gray
  glm::vec4 number = {0.70f, 0.35f, 0.05f, 1.0f};     // Warm orange/brown
  glm::vec4 symbol = {0.30f, 0.32f, 0.36f, 1.0f};     // Neutral symbol color
};

enum class TokenType {
  Normal,
  Keyword,
  TypeName,
  String,
  Comment,
  Number,
  Symbol
};

struct Token {
  std::string text;
  TokenType type;
};

inline const std::unordered_set<std::string> &
getLanguageKeywords(const std::string &lang) {
  static const std::unordered_set<std::string> cppKeywords = {
      "alignas",
      "alignof",
      "and",
      "and_eq",
      "asm",
      "auto",
      "bitand",
      "bitor",
      "bool",
      "break",
      "case",
      "catch",
      "char",
      "char8_t",
      "char16_t",
      "char32_t",
      "class",
      "compl",
      "concept",
      "const",
      "consteval",
      "constexpr",
      "constinit",
      "continue",
      "co_await",
      "co_return",
      "co_yield",
      "decltype",
      "default",
      "delete",
      "do",
      "double",
      "dynamic_cast",
      "else",
      "enum",
      "explicit",
      "export",
      "extern",
      "false",
      "float",
      "for",
      "friend",
      "goto",
      "if",
      "inline",
      "int",
      "long",
      "mutable",
      "namespace",
      "new",
      "noexcept",
      "not",
      "not_eq",
      "nullptr",
      "operator",
      "or",
      "or_eq",
      "private",
      "protected",
      "public",
      "register",
      "reinterpret_cast",
      "requires",
      "return",
      "short",
      "signed",
      "sizeof",
      "static",
      "static_assert",
      "static_cast",
      "struct",
      "switch",
      "template",
      "this",
      "thread_local",
      "throw",
      "true",
      "try",
      "typedef",
      "typeid",
      "typename",
      "union",
      "unsigned",
      "using",
      "virtual",
      "void",
      "volatile",
      "wchar_t",
      "while",
      "xor",
      "xor_eq",
      "std"};

  static const std::unordered_set<std::string> rustKeywords = {
      "as",     "break",  "const", "continue", "crate",  "else",   "enum",
      "extern", "false",  "fn",    "for",      "if",     "impl",   "in",
      "let",    "loop",   "match", "mod",      "move",   "mut",    "pub",
      "ref",    "return", "self",  "Self",     "static", "struct", "super",
      "trait",  "true",   "type",  "unsafe",   "use",    "where",  "while",
      "async",  "await",  "dyn",   "union"};

  static const std::unordered_set<std::string> pyKeywords = {
      "False",  "None",   "True",    "and",      "as",       "assert", "async",
      "await",  "break",  "class",   "continue", "def",      "del",    "elif",
      "else",   "except", "finally", "for",      "from",     "global", "if",
      "import", "in",     "is",      "lambda",   "nonlocal", "not",    "or",
      "pass",   "raise",  "return",  "try",      "while",    "with",   "yield"};

  static const std::unordered_set<std::string> jsKeywords = {
      "async",    "await",    "break",    "case",      "catch",      "class",
      "const",    "continue", "debugger", "default",   "delete",     "do",
      "else",     "export",   "extends",  "false",     "finally",    "for",
      "function", "if",       "import",   "in",        "instanceof", "let",
      "new",      "null",     "return",   "super",     "switch",     "this",
      "throw",    "true",     "try",      "typeof",    "var",        "void",
      "while",    "with",     "yield",    "interface", "type"};

  static const std::unordered_set<std::string> emptySet = {};

  std::string l = lang;
  std::transform(l.begin(), l.end(), l.begin(), ::tolower);

  if (l == "cpp" || l == "c++" || l == "c")
    return cppKeywords;
  if (l == "rust" || l == "rs")
    return rustKeywords;
  if (l == "python" || l == "py")
    return pyKeywords;
  if (l == "javascript" || l == "js" || l == "typescript" || l == "ts")
    return jsKeywords;
  return emptySet;
}

inline std::vector<Token> tokenizeCode(const std::string &code,
                                       const std::string &language) {
  const auto &keywords = getLanguageKeywords(language);
  std::vector<Token> tokens;
  size_t i = 0;
  size_t n = code.size();

  while (i < n) {
    char c = code[i];

    if (std::isspace(static_cast<unsigned char>(c))) {
      std::string ws;
      while (i < n && std::isspace(static_cast<unsigned char>(code[i]))) {
        ws.push_back(code[i++]);
      }
      tokens.push_back({ws, TokenType::Normal});
      continue;
    }

    std::string lLang = language;
    std::transform(lLang.begin(), lLang.end(), lLang.begin(), ::tolower);
    bool isPython = (lLang == "python" || lLang == "py");

    if ((!isPython && c == '/' && i + 1 < n && code[i + 1] == '/') ||
        (isPython && c == '#')) {
      std::string comment;
      while (i < n && code[i] != '\n') {
        comment.push_back(code[i++]);
      }
      tokens.push_back({comment, TokenType::Comment});
      continue;
    }

    if (!isPython && c == '/' && i + 1 < n && code[i + 1] == '*') {
      std::string comment;
      comment.push_back(code[i++]);
      comment.push_back(code[i++]);
      while (i < n && !(code[i - 1] == '*' && code[i] == '/')) {
        comment.push_back(code[i++]);
      }
      if (i < n)
        comment.push_back(code[i++]);
      tokens.push_back({comment, TokenType::Comment});
      continue;
    }

    if (c == '"' || c == '\'') {
      char quote = c;
      std::string str;
      str.push_back(code[i++]);
      while (i < n && code[i] != quote) {
        if (code[i] == '\\' && i + 1 < n) {
          str.push_back(code[i++]);
        }
        str.push_back(code[i++]);
      }
      if (i < n)
        str.push_back(code[i++]);
      tokens.push_back({str, TokenType::String});
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::string num;
      while (i < n && (std::isalnum(static_cast<unsigned char>(code[i])) ||
                       code[i] == '.')) {
        num.push_back(code[i++]);
      }
      tokens.push_back({num, TokenType::Number});
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::string ident;
      while (i < n && (std::isalnum(static_cast<unsigned char>(code[i])) ||
                       code[i] == '_')) {
        ident.push_back(code[i++]);
      }
      if (keywords.find(ident) != keywords.end()) {
        tokens.push_back({ident, TokenType::Keyword});
      } else if (!ident.empty() &&
                 std::isupper(static_cast<unsigned char>(ident[0]))) {
        tokens.push_back({ident, TokenType::TypeName});
      } else {
        tokens.push_back({ident, TokenType::Normal});
      }
      continue;
    }

    std::string sym;
    sym.push_back(code[i++]);
    tokens.push_back({sym, TokenType::Symbol});
  }

  return tokens;
}

inline glm::vec4 getTokenColor(TokenType type, const CodeTheme &theme) {
  switch (type) {
  case TokenType::Keyword:
    return theme.keyword;
  case TokenType::TypeName:
    return theme.typeName;
  case TokenType::String:
    return theme.stringLit;
  case TokenType::Comment:
    return theme.comment;
  case TokenType::Number:
    return theme.number;
  case TokenType::Symbol:
    return theme.symbol;
  case TokenType::Normal:
  default:
    return theme.text;
  }
}

/**
 * @brief Renders a syntax-highlighted code block component with macOS titlebar
 * & copy action.
 */
inline Interaction CodeBlock(const std::string &code,
                             const std::string &language,
                             Modifier &&modifier = Modifier{}) {
  CodeTheme theme{};
  auto *uiState = getUiState();

  const auto &style = modifier.getStyle();
  std::string baseId =
      style.elementLabel.has_value()
          ? style.elementLabel.value()
          : ("CodeBlock_" + language + "_" + std::to_string(code.length()));

  // Track copied state expiration per code block instance
  static std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      s_copiedTimers;

  bool isCopied = false;
  auto it = s_copiedTimers.find(baseId);
  if (it != s_copiedTimers.end()) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second)
            .count();
    if (elapsed < 1500) { // Show "Copied" state for 1.5 seconds
      isCopied = true;
    } else {
      s_copiedTimers.erase(it);
    }
  }

  Clay_ElementId blockId = utils::layout::getNextId(baseId.c_str());

  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(8.0f));

  Modifier blockStyle = std::move(modifier)
                            .column()
                            .background(theme.background)
                            .border(theme.border, 1.0f)
                            .margin(0, 20)
                            .rounded(radius.x)
                            .widthGrow();

  Interaction result = Div(std::move(blockStyle), [&]() {
    // 1. macOS Titlebar Header
    Div(DefaultModifier()
            .background(theme.titlebarBg)
            .padding(40, 12)
            .widthGrow()
            .center(),
        [&]() {
          // Left Side: Window Traffic Lights & Language Title
          Div(DefaultModifier()
                  .alignX(AlignmentX::SpaceBetween)
                  .alignY(AlignmentY::Center),
              [&]() {
                // Red Dot
                Div([]() {
                  Div(DefaultModifier().size(12, 12).rounded(6.0f).background(
                      "#ff5f56"_hex));
                  // Minimize (Yellow)
                  Div(DefaultModifier().size(12, 12).rounded(6.0f).background(
                      "#ffbd2e"_hex));
                  // Maximize (Green)
                  Div(DefaultModifier().size(12, 12).rounded(6.0f).background(
                      "#27c93f"_hex));
                });
                Div(DefaultModifier().widthGrow(), []() {});
                std::string titleLabel = language.empty() ? "source" : language;
                Text(titleLabel,
                     DefaultModifier().color(theme.comment).fontSize(12.0f));
              });
          Div(DefaultModifier().widthGrow(), []() {});

          // Right Side: Action Button with Unique ID per CodeBlock instance
          std::string copyBtnId = "copyBtn_" + baseId;
          atomicComponents::Toast(
              [&]() {
                auto copyBtn = Button(
                    DefaultModifier().id(copyBtnId.c_str()).center(), [&]() {
                      if (isCopied) {
                        Icon(LucideIcon::Check, DefaultModifier()
                                                    .color("#27c93f"_hex)
                                                    .size(14.0f, 14.0f));
                      } else {
                        Icon(LucideIcon::Copy, DefaultModifier()
                                                   .color(theme.comment)
                                                   .size(14.0f, 14.0f));
                      }
                    });
                if (copyBtn.clicked) {
                  getVeraApp()->setClipboardText(code);
                  s_copiedTimers[baseId] = std::chrono::steady_clock::now();
                }
              },
              [&]() {
                Text(isCopied ? "Copied!" : "Copy To ClipBoard",
                     DefaultModifier().fontSize(10).color(Colors::gray[300]));
              });
        });

    // 2. Code Body Content (Column of lines, where each line is a Row of
    // tokens)
    Div(DefaultModifier().column().padding(16, 20).widthGrow(), [&]() {
      std::vector<std::string> lines;
      std::string currentLine;
      for (size_t k = 0; k < code.size(); ++k) {
        if (code[k] == '\n') {
          lines.push_back(currentLine);
          currentLine.clear();
        } else if (code[k] != '\r') {
          currentLine.push_back(code[k]);
        }
      }
      lines.push_back(currentLine);

      for (const auto &lineStr : lines) {
        auto lineTokens = tokenizeCode(lineStr, language);
        // Each line container acts as a row for its tokens
        Div(DefaultModifier().widthGrow(), [&]() {
          if (lineTokens.empty()) {
            Text("", DefaultModifier().fontSize(13.0f));
          } else {
            for (const auto &token : lineTokens) {
              glm::vec4 col = getTokenColor(token.type, theme);
              Text(token.text, DefaultModifier().color(col).fontSize(13.0f));
            }
          }
        });
      }
    });
  });

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(blockId);
  if (elementData.found && uiState) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool isPressed = isHovered && uiState && uiState->pointerDown;
  result.hovered = isHovered;
  result.pressed = isPressed;

  if (uiState && isHovered &&
      Clay_GetPointerState().state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

} // namespace atomic::extras
