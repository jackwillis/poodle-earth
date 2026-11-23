// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// X-AF1 (EXPERIMENTAL, not for merge): does the real X11 Expose path replace an approved
// presentation with unfinished live refinement pixels, and can the existing XImage + resend()
// preserve the approved image? Characterization first: reproducing the defect is an observation,
// not a test failure. CHECKs cover harness validity only (clear works, live and approved differ,
// the negative control is sensitive, the setup is deterministic). Results are `OBS` lines.
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

enum class Arm : std::uint8_t { Idle, P, S, L };
enum class Variant : std::uint8_t { Min, Mid, Tail };

const char* arm_name(Arm a) {
    switch (a) {
    case Arm::Idle:
        return "N";
    case Arm::P:
        return "P";
    case Arm::S:
        return "S";
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
std::uint64_t hash_pixels(const Pixels& v) {
    return fnv(v.data(), v.size() * sizeof(std::uint32_t));
}
std::size_t diff_count(const Pixels& a, const Pixels& b) {
    if (a.size() != b.size())
        return a.size() + b.size();
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}
// First and last differing row, -1 when equal.
std::pair<int, int> diff_rows(const Pixels& a, const Pixels& b) {
    int first = -1, last = -1;
    if (a.size() != b.size())
        return {0, kH - 1};
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x)
            if (a[static_cast<std::size_t>(y) * kW + x] !=
                b[static_cast<std::size_t>(y) * kW + x]) {
                if (first < 0)
                    first = y;
                last = y;
            }
    return {first, last};
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
                if (arm == Arm::S) { // what a stored-image repaint handler would do
                    if (!app.need_redraw_ && app.fb_)
                        app.resend();
                    continue;
                }
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

    struct Result {
        std::uint64_t r0 = 0, n0 = 0, f0 = 0, live = 0, r1 = 0, n1 = 0, f1 = 0, rrel = 0, exp = 0;
        std::size_t steps = 0, publications = 0, d_native = 0, d_raw = 0;
        bool valid = false;
    };

    static Result run(Display* display, const Scene& scene, ElevationPacks& packs, Arm arm,
                      Variant variant, int depth) {
        Result res;
        const std::string tag = std::string("d") + std::to_string(depth) + "." +
                                variant_name(variant) + "." + arm_name(arm);
        Options options;
        options.width = kW;
        options.height = kH;
        App app(display, scene, packs, options);
        start_scene(app);
        Held s;
        if (!enter_hold(app, variant, s, tag)) {
            CHECK(false);
            return res;
        }
        const std::size_t threshold =
            std::max<std::size_t>(64, static_cast<std::size_t>(kW) * kH / 200);
        // Validity: the approved and the live (composed) presentation must demonstrably differ.
        CHECK(variant == Variant::Min ? s.d_native > 0 : s.d_native >= threshold);
        CHECK(app.runner_.active() == (variant != Variant::Tail));
        const auto* job = app.runner_.job();
        const auto generation = app.model_.generation;
        const double progress = job != nullptr ? job->progress() : -1.0;
        const auto f_before = fb_hash(app);
        line((tag + ".R0").c_str(), h(hash_pixels(s.r0)));
        line((tag + ".native0").c_str(), h(hash_pixels(s.n0)));
        line((tag + ".fb0").c_str(), h(s.f0));
        line((tag + ".liveComposed").c_str(), h(hash_pixels(s.live0)));
        line((tag + ".D_native_vs_liveComposed").c_str(), n(s.d_native));
        line((tag + ".D_native_vs_liveRaw").c_str(), n(s.d_raw));
        line((tag + ".held").c_str(), presentation_held(app.model_) ? "1" : "0");
        line((tag + ".mem.native_image_bytes").c_str(),
             n(static_cast<std::size_t>(app.image_->bytes_per_line) * kH));
        line((tag + ".mem.native_bits_per_pixel").c_str(),
             n(static_cast<std::size_t>(app.image_->bits_per_pixel)));
        line((tag + ".mem.framebuffer_bytes").c_str(), n(app.fb_->byte_size()));
        line((tag + ".mem.coverage_bytes").c_str(), n(app.fb_->coverage_byte_size()));
        line((tag + ".generation").c_str(), n(generation));
        line((tag + ".job_progress").c_str(), std::to_string(progress));
        line((tag + ".upload_dirty").c_str(), app.upload_dirty_ ? "1" : "0");
        res.r0 = hash_pixels(s.r0);
        res.n0 = hash_pixels(s.n0);
        res.f0 = s.f0;
        res.live = hash_pixels(s.live0);
        res.steps = s.steps;
        res.publications = s.publications;
        res.d_native = s.d_native;
        res.d_raw = s.d_raw;

        Pixels r1 = s.r0;
        if (arm != Arm::Idle) {
            clear_window(app);
            const Pixels cleared = window(app);
            CHECK(all_background(cleared, background(app))); // the repaint is observable
            line((tag + ".Rclear_all_background").c_str(),
                 all_background(cleared, background(app)) ? "1" : "0");
            const auto heap0 = mallinfo2().uordblks;
            const auto c = drain(app, arm); // the real Expose event, queued by the server
            const auto heap1 = mallinfo2().uordblks;
            line((tag + ".mem.heap_in_use_delta_during_expose").c_str(),
                 std::to_string(static_cast<long long>(heap1) - static_cast<long long>(heap0)));
            CHECK(c.exposes == 1);
            line((tag + ".expose_events").c_str(), n(c.exposes));
            line((tag + ".uploads_during_expose").c_str(), n(c.uploads));
            line((tag + ".resends_during_expose").c_str(), n(c.presents - c.uploads));
            r1 = window(app);
        }
        const Pixels n1 = native(app);
        res.r1 = hash_pixels(r1);
        res.n1 = hash_pixels(n1);
        res.f1 = fb_hash(app);
        const std::size_t d_r1_r0 = diff_count(r1, s.r0), d_r1_live = diff_count(r1, s.live0);
        const std::size_t d_n1_n0 = diff_count(n1, s.n0);
        line((tag + ".R1").c_str(), h(res.r1));
        line((tag + ".native1").c_str(), h(res.n1));
        line((tag + ".D_R1_vs_R0").c_str(), n(d_r1_r0));
        line((tag + ".D_R1_vs_liveComposed").c_str(), n(d_r1_live));
        line((tag + ".D_native1_vs_native0").c_str(), n(d_n1_n0));
        line((tag + ".fb_unchanged_by_repaint").c_str(), res.f1 == f_before ? "1" : "0");
        CHECK(res.f1 == f_before);
        const bool background_only = all_background(r1, background(app));
        const char* verdict = "n/a";
        if (arm == Arm::P) {
            verdict = background_only  ? "NO-REPAINT"
                      : d_r1_r0 == 0   ? "NO-DEFECT-OBSERVED"
                      : d_r1_live == 0 ? "DEFECT-REPRODUCED(window shows live composed frame)"
                                       : "OTHER(neither approved nor live)";
        } else if (arm == Arm::S) {
            verdict = d_r1_r0 == 0 ? "STORED-IMAGE-RESTORES-APPROVED" : "STORED-IMAGE-INVALID";
            CHECK(d_r1_r0 == 0);
        } else if (arm == Arm::L) {
            verdict = (d_r1_live == 0 && d_r1_r0 > 0) ? "CONTROL-SENSITIVE"
                                                      : "CONTROL-INSENSITIVE(INCONCLUSIVE)";
            CHECK(d_r1_live == 0 && d_r1_r0 > 0);
        }
        line((tag + ".VERDICT").c_str(), verdict);
        if (arm != Arm::Idle && d_r1_r0 != 0) {
            const auto rows = diff_rows(r1, s.r0);
            line((tag + ".R1_vs_R0_rows").c_str(),
                 std::to_string(rows.first) + ".." + std::to_string(rows.second));
        }

        // Cancellation / validity: the job and generation survive the repaint and it completes.
        line((tag + ".job_same_after_repaint").c_str(), app.runner_.job() == job ? "1" : "0");
        line((tag + ".job_active_after_repaint").c_str(), app.runner_.active() ? "1" : "0");
        line((tag + ".generation_same").c_str(), app.model_.generation == generation ? "1" : "0");
        CHECK(app.model_.generation == generation);
        CHECK(app.runner_.job() == job);
        CHECK(app.runner_.active() == (variant != Variant::Tail));
        std::size_t more = 0;
        while (app.runner_.active() && more < 4000000) {
            app.refine_step();
            ++more;
        }
        line((tag + ".steps_after_repaint_to_complete").c_str(), n(more));
        line((tag + ".completed").c_str(),
             !app.runner_.active() && !app.model_.refine_active ? "1" : "0");
        CHECK(!app.runner_.active() && !app.model_.refine_active);
        line((tag + ".still_held_after_completion").c_str(),
             presentation_held(app.model_) ? "1" : "0");
        line((tag + ".upload_dirty_before_release").c_str(), app.upload_dirty_ ? "1" : "0");
        line((tag + ".bar_shown_before_release").c_str(), app.bar_shown_ ? "1" : "0");
        line((tag + ".shown_quick_before_release").c_str(), app.shown_quick_ ? "1" : "0");
        const Pixels held_end = window(app);

        // Release: a real ButtonRelease through the event queue and the adapter's handler.
        const auto before_release = counts(app);
        release(app, arm);
        const auto after_release = counts(app);
        const Pixels rel = window(app);
        const Pixels expected = live(app, true); // the settled composed presentation
        res.rrel = hash_pixels(rel);
        res.exp = hash_pixels(expected);
        const std::size_t d_rel = diff_count(rel, expected);
        line((tag + ".uploads_on_release").c_str(),
             n(after_release.uploads - before_release.uploads));
        line((tag + ".window_before_release_hash").c_str(), h(hash_pixels(held_end)));
        line((tag + ".D_beforeRelease_vs_settled").c_str(), n(diff_count(held_end, expected)));
        line((tag + ".Rrelease").c_str(), h(res.rrel));
        line((tag + ".settledComposed").c_str(), h(res.exp));
        line((tag + ".D_Rrelease_vs_settled").c_str(), n(d_rel));
        if (d_rel != 0) {
            const auto rows = diff_rows(rel, expected);
            line((tag + ".Rrelease_vs_settled_rows").c_str(),
                 std::to_string(rows.first) + ".." + std::to_string(rows.second));
        }
        line((tag + ".RELEASE").c_str(),
             d_rel == 0 ? "RECOVERS" : "STALE(finished result not shown after release)");
        line((tag + ".final_stage_refine").c_str(),
             app.model_.stage == QualityStage::Refine ? "1" : "0");
        res.valid = true;
        return res;
    }

    // No approved frame: nothing has been presented. `held` adds a paused drag with refinement
    // advanced into the live buffer. Arm P is production; arm L is the sensitivity control.
    static void no_approved(Display* display, const Scene& scene, ElevationPacks& packs, Arm arm,
                            bool held, int depth) {
        const std::string tag = std::string("d") + std::to_string(depth) + ".S0" +
                                (held ? "held" : "idle") + "." + arm_name(arm);
        Options options;
        options.width = kW;
        options.height = kH;
        App app(display, scene, packs, options);
        start_scene(app);
        line((tag + ".frame_valid").c_str(), app.frame_valid_ ? "1" : "0");
        line((tag + ".need_redraw").c_str(), app.need_redraw_ ? "1" : "0");
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
            line((tag + ".need_redraw_after_drag").c_str(), app.need_redraw_ ? "1" : "0");
        }
        const Pixels live_px = live(app, true);
        const std::size_t live_nonbg =
            live_px.size() -
            static_cast<std::size_t>(std::count(live_px.begin(), live_px.end(), background(app)));
        line((tag + ".live_nonbackground_px").c_str(), n(live_nonbg));
        line((tag + ".live_nonbackground_px_is_nonzero").c_str(), live_nonbg > 0 ? "1" : "0");
        clear_window(app);
        const Pixels cleared = window(app);
        CHECK(all_background(cleared, background(app)));
        const auto c = drain(app, arm);
        const Pixels r = window(app);
        const std::size_t shown_nonbg =
            r.size() - static_cast<std::size_t>(std::count(r.begin(), r.end(), background(app)));
        line((tag + ".uploads_during_expose").c_str(), n(c.uploads));
        line((tag + ".window_nonbackground_px").c_str(), n(shown_nonbg));
        line((tag + ".D_window_vs_liveComposed").c_str(), n(diff_count(r, live_px)));
        if (arm == Arm::P)
            line((tag + ".VERDICT").c_str(),
                 shown_nonbg == 0 ? "NOTHING-SHOWN(no live pixels)" : "LIVE-OR-OTHER-PIXELS-SHOWN");
        if (arm == Arm::L) {
            line((tag + ".VERDICT").c_str(), diff_count(r, live_px) == 0 && live_nonbg > 0
                                                 ? "CONTROL-SENSITIVE"
                                                 : "CONTROL-INSENSITIVE");
            CHECK(diff_count(r, live_px) == 0 && live_nonbg > 0);
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
    std::printf("OBS provenance.depth=%d\n", depth);
    for (const Variant variant : {Variant::Min, Variant::Mid, Variant::Tail}) {
        AfTest::Result first{};
        for (const Arm arm : {Arm::Idle, Arm::P, Arm::S, Arm::L}) {
            const auto r = AfTest::run(display, *scene, packs, arm, variant, depth);
            if (arm == Arm::Idle)
                first = r;
            // Determinism of the setup across arms: identical approved and live state each time.
            CHECK(r.r0 == first.r0 && r.n0 == first.n0 && r.f0 == first.f0 &&
                  r.live == first.live && r.steps == first.steps);
        }
        // Whole-run repeat: the same arm twice gives the same outputs.
        const auto a = AfTest::run(display, *scene, packs, Arm::P, variant, depth);
        const auto b = AfTest::run(display, *scene, packs, Arm::P, variant, depth);
        CHECK(a.r1 == b.r1 && a.n1 == b.n1 && a.rrel == b.rrel && a.live == b.live);
        std::printf("OBS d%d.%s.repeat_identical=%d\n", depth, variant_name(variant),
                    a.r1 == b.r1 && a.n1 == b.n1 && a.rrel == b.rrel ? 1 : 0);
    }
    for (const bool held : {false, true})
        for (const Arm arm : {Arm::P, Arm::L})
            AfTest::no_approved(display, *scene, packs, arm, held, depth);
    XCloseDisplay(display);
    return TEST_MAIN_RESULT();
}
