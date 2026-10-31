# Milestones

## Summary

The project's important steps, newest first: what changed, when it landed (the date and short hash of its commit in
this repository), and why. The [progress dashboard](dashboard.md#milestones) and the repository's front page show the
latest ones. A milestone is added here when it lands; the date is the landing commit's.

<!-- milestones:table -->
| Date | Milestone | Commit | Why |
|---|---|---|---|
| 2026-10-22 | [The checks read the original's meshes](#the-checks-read-the-originals-meshes) | — | The user's choice: the bigger mesh pack is a mod, and no check runs with a mod on; the meshes decide which trees a land has |
| 2026-10-22 | [Mods: the SDK's version, the Mods page and data mods](#mods-the-sdks-version-the-mods-page-and-data-mods) | `bf145e334` | The mod loader's first uses: mods switched in the game's menu, their files read in place of the game's |
| 2026-10-21 | [Faster builds](#faster-builds) | `42b3448ec` | A build of an unchanged tree, or a switch of worktree, no longer redoes work it has done |
| 2026-10-21 | [The leashes a creature knows in the state hash](#the-leashes-a-creature-knows-in-the-state-hash) | `92883235d` | A change to what a creature knows can no longer pass the checks unseen |
| 2026-10-21 | [The top detail level draws the high meshes, chosen from Maximum Detail](#the-top-detail-level-and-maximum-detail) | `e60705658` | The user's chosen differences: the villagers', animals' and birds' high meshes, picked from the options box |
| 2026-10-20 | [Recreate the original, on raffclar's architecture](#recreate-the-original-on-raffclars-architecture) | — | The goal is the original, 1:1 and fast; his pieces come in only where they close our rows |
| 2026-10-20 | [A town's belief is part of the state hash](#a-towns-belief-in-the-state-hash) | `c5fe40ea3` | A change to the belief of a town can no longer pass the checks unseen |
| 2026-10-16 | [Staying on bgfx while upstream moves to SDL3 and slang](#staying-on-bgfx) | — | The renderer the reconstruction is measured on stays |
| 2026-10-16 | [Bullet physics removed](#bullet-physics-removed) | `fafd05625` | The game's own physics and rays replace it |
| 2026-10-15 | [The original's x87 arithmetic, modelled](#the-originals-x87-arithmetic) | `89ba855d8` | Some rules give the original's bits only with its floating point |
| 2026-10-15 | [The frozen v1.42 exe is the original](#the-frozen-v142-exe-is-the-original) | — | One reference that every check and every study reads |
| 2026-10-07 | [Our own Bink video decoder; FFmpeg removed](#our-own-bink-video-decoder) | `11a0f2d72` | The films decode in openblack's own code, frame-identical to the original's |
| 2026-10-07 | [Our own audio decoders; dr_libs removed](#our-own-audio-decoders) | `4e109add5` | Every sound and music segment decodes in openblack's own code, with the same samples |
| 2026-10-07 | [The minimal ModLoader.dll](#the-minimal-modloaderdll) | `72110f2b1` | The one seam a real mod SDK will plug into, once the game is reconstructed |
| 2026-10-04 | [The per-turn state hash](#the-per-turn-state-hash) | `1490e2423` | A change is accepted only when the game's state stays identical, turn by turn |
<!-- /milestones:table -->

## The milestones

### The checks read the original's meshes

- **What:** from joint180 on, every check reads the original game's own `AllMeshes.g3d` (13 MB) where it read the
  user's mesh pack (49 MB) before; the pack is played only as a data mod
  ([The minimal mod loader](mod-loader.md#data-mods-a-mods-files-replace-the-games)). The change moved every reference
  once.
- **When:** a decision, not a commit: 2026-10-22, with joint180 (the checks' copy of the game is outside this
  repository; `805415e19`, the same day, describes the trees it moves).
- **Why:** the user chose to make the mesh pack the first data mod (2026-10-21) and to run no check with a mod on
  (2026-10-22). The meshes matter to play as well as to the look: the land script's trees are refused where a fixed
  object's mesh box takes their spot, so the mesh set decides which trees a land has, and with the pack's bigger
  meshes 25 of Land 1's trees were never made ([Trees and forests](../world/trees.md#general)).

### Mods: the SDK's version, the Mods page and data mods

- **What:** the mod loader's first steps:
  - `f7820657c` (2026-10-21, "Mods: the game and the loader agree by the SDK's version, a hash of the SDK's headers;
    the loader lists the mods and keeps their switches"), on `0cfd1c21e` (the manifest and the list of the mods switched
    on, read as pure rules of their text): the game uses `ModLoader.dll` only when the library carries the game's own
    SDK version, a hash of every header of `src/ModSdk/` made at build time, and the library reads each mod's manifest
    and keeps which mods are switched on;
  - `07c24f9c1` (2026-10-21, "Mods: the options' Mods page lists the mods with a switch each, kept by the loader"): the
    Mods page, a 🎯 difference the user chose;
  - `bf145e334` (2026-10-22, "Mods: a switched-on mod's files replace the game's files of the same path"), on
    `43d1f9b8e` (the loader gives the switched-on mods' folders): data mods, a 🎯 difference the user chose.
- **Why:** the first uses of the minimal library ([below](#the-minimal-modloaderdll)), each the user's choice; without
  the library, or with the checks' empty mods folder, the game is unchanged ([The minimal mod loader](mod-loader.md);
  [chosen differences](reconstruction.md#what-is-deliberately-different--chosen-differences)).

### Faster builds

- **What:** four changes to how openblack is built, none of them to the code generated:
  - MSVC's outputs are reproducible (`/Brepro`): objects, libraries and executables carry no time stamps, so two builds
    of one commit in one folder give byte-identical files, and an unchanged executable can be told by its bytes.
    `9afb5d76a` (2026-10-21, "Build: reproducible MSVC outputs").
  - vcpkg installs only when what it installs from changed since the last install in the build folder: the manifest,
    the patched ports, the vcpkg baseline, the triplet or the compiler (the step took 40-60 s of every CMake run);
    `OPENBLACK_VCPKG_INSTALL=ALWAYS` keeps the old behaviour. `42b3448ec` (2026-10-21, "Build: vcpkg installs only
    when what it installs from changed"). `dfeab383f` (2026-10-23, "Build: a stopped vcpkg install leaves no stamp
    behind") closed two gaps of that step: an install that failed or was stopped kept its stamp, so the next CMake run
    skipped the install of packages it may already have removed, and a new build folder's second CMake run always
    installed again.
  - five headers name what they need without including it, so a change to one of them recompiles fewer files:
    `66db4ca6a` (the registry's land context), `615a2d479` (the loaders' resources), `f2bf8a961` (the leash rules and
    the idle mind), `1536e186a` (a physics material row in a header of its own) and `2bed7c5ba` (the friendship
    header and the creature's attitude), all landed on 2026-10-21.
  - the local script that switches a built worktree to another branch refreshes git's index after it sets the files'
    times, which stopped about 361 needless recompiles per switch (a tool outside this repository, 2026-10-21).
- **Why:** every change is built and checked several times before it lands
  ([Testing and verifying a change](testing.md)), so build time is most of the time a change takes
  ([Build, test, run](debugging.md#build-test-run)).

### The leashes a creature knows in the state hash

- **What:** the "creature" part of the per-turn state hash folds the leashes each creature knows, one bit per leash
  type, after its home. `92883235d` (2026-10-21, "Debug: the state hash's creature part keeps the leashes each
  creature knows"); `hashdiff.py` names the new value `leashesKnown`.
- **Why:** a creature made from its mind file knew the file's leashes at a different turn from the lesson scripts', and
  every check stayed identical. A new value moves every reference once; afterwards a change to what a creature knows
  shows in the hash
  ([the "creature" part](../creature/players-creature.md#what-openblack-does-with-the-players-creature)).

### The top detail level and Maximum Detail

- **What:** at detail level 6, the top one, the villagers, animals and birds are drawn with their high meshes, and the
  options box's top entry, Maximum Detail, sets that level. Both are 🎯 differences the user chose (the high meshes only
  at level 6, 2026-10-15; the menu entry, 2026-10-20 and 2026-10-21):
  - `0194b5339` (2026-10-21, "Graphics: at the top detail level the villagers, animals and birds are drawn again"):
    at level 6 they had not been drawn at all; `3c79c66a7`, `cfd009492` and `4e5d2fa8d` (2026-10-21) give each
    SuperVillager actor its own high mesh, what a high-detail actor shows, and a skeleton's look at that level;
  - `833878872` (2026-10-21, "Options: the box's Maximum Detail entry sets the top detail level"): the entries 0 to 3
    set the levels 0 to 3 and Maximum Detail sets level 6, taken as the Options page is left;
  - `e60705658` (2026-10-21, "Options: leaving the page on Maximum Detail always sets the top detail level"): also
    from the default level 4, which already shows Maximum.
- **Why:** the user's choice; the original draws the high meshes at every level, with its level of detail loading
  disabled ([Visible blocks and level of detail](../rendering/visible-blocks-and-lod.md#openblack);
  [chosen differences](reconstruction.md#what-is-deliberately-different--chosen-differences)).

### Recreate the original, on raffclar's architecture

- **What:** the goal became recreating the original game as fast as possible, behaving 1:1 apart from the
  differences the user chooses. raffclar's architecture stays ours, but the two trees are not going to be merged and
  his master is no longer imported as a whole: only a piece of his that closes one of our open rows or systems, saves
  us work on one, or fixes a fidelity difference is taken, and it is measured against the exe like any of our commits.
  This replaced the fixed target of 2026-10-11 ([the port audit](port-audit.md)) and the import of his newer master of
  2026-10-17.
- **When:** a decision, not a commit: the user's, on 2026-10-20 (the project's working rules, "Taking pieces from
  raffclar").
- **Why:** the two trees have different goals (his includes reverse engineering for its own sake and a renderer
  without bgfx), so the work goes to what brings the original back soonest
  ([Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)).

### A town's belief in the state hash

- **What:** the belief each player has in each town became a part of the per-turn state hash, so the fidelity checks
  compare it. `c5fe40ea3` (2026-10-20, "State hash: a town's belief is part of it"); `ce7e4eb7e` (2026-10-20) made
  `hashdiff.py` name the belief values that move.
- **Why:** a new part moves every reference once, and afterwards a change to a town's belief shows in the hash
  ([What is compared](testing.md#what-is-compared)).

### Staying on bgfx

- **What:** upstream openblack replaced bgfx and moved to SDL3 and the slang shader compiler on 2026-10-16 (in
  upstream's history: `9dfde720b` "SDL: Update to SDL3", `22ab22936` "Rendering: Remove bgfx from project",
  `c22dc9f5a` "slang: Set host to compile shaders"). raffclar's master, which our tree follows, has not taken them (his
  master of 2026-10-22, `e1cc0b8f5`, still builds on SDL2 and bgfx), and neither has ours: the renderer stays on bgfx
  ([credits](credits-and-sources.md)).
- **Why:** the project's rules ask for the original's visuals built "with modern BGFX and C++ techniques" (the
  repository's `CLAUDE.md`). **To confirm:** no dated decision to stay on bgfx is written in the repository; the date
  above is the date of upstream's move.

### Bullet physics removed

- **What:** the Bullet physics library left openblack: the game's own physics moves the bodies, and the land and the
  objects under a pixel come from the original's own rays over the island. `fafd05625` (landed 2026-10-16, written
  2026-10-14, "Physics: Bullet leaves openblack").
- **Why:** physics is openblack's own code ([Reconstruction](reconstruction.md);
  [Bullet removed](port-decisions-engine-world.md#bullet-removed-his-834f657f-under-our-names);
  [Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)).

### The original's x87 arithmetic

- **What:** a pure helper that works out floating point as the original's FPU does: every operation rounded to a
  float's 24-bit mantissa, the exponent kept wide, sine and cosine unrounded until the first operation
  (`src/Common/GUtilsX87.h`). `89ba855d8` (2026-10-15); `cd1f6da1e` (2026-10-20) added the arctangent, arccosine, power
  and logarithm.
- **Why:** the game runs the FPU at 24-bit precision, so some rules give the original's bits only when they are worked
  out the same way (the header of `GUtilsX87.h`). The model and how it was measured are on
  [Floating point](../engine/floating-point.md#the-x87-arithmetic).

### The frozen v1.42 exe is the original

- **What:** "the original" every check and study compares against is the frozen v1.42 unofficial-patch `runblack.exe`,
  with the two community patches it carries, not Lionhead's 1.20
  ([Which exe is the original](reading-the-wiki.md#which-exe-is-the-original)).
- **When:** a decision, not a commit: 2026-10-15, with the two patches kept on 2026-10-17 (the dates the wiki gives).
- **Why:** one fixed reference, so that every measured difference means the same thing.

### Our own Bink video decoder

- **What:** openblack's own decoder of the game's Bink 1 (`.bik`) films, now in `components/bink`. It landed in steps
  on 2026-10-07: `8a8295842` (the bit reader, the format's tables, the Huffman trees and the bundles), `e3f85b4a8` (the
  8×8 blocks) and `11a0f2d72` (`BinkDecoder`, the decoder the game uses). `ffd248787` (2026-10-07, "FFmpeg out")
  removed FFmpeg, and `4c5da9751` (2026-10-11) moved the decoder to `components/bink`. It shows the same pictures as
  the original's Bink library on every frame of the five films ([Bink](../video/bink.md#the-decoder-and-the-binkw32-colours)).
- **Based on:** it is our own code. The format's constant tables are taken from FFmpeg's Bink video decoder, and the
  order of its coefficients and its inverse transform follow it. The container's layout was checked against RAD's
  Bink 1 documentation and FFmpeg's `libavformat/bink.c` (`components/bink/include/BinkFile.h`, line 24).
- **Licence:** openblack is under the GNU General Public License version 3 (the `license` file and every file's
  header). FFmpeg's tables are under the GNU LGPL 2.1 or later and are used here under the GPL version 3, as that
  licence allows. As the sources state it (`components/bink/include/BinkTables.h`, lines 10 to 13):

  > The constant tables of the Bink 1 video format. They are the format's own data and must be exactly these values.
  > They are taken from FFmpeg's Bink video decoder (libavcodec/binkdata.h, Copyright (C) 2009 Konstantin Shishkov),
  > licensed under the GNU Lesser General Public License version 2.1 or later and used here under the GNU General
  > Public License version 3, as that licence allows.

  and `components/bink/src/BinkBlocks.cpp`, lines 10 to 13:

  > The order the coefficients and residues are read in, and the integer inverse DCT, are fixed by the Bink 1 format
  > and follow FFmpeg's Bink video decoder (libavcodec/bink.c and binkdsp.c, Copyright (c) 2009 Konstantin Shishkov,
  > Copyright (C) 2011 Peter Ross), licensed under the GNU Lesser General Public License version 2.1 or later and used
  > here under the GNU General Public License version 3, as that licence allows.
- **Why:** the project decodes the game's media formats with its own code, bit-exact against the output it replaced,
  and FFmpeg is not added back
  ([Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)).

### Our own audio decoders

- **What:** openblack's own decoders for the sound banks and the music (`audio::codec`): WAV/PCM, MS-ADPCM, MPEG layer
  II, and layer III. `1951ab7b2` (2026-10-07, the decoders), `4e109add5` (2026-10-07, the game decodes with them),
  `5ed3f3b88` (2026-10-07, layer III); `2f0a913a9` (2026-10-07) removed dr_libs from the build.
- **Based on:** our own code, written to give the same samples as the libraries before it. As the sources state it,
  `src/Audio/Codec/WaveFile.cpp`: "The header rules, the conversions to 16 bits and the ADPCM decoders follow dr_wav
  0.14 (public domain or MIT-0, by David Reid), which openblack used before, so that every wave decodes to the same
  samples." `src/Audio/Codec/MpegAudio.cpp`: the frame search, the layer II and III tables, the header reading and
  the order of the floating point operations "follow minimp3 (Copyright (c) lieff, CC0 1.0) as dr_mp3 0.7 (public
  domain or MIT-0) carries it". Both files are under openblack's GPL version 3.
- **Why:** as for the video decoder
  ([Third-party libraries replaced by native code](internals.md#third-party-libraries-replaced-by-native-code)).

### The minimal ModLoader.dll

- **What:** the old mod loader and the built-in mods were taken out (`c590b1c16`, 2026-10-06); a minimal mod loader
  library with a small C API stayed, loaded at start-up only when it is there. `72110f2b1` (2026-10-07). Its first
  uses came on 2026-10-21 and 2026-10-22 ([above](#mods-the-sdks-version-the-mods-page-and-data-mods)).
- **Why:** an unfinished mod system spreads hooks, global state and compatibility code; the minimal library is the
  seam a real SDK will plug into ([The minimal mod loader](mod-loader.md)).

### The per-turn state hash

- **What:** a hash of each part of the world state, every turn, with a fixed frame clock, so two runs can be compared
  turn by turn. `1490e2423` (2026-10-04).
- **Why:** a change is accepted when the fidelity runs give an identical per-turn state hash and random trace
  ([Testing and verifying a change](testing.md#behaviour-is-identical-the-fidelity-run)).
