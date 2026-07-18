#define NOMINMAX
#include "avk/atomic_ui.h"
#include "avk/avk_renderer.h"
#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "core/window/window.h"
#include "clay.h"
#include <memory>
#include <vector>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace atomic {
	float sdRoundedBox(glm::vec2 p, glm::vec2 b, glm::vec4 r) {
		float radius = r.x; // Default topLeft
		if (p.x > 0.0f && p.y < 0.0f) {
			radius = r.y; // topRight
		}
		else if (p.x < 0.0f && p.y > 0.0f) {
			radius = r.z; // bottomLeft
		}
		else if (p.x > 0.0f && p.y > 0.0f) {
			radius = r.w; // bottomRight
		}

		glm::vec2 q = glm::abs(p) - b + glm::vec2(radius);
		return glm::min(glm::max(q.x, q.y), 0.0f) + glm::length(glm::max(q, glm::vec2(0.0f))) - radius;
	}

	bool isPointerOverRoundedBox(glm::vec2 pointerPos, Clay_BoundingBox box, glm::vec4 r) {
		if (pointerPos.x < box.x || pointerPos.y < box.y ||
			pointerPos.x > box.x + box.width || pointerPos.y > box.y + box.height) {
			return false;
		}

		glm::vec2 center = glm::vec2(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
		glm::vec2 p = pointerPos - center;
		glm::vec2 halfSize = glm::vec2(box.width * 0.5f, box.height * 0.5f);

		float d = sdRoundedBox(p, halfSize, r);
		return d <= 0.0f;
	}
	struct WindowSession {
		VeraWindow* window = nullptr;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		std::unique_ptr<avk::WindowCanvas> canvas;
		std::chrono::high_resolution_clock::time_point lastTime;
		float lastDeltaTime = 0.016f;
	};

	struct UIState {
		std::unique_ptr<avk::VulkanContext> context;
		std::unique_ptr<avk::Renderer> renderer;
		void* clayArenaMemory = nullptr;
		std::vector<WindowSession> sessions;

		glm::vec2 pointerPos = glm::vec2(0.0f);
		bool pointerPressed = false;

		WindowSession* findSession(VeraWindow* window) {
			auto it = std::find_if(sessions.begin(), sessions.end(), [window](const WindowSession& s) {
				return s.window == window;
				});
			return (it != sessions.end()) ? &(*it) : nullptr;
		}
	};

	static std::unique_ptr<UIState> g_uiState = nullptr;
	static void* g_clayArenaMemory = nullptr;
	static uint32_t g_elementIdCounter = 0;

	static void handleClayError(Clay_ErrorData error) {
		std::cerr << "[Clay Layout]: " << error.errorText.chars << std::endl;
	}

	static Clay_ElementId getNextId(const char* label) {
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "%s_%u", label, g_elementIdCounter++);

		return Clay_GetElementId(Clay_String{
			.isStaticallyAllocated = false,
			.length = static_cast<int32_t>(std::strlen(buffer)),
			.chars = buffer
			});
	}

	void initialize(bool enableValidation) {
		g_uiState = std::make_unique<UIState>();

		g_uiState->context = std::make_unique<avk::VulkanContext>(enableValidation);
		g_uiState->renderer = std::make_unique<avk::Renderer>(g_uiState->context.get());

		uint64_t totalMemorySize = Clay_MinMemorySize();
		g_uiState->clayArenaMemory = std::malloc(totalMemorySize);
		Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, g_uiState->clayArenaMemory);
		Clay_Initialize(arena, Clay_Dimensions{ 800, 600 }, Clay_ErrorHandler{ handleClayError, nullptr });
	}

	void shutdown() {
		if (!g_uiState) return;

		if (g_uiState->context && g_uiState->context->isValid()) {
			vkDeviceWaitIdle(g_uiState->context->getDevice());
		}

		g_uiState->sessions.clear();
		g_uiState->renderer.reset();
		g_uiState->context.reset();

		if (g_uiState->clayArenaMemory) {
			std::free(g_uiState->clayArenaMemory);
		}

		g_uiState.reset();
	}

	void registerWindow(VeraWindow* window) {
		if (!g_uiState) return;

		VeraNativeHandle native = window->getNativeHandle();
		VkSurfaceKHR surface = VK_NULL_HANDLE;

#if defined(VERA_PLATFORM_WIN32)
		surface = g_uiState->context->createWin32Surface(native.hwnd, GetModuleHandle(nullptr));
#elif defined(VERA_PLATFORM_LINUX)
		if (native.waylandSurface != nullptr) {
			surface = g_uiState->context->createWaylandSurface(native.display, native.waylandSurface);
		}
		else {
			surface = g_uiState->context->createX11Surface(native.display, native.x11Window);
		}
#endif

		if (surface == VK_NULL_HANDLE) {
			std::cerr << "avk: Failed to map native surface inside atomicUI." << std::endl;
			return;
		}

		auto state = window->getState();
		auto canvas = std::make_unique<avk::WindowCanvas>(g_uiState->context.get(), surface, state.width, state.height);

		g_uiState->sessions.push_back(WindowSession{
			window,
			surface,
			std::move(canvas),
			std::chrono::high_resolution_clock::now(),
			0.016f
			});

		window->setMouseMoveCallback([](double x, double y) {
			if (!g_uiState) return;
			g_uiState->pointerPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));

			Clay_SetPointerState(
				Clay_Vector2{ g_uiState->pointerPos.x, g_uiState->pointerPos.y },
				g_uiState->pointerPressed
			);
			});

		window->setMouseButtonCallback([](VeraMouseButton button, bool pressed) {
			if (!g_uiState) return;
			if (button == VeraMouseButton::Left) {
				g_uiState->pointerPressed = pressed;

				Clay_SetPointerState(
					Clay_Vector2{ g_uiState->pointerPos.x, g_uiState->pointerPos.y },
					g_uiState->pointerPressed
				);
			}
			});
	}

	void unregisterWindow(VeraWindow* window) {
		if (!g_uiState) return;

		auto it = std::remove_if(g_uiState->sessions.begin(), g_uiState->sessions.end(), [window](const WindowSession& s) {
			return s.window == window;
			});
		if (it != g_uiState->sessions.end()) {
			vkDeviceWaitIdle(g_uiState->context->getDevice());
			g_uiState->sessions.erase(it, g_uiState->sessions.end());
		}
	}

	bool beginFrame(VeraWindow* window) {
		if (!g_uiState) return false;

		WindowSession* session = g_uiState->findSession(window);
		if (!session || !session->canvas->isActive()) return false;

		auto currentTime = std::chrono::high_resolution_clock::now();
		session->lastDeltaTime = std::chrono::duration<float>(currentTime - session->lastTime).count();
		session->lastTime = currentTime;

		g_elementIdCounter = 0;

		Clay_SetLayoutDimensions(Clay_Dimensions{ static_cast<float>(session->canvas->getWidth()), static_cast<float>(session->canvas->getHeight()) });
		Clay_BeginLayout();

		return session->canvas->beginFrame();
	}

	void endFrame(VeraWindow* window) {
		if (!g_uiState) return;

		WindowSession* session = g_uiState->findSession(window);
		if (!session) return;

		Clay_RenderCommandArray renderCommands = Clay_EndLayout(session->lastDeltaTime);

		g_uiState->renderer->begin();

		for (int32_t i = 0; i < renderCommands.length; ++i) {
			Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&renderCommands, i);

			if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
				Clay_RectangleRenderData* rectData = &cmd->renderData.rectangle;

				avk::InstanceData instance{};
				instance.rectXYWH = glm::vec4(
					cmd->boundingBox.x,
					cmd->boundingBox.y,
					cmd->boundingBox.width,
					cmd->boundingBox.height
				);

				instance.fillColorA = glm::vec4(
					rectData->backgroundColor.r / 255.0f,
					rectData->backgroundColor.g / 255.0f,
					rectData->backgroundColor.b / 255.0f,
					rectData->backgroundColor.a / 255.0f
				);

				instance.borderRadius = glm::vec4(
					rectData->cornerRadius.topLeft,
					rectData->cornerRadius.topRight,
					rectData->cornerRadius.bottomLeft,
					rectData->cornerRadius.bottomRight
				);

				instance.shapeType = 0; // Rectangle
				instance.fillType = 0; // Solid
				instance.strokeThickness = 0.0f;
				instance.blur = 0.0f;

				g_uiState->renderer->submit(instance);
			}
		}

		session->canvas->endFrame(*g_uiState->renderer);
	}

	void resizeWindow(VeraWindow* window, uint32_t width, uint32_t height) {
		if (!g_uiState) return;
		WindowSession* session = g_uiState->findSession(window);
		if (session) {
			session->canvas->resize(width, height);
		}
	}

	uint32_t getWidth(VeraWindow* window) {
		if (!g_uiState) return 0;
		WindowSession* session = g_uiState->findSession(window);
		return session ? session->canvas->getWidth() : 0;
	}

	uint32_t getHeight(VeraWindow* window) {
		if (!g_uiState) return 0;
		WindowSession* session = g_uiState->findSession(window);
		return session ? session->canvas->getHeight() : 0;
	}

	void Column(Modifier&& modifier, const std::function<void()>& content) {
		const auto& style = modifier.getStyle();

		Clay__OpenElementWithId(getNextId("Column"));

		Clay_ElementDeclaration decl{};
		decl.layout = {
			.sizing = {
				.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width) : CLAY_SIZING_GROW(),
				.height = style.hasHeight ? CLAY_SIZING_FIXED(style.height) : CLAY_SIZING_GROW()
			},
			.padding = { style.padLeft, style.padRight, style.padTop, style.padBottom },
			.childGap = style.childGap,
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		};
		decl.backgroundColor = {
			style.backgroundColor.r * 255.0f,
			style.backgroundColor.g * 255.0f,
			style.backgroundColor.b * 255.0f,
			style.backgroundColor.a * 255.0f
		};
		decl.cornerRadius = {
			style.borderRadius.x,
			style.borderRadius.y,
			style.borderRadius.z,
			style.borderRadius.w
		};

		Clay__ConfigureOpenElement(decl);

		content();

		Clay__CloseElement();
	}

	void Row(Modifier&& modifier, const std::function<void()>& content) {
		const auto& style = modifier.getStyle();

		Clay__OpenElementWithId(getNextId("Row"));

		Clay_ElementDeclaration decl{};
		decl.layout = {
			.sizing = {
				.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width) : CLAY_SIZING_GROW(),
				.height = style.hasHeight ? CLAY_SIZING_FIXED(style.height) : CLAY_SIZING_GROW()
			},
			.padding = { style.padLeft, style.padRight, style.padTop, style.padBottom },
			.childGap = style.childGap,
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		};
		decl.backgroundColor = {
			style.backgroundColor.r * 255.0f,
			style.backgroundColor.g * 255.0f,
			style.backgroundColor.b * 255.0f,
			style.backgroundColor.a * 255.0f
		};
		decl.cornerRadius = {
			style.borderRadius.x,
			style.borderRadius.y,
			style.borderRadius.z,
			style.borderRadius.w
		};

		Clay__ConfigureOpenElement(decl);

		content();

		Clay__CloseElement();
	}

	
	Interaction Button(Modifier&& modifier) {
		const auto& style = modifier.getStyle();

		Clay_ElementId buttonId = getNextId("Button");
		Clay__OpenElementWithId(buttonId);

		Clay_ElementDeclaration decl{};
		decl.layout = {
			.sizing = {
				.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width) : CLAY_SIZING_GROW(),
				.height = style.hasHeight ? CLAY_SIZING_FIXED(style.height) : CLAY_SIZING_GROW()
			}
		};

		// 1. Fetch previous frame's bounding box using Clay's stable, public API
		bool isHovered = false;
		Clay_ElementData elementData = Clay_GetElementData(buttonId);
		if (elementData.found) {
			isHovered = isPointerOverRoundedBox(g_uiState->pointerPos, elementData.boundingBox, style.borderRadius);
		}

		bool isPressed = isHovered && g_uiState->pointerPressed;

		// 2. Perform automatic visual feedback
		Clay_Color color = {
			style.backgroundColor.r * 255.0f,
			style.backgroundColor.g * 255.0f,
			style.backgroundColor.b * 255.0f,
			style.backgroundColor.a * 255.0f
		};

		if (isPressed) {
			color.r *= 0.8f; color.g *= 0.8f; color.b *= 0.8f; // Darken on press
		}
		else if (isHovered) {
			color.r = std::min(color.r * 1.15f, 255.0f);       // Brighten on hover
			color.g = std::min(color.g * 1.15f, 255.0f);
			color.b = std::min(color.b * 1.15f, 255.0f);
		}

		decl.backgroundColor = color;
		decl.cornerRadius = {
			style.borderRadius.x,
			style.borderRadius.y,
			style.borderRadius.z,
			style.borderRadius.w
		};

		Clay__ConfigureOpenElement(decl);
		Clay__CloseElement();

		// 3. Populate and return Interaction State block
		Interaction result{};
		result.hovered = isHovered;
		result.pressed = isPressed;

		// Click is registered if released this frame while hovered on the active rounded shape
		auto pointerState = Clay_GetPointerState();
		if (isHovered && pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
			result.clicked = true;
		}

		return result;
	}
} // namespace atomic