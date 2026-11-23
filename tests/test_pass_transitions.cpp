// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// X1b (EXPERIMENTAL, not approved for merge): pass-transition equivalence.
//
// The production RefinementJob (driven directly, or through RefineRunner) is the only lifecycle.
// It publishes frames F_0 (the starting frame), F_1 ... F_N. This test asks whether each published
// frame is reproduced by a small model applied to the previous published one:
//
//     F_{k+1} = markers( stamp( restore( draw_pass( clear( F_k ) ) ) ) )      (steps as applicable)
//
// where stamp and markers are pure per-pixel overlays and draw_pass draws the cells of one ring
// (or the limb pass) in an order chosen by the test. The model keeps no phase machine, cursor,
// cancel or progress logic; every transition starts from a frame the production code published.
//
// Arm 1 (calibration) uses the cooperative cell order and must reproduce every production
// transition; otherwise nothing else is meaningful. Arm 2 reorders the work inside each pass (band
// order reversed or shuffled, bands interleaved in chunks of four slices) and must give the same
// published frames. Controls (the limb ordering, no pass barrier, stamp/marker order) must detect
// a difference, or the corresponding contract is reported as untested.
//
// Observed behaviour that differs from the intended contract (the snapshot content, the extra
// preserve-mode publication) is reported separately and is not treated as a requirement.
//
//   test_pass_transitions <repo root>
#include "atlas_util.hpp"
#include "check.hpp"
#include "globe/app.hpp"
#include "globe/elevation.hpp"
#include "globe/landclasses.hpp"
#include "globe/places.hpp"
#include "globe/raster.hpp"
#include "globe/refine_runner.hpp"
#include "globe/render.hpp"
#include "globe/terrain.hpp"
#include "globe/zoom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace globe;
using atlas_util::Bytes;

// The harness allocates a handful of small fixed-size buffers once and validates them where they
// are built. Repeating an optional check at each use would obscure what is observed.
// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace {

using Pixels = std::vector<Rgb565>;

Pixels pixels(const Framebuffer& fb) {
    return {fb.data(), fb.data() + static_cast<std::size_t>(fb.width()) * fb.height()};
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

void obs(const std::string& id, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void obs(const std::string& id, const char* fmt, ...) {
    std::printf("OBS %-44s ", id.c_str());
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

struct Rng {
    std::uint32_t state;
    std::uint32_t next() {
        state = state * 1664525U + 1013904223U;
        return state >> 8;
    }
    std::size_t below(std::size_t n) { return next() % n; }
};

template <typename T> void shuffle(std::vector<T>& v, Rng& rng) {
    for (std::size_t i = v.size(); i > 1; --i)
        std::swap(v[i - 1], v[rng.below(i)]);
}

// ---- Fixtures ----------------------------------------------------------------------------------

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
    w.atlas_bytes = atlas_util::make_atlas(polys, "x1b lines");
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
    // The 24x12 contrast grid: X0's limb-ordering sensitive fixture.
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

// ---- What production published ------------------------------------------------------------------

struct Case {
    const char* name;
    bool runner;    // through RefineRunner (Model, city markers) or a bare RefinementJob
    bool preserved; // the second refinement over a zoom preview (runner cases only)
    Camera camera;  // bare-job cases
    RenderQuality quality;
    TerrainScene* terrain;
    bool snapshot_control = false;    // limb order must show in the snapshot
    bool publication_control = false; // limb order must show in a published frame
};

struct Production {
    Camera camera{};
    RenderQuality quality;
    Layers layers;
    TerrainScene* terrain = nullptr;
    bool markers = false, preserved = false;
    Pixels start;               // F_0
    std::vector<Pixels> frames; // F_1 .. F_N, as returned (after the runner's marker draw)
    Pixels snapshot;            // what the job copied into its snapshot buffer
    bool replay_calibrated = true;
    bool budget_invariant = true;
};

struct Session {
    Model m = initial_model(120, 90);
    RefineRunner runner;
    std::optional<Framebuffer> fb;
    TerrainScene* terrain = nullptr;
    std::uint64_t clock = 100;
    void settle() {
        update(m, Tick{clock + 250});
        update(m, Tick{clock + 251});
    }
    void finish_levels() {
        while (m.refine_active)
            update(m, RefinementFinished{m.generation, m.refine_level});
    }
};

std::shared_ptr<Session> make_session(World& w, bool preserved) {
    auto s = std::make_shared<Session>();
    s->terrain = &*w.terrain;
    s->runner.set_scene(*w.scene);
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

struct JobRun {
    std::vector<Pixels> frames;
    Pixels snapshot;
};

// A bare production job (optionally replaying RefineRunner's marker hook: clip, draw, restore).
JobRun run_job(World& w, const Production& p, const Pixels& start, bool with_markers,
               unsigned budget, bool terrain_only = false) {
    JobRun r;
    auto fb = Framebuffer::create(p.camera.width, p.camera.height);
    auto snap = Framebuffer::create(p.camera.width, p.camera.height);
    std::memcpy(fb->data(), start.data(), fb->byte_size());
    RenderQuality q = p.quality;
    q.preserve_bitmap = p.preserved;
    Layers layers = p.layers;
    if (terrain_only) {
        q.lines_first = false;
        q.terrain_only = true;
        layers = Layers{false, false, false};
    }
    RefinementJob job(p.camera, *w.scene, kDefaultPalette, layers, q, 1, p.terrain);
    job.set_snapshot(&*snap);
    while (job.active()) {
        const RenderStep step = job.step(*fb, 1, budget);
        if (step.band_ready) {
            if (with_markers) {
                fb->clip_rows(step.first_row, step.end_row);
                w.place_scene->draw(*fb, p.camera);
                fb->clip_rows(0, fb->height());
            }
            r.frames.push_back(pixels(*fb));
        }
    }
    r.snapshot = pixels(*snap);
    return r;
}

Production produce(World& w, const Case& c) {
    Production p;
    p.markers = c.runner;
    p.preserved = c.preserved;
    if (c.runner) {
        auto s = make_session(w, c.preserved);
        p.camera = s->m.camera;
        p.quality = quality_for(s->m);
        p.quality.preserve_bitmap = c.preserved;
        p.layers = effective_layers(s->m);
        p.terrain = &*w.terrain;
        p.start = pixels(*s->fb);
        while (s->runner.active()) {
            const RenderStep r = s->runner.step(*s->fb, s->m, 64);
            if (r.band_ready)
                p.frames.push_back(pixels(*s->fb));
        }
        s->finish_levels();
        // The runner's job is the oracle; a bare job plus the marker hook supplies the snapshot
        // buffer (the runner's own cache cannot be read exactly). It must reproduce every frame.
        const JobRun replay = run_job(w, p, p.start, true, 64);
        p.replay_calibrated = replay.frames == p.frames;
        p.snapshot = replay.snapshot;
        const JobRun slow = run_job(w, p, p.start, true, 7);
        p.budget_invariant = slow.frames == p.frames;
    } else {
        p.camera = c.camera;
        p.quality = c.quality;
        p.quality.lines_first = true;
        p.quality.focus = {p.camera.center_x, p.camera.center_y}; // as quality_for does
        p.layers = Layers{};
        p.terrain = c.terrain;
        p.start.assign(static_cast<std::size_t>(p.camera.width) * p.camera.height, 0xF81F);
        const JobRun run = run_job(w, p, p.start, false, 64);
        p.frames = run.frames;
        p.snapshot = run.snapshot;
        p.budget_invariant = run_job(w, p, p.start, false, 7).frames == p.frames;
    }
    return p;
}

// ---- Pure overlays and the draw model -------------------------------------------------------

struct Mask {
    std::vector<bool> on;
    Pixels value;
    std::size_t count = 0;
};

// Pixels that differ between a frame with the lines and one without (no terrain), drawn with the
// job's chord length and look.
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

struct Prep {
    Camera camera;
    RenderQuality quality;
    Palette palette;
    bool skip;
    int bands() const { return (camera.height + kRefineBandRows - 1) / kRefineBandRows; }
};

Prep make_prep(const Production& p) {
    Prep r;
    r.camera = p.camera;
    r.quality = p.quality;
    r.quality.preserve_bitmap = p.preserved;
    r.palette = palette_for_style(kDefaultPalette, r.quality.terrain_style);
    // The condition RefinementJob::step passes to assign_pass.
    r.skip = r.quality.skip_ocean && !r.quality.preserve_bitmap && r.quality.smooth_terrain &&
             r.quality.terrain_style == TerrainStyle::Map && r.palette.ocean == kMapOcean;
    return r;
}

// What the job's ProjectCorners, BoundCells and BuildPassLists phases leave in the workspace.
// Run only while no production job is active (it does not claim the workspace).
void prepare(TerrainScene& t, const Prep& p) {
    const auto plan = make_pass_plan(p.camera, p.quality.focus);
    for (std::size_t i = 0; i < t.corner_count(); ++i)
        t.project_corner(i, p.camera);
    if (t.has_lists())
        t.begin_lists();
    for (std::size_t i = 0; i < t.cell_count(); ++i) {
        t.bound_cell(i, p.camera.height, kRefineBandRows);
        t.assign_pass(i, plan, p.skip);
    }
    if (t.has_lists()) {
        t.finish_lists();
        for (std::size_t i = 0; i < t.cell_count(); ++i)
            t.place_cell(i);
    }
}

enum class PassKind : std::uint8_t { None, Ring, Limb };

struct Spec {
    bool clear = false;
    PassKind pass = PassKind::None;
    unsigned ring = 0;
    bool restore = false, stamp = false, markers = false;
    unsigned ahead = 0; // control: also draw this many further rings before publishing
};

// The transitions between consecutive published frames, in the order production publishes them.
// `intended_preserve` adds the stamp the restore publication currently lacks.
std::vector<Spec> transitions(bool preserved, bool markers, bool intended_preserve = false) {
    std::vector<Spec> specs;
    specs.push_back({true, PassKind::None, 0, false, true, markers, 0}); // clear, lines, markers
    for (unsigned ring = 0; ring < kRefinePasses; ++ring)
        specs.push_back({false, PassKind::Ring, ring, false, true, markers, 0});
    if (preserved) {
        // Observed: the limb pass, then the restore is published before the lines are stamped.
        specs.push_back({false, PassKind::Limb, 0, true, intended_preserve, markers, 0});
        specs.push_back({false, PassKind::None, 0, false, !intended_preserve, markers, 0});
    } else {
        specs.push_back({false, PassKind::Limb, 0, false, true, markers, 0});
    }
    specs.push_back({false, PassKind::None, 0, false, false, markers, 0}); // Done
    return specs;
}

enum class Order : std::uint8_t {
    Cooperative,   // cell by cell, then bands: production's order
    BandsReversed, // band-major, bands from the bottom
    BandsShuffled, // band-major, bands in a seeded random order
    RoundRobin,    // bands interleaved in chunks of four slices
    CellsReversed, // control: cells of the pass in reverse order
    LimbReversed   // control: limb cells in reverse index order
};
struct Policy {
    Order order;
    std::uint32_t seed;
    const char* name;
};

struct Task {
    std::size_t cell;
    int band;
};

constexpr std::size_t kChunk = 4; // production's cap on cell-slices per continuation

void append_tasks(const TerrainScene& t, const Spec& s, unsigned ring, PassKind kind,
                  const Policy& pol, int bands, std::vector<Task>& out) {
    std::vector<std::size_t> cells;
    if (kind == PassKind::Limb) {
        for (std::size_t i = 0; i < t.cell_count(); ++i)
            if (t.is_limb(i))
                cells.push_back(i);
    } else if (t.has_lists()) {
        for (std::size_t pos = 0; pos < t.pass_count(ring); ++pos)
            cells.push_back(t.pass_cell(ring, pos));
    } else {
        for (std::size_t i = 0; i < t.cell_count(); ++i)
            if (t.cell_pass(i) == ring)
                cells.push_back(i);
    }
    if (pol.order == Order::CellsReversed ||
        (pol.order == Order::LimbReversed && kind == PassKind::Limb))
        std::reverse(cells.begin(), cells.end());
    std::vector<Task> coop;
    for (const std::size_t cell : cells) {
        const auto b = t.cell_bands(cell);
        for (int band = b.first; band < b.end; ++band)
            coop.push_back({cell, band});
    }
    (void)s;
    if (pol.order == Order::BandsReversed || pol.order == Order::BandsShuffled ||
        pol.order == Order::RoundRobin) {
        std::vector<int> order(static_cast<std::size_t>(bands));
        for (int i = 0; i < bands; ++i)
            order[static_cast<std::size_t>(i)] =
                pol.order == Order::BandsReversed ? bands - 1 - i : i;
        if (pol.order != Order::BandsReversed) {
            Rng rng{pol.seed ^ (ring * 2654435761U) ^ static_cast<std::uint32_t>(kind)};
            shuffle(order, rng);
        }
        std::vector<std::vector<Task>> queue(static_cast<std::size_t>(bands));
        for (const Task& task : coop)
            queue[static_cast<std::size_t>(task.band)].push_back(task);
        if (pol.order == Order::RoundRobin) {
            std::vector<std::size_t> next(queue.size(), 0);
            bool more = true;
            while (more) {
                more = false;
                for (const int b : order) {
                    auto& q = queue[static_cast<std::size_t>(b)];
                    auto& n = next[static_cast<std::size_t>(b)];
                    for (std::size_t i = 0; i < kChunk && n < q.size(); ++i)
                        out.push_back(q[n++]);
                    more = more || n < q.size();
                }
            }
        } else {
            for (const int b : order)
                for (const Task& task : queue[static_cast<std::size_t>(b)])
                    out.push_back(task);
        }
    } else {
        out.insert(out.end(), coop.begin(), coop.end());
    }
}

void clear_rows(Framebuffer& fb, const Prep& p) {
    for (int y = 0; y < fb.height(); ++y) {
        if (p.quality.preserve_bitmap) {
            fb.reset_coverage_row(y);
        } else {
            fb.clip_rows(y, y + 1);
            fb.fill_span(y, 0, fb.width() - 1, p.palette.background);
            fill_disc(fb, p.camera.center_x, p.camera.center_y, p.camera.radius_px,
                      p.palette.ocean);
        }
    }
    fb.clip_rows(0, fb.height());
}

void restore_rows(Framebuffer& fb, const Prep& p) {
    for (int y = 0; y < fb.height(); ++y) {
        fb.clip_rows(y, y + 1);
        fb.coverage(Framebuffer::Coverage::Uncovered);
        fb.fill_span(y, 0, fb.width() - 1, p.palette.background);
        fill_disc(fb, p.camera.center_x, p.camera.center_y, p.camera.radius_px, p.palette.ocean);
        fb.coverage(Framebuffer::Coverage::Off);
    }
    fb.clip_rows(0, fb.height());
}

struct Diag {
    std::uint64_t chunks = 0, changed = 0, checks = 0, outside = 0;
};

// Draws the tasks of one transition into `fb`; with `diag`, after every continuation of up to
// kChunk slices every pixel must be the starting pixel or the finished-draw pixel.
void draw_tasks(const TerrainScene& t, const Prep& p, Framebuffer& fb,
                const std::vector<Task>& tasks, Diag* diag, const Pixels* finished) {
    const Pixels begin = diag != nullptr ? pixels(fb) : Pixels{};
    Pixels last = begin;
    std::size_t in_chunk = 0;
    for (const Task& task : tasks) {
        fb.clip_rows(task.band * kRefineBandRows,
                     std::min(fb.height(), (task.band + 1) * kRefineBandRows));
        if (p.quality.preserve_bitmap)
            fb.coverage(Framebuffer::Coverage::Mark);
        t.draw_cell(fb, task.cell, task.band, kRefineBandRows, p.quality.terrain_style,
                    p.quality.smooth_terrain);
        fb.coverage(Framebuffer::Coverage::Off);
        fb.clip_rows(0, fb.height());
        if (diag != nullptr && ++in_chunk == kChunk) {
            in_chunk = 0;
            ++diag->chunks;
            const Pixels now = pixels(fb);
            if (now != last) {
                ++diag->changed;
                for (std::size_t i = 0; i < now.size(); ++i) {
                    ++diag->checks;
                    diag->outside += now[i] != begin[i] && now[i] != (*finished)[i];
                }
                last = now;
            }
        }
    }
}

struct Overlays {
    const Mask* lines = nullptr;
    const PlaceScene* places = nullptr;
    const Camera* camera = nullptr;
    bool markers_first = false; // control: markers before the stamp
};

// One transition: from the published frame `pre`, with `G` carrying the model's own coverage
// plane across a sequence (it is reset by the clear step).
Pixels transition(TerrainScene& t, const Prep& p, Framebuffer& G, const Pixels& pre, const Spec& s,
                  const Policy& pol, const Overlays& o, Diag* diag = nullptr) {
    std::memcpy(G.data(), pre.data(), G.byte_size());
    if (s.clear)
        clear_rows(G, p);
    const auto draw = [&](PassKind kind, unsigned ring) {
        std::vector<Task> tasks;
        append_tasks(t, s, ring, kind, pol, p.bands(), tasks);
        if (diag != nullptr) {
            // The finished draw, from a scratch copy so the model's coverage is untouched.
            auto scratch = Framebuffer::create(G.width(), G.height());
            std::memcpy(scratch->data(), G.data(), G.byte_size());
            draw_tasks(t, p, *scratch, tasks, nullptr, nullptr);
            const Pixels finished = pixels(*scratch);
            draw_tasks(t, p, G, tasks, diag, &finished);
        } else {
            draw_tasks(t, p, G, tasks, nullptr, nullptr);
        }
    };
    if (s.pass != PassKind::None) {
        draw(s.pass, s.ring);
        for (unsigned extra = 1; extra <= s.ahead; ++extra)
            draw(PassKind::Ring, s.ring + extra);
    }
    if (s.restore)
        restore_rows(G, p);
    const auto markers = [&] {
        if (o.places != nullptr)
            o.places->draw(G, *o.camera);
    };
    if (o.markers_first && s.markers)
        markers();
    if (s.stamp)
        for (std::size_t i = 0; i < o.lines->on.size(); ++i)
            if (o.lines->on[i])
                G.data()[i] = o.lines->value[i];
    if (!o.markers_first && s.markers)
        markers();
    return pixels(G);
}

struct Outcome {
    std::size_t transitions = 0, matched = 0;
    std::vector<std::size_t> mismatched;
    bool snapshot_matched = false;
    Diag diag;
};

// Runs every transition of a case under one policy and compares with what production published.
Outcome run_sequence(World& w, const Production& prod, const Prep& p, const Mask& lines,
                     const std::vector<Spec>& specs, const Policy& pol, bool diagnose,
                     bool markers_first = false) {
    Outcome out;
    TerrainScene& t = *prod.terrain;
    prepare(t, p);
    auto G = Framebuffer::create(p.camera.width, p.camera.height);
    Overlays o{&lines, prod.markers ? &*w.place_scene : nullptr, &prod.camera, markers_first};
    for (std::size_t k = 0; k < specs.size() && k < prod.frames.size(); ++k) {
        const Pixels& pre = k == 0 ? prod.start : prod.frames[k - 1];
        const Pixels got =
            transition(t, p, *G, pre, specs[k], pol, o,
                       diagnose && specs[k].pass != PassKind::None ? &out.diag : nullptr);
        ++out.transitions;
        if (got == prod.frames[k])
            ++out.matched;
        else
            out.mismatched.push_back(k + 1);
    }
    // The snapshot is the frame when the final stamp begins. Fresh: the limb pass over F_17.
    // Preserved: the restore publication itself (the markers were drawn after it).
    if (prod.frames.size() >= 17) {
        const Pixels& f17 = prod.frames[16];
        if (prod.preserved) {
            Spec s{false, PassKind::Limb, 0, true, false, prod.markers, 0};
            out.snapshot_matched = transition(t, p, *G, f17, s, pol, o) == prod.snapshot;
        } else {
            Spec s{false, PassKind::Limb, 0, false, false, false, 0};
            out.snapshot_matched = transition(t, p, *G, f17, s, pol, o) == prod.snapshot;
        }
    }
    return out;
}

std::string list(const std::vector<std::size_t>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size() && i < 8; ++i)
        s += (i ? "," : "") + std::to_string(v[i]);
    return s.empty() ? "none" : s;
}

// The zoom preview as RefineRunner::render_moving builds it from a cached frame: scale it about the
// view centre, then draw fresh lines over it (an unbaked cache).
Pixels preview_from(World& w, const Session& s, const Pixels& source, const Camera& before,
                    const Camera& after) {
    auto src = Framebuffer::create(source.empty() ? 1 : after.width, after.height);
    std::memcpy(src->data(), source.data(), src->byte_size());
    auto dst = Framebuffer::create(after.width, after.height);
    const Palette palette = palette_for_style(kDefaultPalette, s.m.terrain_style);
    std::vector<int> columns;
    scale_frame(*src, *dst, after.center_x, after.center_y, after.radius_px / before.radius_px,
                palette.background, palette.ocean, after.radius_px, columns);
    RenderQuality q = quality_for(s.m);
    q.lines_only = true;
    render_globe(*dst, after, *w.scene, kDefaultPalette, effective_layers(s.m), true, q, s.terrain);
    return pixels(*dst);
}

// Behaviour observed today that differs from the intended contract. None of it is a requirement
// for a future executor; it is reported so the comparison with today's renderer is honest.
void observed_versus_intended(World& w, const Case& c, const Production& prod, const Prep& prep,
                              const Mask& lines) {
    const std::string tag = std::string(c.name) + " observed/intended";
    // 1. The snapshot. Intended: terrain only. Observed: whatever is on screen when the final stamp
    //    begins.
    const Pixels terrain_final = run_job(w, prod, prod.start, false, 64, true).frames.back();
    const Mask markers = marker_mask(w, prod.camera);
    std::size_t differ = 0, on_markers = 0, on_lines = 0, other = 0;
    for (std::size_t i = 0; i < prod.snapshot.size(); ++i) {
        if (prod.snapshot[i] == terrain_final[i])
            continue;
        ++differ;
        if (markers.on[i])
            ++on_markers;
        else if (lines.on[i])
            ++on_lines;
        else
            ++other;
    }
    obs(tag + " snapshot",
        "observed snapshot differs from the terrain-only final at %zu px: %zu "
        "city-marker, %zu line, %zu other (intended: 0)",
        differ, on_markers, on_lines, other);
    // 2. Does it carry through to the zoom preview? The same preview is built from the observed
    //    snapshot and from the intended terrain-only frame; the first must equal the real runner's.
    auto s = make_session(w, c.preserved);
    while (s->runner.active())
        s->runner.step(*s->fb, s->m, 256);
    s->finish_levels();
    const Camera before = s->m.camera;
    update(s->m, Wheel{s->clock + 5000, 1});
    const Camera after = s->m.camera;
    auto actual_fb = Framebuffer::create(after.width, after.height);
    const bool used = s->runner.render_moving(*actual_fb, s->m, s->terrain);
    const Pixels actual = pixels(*actual_fb);
    const Pixels from_observed = preview_from(w, *s, prod.snapshot, before, after);
    const Pixels from_intended = preview_from(w, *s, terrain_final, before, after);
    std::size_t d_obs = 0, d_int = 0, dark_light = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        d_obs += actual[i] != from_observed[i];
        if (actual[i] != from_intended[i]) {
            ++d_int;
            dark_light += actual[i] == 0 || actual[i] == 0xFFFF;
        }
    }
    obs(tag + " zoom preview",
        "preview used the cache: %d, radius factor %.4f; replica built from "
        "the observed snapshot differs from the real preview at %zu px; the preview built from "
        "an intended terrain-only snapshot differs from the real one at %zu px (%zu of them black "
        "or white)",
        static_cast<int>(used), after.radius_px / before.radius_px, d_obs, d_int, dark_light);
    CHECK(used && d_obs == 0); // the replica is only meaningful if it matches the real preview
    // 3. Preserve mode: the extra publication and its missing lines. Intended: the restored view
    //    keeps its lines when shown.
    if (c.preserved) {
        const std::size_t restore_pub = 17; // frames[17] = publication 18
        std::size_t missing = 0;
        for (std::size_t i = 0; i < lines.on.size(); ++i)
            missing +=
                lines.on[i] && !markers.on[i] && prod.frames[restore_pub][i] != lines.value[i];
        TerrainScene& t = *prod.terrain;
        prepare(t, prep);
        auto G = Framebuffer::create(prep.camera.width, prep.camera.height);
        const Policy cooperative{Order::Cooperative, 0, "cooperative"};
        Overlays o{&lines, &*w.place_scene, &prod.camera, false};
        // Rebuild the model's coverage by running the ring transitions first.
        const auto observed = transitions(true, true);
        for (std::size_t k = 0; k < restore_pub; ++k)
            transition(t, prep, *G, k == 0 ? prod.start : prod.frames[k - 1], observed[k],
                       cooperative, o);
        const Spec intended_spec{false, PassKind::Limb, 0, true, true, true, 0};
        const Pixels intended =
            transition(t, prep, *G, prod.frames[16], intended_spec, cooperative, o);
        std::size_t differs = 0, differs_on_lines = 0;
        for (std::size_t i = 0; i < intended.size(); ++i)
            if (intended[i] != prod.frames[restore_pub][i]) {
                ++differs;
                differs_on_lines += lines.on[i];
            }
        obs(tag + " preserve publication",
            "%zu publications (fresh: 19); the restore is "
            "published as #18 before the final stamp and lacks %zu line px; an intended "
            "restore-with-lines frame differs from it at %zu px (%zu on line px); publication "
            "#19 (after the stamp) equals the intended frame: %d",
            prod.frames.size(), missing, differs, differs_on_lines,
            static_cast<int>(prod.frames[18] == intended));
    }
}

// A seeded search over the 24x12 contrast grid for a view where reversing the limb pass changes a
// published frame (X0's view only shows it before the line stamp). Production focus is the view
// centre. Returns the number of candidates tried through `tried`.
std::optional<Case> find_publication_sensitive(World& w, const RenderQuality& quality,
                                               const Policy& limb_reversed, std::size_t* tried) {
    Rng rng{20261124U};
    const auto real = [&](double lo, double hi) {
        return lo + (hi - lo) * (rng.next() / 16777216.0);
    };
    for (std::size_t n = 0; n < 400; ++n) {
        const double lat = real(-90, 90), lon = real(-180, 180), heading = real(0, 360),
                     radius = real(15, 120);
        const auto camera = make_camera({{lat, lon}, heading, radius, 97, 65});
        if (!camera)
            continue;
        const Case c{"job limb-sensitive 24x12 (searched)",
                     false,
                     false,
                     *camera,
                     quality,
                     &*w.contrast_terrain,
                     false,
                     true};
        const Production prod = produce(w, c);
        const Prep prep = make_prep(prod);
        const Mask lines = line_mask(w, prod.camera, prod.quality);
        const Outcome r =
            run_sequence(w, prod, prep, lines, transitions(false, false), limb_reversed, false);
        if (r.matched < r.transitions) {
            *tried = n + 1;
            std::printf("SEARCH found after %zu candidates: lat=%.17g lon=%.17g heading=%.17g "
                        "radius=%.17g (97x65, center focus)\n",
                        n + 1, lat, lon, heading, radius);
            return c;
        }
    }
    *tried = 400;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : ".";
    auto world = std::make_unique<World>();
    if (!build_world(*world, root))
        return 1;
    World& w = *world;

    RenderQuality relief_q{1.5, true, 0, TerrainStyle::Relief};
    RenderQuality flat_q = relief_q;
    flat_q.smooth_terrain = false;
    std::vector<Case> cases = {
        {"runner fresh (markers)", true, false, {}, {}, &*w.terrain, false, false},
        {"runner preserved (markers)", true, true, {}, {}, &*w.terrain, false, false},
        {"job Relief/flat polar", false, false, *make_camera({{-80, 30}, 40, 50, 97, 73}), flat_q,
         &*w.terrain, false, false},
        {"job limb-sensitive 24x12 (X0 view)", false, false,
         *make_camera({{21.87328577041626, 175.44030904769897},
                       271.49210214614868,
                       22.9573854804039,
                       97,
                       65}),
         relief_q, &*w.contrast_terrain, true, false},
    };
    const Policy cooperative{Order::Cooperative, 0, "cooperative"};
    const Policy limb_search{Order::LimbReversed, 0, "limb reversed"};
    std::size_t tried = 0;
    if (const auto found = find_publication_sensitive(w, relief_q, limb_search, &tried))
        cases.push_back(*found);
    obs("publication-level limb sensitivity", "%s after %zu candidate views",
        cases.size() == 5 ? "found" : "NOT found", tried);
    CHECK(cases.size() == 5);
    const Policy alternatives[] = {
        {Order::BandsReversed, 1, "bands reversed"},
        {Order::BandsShuffled, 7, "bands shuffled (seed 7)"},
        {Order::BandsShuffled, 20261123, "bands shuffled (seed 20261123)"},
        {Order::RoundRobin, 11, "bands interleaved, 4-slice chunks"},
    };

    for (const Case& c : cases) {
        const std::string tag = c.name;
        const Production prod = produce(w, c);
        const Prep prep = make_prep(prod);
        const Mask lines = line_mask(w, prod.camera, prod.quality);
        const auto specs = transitions(prod.preserved, prod.markers);
        obs(tag + " production",
            "%zu publications (F_0 + %zu); budget 7 vs 64 identical: %d; "
            "replay of runner marker hook identical: %d; %zu line px",
            prod.frames.size(), prod.frames.size(), static_cast<int>(prod.budget_invariant),
            static_cast<int>(prod.replay_calibrated), lines.count);
        CHECK(prod.frames.size() == specs.size());
        CHECK(prod.budget_invariant && prod.replay_calibrated);

        // Arm 1: calibration with the cooperative order.
        const Outcome calibration = run_sequence(w, prod, prep, lines, specs, cooperative, false);
        obs(tag + " calibration",
            "%zu of %zu production transitions reproduced from the previous "
            "published frame (cooperative order); mismatched transitions: %s; snapshot "
            "reproduced: %d",
            calibration.matched, calibration.transitions, list(calibration.mismatched).c_str(),
            static_cast<int>(calibration.snapshot_matched));
        CHECK(calibration.matched == calibration.transitions && calibration.snapshot_matched);
        if (calibration.matched != calibration.transitions)
            continue; // nothing else is meaningful

        // Arm 2: the same work in other orders inside each pass.
        for (const Policy& pol : alternatives) {
            const Outcome r = run_sequence(w, prod, prep, lines, specs, pol, true);
            obs(tag + " " + pol.name,
                "%zu of %zu transitions reproduced; mismatched: %s; "
                "snapshot reproduced: %d; draw-only chunk diagnostic: %llu changed chunks of "
                "%llu, %llu of %llu pixel checks outside {pre-pass, finished-pass}",
                r.matched, r.transitions, list(r.mismatched).c_str(),
                static_cast<int>(r.snapshot_matched),
                static_cast<unsigned long long>(r.diag.changed),
                static_cast<unsigned long long>(r.diag.chunks),
                static_cast<unsigned long long>(r.diag.outside),
                static_cast<unsigned long long>(r.diag.checks));
            CHECK(r.matched == r.transitions && r.snapshot_matched);
        }

        // Controls on this case.
        {
            // No pass barrier: two rings drawn before one publication.
            auto lookahead = specs;
            for (auto& s : lookahead)
                if (s.pass == PassKind::Ring && s.ring + 1 < kRefinePasses)
                    s.ahead = 1;
            const Outcome r = run_sequence(w, prod, prep, lines, lookahead, cooperative, false);
            obs(tag + " control: no pass barrier",
                "%zu of %zu transitions still reproduced "
                "(a difference is expected)",
                r.matched, r.transitions);
            CHECK(r.matched < r.transitions);
            if (prod.markers) {
                const Outcome swapped =
                    run_sequence(w, prod, prep, lines, specs, cooperative, false, true);
                obs(tag + " control: markers before stamp", "%zu of %zu transitions reproduced",
                    swapped.matched, swapped.transitions);
            }
            const Policy cells_reversed{Order::CellsReversed, 0, "cells reversed"};
            const Outcome r2 = run_sequence(w, prod, prep, lines, specs, cells_reversed, false);
            obs(tag + " control: cells reversed in pass",
                "%zu of %zu transitions reproduced; "
                "mismatched: %s",
                r2.matched, r2.transitions, list(r2.mismatched).c_str());
            const Policy limb_reversed{Order::LimbReversed, 0, "limb reversed"};
            const Outcome r3 = run_sequence(w, prod, prep, lines, specs, limb_reversed, false);
            obs(tag + " control: limb cells reversed",
                "%zu of %zu published frames reproduced; "
                "mismatched: %s; pre-stamp snapshot reproduced: %d",
                r3.matched, r3.transitions, list(r3.mismatched).c_str(),
                static_cast<int>(r3.snapshot_matched));
            if (c.runner)
                observed_versus_intended(w, c, prod, prep, lines);
            if (c.snapshot_control)
                CHECK(!r3.snapshot_matched); // the X0 view: visible before the stamp only
            if (c.publication_control)
                CHECK(r3.matched < r3.transitions); // a view where it survives into a publication
        }
    }
    return TEST_MAIN_RESULT();
}
// NOLINTEND(bugprone-unchecked-optional-access)
