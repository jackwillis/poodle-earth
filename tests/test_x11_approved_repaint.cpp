// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// Regression: during a held drag an Expose repaints the last approved native image, never the live
// framebuffer, and leaves the refinement job alone. Drives the real X11 adapter's event handler
// with a server-generated Expose (the window is cleared first, so only a repaint can restore
// pixels) and reads the window back. The L arm (explicit live upload) is the negative control: if
// it cannot be told from the approved image the test fails as inconclusive rather than passing.
//
// The adapter is compiled in-process (like test_x11_resize) so its real event handler runs. The
// private members are opened with a test-only `#define private public` after every standard and X11
// header is already included; no production source or header is changed.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <malloc.h>
#include <sys/resource.h>
#include <sys/select.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#define main x11_program_main
#define private public
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../src/platform/x11/main.cpp"
#undef private
#undef main
#include "check.hpp"

namespace {

enum class Arm : std::uint8_t { Idle, P, L };
enum class Variant : std::uint8_t { Min, Mid, Tail };

const char* arm_name(Arm a) {
    switch (a) {
    case Arm::Idle:
        return "N";
    case Arm::P:
        return "P";
    case Arm::L:
        return "L";
    }
    return "?";
}
const char* variant_name(Variant v) {
    switch (v) {
    case Variant::Min:
        return "min";
    case Variant::Mid:
        return "mid";
    case Variant::Tail:
        return "tail";
    }
    return "?";
}

constexpr int kW = 200, kH = 150;          // globe pane; the sidebar is a child window to its right
using Pixels = std::vector<std::uint32_t>; // masked pixel values of the globe pane, row-major

std::uint64_t fnv(const void* data, std::size_t size, std::uint64_t h = 1469598103934665603ULL) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}
std::size_t diff_count(const Pixels& a, const Pixels& b) {
    if (a.size() != b.size())
        return a.size() + b.size();
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}

// The harness reads `app.fb_` only after init() created it; a missing frame would abort the run.
// NOLINTBEGIN(bugprone-unchecked-optional-access)
struct AfTest {
    // The pixel format's value mask over the three channels, as verify_window uses.
    static unsigned long mask(const App& app) {
        return app.format_.red_mask | app.format_.green_mask | app.format_.blue_mask;
    }
    static unsigned long value_at(const App& app, const std::uint8_t* base, std::size_t stride,
                                  int x, int y) {
        const int bytes = app.format_.bits_per_pixel / 8;
        const std::uint8_t* p = base + static_cast<std::size_t>(y) * stride +
                                static_cast<std::size_t>(x) * static_cast<std::size_t>(bytes);
        unsigned long v = 0;
        for (int k = 0; k < bytes; ++k) {
            const int shift = 8 * (app.format_.msb_first ? bytes - 1 - k : k);
            v |= static_cast<unsigned long>(p[k]) << shift;
        }
        return v & mask(app);
    }
    // What the window really holds (server readback), the globe pane only.
    static Pixels window(App& app) {
        XSync(app.dpy_, False);
        Pixels out(static_cast<std::size_t>(kW) * kH, 0);
        XImage* img = XGetImage(app.dpy_, app.win_, 0, 0, kW, kH, AllPlanes, ZPixmap);
        CHECK(img != nullptr);
        if (img == nullptr)
            return out;
        const unsigned long m = mask(app);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                out[static_cast<std::size_t>(y) * kW + x] =
                    static_cast<std::uint32_t>(XGetPixel(img, x, y) & m);
        XDestroyImage(img);
        return out;
    }
    // The adapter's own native image (what resend() would send).
    static Pixels native(const App& app) {
        Pixels out(static_cast<std::size_t>(kW) * kH, 0);
        const auto* base = reinterpret_cast<const std::uint8_t*>(app.image_->data);
        const auto stride = static_cast<std::size_t>(app.image_->bytes_per_line);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                out[static_cast<std::size_t>(y) * kW + x] =
                    static_cast<std::uint32_t>(value_at(app, base, stride, x, y));
        return out;
    }
    // Independent conversion (generic converter) of the live frame. `composed` runs it through a
    // private ShownFrame exactly as upload() would (overlays, scale, compass, progress bar).
    static Pixels live(App& app, bool composed) {
        const std::size_t stride = pixel_row_bytes(app.format_, kW);
        std::vector<std::uint8_t> bytes(stride * kH);
        if (composed) {
            ShownFrame probe;
            probe.set_scale_units(app.model_.prefs.units);
            const bool settled = frame_settled(app.model_, app.runner_.active());
            const Framebuffer& shown =
                probe.begin(app.overlays_, *app.fb_, app.model_.camera,
                            static_cast<std::int64_t>(std::time(nullptr)), settled,
                            bar_progress(app.model_, app.runner_.job()));
            CHECK(convert_pixels_generic(shown, app.format_, bytes.data(), stride));
            probe.end();
        } else {
            CHECK(convert_pixels_generic(*app.fb_, app.format_, bytes.data(), stride));
        }
        Pixels out(static_cast<std::size_t>(kW) * kH, 0);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                out[static_cast<std::size_t>(y) * kW + x] =
                    static_cast<std::uint32_t>(value_at(app, bytes.data(), stride, x, y));
        return out;
    }
    static std::uint64_t fb_hash(const App& app) {
        return fnv(app.fb_->data(), app.fb_->byte_size());
    }
    static bool all_background(const Pixels& p, std::uint32_t background) {
        return std::all_of(p.begin(), p.end(), [&](std::uint32_t v) { return v == background; });
    }

    struct Counts {
        std::size_t uploads = 0, presents = 0, exposes = 0;
    };
    static Counts counts(const App& app) {
        return {app.t_.pack_ms.size(), app.t_.present_ms.size(), 0};
    }

    // Drain the real X event queue. A count==0 Expose on the globe window is dispatched per arm.
    static Counts drain(App& app, Arm arm) {
        const auto before = counts(app);
        std::size_t exposes = 0;
        XSync(app.dpy_, False);
        while (XPending(app.dpy_) > 0) {
            XEvent ev{};
            XNextEvent(app.dpy_, &ev);
            if (ev.type == Expose && ev.xany.window == app.win_ && ev.xexpose.count == 0) {
                ++exposes;
                app.first_expose_ = true;
                if (arm == Arm::L) { // deliberate live conversion
                    if (app.fb_) // unguarded: the control must show live pixels whatever the state
                        app.upload(0, app.fb_->height());
                    continue;
                }
            }
            CHECK(!app.handle(ev));
        }
        const auto after = counts(app);
        return {after.uploads - before.uploads, after.presents - before.presents, exposes};
    }
    static void render(App& app) {
        if (app.need_redraw_) {
            app.present();
            app.need_redraw_ = false;
        }
    }
    static std::uint32_t background(App& app) {
        return static_cast<std::uint32_t>(BlackPixel(app.dpy_, DefaultScreen(app.dpy_)) &
                                          mask(app));
    }
    // A real Expose: the server clears the window (to the background pixel) and queues the event.
    static void clear_window(App& app) {
        XClearArea(app.dpy_, app.win_, 0, 0, 0, 0, True);
        XSync(app.dpy_, False);
    }
    static std::uint64_t now_ms() { return monotonic_ns() / 1000000ULL; }

    static void release(App& app, Arm arm) {
        XEvent ev{};
        ev.xbutton.type = ButtonRelease;
        ev.xbutton.display = app.dpy_;
        ev.xbutton.window = app.win_;
        ev.xbutton.button = Button1;
        ev.xbutton.x = kW / 2 + 20;
        ev.xbutton.y = kH / 2 + 10;
        XSendEvent(app.dpy_, app.win_, False, ButtonReleaseMask, &ev);
        drain(app, arm);
        render(app); // the event loop presents a pending redraw after the events
    }

    static void start_scene(App& app) {
        CHECK(app.init());
        app.country_names_ = false; // first experiment: no native label overlay
        XMoveWindow(app.dpy_, app.win_, 0, 0);
        XMoveWindow(app.dpy_, app.toolbar_, 0, 400);
        drain(app, Arm::P); // map/configure/initial Expose; ignored while need_redraw_ is set
    }

    // The sequence reaches a held, refining state with an approved moving frame on screen.
    struct Held {
        Pixels r0, n0, live0;
        std::uint64_t f0 = 0;
        std::size_t publications = 0, steps = 0, d_native = 0, d_raw = 0;
    };

    static void line(const char* key, const std::string& value) {
        std::printf("OBS %s=%s\n", key, value.c_str());
    }
    static std::string h(std::uint64_t v) {
        char b[24];
        std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
        return b;
    }
    static std::string n(std::size_t v) { return std::to_string(v); }

    static bool enter_hold(App& app, Variant variant, Held& s, const std::string& tag) {
        render(app); // the initial frame; becomes approved
        const std::uint64_t base = 100000;
        app.apply(Tick{base});
        // A drag past the slop: the moving frame is presented while held.
        app.apply(PointerDown{base + 1, 100.0, 75.0});
        app.apply(PointerMove{base + 10, 120.0, 85.0});
        CHECK(presentation_held(app.model_));
        render(app);
        CHECK(app.frame_valid_ && !app.need_redraw_);
        XSync(app.dpy_, False);
        s.r0 = window(app);
        s.n0 = native(app);
        s.f0 = fb_hash(app);
        CHECK(diff_count(s.r0, s.n0) == 0); // a full upload: window == native image
        // The drag pauses: refinement starts but is not presented.
        app.apply(Tick{base + 10 + kDragPauseMs});
        app.apply(Tick{base + 10 + kDragPauseMs + kRefineDelayMs});
        CHECK(app.runner_.active() && presentation_held(app.model_));
        if (!app.runner_.active())
            return false;
        const auto bands0 = app.refine_bands_;
        const auto uploads0 = app.t_.pack_ms.size();
        const std::size_t threshold =
            std::max<std::size_t>(64, static_cast<std::size_t>(kW) * kH / 200);
        std::size_t guard = 0;
        for (;;) {
            app.refine_step();
            ++s.steps;
            if (variant == Variant::Tail) {
                if (!app.runner_.active())
                    break;
            } else {
                // `min`: the first step at which live and approved differ at all. `mid`: a
                // substantial, still-running refinement.
                s.live0 = live(app, true);
                s.d_native = diff_count(s.live0, s.n0);
                if (variant == Variant::Min && s.d_native > 0)
                    break;
                if (variant == Variant::Mid && app.refine_bands_ - bands0 >= 6 &&
                    s.d_native >= threshold)
                    break;
            }
            if (++guard > 2000000 || !app.runner_.active())
                break;
        }
        s.publications = app.refine_bands_ - bands0;
        s.live0 = live(app, true);
        s.d_native = diff_count(s.live0, s.n0);
        s.d_raw = diff_count(live(app, false), s.n0);
        CHECK(app.t_.pack_ms.size() == uploads0); // held: refinement uploaded nothing
        line((tag + ".publications").c_str(), n(s.publications));
        line((tag + ".steps").c_str(), n(s.steps));
        return true;
    }

    static std::string tag_of(int depth, const char* what, Arm arm) {
        return std::string("d") + std::to_string(depth) + "." + what + "." + arm_name(arm);
    }

    // One held-drag scenario. `verify` runs the adapter with --verify-pixels semantics.
    static void run(Display* display, const Scene& scene, ElevationPacks& packs, Arm arm,
                    Variant variant, int depth, bool verify = false) {
        const std::string tag = tag_of(
            depth, (std::string(variant_name(variant)) + (verify ? "+verify" : "")).c_str(), arm);
        Options options;
        options.width = kW;
        options.height = kH;
        options.verify_pixels = verify;
        App app(display, scene, packs, options);
        start_scene(app);
        Held s;
        const bool held = enter_hold(app, variant, s, tag);
        CHECK(held);
        if (!held)
            return;
        const std::size_t threshold =
            std::max<std::size_t>(64, static_cast<std::size_t>(kW) * kH / 200);
        // Validity: the approved and the live (composed) presentation demonstrably differ.
        CHECK(variant == Variant::Min ? s.d_native > 0 : s.d_native >= threshold);
        CHECK(app.runner_.active() == (variant != Variant::Tail));
        const auto* job = app.runner_.job();
        const auto generation = app.model_.generation;
        const double progress = job != nullptr ? job->progress() : -1.0;
        const auto f_before = fb_hash(app);
        const auto frames_before = app.verified_frames_;

        if (arm != Arm::Idle) {
            clear_window(app);
            CHECK(all_background(window(app), background(app))); // only a repaint can restore it
            const auto c = drain(app, arm); // the real Expose queued by the server
            CHECK(c.exposes == 1);
            const Pixels r1 = window(app);
            const Pixels n1 = native(app);
            line((tag + ".uploads_during_expose").c_str(), n(c.uploads));
            line((tag + ".resends_during_expose").c_str(), n(c.presents - c.uploads));
            line((tag + ".D_R1_vs_R0").c_str(), n(diff_count(r1, s.r0)));
            line((tag + ".D_R1_vs_liveComposed").c_str(), n(diff_count(r1, s.live0)));
            CHECK(fb_hash(app) == f_before); // no repaint touches the live frame
            if (arm == Arm::P) {
                // The contract: the approved image is shown again, unconverted and intact.
                CHECK(c.uploads == 0 && c.presents - c.uploads == 1);
                CHECK(diff_count(r1, s.r0) == 0);
                CHECK(diff_count(n1, s.n0) == 0);
                CHECK(diff_count(r1, s.live0) > 0);
            } else { // negative control: a live conversion must be distinguishable from approved
                CHECK(diff_count(r1, s.live0) == 0 && diff_count(r1, s.r0) > 0);
            }
        }
        // The job, generation and progress are untouched and the refinement completes.
        CHECK(app.model_.generation == generation);
        CHECK(app.runner_.job() == job);
        CHECK(app.runner_.active() == (variant != Variant::Tail));
        if (job != nullptr) // no refinement step ran since `progress` was read, so exact equality
            CHECK(app.runner_.job()->progress() == progress);
        std::size_t more = 0;
        while (app.runner_.active() && more < 4000000) {
            app.refine_step();
            ++more;
        }
        CHECK(!app.runner_.active() && !app.model_.refine_active);
        CHECK(app.model_.generation == generation);
        CHECK(presentation_held(app.model_));
        const Pixels held_end = window(app);
        // Held: neither the repaint nor the completion may show the finished result early.
        if (arm == Arm::P || arm == Arm::Idle)
            CHECK(diff_count(held_end, s.r0) == 0);

        const auto before_release = counts(app);
        release(app, arm);
        const auto after_release = counts(app);
        const Pixels rel = window(app);
        const Pixels expected = live(app, true); // the settled composed presentation
        line((tag + ".uploads_on_release").c_str(),
             n(after_release.uploads - before_release.uploads));
        line((tag + ".D_Rrelease_vs_settled").c_str(), n(diff_count(rel, expected)));
        if (arm == Arm::P || arm == Arm::Idle) {
            CHECK(after_release.uploads - before_release.uploads == 1);
            CHECK(diff_count(rel, expected) == 0);
            CHECK(diff_count(rel, s.r0) > 0); // the finished result replaced the approved frame
        }
        if (verify) {
            CHECK(app.pixels_ok_);
            CHECK(app.verified_frames_ > frames_before);
            line((tag + ".pixels_ok").c_str(), app.pixels_ok_ ? "1" : "0");
        }
    }

    // No approved frame: nothing was presented. `held` adds a paused drag with refinement advanced
    // into the live buffer; `flag_clear` forces the state the production guard normally excludes
    // (no approved frame and no pending redraw), so the held-repaint branch itself is exercised.
    static void no_approved(Display* display, const Scene& scene, ElevationPacks& packs, Arm arm,
                            bool held, bool flag_clear, int depth) {
        const std::string tag =
            tag_of(depth, held ? (flag_clear ? "S0held-noflag" : "S0held") : "S0idle", arm);
        Options options;
        options.width = kW;
        options.height = kH;
        App app(display, scene, packs, options);
        start_scene(app);
        CHECK(!app.frame_valid_);
        if (held) {
            const std::uint64_t base = 100000;
            app.apply(Tick{base});
            app.apply(PointerDown{base + 1, 100.0, 75.0});
            app.apply(PointerMove{base + 10, 120.0, 85.0});
            app.apply(Tick{base + 10 + kDragPauseMs});
            app.apply(Tick{base + 10 + kDragPauseMs + kRefineDelayMs});
            CHECK(app.runner_.active() && presentation_held(app.model_));
            for (int i = 0; i < 200 && app.runner_.active(); ++i)
                app.refine_step();
        }
        if (flag_clear)
            app.need_redraw_ = false;
        const Pixels live_px = live(app, true);
        const auto live_nonbg =
            live_px.size() -
            static_cast<std::size_t>(std::count(live_px.begin(), live_px.end(), background(app)));
        CHECK(live_nonbg > 0); // there are live pixels that must not appear
        clear_window(app);
        CHECK(all_background(window(app), background(app)));
        const auto c = drain(app, arm);
        const Pixels r = window(app);
        line((tag + ".uploads_during_expose").c_str(), n(c.uploads));
        line((tag + ".resends_during_expose").c_str(), n(c.presents - c.uploads));
        if (arm == Arm::P) {
            CHECK(c.uploads == 0 && c.presents == 0);
            CHECK(all_background(r, background(app)));
        } else {
            CHECK(diff_count(r, live_px) == 0); // the control shows the live pixels
        }
    }

    // OBSERVATIONS ONLY (no CHECK): related hazards the fix does not address.
    static void observe(Display* display, const Scene& scene, ElevationPacks& packs, int depth) {
        Options options;
        options.width = kW;
        options.height = kH;
        {
            App app(display, scene, packs, options);
            start_scene(app);
            Held s;
            if (!enter_hold(app, Variant::Mid, s, "obs.reshow"))
                return;
            app.overlays_.toggle_night(); // a keyboard overlay toggle is possible during a drag
            app.reshow();
            const Pixels w = window(app);
            line(
                (std::string("d") + std::to_string(depth) + ".obs.reshow_during_hold.D_vs_approved")
                    .c_str(),
                n(diff_count(w, s.r0)));
            line((std::string("d") + std::to_string(depth) + ".obs.reshow_during_hold.D_vs_live")
                     .c_str(),
                 n(diff_count(w, live(app, true))));
        }
        {
            App app(display, scene, packs, options); // not held: row-partial band uploads
            start_scene(app);
            render(app);
            app.apply(Tick{1000});
            app.apply(Tick{1150});
            app.apply(Tick{1151});
            std::size_t seen = 0, differing = 0, max_diff = 0;
            for (std::size_t i = 0; i < 200000 && app.runner_.active() && seen < 12; ++i) {
                app.next_upload_ms_ = 0;
                const auto before = counts(app).uploads;
                app.refine_step();
                if (counts(app).uploads != before) {
                    ++seen;
                    const auto d = diff_count(window(app), native(app));
                    differing += d > 0;
                    max_diff = std::max(max_diff, d);
                }
            }
            const std::string p = std::string("d") + std::to_string(depth) + ".obs.partial_rows.";
            line((p + "band_uploads_checked").c_str(), n(seen));
            line((p + "uploads_where_window_differs_from_native").c_str(), n(differing));
            line((p + "max_px_window_vs_native").c_str(), n(max_diff));
        }
    }
};
// NOLINTEND(bugprone-unchecked-optional-access)
} // namespace

int main() {
    auto* display = XOpenDisplay(nullptr);
    if (display == nullptr)
        return 2;
    const auto atlas = Atlas::parse(kEmbeddedAtlasData, kEmbeddedAtlasSize);
    const auto scene = atlas ? Scene::create(*atlas) : std::nullopt;
    ElevationPacks packs;
    if (!scene || !packs.initialize(kEmbeddedElevationData, kEmbeddedElevationSize))
        return 2;
    const int depth = DefaultDepth(display, DefaultScreen(display));
    for (const Variant variant : {Variant::Min, Variant::Mid, Variant::Tail})
        for (const Arm arm : {Arm::Idle, Arm::P, Arm::L})
            AfTest::run(display, *scene, packs, arm, variant, depth);
    // Production verification mode (--verify-pixels) must not call a held repaint a mismatch.
    AfTest::run(display, *scene, packs, Arm::P, Variant::Mid, depth, true);
    for (const Arm arm : {Arm::P, Arm::L}) {
        AfTest::no_approved(display, *scene, packs, arm, false, false, depth);
        AfTest::no_approved(display, *scene, packs, arm, true, false, depth);
        AfTest::no_approved(display, *scene, packs, arm, true, true, depth);
    }
    AfTest::observe(display, *scene, packs, depth);
    XCloseDisplay(display);
    return TEST_MAIN_RESULT();
}
