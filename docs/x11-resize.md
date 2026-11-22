# X11 resize refusal and recovery

A resize is accepted only after its framebuffer and native presentation image are
ready. This is a frontend resource-lifetime contract, related to [#114](https://github.com/jackwillis/poodle-earth/issues/114),
not a shared frontend controller or a change to Accepted ADR 0002.

## Contract

- Prepare candidate pixels, coverage, XImage and XImage pixel storage while the old
  resources remain usable. A failure releases the partial candidate and leaves the
  accepted Model dimensions, generation, active continuation, wake deadline and
  framebuffer unchanged.
- After all allocations and pixel-format validation succeed, cancel old rendering
  before releasing either old display buffer. Install the buffers and immediately
  apply `Resize`, without yielding to another event. Model and buffer dimensions
  then match, and the normal redraw/settle path resumes.
- The window manager has already changed the native window when ConfigureNotify
  arrives. On refusal, keep the accepted viewport and sidebar layout at their old
  coordinates. Expose uploads the retained frame at (0,0); the server clips it in a
  smaller window and newly exposed space uses the black window background in a
  larger one. Navigation continues in accepted viewport coordinates. Do not force
  the window manager to resize back or add an allocation retry timer. Another
  ConfigureNotify with dimensions different from the accepted viewport can retry.
- Scripted X11 resizes prepare buffers before applying their message too. A refused
  scripted resize is skipped, and later messages continue; the native window is
  resized only after success.

FLTK can adopt the same prepare/cancel/commit and failure-retention contract in a
separate change, using its own native buffers and window behavior. This patch does
not change Classic, FLTK, paused-drag repaint, cache eviction or atlas loading.

## Regression evidence

Base: `18725cfe9f6a4b1d61d3f6968ebe32310fc93ab1` (origin/main on 2026-11-22).
Linux x86_64 (Fedora 44), GCC 16.2.1, Xvfb, Release build.

`test_x11_resize` compiles the actual X11 adapter and drives its normal event
handler with server-generated ConfigureNotify and Expose events, plus an XSendEvent
navigation key. It substitutes allocation outcomes only inside the test executable:
fallible array allocation for framebuffer pixels/coverage and linker wrappers for
XCreateImage and its pixel-storage malloc. No production flag or environment
variable injects failures. The test is registered when Xvfb and linker wrapping
are available.

For both a grow (320x240 to 400x300) and shrink (to 280x200), each of the four
failure points checks retained model/generation, continuation identity/progress,
wake deadline, buffer identities and live allocation counts. XGetImage readback
checks retained/clipped repaint, black newly exposed space, changed output after
navigation, and repaint after recovery to 410x310. Refinement continues after
refusal. Release observers check that a new active job is canceled before the old
pixels and XImage are destroyed on successful recovery. Teardown returns tracked
array/image counts to their baseline.

The original main implementation fails the final regression with 72 assertions:
for example, a failed grow advances the model to 400x300 and generation 1 to 2,
while the framebuffer remains 320x240 and the old job is canceled. A second
mutation, removing cancellation before old-buffer release from the fix, fails all
eight release-order checks. Both mutations were restored before final validation.

Commands (from the task worktree):

```sh
cmake -S . -B /tmp/poodle-resize-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/poodle-resize-build -j4
ctest --test-dir /tmp/poodle-resize-build --output-on-failure -j1
cmake -S . -B /tmp/poodle-resize-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGLOBE_SANITIZE=ON
cmake --build /tmp/poodle-resize-asan --target test_x11_resize -j4
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir /tmp/poodle-resize-asan -R '^x11_resize$' --output-on-failure
clang-tidy -p /tmp/poodle-resize-build src/platform/x11/main.cpp tests/test_x11_resize.cpp --quiet
clang-format --dry-run --Werror src/platform/x11/main.cpp tests/test_x11_resize.cpp
tools/check-policy.sh
git diff --check
```

Xvfb requires local display sockets, unavailable in the restricted shell; the X11
CTest commands were run with that sandbox restriction lifted. The final Release
suite passed 70/70 tests (58.82 seconds). Format, changed-file clang-tidy 22.1.8,
repository policy and whitespace checks passed. ASan/UBSan with leak
detection passed the grow/shrink failure and recovery test. Existing host ledger,
application, rendering and cancellation tests remain separate evidence; they do
not substitute for the X11 event/readback checks above.

The fixture explicitly controls ticks and continuation steps to inspect the
transaction before normal event-loop progress. It does not simulate a desktop
window manager, physical memory pressure, native NetBSD/macppc, scanout, or OS 9
heap fragmentation. No Retro68 or hardware run was performed: production changes
are confined to X11 and leave the portable core and Classic sources unchanged.
