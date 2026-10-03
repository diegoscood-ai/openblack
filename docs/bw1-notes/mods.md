# Modified packs

The `AllMeshes.g3d` of the installation is a mod by the user, not the original pack: here are its differences from the
original (embedded textures, rock origins, villager textures), what was found out about the villager meshes
and the openblack **HD-Tweaks** mod that improves them. The mod library in general is in
[mod-library.md](mod-library.md).

- [The installation's pack](#the-installations-pack)
- [Embedded textures](#embedded-textures)
- [Mesh origin (floating rocks)](#mesh-origin-floating-rocks)
- [Villagers: meshes and textures](#villagers-meshes-and-textures)
- [HD-Tweaks mod](#hd-tweaks-mod)
  - [Options](#options)
  - [Live reload and technical details](#live-reload-and-technical-details)
  - [Tests](#tests)
  - [Status](#status)

## The installation's pack

- The current `Data\AllMeshes.g3d` is a mod by the user (626 meshes, textures 0x01–0x70). There is no copy of the base
  pack.
- References for original meshes: the Creature Isle pack (`...\CreatureIsle\Data\AllMeshes.g3d`, **different
  indices**: check its `AllMeshes.h`).
- The `Ultimate\Data\AllMeshes.g3d` pack is another mod (704 meshes).

## Embedded textures

- The palm trees (meshes 586–589) carry their texture embedded with id **0x1001**, but their material asks for 0x85–0x88, which do not
  exist in the pack. Ultimate repeats the pattern. In the original game they look fine; openblack uses the mesh's embedded
  skin when the material does not find its texture.
- In `LH3DMesh::Create` (0x806460) the embedded skins are registered with `fn_008379E0`, and the value 0x1001 could be
  format + id (**inferred**).
- Scanner: `python C:\Users\diewgarc\dev\documentacion\mod\scan_skins.py <pack.g3d> <salida.txt>` lists the meshes whose
  materials ask for textures that are neither in the pack nor embedded.

## Mesh origin (floating rocks)

- The mod's rock meshes have their origin shifted relative to the original ones:
  `MSH_Z_SPELLROCK01` lowest vertex +0.8 (original −0.6), `MSH_BOULDER3_LIME` +0.2 (original −0.2).
- The script altitudes are designed for the original meshes, so with the mod they end up in the air (in
  Land1, 74 of 243 static objects float more than 5 cm). The original does not correct it.
- In addition, many of the mod's rocks end up resting on a tip because of their rotation: even though the lowest vertex touches the ground,
  they seem to float. Planned solution: physics (**not checked** whether it has already been done).

## Villagers: meshes and textures

Research for the "hd people" mod (2026-09-29). Scripts in `C:\Users\diewgarc\dev\documentacion\hdpeople\` (compare.py,
anims.py, summary.txt).

**Meshes**

- `MSH_P_*` meshes 413–524 of the base pack (geometry identical to Creature Isle's, so the user's mod does not touch
  them). Each tribe and sex has **its own mesh**, in 3 levels: `_1` ~260–300 vertices, `_2` ~110–150, `_3`
  26–40. There is one texture per tribe, shared by man and woman (0x4E–0x5C). Some meshes repeat geometry with
  another texture (TIBETAN=TIBT, JAPANESE=JAPN, SHAOLIN_MONK=JAPN_M_A_1, TAN/WHITE girls, INTRO_M=CULT_PRIEST).
- `MSH_P_INTRO_M`/`_F` (483/484) **do not have more polygons**: 258 v / 348 t, like a normal villager. INTRO_M is the
  CULT_PRIEST geometry with texture 0x59; INTRO_F uses the female villagers' skeleton. Their textures are 256².
  `INTRO.bik` is a pre-rendered video: its models are not in the files.
- Skeleton: 110 of the 112 meshes have the same 22-bone hierarchy (EGPT_M_B_2 has 21), and all the
  `M_P_*` animations of AllAnims.anm (232) are 22-bone. The rest poses vary a little (groups: 85 meshes,
  12 female, 10, CULT_PRIEST+INTRO_M).
- Which mesh is drawn: the original, always LOD 1 (the LevelOfDetail loads are disabled): `stdDetail` /
  `childMeshMedium` for villagers (about 5.7 KB of mesh versus 12 KB for the high one) and `std` for animals. openblack
  does the same since 2026-09-30 (**faithful**); the high mesh, with twice the triangles, only with the HD-Tweaks mod
  `detail = high` (`ECS/DetailMeshes`). History: previously openblack used only `highDetail` (no LOD) and drew the
  villagers in the rest pose (Renderer.cpp, "Get animation frame instead of default"; L3DAnim loaded AllAnims but
  there was no playback, today see [animation.md](animation.md)).

**Textures** (contact sheet in `dev\documentacion\hdpeople\tex\sheet.png`)

- All are **native** 256² atlases. The ones the user's pack has at 512 or 1024 are upscales with
  duplicated pixels (the error versus doubling their half is < 1.5 levels), except 0x5A and 0x47 (native 512).
- INTRO_M/F look finer because their 256² texture is of **a single** character (the villagers: 4 per atlas).
  No pack (base, Creature Isle, Ultimate) comes with better ones.
- **The Norse atlas of the user's pack (0x5A, 1024 px) is not the original**: it has the intro characters
  painted on it (the face of the blonde woman of INTRO_F, the bearded man, one with a red shirt and jeans).
  The Creature Isle Norse atlas (skin 0x74 of its pack, `MSH_P_NORS_F_A_1` = 580 there) has the original clothing
  (dark dress, men in black with a belt). That is why the female villagers of Land1 (Norse village) look like "the ones from the
  intro": the mesh (`NORS_F_A_1`, 498) and the clip (`M_P_Walk_Woman`) are the correct ones. The HD textures of
  graphics.hd-tweaks came from that atlas. Comparison in `dev\documentacion\hdpeople\tex\norse_cmp.png`.

## HD-Tweaks mod

**Mod/own**: `graphics.hd-tweaks` (2026-09-29/30), formerly `graphics.hd-people`; renamed because it is no longer only for
villagers. Options table in [mod-library.md](mod-library.md). Everything is applied **live** (without restarting),
disabled by default like every mod.

### Options

- `textures` hd/original: the 18 villager atlases and the 5 animal ones (0x2-0x5, 0x64; 2026-09-30) ×4
  (Real-ESRGAN), `Resources/HdTextures` + `textures.json` (FNV-1a hash of the source DDS: with another AllMeshes.g3d they are not
  used). The animal atlases are shared by some objects (gravestones, a gate, a tipi...), which also come out in
  HD.
  - Generation: Real-ESRGAN `realesrgan-x4plus` (the anime model flattens the painting) from the native resolution, with
    `python assets\mods\graphics.hd-tweaks\tools\make_textures.py <AllMeshes.g3d> <AllMeshes.h> <carpeta del mod>`
    (portable Real-ESRGAN in `C:\Users\diewgarc\dev\herramientas\realesrgan`, ~3 min with the GPU); they are not in git. The
    generator reads the `MSH_P_*` and `MSH_A_*` meshes from the game's AllMeshes.h and reuses the images already made whose
    hash is still that of the pack.
- `smooth` off/soft/round: PN triangles (`3D/PnTessellation`, Vlachos 2001) split into 4 or 9 on the
  villager and animal meshes (with bones and all their textures in the list) **and the hand** (`Hand_Boned_Base2`).
  `L3DSubMesh::IsHdTweaked`.
  - The **joint triangles** (corners on different bones) are not curved inside: they are a fan over
    their single-bone edge and stretch like the original ones (with interior points stuck to one bone they
    bent when animating).
  - Collision (hand, physics) is still the original mesh.
- `light` smooth/original: the original's lighting (the entire rule of `fn_0084BA90` with the same functions from
  `assets/shaders/model_light.sh`, the same ambient and the same light; see
  [Model lighting](rendering-objects.md#model-lighting)) computed per pixel in `fs_object` with the smooth
  normals (`u_window.y`), only on lit instances like the original (not reflections or shadows). Per pixel the direction
  is taken in the world, from the pixel to the light, instead of in mesh space from the bone origin
  **(approximate)**: there is no free varying left. It only matches with a distant light (by day, the sun at 500000); in the middle of the night the
  light is 3 units from the hand and on a nearby villager the shading changes visibly (known night-time difference
  of the mod).
  **Tried and discarded**: a rim of light on the silhouette, 0.8·(1−N·V)² ("rim"); the user found it ugly.
- `sharp` on/off: mip bias −1 on those textures (`u_window.z`).
- `detail` high/original: villagers and animals with their high mesh (`ECS/DetailMeshes`); without the mod, the original's
  LOD 1 (see [Villagers: meshes and textures](#villagers-meshes-and-textures)).

### Live reload and technical details

- **Live reload** (`resources::hd_tweaks::Update`, at the start of `Game::Update`): if the options change it rereads
  AllMeshes.g3d and reloads only the villager textures, the boned meshes that use them and the hand (~0.6 s when
  enabling, ~0.15 s when disabling; the PNGs are decoded in parallel, also at startup). `detail_meshes::Update`
  changes the mesh of the villagers and animals that already exist.
- **Visibility at a distance**: at 20-40 m a villager measures 40-70 px and the ×4 textures alone are barely noticeable; what is
  noticeable is the round shape + per-pixel lighting + `sharp`.
- **32-bone shader variants** (map session, `vs_object_instanced_b32.sc`): they include `vs_object.sc` and use
  `fs_object`, so the mod works the same way through that path (checked with screenshots).
- **Old folder**: if `Mods/graphics.hd-people` appears (an old exe or a copy of `Mods`), `ModRegistry`
  (`MigrateRenamedFolder`) moves into `graphics.hd-tweaks` whatever it is missing and deletes it; it never shows up as a data mod.

### Tests

- `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` changes the options mid-game.
- `dev\herramientas\shot_villager.sh`, `dev\herramientas\shot_hand.sh` and `dev\herramientas\shot_animal.sh <n,distancia,ángulo,1>` (private
  copy in `dev\hdp_run`; animals are only followed with the game running, without START_PAUSED).
- `OPENBLACK_START_PAUSED=1` keeps the villagers still for A/B comparison; `OPENBLACK_TEST_ANIM=<clip>,<ms>` for a
  pose (sitting 369, praying 343).
- Screenshots at frame 2900 sometimes fail: repeat.

### Status

- **Checked by the user** (2026-09-30): with `round` the animations no longer break, nor do the animals' ones.
- **Pending**: check in game whether `sharp` flickers in motion.
