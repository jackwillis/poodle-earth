// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// X1a (EXPERIMENTAL, not approved for merge): characterization of the existing cooperative
// refinement lifecycle. It records what RefinementJob, RefineRunner, ZoomPreview and ShownFrame
// actually do at their public boundaries: logical publication events, their frame contents,
// progress, snapshot adoption, city markers, keep_partial on cancel, stale generations and
// workspace reuse, and per-continuation bounds. It changes nothing and proposes nothing.
//
// Observations are printed ("OBS ..."). CHECKs cover only harness validity and contracts that
// tests/test_line_layer.cpp or tests/test_zoom.cpp already pin; a difference between budgets or
// configurations is a finding, not a failure.
//
// A logical publication is a RenderStep with band_ready set. It is not a displayed frame: X11
// converts at band_ready (throttled); the Classic app only requests a capture there and converts
// the live frame in a later turn. Nothing here measures display time.
//
//   test_publication_lifecycle <repo root>
#include "atlas_util.hpp"
#include "check.hpp"
#include "globe/app.hpp"
#include "globe/elevation.hpp"
#include "globe/landclasses.hpp"
#include "globe/overlays.hpp"
#include "globe/places.hpp"
#include "globe/raster.hpp"
#include "globe/refine_runner.hpp"
#include "globe/render.hpp"
#include "globe/shown_frame.hpp"
#include "globe/terrain.hpp"
#include "globe/zoom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace globe;
using atlas_util::Bytes;

// The harness allocates a handful of small fixed-size buffers and objects once, validates them
// (CHECK) where they are built, and then dereferences them throughout. Repeating an optional check
// at every use would only obscure what is being observed, so that one check is scoped off here.
// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace {

using Pixels = std::vector<Rgb565>;

Pixels pixels(const Framebuffer& fb) {
    return {fb.data(), fb.data() + static_cast<std::size_t>(fb.width()) * fb.height()};
}

std::uint64_t hash_of(const Pixels& p) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const Rgb565 v : p)
        h = (h ^ v) * 1099511628211ULL;
    return h;
}

Bytes read_file(const std::string& path) {
    Bytes b;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        int c;
        while ((c = std::fgetc(f)) != EOF)
            b.push_back(static_cast<std::uint8_t>(c));
        std::fclose(f);
    }
    return b;
}

void obs(const char* id, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void obs(const char* id, const char* fmt, ...) {
    std::printf("OBS %-34s ", id);
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

// ---- Fixtures ------------------------------------------------------------------------------

struct World {
    Bytes atlas_bytes, elevation_bytes, class_bytes, city_bytes, contrast_bytes;
    std::optional<Atlas> atlas;
    std::optional<Scene> scene;
    std::optional<Elevation> elevation, contrast;
    std::optional<LandClasses> classes;
    std::optional<TerrainScene> terrain, contrast_terrain;
    std::optional<Places> places;
    std::optional<PlaceScene> place_scene;
};

bool build_world(World& w, const std::string& root) {
    std::vector<atlas_util::Poly> polys;
    for (unsigned layer = 1; layer <= 2; ++layer) {
        atlas_util::Poly poly{static_cast<std::uint8_t>(layer), {}};
        for (int i = 0; i < 65; ++i) {
            const double lon = -170 + i * 5.;
            const double lat = (layer == 1 ? 30. : -30.) * std::sin(i * .17);
            poly.points.emplace_back(static_cast<std::uint16_t>((lon + 180) * 65536 / 360),
                                     static_cast<std::uint16_t>((lat + 90) * 65536 / 360));
        }
        polys.push_back(poly);
    }
    w.atlas_bytes = atlas_util::make_atlas(polys, "x1a lines");
    w.atlas = Atlas::parse(w.atlas_bytes.data(), w.atlas_bytes.size());
    w.elevation_bytes = read_file(root + "/data/elevation-1deg.gelv");
    w.class_bytes = read_file(root + "/data/landclasses-1deg.ggrd");
    w.city_bytes = read_file(root + "/data/cities.gplc");
    CHECK(w.atlas && !w.elevation_bytes.empty() && !w.class_bytes.empty() && !w.city_bytes.empty());
    if (!w.atlas)
        return false;
    w.scene = Scene::create(*w.atlas);
    w.elevation = Elevation::parse(w.elevation_bytes.data(), w.elevation_bytes.size());
    w.classes = LandClasses::decode(w.class_bytes.data(), w.class_bytes.size());
    w.places = Places::parse(w.city_bytes.data(), w.city_bytes.size());
    CHECK(w.scene && w.elevation && w.classes && w.places);
    if (!w.scene || !w.elevation || !w.classes || !w.places)
        return false;
    w.terrain = TerrainScene::create(*w.elevation, &*w.classes);
    w.place_scene = PlaceScene::create(*w.places);
    // A 24x12 grid with contrasting neighbours: X0's limb-ordering sensitive fixture.
    w.contrast_bytes = w.elevation_bytes;
    w.contrast_bytes.resize(552 + std::size_t{24} * 12);
    store_le32(w.contrast_bytes.data() + 8, 24);
    store_le32(w.contrast_bytes.data() + 12, 12);
    for (std::size_t i = 0; i < std::size_t{24} * 12; ++i)
        w.contrast_bytes[552 + i] = static_cast<std::uint8_t>((i * 73) % 256);
    store_le32(w.contrast_bytes.data() + 36,
               crc32_extend(crc32(w.contrast_bytes.data(), 36), w.contrast_bytes.data() + 40,
                            w.contrast_bytes.size() - 40));
    w.contrast = Elevation::parse(w.contrast_bytes.data(), w.contrast_bytes.size());
    CHECK(w.contrast);
    if (!w.contrast)
        return false;
    w.contrast_terrain = TerrainScene::create(*w.contrast);
    CHECK(w.terrain && w.place_scene && w.contrast_terrain);
    return w.terrain && w.place_scene && w.contrast_terrain;
}

// ---- A driver: one lifecycle that can be stepped, recorded and cancelled ---------------------

struct Driver {
    Framebuffer* fb = nullptr;
    std::function<RenderStep(unsigned)> step;
    std::function<bool()> active;
    std::function<double()> progress;
    std::function<std::uint32_t()> slices;
    std::function<std::pair<bool, bool>()> zoom; // (valid, baked); false,false for a bare job
    std::shared_ptr<void> keep;                  // owns everything the lambdas point at
    Pixels initial;                              // the frame the job started from
    std::function<Pixels()> snapshot_pixels;     // job-driven only: the snapshot buffer
};

struct Ev {
    std::uint64_t cum_units = 0;
    unsigned units = 0;
    bool ready = false, active_after = false;
    int first = 0, end = 0;
    double progress = 0;
    std::uint32_t slices = 0, slice_delta = 0;
    bool zoom_valid = false, zoom_baked = false;
    std::uint64_t hash = 0;
};

struct Trace {
    std::vector<Ev> steps;
    std::vector<std::size_t> pubs; // indices into steps
    std::vector<Pixels> frames;    // one per publication (what the caller could present)
    std::int64_t adopt_step = -1;  // first step after which the zoom cache is valid
    std::uint32_t max_slice_delta = 0;
    unsigned max_units = 0;
};

// Steps a driver to completion at `budget`, recording every returned event.
Trace record(Driver& d, unsigned budget, const std::function<void(const Ev&, Driver&)>& hook = {}) {
    Trace t;
    std::uint64_t cum = 0;
    std::uint32_t last_slices = 0;
    while (d.active()) {
        const RenderStep r = d.step(budget);
        Ev e;
        e.units = r.work_units;
        cum += r.work_units;
        e.cum_units = cum;
        e.ready = r.band_ready;
        e.first = r.first_row;
        e.end = r.end_row;
        e.active_after = d.active();
        e.progress = d.progress();
        e.slices = d.slices();
        e.slice_delta = e.slices - last_slices;
        last_slices = e.slices;
        const auto z = d.zoom();
        e.zoom_valid = z.first;
        e.zoom_baked = z.second;
        t.max_slice_delta = std::max(t.max_slice_delta, e.slice_delta);
        t.max_units = std::max(t.max_units, e.units);
        if (e.zoom_valid && t.adopt_step < 0)
            t.adopt_step = static_cast<std::int64_t>(t.steps.size());
        if (e.ready) {
            e.hash = hash_of(pixels(*d.fb));
            t.pubs.push_back(t.steps.size());
            t.frames.push_back(pixels(*d.fb));
        }
        t.steps.push_back(e);
        if (hook)
            hook(e, d);
        if (t.steps.size() > 4000000)
            break;
    }
    return t;
}

// ---- Job-driven lifecycles (no runner side effects) ------------------------------------------

struct JobCase {
    const char* name;
    Camera camera;
    RenderQuality quality;
    Layers layers;
    bool stale; // start over a previous frame (preserve_bitmap)
    TerrainScene* terrain;
};

std::vector<Rgb565> stale_seed(int w, int h) {
    std::vector<Rgb565> v(static_cast<std::size_t>(w) * h);
    std::uint32_t s = 0x51A1E;
    for (auto& p : v) {
        s = s * 1664525U + 1013904223U;
        p = static_cast<Rgb565>(0xF800 | ((s >> 12) & 0x7E0)); // never a palette line colour
    }
    return v;
}

// `start` replaces the default starting frame; `places` replays RefineRunner::step's marker hook
// (clip to the published rows, draw, restore the clip) so a bare job can be compared with it.
Driver job_driver(World& w, const JobCase& c, std::size_t line_list_limit, bool terrain_only,
                  const Pixels* start = nullptr, const PlaceScene* places = nullptr) {
    struct State {
        std::optional<Framebuffer> fb;
        std::optional<Framebuffer> snap;
        std::optional<RefinementJob> job;
    };
    auto st = std::make_shared<State>();
    st->fb = Framebuffer::create(c.camera.width, c.camera.height);
    st->snap = Framebuffer::create(c.camera.width, c.camera.height);
    if (start != nullptr) {
        std::memcpy(st->fb->data(), start->data(), st->fb->byte_size());
    } else if (c.stale) {
        const auto seed = stale_seed(c.camera.width, c.camera.height);
        std::memcpy(st->fb->data(), seed.data(), st->fb->byte_size());
    } else {
        st->fb->clear(0xF81F);
    }
    RenderQuality q = c.quality;
    q.preserve_bitmap = c.stale;
    q.line_list_limit = line_list_limit;
    Layers layers = c.layers;
    if (terrain_only) {
        q.lines_first = false;
        q.terrain_only = true;
        layers = Layers{false, false, false};
    }
    st->job.emplace(c.camera, *w.scene, kDefaultPalette, layers, q, 1, c.terrain);
    st->job->set_snapshot(&*st->snap);
    Driver d;
    d.fb = &*st->fb;
    d.initial = pixels(*st->fb);
    d.keep = st;
    d.step = [st, places, camera = c.camera](unsigned b) {
        const RenderStep r = st->job->step(*st->fb, 1, b);
        if (r.band_ready && places != nullptr) {
            st->fb->clip_rows(r.first_row, r.end_row);
            places->draw(*st->fb, camera);
            st->fb->clip_rows(0, st->fb->height());
        }
        return r;
    };
    d.active = [st] { return st->job->active(); };
    d.progress = [st] { return st->job->progress(); };
    d.slices = [st] { return st->job->stats().terrain_slices; };
    d.zoom = [st] { return std::pair<bool, bool>{st->job->snapshot_taken(), false}; };
    d.snapshot_pixels = [st] { return pixels(*st->snap); };
    return d;
}

// ---- Runner-driven lifecycles (real Model, RefineRunner, ZoomPreview, markers) ----------------

struct Session {
    Model m = initial_model(120, 90);
    RefineRunner runner;
    std::optional<Framebuffer> fb;
    TerrainScene* terrain = nullptr;
    std::uint64_t clock = 100;
    Effects settle() {
        update(m, Tick{clock + 250});
        return update(m, Tick{clock + 251});
    }
    void finish_levels() {
        while (m.refine_active)
            update(m, RefinementFinished{m.generation, m.refine_level});
    }
};

// The zoom cache as the runner would show it: a Moving-stage frame at the same view (factor 1).
// Fresh lines are drawn over an unbaked cache, so only a baked cache is returned exactly.
std::optional<Pixels> read_cache(Session& s, bool* baked) {
    *baked = s.runner.zoom().baked();
    if (!s.runner.zoom().valid())
        return std::nullopt;
    update(s.m, Wheel{s.clock + 5000, 0});
    auto scratch = Framebuffer::create(s.fb->width(), s.fb->height());
    if (!s.runner.render_moving(*scratch, s.m, s.terrain))
        return std::nullopt;
    return pixels(*scratch);
}

// A session whose runner is about to run its first (fresh) or a preserved (zoom over preview)
// refinement. The returned session has already called runner.start().
std::shared_ptr<Session> make_session(World& w, bool preserved, bool places = true) {
    auto s = std::make_shared<Session>();
    s->terrain = &*w.terrain;
    s->runner.set_scene(*w.scene);
    if (places)
        s->runner.set_places(&*w.place_scene);
    s->fb = Framebuffer::create(120, 90);
    update(s->m, RegisterElevation{0, 360, 180});
    update(s->m, Wheel{100, 0});
    s->runner.render_moving(*s->fb, s->m, s->terrain);
    s->settle();
    s->runner.start(s->m, &*s->fb, s->terrain);
    if (preserved) {
        while (s->runner.active())
            s->runner.step(*s->fb, s->m, 256);
        s->finish_levels();
        s->clock = 1000;
        update(s->m, Wheel{1000, 2});
        s->runner.render_moving(*s->fb, s->m, s->terrain); // the zoom preview is on screen
        s->settle();
        s->runner.start(s->m, &*s->fb, s->terrain);
    }
    return s;
}

Driver runner_driver(const std::shared_ptr<Session>& s) {
    Driver d;
    d.fb = &*s->fb;
    d.keep = s;
    d.initial = pixels(*s->fb);
    d.step = [s](unsigned b) { return s->runner.step(*s->fb, s->m, b); };
    d.active = [s] { return s->runner.active(); };
    d.progress = [s] { return s->runner.job() != nullptr ? s->runner.job()->progress() : 0.0; };
    d.slices = [s] {
        return s->runner.job() != nullptr ? s->runner.job()->stats().terrain_slices : 0U;
    };
    d.zoom = [s] {
        return std::pair<bool, bool>{s->runner.zoom().valid(), s->runner.zoom().baked()};
    };
    return d;
}

// ---- Analysis helpers ------------------------------------------------------------------------

bool same_publications(const Trace& a, const Trace& b, bool with_units, std::string* why) {
    if (a.pubs.size() != b.pubs.size()) {
        *why = "count " + std::to_string(a.pubs.size()) + " vs " + std::to_string(b.pubs.size());
        return false;
    }
    for (std::size_t i = 0; i < a.pubs.size(); ++i) {
        const Ev& x = a.steps[a.pubs[i]];
        const Ev& y = b.steps[b.pubs[i]];
        const bool same = x.hash == y.hash && x.first == y.first && x.end == y.end &&
                          x.active_after == y.active_after && x.progress == y.progress &&
                          x.slices == y.slices && x.zoom_valid == y.zoom_valid &&
                          x.zoom_baked == y.zoom_baked &&
                          (!with_units || x.cum_units == y.cum_units);
        if (!same) {
            *why = "publication " + std::to_string(i + 1) + " differs";
            return false;
        }
    }
    return true;
}

// Mid-state hypothesis H-M: between publication i and i+1, every pixel of every step state is
// one of {publication i, publication i+1, the terrain-only frame of the pass in progress}. For
// the state before publication 1: {the starting frame, the cleared frame, publication 1}.
struct Membership {
    std::uint64_t states = 0, changed_states = 0, pixel_checks = 0, violations = 0;
    std::uint64_t prep_states = 0, prep_unchanged = 0;
    std::vector<std::pair<int, int>> sample; // a few violating (publication index, pixel)
    std::uint64_t on_marker = 0, on_line = 0, on_other = 0;
};
struct Mask {
    std::vector<bool> on;
    Pixels value;
    std::size_t count = 0;
};

Membership validate_membership(const Trace& pubs, const Trace& terrain_only, Driver& d,
                               const Pixels& cleared, const Trace* bare = nullptr,
                               const Mask* lines = nullptr, const Mask* markers = nullptr) {
    Membership m;
    Pixels previous = d.initial;
    std::size_t emitted = 0;
    std::uint32_t slices_seen = 0;
    bool first_slice_seen = false;
    while (d.active()) {
        const RenderStep r = d.step(1);
        const Pixels now = pixels(*d.fb);
        ++m.states;
        const bool changed = std::memcmp(now.data(), previous.data(), now.size() * 2) != 0;
        const std::uint32_t slices = d.slices();
        if (emitted >= 1 && !first_slice_seen) {
            ++m.prep_states; // between publication 1 and the first terrain slice
            m.prep_unchanged += !changed;
        }
        if (slices != slices_seen)
            first_slice_seen = true;
        slices_seen = slices;
        if (changed && !r.band_ready) {
            ++m.changed_states;
            const Pixels& p_i = emitted == 0 ? d.initial : pubs.frames[emitted - 1];
            const Pixels& p_next = emitted < pubs.frames.size() ? pubs.frames[emitted] : p_i;
            const Pixels& t =
                emitted == 0
                    ? cleared
                    : terrain_only.frames[std::min(emitted - 1, terrain_only.frames.size() - 1)];
            const Pixels* b_next =
                bare != nullptr && emitted < bare->frames.size() ? &bare->frames[emitted] : nullptr;
            for (std::size_t px = 0; px < now.size(); ++px) {
                ++m.pixel_checks;
                if (now[px] != p_i[px] && now[px] != p_next[px] && now[px] != t[px] &&
                    (b_next == nullptr || now[px] != (*b_next)[px])) {
                    ++m.violations;
                    if (markers != nullptr && markers->on[px])
                        ++m.on_marker;
                    else if (lines != nullptr && lines->on[px])
                        ++m.on_line;
                    else
                        ++m.on_other;
                    if (m.sample.size() < 4)
                        m.sample.emplace_back(static_cast<int>(emitted), static_cast<int>(px));
                }
            }
        }
        if (r.band_ready)
            ++emitted;
        previous = now;
    }
    return m;
}

Pixels cleared_frame(const Camera& c, const Palette& p) {
    auto fb = Framebuffer::create(c.width, c.height);
    fb->clear(p.background);
    fill_disc(*fb, c.center_x, c.center_y, c.radius_px, p.ocean);
    return pixels(*fb);
}

// Pixels that differ between a frame with the lines and the same frame without (no terrain), drawn
// with the quality (chord length, look) of the job being analysed.
Mask line_mask(World& w, const Camera& camera, const RenderQuality& job_quality) {
    Mask m;
    RenderQuality q{};
    q.terrain_style = job_quality.terrain_style;
    q.max_chord_px = job_quality.max_chord_px;
    Palette palette = kDefaultPalette;
    if (q.terrain_style != TerrainStyle::Map)
        palette.border = rgb565(0, 0, 0); // what a terrain job does to the border colour
    auto with = Framebuffer::create(camera.width, camera.height);
    auto without = Framebuffer::create(camera.width, camera.height);
    RefinementJob a(camera, *w.scene, palette, Layers{}, q, 1, nullptr);
    RefinementJob b(camera, *w.scene, palette, Layers{false, false, false}, q, 1, nullptr);
    while (a.active())
        a.step(*with, 1, 256);
    while (b.active())
        b.step(*without, 1, 256);
    m.value = pixels(*with);
    const Pixels base = pixels(*without);
    m.on.assign(base.size(), false);
    for (std::size_t i = 0; i < base.size(); ++i)
        if (m.value[i] != base[i]) {
            m.on[i] = true;
            ++m.count;
        }
    return m;
}

Mask marker_mask(World& w, const Camera& camera) {
    Mask m;
    auto fb = Framebuffer::create(camera.width, camera.height);
    fb->clear(0x1234);
    w.place_scene->draw(*fb, camera);
    m.value = pixels(*fb);
    m.on.assign(m.value.size(), false);
    for (std::size_t i = 0; i < m.value.size(); ++i)
        if (m.value[i] != 0x1234) {
            m.on[i] = true;
            ++m.count;
        }
    return m;
}

// ---- Observations ----------------------------------------------------------------------------

void job_case_observations(World& w, const JobCase& c) {
    const std::string tag = c.name;
    // Budget invariance of the publication sequence.
    Trace by_budget[3];
    const unsigned budgets[3] = {1, 7, 64};
    for (int i = 0; i < 3; ++i) {
        Driver d = job_driver(w, c, 0, false);
        by_budget[i] = record(d, budgets[i]);
    }
    const Trace& t64 = by_budget[2];
    std::string why;
    obs((tag + " pubs").c_str(),
        "%zu publications (fresh=%d); final active_after=%d; "
        "units at pub1=%llu",
        t64.pubs.size(), c.stale ? 0 : 1, static_cast<int>(t64.steps[t64.pubs.back()].active_after),
        static_cast<unsigned long long>(t64.steps[t64.pubs.front()].cum_units));
    for (int i = 0; i < 2; ++i) {
        const bool same = same_publications(by_budget[i], t64, true, &why);
        obs((tag + " budget " + std::to_string(budgets[i]) + " vs 64").c_str(), "%s %s",
            same ? "identical publications (frames, rows, progress, slices, units)" : "DIFFERENT",
            same ? "" : why.c_str());
    }
    for (int i = 0; i < 3; ++i)
        obs((tag + " bounds b=" + std::to_string(budgets[i])).c_str(),
            "max units/step %u (budget %u), max slices/step %u, steps %zu", by_budget[i].max_units,
            budgets[i], by_budget[i].max_slice_delta, by_budget[i].steps.size());
    // Sparse list versus scan fallback.
    Trace limited[2];
    const std::size_t limits[2] = {1, 100000};
    for (int i = 0; i < 2; ++i) {
        Driver d = job_driver(w, c, limits[i], false);
        limited[i] = record(d, 64);
    }
    const bool frames_same = same_publications(limited[0], limited[1], false, &why);
    const bool units_same = same_publications(limited[0], limited[1], true, &why);
    obs((tag + " scan vs sparse").c_str(),
        "frames/progress/slices %s; work units identical: %s (units at final pub: scan %llu, "
        "sparse %llu)",
        frames_same ? "identical" : "DIFFERENT", units_same ? "yes" : "no",
        static_cast<unsigned long long>(limited[0].steps[limited[0].pubs.back()].cum_units),
        static_cast<unsigned long long>(limited[1].steps[limited[1].pubs.back()].cum_units));
    // Progress at publication points.
    std::string series;
    bool monotone = true;
    double last = 0;
    for (const auto idx : t64.pubs) {
        char text[24];
        std::snprintf(text, sizeof text, "%.4f ", t64.steps[idx].progress);
        series += text;
        monotone = monotone && t64.steps[idx].progress >= last;
        last = t64.steps[idx].progress;
    }
    obs((tag + " progress@pubs").c_str(), "monotone=%d: %s", static_cast<int>(monotone),
        series.c_str());
    // Hypothesis H-M on the job's own states at budget 1.
    Driver terrain_only = job_driver(w, c, 0, true);
    const Trace t_only = record(terrain_only, 64);
    Driver walker = job_driver(w, c, 0, false);
    const Pixels cleared =
        c.stale
            ? walker.initial
            : cleared_frame(c.camera, palette_for_style(kDefaultPalette, c.quality.terrain_style));
    const Mask lines = line_mask(w, c.camera, c.quality);
    const Membership m =
        validate_membership(t64, t_only, walker, cleared, nullptr, &lines, nullptr);
    CHECK(m.states > 1000 || c.camera.width < 100);
    obs((tag + " H-M membership").c_str(),
        "%llu states, %llu changed mid-state frames, %llu pixel checks, %llu outside "
        "{pub_i, pub_i+1, terrain-only} (%llu on line pixels, %llu elsewhere); prep: %llu of %llu "
        "states left the frame unchanged",
        static_cast<unsigned long long>(m.states),
        static_cast<unsigned long long>(m.changed_states),
        static_cast<unsigned long long>(m.pixel_checks),
        static_cast<unsigned long long>(m.violations), static_cast<unsigned long long>(m.on_line),
        static_cast<unsigned long long>(m.on_other),
        static_cast<unsigned long long>(m.prep_unchanged),
        static_cast<unsigned long long>(m.prep_states));
    for (const auto& [pub, px] : m.sample)
        obs((tag + " H-M violation").c_str(), "after publication %d at (%d,%d)", pub,
            px % c.camera.width, px / c.camera.width);
    // Lines-first: the units before the first publication, and line pixels in every publication.
    std::size_t intact_all = 0;
    std::string missing;
    for (std::size_t k = 0; k < t64.frames.size(); ++k) {
        std::size_t intact = 0;
        for (std::size_t px = 0; px < lines.on.size(); ++px)
            intact += lines.on[px] && t64.frames[k][px] == lines.value[px];
        if (intact == lines.count)
            ++intact_all;
        else
            missing += " #" + std::to_string(k + 1) + ":" + std::to_string(lines.count - intact);
    }
    obs((tag + " lines in pubs").c_str(),
        "%zu line pixels; %zu of %zu publications have all of "
        "them; missing counts by publication:%s",
        lines.count, intact_all, t64.frames.size(), missing.empty() ? " none" : missing.c_str());
}

// Units before the first publication for two terrain sizes (lines-first should not depend on it).
void lines_first_timing(World& w) {
    const auto camera = *make_camera({{20, 10}, 0, 45, 100, 81});
    RenderQuality q{1.5, true, 0, TerrainStyle::Map};
    q.lines_first = true;
    std::uint64_t units[2] = {0, 0};
    TerrainScene* grids[2] = {&*w.contrast_terrain, &*w.terrain};
    for (int i = 0; i < 2; ++i) {
        JobCase c{"timing", camera, q, Layers{}, false, grids[i]};
        Driver d = job_driver(w, c, 0, false);
        const Trace t = record(d, 64);
        units[i] = t.steps[t.pubs.front()].cum_units;
    }
    obs("lines-first units to pub 1",
        "24x12 grid: %llu units; 1-degree grid: %llu units; "
        "terrain cells %zu vs %zu",
        static_cast<unsigned long long>(units[0]), static_cast<unsigned long long>(units[1]),
        w.contrast_terrain->cell_count(), w.terrain->cell_count());
}

void runner_observations(World& w, bool preserved) {
    const std::string tag = preserved ? "runner preserved" : "runner fresh";
    Trace by_budget[3];
    const unsigned budgets[3] = {1, 7, 64};
    for (int i = 0; i < 3; ++i) {
        auto s = make_session(w, preserved);
        Driver d = runner_driver(s);
        by_budget[i] = record(d, budgets[i]);
        const bool was_active = s->m.refine_active;
        s->finish_levels();
        if (i == 2) {
            // The Model-visible ordering: the job ended, then RefinementFinished retires it.
            obs((tag + " model").c_str(),
                "refine_active before RefinementFinished=%d, after=%d; runner.active=%d",
                static_cast<int>(was_active), static_cast<int>(s->m.refine_active),
                static_cast<int>(s->runner.active()));
        }
    }
    const Trace& t = by_budget[2];
    std::string why;
    obs((tag + " pubs").c_str(), "%zu publications; final: active_after=%d first=%d end=%d",
        t.pubs.size(), static_cast<int>(t.steps[t.pubs.back()].active_after),
        t.steps[t.pubs.back()].first, t.steps[t.pubs.back()].end);
    for (int i = 0; i < 2; ++i) {
        const bool same = same_publications(by_budget[i], t, true, &why);
        obs((tag + " budget " + std::to_string(budgets[i]) + " vs 64").c_str(), "%s %s",
            same ? "identical publications (frames incl. markers, rows, progress, zoom state)"
                 : "DIFFERENT",
            same ? "" : why.c_str());
    }
    // After the final publication nothing more is returned.
    {
        auto s = make_session(w, preserved);
        Driver d = runner_driver(s);
        (void)record(d, 64);
        bool quiet = true;
        const auto before = hash_of(pixels(*s->fb));
        for (int i = 0; i < 3; ++i) {
            const RenderStep r = s->runner.step(*s->fb, s->m, 64);
            quiet = quiet && r.work_units == 0 && !r.band_ready;
        }
        obs((tag + " after final").c_str(),
            "later steps return no work and no publication: %d; "
            "frame unchanged: %d",
            static_cast<int>(quiet), static_cast<int>(hash_of(pixels(*s->fb)) == before));
    }
    // Snapshot adoption and the final publication.
    {
        std::int64_t zoom_first = t.adopt_step;
        std::size_t pubs_before = 0;
        for (const auto idx : t.pubs)
            pubs_before += static_cast<std::int64_t>(idx) < zoom_first;
        obs((tag + " snapshot").c_str(),
            "cache becomes valid at step %lld of %zu (cumulative units %llu); publications "
            "returned before then: %zu of %zu; baked=%d",
            static_cast<long long>(zoom_first), t.steps.size(),
            static_cast<unsigned long long>(
                zoom_first >= 0 ? t.steps[static_cast<std::size_t>(zoom_first)].cum_units : 0),
            pubs_before, t.pubs.size(),
            zoom_first >= 0
                ? static_cast<int>(t.steps[static_cast<std::size_t>(zoom_first)].zoom_baked)
                : -1);
    }
    // What the snapshot contains, and how the final frame decomposes. The runner's cache cannot
    // be read exactly through public API (an unbaked cache gets fresh lines drawn over it), so a
    // bare job replays the runner's marker hook (clip, draw, restore: four lines) with its own
    // snapshot buffer. Calibration: that job must reproduce every runner publication.
    {
        auto s = make_session(w, preserved);
        Driver runner_d = runner_driver(s);
        const Camera camera = s->m.camera;
        const Palette palette = palette_for_style(kDefaultPalette, s->m.terrain_style);
        const Pixels start = runner_d.initial;
        RenderQuality q = quality_for(s->m);
        q.preserve_bitmap = preserved;
        const JobCase jc{"replay", camera, q, effective_layers(s->m), preserved, &*w.terrain};
        const Trace run = record(runner_d, 64);
        Driver replay = job_driver(w, jc, 0, false, &start, &*w.place_scene);
        const Trace rep = record(replay, 64);
        bool calibrated = rep.frames.size() == run.frames.size();
        for (std::size_t k = 0; calibrated && k < run.frames.size(); ++k)
            calibrated = rep.frames[k] == run.frames[k];
        obs((tag + " replay calibration").c_str(),
            "bare job + marker hook reproduces all %zu runner publications: %d", run.frames.size(),
            static_cast<int>(calibrated));
        std::size_t pubs_before_adopt = 0;
        for (const auto idx : rep.pubs)
            pubs_before_adopt += static_cast<std::int64_t>(idx) < rep.adopt_step;
        obs((tag + " snapshot taken").c_str(),
            "job's snapshot_taken() first true at step %lld of %zu (cumulative units %llu); "
            "publications returned before then: %zu of %zu; final publication at units %llu",
            static_cast<long long>(rep.adopt_step), rep.steps.size(),
            static_cast<unsigned long long>(
                rep.adopt_step >= 0 ? rep.steps[static_cast<std::size_t>(rep.adopt_step)].cum_units
                                    : 0),
            pubs_before_adopt, rep.pubs.size(),
            static_cast<unsigned long long>(rep.steps[rep.pubs.back()].cum_units));
        const Pixels snapshot = replay.snapshot_pixels();
        const Pixels final_frame = run.frames.back();
        // Terrain-only reference over the same starting frame.
        RenderQuality tq = q;
        tq.lines_first = false;
        tq.terrain_only = true;
        const JobCase tc{"terrain-only", camera,     tq, Layers{false, false, false},
                         preserved,      &*w.terrain};
        Driver terrain_d = job_driver(w, tc, 0, true, &start);
        const Trace t_only = record(terrain_d, 64);
        const Pixels terrain_final = t_only.frames.back();
        const Mask lines = line_mask(w, camera, q);
        const Mask markers = marker_mask(w, camera);
        std::size_t differ = 0, on_lines = 0, on_markers = 0, other = 0, lines_intact = 0;
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            lines_intact += lines.on[i] && snapshot[i] == lines.value[i];
            if (snapshot[i] == terrain_final[i])
                continue;
            ++differ;
            if (markers.on[i])
                ++on_markers;
            else if (lines.on[i])
                ++on_lines;
            else
                ++other;
        }
        obs((tag + " snapshot content").c_str(),
            "snapshot differs from the terrain-only final at %zu px: %zu on city-marker pixels, "
            "%zu on line pixels, %zu elsewhere; %zu of %zu line pixels are present in it",
            differ, on_markers, on_lines, other, lines_intact, lines.count);
        // Final frame model: terrain, then lines, then markers.
        Pixels model = terrain_final;
        for (std::size_t i = 0; i < model.size(); ++i) {
            if (lines.on[i])
                model[i] = lines.value[i];
            if (markers.on[i])
                model[i] = markers.value[i];
        }
        std::size_t off = 0;
        for (std::size_t i = 0; i < model.size(); ++i)
            off += model[i] != final_frame[i];
        obs((tag + " final composition").c_str(),
            "final frame equals terrain + line stamp + markers at all but %zu px", off);
        std::string frac;
        for (std::size_t k = 0; k < run.frames.size(); ++k) {
            std::size_t intact = 0;
            for (std::size_t i = 0; i < markers.on.size(); ++i)
                intact += markers.on[i] && run.frames[k][i] == markers.value[i];
            char text[24];
            std::snprintf(text, sizeof text, " %zu", intact);
            frac += text;
        }
        obs((tag + " markers/pub").c_str(), "of %zu marker pixels intact at each publication:%s",
            markers.count, frac.c_str());
        // Hypothesis H-M with markers, through the runner. Each walker's job is created right
        // before it is walked: creating a job claims the shared terrain workspace.
        Driver bare_d = job_driver(w, jc, 0, false, &start, nullptr);
        const Trace bare = record(bare_d, 64);
        const Pixels cleared = preserved ? start : cleared_frame(camera, palette);
        auto session2 = make_session(w, preserved);
        Driver walker2 = runner_driver(session2);
        const Membership three =
            validate_membership(run, t_only, walker2, cleared, nullptr, &lines, &markers);
        auto session3 = make_session(w, preserved);
        Driver walker3 = runner_driver(session3);
        const Membership m =
            validate_membership(run, t_only, walker3, cleared, &bare, &lines, &markers);
        CHECK(three.states > 1000 && m.states > 1000 && calibrated);
        obs((tag + " H-M (3-set)").c_str(),
            "%llu changed mid-state frames; %llu of %llu pixel "
            "checks outside {pub_i, pub_i+1, terrain-only} with markers present (%llu on marker "
            "px)",
            static_cast<unsigned long long>(three.changed_states),
            static_cast<unsigned long long>(three.violations),
            static_cast<unsigned long long>(three.pixel_checks),
            static_cast<unsigned long long>(three.on_marker));
        obs((tag + " H-M (+pre-marker pub)").c_str(),
            "%llu states, %llu changed mid-state frames, %llu pixel checks, %llu outside "
            "{pub_i, pub_i+1, terrain-only, pre-marker pub_i+1} (%llu on marker px, %llu on line "
            "px, %llu elsewhere)",
            static_cast<unsigned long long>(m.states),
            static_cast<unsigned long long>(m.changed_states),
            static_cast<unsigned long long>(m.pixel_checks),
            static_cast<unsigned long long>(m.violations),
            static_cast<unsigned long long>(m.on_marker),
            static_cast<unsigned long long>(m.on_line),
            static_cast<unsigned long long>(m.on_other));
    }
    // Bounds per budget.
    for (int i = 0; i < 3; ++i)
        obs((tag + " bounds b=" + std::to_string(budgets[i])).c_str(),
            "max units/step %u, max slices/step %u, steps %zu", by_budget[i].max_units,
            by_budget[i].max_slice_delta, by_budget[i].steps.size());
}

// ShownFrame at every publication, and a simulated Classic capture a few steps later.
void presentation_observations(World& w) {
    Trace plain, shown;
    {
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        plain = record(d, 64);
    }
    std::size_t frames_changed_by_shown = 0, bar_rows_differ = 0;
    {
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        Overlays overlays;
        ShownFrame shown_frame;
        shown = record(d, 64, [&](const Ev& e, Driver&) {
            if (!e.ready)
                return;
            const Pixels before = pixels(*s->fb);
            const bool settled = frame_settled(s->m, s->runner.active());
            const double bar = bar_progress(s->m, s->runner.job());
            const Framebuffer& view =
                shown_frame.begin(overlays, *s->fb, s->m.camera, 0, settled, bar);
            const Pixels seen = pixels(view);
            bar_rows_differ += seen != before;
            shown_frame.end();
            frames_changed_by_shown += pixels(*s->fb) != before;
        });
    }
    std::string why;
    obs("ShownFrame",
        "frame unchanged after end() at every publication: %d; shown frame differs "
        "from the rendered one at %zu of %zu publications; trace unchanged by presenting: %d",
        static_cast<int>(frames_changed_by_shown == 0), bar_rows_differ, shown.pubs.size(),
        static_cast<int>(same_publications(plain, shown, true, &why)));
    // A Classic-style capture K steps after a publication: is it the published frame?
    for (const int lag : {0, 5, 16}) {
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        std::size_t total = 0, equal = 0;
        std::vector<std::pair<int, std::uint64_t>> pending; // steps left, publication hash
        (void)record(d, 64, [&](const Ev& e, Driver&) {
            for (auto& p : pending)
                --p.first;
            for (auto it = pending.begin(); it != pending.end();) {
                if (it->first < 0) {
                    ++total;
                    equal += hash_of(pixels(*s->fb)) == it->second;
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
            if (e.ready) {
                if (lag == 0) {
                    ++total;
                    ++equal;
                } else {
                    pending.emplace_back(lag, e.hash);
                }
            }
        });
        obs("Classic-style capture lag",
            "capture %2d steps after a publication equals that "
            "publication in %zu of %zu cases (steps of 64 units)",
            lag, equal, total);
    }
    // Progress between publications (what a late capture's bar would show).
    {
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        double lowest = 2, highest = -1;
        std::size_t distinct_between = 0;
        double last_pub_progress = -1;
        (void)record(d, 64, [&](const Ev& e, Driver&) {
            if (e.ready) {
                last_pub_progress = e.progress;
            } else if (e.progress != last_pub_progress) {
                ++distinct_between;
            }
            lowest = std::min(lowest, e.progress);
            highest = std::max(highest, e.progress);
        });
        obs("progress between pubs",
            "progress() differs from the last publication's value in %zu "
            "non-publication steps (range %.3f..%.3f)",
            distinct_between, lowest, highest);
    }
}

// keep_partial and cancel semantics at selected points.
void cancel_observations(World& w, bool preserved) {
    struct Point {
        const char* name;
        std::function<bool(const Trace&, const Ev&)> stop;
    };
    const auto pubs_seen = [](const Trace& t) { return t.pubs.size(); };
    const std::vector<Point> points = {
        {"after pub 1 (lines only)",
         [&](const Trace& t, const Ev& e) { return e.ready && pubs_seen(t) == 1; }},
        {"during terrain prep",
         [&](const Trace& t, const Ev& e) {
             return pubs_seen(t) == 1 && e.slices == 0 && t.steps.size() > 40;
         }},
        {"after pub 8", [&](const Trace& t, const Ev& e) { return e.ready && pubs_seen(t) == 8; }},
        {"mid ring 6",
         [&](const Trace& t, const Ev& e) {
             return pubs_seen(t) == 7 && !e.ready && e.slice_delta > 0;
         }},
        {"after pub 17",
         [&](const Trace& t, const Ev& e) { return e.ready && pubs_seen(t) == 17; }},
        {"mid limb pass",
         [&](const Trace& t, const Ev& e) {
             return pubs_seen(t) == 17 && !e.ready && e.slice_delta > 0;
         }},
        {"snapshot taken, final stamp pending",
         [&](const Trace& t, const Ev& e) {
             return e.zoom_valid && pubs_seen(t) < 18 && !e.ready;
         }},
        {"after final publication",
         [&](const Trace&, const Ev& e) { return e.ready && !e.active_after; }},
    };
    for (const Point& point : points) {
        auto s = make_session(w, preserved);
        Driver d = runner_driver(s);
        Trace partial;
        Trace* tp = &partial;
        std::size_t hits = 0;
        bool stopped = false;
        std::uint64_t cum = 0;
        std::uint32_t last_slices = 0;
        while (d.active() && !stopped) {
            const RenderStep r = d.step(7);
            Ev e;
            cum += r.work_units;
            e.cum_units = cum;
            e.ready = r.band_ready;
            e.active_after = d.active();
            e.slices = d.slices();
            e.slice_delta = e.slices - last_slices;
            last_slices = e.slices;
            const auto z = d.zoom();
            e.zoom_valid = z.first;
            e.zoom_baked = z.second;
            if (e.ready) {
                tp->pubs.push_back(tp->steps.size());
            }
            tp->steps.push_back(e);
            if (point.stop(*tp, e)) {
                ++hits;
                stopped = true;
            }
        }
        if (!stopped) {
            obs("cancel", "%s %s: point not reached", preserved ? "preserved" : "fresh",
                point.name);
            continue;
        }
        const Pixels live = pixels(*s->fb);
        const bool valid_before = s->runner.zoom().valid();
        const bool baked_before = s->runner.zoom().baked();
        s->runner.cancel(&*s->fb, s->m.camera);
        const bool valid_after = s->runner.zoom().valid();
        const bool baked_after = s->runner.zoom().baked();
        const bool changed_by_cancel = valid_before != valid_after || baked_before != baked_after;
        // Later steps must neither work nor change the frame.
        const RenderStep later = s->runner.step(*s->fb, s->m, 64);
        const bool inert = later.work_units == 0 && !later.band_ready && pixels(*s->fb) == live;
        CHECK(inert); // pinned by test_line_layer and test_terrain: a cancelled job is inert
        bool baked_read = false;
        const auto cache = read_cache(*s, &baked_read);
        const char* relation = "n/a";
        if (cache.has_value() && baked_read)
            relation = *cache == live ? "cache == live frame at cancel" : "cache != live frame";
        else if (cache.has_value())
            relation = "unbaked (finished snapshot + fresh lines)";
        obs("cancel",
            "%-9s %-36s cache valid/baked before %d/%d after %d/%d (%s); %s; later "
            "step inert: %d",
            preserved ? "preserved" : "fresh", point.name, static_cast<int>(valid_before),
            static_cast<int>(baked_before), static_cast<int>(valid_after),
            static_cast<int>(baked_after),
            changed_by_cancel ? "cancel changed it" : "cancel left it", relation,
            static_cast<int>(inert));
        (void)hits;
    }
}

// A generation change through the Model, with and without the runner being told.
void generation_observations(World& w) {
    {
        // Platforms call runner.cancel when the generation changes; here the camera moves first.
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        for (int i = 0; i < 400 && d.active(); ++i)
            d.step(64);
        const Pixels live = pixels(*s->fb);
        const std::uint64_t generation = s->m.generation;
        update(*&s->m, Wheel{3000, 1});
        const Camera now = s->m.camera;
        s->runner.cancel(&*s->fb, now);
        const Palette palette = palette_for_style(kDefaultPalette, s->m.terrain_style);
        auto scratch = Framebuffer::create(120, 90);
        const bool preview = s->runner.render_moving(*scratch, s->m, &*w.terrain);
        obs("generation change (told)",
            "generation %llu -> %llu; runner inactive=%d; cache valid=%d "
            "baked=%d; the next moving frame used the zoom preview: %d (a partial frame can reach "
            "the screen through the preview)",
            static_cast<unsigned long long>(generation),
            static_cast<unsigned long long>(s->m.generation), static_cast<int>(!s->runner.active()),
            static_cast<int>(s->runner.zoom().valid()), static_cast<int>(s->runner.zoom().baked()),
            static_cast<int>(preview));
        (void)live;
        (void)palette;
    }
    {
        // The model's generation advances but nobody calls runner.cancel.
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        for (int i = 0; i < 400 && d.active(); ++i)
            d.step(64);
        const Pixels live = pixels(*s->fb);
        update(s->m, Wheel{3000, 1});
        const RenderStep r = s->runner.step(*s->fb, s->m, 64);
        obs("generation change (not told)",
            "next step: %u units, publication=%d, job inactive=%d, "
            "frame unchanged=%d, zoom cache valid=%d (keep_partial was never called)",
            r.work_units, static_cast<int>(r.band_ready), static_cast<int>(!s->runner.active()),
            static_cast<int>(pixels(*s->fb) == live), static_cast<int>(s->runner.zoom().valid()));
    }
    {
        // A second runner takes the terrain workspace while the first is mid-job.
        auto s = make_session(w, false);
        Driver d = runner_driver(s);
        for (int i = 0; i < 400 && d.active(); ++i)
            d.step(64);
        const Pixels live = pixels(*s->fb);
        RefineRunner other(*w.scene);
        Model m2 = initial_model(120, 90);
        update(m2, RegisterElevation{0, 360, 180});
        update(m2, Wheel{100, 0});
        update(m2, Tick{350});
        update(m2, Tick{351});
        auto fb2 = Framebuffer::create(120, 90);
        other.start(m2, &*fb2, &*w.terrain);
        const RenderStep r = s->runner.step(*s->fb, s->m, 64);
        obs("workspace reuse",
            "first runner after a second job claimed the workspace: %u units, "
            "publication=%d, inactive=%d, frame unchanged=%d",
            r.work_units, static_cast<int>(r.band_ready), static_cast<int>(!s->runner.active()),
            static_cast<int>(pixels(*s->fb) == live));
        while (other.active())
            other.step(*fb2, m2, 256);
        auto ref = Framebuffer::create(120, 90);
        RefinementJob direct(m2.camera, *w.scene, kDefaultPalette, effective_layers(m2),
                             quality_for(m2), 99, &*w.terrain);
        while (direct.active())
            direct.step(*ref, 99, 256);
        obs("workspace reuse",
            "second job's final frame equals a from-scratch job: %d (places not "
            "set on the second runner)",
            static_cast<int>(pixels(*fb2) == pixels(*ref)));
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : ".";
    auto world = std::make_unique<World>();
    if (!build_world(*world, root))
        return 1;
    World& w = *world;

    RenderQuality map_q{1.5, true, 0, TerrainStyle::Map};
    map_q.lines_first = true;
    RenderQuality relief_q{1.5, true, 0, TerrainStyle::Relief};
    relief_q.lines_first = true;
    RenderQuality flat_q = relief_q;
    flat_q.smooth_terrain = false;
    const auto cam_a = *make_camera({{20, 10}, 0, 45, 120, 90});
    const auto cam_polar = *make_camera({{-80, 30}, 40, 50, 97, 73});
    const auto cam_limb = *make_camera({{21.8733, 175.4403}, 271.492, 22.957, 97, 65});
    const JobCase cases[] = {
        {"job Map fresh", cam_a, map_q, Layers{}, false, &*w.terrain},
        {"job Map preserved", cam_a, map_q, Layers{}, true, &*w.terrain},
        {"job Relief/flat polar", cam_polar, flat_q, Layers{}, false, &*w.terrain},
        {"job limb-sensitive 24x12", cam_limb, relief_q, Layers{}, false, &*w.contrast_terrain},
    };
    for (const JobCase& c : cases)
        job_case_observations(w, c);
    lines_first_timing(w);
    runner_observations(w, false);
    runner_observations(w, true);
    presentation_observations(w);
    cancel_observations(w, false);
    cancel_observations(w, true);
    generation_observations(w);

    // Contracts the existing tests already pin (test_line_layer: 19 fresh publications, lines
    // first, final frame equals the lines-at-end job; test_zoom: snapshot taken and adopted).
    {
        Driver d = job_driver(w, cases[0], 0, false);
        const Trace t = record(d, 64);
        CHECK(t.pubs.size() == kRefinePasses + 3);
        CHECK(t.steps[t.pubs.front()].cum_units < 5000); // lines come before any terrain set-up
    }
    return TEST_MAIN_RESULT();
}
// NOLINTEND(bugprone-unchecked-optional-access)
