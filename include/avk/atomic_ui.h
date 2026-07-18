#pragma once

#include <glm/glm.hpp>
#include <string>
#include <functional>
#include <memory>

// Forward declare Vera structures
class VeraWindow;

namespace atomic {

	/**
	 * @brief Chaining Modifier holding rendering and alignment styles.
	 */
	struct Style {
		glm::vec4 backgroundColor = glm::vec4(0.0f);
		glm::vec4 borderRadius = glm::vec4(0.0f);
		glm::vec4 strokeColor = glm::vec4(0.0f);
		float strokeThickness = 0.0f;

		float width = 0.0f;
		float height = 0.0f;
		bool hasWidth = false;
		bool hasHeight = false;

		uint16_t padLeft = 0;
		uint16_t padRight = 0;
		uint16_t padTop = 0;
		uint16_t padBottom = 0;

		uint16_t childGap = 0;
	};

	/**
	* @brief Lightweight, implicit-bool convertible window interaction state block.
	*/
	struct Interaction {
		bool clicked = false;
		bool hovered = false;
		bool pressed = false;

		explicit operator bool() const { return clicked; }
	};



	class Modifier {
	public:
		Modifier() = default;

		Modifier background(const glm::vec4& color)&& {
			m_style.backgroundColor = color;
			return std::move(*this);
		}

		Modifier size(float width, float height)&& {
			m_style.width = width;
			m_style.height = height;
			m_style.hasWidth = true;
			m_style.hasHeight = true;
			return std::move(*this);
		}

		Modifier rounded(float radius)&& {
			m_style.borderRadius = glm::vec4(radius);
			return std::move(*this);
		}

		Modifier border(const glm::vec4& color, float thickness)&& {
			m_style.strokeColor = color;
			m_style.strokeThickness = thickness;
			return std::move(*this);
		}

		Modifier padding(uint16_t horizontal, uint16_t vertical)&& {
			m_style.padLeft = horizontal;
			m_style.padRight = horizontal;
			m_style.padTop = vertical;
			m_style.padBottom = vertical;
			return std::move(*this);
		}

		Modifier gap(uint16_t spacing)&& {
			m_style.childGap = spacing;
			return std::move(*this);
		}

		const Style& getStyle() const { return m_style; }

	private:
		Style m_style;
	};

	inline Modifier DefaultModifier() {
		return Modifier{};
	}

	void initialize(bool enableValidation = false);
	void shutdown();

	void registerWindow(VeraWindow* window);
	void unregisterWindow(VeraWindow* window);

	bool beginFrame(VeraWindow* window);
	void endFrame(VeraWindow* window);

	void resizeWindow(VeraWindow* window, uint32_t width, uint32_t height);

	uint32_t getWidth(VeraWindow* window);
	uint32_t getHeight(VeraWindow* window);

	void Column(Modifier&& modifier, const std::function<void()>& content);
	void Row(Modifier&& modifier, const std::function<void()>& content);
	Interaction Button(Modifier&& modifier);

} // namespace atomic