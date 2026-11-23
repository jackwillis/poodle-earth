// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// X0 (EXPERIMENTAL, not approved for merge): is the settled terrain image invariant under
// partitioning into 16-row bands that are executed in different orders (X0a), and can each band
// render into storage it exclusively owns and be assembled into the same image (X0b)?
//
// Test-only. It uses the public TerrainScene / Framebuffer / raster API and no threads, atomics
// or production changes. Everything that draws takes `const TerrainScene&`, so the compiler
// enforces that pixel generation does not mutate the prepared view plan.
//
// Oracles: (A) a copy of tests/test_terrain.cpp's reference_terrain (the definition of the final
// image: unclipped cells in index order, then clipped cells in index order), extended with the
// preserve-bitmap coverage and restore steps; (B) a completed production RefinementJob.
//
//   test_partition_invariance <repo root> [full]
#include "atlas_util.hpp"
#include "check.hpp"
#include "globe/elevation.hpp"
#include "globe/framebuffer.hpp"
#include "globe/landclasses.hpp"
#include "globe/memory_ledger.hpp"
#include "globe/raster.hpp"
#include "globe/render.hpp"
#include "globe/terrain.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace globe;
using atlas_util::Bytes;

namespace {

using Pixels = std::vector<Rgb565>;

struct Rng {
    std::uint32_t state;
    std::uint32_t next() {
        state = state * 1664525U + 1013904223U;
        return state >> 8;
    }
    double real(double lo, double hi) { return lo + (hi - lo) * (next() / 16777216.0); }
    std::size_t below(std::size_t n) { return next() % n; }
};

template <typename T> void shuffle(std::vector<T>& v, Rng& rng) {
    for (std::size_t i = v.size(); i > 1; --i)
        std::swap(v[i - 1], v[rng.below(i)]);
}

constexpr std::uint32_t kCanarySalt = 0xC0FFEE, kSeedSalt = 0x5EED;
constexpr Rgb565 kProbe = 0xBEEF;

// Position-dependent filler: "previous frame" content and guard canaries.
Rgb565 pattern(int x, int y, std::uint32_t salt) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093U ^
                      static_cast<std::uint32_t>(y) * 19349663U ^ salt;
    h ^= h >> 13;
    h *= 0x5bd1e995U;
    h ^= h >> 15;
    return static_cast<Rgb565>(h);
}

void fill_pattern(Framebuffer& fb, int first, int end, std::uint32_t salt) {
    for (int y = first; y < end; ++y)
        for (int x = 0; x < fb.width(); ++x)
            fb.data()[static_cast<std::size_t>(y) * fb.stride() + x] = pattern(x, y, salt);
}

Pixels snapshot(const Framebuffer& fb) {
    return {fb.data(), fb.data() + static_cast<std::size_t>(fb.width()) * fb.height()};
}

// ---- Result bookkeeping -------------------------------------------------------------------

struct Tally {
    unsigned runs = 0, bad = 0;
};
struct Repro {
    long area = -1;
    std::string text;
};
std::map<std::string, Tally>& tallies() {
    static std::map<std::string, Tally> t;
    return t;
}
std::map<std::string, Repro>& repros() {
    static std::map<std::string, Repro> r;
    return r;
}

// Records one comparison; keeps the smallest failing viewport per key as the reproducer.
void record(const std::string& key, bool ok, const std::string& where, long area,
            const Pixels* got = nullptr, const Pixels* want = nullptr, int width = 0) {
    auto& tally = tallies()[key];
    ++tally.runs;
    if (ok)
        return;
    ++tally.bad;
    auto& repro = repros()[key];
    if (repro.area >= 0 && repro.area <= area)
        return;
    repro.area = area;
    repro.text = where;
    if (got != nullptr && want != nullptr && got->size() == want->size()) {
        std::size_t differing = 0, first = got->size();
        for (std::size_t i = 0; i < got->size(); ++i)
            if ((*got)[i] != (*want)[i]) {
                ++differing;
                first = std::min(first, i);
            }
        char text[160];
        std::snprintf(text, sizeof text, "; %zu px differ, first at (%zu,%zu): %04x vs %04x",
                      differing, first % static_cast<std::size_t>(width),
                      first / static_cast<std::size_t>(width), (*got)[first], (*want)[first]);
        repro.text += text;
    }
}

// ---- Fixtures -----------------------------------------------------------------------------

Bytes read_file(const char* path) {
    Bytes b;
    if (std::FILE* f = std::fopen(path, "rb")) {
        int c;
        while ((c = std::fgetc(f)) != EOF)
            b.push_back(static_cast<std::uint8_t>(c));
        std::fclose(f);
    }
    return b;
}

// The real pack's header and value table with contrasting neighbours, so any draw-order change
// shows in pixels (same construction as sized_grid in test_terrain.cpp).
Bytes contrast_grid(unsigned width, unsigned height) {
    auto bytes = read_file("data/elevation-1deg.gelv");
    bytes.resize(552 + static_cast<std::size_t>(width) * height);
    store_le32(bytes.data() + 8, width);
    store_le32(bytes.data() + 12, height);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
        bytes[552 + i] = static_cast<std::uint8_t>((i * 73) % 256);
    store_le32(bytes.data() + 36,
               crc32_extend(crc32(bytes.data(), 36), bytes.data() + 40, bytes.size() - 40));
    return bytes;
}

struct Fixture {
    std::string name;
    Bytes bytes, class_bytes;
    std::optional<Elevation> grid;
    std::optional<LandClasses> classes;
    std::optional<TerrainScene> terrain;
    unsigned trials = 0;
    bool real = false;
};

std::unique_ptr<Fixture> make_fixture(const std::string& name, Bytes bytes, bool classes,
                                      unsigned trials) {
    auto f = std::make_unique<Fixture>();
    f->name = name;
    f->bytes = std::move(bytes);
    f->real = classes;
    f->trials = trials;
    f->grid = Elevation::parse(f->bytes.data(), f->bytes.size());
    CHECK(f->grid);
    if (!f->grid)
        return nullptr;
    if (classes) {
        f->class_bytes = read_file("data/landclasses-1deg.ggrd");
        f->classes = LandClasses::decode(f->class_bytes.data(), f->class_bytes.size());
        CHECK(f->classes);
    }
    f->terrain = TerrainScene::create(*f->grid, f->classes ? &*f->classes : nullptr);
    CHECK(f->terrain);
    return f->terrain ? std::move(f) : nullptr;
}

// ---- A view and how it is prepared --------------------------------------------------------

struct Look {
    TerrainStyle style;
    bool smooth, skip, preserve;
};

std::string look_name(const Look& l) {
    return std::string(l.style == TerrainStyle::Map ? "Map" : "Relief") +
           (l.smooth ? "/smooth" : "/flat") + (l.skip ? "/skip" : "/noskip") +
           (l.preserve ? "/preserve" : "/fresh");
}

struct Prep {
    Camera camera;
    RenderQuality quality;
    Palette palette;
    bool skip;
    int bands() const { return (camera.height + kRefineBandRows - 1) / kRefineBandRows; }
    bool preserve() const { return quality.preserve_bitmap; }
};

Prep make_prep(const Camera& camera, const Look& look, ScreenPoint focus) {
    Prep p;
    p.camera = camera;
    p.quality = RenderQuality{1.5, true, 0, look.style};
    p.quality.smooth_terrain = look.smooth;
    p.quality.skip_ocean = look.skip;
    p.quality.preserve_bitmap = look.preserve;
    p.quality.terrain_only = true;
    p.quality.focus = focus;
    p.palette = palette_for_style(kDefaultPalette, look.style);
    // The same condition RefinementJob::step passes to assign_pass.
    p.skip = p.quality.skip_ocean && !p.quality.preserve_bitmap && p.quality.smooth_terrain &&
             p.quality.terrain_style == TerrainStyle::Map && p.palette.ocean == kMapOcean;
    return p;
}

// What the job's ProjectCorners, BoundCells and BuildPassLists phases leave in the workspace.
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

// Everything observable about the prepared plan, to prove drawing never changes it.
std::uint64_t signature(const TerrainScene& t) {
    std::uint64_t h = 1469598103934665603ULL;
    const auto mix = [&](std::uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
    for (std::size_t i = 0; i < t.cell_count(); ++i) {
        const auto b = t.cell_bands(i);
        mix(t.cell_pass(i));
        mix(static_cast<std::uint16_t>(b.first));
        mix(static_cast<std::uint16_t>(b.end));
        mix(t.is_limb(i));
    }
    if (t.has_lists())
        for (unsigned k = 0; k < kRefinePasses; ++k) {
            mix(t.pass_count(k));
            for (std::size_t pos = 0; pos < t.pass_count(k); ++pos)
                mix(t.pass_cell(k, pos));
        }
    return h;
}

// ---- Row-local steps the job performs -----------------------------------------------------

void clear_rows(Framebuffer& fb, const Prep& p, int first, int end) {
    for (int y = first; y < end; ++y) {
        if (p.preserve()) {
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

void restore_rows(Framebuffer& fb, const Prep& p, int first, int end) {
    for (int y = first; y < end; ++y) {
        fb.clip_rows(y, y + 1);
        fb.coverage(Framebuffer::Coverage::Uncovered);
        fb.fill_span(y, 0, fb.width() - 1, p.palette.background);
        fill_disc(fb, p.camera.center_x, p.camera.center_y, p.camera.radius_px, p.palette.ocean);
        fb.coverage(Framebuffer::Coverage::Off);
    }
    fb.clip_rows(0, fb.height());
}

void init_frame(Framebuffer& fb, const Prep& p) {
    fill_pattern(fb, 0, fb.height(), p.preserve() ? kSeedSalt : kCanarySalt);
}

// ---- Oracles ------------------------------------------------------------------------------

// Oracle A: the definition of the final image, drawn directly cell by cell (a copy of
// reference_terrain in tests/test_terrain.cpp, plus preserve-bitmap coverage and restore).
Pixels reference_image(TerrainScene& t, Prep p) {
    p.quality.focus = {p.camera.center_x, p.camera.center_y};
    p.skip = false;
    prepare(t, p);
    auto fb = Framebuffer::create(p.camera.width, p.camera.height);
    if (!fb)
        return {};
    init_frame(*fb, p);
    clear_rows(*fb, p, 0, fb->height());
    for (const bool limb : {false, true})
        for (std::size_t i = 0; i < t.cell_count(); ++i) {
            if (t.cell_pass(i) == 255 || t.limb_cell(i) != limb)
                continue;
            const auto bands = t.cell_bands(i);
            for (int band = bands.first; band < bands.end; ++band) {
                fb->clip_rows(band * kRefineBandRows,
                              std::min(fb->height(), (band + 1) * kRefineBandRows));
                if (p.preserve())
                    fb->coverage(Framebuffer::Coverage::Mark);
                t.draw_cell(*fb, i, band, kRefineBandRows, p.quality.terrain_style,
                            p.quality.smooth_terrain);
                fb->coverage(Framebuffer::Coverage::Off);
            }
        }
    fb->clip_rows(0, fb->height());
    if (p.preserve())
        restore_rows(*fb, p, 0, fb->height());
    return snapshot(*fb);
}

// Oracle B: the production cooperative job, run to completion. With `publications`, a snapshot
// is taken at every band_ready (a pass boundary when no line layer is in use).
Pixels job_image(TerrainScene& t, const Scene& scene, const Prep& p, RenderStats* stats,
                 std::vector<Pixels>* publications = nullptr) {
    auto fb = Framebuffer::create(p.camera.width, p.camera.height);
    if (!fb)
        return {};
    init_frame(*fb, p);
    RefinementJob job(p.camera, scene, kDefaultPalette, Layers{false, false, false}, p.quality, 1,
                      &t);
    while (job.active()) {
        const auto step = job.step(*fb, 1, 256);
        if (publications != nullptr && step.band_ready)
            publications->push_back(snapshot(*fb));
    }
    if (stats != nullptr)
        *stats = job.stats();
    return snapshot(*fb);
}

// ---- The band executor --------------------------------------------------------------------

// One unit of independently schedulable work: draw cells in one 16-row band, or finish its rows.
enum class Work : std::uint8_t {
    Ring,              // the cells of ring `pass`, in pass-list order
    Limb,              // clipped cells, index order
    LimbReversed,      // control
    Unclipped,         // every unclipped cell, index order (the definition of the final image)
    UnclippedReversed, // control
    Restore            // preserve-bitmap: uncovered pixels back to background and ocean
};
struct Step {
    int band;
    Work work;
    unsigned pass;
};
using Steps = std::vector<Step>;

class Executor {
  public:
    // targets[band] is the framebuffer that band draws into (all one buffer, or one per band).
    Executor(const TerrainScene& terrain, const Prep& prep, std::vector<Framebuffer*> targets)
        : t_(terrain), p_(prep), targets_(std::move(targets)), state_(targets_.size()) {}

    void run(const Step& s) {
        Framebuffer& fb = *targets_[static_cast<std::size_t>(s.band)];
        auto& st = state_[static_cast<std::size_t>(s.band)];
        const int first = s.band * kRefineBandRows, end = std::min(fb.height(), first + 16);
        if (!st.cleared) {
            clear_rows(fb, p_, first, end);
            st.cleared = true;
        }
        if (s.work == Work::Restore) {
            violations += !st.limb;
            restore_rows(fb, p_, first, end);
            return;
        }
        fb.clip_rows(first, end);
        const std::size_t n = t_.cell_count();
        switch (s.work) {
        case Work::Ring:
            if (t_.has_lists()) {
                for (std::size_t pos = 0; pos < t_.pass_count(s.pass); ++pos)
                    draw(fb, t_.pass_cell(s.pass, pos), s.band);
            } else {
                for (std::size_t i = 0; i < n; ++i)
                    if (t_.cell_pass(i) == s.pass)
                        draw(fb, i, s.band);
            }
            st.rings += 1;
            break;
        case Work::Unclipped:
            for (std::size_t i = 0; i < n; ++i)
                if (t_.cell_pass(i) != 255 && !t_.is_limb(i))
                    draw(fb, i, s.band);
            st.unclipped = true;
            break;
        case Work::UnclippedReversed:
            for (std::size_t i = n; i-- > 0;)
                if (t_.cell_pass(i) != 255 && !t_.is_limb(i))
                    draw(fb, i, s.band);
            st.unclipped = true;
            break;
        case Work::Limb:
        case Work::LimbReversed:
            violations += !(st.unclipped || st.rings >= kRefinePasses);
            if (s.work == Work::Limb) {
                for (std::size_t i = 0; i < n; ++i)
                    if (t_.is_limb(i))
                        draw(fb, i, s.band);
            } else {
                for (std::size_t i = n; i-- > 0;)
                    if (t_.is_limb(i))
                        draw(fb, i, s.band);
            }
            st.limb = true;
            break;
        case Work::Restore:
            break;
        }
        fb.clip_rows(0, fb.height());
    }

    std::uint64_t slices = 0; // draw_cell calls that reached a band, as RenderStats counts them
    unsigned violations = 0;  // steps run before a dependency of the stated model was met

  private:
    struct BandState {
        bool cleared = false, unclipped = false, limb = false;
        unsigned rings = 0;
    };
    void draw(Framebuffer& fb, std::size_t cell, int band) {
        const auto b = t_.cell_bands(cell);
        if (band < b.first || band >= b.end)
            return;
        if (p_.preserve())
            fb.coverage(Framebuffer::Coverage::Mark);
        t_.draw_cell(fb, cell, band, kRefineBandRows, p_.quality.terrain_style,
                     p_.quality.smooth_terrain);
        fb.coverage(Framebuffer::Coverage::Off);
        ++slices;
    }
    const TerrainScene& t_; // const: drawing cannot mutate the prepared view plan
    const Prep& p_;
    std::vector<Framebuffer*> targets_;
    std::vector<BandState> state_;
};

// ---- Schedules ----------------------------------------------------------------------------

enum class Order : std::uint8_t {
    Normal,            // rings 0..15, limb
    Index,             // unclipped in index order, limb (the definition of the final image)
    LimbFirst,         // control: limb before the rings
    RingsReversed,     // control: rings 15..0, then limb
    UnclippedReversed, // control: unclipped cells in reverse index order
    LimbReversed,      // control: limb cells in reverse index order
    RestoreEarly       // control: restore before the limb pass (preserve only)
};

Steps band_steps(int band, Order order, bool preserve) {
    Steps s;
    const auto rings = [&](bool reversed) {
        for (unsigned k = 0; k < kRefinePasses; ++k)
            s.push_back({band, Work::Ring, reversed ? kRefinePasses - 1 - k : k});
    };
    switch (order) {
    case Order::Normal:
        rings(false);
        s.push_back({band, Work::Limb, 0});
        break;
    case Order::Index:
        s.push_back({band, Work::Unclipped, 0});
        s.push_back({band, Work::Limb, 0});
        break;
    case Order::LimbFirst:
        s.push_back({band, Work::Limb, 0});
        rings(false);
        break;
    case Order::RingsReversed:
        rings(true);
        s.push_back({band, Work::Limb, 0});
        break;
    case Order::UnclippedReversed:
        s.push_back({band, Work::UnclippedReversed, 0});
        s.push_back({band, Work::Limb, 0});
        break;
    case Order::LimbReversed:
        rings(false);
        s.push_back({band, Work::LimbReversed, 0});
        break;
    case Order::RestoreEarly:
        rings(false);
        if (preserve)
            s.push_back({band, Work::Restore, 0});
        s.push_back({band, Work::Limb, 0});
        return s;
    }
    if (preserve)
        s.push_back({band, Work::Restore, 0});
    return s;
}

std::vector<int> band_order(int bands, int kind, Rng& rng) {
    std::vector<int> order(static_cast<std::size_t>(bands));
    for (int i = 0; i < bands; ++i)
        order[static_cast<std::size_t>(i)] = kind == 1 ? bands - 1 - i : i;
    if (kind == 2)
        shuffle(order, rng);
    return order;
}

// Each band runs its whole sequence, bands in the given order.
Steps band_major(const std::vector<int>& order, Order what, bool preserve) {
    Steps all;
    for (const int band : order) {
        const auto s = band_steps(band, what, preserve);
        all.insert(all.end(), s.begin(), s.end());
    }
    return all;
}

// Random interleaving of the per-band sequences (each band's own order preserved).
Steps interleaved(int bands, Order what, bool preserve, Rng& rng) {
    std::vector<Steps> queues;
    std::vector<std::size_t> next(static_cast<std::size_t>(bands), 0);
    std::size_t remaining = 0;
    for (int b = 0; b < bands; ++b) {
        queues.push_back(band_steps(b, what, preserve));
        remaining += queues.back().size();
    }
    Steps all;
    while (remaining > 0) {
        const auto b = rng.below(static_cast<std::size_t>(bands));
        if (next[b] < queues[b].size()) {
            all.push_back(queues[b][next[b]++]);
            --remaining;
        }
    }
    return all;
}

// Pass barriers: every band finishes pass k before any starts pass k+1; bands in random order
// within a pass. Rows are restored after the limb pass.
Steps pass_major(int bands, bool preserve, Rng& rng,
                 std::vector<std::size_t>* pass_ends = nullptr) {
    Steps all;
    for (unsigned k = 0; k <= kRefinePasses; ++k) {
        for (const int b : band_order(bands, 2, rng))
            all.push_back({b, k < kRefinePasses ? Work::Ring : Work::Limb, k});
        if (pass_ends != nullptr)
            pass_ends->push_back(all.size());
    }
    if (preserve)
        for (const int b : band_order(bands, 2, rng))
            all.push_back({b, Work::Restore, 0});
    return all;
}

// ---- X0a / X0b runners --------------------------------------------------------------------

struct Run {
    Pixels image;
    std::uint64_t slices = 0;
    unsigned violations = 0;
    bool guards_ok = true, coverage_guards_ok = true, assembly_ok = true;
    std::uint64_t buffer_bytes = 0, peak_bytes = 0;
};

// X0a: all bands share one framebuffer.
Run run_shared(const TerrainScene& t, const Prep& p, const Steps& steps) {
    Run r;
    auto fb = Framebuffer::create(p.camera.width, p.camera.height);
    if (!fb)
        return r;
    init_frame(*fb, p);
    Executor ex(t, p, std::vector<Framebuffer*>(static_cast<std::size_t>(p.bands()), &*fb));
    for (const auto& s : steps)
        ex.run(s);
    r.image = snapshot(*fb);
    r.slices = ex.slices;
    r.violations = ex.violations;
    return r;
}

// X0b: every band draws into its own full-size framebuffer (canary everywhere outside the
// band's rows), then only the owned rows are assembled, in a random order, into the output.
Run run_owned(const TerrainScene& t, const Prep& p, const Steps& steps, Rng& rng) {
    Run r;
    const int w = p.camera.width, h = p.camera.height, bands = p.bands();
    MemoryLedger ledger;
    std::vector<std::optional<Framebuffer>> buffers(static_cast<std::size_t>(bands));
    std::vector<Framebuffer*> targets;
    for (int b = 0; b < bands; ++b) {
        auto& slot = buffers[static_cast<std::size_t>(b)];
        slot = Framebuffer::create(w, h, &ledger, MemoryCategory::JobWorkspace);
        if (!slot)
            return r;
        const int first = b * kRefineBandRows, end = std::min(h, first + kRefineBandRows);
        fill_pattern(*slot, 0, h, kCanarySalt);
        if (p.preserve())
            fill_pattern(*slot, first, end, kSeedSalt);
        targets.push_back(&*slot);
    }
    r.buffer_bytes = targets.front()->byte_size() + targets.front()->coverage_byte_size();
    r.peak_bytes = ledger.snapshot().total_peak;
    Executor ex(t, p, targets);
    for (const auto& s : steps)
        ex.run(s);
    r.slices = ex.slices;
    r.violations = ex.violations;
    // Ownership: nothing outside a band's rows changed, in pixels or in coverage. The coverage
    // probe paints only uncovered pixels, so any coverage bit set outside the band survives it.
    for (int b = 0; b < bands; ++b) {
        auto& fb = *targets[static_cast<std::size_t>(b)];
        const int first = b * kRefineBandRows, end = std::min(h, first + kRefineBandRows);
        for (int y = 0; y < h; ++y) {
            if (y >= first && y < end)
                continue;
            for (int x = 0; x < w; ++x)
                r.guards_ok =
                    r.guards_ok && fb.data()[static_cast<std::size_t>(y) * fb.stride() + x] ==
                                       pattern(x, y, kCanarySalt);
        }
        for (int y = 0; y < h; ++y) {
            if (y >= first && y < end)
                continue;
            fb.clip_rows(y, y + 1);
            fb.coverage(Framebuffer::Coverage::Uncovered);
            fb.fill_span(y, 0, w - 1, kProbe);
            fb.coverage(Framebuffer::Coverage::Off);
            for (int x = 0; x < w; ++x)
                r.coverage_guards_ok =
                    r.coverage_guards_ok &&
                    fb.data()[static_cast<std::size_t>(y) * fb.stride() + x] == kProbe;
        }
        fb.clip_rows(0, h);
    }
    // Assembly in two different random orders must give the same bytes.
    Pixels assembled[2];
    for (auto& image : assembled) {
        auto out = Framebuffer::create(w, h);
        if (!out)
            return r;
        fill_pattern(*out, 0, h, kCanarySalt);
        std::vector<int> order = band_order(bands, 2, rng);
        for (const int b : order) {
            const int first = b * kRefineBandRows, end = std::min(h, first + kRefineBandRows);
            std::memcpy(out->data() + static_cast<std::size_t>(first) * out->stride(),
                        targets[static_cast<std::size_t>(b)]->data() +
                            static_cast<std::size_t>(first) * out->stride(),
                        static_cast<std::size_t>(end - first) * out->stride() * sizeof(Rgb565));
        }
        image = snapshot(*out);
    }
    r.assembly_ok = assembled[0] == assembled[1];
    r.image = std::move(assembled[0]);
    return r;
}

// ---- Trial --------------------------------------------------------------------------------

struct Trial {
    Fixture* fixture;
    unsigned index;
    Look look;
    int width, height;
    double lat, lon, heading, radius;
    ScreenPoint focus;
    std::uint32_t seed;
};

std::string describe(const Trial& t) {
    char text[256];
    std::snprintf(text, sizeof text,
                  "%s #%u %s %dx%d lat=%.4f lon=%.4f heading=%.3f radius=%.3f focus=(%.2f,%.2f) "
                  "seed=%u",
                  t.fixture->name.c_str(), t.index, look_name(t.look).c_str(), t.width, t.height,
                  t.lat, t.lon, t.heading, t.radius, t.focus.x, t.focus.y, t.seed);
    return text;
}

struct Memory {
    int bands = 0;
    std::uint64_t buffer = 0, all_live = 0;
};
std::map<std::string, Memory>& memory_table() {
    static std::map<std::string, Memory> m;
    return m;
}

void run_trial(const Trial& tr, const Scene& scene) {
    if (!tr.fixture->terrain)
        return;
    TerrainScene& terrain = *tr.fixture->terrain;
    const auto camera = make_camera({{tr.lat, tr.lon}, tr.heading, tr.radius, tr.width, tr.height});
    if (!camera)
        return;
    const Prep prep = make_prep(*camera, tr.look, tr.focus);
    const std::string where = describe(tr);
    const long area = static_cast<long>(tr.width) * tr.height;
    const std::string cls = tr.look.preserve ? "preserve" : "fresh";
    Rng rng{tr.seed ^ 0x9E3779B9U};

    const Pixels ref = reference_image(terrain, prep);
    RenderStats stats;
    const Pixels job = job_image(terrain, scene, prep, &stats);
    record("oracle: job == reference (" + cls + ")", job == ref, where, area, &job, &ref, tr.width);

    prepare(terrain, prep);
    const auto before = signature(terrain);
    const int bands = prep.bands();
    // What this view actually exercised (reported, not asserted).
    {
        const auto& grid = terrain.grid();
        std::size_t limb = 0, flat_antarctic = 0, hidden_by_skip = 0, visible = 0;
        for (std::size_t i = 0; i < terrain.cell_count(); ++i) {
            if (terrain.cell_pass(i) == 255) {
                hidden_by_skip +=
                    prep.skip && terrain.cell_bands(i).first != terrain.cell_bands(i).end;
                continue;
            }
            ++visible;
            limb += terrain.is_limb(i);
            const std::size_t row = i / grid.width();
            const double lat = grid.south_deg() + (grid.north_deg() - grid.south_deg()) *
                                                      (static_cast<double>(row) + 0.5) /
                                                      grid.height();
            flat_antarctic += grid.value(grid.cells()[i]) >= 0 && lat < -60 && !terrain.is_limb(i);
        }
        const auto note = [&](const char* what, bool yes) {
            record(std::string("coverage: ") + what, yes, where, 0);
        };
        note("views with a visible limb cell", limb > 0);
        note("views with a visible flat Antarctic cell (not limb)", flat_antarctic > 0);
        note("views where ocean-skip removed cells", hidden_by_skip > 0);
        note("views with no visible terrain", visible == 0);
    }
    // The index-order schedule draws each visible cell once (limb cells are not also drawn in a
    // ring), so its slice count is the visible cells' band spans, not the job's.
    std::uint64_t single_pass_slices = 0;
    for (std::size_t i = 0; i < terrain.cell_count(); ++i)
        if (terrain.cell_pass(i) != 255)
            single_pass_slices +=
                static_cast<std::uint64_t>(terrain.cell_bands(i).end - terrain.cell_bands(i).first);
    const auto check = [&](const std::string& key, const Run& run, bool primary,
                           bool single_pass = false) {
        const std::string full = (primary ? "" : "control ") + key;
        record(full + " vs reference", run.image == ref, where, area, &run.image, &ref, tr.width);
        record(full + " vs job", run.image == job, where, area, &run.image, &job, tr.width);
        if (primary) {
            record(key + (single_pass ? ": slices == visible cell spans"
                                      : ": slices == job.terrain_slices"),
                   run.slices == (single_pass ? single_pass_slices : stats.terrain_slices), where,
                   area);
            record(key + ": no dependency violation", run.violations == 0, where, area);
        }
    };

    // X0a: shared framebuffer, primary schedules.
    check("X0a band-major normal",
          run_shared(terrain, prep,
                     band_major(band_order(bands, 0, rng), Order::Normal, prep.preserve())),
          true);
    check("X0a band-major reverse",
          run_shared(terrain, prep,
                     band_major(band_order(bands, 1, rng), Order::Normal, prep.preserve())),
          true);
    for (int i = 0; i < 2; ++i)
        check("X0a band-major shuffled",
              run_shared(terrain, prep,
                         band_major(band_order(bands, 2, rng), Order::Normal, prep.preserve())),
              true);
    check("X0a band-major index-order (definition)",
          run_shared(terrain, prep,
                     band_major(band_order(bands, 2, rng), Order::Index, prep.preserve())),
          true, true);
    check("X0a pass-major, shuffled bands per pass",
          run_shared(terrain, prep, pass_major(bands, prep.preserve(), rng)), true);
    check("X0a random interleaving",
          run_shared(terrain, prep, interleaved(bands, Order::Normal, prep.preserve(), rng)), true);

    // X0b: exclusively owned outputs.
    const auto owned = [&](const std::string& key, const Steps& steps) {
        const Run run = run_owned(terrain, prep, steps, rng);
        check(key, run, true);
        record(key + ": guard rows untouched", run.guards_ok, where, area);
        record(key + ": coverage untouched outside band", run.coverage_guards_ok, where, area);
        record(key + ": assembly order irrelevant", run.assembly_ok, where, area);
        auto& m = memory_table()[std::to_string(tr.width) + "x" + std::to_string(tr.height)];
        m.bands = bands;
        m.buffer = run.buffer_bytes;
        m.all_live = run.peak_bytes;
    };
    owned("X0b band-major shuffled",
          band_major(band_order(bands, 2, rng), Order::Normal, prep.preserve()));
    owned("X0b pass-major, shuffled bands per pass", pass_major(bands, prep.preserve(), rng));
    owned("X0b random interleaving", interleaved(bands, Order::Normal, prep.preserve(), rng));

    // Controls: orderings the stated model forbids or does not need. Differences are findings.
    struct Control {
        const char* name;
        Order order;
    };
    for (const Control c :
         {Control{"limb-first", Order::LimbFirst}, Control{"rings-reversed", Order::RingsReversed},
          Control{"unclipped-reversed", Order::UnclippedReversed},
          Control{"limb-reversed", Order::LimbReversed},
          Control{"restore-early", Order::RestoreEarly}}) {
        if (c.order == Order::RestoreEarly && !prep.preserve())
            continue;
        const Run run = run_shared(terrain, prep,
                                   band_major(band_order(bands, 2, rng), c.order, prep.preserve()));
        record(std::string("control ") + c.name + " vs reference", run.image == ref, where, area,
               &run.image, &ref, tr.width);
        record(std::string("control ") + c.name + " [" + tr.fixture->name + "]", run.image == ref,
               where, area);
    }

    // The drawing steps did not change the prepared plan (observable part).
    record("prepared plan unchanged by drawing", signature(terrain) == before, where, area);

    // Publication: with pass barriers, the image after pass k is the cooperative job's image at
    // that pass boundary, whatever the order of bands within each pass.
    if (!prep.preserve() && tr.index % 3 == 0) {
        std::vector<Pixels> published;
        job_image(terrain, scene, prep, nullptr, &published);
        prepare(terrain, prep);
        std::vector<std::size_t> ends;
        Rng pass_rng{tr.seed ^ 0xABCDEFU};
        const Steps steps = pass_major(bands, false, pass_rng, &ends);
        auto fb = Framebuffer::create(tr.width, tr.height);
        if (fb) {
            init_frame(*fb, prep);
            Executor ex(terrain, prep,
                        std::vector<Framebuffer*>(static_cast<std::size_t>(bands), &*fb));
            std::size_t done = 0;
            // The job reports band_ready after each of the 17 passes and once more on reaching
            // Done.
            record("publication: 17 pass boundaries + 1 final band_ready",
                   published.size() == ends.size() + 1, where, area);
            for (std::size_t k = 0; k < ends.size() && k < published.size(); ++k) {
                for (; done < ends[k]; ++done)
                    ex.run(steps[done]);
                const Pixels now = snapshot(*fb);
                record("publication: image at pass boundary == cooperative job",
                       now == published[k], where, area, &now, &published[k], tr.width);
            }
        }
    }
}

// What happens to a worker that keeps drawing after the workspace is re-prepared for a new view.
void workspace_reuse(Fixture& f, const Scene& scene) {
    if (!f.terrain)
        return;
    TerrainScene& terrain = *f.terrain;
    const Look look{TerrainStyle::Relief, true, false, false};
    const auto maybe_a = make_camera({{20, 10}, 0, 60, 160, 120});
    const auto maybe_b = make_camera({{-35, 120}, 40, 70, 160, 120});
    if (!maybe_a || !maybe_b)
        return;
    const Camera cam_a = *maybe_a, cam_b = *maybe_b;
    const Prep a = make_prep(cam_a, look, {80, 60}), b = make_prep(cam_b, look, {80, 60});
    const Pixels want_a = reference_image(terrain, a), want_b = reference_image(terrain, b);
    prepare(terrain, a);
    auto fb = Framebuffer::create(160, 120);
    if (!fb)
        return;
    init_frame(*fb, a);
    Executor first(terrain, a,
                   std::vector<Framebuffer*>(static_cast<std::size_t>(a.bands()), &*fb));
    Rng unused{1};
    const Steps steps = band_major(band_order(a.bands(), 0, unused), Order::Normal, false);
    const std::size_t half = steps.size() / 2;
    for (std::size_t i = 0; i < half; ++i)
        first.run(steps[i]);
    // A new job claims the workspace and prepares it; the old executor is not told.
    const auto token = terrain.token();
    RefinementJob old_job(cam_a, scene, kDefaultPalette, Layers{false, false, false}, a.quality, 1,
                          &terrain);
    RefinementJob new_job(cam_b, scene, kDefaultPalette, Layers{false, false, false}, b.quality, 2,
                          &terrain);
    record("reuse: claim() advances the workspace token", terrain.token() != token, f.name, 0);
    record("reuse: superseded RefinementJob refuses to step",
           old_job.step(*fb, 1, 256).work_units == 0, f.name, 0);
    prepare(terrain, b);
    for (std::size_t i = half; i < steps.size(); ++i)
        first.run(steps[i]);
    const Pixels mixed = snapshot(*fb);
    // draw_cell itself checks nothing: the stale executor kept drawing, now with view B's plan.
    record("reuse: stale executor output (expected to match neither view)",
           mixed != want_a && mixed != want_b, f.name, 0);
}

Trial make_trial(Fixture& f, unsigned index, std::uint32_t base_seed, bool production_size) {
    static const int sizes[][2] = {{100, 81}, {160, 120}, {97, 65}, {40, 17}, {128, 128}};
    Trial t;
    t.fixture = &f;
    t.index = index;
    t.seed = base_seed + index * 7919U;
    Rng rng{t.seed};
    t.look = {(index & 1U) != 0 ? TerrainStyle::Map : TerrainStyle::Relief, (index & 2U) != 0,
              (index & 4U) != 0, (index & 8U) != 0};
    const bool fine = f.terrain && f.terrain->cell_count() > 20000;
    const auto* size = sizes[index % 5];
    t.width = production_size ? 640 : (fine ? std::min(size[0], 100) : size[0]);
    t.height = production_size ? 480 : (fine ? std::min(size[1], 81) : size[1]);
    const bool polar = index % 11 == 0;
    const bool south = f.real && index % 3 == 1;
    t.lat = polar ? ((index & 16U) ? 89.9 : -89.9) : south ? rng.real(-90, -62) : rng.real(-90, 90);
    t.lon = rng.real(-180, 180);
    t.heading = rng.real(0, 360);
    t.radius =
        production_size ? (index % 2 ? 440.0 : 220.0) : rng.real(15, index % 5 == 0 ? 700 : 120);
    t.focus = {rng.real(-20, t.width + 20), rng.real(-20, t.height + 20)};
    return t;
}

} // namespace

int main(int argc, char** argv) {
    const bool full = argc > 2 && std::strcmp(argv[2], "full") == 0;
    const unsigned scale = full ? 5 : 1;
    auto ab = atlas_util::make_atlas({}, "x0");
    auto atlas = Atlas::parse(ab.data(), ab.size());
    CHECK(atlas);
    if (!atlas)
        return 1;
    auto scene = Scene::create(*atlas);
    CHECK(scene);
    if (!scene)
        return 1;

    std::vector<std::unique_ptr<Fixture>> fixtures;
    fixtures.push_back(make_fixture("contrast 24x12", contrast_grid(24, 12), false, 32 * scale));
    fixtures.push_back(make_fixture("contrast 36x18", contrast_grid(36, 18), false, 32 * scale));
    fixtures.push_back(make_fixture("contrast 60x30", contrast_grid(60, 30), false, 32 * scale));
    fixtures.push_back(make_fixture("contrast 120x60", contrast_grid(120, 60), false, 32 * scale));
    fixtures.push_back(make_fixture("contrast 400x300 (no pass lists)", contrast_grid(400, 300),
                                    false, 16 * scale));
    fixtures.push_back(make_fixture("real 1deg + land classes",
                                    read_file("data/elevation-1deg.gelv"), true, 64 * scale));
    fixtures.push_back(make_fixture("real 0.5deg + land classes (no pass lists)",
                                    read_file("data/elevation-0.5deg.gelv"), true, 16 * scale));
    for (const auto& f : fixtures)
        if (f == nullptr)
            return 1;

    for (const auto& f : fixtures) {
        for (unsigned i = 0; i < f->trials; ++i)
            run_trial(make_trial(*f, i, 20261123U, false), *scene);
        const Fixture& fixture = *f;
        const bool lists = fixture.terrain.has_value() && fixture.terrain->has_lists();
        std::printf("fixture done: %s (%u trials, %s pass lists)\n", f->name.c_str(), f->trials,
                    lists ? "with" : "without");
        std::fflush(stdout);
    }
    // Production-size frames (640x480) on the real 1 degree pack, both radii, for memory figures.
    for (unsigned i = 0; i < 4 * scale; ++i)
        run_trial(make_trial(*fixtures[5], i, 640480U, true), *scene);
    workspace_reuse(*fixtures[5], *scene);

    // ---- Report ----
    std::printf("\n%-78s %6s %6s\n", "comparison", "runs", "differ");
    for (const auto& [key, tally] : tallies()) {
        if (key.rfind("coverage: ", 0) == 0) // "differ" column = views where the condition held
            std::printf("%-78s %6u %6u  (held)\n", key.c_str(), tally.runs, tally.runs - tally.bad);
        else
            std::printf("%-78s %6u %6u%s\n", key.c_str(), tally.runs, tally.bad,
                        tally.bad != 0 ? "  <--" : "");
    }
    std::printf("\nsmallest reproducers\n");
    for (const auto& [key, repro] : repros())
        std::printf("  %s\n    %s\n", key.c_str(), repro.text.c_str());
    std::printf("\nmemory (full-size buffer per band; X0b)\n%-10s %6s %12s %14s %14s\n", "view",
                "bands", "per band", "all bands", "row window");
    for (const auto& [size, m] : memory_table()) {
        const int w = static_cast<int>(std::strtol(size.c_str(), nullptr, 10));
        const unsigned long long window =
            static_cast<unsigned long long>(w) * kRefineBandRows * 2 +
            static_cast<unsigned long long>((w + 7) / 8) * kRefineBandRows;
        std::printf("%-10s %6d %12llu %14llu %14llu\n", size.c_str(), m.bands,
                    static_cast<unsigned long long>(m.buffer),
                    static_cast<unsigned long long>(m.all_live),
                    window * static_cast<unsigned long long>(m.bands));
    }

    // Pass/fail: the primary X0a/X0b comparisons, the bookkeeping checks and the reuse findings.
    // Controls are findings, not assertions.
    for (const auto& [key, tally] : tallies()) {
        if (key.rfind("control ", 0) != 0 && key.rfind("coverage: ", 0) != 0 && tally.bad != 0) {
            std::printf("FAILED: %s\n", key.c_str());
            CHECK(tally.bad == 0);
        }
    }
    if (test::failures() != 0) {
        std::printf("\n%d check(s) failed\n", test::failures());
        return 1;
    }
    std::printf("\nall primary checks passed\n");
    return 0;
}
