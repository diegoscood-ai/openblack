# A reconstruction, not a decompilation

## Summary

openblack rebuilds Black & White so that it **behaves exactly like the original** (the v1.42 `runblack.exe`), not
so that its code is a copy of the original's code. The goal is a 1:1 result: the same game state turn by turn, the
same random numbers, the same pictures and sounds. How openblack gets there is free. The engine is a modern one, and
in several places it works very differently from the 2001 engine.

This wiki describes the original in detail (addresses, formulas, order of operations) because exact behaviour needs
exact knowledge. The "openblack" section of each page says how our code reaches the same result, which is often by a
different route.

## The original

The original is a single-threaded Windows program of 2001. Game logic and drawing share one thread, and it draws
through Direct3D 7 with fixed-function stages, software lighting and a CPU-side sort of what it draws. For what
"the original" means here, see [Reading this wiki](reading-the-wiki.md).

## openblack

### What must be identical

These are checked on every change ([Testing and verifying a change](testing.md)):
- the **game state** after every turn: positions, states, timers, every entity, through the per-turn state hash;
- every **random number** drawn, on every stream, in the original's order ([Random numbers and streams](../engine/random.md));
- the **pictures** of the land views, by day and by night, and of the scripted scenes, pixel for pixel against
  references;
- the **scripted hand demos**, and the samples every sound decodes to.

Every check reads one frozen copy of the game. Since joint180 its `AllMeshes.g3d` is the original's own (13 MB), and
the user's bigger mesh pack (49 MB) is played only as a data mod
([The minimal mod loader](mod-loader.md#data-mods-a-mods-files-replace-the-games)). No check runs with a mod on. The
meshes change play too: with the pack's bigger meshes, 25 of Land 1's trees are never made
([Trees and forests](../world/trees.md#general)).

A rule from the original is ported with its exact numbers and float behaviour, even where a modern engine would
round differently ([Floating point](../engine/floating-point.md)).

### What is deliberately different: the engine

The engine is free to differ from the original **as long as the results stay identical**. This follows the guidelines
raffclar set for the project (his collaboration guide, CLAUDE.md in the repository): modern game-engine patterns and
shader techniques, not the original's design patterns, with the original's visuals and behaviour preserved:
- **Graphics.** openblack draws through bgfx on modern graphics APIs (Direct3D 12, Vulkan, Metal), with programmable
  shaders on the GPU. The original's fixed-function Direct3D 7 stages are reproduced in shaders where the picture
  must match, not emulated call by call. See [The frame and its draw stages](../rendering/frame.md).
- **Threads.** The game turn and the drawing may run on separate CPU threads, and draw work may move to the GPU,
  only with identical results. The original's local random stream, its C runtime random numbers and the particle
  selection are used by both the turn and the frame, so a split must keep their order. A one-frame display delay is
  accepted if threads need it, and a single-thread, no-delay mode stays as the test reference. Some work already runs
  on its own threads (for example the music stream's decoder).
- **Libraries.** Physics, video and audio decoding are openblack's own code. Bullet, FFmpeg and dr_libs were removed
  ([Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)).
- **Architecture and code (raffclar's guidelines).** Modern, idiomatic C++23, not the original's structure:
  - no globals or singletons: game state lives in an ECS (EnTT), with entities, components and systems, and services
    are reached through a service locator by their interface;
  - logic that the original did inline (formulas, state machines) is pulled out into **pure functions** free of global
    state, behind small wrappers, so that unit tests can pin it to the original's numbers with fakes;
  - resources are owned with RAII, and `std::optional`, `std::span`, `enum class` and `constexpr` are used where they
    fit; one-way notifications are events;
  - names, files and layout follow openblack's conventions. The original's function names and addresses appear only in
    this wiki, never in the code's comments.

  The full rules are in the repository's CLAUDE.md and in [Refactor conventions](refactor-conventions.md).

<a id="what-is-deliberately-different-chosen-differences"></a>

### What is deliberately different: 🎯 chosen differences

A few visible differences from the original are chosen on purpose by the user. Each is recorded, with its date, on the
page it belongs to, and listed here:

| Chosen | What openblack does | The original | Recorded on |
|---|---|---|---|
| 2026-10-15 | The villagers', animals' and birds' high-detail meshes are drawn only at detail level 6, the top one | Draws them at every level, with its level of detail loading disabled | [Meshes and level of detail](../rendering/meshes-and-lod.md#villager-blobs-object-reflections-and-lod) |
| 2026-10-20 | The options box's top detail entry, Maximum Detail, sets level 6 (the entries 0 to 3 set the levels 0 to 3), taken as the Options page is left | Its entry is the detail index itself, taken at the next start | [Visible blocks and level of detail](../rendering/visible-blocks-and-lod.md#openblack) |
| 2026-10-21 | Leaving the Options page on Maximum Detail always sets level 6, also from the default level 4, which already shows Maximum | (as above) | [Visible blocks and level of detail](../rendering/visible-blocks-and-lod.md#openblack) |
| 2026-10-21 | openblack starts at detail level 6, the top one (`detail_level::k_Start`, the `--detail-level` default), and every check and reference runs there from joint172 | It starts at its default level 4 | [Visible blocks and level of detail](../rendering/visible-blocks-and-lod.md#openblack) |
| 2026-10-20 | The mesh pack's textures keep 32-bit colour and their own sizes | Puts every pack texture on the card in a 16-bit slot (ARGB4444 where the card has it) of 256 × 256 | [Textures](../rendering/textures.md#the-mesh-packs-textures-in-openblack) |
| 2026-10-21 | The sun glare's test of what is drawn in front of its samples (detail levels 3 to 6) answers a frame or two late, through occlusion queries | Reads that same frame's 16-bit depth buffer on the CPU | [Sky](../world/sky.md#the-sun-and-its-glare-in-openblack) |
| 2026-10-21 | Four-sample multisampling of the back buffer, a debug switch only (the debug bar's View menu, off by default, so the checks and play draw as the original) | Draws with no anti-aliasing | [The frame](../rendering/frame.md#comparison-with-openblack) |
| 2026-10-21 | With the mod loader library there and its mods folder read, the options of the menu Escape brings up during a game (not the start-up menu) have a sixth tab, Mods, listing each mod's name, version and author with a switch each, kept between runs | Has no mods and five options tabs | [The minimal mod loader](mod-loader.md#the-minimal-mod-loader) |
| 2026-10-22 | A switched-on mod's files replace the game's files of the same path (data mods), only with the mod loader and its mods folder read | It reads only its own folder | [The minimal mod loader](mod-loader.md#data-mods-a-mods-files-replace-the-games) |
| 2026-10-20 | The camera's helper icons by the hand (the tricons) are sized from the width of a 4:3 screen as tall as the screen, so a wide screen does not make them bigger (raffclar's dev#11, taken) | Sizes them from the focal length across the whole width | [The player's camera](../camera/player-camera.md#the-tricons-what-is-shown); [deviations](deviations.md#taken) |
| 2026-10-27 (replaces 2026-10-22) | What moves is drawn between turns as the original draws it, a turn at a time where it steps. A debug switch of the View menu, off by default and not as the original, "Smooth drawing of animals, birds and other gods' hands", draws the animals and birds moving and turning smoothly between turns, and slides what the other gods' hands hold from one turn's place to the next (issue #220) | Draws some kinds a turn at a time | [Animation](../beings/animation.md#drawing-between-turns) |
| 2026-11-02 | Football is on, as if the Soccer Add-On were installed, outside network games only | Football is on only when the add-on's installer has written the registry code BWCheckFeatureIsEnabled("football") checks (0x5259D0 → 0x525A40, its checksum 0x525B80); `GGame::FootballEnabled` 0xD0196C is written only at 0x54F54F, and the feature's multiplayer byte is 0, so a network game keeps it off | [Football](progress/town/football.md#the-add-on-and-its-switch) |
| 2026-11-09 | The gesture icons at the bottom right are placed again when the window changes size (resized, or put full screen), so they keep to the screen's corner | Places them only when the looked-for table changes, for the screen's size then (fn_0068AB70, called only from the list's update fn_0068ABA0 and an icon's init fn_0068B380); no resolution-change path reaches them | [Gestures](../magic/gestures.md#the-gesture-icons) |
| 2026-11-09 | In a window, the challenge and saved-game pictures are of the game's own frame, not the desktop's (full screen they are the same) | Its scripted screenshot (`ScriptedScreenShot::Process` 0x821750) locks the front buffer, which in a window is the desktop: seen 2026-11-09, its saved game's picture shows another application's toolbar and the game's view inside it (`refs/temple/original_window_picture/`) | [The pictures](../interface/temple-interior.md#the-challenge-list) |
| 2026-11-11 | A save made while a living still points at a footpath node a re-plan deleted writes that living on its footpath with no node there, and the save is never refused for it | Its node's `ToBeDeleted` 0x538970 frees the node and leaves the living's pointer to it; `GameOSFile::WritePtr` 0x561E10 then runs `dynamic_cast`, `GetSaveType` and `Save` on the freed memory and writes whatever is there as a new record (bw1-decomp GameOSFile.cpp:1434), which is undefined | [Saved games](../engine/saved-games.md#openblack) |
| 2026-11-10 | The "Pentium 4" boxes at the in-loop logo are never shown | Draws two boxes of `p4.raw` and `p4t.raw` at the bottom left of the in-loop logo when the Intel compiler's CPU indicator reads CPUID base family 15 with SSE2 (`LH3DP3::IsPentium4`, `___intel_cpu_indicator_init` 0x8A2610), which some CPUs report (AMD's base family 0xF among them) | [The in-loop logo](../engine/game-clock.md#the-in-loop-logo) |
| 2026-10-23 | The frame counter ("<fps> FPS  <ms> ms", top right) shows by default while playing, and the debug bar's View menu turns it off; a check run never draws it, so the references are unchanged | Shows no frame counter | [Debugging](debugging.md#the-debug-menu-bar) |

Any other difference from the original that is found is a bug, and it is fixed as its own measured change.

Withdrawn: on 2026-11-02 the user withdrew the choice of 2026-10-28 that the villagers' and animals' game logic read the
placement worked out in the turn ("hazlo como el original todo", "do it all as the original"). The game logic of villagers, animals and the creature
reads the matrix the last drawn frame left, as the original
([Matrices and the Zoomer](../engine/matrices-and-zoomer.md#the-objects-own-placement-in-openblack)).

## Pending

- Keep this page's list of 🎯 chosen differences in step with the pages that record them.
