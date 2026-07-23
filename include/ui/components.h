#include "avk/atomic_ui.h"

namespace atomic {

/**
 * @brief Clean interactive Button layout component with optional child
 * composition.
 */
Interaction Button(Modifier &&modifier,
                   const std::function<void()> &content = nullptr);

/*
 * @brief A basic Column component
 * */
void Column(Modifier &&modifier, const std::function<void()> &content);

/*
 * @brief A basic Row component
 * */
void Row(Modifier &&modifier, const std::function<void()> &content);

/**
 * @brief Renders a styled vector image component.
 * @param textureIndex Index of the GPU-uploaded boundless texture.
 * @param tint Color multiplier to tint the image (Defaults to white).
 */
Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint = glm::vec4(1.0f));

/**
 * @brief Renders a styled, interactive, layout-integrated text component.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier = DefaultModifier());

// overload with no fontID
Interaction Text(const std::string &text,
                 Modifier &&modifier = DefaultModifier());

/**
 * @brief Renders a highly interactive immediate-mode text input box with full
 * selection and controls.
 * @param textBuffer Reference to the std::string that will hold the typed
 * characters.
 * @param placeholder Fallback placeholder text shown when the buffer is empty.
 */
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId);

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder);

} // namespace atomic
