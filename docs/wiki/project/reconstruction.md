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
- **Architecture and code (raffclar's guidelines).** Modern, idiomatic C++20, not the original's structure:
  - no globals or singletons: game state lives in an ECS (EnTT), with entities, components and systems, and services
    are reached through a service locator by their interface;
  - logic that the original did inline (formulas, state machines) is pulled out into **pure functions** free of global
    state, behind small wrappers, so that unit tests can pin it to the original's numbers with fakes;
  - resources are owned with RAII, and `std::optional`, `std::span`, `enum class` and `constexpr` are used where they
    fit; one-way notifications are events;
  - names, files and layout follow openblack's conventions. The original's function names and addresses appear only in
    this wiki, never in the code's comments.

  The full rules are in the repository's CLAUDE.md and in [Refactor conventions](refactor-conventions.md).

### What is deliberately different: 🎯 chosen differences

A few visible differences from the original are chosen on purpose by the project. Each is recorded, with its date, on the page it
belongs to. For example, the villagers' and animals' high-detail meshes are used only at the top detail level
([Meshes and level of detail](../rendering/meshes-and-lod.md)). Any other difference from the original that is found
is a bug, and it is fixed as its own measured change.

## Pending

- Keep this page's list of 🎯 chosen differences in step with the pages that record them.
