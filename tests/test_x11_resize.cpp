// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Exercise the real X11 adapter and server event queue. Only allocation outcomes are substituted.
#define main x11_program_main
// Compile the adapter itself so ConfigureNotify and allocation order cannot diverge from the app.
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/x11/main.cpp"
#undef main
#include "check.hpp"
#include <new>

// Linker --wrap requires these exact symbol names.
// NOLINTNEXTLINE(bugprone-reserved-identifier)
extern "C" void* __real_malloc(std::size_t);
// NOLINTNEXTLINE(bugprone-reserved-identifier)
extern "C" XImage* __real_XCreateImage(Display*, Visual*, unsigned, int, int, char*, unsigned,
                                       unsigned, int, int);

namespace {
enum class Failure : std::uint8_t { Disabled, Pixels, Coverage, Image, ImageData };
Failure failure = Failure::Disabled;
unsigned array_attempts = 0;
const RefineRunner* retiring_runner = nullptr;
const void* retiring_pixels = nullptr;
XImage* retiring_image = nullptr;
unsigned retired_resources = 0;
bool retired_while_active = false;
bool inject = false, native_pending = false;
struct Allocation {
    void* pointer = nullptr;
};
std::array<Allocation, 128> arrays{};
struct Image {
    XImage* pointer = nullptr;
    int (*destroy)(XImage*) = nullptr;
};
std::array<Image, 8> images{};
unsigned live_arrays() {
    return static_cast<unsigned>(
        std::count_if(arrays.begin(), arrays.end(), [](auto a) { return a.pointer != nullptr; }));
}
unsigned live_images() {
    return static_cast<unsigned>(
        std::count_if(images.begin(), images.end(), [](auto a) { return a.pointer != nullptr; }));
}
void release_array(void* pointer) noexcept {
    if (retiring_runner && pointer == retiring_pixels) {
        ++retired_resources;
        retired_while_active |= retiring_runner->active();
    }
    for (auto& a : arrays)
        if (a.pointer == pointer)
            a = {};
    std::free(pointer);
}
int destroy_image(XImage* pointer) {
    if (retiring_runner && pointer == retiring_image) {
        ++retired_resources;
        retired_while_active |= retiring_runner->active();
    }
    for (auto& image : images) {
        if (image.pointer == pointer) {
            const auto destroy = image.destroy;
            image = {};
            return destroy(pointer);
        }
    }
    std::abort();
}
} // namespace

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (inject) {
        ++array_attempts;
        if ((failure == Failure::Pixels && array_attempts == 1) ||
            (failure == Failure::Coverage && array_attempts == 2))
            return nullptr;
    }
    void* pointer = __real_malloc(size == 0 ? 1 : size);
    if (pointer)
        for (auto& a : arrays)
            if (!a.pointer) {
                a.pointer = pointer;
                return pointer;
            }
    if (pointer)
        std::abort();
    return nullptr;
}
void operator delete[](void* pointer) noexcept {
    release_array(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    release_array(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    release_array(pointer);
}

// NOLINTNEXTLINE(bugprone-reserved-identifier)
extern "C" void* __wrap_malloc(std::size_t size) {
    if (native_pending) {
        native_pending = false;
        if (inject && failure == Failure::ImageData)
            return nullptr;
    }
    return __real_malloc(size);
}
// NOLINTNEXTLINE(bugprone-reserved-identifier)
extern "C" XImage* __wrap_XCreateImage(Display* d, Visual* v, unsigned depth, int format,
                                       int offset, char* data, unsigned width, unsigned height,
                                       int pad, int stride) {
    if (inject && failure == Failure::Image)
        return nullptr;
    auto* result =
        __real_XCreateImage(d, v, depth, format, offset, data, width, height, pad, stride);
    if (result) {
        for (auto& image : images)
            if (!image.pointer) {
                image = {result, result->f.destroy_image};
                result->f.destroy_image = destroy_image;
                native_pending = true; // the adapter's next malloc is this image's pixel storage
                return result;
            }
        std::abort();
    }
    return result;
}

namespace {
struct ResizeTest {
    static Framebuffer& frame(App& app) {
        if (!app.fb_)
            std::abort();
        return *app.fb_;
    }
    static void drain(App& app) {
        XSync(app.dpy_, False);
        while (XPending(app.dpy_)) {
            XEvent event{};
            XNextEvent(app.dpy_, &event);
            CHECK(!app.handle(event));
        }
    }
    static void render(App& app) {
        if (app.need_redraw_) {
            app.present();
            app.need_redraw_ = false;
        }
    }
    static void resize(App& app, int width, int height) {
        XResizeWindow(app.dpy_, app.win_, static_cast<unsigned>(width + StyleWindow::kWidth),
                      static_cast<unsigned>(height));
        drain(app); // real ConfigureNotify delivered by Xvfb, not a direct Resize message
    }
    static std::uint64_t pixels(App& app, int width, int height) {
        XSync(app.dpy_, False);
        auto* image = XGetImage(app.dpy_, app.win_, 0, 0, static_cast<unsigned>(width),
                                static_cast<unsigned>(height), AllPlanes, ZPixmap);
        CHECK(image != nullptr);
        if (!image)
            return 0;
        std::uint64_t hash = 1469598103934665603ULL;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                hash = (hash ^ XGetPixel(image, x, y)) * 1099511628211ULL;
        XDestroyImage(image);
        return hash;
    }
    static void expose(App& app) {
        XClearArea(app.dpy_, app.win_, 0, 0, 0, 0, True);
        drain(app); // real Expose; the retained frame must be uploaded again
    }
    static void run(Display* display, const Scene& scene, ElevationPacks& packs, Failure point,
                    int requested_width, int requested_height) {
        Options options;
        options.width = 320;
        options.height = 240;
        App app(display, scene, packs, options);
        CHECK(app.init());
        XMoveWindow(display, app.win_, 0, 0);
        XMoveWindow(display, app.toolbar_, 620, 0);
        drain(app);
        render(app);
        // Keep an actual partially executed job across the refusal. No timer sleeps are needed.
        app.apply(Tick{1000});
        app.apply(Tick{1150});
        app.apply(Tick{1151});
        CHECK(app.runner_.active());
        app.refine_step();
        CHECK(app.runner_.active());
        expose(app); // publish the current partial frame before comparing subsequent repaints
        const auto model = app.model_;
        const auto* framebuffer = frame(app).data();
        auto* image = app.image_;
        const auto* job = app.runner_.job();
        const auto progress = job->progress();
        const auto wake = app.wake_at_ms_;
        const auto arrays_before = live_arrays(), images_before = live_images();
        const int visible_width = std::min(320, requested_width);
        const int visible_height = std::min(240, requested_height);
        const auto before = pixels(app, visible_width, visible_height);
        failure = point;
        array_attempts = 0;
        inject = true;
        resize(app, requested_width, requested_height);
        inject = false;
        CHECK(array_attempts == (point == Failure::Pixels ? 1U : 2U));
        std::printf("failure=%d model=%dx%d buffer=%dx%d generation=%llu -> %llu\n",
                    static_cast<int>(point), app.model_.camera.width, app.model_.camera.height,
                    frame(app).width(), frame(app).height(),
                    static_cast<unsigned long long>(model.generation),
                    static_cast<unsigned long long>(app.model_.generation));
        CHECK(app.model_.camera.width == 320 && app.model_.camera.height == 240);
        CHECK(app.model_.generation == model.generation);
        CHECK(app.model_.refine_active == model.refine_active);
        CHECK(app.runner_.active() && app.runner_.job() == job);
        CHECK(app.runner_.job()->progress() == progress);
        CHECK(app.wake_at_ms_ == wake);
        CHECK(!app.need_redraw_ && app.frame_valid_);
        CHECK(frame(app).data() == framebuffer && app.image_ == image);
        CHECK(live_arrays() == arrays_before && live_images() == images_before);
        expose(app);
        CHECK(pixels(app, visible_width, visible_height) == before);
        CHECK(app.model_.generation == model.generation && app.runner_.active());
        // A refused native grow leaves the old viewport at (0,0); new area stays background.
        if (requested_height > 240) {
            auto* margin = XGetImage(display, app.win_, 0, 280, 1, 1, AllPlanes, ZPixmap);
            CHECK(margin && XGetPixel(margin, 0, 0) == BlackPixel(display, DefaultScreen(display)));
            if (margin)
                XDestroyImage(margin);
        }
        // Progress is quantized during line setup; allow a bounded number of continuations.
        for (unsigned i = 0;
             i < 4096 && app.runner_.active() && app.runner_.job()->progress() == progress; ++i)
            app.refine_step();
        CHECK(app.runner_.job()->progress() > progress);
        XEvent key{};
        key.xkey.type = KeyPress;
        key.xkey.display = display;
        key.xkey.window = app.win_;
        key.xkey.keycode = XKeysymToKeycode(display, XK_Right);
        XSendEvent(display, app.win_, False, KeyPressMask, &key);
        drain(app);
        CHECK(app.model_.generation > model.generation);
        CHECK(!app.runner_.active());
        render(app);
        const auto navigated = pixels(app, visible_width, visible_height);
        CHECK(navigated != before);
        expose(app);
        CHECK(pixels(app, visible_width, visible_height) == navigated);
        // Success must cancel an active continuation before either old resource is released.
        const auto input_time = app.model_.stage_since_ms;
        app.apply(Tick{input_time + kSettleDelayMs});
        app.apply(Tick{input_time + kSettleDelayMs + kRefineDelayMs});
        CHECK(app.runner_.active());
        retiring_runner = &app.runner_;
        retiring_pixels = frame(app).data();
        retiring_image = app.image_;
        retired_resources = 0;
        retired_while_active = false;
        // A distinct native size sends another ConfigureNotify and retries allocation normally.
        const auto before_success = app.model_.generation;
        resize(app, 410, 310);
        retiring_runner = nullptr;
        CHECK(app.model_.generation == before_success + 1);
        CHECK(retired_resources == 2 && !retired_while_active);
        CHECK(app.model_.camera.width == 410 && app.model_.camera.height == 310);
        CHECK(frame(app).width() == 410 && frame(app).height() == 310);
        CHECK(app.image_->width == 410 && app.image_->height == 310);
        CHECK(!app.runner_.active());
        render(app);
        const auto recovered = pixels(app, 410, 310);
        expose(app);
        CHECK(pixels(app, 410, 310) == recovered);
    }
};
} // namespace

int main() {
    auto* display = XOpenDisplay(nullptr);
    if (!display)
        return 2;
    const auto atlas = Atlas::parse(kEmbeddedAtlasData, kEmbeddedAtlasSize);
    const auto scene = atlas ? Scene::create(*atlas) : std::nullopt;
    ElevationPacks packs;
    if (!scene || !packs.initialize(kEmbeddedElevationData, kEmbeddedElevationSize))
        return 2;
    const auto baseline_arrays = live_arrays();
    for (auto point : {Failure::Pixels, Failure::Coverage, Failure::Image, Failure::ImageData}) {
        for (const auto size : {std::pair{400, 300}, std::pair{280, 200}}) {
            ResizeTest::run(display, *scene, packs, point, size.first, size.second);
            CHECK(live_arrays() == baseline_arrays && live_images() == 0);
        }
    }
    XCloseDisplay(display);
    return TEST_MAIN_RESULT();
}
