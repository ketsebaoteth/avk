#include "core/app/app.h"
#include "avk/atomic_ui.h"

void drawUI(VeraWindow* window) {
	float w = static_cast<float>(atomic::getWidth(window));
	float h = static_cast<float>(atomic::getHeight(window));

	atomic::Column(atomic::DefaultModifier().background({ 0.05f, 0.05f, 0.05f, 1.0f }).size(w, h).padding(20, 20).gap(15), [&]() {

		atomic::Row(atomic::DefaultModifier().background({ 0.1f, 0.1f, 0.12f, 1.0f }).size(w - 40.0f, 100.0f).rounded(12.0f).padding(10, 10).gap(10), [&]() {
			atomic::Button(atomic::DefaultModifier().background({ 1.0f, 0.5f, 0.0f, 1.0f }).size(80.0f, 40.0f).rounded(6.0f));
			auto mybtn = atomic::Button(atomic::DefaultModifier().background({ 1.0f, 0.0f, 0.0f, 1.0f }).size(80.0f, 40.0f).rounded(6.0f));
			if (mybtn.hovered) {
				printf("Hovered!\n");
			}
		});
	

		atomic::Column(atomic::DefaultModifier().background({ 0.08f, 0.08f, 0.1f, 1.0f }).size(w - 40.0f, h - 180.0f).rounded(12.0f), [&]() {
			});
		});
}

int main() {
	VeraApp app(VeraAppInfo{});
	atomic::initialize();

	VeraWindow* window = app.createWindow({
		.width = 800,
		.height = 600,
		.title = "atomicUI Modern Composer",
		.customTitleBar = true
	}).value();

	atomic::registerWindow(window);

	window->setTitlebarHitTestRegions({
		.dragRegion = VeraRect{0, 0, 800, 30},
		.minimizeButton = VeraRect{740, 5, 30, 20},
		.maximizeButton = VeraRect{770, 5, 30, 20},
		.closeButton = VeraRect{800, 5, 30, 20}
		});

	bool isClosing = false;
	window->setCloseRequestCallback([&]() -> bool {
		isClosing = true;
		return true;
	});

	window->setResizeCallback([&isClosing, window](uint32_t w, uint32_t h) {
		if (isClosing) return;

		atomic::resizeWindow(window, w, h);

		window->setTitlebarHitTestRegions({
			.dragRegion = VeraRect{0, 0, w, 30},
			.minimizeButton = VeraRect{w - 60, 5, 30, 20},
			.maximizeButton = VeraRect{w - 30, 5, 30, 20},
			.closeButton = VeraRect{w, 5, 30, 20}
			});

		if (atomic::beginFrame(window)) {
			drawUI(window);
			atomic::endFrame(window);
		}
		});

	while (app.getWindowCount() > 0) {
		app.pollEvents();
		if (app.getWindowCount() == 0) {
			break;
		}

		if (atomic::beginFrame(window)) {
			drawUI(window);
			atomic::endFrame(window);
		}
	}

	atomic::shutdown();

	return 0;
}