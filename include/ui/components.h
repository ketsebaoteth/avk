#include "avk/atomic_ui.h"

namespace atomic {

/**
 * @brief Clean interactive Button layout component with optional child
 * composition.
 */
Interaction Button(Modifier &&modifier,
                   const std::function<void()> &content = nullptr);

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

} // namespace atomic
