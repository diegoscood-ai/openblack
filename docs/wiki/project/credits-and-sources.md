# Credits and sources

## Summary

The projects, repositories, tools and libraries this reconstruction is built on or learnt from, with links. Where a
finding or a piece of code comes from one of them, the page that uses it says so.

Most of these people work on Black & White for the love of the game, so that it can be played again, and played
better. Every page that uses their work says so, and this page is our thanks to all of them.

## The original

- **Black & White** (Lionhead Studios, 2001), version 1.42 (the community's unofficial patch) with two community
  patches on top, Infinite Drawing Distance and the Creature Growth Patch
  ([Which exe is the original](reading-the-wiki.md#which-exe-is-the-original)), is the behaviour this wiki describes.
  Its data and executable are not part of this repository: every check reads the user's own copy.

## openblack

### The code base

| Project | What we use it for |
|---|---|
| [openblack/openblack](https://github.com/openblack/openblack) | The upstream open-source re-implementation of Black & White. This fork is one commit on top of its `master`, and stays comparable with it |
| [raffclar/openblack](https://github.com/raffclar/openblack) | raffclar's tree, whose architecture is the base of this reconstruction (layout, names, the service locator, systems and their tests). His `master` at `0222f9f5` was the fixed port target of 2026-10-11 ([Port audit](port-audit.md), kept as history); since 2026-10-20 a piece of his is taken only when it closes one of our open rows or systems, saves us work on one, or fixes a fidelity difference ([Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)) |
| [diegoscood-ai/openblack](https://github.com/diegoscood-ai/openblack) | This fork. Its branches: [`AI-Decompiled`](https://github.com/diegoscood-ai/openblack/tree/AI-Decompiled), the published reconstruction (one commit on upstream `master`); [`coordination`](https://github.com/diegoscood-ai/openblack/tree/coordination), the progress and coordination notes for collaborators; [`master`](https://github.com/diegoscood-ai/openblack/tree/master), the mirror of upstream openblack the fork started from; and [`bw-replica`](https://github.com/diegoscood-ai/openblack/tree/bw-replica), the earlier history of the reconstruction (to 2026-10-04), kept for reference. Its issues track the open work |

### Reverse engineering

| Project | What we use it for |
|---|---|
| [openblack/bw1-decomp](https://github.com/openblack/bw1-decomp) | The matching decompilation of `runblack.exe` (version 1.20 symbols). Read first for every function before the disassembly; where the 1.42 patch changed code, the exe's bytes win |
| [decomp.dev](https://decomp.dev) | bw1-decomp's progress figures (read-only) |
| [BW Land Editor](https://github.com/Daniels118/BWLandEditor) by Daniels118 ([site](https://daniels118.github.io/BWLandEditor/), GPL-3.0) | A graphical editor for Black & White landscapes and LHX data; a source for the land, country and material formats ([Land and script files](../formats/land-and-script-files.md)) |
| The creature mind editor (`bw_creature_editor.py`, `bw_action_catalog.py`), by **Gwenteley** (openblack Discord), who shared it and is improving it so the whole community can edit their creatures' minds; not on GitHub | Reading and editing `.erc` creature mind files and the 328 creature action ids ([The creature's mind](../creature/mind.md)) |
| [Black & White Game Network (bwgame.net)](https://www.bwgame.net/downloads/categories/black-white-1.1/) | The community site, with old Black & White downloads (patches, tools, maps, mods) and documentation; a source of period material. Files from it are read from the user's own copies, never committed |
| Our own tools | xref, the oracle, precheck and hashdiff ([Research tools](research-tools.md)) |

### Libraries in the build

| Library | Use |
|---|---|
| [bgfx](https://github.com/bkaradzic/bgfx) (with bx, bimg) | The renderer, on Direct3D 12, Vulkan and Metal |
| [EnTT](https://github.com/skypjack/entt) | The ECS registry, the service locator and events |
| [SDL2](https://github.com/libsdl-org/SDL) | The window, input and platform layer |
| [OpenAL Soft](https://github.com/kcat/openal-soft) | Audio output |
| [Dear ImGui](https://github.com/ocornut/imgui) | The debug windows |
| [spdlog](https://github.com/gabime/spdlog) and [{fmt}](https://github.com/fmtlib/fmt) | Logging and formatting |
| [GLM](https://github.com/g-truc/glm) | Vector and matrix maths |
| [stb](https://github.com/nothings/stb) | Image helpers |
| [cxxopts](https://github.com/jarro2783/cxxopts) | The command line |
| [minizip](https://github.com/madler/zlib/tree/develop/contrib/minizip) (zlib contrib) | Reading packed archives |
| [GoogleTest](https://github.com/google/googletest) | The unit tests |
| [vcpkg](https://github.com/microsoft/vcpkg) | Building the dependencies |

### Credited code

- The **Bink decoder** is openblack's own, but its tables and coefficient order follow
  [FFmpeg](https://github.com/FFmpeg/FFmpeg)'s Bink decoder (Konstantin Shishkov, Peter Ross; LGPL-2.1-or-later,
  used under GPL-3). FFmpeg itself is no longer used
  ([Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)).
- The **audio decoders** are openblack's own, written to give the same samples as the libraries before them: the wave
  header rules, the conversions to 16 bits and the ADPCM decoders follow dr_wav 0.14 (David Reid; public domain or
  MIT-0), and the MPEG frame search, tables and order of operations follow minimp3 (lieff; CC0 1.0) as dr_mp3 0.7
  carries it (`src/Audio/Codec/WaveFile.cpp`, `src/Audio/Codec/MpegAudio.cpp`). dr_libs itself is no longer used
  ([Our own audio decoders](milestones.md#our-own-audio-decoders)).

### This wiki's viewer

- [marked](https://github.com/markedjs/marked) (MIT) renders the pages, and [highlight.js](https://github.com/highlightjs/highlight.js)
  (BSD-3-Clause) the code. Both are bundled in `docs/vendor/` with their licences.

## Pending

- Add any new source here when a page starts citing it.
