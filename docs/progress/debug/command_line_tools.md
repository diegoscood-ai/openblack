# Command-line tools

The small programs in `apps/` for looking inside the game's files and making new ones, each built on the file-format
component in `components/` that the game reads the same files with.

**Progress: 13/23 done, 0 partial — 57%**

How the original does it, in our wiki: [Tools and formats](../../bw1-notes/tooling.md), [openblack internals](../../bw1-notes/openblack-internals.md).

## Tools

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| packtool: list a pack's blocks, view a block's bytes, the info block, meshes, textures, animations and sounds | done | `apps/packtool/packtool.cpp`, `components/pack` |
| packtool: extract a block, and write raw data and mesh packs | done | `apps/packtool/packtool.cpp` (extract, write-raw, write-mesh, write-animation) |
| l3dtool: print a mesh's header, skins, extra points, bones, footprints, names and metrics | done | `apps/l3dtool/l3dtool.cpp`, `components/l3d` |
| l3dtool: write a mesh from glTF and extract a mesh to glTF | done | `apps/l3dtool/l3dtool.cpp` (write and extract through glTF) |
| anmtool: list an animation's keyframes and their contents | done | `apps/anmtool/anmtool.cpp`, `components/anm` |
| anmtool: write an animation from glTF | done | `apps/anmtool/anmtool.cpp` (write) |
| lndtool: print a land's low resolution textures, blocks, countries, materials and the rest | done | `apps/lndtool/lndtool.cpp`, `components/lnd` |
| lndtool: write a land from a height map, noise and bump maps | done | `apps/lndtool/lndtool.cpp` (write) |
| glwtool: list glows and their contents, and read and write them as JSON | done | `apps/glwtool/glwtool.cpp`, `components/glw` |
| camtool: print a camera path's points, and write one from points | todo | no camtool in our tree; `components/cam` reads the paths |
| morphtool: print the creatures' morph files, animation sets, hair groups and extra data | done | `apps/morphtool/morphtool.cpp`, `components/morph` |
| morphtool: writing morph files back | todo | read only |
| creaturemindtool: dump creature mind files, with their learning episodes, and check each writes back byte for byte | todo | no creaturemindtool in our tree; `components/creaturemind` reads and writes the files (tests `CreatureMindFile.*`) |
| lhvmtool: read a compiled script program: its scripts, autostarts, global stack and data | done | `apps/lhvmtool/lhvmtool.cpp` |
| lhvmtool: decompile it into source, naming constants from the script headers, on the recorded source lines, with how well it went | done | `apps/lhvmtool/lhvmtool.cpp` (decompile), `components/lhvmdecompiler`; test `test/lhvm/test_lhvm_decompiler.cpp` |
| lhvmtool: compile source back into a program | done | `apps/lhvmtool/lhvmtool_compile.cpp`, `components/lhvmcompiler` |

## Files without a tool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gesture templates | todo | read through `components/gestures` (`GestureFile`) by `GestureTemplatesLoader` (`src/Resources/Loaders.cpp`, `src/Magic/Gestures/GestureTemplates.cpp`), no tool |
| Particle effect files | todo | read through `components/psys` (`ParticleFile`, enum headers, stacked bitmaps) by `src/Particles/PSysFile.cpp`, no tool |
| Raw images (the game's textures outside packs) | todo | read by `components/rawimage`, no tool |
| The game's info tables | todo | read by `src/Parsers/InfoFile.cpp`, no tool |
| The game's fonts | todo | read by `src/Graphics/GameFont.cpp`, no tool |
| Saved games and profiles | todo | not read at all yet |
| Sound banks outside packs, and the dance and footpath files | todo | (unconfirmed which of these need their own tool) |
