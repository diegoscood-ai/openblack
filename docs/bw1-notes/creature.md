# The creature: groundwork, random streams and what is unknown

What openblack has of the player's creature so far, and what is known of the original's. The creature code comes from
raffclar's tree (`stack/81-creature-mode` at `ff1e79d1`), ported onto ours first as pure groundwork
(formulas, state machines and file formats, unit tested with fakes) and since then wired into the game: the creature
systems are emplaced in `Locator.cpp` and run every turn and every frame through `ecs::creature_loop` (`ProcessTurn`,
`UpdateFrame`, `UpdateLeash`, called from `Game.cpp`), and the player's creature exists on Land 1, made by the land
script's `LOAD_MY_CREATURE` from the default `--creature-file` ([The player's creature](#the-players-creature)). Most of the creature classes of `runblack.exe` (`Creature`, `CreatureMental`,
`CreatureAction`, `CreatureAgenda`, `CreatureLook`, `CreatureFace`, `CreatureMimic`, `CreatureRoom`, `LH3DCreature`)
have headers in bw1-decomp but no bodies, so most of what follows is his model, not a reading of the original: the
constants that have no source are listed in [Stand-ins](#stand-ins-and-unverified-constants) and
[Pending](#pending).

Progress: [creature](../progress/creature/) (what our tree does of it, row by row).

- [What is ported](#what-is-ported)
  - [The creature tables in info.dat](#the-creature-tables-in-infodat)
  - [The body](#the-body)
  - [The mind and its file](#the-mind-and-its-file)
  - [Learning by watching](#learning-by-watching)
  - [What a creature looks at](#what-a-creature-looks-at)
  - [What frightens a creature, and the fire it believes](#what-frightens-a-creature-and-the-fire-it-believes)
  - [Casting miracles: the rules, not yet called](#casting-miracles-the-rules-not-yet-called)
  - [Movement, body over time, spells, fights, throws](#movement-body-over-time-spells-fights-throws)
  - [Leash and the hand](#leash-and-the-hand)
  - [Drawing the leash](#drawing-the-leash)
  - [Audio, status panel and the follow camera](#audio-status-panel-and-the-follow-camera)
- [The spec file's animation numbers](#the-spec-files-animation-numbers)
- [Random streams](#random-streams)
- [Mimicry and the town hooks](#mimicry-and-the-town-hooks)
  - [What openblack does with a deed](#what-openblack-does-with-a-deed)
  - [Deed table (complete for W120)](#deed-table-complete-for-w120)
  - [Town and villager hooks](#town-and-villager-hooks)
- [Leash natives](#leash-natives)
  - [What openblack does with the leash natives](#what-openblack-does-with-the-leash-natives)
  - [The leash keys](#the-leash-keys)
- [The player's creature](#the-players-creature)
  - [What openblack does with the player's creature](#what-openblack-does-with-the-players-creature)
- [Fights: the camera](#fights-the-camera)
- [Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting)
- [The body posed for the turn](#the-body-posed-for-the-turn)
- [Stand-ins and unverified constants](#stand-ins-and-unverified-constants)
- [Addresses of the original, moved out of the code](#addresses-of-the-original-moved-out-of-the-code)
- [openblack](#openblack)
- [Pending](#pending)

> **Code rules.** The creature's pure logic stays free of global state and draws no random number itself: every draw
> is passed in by its caller, so the systems that come later choose the stream (see [Random streams](#random-streams)).
> No `random_device`, clock or file I/O in it; mind files are bytes in and bytes out, and the loader goes through the
> resource caches. Comments describe behaviour in plain English, with no decompiled names or addresses (those belong
> here). See [openblack-internals.md](openblack-internals.md).

## What is ported

### The creature tables in info.dat

- **`GCreatureInfo`** (one row per species, 0x384 bytes): the 416 bytes that used to be one opaque array are named.
  The species row is at +0x1E4, `startEnergy` +0x1F4, `comfortTemperature` +0x1FC, `growUpMinutes` +0x204,
  `energyDrain` +0x220, `sleepRecover` +0x230, `slowSpeed` / `walkSpeed` / `runSpeed` from +0x240, `runAwayDistance`
  +0x2D8, `sleepLength` +0x364, `pooPerEnergy` +0x370. Four blocks are still unread: +0x234 (0xC bytes), +0x24C
  (0x8C), +0x2DC (0x88), +0x374 (0x10). In the code these offsets survive only as `static_assert`s on the layout.
- **`CreatureActionInfo`**: `strengthGain` / `energyCost` / `exhaustionCost` at +0x0 / +0x4 / +0x8, `desire` +0x98,
  `desireMultiplier` +0xC0. `field0xfc` exists only in patch 1.20 (openblack issue #684).
- **`CreatureInitialDesireInfo`**: 0x1C0 bytes in memory, 0x1B0 in the file, which leaves out the 0x10-byte header;
  our `field0xN` is therefore the original's +0x10+N. Read from `info.dat`:
  - +0x10..+0x2C: the 8 sources of the desire;
  - +0x38 (our `field0x28`): learnable;
  - +0x4C (our `field0x3c`): holds 0.3 to 3 in the data (hunger and tiredness 2, healing 3); bw1-decomp calls it
    `DesireDecay`; his code uses it as the desire's cap;
  - +0x50 / +0x54 (our `field0x40` / `field0x44`): hold 0.999 / 1 in nearly every row (0.9 / 1 in two); bw1-decomp
    calls them `InitialValueMin` / `InitialValueMax`; one number is drawn from this range per desire (see
    [Random streams](#random-streams)), and his code uses it as the desire's per-turn decay;
  - +0x78 (our `field0x68`): the weight (bw1-decomp: `DesireGrowthRate`);
  - +0x80 / +0xC0 / +0x100 / +0x140 / +0x180: the name and the desire, short, trying and doing texts.

  bw1-decomp marks the names of +0x4C..+0x54 as fabricated, and they are wrong. **His reading (cap = +0x4C, decay
  drawn in [+0x50, +0x54], value 0) is the original's, read in W120 2026-10-09** (it was only inferred before):
  - `CreatureDesires::Initialise` 0x4DC100, for each desire i in order: activated (+0x8) from the game flag 0x2000,
    the suppression counter (+0xA8) and the value (+0x148) to 0; then `fld [+0x54]; fsub [+0x50]; fstp` (the range,
    rounded to a float), `GameFloatRand(range)` (call 0x4DC14E), `fadd [+0x50]` and `fstp [+0x288 + 4i]` (0x4DC163):
    the drawn number is stored as a float, no `__ftol` (bw1-decomp types the array `uint32_t`). Then the increase time
    (+0x1E8) from the per-species table, `RandomiseIncreaseTime` 0x4DC310 (which draws only with the cheat flag
    [0xD00DEC] set), `+0x468 = [+0x4C]` (0x4DC188) and `+0x508 = [+0x78]` (0x4DC191).
  - `CreatureDesires::UpdateDesires` 0x4DC430 shows what they are. For an activated desire, the suppression counter
    goes down by 1 if above 0; then with an increment from the sources above 0 and the counter at 0, `value +=
    increment` (0x4DC495); otherwise `value *= [+0x288]` (0x4DC499): **+0x288 is the per-turn decay**. Then `value <
    [creature info +0x274]` gives that floor (0x4DC4A8), else `value > [+0x468]` gives that cap (0x4DC4C1): **+0x468,
    from +0x4C, is the cap**. The floor is the `GCreatureInfo` field openblack calls `desireFloor` (file offset 0x264):
    0.0 in all 17 species rows of `Scripts\info.dat` and in all 19 of `CreatureIsle\Scripts\info.dat`.
  - `FindWeakestDesire` 0x4DC7B0 compares +0x148 (the value), not +0xA8 as an earlier note said.
  - **Feedback** moves the decay: `ModifyDesireAfterFeedback` 0x4DCA60 does `+0x288 += feedback × 0.001` (0x4DCC41),
    then clamps it to [+0x50, +0x54] of the row (0x4DCC55, 0x4DCC73), the low bound tested first.
  - **A mind file does not carry the decay.** The reader fn_004E84A0 (from `LoadFullyQualified` 0x4E8040 and
    `LoadMindIntoPreexistingCreature` 0x4E8440, both after `Creature::Create`, whose constructor has already run
    `Initialise`) reads per desire: activated (+0x8, 0x4E861D), value (+0x148, 0x4E8625), cap (+0x468, 0x4E8633),
    increase time (+0x1E8, 0x4E8641), the dropped float of versions below 7 and the one of versions 6 to 9, and from
    version 9 the sources (rebuilt, 0x4E870D) and, below 15, a dropped int. It writes nothing to +0x288 or +0x508: the
    decay drawn when the creature was made, and the weight, stay.

### The body

- **Meshes** (`src/3D/CreatureBody`): a creature mesh's file name gives the species, the appearance (base, evil, good,
  thin, fat, weak, strong) and the Ogre variants. The mesh folder `Data/CreatureMesh` also holds files whose last name
  part is not an appearance (`A_Tiger2_Base - Realistic Face.l3d`, `Eyeball.l3d`, `Eyelid.l3d`); those are now
  skipped, while an unknown species still names a mesh and logs an error. **This is the one change to running code:**
  before, `Eyeball` or `Eyelid` (whichever loaded first) took the id `creature/0/0`, the Unknown species' base mesh,
  so a creature of type 0 (`CREATURE_TYPE` 0, only the Playgrounds use it) drew an eyeball and now has no mesh. Nothing
  looks these meshes up by name and no creature of type 0 is made on Land 1 or Land 2, so no verification check
  changed. Our
  rule is a port decision, not a reading (see [Pending](#pending)).
- **Morph** (`CreatureMorph`): the body is the base mesh pulled towards one mesh on each of three axes (evil-good,
  thin-fat, weak-strong); threshold and vertex blend as the hand's (0.03; base + |a|(m − base),
  [hand-and-interface.md](hand-and-interface.md)).
- **Seams (vertex blends)** (`src/3D/VertexBlend`, `L3DSubMesh::GetBlendSource`, `vs_object` with `USE_MORPH`): each
  primitive of a mesh lists blends of 8 bytes (the vertex that moves and the vertex it moves towards, both counted in
  the primitive, then a float weight). The original applies them in its boned primitive draws, after every vertex of
  the primitive has been placed by its own bone into camera space and lit, and before the clip codes and the
  projection: in list order, in place, `v = v + (p − v)·w` for x, y and the depth, the weight as the file has it, not
  clamped. Only the position moves. A global switch, on at start, gates them; one object draw turns it off while it
  draws with a non-zero first argument (see [Pending](#pending)). openblack blends in world
  space in the vertex shader, which is the same affine image; the partner is morphed like the body and placed by its
  own bone. **Which meshes carry blends** (counted with the l3d component over the game's data): every mesh in
  `Data/CreatureMesh` (all species and their six variants, 144 to 418 blends each, weights 1e-14 to 0.5, 33 090 in
  all) and three spell meshes in `Data/Spells/Anims` (`PUPIL.l3d` and `TEACHER.l3d`, 224 each, and
  `Tree_Goddess_Boned.L3D`, 1); nothing else: no mesh of `AllMeshes.g3d` (626, 167 of them boned: villagers,
  animals), not the hand (`Hand_Boned_*2.l3d`, `hand_intro.l3d`), not the eyes. In the data no vertex moves twice,
  none moves towards a vertex that moves, none names a vertex outside its primitive or itself, so one partner per
  vertex, read from the vertices as placed, gives the original's result. openblack blends only the creature's body
  (see [Pending](#pending) for the spell meshes).
- **Layers, face, eyes, look** (`CreatureLayers`, `CreatureFace`, `CreatureEyes`, `CreatureLook`): one body animation
  at a time (stand, a one-shot action, or start / loop / end), with the head turn, the face and a gesture on top; the
  faces a feeling pulls and how long they hold; the eyes (eyeball and eyelid set at points on the body's triangles,
  blinking, the lids following the pupils); what an idle creature looks at.
- **Skeletal animation** (`src/3D/SkeletalAnimation`, `CreatureAnimation`): a species' variant animations (evil, good,
  thin, fat) are blended into the base's by the body's axes, element by element on the bone matrices. The rotation is
  our `affine::RotationYXZ` read by rows (the same nine cells as his formula; ours models the original's FPU
  rounding), and the rows are normalised with `std::hypot` as `HandAnimator` does. Angles are read back with his
  `asin` / `atan2` decomposition, not `affine::DecomposeYXZ`: the blended matrices are not unit, and at the gimbal
  (`m6 = m8 = 0`) `DecomposeYXZ` gives NaN as the original does, which would poison the keyframes. Two known
  differences from the hand's animator: in the non-looping edge frame the hand clamps to the last frame and the
  creature wraps to the first; and a bone the animation does not move takes the stand animation's first frame (the
  hand: the bind pose).
- **Skin** (`CreatureTattoo`, `CreatureMarks`, `CreatureSkin`): up to eight tattoos of sixteen designs
  (`Data/Textures/PlayersSymbols.raw`, colours from `tattoocols.raw`); wounds, burns and blood trails that age into
  scars; each base skin blended towards the evil or good skin, 4 bits a channel, weight trunc(|a|·256) ≤ 255 —
  bit for bit the hand's `MorphTexture` blend (a test pins it against `HandMorph.cpp`'s blend over all 256 weights).
- **The default tattoo symbols** come from `Data\Textures\OriginalChooseSymbol.raw`, 256×256 RGB (196 608 bytes),
  the exe's own file: `runblack.exe` names `PlayersSymbols`, `ChooseSymbol` and `OriginalChooseSymbol` and loads them
  at 0x5DE410 / 0x5DE425 / 0x5DE43D ([rendering.md](rendering.md), the `k_AlphaFlagStems` list), each with its `a.raw`
  alpha. His `Game.cpp:1738` reads the defaults from `I_PLAYER_SYMBOLS_.raw` instead. That file and
  `I_PLAYER_SYMBOLS_A.raw` are a repack's copies that the exe never loads: neither name occurs in `runblack.exe`;
  `I_PLAYER_SYMBOLS_.raw` (65 536 bytes, one grey byte a texel) is byte for byte `ChooseSymbola.raw`, the alpha, and
  `I_PLAYER_SYMBOLS_A.raw` (196 608 bytes, RGB) is byte for byte `OriginalChooseSymbol.raw` in the Complete
  Collection's `Data\Textures`. openblack's skin-art loader (`CreatureSkinArtLoader`, loaded at start-up under
  `creature_skin::k_ArtId`) reads the defaults from `OriginalChooseSymbol.raw` as his code reads its file: RGB, the
  level of a texel is its blue >> 4, 64×64 cells, 4 to a row. The players' symbols (`PlayersSymbols.raw`) are optional;
  a cell left blank there takes the default.
- **Hair and footprints** (`CreatureHair`, `CreatureFootprints`): tufts of strands as spring chains whose look moves
  from neutral towards evil or good; a print under the lower foot at each footstep, fading over about five seconds.

### Drawing in the frame

Read from `runblack.exe` (W120), 2026-10-08:

- **Where.** A creature is drawn in the models stage ([original-frame.md](original-frame.md#2-draw-stages-in-order),
  4m): `Creature::Draw` 0x517910 (vt+0x610) points the game object's 3D object (+0x40) at the body
  ([+0x160]+0x58 → +0x482C, 0x517AD1..0x517AE5), then calls `LH3DCreature::AddForDrawing` 0x48E1C0 (vt+0x1C,
  0x517AED). That one lights the body (`fn_00801C90`, haze `fn_007FEB30`), runs `CheckRegionOnScreen` and calls
  `DrawNow` 0x48E260 at once. Other callers of `DrawNow`: `FallingSpell::Draw` 0x526A42, `fn_00640054` 0x640386 (the
  creature snapshot) and `CreatureRoom::DrawAdditional` 0x7888B2 (the Cave).
- **The body** is an animated 3D object: `Morphable::MorphInit` 0x61731A makes it with `LH3DObject::Create(3)`, which
  is vtable 0x9A3068 (0x80B5DE; the hand's body is made the same way). `DrawNow` calls its `AddDrawing` (vt+0x100 =
  `fn_00813340`, 0x48E2A6): off screen or with +0xB8 set nothing; queued whole (`NewZObject` 0x813413, callback
  0x7FA980, key (dx² + dy²) + dz² from +0x38..+0x40) if its Z-sort bit (vt+0x44, from the mesh's 0x200) **or** its
  global-alpha bit (vt+0x4C, +4 & 0x80) is set; otherwise its Draw at once (vt+0x108 = `fn_00813BA0`, 0x8133C4). No
  creature base mesh in `Data/CreatureMesh` has the 0x200 flag (only the Lion's six variants and the three hand meshes
  have it; the body gets the base), and no write of the global-alpha bit to the body was found, so a creature is drawn
  at once, in the draw list's order, never through the Z-sorter **(inferred: the global-alpha part)**.
- **Inside the body's Draw** (`fn_00813BA0`): the mesh, then the shadows cast on objects (the list [0xFAA7E0],
  `fn_0080B050`, 0x813F8E..0x813FE0; a shadow is not drawn on its own caster, +0x464), then the **hair**
  (`LH3DObjectHair` at +0x88, `fn_00848390` 0x814029) only while +0x94 and +0xA0 (the body's cross-fade state) are
  both below 0.2. The hair strands go through `Draw3DWorldTriangle` 0x81C090 with the hair group's material (+0x78), or
  `fn_0081BC10` untextured (`fn_00847060` 0x847696 / 0x8476BC): drawn at once, no Z object.
- **The eyes** come after the body's `AddDrawing` returns: the shared eyeball and eyelid objects ([0xC64224] /
  [0xC64228], `LH3DObject::Create(0)`, `Data\eyeball.l3d` / `Data\eyelid.l3d`, 0x48E2DA..0x48E364), two eyes each,
  drawn at once with vt+0x108 (`fn_0080DB30`), or the cross-fade draws vt+0x15C / vt+0x154 when the body's +0x94 /
  +0xA0 is set (0x48E9A2..0x48E9DD, 0x48EFD3..0x48F00E). So the order is body, shadows over it, hair, eyes; a queued
  body (not seen) would have its eyes drawn before it.
- **No shadow falls on the body.** The receiver flag (+4 & 0x40) is set only through vt+0x78 with 1; no creature code
  calls it (none of the 33 vt+0x78 call sites is on the body), and a bare 3D object starts with +4 = 0x10009
  (0x816537). The game object's own 3D object (`Object::Create3DObject` 0x6365F0) is the one that asks, and it is
  replaced by the body before the draw. openblack: `ReceivesDynamicShadow` leaves out the creatures.
- **The creature's own shadow** is a projected shadow (`CreateDynamicShadow`, [rendering.md](rendering.md#projected-shadows-shadowinfo)),
  drawn with the land blocks (4i) and over the objects that receive. openblack: a complex caster in `graphics::shadow_list`
  in half rows, as the hand (`CreatureShadow`).
- **The hair's material** (read 2026-10-08): one material for every creature, made the first time a creature loads
  (`fn_006186B0`, called from `LH3DCreature::LoadBase` 0x4EB11C and `Morphable::ReadBinary` 0x617DA4):
  `data\c_ape_hair.raw` (format 0x41), `CreateMaterial(9, texture)` 0x618701 into [0xD3F008], then +4 (ALPHAREF) = 0x32
  (0x618713); +5 stays 0 (`CreateMaterial` 0x82FD56), so it is one-sided (CCW) and clamped. `PrepareForDrawing` passes
  it to every hair group (0x4ED539 / 0x4ED5AC → `fn_00848350` → `fn_00847C90`, group +0x78). A group without its
  texture (+0x60 ≠ 1) goes through `fn_0081BC10` with no material, so the triangle draw's default one
  ([0xEA9EC4], `CreateMaterial(0, 0)` at 0x819011): mode 0, opaque. Both write depth, so the hair is never covered by
  what is drawn after it and behind it, whatever the draw-list order.
- openblack (`Renderer::DrawPass`, `Renderer::DrawCreatureParts`): the body in the models' loop at once (the mesh's
  0x200 decides, as for any model), then the shadows over it, the hair and the eyes. The hair in
  `render_modes::materials::k_CreatureHair` (mode 9, ALPHAREF 50; the world quads' shader drops the texels under
  ALPHAREF) and `k_CreatureHairPlain` (mode 0). **(approximate)** both two-sided: the ribbons are built facing the
  camera in openblack's own winding. **(approximate)** the models are drawn per mesh, not in the original's draw-list
  order (near blocks first); with the hair writing depth this only changes which background its blended edges show.
  Before 2026-10-08 the hair was in mode 6 (no depth write), so a model drawn after it and behind it (the temple)
  covered it.

### Drawing between turns

A creature's Transform moves once a turn (`creature_pose::CommitTurnPose`); every frame the locomotion writes
`components::CreatureDrawPose`, the position lerped from the turn's start to its end by the turn fraction and the
heading lerped the short way round (`CreatureLocomotionSystem::Update`). It is the villagers' "one turn behind" scheme
([animation.md](animation.md#drawing-between-turns)) without their slope shear or their yaw rate.

- **(inferred)** the original does the same: its creature is a Living on the same list, so the start-of-turn copy
  (+0x2C) and the per-frame lerp apply. `Creature::Draw` 0x517910 was read only for its stage and order, not for its
  matrix source; bw1-decomp has no body for it.
- openblack (2026-10-08): `creature_pose::BetweenTurns` gives the pose while it belongs to the turn that put the
  Transform where it is (the locomotion started, `toPosition` equal to the Transform's position, no hand or physics
  pose); otherwise the Transform. `ecs::DrawnModel` takes it, decided per row, after the hand's and the physics' poses
  and before `DrawPosition`, with the scale it already had (the pen's drawn scale, else the Transform's): one matrix
  for the whole drawn creature. A standing creature (pose equal to its Transform) keeps the Transform's matrix bit for
  bit. Every reader takes that matrix (`ecs::DrawnBodyModel`): the body's row (`RenderingSystem::WriteEntityRow`),
  hence its shadow and the hair's light; the eyes (`CreatureAnimationSystem::PlaceEyes`), the hair roots, what it
  holds (`UpdateHeldDraw`, its `PhysicsDrawPose` only), the footprints, the hand's touch and pick
  (`CreatureHandSystem`: `CreatureAlong` and the strokes and slaps, the original's hand colliding the drawn, posed
  body), the leash's collar end ([The leash's collar end](#the-leashs-collar-end)), and fire, spell seeds and tree
  bending (`DrawnModel` / `DrawnPosition`). `creature_pose::DrawnPlacementOf`
  gives the same sources' rotation alone, for where the eyes look ahead and the turn of what it holds. On the
  Transform, because the turn or the hash reads them: the head's look in `PoseBody` (its bones place the held object's
  Transform in the turn), the held object's Transform, the audio positions and the fight's hits.
- **(approximate)** the hand's touch on a creature changes the game (which creature is picked, strokes and slaps that
  reach its mind): it now reads the drawn body, so it can differ from before by up to one turn's step while the
  creature walks, and in its pen it finds the smaller body that is drawn there.
- **The pen's drawn size reaches every part.** The eyes' size and how deep they sit shrink by
  `creature_pose::DrawnSizeShare` (the drawn scale over the Transform's, 1 outside a pen); the reflection's bounding
  radius takes `creature_pose::DrawnScale`. The hair and the footprints take the size it is drawn at
  (`creature_pose::DrawnSize`), as the original: the hair's scale is `GetExtraHeadScale` × +0x90 × 15
  (`PrepareForDrawing` 0x4ED548..0x4ED55E; `GetExtraHeadScale` 0x4EC450 = 1.8 − 0.5 × clamp(+0x90, 0, 2)), so in the
  pen its strands' length, width and root depth are `HairScale(pen size)`, larger than the body's share of its own
  size's (at size 2 in the pen 5.58 against the share's 2.64); the footprint's side is 4 × +0x90 (`fn_00483290`
  0x483582), `creature_footprints::Side(pen size)`. The hair's stiffness follows its scale, as for a smaller creature.
  **(approximate)** the original's eyes under a drawn size are not read: they are scaled as one with the body here.
- **The eyes are drawn where they are placed** (fixed). `PlaceEyes` gives world matrices, and the eyes are drawn with
  the body's instance row (for its land light, fade and the lid's tint), which the instanced shader applies after
  `u_model`; so `creature_draw::InBodySpace` takes each eye into the body's space first (the row's inverse, its w
  components dropped as the shader drops them). Before, the body was applied twice: the eyes were drawn about 700 m
  off and at a sixth of their size. `Eyeball.l3d` and `Eyelid.l3d` have no bones, so the one `u_model` is enough.

### The mind and its file

- **The mind file** (`components/creaturemind`, `MindFile`): the `.erc` / mind-file format, read and written from
  bytes. The names are enciphered with MSVC `rand`'s LCG (214013 / 2531011, `>> 16 & 0x7FFF`) seeded with 0x913 for
  each name. The two `.erc` files in `Scripts\CreatureMind`, `C4ba71b36.erc` (a Mandrill, row 14) and `Cacab45d4.erc`
  (a Tiger, row 2), are version 33 and read and write back byte for byte. They are not templates: they are two
  profiles' creatures, the files `LOAD_MY_CREATURE` reads ([The player's creature](#the-players-creature)).
  `Profiles\<name>\creature.lhp` is not a mind file (both profiles' files are refused as an unsupported version): it is
  an export the game writes and never reads back.
- **The body's fields**, checked in the game's writer 0x4E8E70 and reader 0x4E8FF0:
  - physical +0x70 is unnamed (it reads like a turn number);
  - then +0x08 the age, +0x0C the strength, +0x14 the fatness, +0x18 the previous fatness, +0x1C the energy, +0x20
    (from version 6), the u16 flags, and six needs from +0x2C (the amount of poo, the exhaustion, then four more);
  - from version 22, an int from +0x50 and the size.
  - The names Age, Strength, Fatness, PreviousFatness, Energy, AmountOfPoo and Exhaustion are the format strings of
    `Creature::OutputCreatureInformation` (0x4785A0 is inside it).
  - The fatness and previous fatness were first named by bw_creature_editor.py, a creature-editor tool shared on the
    openblack Discord (credited in `MindFile.h`), which walks the same sections as openblack's reader up to the body.
    The land change's save (`creature_mind_body::ToMindFile`) writes the fatness at that field on its strength.
- **Where that tool differs from the original, which openblack follows:**
  - a belief's two numbers are the object's x/z position (`UpdateAttributes` 0x4D7E80), not object ids;
  - the 42-entry table holds (count, last turn) per spell (`ConsiderLearningAction` 0x4E2443, 0x4E2499);
  - the physique file's first number is the species row, not a version (it equals the mind file's species in every
    copy checked).
- **A mind is saved as the creature itself**, not as the miracles left it (raffclar's rule, not yet checked against
  the original's code). Saving takes the size, the strength and the alignment the creature had before any spell of that
  kind began (before big, else before small; before strong, else before weak; before nice, else before nasty). While the
  land's `CreatureCurse` script runs, his rule takes instead the script's globals `OriginalSizeOfMyCreature` (a height,
  so a size of height / 15), `OriginalStrengthOfMyCreature` and `OriginalAlignmentOfMyCreature`. **Unverified:** the
  four names do sit in the exe near the "Save Mind" and `CreatureMentalSaveAndLoad.cpp` strings (0x7DFF00..0x7E0010),
  and `Scripts\Quests\challenge.chl` holds them as script names, but nearness of strings shows no reader. bw1-decomp
  points elsewhere: the game keeps the cursed creature's values natively, as `ScriptCreatureCurse {height, strength,
  alignment}` in `GGame::script_creature_curse` (`Game.h:224`). `GGame::Save` calls `ScriptCreatureCurse::Init` on the
  local player's creature (0x6F6190) and writes the record last (`GameOSFile::WriteIt`, 0x556F80); `GGame::Load` reads
  it back (`ReadIt`, 0x557070) and the load's resolve hands it to `ScriptCreatureCurse::ResolveLoad` with that creature
  (0x6F61E0). Init's and ResolveLoad's bodies are not decompiled yet, and nothing shown so far ties the mind file's
  save to the record or to the script's globals. The exe has no `OriginalFatnessOfMyCreature`, though the script keeps
  one. openblack: `creature_spells::ValuesToSave`, `mind_detail::CurseValuesToSave` and `CreatureMindSystem::SaveMind`
  (his rule, dormant: nothing calls `SaveMind`).
- **Mind** (`CreatureDesires`, `CreatureDecisionTree`, `CreatureLearning`, `CreatureWatching`, `CreaturePlanner`,
  `CreatureMindModel`, `CreatureMindTables`, `CreatureMindFileBody`): 40 desires that grow from their sources past
  their thresholds and otherwise fade; two decision trees per desire (what to act on, what to use); learning from
  strokes and slaps credited to recent actions; learning skills and miracles by watching; the planner that picks a goal
  and an action per desire. The sigmoids are ours: `Sigmoid` is `gutils::CreatureSigmoidThreshold`, `SigmoidStep` is
  `gutils::SigmoidThreshold` (the game's 41 exact floats, [engine-math.md](engine-math.md)); his decimal table differs
  in 14 steps (16, 18, 19, 23, 24, 26, 27, 29-33, 35, 36) by 1-2 ULP and gave 1 instead of 0 at a threshold of exactly 1.
- **Idle mind and actions** (`CreatureIdleMind`, `CreaturePlanActions`, `CreatureFeedback`, `CreatureReach`,
  `CreatureObjectActions`): the agenda an idle creature works through once a turn; which of the table's actions it can
  carry out; strokes and slaps; reaching by blending four animations; picking up, eating, tossing and throwing things.

### Learning by watching

Read in full from the W120 code, 2026-10-09 (`ConsiderLearningAction`, the magic scroll; disassembly, no bodies in
bw1-decomp).

- **`CreatureMental::ConsiderLearningAction(type, index)`** 0x4E2380, type 0 a skill, 1 a miracle. Callers (not read
  further): `CreatureBeliefAboutVillager::UpdateAttributes` 0x4D8158, `DecideOnNewPlan` 0x4EA6ED, 0x4F2945, 0x4F2963,
  0x50613D, 0x50A4EC. The mind's fields: +0x20CE4 mind active, +0x1A9FC `CreatureActionsKnownAbout` (two lists, skills
  and miracles), +0x17D38 + 4·i a miracle's sightings, +0x17DE0 + 4·i its last turn, +0x20D18 the creature. The turn is
  the game's (`g_game`+0x205A40).
  1. With the mind not active it returns 0 at once (0x4E2388), for both types. The flag is cleared when the freeze
     spell starts (0x4F5449, after `FinishActionUnsuccessfully("frozen by spell")`) and set again at the thaw
     (0x4F54F6).
  2. **The prerequisite**: a static table at 0xBDFAE0 (initialised data, nothing writes it), 8-byte entries
     `(type, index)` at `[type·42 + index]`, type 2 for none. When there is one and `KnowsAction(type, index)`
     0x4E2890 is false it returns 0: no message, nothing written. The miracle rows: 2 and 3 need miracle 1; 5 and 6
     need 4; 8 needs 7; 9 needs 8; 11 needs 10; 17 needs 16; 18 needs 17; 40 and 41 need skill 0 (building). Every other
     row is none. (The skill rows 0..17 are none; 18..41 read (0, 0), but there are only six skills.)
  3. **A miracle**:
     - with the creature's phase (+0x1268) below the row's (0xCA9CD0 + 0x70·i): message 0xB, return 0, nothing counted;
     - when `now − last > 50` (unsigned, `ja`) or `last == 0`, the count goes up by 1, and by 2 more with the leash on
       and of type 2 (the rope; leash +0x12C → +0x1C): 1 or 3. A last turn of 0 is the "never seen" mark;
     - `last = now` always, also when the sighting did not count;
     - `AddActionLearnt(creature, 1, i, notify 0)` 0x4E28C0: the miracle is known about from this sighting on;
     - `needed = NumTimesMagicActionMustBeSeenBeforeItIsLearnt(i)` 0x4F8CA0, `fild(timesToSee)` times
       `fmul dword[CreatureInfo + 0x37C]`, 0 for i ≥ 42, not rounded. The game thread runs the x87 at 24 bits
       ([audio.md](audio.md#the-games-fpu-runs-at-24-bits)), so it is the float product: 15 × 1.2 is 18 exactly;
     - `needed <= count`: message 7 and result 1, and the meter (`fn_0047CDA0(1.0)`) while `count − needed < 3`
       (the 3.0 at 0x8C2C50); return 1;
     - otherwise `share = count / needed` (float); `share >= 0.75` (0x8AB274) gives message 9; the meter
       `fn_0047CDA0(share)`; return 0.
  4. **A skill** (for reference): phase against 0xCAAF40; the first-seen turn (0 again the mark) at +0x17D20, the count
     at +0x17D08; learnt once the seconds since the first sighting reach the row's watching time (0xCAAF38): the meter at
     1.0 if not yet known, then `AddActionLearnt(…, notify 1)`; otherwise message 8 and the meter at 0.5 when not known;
     too young, message 0xA.
  - `fn_004C9FE0(code, index, 0, 0, 0)` shows a learning message, only for the local player's creature (INFERRED from
    its player check at 0x4CA036; codes 6 and 7 learnt, 8 and 9 nearly, 0xA and 0xB too young). `fn_0047CDA0(v)` draws
    a meter over the creature, at most once in 100 turns (0x47CDBA) (INFERRED from the code; the drawing is not read).
- **No miracle is known at the start.** The `CreatureMental` constructor 0x4D23D6 builds `CreatureActionsKnownAbout`
  through fn_004E21F0 with both lists empty. Field +0x54 of `CreatureMagicActionKnownAboutEntry` (1 in 24 rows of
  `Scripts/info.dat`: 0, 1, 4, 10, 13..30, 34, 35) is read only by the computer players' creature code: every
  reference to 0xCA9CD4 in the code is in fn 0x4E1F70 (counts the flagged miracles not known about), called only from
  fn_0065BCB0 and from the thunks 0x47C940 / 0x47C950 (no caller, no pointer), and in the helpers 0x4E29E0..0x4E2C80
  ("the first flagged miracle not known about"), called only from 0x659192..0x65A994 in the `PlayerComputerCreature`
  area. raffclar's comment on the field says the same.
- **The magic scroll** `CreatureRoom::MakeMagicScrollText` 0x789070: for i = 1..41 (never 0), only when
  `KnowsAction(1, i)` and the row's text id (+0 of the 0x11C magic row) is not 0, the percent is
  `__ftol(min(1, count / needed) · 100)`, in float steps, cut short. A miracle known about at 0 % is listed.
- **openblack** (`src/Creature/CreatureWatching`, `CreatureMindSystem::SeeSkill` / `SeeMiracle`, the cave's snapshot
  and scroll facts): as above, the miracle known about is `Knowledge::miraclesKnown`; the prerequisite table is
  `creature_watching::MiraclePrerequisite`; `TimesNeeded` is the float product, rounded once (exact for any count below
  2^29); the messages and the meter are returned as data (`Progress::event`, `Progress::meter`) that nothing shows yet;
  a paused mind (the freeze spell sets it) skips both kinds of sighting; the scroll lists every miracle from 1 known
  about, with the percent cut short. The "I've learnt" thought of the mind's log is openblack's: it comes once, at the
  sighting that reaches the times needed. Nothing in the game reports a sighting yet: only the debug spawner does, so
  the game's runs are unchanged. The text-id test of the scroll is not ported: our scroll names each miracle by its
  place.
### What a creature looks at

- **The scan** (read in W120): `Creature::Look` 0x4D0700 → fn_004D07A0 scans up to 9 map cells a turn in a spiral
  (`CanSeePos` 0x477440), and fn_004D0A20 hands every object in each cell (it skips those with flag `+0xA & 1`) to
  `AddBeliefAboutObject` 0x4D7BD0 and to fn_004D1D70 (`CreatureInterestingThingToLookAt::ConsiderNewObjectToLookAt`).
  That skips the creature itself, tests `CanSeePos` and fn_004774F0(pos, 6), and weighs the object by the virtual
  `GetHowMuchCreatureWantsToLookAtMe` (vt+0x3E8) × the distance factor (1 within half the range, else 1 − d/range,
  range = √(fn_004EFC70) × 10) × the boredom term (1 − min(watched, 20)/20) × the per-kind weight at mind+0x1AA2C.
  openblack: `creature_look` (`LookRange`, `DistanceFactor`, `Priority`, `LookAbout`).
- **vt+0x3E8 by class** (bodies 0x4D1AF0..0x4D1B80, read through every vtable): the Dove class 0.9 (the vtables of
  Dove, Crow, Swallow, Pigeon, Seagull, Bat, SpellDove, SpellBat and Vulture), Citadel 0.9, Creature 0.85, Animal 0.7
  (the vtables of Lion, Tiger, Leopard, Wolf, SpellWolf, Cow, Sheep, Horse, Pig, Tortoise, Goat, Zebra and the
  Piece/Puzzle ones), Villager 0.5, Abode 0.4, Tree 0.3, Fixed 0.25, MobileStatic 0.9 for a toy, else 0.4
  (`creature_look::InterestOf`). These are classes, read by their vtables; which `AnimalInfo` value makes which class
  is the next point.
- **Which animal is which class**: the animal factory fn_00419E00 jumps on info+0x1F4 (27 cases, table 0x419FD0, and
  above 26 it returns no object, 0x419E0C); each case's create function sets its class's vtable, and the cases line up
  with `AnimalInfo` 0..26. Traced through the factory: 0-4, 6 and 8-10 the Animal class, the flying ones (11-16, 20,
  21) the Dove class, 22-26 (SpellWolf and the Piece ones) Animal. Cases 5 (Goat), 7 (Zebra) and 17-19 (Vulture,
  CitadelDove, CitadelBat) create nothing. So every bird the factory makes is looked at as much as a dove, not only
  the miracle's doves (raffclar's tree: SpellDove only).
  - Not traced through the factory, and so **assumed by name**: the Goat, Zebra and Vulture classes exist (vtables
    0x8AF4E0, 0x8B1964, 0x8BB7F8; Animal, Animal and Dove at vt+0x3E8), but their only constructor call sites are in
    the save loader `GameOSFile::LoadInstance` (0x55ACAA, 0x55AD10, 0x55B05D), which picks the class by the saved
    type; the `AnimalInfo` such an object carries was not read. openblack gives `AnimalInfo` 5, 7 and 17 the class of
    the same name.
  - The Puzzle classes (PuzzleHorse, PuzzleCow, PuzzleTortoise, PuzzlePig; Animal at vt+0x3E8) are made only by the
    chess puzzle (`PuzzlePig::AddToBoxPositionForChessGame`, 0x6DD1BB..0x6DD384) with the Horse, Cow, Tortoise and Pig
    entries of the animal info table (0xC4E95C, 0xC4E690, 0xC4E0F8, 0xC4EC28: entries 9, 8, 6, 10 of the 0x2CC-byte
    table at 0xC4D030). So no object of the original is known to carry 27-30; whatever it carries, a Puzzle animal is
    of the Animal class (0.7), the value openblack gives 27-30.
  - CitadelDove (18) and CitadelBat (19) have no create function and no vtable named for them, so their class is not
    known; openblack's Animal (0.7) is a placeholder.
  - openblack: `mind_detail::LookInterestOf`, Dove for `animal_ai::IsDoveClass` (`IsFlyingSpecies` and the Vulture,
    the same test the script types and the heal rule use), Animal for the rest. The Vulture, Goat, Zebra and citadel
    cases are in [Pending](#pending).
- **The candidates** (`mind_detail::GatherCandidates`, through the const registry): creatures, villagers, abodes,
  trees, temples and, from 2026-10-09, every animal. An animal is looked at at its `Transform` position, as in
  raffclar's tree; the point the original turns the head to is not known (see [Pending](#pending)). Every entity of
  these kinds is offered, with no test of where it is: one in the god's hand or the creature's, or one dying or dead,
  included. The original offers only what is linked into the scanned cells and skips the UNAVAILABLE ones (+0xA & 1);
  the difference is in [Pending](#pending).

### What frightens a creature, and the fire it believes

- **`CanBeFrighteningToCreature`** is vt+0x238 (read in W120 through every class vtable). It answers 1 for `Creature`
  0x4740B0, `Spell` 0x55CED0 (and every Spell subclass), `Bat` 0x41EF20, `SpellBat` 0x41F000, `Vulture` 0x41F0D0, and
  `Lion` 0x41FC70, which Tiger, Leopard, Wolf and SpellWolf share (the same vtable entry). Every other class answers 0,
  PieceLion/PuzzleLion and PieceWolf/PuzzleWolf (0x4200E0, 0x421D90), SpellIcon and Fixed included. The animals' part
  is `AnimalSystem::IsFrighteningToCreature` ([animals.md](animals.md#api-for-other-systems-ecsanimalaih)).
- **It is the Fear desire's object test.** `g_CreatureDesireDependency` has one 0x28-byte entry per desire, filled at
  0x4DB5F0 from 0xC87850. Entry 5 (`CREATURE_DESIRE_FEAR`) +0x14 is 0x4DD0E0, a `jmp [eax+0x238]` thunk.
  `CreatureMental::IsObjectSuitableForDesire` 0x4E3730 calls entry[d]+0x14 on the object (0x4E37C5), after
  `IsSuitableForCreatureAction` vt+0x21C. The other entries, read but not ported: Impress → vt+0x244, Compassion →
  0x4C33D0 (vt+0x23C `CanBeHelpedByCreature`; the Villager override is vt+0x628), Anger → vt+0x234
  `CanBeAttackedByCreature`, Play → vt+0x240.
- **In the data** (`Scripts\info.dat`, the desire → action rows after the 328 actions): Fear (5) lists
  `RunAwayFromObject` (16), `BeFrightenedOnTheSpot` (174) and `RunHome` (170), and no other desire lists 16. So, for
  this data, "the object of RunAwayFromObject must frighten" is the original's per-desire rule exactly. openblack:
  `Target::Frightening` (`creature_plan_actions`), RunAwayFromObject's target; `mind_detail::Accepts` takes another
  creature, a `Spell` or an animal `IsFrighteningToCreature` says yes to. `mind_detail::Gather` offers it no spell: how
  a spell reaches the Fear test in the original is not known (see [Pending](#pending)), so only creatures and animals
  are its candidates. The test
  `CreatureFrightening.OnlyFearListsRunningAwayFromAnObject` pins the data side; it skips without the game's data.
  Before 2026-10-09 openblack ran from any villager, animal or creature (raffclar's tree: creatures, spells and his
  four frightening animals).
- **The Fear desire's numbers** (row 5 of `CreatureInitialDesireInfo`, `CREATURE_DESIRE_FEAR`, `Desire::Fear`; the
  name in the row is "Fear"). The same in `Scripts\info.dat` (row at file offset 0x65414) and in
  `CreatureIsle\Scripts\info.dat` (0x6CFE0):
  - sources 17, 18, 19 (`FEAR_FROM_DARKNESS`, `FEAR_FROM_BEING_DAMAGED`, `FEAR_FROM_SEEING_SCAREY_MAGIC`), then 61
    (none) five times; learnable 1;
  - cap +0x4C (our `field0x3c`) = 0x3F6B851F = 0.92;
  - decay range +0x50 / +0x54 (our `field0x40` / `field0x44`) = 0x3F7FBE77 = 0.999 / 0x3F800000 = 1.0; the range is
    1.0 − 0.999 = 0x3A831200 (0.00099998713);
  - weight +0x78 (our `field0x68`) = 1.0.

  So a creature's Fear decay is drawn once, when the creature is made, as `0.999 + GameFloatRand(0.00099998713)` on
  the synchronised stream (the 6th of the 40 draws of `Initialise`): from 0.999 up to 1.0, the largest draws rounding
  to 1.0 exactly (no fading at all). A mind file keeps it; feedback moves it in steps of 0.001 × feedback within
  [0.999, 1.0]. Each turn without a positive increment, Fear is multiplied by it and held in [0, 0.92].
- **openblack's Fear decay** follows the same rule for a fresh mind: `SetupFor` reads the cap and the range
  (`CreatureMindSystem.cpp:160-162`), `ProcessTurn` makes the desires on the creature's first mind turn with
  `game_random::GameFloatRange` (`:747`, `:763-766`; `GameFloatRange` is `GameFloatRand(b − a) + a`,
  `GameRandom.cpp:115-120`), stored by `creature_desires::Create` (`CreatureDesires.cpp:123`), applied and clamped to
  [0, cap] in `UpdateDesires` (`CreatureDesires.cpp:194-195`), moved by feedback as the original
  (`CreatureLearning.cpp:60-61`, `k_DecayStep` 0.001). One difference is left, and one was fixed:
  - **Fixed 2026-10-09: a mind taken up from a file keeps its drawn decays.** `TakeUpFile` builds its stand-in
    desires at the middle of the range and `FromFile` keeps their decays, as the file has none; until this fix the
    40 decays drawn on the first turn were lost, so every desire of a creature taken up from a file faded at its
    range's middle (Fear 0x3F7FDF3C, 0.9995). `TakeUpFile` now puts the drawn decays back, as the original keeps
    them. The draw count was and is 40. The weights come from the table either way.
  - The floor is our constant 0 (`CreatureDesires.cpp:195`), not the species' `desireFloor`; every row of both
    info.dat files holds 0, so the result is the same with the game's data.
  - When the 40 draws are made is not compared: the original makes them inside `Creature::Create`, ours on the
    creature's first mind turn. See [Pending](#pending).
- **The OnFire belief.** `ATTRIBUTE_TYPE_ABODE_ON_FIRE` = 22. `AttributeOnFire` is built only in
  `CreatureBeliefAboutVillager::CreateAttributes` (0x4D9C3E) and `CreatureBeliefAboutAbode::CreateAttributes`
  (0x4D9D05). Its `UpdateValue` 0x4D4020 asks `IsOnFire` vt+0x298 (Object → 0x637CC0: a FireEffect at +0x44 with
  T ≥ Tc), and **0 is on fire, 1 is not**: its `ValueNames` at 0xBDE9D4 are "IsOnFire", "IsNotOnFire", and its
  `ValueRange` is 2. openblack: `mind_detail::OnFireValue(ecs::fire::IsOnFire(thing))` for villagers and abodes. Before
  2026-10-09 it was always 0, which in the original's values is "on fire" for every villager and abode; raffclar's
  tree has it inverted (1 when on fire).
- A side note: in the decision-tree attribute array (`CreateAttributeArray` 0x4D52B0), slot 22 "OnFire?" is built with
  the `AttributeCreatureDominantDesire` vtable 0x8CFB3C. It looks like the original's own slip, and matters only if
  openblack's tree learning ever builds attributes from that array (see [Pending](#pending)).

### Casting miracles: the rules, not yet called

Pure code only: nothing in the mind reaches it yet, so the creature casts nothing and the agendas it builds never carry
a cast step.

- **`CreatureCastAgenda`**: the order a creature goes about a cast in. One time in five it first shows how it feels
  (angry, kind, playful), then goes near the thing, backs off to a distance, turns to face it and holds still a tenth of
  a second, draws the miracle's gesture if it has one, and last takes the casting pose and casts as the pose's loop
  begins, holding the miracle three seconds. A lightning bolt is cast from 50 away, backing off to ten more than the
  creature's height; a helpful miracle from twice its height; a creature spell from five times its height, backing off
  to twice it.
- **`CreatureCastMoves`**: how it measures those moves. The clearance it keeps walking up to a thing by how tall each
  is, when it has arrived, how far away "away" is and where that point lies, the circles a fixed thing keeps clear, and
  the nearest clear square of cells as wide as the creature is tall.
- **`CreatureSpellCasting`**: what a cast costs its body. Chants to energy is
  `chants / ((size * sizeFactor + strength + 1) * chantsPerEnergy)`, the most chants it has to give is
  `(size + strength + 1) * (energy - floor) * chantsPerEnergy`, paying tires it by at most 0.7 at a time, its energy is
  kept between 0 and its size (at least 1), and it cannot cast what would leave it past 0.85 exhausted. Also the fight's
  stamina cost, the share of sightings at which it tries a miracle and at which a try stops fizzling, and how big it
  casts.
- **`CreatureSpellMind`**: what the mood and need spells do to its desires. The spell's desire is made fully dominant
  and most others are held down for the cheat's time; the body's needs only when all are to be; never its wish to idle
  or play with the player, to restore its health, make friends, show its state, rest, hang around at home or look
  around. As a spell wears off its desire drops below the weakest of the others. Its cheat is the original's timed
  dominant desire ([below](#the-timed-dominant-desire)): his names in front of the rules read from the original.
- **`ecs::components::CreatureCasting`**: the component a creature's casting will live on (the move it is making, the
  turns it has gone on, where it set off for, and a try that fizzled). Nothing creates it yet.

The body's side of `CreatureSpellCasting` is now read in `runblack.exe` W120 (2026-10-09) and matches:
`Creature::ConvertSpellChantsToEnergyDiff` 0x4F8260 = `chants / ((GetUserSize × info +0x390 + physical +0x0C + 1) ×
info +0x294)`; `Creature::GetMaxSpellChantsAvailable` 0x4F82A0 = `(GetUserSize + physical +0x0C + 1) × (physical +0x1C −
info +0x298) × info +0x294`; `Creature::MaintainSpell` 0x4F8350 (vt +0x58): an amount ≤ 0 is returned as it is; the most
chants ≤ 0 gives 0; it pays the amount, or the most when that is not above it; `d` = the energy difference of what it
pays, below 0 taken as 0 and above 0.7 as 0.7; energy (physical +0x1C) −= `d` × the magic type's share; below 0 it is
0, else it is kept to `max(GetUserSize, 1)`; exhaustion (physical +0x30) += `d`; it returns what it paid. The can-cast
test fn_004F82F0 (`HasEnoughEnergyToCastSpell`) is `0.85 − d(costToCreate) > exhaustion`. The info offsets are the
species row's in memory; in the file (and in `GCreatureInfo`) they are 0x10 lower: `spellSizeFactor` 0x380,
`chantsPerEnergy` 0x284, `spellEnergyFloor` 0x288. `GetUserSize` (0x4EF4F0) is the 3D body's size (`LH3DCreature`
+0x90), which `Creature::GetScale` 0x47B190 also returns. The share is `CreatureMagicActionKnownAboutEntry[magic]` +0x6C
in memory (the table at 0xCA9C70, 0x70 bytes each, made by fn_004E2DE0 and filled by `load_variables` from
DETAIL_CREATURE_MAGIC_ACTION_KNOWN_ABOUT), the file's `field0x5c`: the table's data starts 0x10 into each entry, as
`NumTimesMagicActionMustBeSeenBeforeItIsLearnt` 0x4F8CA0 shows (it reads memory +0x54, the file's +0x44, the times to
see it). The magic type is the spell's +0xB4. The 0.85 bar and the 0.7 cap are the executable's constants
(0x8CEFAC, 0x8AB238). The dominance rules and the agenda's numbers are still raffclar's (see [Pending](#pending)), with
the two known issues his step machine is carried with.

### Casting a miracle: the original's steps (W120, read 2026-10-09)

- **The sub-action**: a cast is the creature sub-action "CastSpellAtObject" or "CastSpellAtPos" (its static table is
  built by the `CreatureSubAction` initialiser 0x4F8D70, entries at 0xBE2188 and 0xBE2218, 0x90 bytes each). Initiate
  0x500B70 starts the casting body action (`fn_00484310` with the sub-action's +0x38) and sets the hold counter
  (Creature +0x58) to 0. Perform at an object 0x500BC0 or at a position 0x500CD0 waits while `LH3DCreature` +0x4994 is
  not 3 (the body action's loop has not begun); then, when the magic (+0xFF4) is not NONE, it sets mind +0x21F4 = 1 and
  calls `CastSpellOnObject(magic, object, 10.0, 0)` or `CastSpellAtPos(magic, position, 10.0, 0)`. A failed cast
  returns 1 (the sub-action fails). A cast that worked holds for `ftol(sub-action +0x48 seconds × (1000 / ms per turn))`
  turns. Finish 0x500E10 counts the hold down; at 0 it lets go of the cast (fn_004F81F0), ends the body action
  (fn_004848C0) and is done once `IsPerformingBodyAction` is false. `CastSpellOnObject` is called only from 0x500C63
  and `CastSpellAtPos` only from 0x500D9C.
- **`Creature::CastSpellOnObject`** 0x4F7970: first it lets go of the cast it holds (fn_004F81F0). In a fight
  (`IsFighting` 0x47B1C0) the cost is `costToCreate / 10000` clamped to 0..1; with less stamina (`LH3DCreature`
  +0x4AAC) than that it returns 0 at once (no hunger), else the stamina loses it and is kept to 0..1. The fizzle: `n` =
  `NumTimesMagicActionMustBeSeenBeforeItIsLearnt(magic) − 1`, at least 1; `r` = the sightings (mind +0x17D38 + 4 ×
  magic) / `n`; when `r` is not above 1 and below 0.999 the mind's +0x21D0 becomes `MessUpCastSpellOnObject`
  (0x50A1B0), a belief about the object goes to +0x21E0, +0x21F0 = the magic, fn_004C9FE0(0x27, 1, 0, 0, 0), the
  sightings go up by 1, and it returns 0. Then it measures the object's bounding sphere and an angle
  (`Get3DAngleFromXZ`) whose result it throws away. The magnitude is the object's `Get2DRadius` × 1.1, or for a
  creature (`CastCreature`, vt +0xA4) its 3D body's size × 15 × 1.1; for a magic whose `GMagicInfo` +0x28
  (`spellSeedType`) is 2 (the fire seed) it is the caster's `GetHeight` / 10, at most 1. The cast data is that
  magnitude, the effect's initial chants (+0x74), the creature's timer (+0x6C, fn_005FB7B0) and −1 objects. Then the
  energy test fn_004F82F0 and the class's rule on the object (`GMagicInfo` vt +0x2C, the object and the caster):
  either failing unsuppresses HUNGER (desire 4) and makes it fully dominant (`UnsuppressDesire`,
  `MakeDesireFullyDominant`), and returns 0. Then a process info of zeros (power 1, enabled), `UpdateSpellInfo(NULL,
  info)`, `InitialUpdateSpellInfo(object, the object's MapCoords, magic, info)`, the cast through fn_005FB520 (on the
  object, or at its position when the seed's `castOnObject` is 0, fn_005FB490) into Creature +0x1078, and
  fn_004F81B0, `DoPostSpellCastThings(spell, 0)`: with the int 0 and a spell, `MaintainSpell(spell, costToCreate)`
  (vt +0x58), whose result is dropped. It returns 1.
- **`Creature::CastSpellAtPos`** 0x4F7E20: the same fizzle first (MessUp `MessUpCastSpellAtPoint` 0x50A1D0, the point
  kept at mind +0x21E4), then the release (fn_004F81F0), the same unused angle, magnitude 10 (0x8D161C) or the fire
  seed's rule, the same cast data, the energy test and the class's rule at the position (vt +0x30), the same hunger on
  failure, `UpdateSpellInfo(NULL)`, `InitialUpdateSpellInfo(NULL, position, magic, info)`, the cast at the position
  (fn_005FB490) and `DoPostSpellCastThings`. There is no fight branch.
- **`Creature::InitialUpdateSpellInfo`** 0x4F8590: info +0x00 = the target point (the MapCoords in metres, y = the land
  there + the MapCoords' altitude). When the magic is cast from above (`GMagicInfo` +0x40 == 1, fn_005FB7E0): +0x0C =
  +0x00 raised by 10, or by the target's `GetHeight` × 1.3 when that is not below 10; the two beams (+0x107C, +0x1080)
  are closed down and made again with `GParticleContainer::CreateSpotVisual(point, 0x14 MAGIC_BEAM, size, the
  creature)`, the first at the mirror of the hand bone, the second at the hand bone (`LH3DCreature` +0x51C8, the first
  action point the body's file holds: `LoadBinary` 0x4EB6EB), each at the bone's own translation in the safe buffer
  (`GetBonePos` 0x4813F0: `GetSafeBuffer` + bone × 0x30 + 0x24), size = `0.4 + (1 − 0.4) × GetScale × 0.5` (the pair at
  0x8D1628; with the FPU at 24 bits `1 − 0.4` rounds, a tie, to the float 0.6, so float arithmetic gives the same),
  each aimed at +0x0C (fn_0063E380, the effect's `AddTarget_` of a point, vt +0x110). The mirror bone is
  `GetMirrorBone` 0x481390: the table at `LH3DCreature` +0x51F0, the last number of the creature block for each bone
  (`LoadBinary` 0x4EBC7D; on the files 46↔28 for the ape, 34↔27 for the cow, 33↔24 for the tiger and the lion).
- **`Creature::UpdateSpellInfo`** 0x4F8750 (vt +0x5C), each turn of a spell it made and once before the initial one:
  unless the spell is cast from above (a NULL spell is not), +0x0C = `FillAverageHandPos` 0x48CC80; +0x18 = +0x00 −
  +0x0C, normalised as `1 / sqrt((z² + y²) + x²)` unless all three are 0 (an unordered part counting as 0). The first
  call comes before +0x00 is written, so a cast's first direction is the normalised −hands, until the spell's first
  turn.
- **`FillAverageHandPos`** 0x48CC80 is not the hand bone's own place. For the hand bone and for its mirror, fn_0083A0E0
  walks the bone records (fn_0083A140; records at the model's +0x10, 0x3C bytes each: parent, first child, right
  sibling): from the start bone along its right siblings, a bone with no child adds its translation (buffer[bone ×
  0x30 + 0x24..0x2C]), rounded to a float after each add, and is counted; a bone with children recurses into its first
  child. The sum is multiplied by `1 / count`. The two means are added and halved. Every species' hand bone has
  children and no right sibling, so the point is the mean of the fingertips (the ape's 61, 59, 57, 55, 53 and the
  mirror's 43, 41, 39, 37, 35). The buffer is `GetSafeBuffer` 0x4842B0: +0x5178, the posed skeleton of the **game
  turn** (`Creature::ProcessState` calls `LH3DCreature::UpdateTime` at 0x473939, which calls `UpdateBuffers` 0x4EC590,
  posed on the world matrix), or its frozen copy +0x5180 while +0x5270 is set (fn_004806E0, cleared by
  `ReconnectToGame`); not the drawn blend +0x47F0. In openblack's skeletons the first child is the lowest-index child
  and the right siblings follow in index order, so the walk can be rebuilt from the parents.
- **The release** fn_004F81F0: the held spell (+0x1078), when its duration (+0x50) is below 0, is closed down (vt
  +0x530), and +0x1078 cleared; the two beams closed down; the thing at +0x1084 deleted (what it is is not read). It is
  called by the two cast functions, `StopWhatIAmDoing` 0x475AAE, the fight 0x48B239, `GInterface::
  PlaceObjectInMagicHand` 0x5DA9EC (the creature picked up) and Finish 0x500E1C.
- **What openblack does:** a spell whose creator is a creature is paid for by its body (`magic::creator::MaintainSpell`
  for `Kind::Creature`: `creature_spell_casting::MaintainSpell` on `Creature::size` and `strength` and the
  `CreatureNeeds` energy and exhaustion, the share from `creatureMagicActionKnownAboutEntry[magic].field0x5c`).

### Movement, body over time, spells, fights, throws

`CreatureLocomotion` (speed towards a fraction of the top speed, slower uphill, walk and run blended by distance),
`CreaturePhysiology` (age, growth, energy and fat, tiredness, thirst, warmth through `gutils::SigmoidThreshold`,
strength, poo), `CreatureSpells` (freeze, small, big, weak, strong, fat, thin, invisible, nice, nasty, itchy and the
unfinished ones; each eases in, holds and eases out; the creature's sound actions per spell are 0x76..0x7E),
`CreatureFight` and `CreatureFightHud` (the duel in a circular arena and its panel) and `CreatureThrow` (the release
velocities). His own flight (`Fly`, bounce and slide) is left out: what a creature throws will fly through our
physics objects.

### Growth, size and age as time passes

Read in `runblack.exe` W120, 2026-10-09. `phys` is the creature's physical part (creature +0x160).

- **Where in the turn.** `Creature::ProcessState` 0x472DC0: the spells (0x472EB4), the pen's size
  (`ShrinkDownIfNearCitadel`, 0x472F72), then, unless the creature is in a locked select (`Flags +0x24 & 0x10`,
  0x472F77, which jumps to 0x4737DB), the autoscale step (0x472FB7), the turn counter phys+0x70 `++` (0x472FC2) at
  every development phase, and, inside the block run only when [0xD00DE8] is not 0 (0x4732FA `cmp [0xD00DE8], ebx`,
  0x473300 `je 0x473391`; see [the gate](#gplayermakecreatureempathisewithplayertowndesire-0x4c80f0), read as set),
  when [0xD00DE0] is also set (no static writer; read as set) and the phase (creature +0x1268) is at least 1,
  `UpdateAttributesAsTimePasses` 0x4EF510 (0x473337).
- **Age.** Inside it (0x4EF510..0x4EF53E): phys+0x08 `++` when phys+0x70 % ((1000 / [0xD01A38]) × info+0x210) == 0,
  all unsigned 32-bit integers: `div` of 1000 by the ms a turn (0x4EF517), `imul` by info+0x210 (0x4EF52E), `div` of
  the counter (0x4EF538). The turns a second are cut to a whole number before the multiply, so at 30 ms a turn the
  period is 33 × info+0x210. The counter runs from phase 0, so turns before phase 1 count towards the first tick.
  With more than 1000 ms a turn the period is 0 and the original's `div` faults; ours then never ages.
- **Growth.** 0x4EF5DF..0x4EF60A: `LH3DCreature::IsMoving` is called and tested (`test eax, eax`), but the
  conditional jump after it is two NOPs (0x4EF5E9, 0x4EF5EA): the moving test is patched out and the creature grows
  moving or not. It grows when the phase is at least 3 (0x4EF5EE), by `CalculateHeightIncrease` 0x4EFA60.
- **The size step** 0x4EFCA0: phys+0x6C += the growth; below 0 it is set to 0, above 2 (0x8AB478) to 2. It runs every
  turn the growth does, so a creature made bigger than 2 by other means is put back to 2 by its next growth.
- **openblack** (`creature_physiology::TickTurn`, `Grow`): the turn counter `Needs::turns` goes up at every phase, the
  age only from phase 1; from phase 3 the creature grows whether `Turn::moving` or not, and `Grow` keeps the size
  within 0 and 2. The debug window's body panel shows the size, the age and this turn's growth, and has a "Grow now"
  step (`CreaturePhysiologySystemInterface::GrowthOf`, `GrowNow`). The locked-select skip is not ported (see
  [Pending](#pending), "The body's turn while the hand holds the creature").

### Asleep, sleeping and sitting

Read in `runblack.exe` W120, 2026-10-10. `phys` is creature +0x160, the mind creature +0x164, the species' row creature
+0x28.

- **Which actions count as asleep.** The body goes by the action the mind carries out (mind +0xF60, a row of the
  creature action table in `info.dat`, the script's `CREATURE_ACTION` numbers), never by an animation. The energy
  drain gets +3 on its divisor when the action is 17 or 59 (0x4EF6D1..0x4EF6E1); the growth is multiplied by 3 when
  it is 17 or 78 (`CalculateHeightIncrease`, 0x4EFB35..0x4EFB45). In `info.dat` row 17 is `SleepAtHome`, 59
  `RestToGetBetter`, 78 `SleepByObject`; `SleepOnTheSpot`, the sleep openblack's creature does, is row 313, which
  counts as neither (read from the table's names, 256 bytes a row).
- **Sleeping** is the sleep sub-action 0x5086D0, one call a turn: life = clamp(life + info+0x23C, 0, 1) (`SetLife`),
  exhaustion phys+0x30 = clamp(exhaustion − info+0x240, 0, 1). It wakes (`fn_004848C0`) when the exhaustion is 0, it
  is not night (`IsVisualNight`) and the sub-action's turns (mind +0x1BDC) are more than 50; or when the exhaustion
  is below 0.1, those turns are at least `size(phys+0x6C) · info+0x374 · [0xD01A40] · 5` and more than 50
  (0x5087D4..0x508807). [0xD01A40] is `GGameInfo` +0x48 (`GGameInfo` at 0xD019F8): the day cycle's duration in
  seconds, written raw by `SetVisualTimeCycle` 0x557620 (0x557633), 1700 by default. The product is made in that
  order on the game thread's 24-bit FPU and stored as a float before the compare. With a sleep length of 0.1 a size-2
  creature sleeps 1700 turns of the default day.
- **Sitting** is the sub-action 0x500AC0 that plays an animation for a number of turns (its start 0x500A30 sets the
  count to the seconds × (1000 / ms a turn)): while that animation is 38, the sit loop (0x500AD7), exhaustion =
  clamp(exhaustion − 0.2 · info+0x240, 0, 1) each turn (0x500AE4..0x500B24). Life does not change.
- **Where in the turn.** In `Creature::ProcessState` the body's time-passing step (0x473337) comes before the mind's
  part of the turn (`Look` 0x4733A9, the plan checks and `ConstructSubActionsFromActionAgenda` 0x4734F8,
  `UpdateDesires` 0x47364D, `ForceActivityAndForceAction` 0x4736F9) and before the sub-actions (`ProcessSubAction`
  0x4737B0): the body reads mind +0xF60 as it stands before the mind's part of the turn, and the sleep and sit
  sub-actions run after the body, in the same turn, once a turn.
- **openblack** (`creature_physiology::SlowsEnergy`, `GrowsAsleep`, `LongEnoughAsleep`, `SleepTurn`, `SitTurn`):
  at its own turn the body asks the mind for the action it carries out
  (`CreatureMindSystemInterface::CurrentActionOf`: its plan's action, or the row its idle activity counts as, the
  same rows feedback is credited to), as the mind's turn, planning and learning of the turn before left it, and keeps
  it in `CreatureNeeds::action`; it takes the energy and growth flags from it and the day's length from
  `DayNightClock::GetDuration` (the cycle the land or a script last set). The mind rests a sitting creature
  (`CreaturePhysiologySystemInterface::Sit`) once a mind turn, after the body's turn, while the body plays the sit
  loop (`CreatureNeeds::sitting`). The sleep's heal and rest stay in the body's sleep step (`Rest::Asleep`). The
  debug window's body panel shows the action, whether it counts as asleep for energy and growth, sitting, and the
  turns after which it has slept long enough. What still differs is in [Pending](#pending), "What counts as asleep,
  still open".
- **What it moves in the checks** (measured 2026-10-10 against the build before it): on Land 1 the creature sleeps
  on the spot from turn 32 and, by the day's length, has not slept long enough at turn 119, where the old rule woke it;
  so it does not get up and think at turn 137 (three fewer synchronised draws there), and the fidelity run, the cycle
  and the hand demos move from turn 137, the land-view shots too. The temple shot `t120_200` and the testbed scenario
  `creature.walks_in_temple_pen` move with it (that scenario's random part from turn 130, 142 draws against 147).

### Thirst, fainting and drinking

Read in `runblack.exe` W120, 2026-10-10. `phys` is creature +0x160, the mind creature +0x164, the species' row creature
+0x28 (`info`).

- **Thirst** (`UpdateAttributesAsTimePasses`, 0x4EF7F4..0x4EF85D): from phase 3 (creature +0x1268), phys+0x34 =
  clamp(phys+0x34 + 1 / n, 0, 1), where n = `__ftol`((1000 / [0xD01A38]) · info+0x224): the turns a second are an
  unsigned integer `div` (0x4EF807), loaded with `fild` and multiplied by the float info+0x224 (0x4EF819), and the
  product is cut towards zero (`__ftol`, 0x4EF81F; 0x7A1400 takes `cvttsd2si eax` on a CPU with SSE2, `LH3DP3::HasSSE2`
  [0xE83A20], so a NaN or a product out of the int32 range gives INT32_MIN, 0x80000000; the x87 path without SSE2 would
  keep the low half of a 64-bit `fistp`); n is the low half `eax`, loaded back as an unsigned 64-bit integer (high half
  0, 0x4EF828..0x4EF82C) and divides 1 (`fdivr`, 0x4EF830). There is no test for n = 0: 1 / 0 is +infinity and the clamp
  makes the creature fully thirsty at once. Below 0 the result is set to 0, above 1 to 1. In `info.dat` every one of the
  17 species' rows has info+0x224 = 5000, so at 10 turns a second n = 50000 and the cut changes nothing there.
- **Fainting** has two gates. In `Creature::ProcessState`, after the mind's plan step, 0x47355B..0x473585 call
  fn_0047CE80 and then the faint test fn_0047CF00 only when `GetPlayer` (vt +0x1C) is not null, that player's type
  (`GPlayer` +0x8E0, `PLAYER_TYPE`) is not 2 (the computer) and the creature is not controlled by a script
  (`Flags` +0x24 & 0x400, 0x473576). fn_0047CF00 itself needs phase ≥ 5 (0x47CF03) and tests the script bit again
  (0x47CF10); then, in this order, life (`GetLife`, vt +0x11C) ≤ 0, energy phys+0x1C ≤ 0, exhaustion phys+0x30 ≥ 1
  each call fn_004C9FE0(0x1E, reason 3, 1 or 5, 0, 0, 0) and `Creature::Faint` 0x476FA0.
- **Drinking** is the sub-action 0x5052B0: phys+0x34 = 0, `SuppressDesire`(14, 20 s) on mind +8, and
  `Object::SetTemperature`(0, no source) 0x639A60 (0x5052DB): an existing fire on the creature takes the temperature 0;
  without one nothing is made, 0 not being hotter than it.
- **openblack** (`creature_physiology::TickTurn`, `ShouldFaint`, `FaintGates`; `CreaturePhysiologySystem::Drink`): the
  thirst cuts the turns a second and then the product to whole numbers (`map_coords::FtoL`, the SSE2 path: a NaN or
  out-of-range product is 2^31 turns, a negative one wraps round as unsigned), and a cut to none parches the creature in
  one turn. A creature faints only from phase 5, owned by a player (not the neutral one) whose `PlayerMagic` player type
  (`magic::players::ReadMagicOf`, the player system's for a player without an entity) is not 2
  (`PlayerMagic::k_ComputerPlayerType`), and not controlled by a script (`ScriptHeld::controlledByScript`, the 0x400
  bit), the three reasons in the original's order. Drinking empties the thirst and sets the creature's temperature to 0
  through the fire system (`FireSystemInterface::SetTemperature`), after which the mind suppresses the water desire for
  20 s as before. The debug window's body panel has an "Exhaust" trigger (exhaustion 1: it faints at its next turn if
  the gates allow). What still differs is in [Pending](#pending), "Thirst, fainting and drinking, still open".

### The size spells: the start delay and the targets

Read in `runblack.exe` W120, 2026-10-10. `tps` = 1000 / [0xD01A38], an integer.

- **The start delay.** `ActualReceiveSpell` 0x4F5060, for a new spell and for one let in from the waiting list
  (0x4F4E60, at 0x4F4EAE), puts the spell's slot in state 1 (waiting) with its counter = `ftol(tps + tps)`
  (0x4F5098..0x4F50C7, `fadd st0, st0`) and its hold = `ftol(tps · seconds)` (0x4F5071..0x4F5095). It is 2 s of
  whole turns for every creature spell, not only the size ones. `CreatureReceiveSpell::Process` 0x4F48C0 counts it
  down like the other parts (0x4F4AA9: the counter `dec`; the ease is called only in states 2 and 4, the per-turn hold
  call in every state, waiting included) and starts the spell (0x4F4951) at the turn it finds the counter at 0. At
  100 ms a turn: 20 turns of waiting, then the start. `tps` being an integer, the delay is `2 · floor(1000 / ms)`
  and every hold, start and finish `ftol(floor(1000 / ms) · seconds)` (0x4F4F76..0x4F4F9C for the time added to a
  holding spell, 0x4F499A..0x4F49C6 and 0x4F4A2F..0x4F4A62 for the start and finish parts): at 60 ms a turn, 32 and
  not 33.
- **Letting in the waiting spells.** `Process` walks the waiting list (0x4F4E60) from inside its slot loop, at the slot
  that has just worn off (state 4 → 0: 0x4F4A9C..0x4F4AA2, after the end packet 0x1B and the miracle let go), not
  after the loop, and once for every slot that wears off. The walk lets in, in the list's order, each waiting spell
  whose kind has nothing on (`IsCreatureSubTypeSpellActive` 0x4F5200 at 0x4F4E98), through 0x4F5060, and takes it off
  the list. The slot loop then goes on with the next slot, so a spell let in at a later slot of the table is counted
  down that same turn (its delay 2 tps − 1 at the turn's end), and one at an earlier slot from the next turn. A spell
  that stops without reversion (state 3 → 0, 0x4F49FB..0x4F4A1C) does not walk the list.
- **The targets.** Big 0x47DB50, small 0x47DBD0. With no joint-activity partner (mind +0x1BE8 = 0), big's is the
  creature's +0x374 and small's its +0x378, as they are. The `Creature` constructor sets +0x378 = 0.2 (0x4741DA,
  0x3E4CCCCD) and +0x374 = 2.4 (0x4741E4, 0x4019999A). SET_PROPERTY 34 `CREATURE_MIN_SIZE` writes +0x378 (0x70ECF3)
  and 35 `CREATURE_MAX_SIZE` +0x374 (0x70ED13), the value as given; GET_PROPERTY 34 / 35 returns the targets
  themselves (0x70DD7D / 0x70DD99). On anything but a creature both say "Object not a creature" and "Cannot Get
  Property %d" / "Cannot Set Property %d" (0x70E741, 0x70F287); the GET gives 0. The partner rule is in
  [Pending](#pending).
- **The ease.** Big 0x4F55F0: `v = target ≥ before ? target : before`; small 0x4F5540: `v = target ≤ before ? target
  : before` (`test ah, 0x41`). Then the size phys+0x6C = before + (v − before) · ratio, the target read again every
  eased turn.
- **openblack** (`creature_spells::Receive`, `Step`, `StartDelayTurns`, `SizeTarget`;
  `ecs::components::CreatureSizeLimits`): every spell waits the 2 s (`k_StartDelaySeconds`) before it starts, a
  queued one too, counted in whole turns a second (`WholeTurnsPerSecond`, as are the hold, start and finish
  times); a waiting spell is let in at the slot that wears off, inside the turn's slot loop; big and small aim for
  the creature's own largest and smallest size, the defaults 2.4 and 0.2 when it has none (`SizeLimits`), never
  shrinking it (big) or making it bigger (small). The script's CREATURE_MIN_SIZE and CREATURE_MAX_SIZE get and set
  them (`player_creature::GetCreatureProperty`, `SetCreatureProperty`; a set gives the creature the component). The
  debug window's body panel reads out both targets and casts big or small on the creature.

### The fatness the body shows

Read in `runblack.exe` W120, 2026-10-09. `phys` is the creature's physical part (creature +0x160), `3d` its drawn
body (phys+0x58).

- **Where in the turn.** `Creature::ProcessState` calls `Update3DCreatureFromAttributes` 0x477210 at 0x4737D2: after
  the time-passing body (0x473337) and after `ProcessSubAction` 0x4FEDA0 (0x4737B0), the step in which a meal is
  eaten, so the fat a meal puts on starts to show the same turn. It runs when [0xD00DE4] is not 0 (0x4737B5, no static
  writer; read as set), right after the exhausted slow-down `fn_004EFC40` (0x4737CB), and is skipped with the rest of
  that block under the locked select (the jump from 0x472F77 lands at 0x4737DB, past the call).
- **The shown fatness** phys+0x18 (the mind file's "previous fatness"): `d = clamp(phys+0x14 − phys+0x18, −0.01,
  0.01)` (0x477217..0x477243; a `d` below −0.01 is −0.01, above 0.01 is 0.01), then `phys+0x18 = clamp(phys+0x18 +
  d, 0, 1)` (0x477249..0x477278; below 0 is 0, above 1 is 1). The thin-fat target 3d+0xA4 = clamp(2 · phys+0x18 − 1,
  −1, 1) (0x477284..0x4772B8). With the FPU at 24 bits each `fsub` / `fadd` rounds to float, as openblack's float
  arithmetic does.
- **The other two targets, in the same function.** The evil-good target 3d+0x9C = clamp(alignment, −1, 1) (creature
  +0x168 → +0x08, 0x4772BE..0x4772FC); the weak-strong target 3d+0xAC = clamp(`ConvertStrengthTo3D` 0x4F02B0 (phys+0x0C,
  and a second argument read at [creature+0x28]+0x1F4), −1, 1) (0x477302..0x477368).
- **The shape drawn from the targets, once a turn and in steps** (read 2026-10-10). `Morphable::UpdateMorphing`
  0x618C40 runs in `LH3DCreature::UpdateTime` (0x481FD8, after `TimeWarpHeal`, before `AddEvilGoodSparkles` and
  before the turn's pose, `UpdateBuffers` at 0x48257F), once a creature turn. With the last applied values at 3d+0xA0,
  +0xA8 and +0xB0 and 0.03 at [0x900AD4]: when |evil-good target − applied| ≥ 0.03, all three axes take their targets
  (and the texture and the animations are redone); else when the thin-fat gap is ≥ 0.03, thin-fat and weak-strong
  take theirs (the animations redone); else when the weak-strong gap is ≥ 0.03, weak-strong alone. Each time the
  vertices are baked again on the CPU (`MorphVertices` 0x618D10: base + |w|·(mesh − base) per axis, the mesh of the
  weight's sign, the normals renormalised). Drawing never blends the shape: `PrepareForDrawing` 0x4ED320 uses the
  turn fraction for the pose only and never reads the morph. So the body's shape moves in steps, as the original
  draws it: thin-fat, whose target moves 0.02 a turn, about every two turns; evil-good and weak-strong in one step
  when the alignment or the strength moves by 0.03 or more. The other callers: `Creature::SwapMinds`, a script's
  ALIGNMENT set, `Creature::CanLoad3D` at load, two attribute setters, and `CHand::PrepareForDrawing` on the hand's
  own morph (not the creature's).
- **openblack's drawn shape** (`creature_morph::RefreshDrawn`, `CreatureAnimationSystem::PoseTurn`): the same rule
  and the same order of the axes, taken once a turn at the start of the turn's pose, after the shown fatness has taken
  its step; the frames draw the shape as it is. Not ported: the other callers' redraw at once (a script's ALIGNMENT,
  `SwapMinds`, the two attribute setters 0x4E9138 and 0x5077F9, whose purpose is not read): with them the shape waits
  for the next turn.
- **openblack** (`creature_morph::EaseFatness`, `CreaturePhysiologySystemInterface::ProcessShownFatness`): the shown
  fatness `CreatureMorph::shownFatness` takes its step last in the creature turn (`ecs::creature_loop::ProcessTurn`),
  after the minds, the actions, the fights, the spells and the autoscale, so a change to the fatness made by these in a
  turn (the time-passing burn, a meal, a spell) is followed the same turn, as in the original. A script's change is
  followed the next turn in openblack, since the game turn runs the scripts after the creature turn; where the
  original runs its scripts against the creature's turn is not yet read (see [Pending](#pending)). The shown fatness
  is kept within 0 and 1 even when the fatness is not (a script may set the fatness to any value). The debug window's selected
  creature shows the fatness and the fatness shown, with sliders for the alignment, the fatness and the strength. The
  alignment and strength targets, and the locked-select skip, are not yet as the original (see [Pending](#pending)).

### Leash and the hand

`LeashRules`, `LeashRope` (40 masses on springs at 200 Hz), `LeashOwnership` (one leashable creature per player),
`LeashKeys` (L, V, B, checked against our `input::GameActionMap`, read once a frame by
`ecs::creature_loop::ProcessLeashKeys`; his shake tracker is left out) and
`CreatureHandRules`. The hand holds only the player's own or an allied player's creature, in or out of the
influence (`Creature::ValidForLockedSelectProcess` 0x476E10 and the action press 0x5D13FB..0x5D1481; raffclar's rule
allowed any god's); openblack has no alliances, so only the player's own. Let go within 450 ms of camera time, the
press was a click: packet 0x5F, the TOGGLE_LEASH path below (openblack: the leash key). Strokes and slaps act at frame
time on the local machine (packet 0x27 only replays them elsewhere); how the creature was treated goes to its mind
with packet 0x59, the mind's "player feedback" fn_004E06A0 (`FinishActionUnsuccessfully("player feedback")`, then a
plan when |v| > 0.01). The hand's side is in [hand-and-interface.md](hand-and-interface.md#the-hand-on-a-creature).

#### The timed dominant desire

What a leash, a town tied to and the script's `SET_CREATURE_ONLY_DESIRE` force on a creature (read 2026-10-09).
Fields: mind (Creature +0x164) +0x700 the forced desire (0x28 = none), +0x704 its turns, +0x708 its whole seconds.

- **`CreatureMental::SetCheatDominantDesire(d, seconds, needsToo)` 0x4DCE20**, in order: +0x700 = d, +0x704 = 0,
  +0x708 = `ftol(seconds)`; `SetActivated(d, 1)`; for every other desire i of 0..39 a byte table at 0x4DCEF4
  (index i − 4, three targets at 0x4DCEE8) chooses: **never held down** 9 IdleWithPlayer, 15 RestoreHealth,
  16 BeFriends, 18 ManifestState, 23 Rest, 30 PlayWithPlayer, 35 HangAroundAtHome, 38 LookAround; **only with
  needsToo** 4 Hunger, 7 Poo, 8 Tiredness, 14 Water; every other `SuppressDesire(i, seconds)`. Then, if d is
  Compassion (1), `UnsuppressDesire(16)` and `MakeDesireFullyDominantAndChangeSource(16)`; last `UnsuppressDesire(d)`
  and `MakeDesireFullyDominantAndChangeSource(d)`. The desire numbers are openblack's `creature_desires::Desire` order
  (each source's owning desire in info.dat's source table agrees).
- `SuppressDesire` 0x4DC260: turns = `ftol((1000 div [0xD01A38]) · seconds)` (integer turns a second, a float product),
  kept only when larger than the counter (+0xA8 + 4i). `UnsuppressDesire` 0x4DC2D0 zeroes one counter,
  `UnsuppressAllDesires` 0x4DC2F0 all forty.
- `MakeDesireFullyDominantAndChangeSource` 0x4DC9A0: `MakeDesireFullyDominant` 0x4DC920 (only if the desire is
  activated: its value = its maximum +0x468, every other desire's value = the species' floor, creature info +0x274,
  activated or not), then `MakeAllSourcesMaximum` 0x4DE780 (every source of the desire but types 9
  AngerFromBeingDamaged and 33 RestoreHealthFromLife: `SetSource(type, 1)`), then `SetActivated(d, 1)`. So under
  Compassion, BeFriends ends unsuppressed, activated, its sources (34, 35) full and its value **back at the floor**,
  set there by Compassion's own step.
- `SetSource` 0x4DE6C0 clamps to 0..1 and calls the type's setter, a member-function pointer from a per-type table at
  0xC8D478 (32 bytes a row, getter +0, setter +0x10) filled by the static initialiser 0x4DD430. Setters exist for
  types 5, 11, 14, 21, 22, 26, 27, 32, 33, 35, 36, 37, 39 and 55. The innate ones write a mind field their getter
  reads: 5 kindness +0x1AA14, 11 aggression +0x1AA18, 26 lethargy +0x1AA1C, 35 friendliness +0x1AA20,
  39 communicativeness +0x1AA24. Of the leash's desires' sources (Impress 0, 1; Compassion 2..5; Anger 6..11;
  BeFriends 34, 35) only 5, 11 and 35 have setters, all innate.
- **Each turn**, `Creature::ProcessState` 0x473295..0x4732F0 (after the leash's confinement): if +0x700 is not 0x28,
  `UnsuppressDesire(+0x700)`, ++(+0x704), and when `(+0x704) div (1000 div [0xD01A38]) > (+0x708)` (unsigned),
  `UnsuppressAllDesires` and +0x700 = 0x28. With 10 turns a second and S seconds, it ends on the step that brings the
  count to (S + 1) · 10.
- **The clear** fn_004DCF20 (from `UpdateMoodsFromLeash` for ROPE, `SET_CREATURE_ONLY_DESIRE_OFF` and fn_004F5754):
  nothing if the creature has a player and `fn_0064A9C0(player)` → +0x12C → +0x1C is **not** 2 (`cmp [eax+0x1C], 2;
  jne` to the `ret`); otherwise, with no player or with that value 2,
  `UnsuppressAllDesires` if one was forced, and +0x700 = 0x28.
- **The clear's gate is the player's leash type** (read 2026-10-09). `fn_0064A9C0` 0x64A9C0 returns the first non-null
  of the 18 interface pointers at player +0x14, its +0x39C: the same walk and the same field as
  `GPlayer::GetNextInterfaceStatus` 0x64AAC0, so it is the player's first `GInterfaceStatus`. Its +0x12C is the
  `GLeashStatus` (as `UpdateLeash` reads it from `GetInterfaceStatusLeashOn`) and +0x1C the leash type, 2 = ROPE. So
  the clear acts only for a creature with no player or whose player's leash is the learning leash. Called from
  `UpdateMoodsFromLeash` for ROPE, the type was just set to 2 (by `SetOn`'s caller or `SetType`), so that call always
  clears.
- **The mood and need spells**: their start and end handlers, 0x4F5980..0x4F5E5F (past the label fn_004F5754, which is
  why that label shows nine calls). Each start: `SetActivated(d, 1)`, `MakeDesireFullyDominantAndChangeSource(d)`, then
  `SetCheatDominantDesire(d, [0x8D1598], 1)`; Compassion and Anger first `FinishActionUnsuccessfully("nice spell
  started")` unless the byte at +0x25 has 0x4 set, Scratch `"made itchy"` likewise. [0x8D1598] is a float in `.rdata`,
  **20000.0** (0x469C4000), read by these nine calls only: so the spells' 20000 s is the seconds they pass, always with
  needsToo 1, for Compassion 1 (0x4F59D6), Anger 2 (0x4F5AF6), Hunger 4 (0x4F5BF3), Fear 5 (0x4F5C53), Tiredness 8
  (0x4F5CB3), Illness 25 (0x4F5D13), Water 14 (0x4F5D73) and Scratch 21 (0x4F5DE7; and 0x4F5E1A, which first takes the
  leash off and does not make it dominant beforehand). Each end calls `MakeDesireLeastDominant(d, info, 1.3)`
  0x4DC9F0; only Compassion (0x4F5A50), Anger (0x4F5B70) and Scratch (0x4F5E30) then call the clear fn_004DCF20. The
  ends of Hunger, Fear, Tiredness, Illness and Water (0x4F5C00, 0x4F5C60, 0x4F5CC0, 0x4F5D20, 0x4F5D80) do not, so
  their dominance stays until its 20000 s run out or another call replaces it.
- **Callers**: `UpdateLeash` every turn, Compassion (GOOD, 0x4CE5F9) or Anger (EVIL, 0x4CE617, and on a creature it is
  tied to, 0x4CE673) for 36000 s, Impress for 120 s for a town (0x4CE574); the tie fn_005E6BD0 0x5E6DD9 (Impress
  120 s); `UpdateMoodsFromLeash` 0x4CEFC0 (GOOD Compassion, EVIL Anger, 36000 s; ROPE the clear); the script's
  `SET_CREATURE_ONLY_DESIRE` 0x6F476D (needsToo 1). Every leash call passes needsToo 0. Because the leash calls it
  every turn, the count restarts every turn and a leash's 36000 s never runs out while it is on, and every turn the
  other desires' values go back to the floor.
- **When the leash's moods are taken.** `UpdateMoodsFromLeash` is called through fn_005E6B00 (the heart's leash pick,
  then the player's creature's moods) from `GLeashStatus::SetOn(on)` (0x5E7001, before the lengths) and from the type
  setter 0x5E6AE0 (stores +0x1C, then 0x5E6B00; packet 0x65 below). The tie fn_005E6BD0 calls `SetOn(creature, 1)`
  (0x5E6D1A) every time, already on or not, then the tied lengths, `MimicPlayer`, the town-desire flags and, for a
  `TownCentre` (RTTI) on a leash that is not EVIL, `SetCheatDominantDesire(0 Impress, 120, 0)` (0x5E6DD9; the
  `FindDominantDesire` just before it is not used). `SetOn(off)` (0x5E7062..0x5E7091) clears with no gate unless the
  type is ROPE.
- **`UpdateLeash`'s order each turn** (0x4CE320, only while a leash is on): tied (+0x24), the kept point is the
  object's; then a `ShowNeedsVisuals` tie ([below](#the-leash-tied-to-a-needs-sign)); then the town: the object as a `TownCentre`, replaced by the
  `TotemStatue`'s +0x7C when it is one (the totem's town centre: its constructor 0x737B20 stores its first argument
  there at 0x737B78), and, when that is set, the leash type is not 1 (EVIL) and `FindDominantDesire(0x28)` is not 0,
  its `GetTown` (vt +0x48): with the creature's player, `GetBeliefInPlayer(n)` (n = player +0xB5) stored as a float,
  `GetMaxBeliefMeNotIncluded(n)` × 0.5 compared with it (`fcomp`, `test ah, 1`): Impress 120 s when half the
  largest other belief is at least it (ordered; an unordered compare does not), else only when the town's player is
  not the creature's; with no player, when the town's player is not the creature's (none). Not tied: the kept point
  is the hand's. Then the radius = the full length, then the type's own desire (GOOD Compassion, EVIL Anger, 36000 s)
  after the town's, so on the compassion leash Compassion takes over the town's Impress every turn and only a ROPE tie
  keeps it; then, EVIL and tied to a creature (`CastCreature`) whose `FindDominantDesire(0x28)` is not 2 (Anger) and
  `GetDistanceInMetres(other, creature)` below 8 × the leashed creature's `GetHeight` (`fcomp`, `test ah, 0x41`:
  strictly below, ordered), Anger 36000 s on the other; last the attitude step.
- **`CreatureDesires::FindDominantDesire(except)` 0x4DC5B0**: over the 40 desires, the activated ones but `except`
  (0x28 leaves none out), the first whose value is strictly above the best so far, the best starting at 0.0; it
  returns 0 when none is above 0, the same as Impress. Held-down desires count.
- **Where the creature's turn runs.** `Living::ProcessLiving` 0x5EC810 calls `ProcessState` (Creature vtable +0x620 =
  0x472DC0, 0x5EC8CF) for every creature every turn, after `ProcessReaction`. `ProcessState` returns before
  `UpdateLeash` only when the hand's locked select holds it (Flags +0x24 & 0x10, 0x472F77); nothing there tests a
  fight. The keep-alive (0x473295) runs after the leash's confinement and before `UpdateDesires` (0x47364D).
- **openblack**: the API is raffclar's `creature_spell_mind` (`SetCheatDominant`, `StepCheat`,
  `ClearCheatDominance`, `HeldDownByCheat`, `MakeFullyDominantOverOthers`, `CheatTurnsLeft`, `k_CheatSeconds` = the
  spells' 20000 s as the default seconds), with the state, `Cheat`, in `CreatureMindState::dominantDesire`. Behind it
  are the rules read here, `creature_desires::detail::SetDominant`, `StepDominant`, `ClearDominant`,
  `HeldDownByDominant` and `DominantTurnsLeft`, over `MakeFullyDominantWithFullSources`, `Unsuppress` and
  `UnsuppressAll` (tests `test_creature_dominant_desire` and `test_creature_spell_mind`). The leash calls them
  (`LeashSystem`): `TakeMoodOf` as the leash is put on, changed or tied (GOOD and EVIL make their desire dominant for
  `k_LeashDesireSeconds`, ROPE clears), every turn again for GOOD and EVIL, the town rule
  (`creature_leash::WantsToImpress`, `k_ImpressTownSeconds`, the tie's 120 s for a town centre abode), the anger of a
  tied creature, and the clear as any leash but ROPE comes off; every call passes the body's needs free, the
  species' `desireFloor` and the whole turns a second from `game_clock::MsPerTurn()`. The mind counts it with
  `StepCheat` once a turn in `CreatureMindSystem::ProcessTurn`, before its sources and desires update.
  `creature_desires::FindDominant` is `FindDominantDesire`. The leash's turn runs while the creature fights or is
  knocked out too; only its moving it (the hand's pull, the walks back) waits for the fight to end. The forced
  strength the planner once took (max(value, 36000)) is gone. `MakeFullyDominantOverOthers` stays raffclar's: the desire let go, then, if active, at
  its maximum and every other at the floor, with no sources filled and no activation, as the Hunger calls at 0x4F7DE5
  and 0x4F8174 do (`UnsuppressDesire(4)`, then the plain `MakeDesireFullyDominant` 0x4DC920).

#### The leash's collar end

- **The original takes it from the drawn 3D creature.** `GInterface::UpdateAllLeashes` 0x5D9130 (every frame, from
  `PostDrawProcess`), for each interface status whose leash is on, gets the 3D creature (`Creature::GetCreature3D`
  0x477850) and reads its collar from `TransformedMatrices` (the pointer at Morphable +0x47F0) at the leash bone's
  index +0x51D4 (0x30-byte LHMatrix, translation at +0x24; 0x5D91A1..0x5D91CC), then `GetLeashStartPoint` 0x5D8EA0 for
  the other end, then steps the rope `fn_00848970(GInterface+0x198, start, collar, dt)` (the drag ropes +0x19C /
  +0x1A0 alike in interface state 0x15). Laying or tying the rope (`GLeashStatus::SetOn` 0x5E6FC3..0x5E6FEE,
  `fn_005D4350` at 0x5D44B2) seeds it from +0x5178 `[+0x51D4]`, and `GetLeashStartPoint` takes a creature the leash is
  tied to from +0x5178 / +0x47F0 `[+0x51D4]` (0x5D8ED8..0x5D8EE6). The same point serves the rope's simulation, its
  tension, its drawing (`fn_008491B0`) and the hand's pick of it (`fn_00848F90`, from `UpdateInterfaceCollide`).
- **Those bones carry the drawn size and place (confirmed, 2026-10-08).** `LH3DCreature::SetSize` 0x480530 writes the
  size clamped to 0.05..4 to +0x90 (0x480599) and size × 15 / [+0x8C] to +0x94 (0x4805A1..0x4805B1).
  `LH3DCreature::UpdateBuffers` 0x4EC590 (from `UpdateTime` 0x481DF0 at 0x482498 and 0x48257F) builds the 3D
  object's world matrix with `fn_004EBE10`: the yaw from +0x84 and a lean of at most ±0.3, every rotation cell × [+0x94]
  (0x4EC024..0x4EC08B), the translation the object's own position +0x78 / +0x7C / +0x80 (0x4EC08E..0x4EC0A9). The
  skeleton is posed onto that matrix into the buffer at +0x517C: `fn_00839F10` (misnamed `LH3DTech::GetCameraPosition`
  in the symbols) multiplies each bone by its parent's matrix, a root bone (parent -1) by the matrix passed
  (0x839F46..0x839F66); `LH3DAnim::SetTransform` 0x83A1D0 at 0x4ED0F0 when there is no animation. +0x517C is then
  swapped into +0x5178 (0x4ED106..0x4ED118; the frame before stays in +0x5174). `LH3DCreature::PrepareForDrawing`
  0x4ED320 writes `TransformedMatrices` as a weighted sum of +0x5178 and +0x5174 (`fn_004EECD0` 0x4EECD0 at 0x4ED450
  and 0x4ED46B; the two weights were not traced). So in the pen, where `ShrinkDownIfNearCitadel` calls `SetSize` with
  0.22 ([Drawn smaller in the pen](#the-players-creature)), the collar is the small body's neck, and while the creature
  walks it is the neck where the body is drawn.
- **The lengths follow the drawn size.** The rope's slack and full length are `GLeashStatus` +0x28 / +0x2C, copied to
  the rope (+0x65C / +0x660) by `SetLocalLeashFromInterfaceStatus` 0x5D9510. `LH3DCreature::SetSize` 0x480530 sets
  them (0x480652..0x4806A7, `fn_005E6980` 0x5E69A2) for each of the owner's interface statuses whose leash is not tied
  (+0x24 = 0): slack 0.7 × 15 × +0x90 + 22, full 3 × 15 × +0x90 + 32 (0x92B360..0x92B36C). Since the pen shrink calls
  `SetSize` every turn with the pen size, the pen does change them.
- **openblack.** `LeashSystem`'s `CollarPoint` gathers the inputs of the pure `creature_leash::CollarAt`: the matrix the
  body is drawn with (`ecs::DrawnBodyModel`: the hand's and the physics' poses, between turns, the pen's scale, the
  slope shear), the posed bones and the rig's leash bone, the creature's height and `creature_pose::DrawnSizeShare`.
  With no bone the collar is the drawn translation lifted by 0.7 of the height × the share. `LengthsOf` takes the
  size it is drawn at (`creature_pose::DrawnSize`): in the hand `creature_leash::InHand(pen size)`, refreshed every
  frame the leash is held, and tied to a mobile its height (15 × the pen size), so the pen shortens them as in the
  original. The rope's laying, stepping, tension and drawing and a creature tied to another all go through it. Out of the
  pen and standing, the point is the one the Transform gave up to float rounding (`affine::Model` writes the position,
  `creature::PlacementMatrix` translates by R·(p·R)); with no bone it is bit for bit the same.
- **The tension.** `fn_00848970` writes it to rope+0x18: `clamp((|node − start| − slack/41) / (max/41 − slack/41),
  0, 1)`. **(inferred, static scan)** No game-turn code reads it: every load of `[reg+0x198]`, `[reg+0x19C]`,
  `[reg+0x1A0]` with the 9 instructions after it, the rope functions 0x848600..0x849FFF and `GetLeashStartPoint` were
  read; a read through a copied pointer would not be found. Its one other reader, 0x5D95E0, is called only by the tug's
  sender, which nothing calls ([The leash's tug](#the-leashs-tug)). The turn side, `Creature::UpdateLeash` 0x4CE320, keeps the
  creature to the hand's x, z (`GInterfaceStatus` +0xC8 / +0xD0, × 6553.6 at 0x4CE57B) at the leash's length
  `GLeashStatus` +0x2C (to Creature +0x11B4), never reading the rope. Ours does the same since 2026-10-10
  ([The walk back into the kept area](#the-walk-back-into-the-kept-area)); raffclar's pull at a tension above 0.8
  is gone.

#### The leash's rope

Read 2026-10-09 from runblack.exe (the functions below), with a float32 model of them (one rounding per operation:
the game thread's x87 runs at 24 bits, [audio.md](audio.md)). The rope object is `GInterface` +0x198 (the drag
ropes +0x19C / +0x1A0): +0x00 start, +0x0C end, +0x18 tension, +0x1C the 40 masses of 0x28 bytes (+0x00 position,
+0x0C velocity, +0x18 force, +0x24 stretch), +0x65C slack length, +0x660 full length, +0x668 half width, +0x66C v0,
+0x670 v1, +0x674 a byte, +0x678 the texture's scale along it, +0x67C the material.

| Value | Address | Bits | Used for |
|---|---|---|---|
| 1/41 | 0x9A3B88 | 0x3CC7CE0C | rest = slack × c, taut = full × c, the lay's step |
| 0.8 | 0x9A3B7C | 0x3F4CCCCD | the mass (gravity's force 0.8 × g, then F × dt / 0.8) |
| −9.81 | 0x9A3B78 | 0xC11CF5C3 | g (0.8 × g = 0xC0FB22D2) |
| −0.1 | 0x9A3B84 | 0xBDCCCCCD | the drag |
| 20000 | 0x9A3B80 | 0x469C4000 | the springs' stiffness |
| 0.5 | 0x8AA3B4 | 0x3F000000 | the drag's least length; the clearance over the ground |
| 90000 / 300 | 0x9A3928 / 0x8AB234 | 0x47AFC800 / 0x43960000 | the speed cap (squared / speed) |
| 0.005 | pushed as 0x3BA3D70A | 0x3BA3D70A | the sub-step, in seconds |
| −200 | 0x9A3B8C | 0xC3480000 | the sub-step count |
| 0.001 | 0x8AC418 | 0x3A83126F | the frame's whole ms to seconds (`UpdateAllLeashes` 0x5D9155) |
| −1000 / 6120 / 0 | 0x8CF9AC / 0x9A3B90 / 0x8AA398 | 0xC47A0000 / 0x45BF4000 / 0 | the ends' clamp |
| 0.15 | immediate at 0x848654 | 0x3E19999A | the half width written as the rope is laid |

- **Laying, `fn_00848600(start, end)`** (from `GLeashStatus::SetOn`): copies the ends; sets the texture scale 2.5,
  v0 0.125, v1 0.25, half width 0.15 and the byte 0, and leaves the tension as it was. `step = (end − start) × (1/41)`
  per component (the difference, then the product), `p = step + start`, then for each mass (0x8486CC..0x848771):
  `floor = GetAltitude(p.x, p.z) + 0.15` (no 0.5 here); the mass gets `p` as it is, still; then, if `p.y < floor`,
  `p.y = floor`, which only moves the running point, so the masses after it are laid higher, one mass late;
  `p += step`. Then the material (mode 15, `leash.raw`).
- **The look, once a game turn**: `GInterface::Process` (0x5CEC10, from `ProcessGameInputs` in
  `GGame::ProcessOneGameTurn`) calls `UpdateLocalLeashesFromInterfaceStatus` 0x5D94A0 (at 0x5CEC18), which calls
  `SetLocalLeashFromInterfaceStatus` 0x5D9510 for every interface status with a leash on. That rewrites the rope's
  slack +0x65C and full length +0x660 from the status (+0x28, +0x2C), the texture scale +0x678 = 2.5 and the byte
  +0x674 = 0, and then, by the leash type (status +0x1C), half width / v0 / v1: type 3 (Good) 0.225 (0x3E666667) /
  0.25 / 0.375 (0x5D957B..0x5D958F); type 1 (Evil) 0.15 / 0.375 / 0.5 (0x5D95A1..0x5D95CF); any other type 0.15 /
  0.125 / 0.25 (0x5D9555..0x5D9569). Only the lay uses 0.15 whatever the type; the steps (once a frame, from
  `GInterface::PostDrawProcess` → `UpdateAllLeashes` 0x5D9130) read the look's half width at +0x668 (0x84893A).
  Nothing else writes +0x668 but `fn_005D4350`, which copies the main rope's width into the drag ropes.
- **A frame, `fn_00848970(rope, start, end, dt)`** (from `UpdateAllLeashes`, dt = the frame's whole game ms × 0.001f):
  the targets are clamped (x, z to −1000..6120, y to 0..6120; `x <= lo`, or unordered, gives lo, `x < hi` keeps x,
  else hi). A negative dt is 0, and `n = 1 − ftol(dt × −200)` = 1 + trunc(200 × dt) (5 ms gives 2, as 5 × 0.001f
  rounds just over 0.005; 16 ms gives 4, 33 ms 7). Each end's move is `(target − current) × (1/n)`, worked out once;
  each of the n sub-steps adds it (`move + current`, so the ends come close to the targets but not onto them), then
  works out the forces (`fn_008487C0`) and integrates (`fn_00848830(0.005)`). After the loop the ends are set to the
  targets and the tension is `(|node 0 − start| − slack × c) / (full × c − slack × c)`: `t <= 0` or unordered gives 0,
  `t < 1` keeps t, else 1 (no guard when full = slack).
- **The forces, `fn_008487C0`**: rest = slack × (1/41); every mass from the positions at the start of the sub-step,
  mass 0 between the start and mass 1, the last between mass 38 and the end. For one mass, `fn_008484C0(prev, next,
  rest)`: `s = sqrt((vz² + vy²) + vx²)` (every length in the rope is summed z, y, x); `L = 0.5 > rest ? 0.5 : rest`;
  the force is set (not added) to `(−0.1 × (s × v)) × (1/L)` per component; `F.y = (0.8 × −9.81) + F.y`; then the
  springs to the previous and to the next point, `fn_008483C0(n, rest)`: `d = n − p`, `len = sqrt((dz² + dy²) +
  dx²)`; `len <= rest` (or unordered) writes stretch 0 and adds nothing; else stretch `len / rest − 1` (both springs
  write +0x24, so the next segment's value is kept), `k = rest / len`, `a = d × k + p` (the rest point),
  `F = 20000 × (n − a) + F`.
- **Integrating, `fn_00848830(0.005)`**, each mass in order: `v = (0.005 / 0.8) × F + v`; `q = (vz² + vy²) + vx²`,
  and if `q > 90000`, `v ×= 300 / sqrt(q)`; `p = 0.005 × v + p`; `floor = (GetAltitude(p.x, p.z) + half width) +
  0.5`, and if `floor > p.y`, `p.y = floor`. GetAltitude takes `ftol(x × 65536 × 0.1f)` map coordinates. Velocity
  first, then the position with the new velocity (semi-implicit Euler).
- **openblack** (`Creature/LeashRope`) takes the same steps, bit for bit against the model, since 2026-10-09: the cap
  300 (it was 150), lengths times 1/41 (they were divided by 41), gravity's force `k_Mass × k_GravityAcceleration` in
  one rounding (it was the literal −7.848f, one ulp off), the z, y, x sums, the drag and the springs in the original's
  order of operations, the ends moved by an equal share added each sub-step (they were placed at k/n of the way),
  `Create` laying over the land as above (it ignored the ground; `LeashSystem` passes the land's height), and the next
  segment's stretch kept. The lay uses the default look's half width 0.15, the one the original writes as it lays,
  whatever look the leash then takes; each step then uses the look of the leash's type (`creature_leash::LookFor`, the
  three looks above, set before every step), which matches the original's per-turn rewrite. `test_leash_rope.cpp` pins
  the model's bits; the pins assume no fused multiply-adds (MSVC x64 `/fp:precise` and x86-64 GCC and Clang do not
  fuse, and the configure rejects the flags that would). `LeashRope.cpp` is built with `-ffp-contract=off` on GCC and
  Clang, as the MPEG audio decoder. Kept as ours: the tension's guard when full ≤ slack (0), the unordered edges of
  the clamps and the spring (`std::clamp`; a length of 0 adds nothing), and `Create` still works out the tension (the
  original leaves +0x18 as it was; nothing reads it before the first step).
- None of the old differences made ours faster; the cap made it slower when yanked. What makes the rope look faster
  is the frame rate (Pending, "The rope and the frame rate").

#### The tied lengths, two creatures leashed together, and the young creature's home

Read 2026-10-09 with the disassembler.

- **The tied lengths.** `GLeashStatus::UpdateLeashLengthWhenAttachedToObject` 0x5E69E0(creature, object): when the
  object `IsTree` (vt +0x338: the `Tree` class only, `GameThingWithPos::IsTree` 0x402320 is 0, so a felled tree is not
  one), full +0x2C = the creature's `GetHeight` (vt +0x42C) × 6 (0x8AB35C), at most 40 (0x8CF300), and slack +0x28 =
  full × 0.5 (0x5E6A01..0x5E6A5A). For anything else (another creature, a villager, a town, a post): full =
  `GUtils::GetDistanceInMetres` 0x74CD70 (creature +0x14, object +0x14) × 1.5 (0x8AB24C), stored as a float; below
  180 (0x8CF38C, also when unordered) it is 180, above 360 (0x92B380) it is 360; slack half of it
  (0x5E6A62..0x5E6ACB). Each product is of two floats, so it is rounded once: single-precision arithmetic gives the
  same bits. `fn_005E6980` takes these when the leash is tied (+0x24), the hand lengths otherwise.
- **The tied lengths stay while tied.** `LH3DCreature::SetSize` 0x480530 calls `fn_005E6980` only for an interface
  status whose leash is not tied (`[+0x12C]+0x24` = 0 at 0x480677..0x48068B): a size change recomputes only the hand
  lengths.
- **openblack takes them at the tie only.** `LeashSystem::TieTo` sets them; `Update` recomputes the lengths only
  while the leash is held in the hand (until 2026-10-09 it also did on the first frame after a tie).
- **A felled tree.** openblack's felling (`ecs::trees`, the hand's dead tree) keeps the entity and swaps the `Tree`
  component for `DeadTree` (and `FelledTree`), so a leash tied to the tree stays tied to the dead tree. In the
  original the dead tree is a new object and the tree is deleted; `fn_005E6B90` drops the tie when the tied object is
  not `IsAvailable` (vt +0x2C, 0x5E6B9C..0x5E6BBC, then `StartImmersion(7)`). Where fn_005E6B90 is called from was not
  read (Pending).
- **The land number.** openblack's `MapScriptGlobals::landNumber` is what `SET_LAND_NUMBER` writes, as the
  original's g_game +0x205A08 ([map-loading.md](map-loading.md)), so the home rule's land test reads the same value.
- **openblack.** `creature_leash::TiedToTree` and `TiedToObject`; `LeashSystem`'s `LengthsOf` tells a tree by the
  `Tree` component and takes `gutils::GetDistanceInMetres`. There is no class of moving things any more.
- **Two creatures leashed together.** `Creature::UpdateLeash` 0x4CE5D2..0x4CE6CF: the step is −0.1 (0xBDCCCCCD,
  0x4CE678) on leash type 1 (aggression), +0.1 (0x3DCCCCCD, 0x4CE5FE) on type 3 (compassion), 0 on type 2 (learning).
  With the tied object cast to a creature (`CastCreature` vt +0xA4), when the other's mind (+0x164) +0x18C3C is 0 or
  the turn (g_game +0x205A40) minus it is above 600 (unsigned, `jbe`), it is set to the turn and
  `UpdateHowNiceIsCreature` 0x4C4D50 is called on both minds with the step, 0 included. The stamp lives on the other
  creature's mind: every creature leashed to it shares it, and untying or tying again leaves it.
- **`UpdateHowNiceIsCreature`** 0x4C4D50(creature, step): the attitude found by `GetAttitudeToCreature` 0x4C4C80 (none:
  skipped) gets howNice + step clamped to −1..1 (0x4C4D6E..0x4C4DA0); then two lessons of ±0.5: a step below 0 teaches
  anger (2) +0.5 and befriending (16) −0.5, any other step (0 included) the reverse (0x4C4DB6..0x4C4EA8).
- **openblack.** `creature_leash::AttitudeStepDue` and `AttitudeStep`; the stamp is `CreatureLeashAttitude` on the
  other creature, made the first time a creature is leashed to it.
- **The young creature's home.** `Creature::ProcessState` 0x473042..0x4730D2, every turn after `UpdateLeash`: when
  `GetInterfaceStatusLeashOn` is null, the development phase +0x1268 is below 5 (signed), `GetPlayer` is set, the
  player's +0x8E0 is not 2, `GGame::IsMultiplayerGame` 0x552F80 is 0, the player is the local one (g_game +0x18 +
  0xA60 × the byte g_game +0x205A59) and the land number g_game +0x205A08 is 1, the kept point +0x11A8 = the home
  +0x1200 and the radius +0x11B4 = 10.0 (0x41200000). When it fails nothing is reset. `IsConfinedToArea` 0x47CCE0
  then holds while the radius is above 0, and, leashed, while the leash works (`IsLeashWorking` 0x4CF0A0).
- **Player +0x8E0** is the player's kind: `TOGGLE_COMPUTER_PLAYER` sets 2 (a computer player,
  [magic.md](magic.md)); 0 is a player who is not playing (`GGame::GetNextActivePlayer` 0x5508D0).
- **Another kept area before it** (0x472FDF..0x47303C): with Creature +0x1294 and +0x1298 set, the kept point is the
  position of the object at +0x1298 and the radius min(`[+0x160]+8` as an unsigned integer, 150) + 50. Not identified.
- **openblack.** `creature_leash::KeptAtHome(HomeKeeping)`, called by `LeashSystem::ProcessTurn` for a creature with no
  leash and a home; `LeashSystemInterface::HomeKeepingOf` gives its inputs. openblack keeps no player kind and has no
  multiplayer, so both are passed as false. On Land 1 today the creature is at phase 13 (`LOAD_MY_CREATURE`), so the
  rule does not hold.

#### The walk back into the kept area

Read 2026-10-10 with the disassembler. One rule keeps a creature near the hand, near what its leash is tied to and,
young, near its home. Fields: Creature +0x11A8 the kept point (a `MapCoords`: x, z, altitude), +0x11B4 the radius,
+0x11B8 walking back, +0x380 led, +0x12A8, +0x1294 / +0x1298; mind (+0x164) +0x1C10, +0x1C14 the forced plan,
+0x1C38, +0x1C78 where it walks.

- **The point and the radius** (`Creature::UpdateLeash` 0x4CE320, every turn with a leash on): tied, the tied object's
  `Pos` +0x14 (0x4CE351); in the hand, the interface status's hand x (+0xC8) and z (+0xD0) × 6553.6, `ftol`, and
  altitude 0 (0x4CE57B..0x4CE5A9); then the radius = the full length `GLeashStatus` +0x2C (0x4CE5B7), whether the leash
  works or not. The young creature's home sets the home and 10 (above). `SetOn(off)` sets the radius to 0.
- **The test** (`Creature::ProcessState` 0x4730DC..0x473165), every turn, all of: +0x12A8 = 0; `IsConfinedToArea`
  0x47CCE0 (radius > 0, and with a leash on, `IsLeashWorking`); +0x380 ≠ 2; mind +0x1C10 = 0; mind +0x1C38 = 0 or
  +0x1294 set; the creature's Flags bit 0x400 (`CONTROLLED_BY_SCRIPT`, byte +0x25 & 4) clear; the point
  `MapCoords::InBounds`; and `GUtils::GetDistanceInMetres` (+0x14, the point) above the radius (`jne` on C0 | C3:
  equal or unordered stays).
- **The restart guard** (0x47316B..0x4731BC): while +0x11B8 is set, it is sent again only when where it walks (mind
  +0x1C78) is farther than g from the point, g = r × 0.5, or 5 when r × 0.5 is below 5 (0x8AB6E4).
- **The land under the point** (0x4731C2..0x4731DE): `LandAvoid[z][x]` of the point's cell (the high words of +0x11AE
  and +0x11AA) must be 0: water (6) and the rest stop it.
- **The walk** (0x473206..0x47328F): x = the distance, 0 below 0 (or unordered), 2r above 2r; `SetRequiredSpeed(x ×
  0.8 / 2r + walk)` (0.8 at 0x8C4A04, walk = the species row +0x254 through Creature +0x28, openblack's
  `GCreatureInfo::walkSpeed`), `LH3DCreature::EndAnyAnimsRapidly` 0x48F750, `fn_004C7220(point)`, +0x11B8 = 1, and +0x380
  = 1 when a leash is on. Tied, before it, mind[+0xF60 × 4 + 0x1CEA0] = 0 (0x4731E4..0x4731FF).
- **`fn_004C7220`** (CreatureDirectControl.cpp): mind +0x1C78 = the point; the 3D creature's `fn_0048F710(0, 1)`
  (0x4C7289; 1 pushed, then 0). In `fn_0048F710` the second argument gates four state-ending calls (0x48F71B..0x48F730:
  `fn_004848F0` state 6 → 7, state 5 → +0x5238 = 1; `fn_004845F0` +0x528C = 0 and a camera check in states 22..24, 26,
  27, 29, 32..34; `fn_004848C0` state 3 → 4, state 2 → +0x5238 = 1; `fn_00484AE0` state 8 → 9) and the first gates
  `LH3DCreature::StopMoving` 0x484260 (0x48F739..0x48F73F), so the walk back's own call does not stop the creature;
  `ForceActivityAndForceAction` 0x4C4450 with desire 0x18
  (ObeyPlayer) and the belief about itself; mind +0x1C14 = 1; the sub-action agenda's `fn_004FF5C0`; +0x11B8 = 1; and a
  move sub-action whose `SubArgumentPointAndFloat` holds the point and max(`GetHeight`, radius) (0x4C72ED..0x4C731E:
  the radius when the height is below it).
- **The end of the walk.** +0x11B8 and +0x380 are cleared by `Creature::StopWhatIAmDoing` (0x475C4A, 0x475BD2) and at
  creation (`fn_00474130`); `SetOn(off)` clears +0x380 and the radius, not +0x11B8, so a walk back under way goes on.
- **A point it can't stand on** (read 2026-10-10). The move is sub-action 8, `MoveToPos` (named and filled in by the
  sub-action table's initialiser, 0x4F9267..0x4F9373: step 0 is 0x5018A0, then 0x501950, which waits on
  `LH3DCreature::IsMoving`). Step 0 calls 0x47A880, whose move 0x483FE0 first tests the point with `fn_00483850`
  (`fn_00483890(point, 7.1)`, the creature's radius). Refused, it returns 0 and step 0 calls
  `FinishActionUnsuccessfully("Stopping because navigation failed")`: mind +0x1C14 = 0, then `FinishActionGeneric`'s
  `StopWhatIAmDoing` (+0x11B8, +0x380 and mind +0x1C14 to 0, the plan's desire +0xF50 to 0x28). The failure is
  0x47A880 returning 0 or 2 (tested at 0x4FFAF9 and 0x4FFC2D), and the call is
  `FinishActionUnsuccessfully("Stopping because navigation failed", 1, 1)` (0x4FFC9C..0x4FFCA7): the two 1s pass through
  `FinishActionGeneric` to `StopWhatIAmDoing`, whose `fn_0048F710(1, 1)` (0x475B76) runs both the state-ending calls and
  `StopMoving`. That is what stops the creature on a refused walk. Nothing picks another
  point or tries again within the walk. The sub-action runs in `ProcessSubAction` (0x4737B0), after the mind's choice
  (`fn_004F0370`, 0x47369D), which chooses nothing while the plan has a sub-action left (agenda +0xFB4 below +0xFBC,
  step +0xFB8 below 3). So every turn the test sends it again, its mind stays out and the move fails: the creature
  stands where it is for as long as the point stays where it can't stand.
  openblack: `WalkBack` is refused by `StartMove`'s `IsValid(point, k_CreatureRadius)`; `KeepWithin` then stops the
  creature (the failed move's `StopMoving` above) and sends it all the same, so its mind stays out for the turn;
  stopped, the walk is over at the top of the next turn and it is sent again.
- **`EndAnyAnimsRapidly`** sets the 3D creature's +0x5730 for its animation states (+0x4994) 2..12, 14, 16, 22..25
  and 37 (the byte table at 0x48F780); `UpdateTime` then doubles the animation step +0x48BC (0x481E6C) and
  `StateSet` 0x484EC0 clears it. So the action it was playing runs at twice its speed until its next state.
- **What the fields are.** +0x12A8: set by 0x47D9A0 (called at 0x48D39D, the 3D creature's route update, when the route
  follower's state +0x64054 is 6) with a forced ObeyPlayer plan to pick up or kick one of up to three things in its way; cleared by
  `StopWhatIAmDoing`. +0x380 = 2: set by `GPacket::ProcessPacket` after the tie (fn_005E6BD0, then
  `ForceActOnObject` on the tied thing, 0x63D49C..0x63D50B) and after the leash sends it to act on a thing or a point
  (0x63D520..0x63D5B9): doing what the leash sent it to do. Mind +0x1C10: set by `MoveByTeleport` 0x47A18F and
  `SetupReactToTeleport` 0x4F3AFE. Mind +0x1C38: written by `ForceActOnObject` (0) and `ForceActOnMapCoord`.
  +0x1294 / +0x1298: a creature made by 0x474BA0 for an object (+0x1298), kept within min([+0x160]+8, 150) + 50 of
  it (0x472FDF..0x47303C).
- **openblack.** `creature_leash::ShouldWalkBack(WalkBackCheck)` in the original's order, `WalkBackRestartDistance`,
  `WalkBackHurry` and `WalkBackArrival`; `LeashSystem::ProcessTurn` takes the point and radius (the hand on the
  ground, the tied object's `Transform`, the home) and its `KeepWithin` reads the script control from `ScriptHeld`,
  the cell from `land_avoid::At`, and starts `CreatureLocomotionSystemInterface::WalkBack` (the species' walk
  fraction + the hurry). The walk sets `CreatureLeash::returning` and `returningTo` (+0x11B8, mind +0x1C78) and, with a
  leash on, `Control::Led` (+0x380 = 1); the mind's plan becomes the forced one (ObeyPlayer, action 1, itself;
  [The leash's tug](#the-leashs-tug)) and `obeying` stands for mind +0x1C14.
  The walk is over when the locomotion stops (arrived, or given up), which stands for `StopWhatIAmDoing`. The rope's
  tension and slack trigger nothing. A fight stops the walk back, as openblack's own guard.

#### The leash's tug

Read 2026-10-10 in W1.20 (bwdis, with a scan of every `E8` / `E9` rel32 in `.text` and every dword of the image).

- **What sends it.** Packet 0x57 is sent only by the unnamed Creature method at 0x4CE750 (one float argument),
  through the unnamed `SetPacket` overload 0x551530 (type, object index, player number, float: the float to the packet
  +0x10, the player to +0x14, function number 0x20), at 0x4CE844 and 0x4CE8A9. The method: a debug message by +0x380
  ("Leash Control NONE" / "PULL"); `GLeashStatus` S = `MyInterfaceStatus` +0x12C; the rope's tension (0x5D95E0, the
  rope +0x18) to S+0x20 ("Leash sent force %.2f"); untied, the object at `GInterface` +0x3C8 (its index, or 0) is sent
  when the argument is above 0.8 and +0x380 ≠ 2 ("Leash is TAUT, so PULLING creature"), else "Leash is LOOSE"; tied,
  the tied object (cast to `GameThingWithPos`) is sent at once while +0x380 = 0 ("Leash is attached to object, so
  DIRECTING"), else under the same 0.8 test.
- **Nothing calls it.** No call, jump or stored pointer reaches 0x4CE750; nor 0x4CE6E0 (a pull setter: −1 gives
  `SetPull(0)` and `SetOn(off)`, otherwise `SetPull(v)`) nor 0x4CECF0 (it sets the pull and calls 0x4CEE80, the lead
  onto an object). The pull, `LH3DCreature` +0x48B0, is written only by `Init` (0), the fade in `UpdateTime`, `SetOn(off)`
  (0) and 0x47FBF0 (`SetPull`: below 0.2 it stores 0), which only 0x4CE6E0, 0x4CECF0 and the tug (0x4CEBC0) call. So in
  W1.20 the tug never happens and the pull stays 0: a creature in the hand moves only by
  [the walk back](#the-walk-back-into-the-kept-area). raffclar's stand-in (the tension above 0.8 pulls) is close to
  this method's taut test, which the game never runs; that test is on the method's argument, not on the tension the
  method reads.
  Checked again 2026-10-10 with a scan of every call into the `SetPacket` overloads (0x550900..0x551800, the type
  pushed or moved in the six instructions before): type 0x57 is sent only at those two places, and no hand drag of the
  rope sends it (`HandStateTug` 0x5B7DF0 / 0x5B8070 is the tree uproot). openblack matches: nothing in the game calls
  `LeashSystem::Tug`.
- **The packet** (`GPacket::ProcessPacket` 0x63C420: the type byte +1 minus 6 indexes the jump table; type 0x57 is case
  0x51 at 0x63CEE4): the object from the packet (0x63DF70), the player +0x14, that player's creature (+0xA4C), then
  0x4CE8E0(object). The float is not read.
- **0x4CE8E0(object)**, in order: nothing when +0x12A8, +0x11B8 (walking back), or the creature's Flags bit 0x400 (held by
  a script) unless [0xC64150] (only `Creature::OnClearMap` writes it, 0); +0x10B4 = 1; nothing when mind +0x1C00 (only
  0 is written to it, by `Creature::Initialise` and `StopWhatIAmDoing`); an object not `IsInteractable` (vtable +0x190)
  is dropped; +0x380 = 0: 0x4CEC00 (pulled away, below); no player or no leash on: nothing more; +0x380 set with a
  valid plan (`CreaturePlan::IsValid`, mind +0xF48) whose belief's object (mind +0xF58, +0x30) is not itself: tied,
  nothing more; the 3D body's +0x78 within 10 of the hand (+0xC8, 3D) (`fcomp`, C0), nothing more;
  `SetRequiredSpeed(walk + pull × (2·run − walk))` (species +0x254 / +0x258, 0x4CEA13..0x4CEA51). With an object
  (0x4CEA5E..0x4CEAE8): needs the plan's belief, the object neither the plan's nor itself and more than 0.0001 from
  mind +0x1C04; tied, 0x4CEE70(object); mind +0xF74 = 0.1; unless the script bit, +0x10B8 = 1. With none
  (0x4CEAEB..0x4CEBF0): the hand as `MapCoords`; +0x380 set: the route's end (0x483ED0, the route follower's
  +0x64074 / +0x64078) and `GetDistanceInMetres` to the hand over the creature's (+0x14), when that is above 0
  (`fcom` with C0 | C3), below 0.5: nothing more; then mind +0x1C78 (where it was last sent) more than 1 from the
  hand (C0 | C3 clear): 0x4C7220(hand), `SetPull(1)`, S+0x20 = 1, mind +0x1C14 = 1, +0x380 = 1.
- **0x4CEC00 (pulled away).** d = the plan's desire (mind +0xF50); when d < 0x28 and no forced plan (mind +0x1C14 = 0):
  ++mind[+0x660 + 4d]; above 1, `SuppressDesire(d, count × 30)` and the count to 0; then 0x4C9FE0(0x28, 7, 0, 0, 0).
  Always `FinishActionUnsuccessfully("Pulled by leash man")`, then a plan (`SetToZero` on it, so every field 0):
  desire 0x18 (ObeyPlayer), action 0x59 (89, the action table's go-to-hand row), the belief about itself at +0x10,
  0.01 at +0x2C (`CreaturePlan` 0x4F1230 fills +0x1C..+0x2C with one value), made current with
  `ConstructCurrentPlanFromPlan(plan, 1)` 0x4F1560.
- **Who clears the forced plan** (mind +0x1C14; every write found by scanning `.text` for the offset, read
  2026-10-10): to 0 by `StopWhatIAmDoing` (0x475C1F), `FinishActionUnsuccessfully` (0x475773, before its
  `FinishActionGeneric`, which calls `StopWhatIAmDoing`), the mind's choice `fn_004F0370` (0x4F03C3, once the plan has
  no sub-action left); `fn_0047CFB0` (0x47D126), `fn_0047D950` (0x47DA59), `ReduceLife` (0x47DDCB) and
  `GScript::DevFunction` also write it from a register, not followed. `StopWhatIAmDoing` also sets the plan's desire
  (+0xF50) to 0x28 (0x475A91). So when the leash's walk ends, by arriving or by failing, the plan left has no desire and a tug counts nothing for it.
- **0x4C7220 (sent walking)**, the walk back's too: `ForceActivityAndForceAction` 0x4C4450 with (0x18, no belief,
  action 1 (walk to a point), the belief about itself, none, 1, 0) (0x4C7294..0x4C72A7); arriving within
  max(`GetHeight`, +0x11B4) (0x4C72ED..0x4C731E).
- **The fade.** `LH3DCreature::UpdateTime` 0x481DF0 (vtable +0x14; also in `CreatureFalling`'s and `CreatureCitadel`'s
  vtables) is called once a creature turn, from `Creature::ProcessState` at 0x473939 with
  `ftol([+0x160]+0x48 × ms per turn)`, when [0xD00DE4] and mind +0x20CE8 are set (0x4737B5..0x4737F3). At
  0x482062..0x4821E8, while the pull is above 0.05 (C0 | C3 clear): the body force below, then pull × 0.95 stored, and
  0 when that is below 0.3. Led or not.
- **The body force.** In the same block: towards the leashed creature's interface status point +0xC8 (or its own +0x78
  with no player), mass (`GetMass` 0x47FA80) × pull × 15 along the normalised direction; that force goes to 0x4813B0,
  whose result goes to 0x47FAC0 (which divides by the mass and writes +0x48AC); −0.9 × the force is kept in locals.
  Neither callee was followed. With the pull always 0 in W1.20 this never runs.
- **openblack.** `creature_leash::DecideTug(TugCheck)` is 0x4CE8E0's no-object path in that order (`Tug`'s
  `pulledAway` and `Lead`); `LeashSystem::Tug` fills it: walking back = `CreatureLeash::returning`, the script bit =
  `ScriptHeld`, led = `Control::Led`, the plan = `CreatureMindState::planner.current` and its object, the body to hand
  with `glm::distance`, the map distances with `gutils::GetDistanceInMetres`, the route's end =
  `CreatureLocomotion::destination`, where it was last sent = `returningTo`. Pulled away: `RecordPull` and
  `creature_desires::Suppress` for the plan's desire, at 1000 / `game_clock::MsPerTurn()` whole turns a second as
  `SuppressDesire`, unless `obeying` (mind +0x1C14) or the plan is the walk the leash sent it on (`{ObeyPlayer,
  k_WalkToPointAction, itself}`): openblack's walk ends without touching the plan, which the mind keeps until it makes
  its own, where the original's leaves no desire; the plan becomes `{ObeyPlayer, k_GoToHandAction, itself}` with
  `planActive` false. To the hand: `LeadTo` at the old pull, arriving within `WalkBackArrival(height,
  confinementRadius)`, and refused (the hand where it can't stand) the creature is stopped, as the original's failed
  move (`FinishActionUnsuccessfully("Stopping because navigation failed", 1, 1)`, which reaches `StopMoving` through
  `StopWhatIAmDoing`'s `fn_0048F710(1, 1)`);
  then `SendWalking` (returning, returningTo, the plan `{ObeyPlayer,
  k_WalkToPointAction, itself}`, `obeying`), pull 1, `Control::Led`. The walk back sets the same plan. `FadePull`
  runs at the top of every creature's leash turn. Nothing in the game calls `Tug`; the creature spawner's leash
  section has a "Tug" button.

#### Tying the leash with the hand

Read 2026-10-10 in W1.20 (bwdis; the scan of every call into the `SetPacket` overloads 0x550900..0x551800).

- **The gesture.** A double click of the Action (right) button while the leash is in the hand. `CMouse::ProcessButtons`
  0x61A150 tests the ControlMap's second binding (+0x618, mouse 5, the Action button) with `fn_00470DE0` (0x61A39F) and
  sends interface message 6, the double click (`fn_0061A510` → `fn_005D9DD0(6)`), except while the temple interior is
  up (g_game +0x205A28 == 1). Message 6's handler 0x5D9930 (entry 6 of the table 0xD186B8, stride 0x38) sets
  `GInterface` +0x39 |= 0x08 and |= 0x02 and +0x40 |= 1 when the low byte of +0x38 is 0 or has bit 1. The idle action
  states 0 and 1 share the process 0x5D5190, which after `ResetActionState` takes +0x39 & 0x08 as the tie (0x5D4210 at
  0x5D51DC) before the grab press (0x5D1320) and the action press (0x5D1330): the double click is taken in place of an
  action press.
- **The decision, `GInterface` 0x5D4210**, in order:
  - the target is the action collide object (+0x400), else `FindObjectNearMapCoord` 0x5D39E0 at the action point
    +0x3F0 (the nearest within 5); none: nothing;
  - the interface's player's creature (`GPlayer` +0xA4C), whose player must be the interface's; none: nothing;
  - the first of the player's interface statuses (`GetNextInterfaceStatus` 0x64AAC0) whose `GLeashStatus` (+0x12C) is
    tied (+0x24): on the tied object or on the creature, packet **0x5C with object 0** (untie), otherwise nothing;
  - not tied: the target must not be the creature, `ValidAsInterfaceTarget` (vt +0x6F0) must be 1 and
    `GetInterfaceStatusLeashOn(creature)` 0x4CF060 must be this interface's status (+0x39C), the leash in this hand;
    then a `OneOffSpellSeed` (RTTI) is tapped (`SendTap` 0x5D38A0), and anything whose `ValidAsInterfaceLeashTarget`
    (vt +0x6F4) is not 0 gets packet **0x5C with its index** (`fn_00550C80`, 0x5D430A);
  - always at the end +0x38 = 0 and `ResetActionState`, and it returns 1.
- **Which things.** `ValidAsInterfaceTarget` is 1 for every `Object` (0x402840) but `ScriptHighlight` (0x709800 → 0).
  `ValidAsInterfaceLeashTarget` is 1 (0x402850) but `LeashObj` (the temple's posts, 0x464850) and `SpellIcon`,
  `TownSpellIcon`, `TownCentreSpellIcon`, `WorshipSpellIcon` (0x55D410). So the leash ties to anything in the world
  (trees, buildings, villagers, animals, a village centre, a totem, a needs sign, another creature, a rock) but a
  temple post, a spell icon, a script highlight and its own creature; a double click on a one-off seed taps it.
- **The single tap with the leash in the hand.** `GInterface::Tap` 0x5D3930(object): first, unless the leash is on
  (+0x14), untied (+0x24 = 0) and the object `IsReward` (vt +0x3F4), `fn_005D36D0(object)` (not read;
  0x5D393C..0x5D3967: it runs with the leash off, tied, or the object not a reward; no object skips it and the leash
  path); then with the leash on and untied, `ValidAsInterfaceTarget` == 1 and `ValidAsInterfaceLeashTarget`, 0x5D4180
  sends **packet 0x5D** (the object) when the creature's leash is in this hand (`GetInterfaceStatusLeashOn` == +0x39C)
  and its current sub-action is not 0x5D (93; mind +0xFB4 the agenda index, +0xFD8 its sub-action, 0x8F when the agenda
  count +0xFBC is 0); sent: +0x38 = 0, `ResetActionState`, return 1; otherwise `SendTap` 0x5D38A0. On the land,
  0x5D3D10(point) returns 1 on every path. First the origin (0x5D3D16..0x5D3D32): the first two words of the point
  compared with 0 as integers (so -0.0 there is not the origin) and the third as a float (`fcomp`, `test ah, 0x40`: -0.0
  counts, and so does a NaN, unordered setting C3); at the origin it jumps to 0x5D3DC0 and returns 1 having done
  nothing: no packet, no land tap, +0x38 left as it is. Elsewhere the leash on and untied, a creature, and the agenda's
  current sub-action not 0x5D (read without the count test) send **packet 0x5E** (the point, 0x550CB0), then +0x38 = 0
  (no `ResetActionState`) and return 1; there is no in-this-hand test. Otherwise the land tap (0x6DCB90, and the point
  kept at 0xD4EEE8..0xD4EEF0, 0x5D3D9E..0x5D3DBA).
- **The packets** (`GPacket::ProcessPacket`, the type byte − 6 indexes the jump table 0x63DDCC):
  - **0x5C** (0x63D416): the object from the packet (0x63DF70) cast to `Object`, the sender's player's creature
    (+0xA4C); needs Creature +0x1110 (the learning leash known), `GetInterfaceStatusLeashOn` ≠ 0 and its leash works
    (`GLeashStatus` +0x18). No object (none sent, or gone): `SetOn(creature, 0)`, then `fn_005E6EA0(creature)` on the
    sender's status: +0x24 = 0, `SetOn(creature, 1)` (whose on path tests only Creature +0x1110, else it turns the
    leash off, 0x5E6F91; it returns nothing, and the sound below plays whatever it did), and for the creature's player
    being the local one and the local player's own status (`MyInterfaceStatus`)
    sound 0x94 + t, t = ([0xBF3630] == 0) stored back (the static starts at 1, so 0x94 first, then 0x95, in turn).
    An object: `fn_005E6BD0(creature, object)` (the tie: its own sound turn [0xBF362C], +0x24, +0x34, `SetOn(1)`, the
    tied lengths, `MimicPlayer`, the town and needs-sign branches), `FinishActionUnsuccessfully("pulled by leash", 1,
    1)`, `ForceActOnObject(object)` 0x4C5FF0, Creature +0x380 = 2.
  - **0x5D** (0x63D4B6): the sender's interface status's leash works (+0x18), a creature and an object:
    `ForceActOnObject(object)`, +0x380 = 2. No test of the leash being on or of the learning leash.
  - **0x5E** (0x63D57E): a creature, `GetInterfaceStatusLeashOn` ≠ 0 and works: `ForceActOnMapCoord(point)`
    0x4C6540, +0x380 = 2.
- **Other callers of the tie** (not the hand): the script's `AttachObjectLeashToObject` 0x6F456B and the hand demo's
  `AttachLeashToObject` 0x653CC0 (registered by 0x64E720 at 0x64EF46; its tie call 0x653D6A), which call
  `fn_005E6BD0` directly.
- **openblack.** The rules are `creature_leash` (`Creature/LeashTie.h`): `TieTargetKind`, `ValidAsTarget`,
  `ValidAsLeashTarget`, `DecideHandTie(HandTieCheck)` → Nothing / Tap / Tie / Untie in 0x5D4210's order, and
  `DecideLeashTapOnObject` / `DecideLeashTapOnLand` → NotLeash / ActOnObject / ActOnPoint / Nothing (the land at the
  origin, `IsTapOrigin`: the first two coordinates bit for bit, the third as a float). The packets are
  `game_packets::Type::LeashTie` (0x5C), `LeashActOnObject` (0x5D) and `LeashActOnPoint` (0x5E). `ecs::leash_tie`
  (`ECS/LeashHandTie.h`) holds the hand area's entry points, `OnHandDoubleClick(player, target)` and
  `OnHandTap(player, target)` (a bool) / `OnHandTap(player, point)` (a `LeashTap`), which take the kind from the
  components (`LeashPost`, `SpellIcon` for every icon kind, `ScriptHighlight`, `OneOffSpellSeed`), decide and push the
  packet, and the turn's handlers, registered after the temple leash's in `Game::ResetRegistryForNewLand`. 0x5C needs
  `Knows(Rope)`, `IsLeashed` and `Works`, then `LeashSystem::ReturnToHand` (`TakeOff`, then on again in the holder's
  hand with no `WhyNot` and no refusal kept, only when the creature knows the learning leash as `SetOn`'s on path,
  untied, whether it works kept, the untying sounds 148 / 149 in their own turn for the local player whether or not it
  went back on) or `TieTo`, whose result is not looked at, then always `PullAwayFromAction` and `ActOn`.
  `PullAwayFromAction` stands for `FinishActionUnsuccessfully`: it stops the creature (`StopWhatIAmDoing`'s
  `fn_0048F710(1, 1)`), and clears `CreatureLeash::returning` (+0x11B8, 0x475C4A), `Control::Led` (+0x380, 0x475BD2) and
  `obeying` (mind +0x1C14, 0x475773 and 0x475C1F), and gives up the plan (`planActive`, `planner.current`, the idle
  plan; the agenda cleared at 0x475A86 and the plan's desire +0xF50 = 0x28 at 0x475A91). `ActOn` adds to the mind's
  `leash.actOn` on every call, as `ForceActOnObject` acts on every packet: the mind takes the last entry and clears the
  list, and the fight system finds a creature already fighting on a repeat, so the repeats change nothing. 0x5D tests
  only that a worn leash works (`!IsLeashed || Works`): openblack keeps the flag on the worn leash, so with none on the
  leash counts as working, as every leash starts. 0x5E checks `IsLeashed` and `Works` and does nothing yet. `TieTo`
  keeps no refusal of its own for posts, icons, highlights or seeds: the script's tie has none.
- **What the hand area adds**: the Action button's double click, taken in the idle state in place of the action press;
  the target (the object under the hand, else the nearest within 5 of the action point); a call to `OnHandDoubleClick`
  (on `HandTie::Tap` its own tap; either way +0x38 = 0 and `ResetActionState`); on a thing, its own `fn_005D36D0` step
  first (with the leash off, tied, or the thing not a reward), then `OnHandTap(player, target)` before its own tap
  (true: +0x38 = 0, `ResetActionState` and no tap); on the land, `OnHandTap(player, point)` before its own land tap
  (`ActOnPoint`: +0x38 = 0, no `ResetActionState`, no tap; `Nothing`: no tap and +0x38 left; `NotLeash`: its own tap).
  Until then nothing sends these packets, so the verification runs are unchanged. The creature spawner's leash section
  has "Tie with the hand" (closed by default): the decision for the thing last clicked and the nearest thing, and the
  tie and untie packets.

#### The leash tied to a needs sign

Read 2026-10-10 in W1.20.

- **Which sign.** A `WorshipTotem`'s constructor 0x7808B0 sets its +0x100 to its worship site and makes a
  `ShowNeeds(totem)` (0x719B60 → 0x719AB0), which makes three `ShowNeedsVisuals`, the `GShowNeedsInfo` rows 0, 1 and
  2 (0xD99738 + i × 0x114), at ShowNeeds +0x18 / +0x1C / +0x20, each owned (+0x60) by the ShowNeeds. Row 3 is the
  workshop's own sign (`Workshop::CallVirtualFunctionsForCreation` 0x779429), whose owner is not a `ShowNeeds`: it never
  qualifies. `fn_00719B90` (from `WorshipSite::Process` 0x77B2B1) sets each sign's desire (+0x5C) with `fn_00719E80`:
  row 0 the totem's `CalculateDesireForFood` (vt +0x420), row 1 `CalculateDesireForRest` (vt +0x424), row 2
  `CalculatePeopleHidingIndicator` (vt +0x428).
- **The fill, `fn_0071A200`.** The desire (+0x5C) against the info's +0x108 (`MaxNeedValue`), `fcompp` with `test ah,
  0x41`: the most strictly above the desire gives desire / most, otherwise (equal, below, unordered) most / most. At the
  game's 24-bit precision the quotient is a float.
- **At the tie** (`fn_005E6BD0`, 0x5E6DE0..0x5E6E87): the tied object cast to `ShowNeedsVisuals`, its +0x60 to
  `ShowNeeds`, that one's +0x14 to `WorshipTotem` with +0x100 set; then mind +0x21D0..+0x21DC = {0x50A760, 0, 0, 0} and
  +0x21E0 = `AddBeliefAboutObject(creature, sign)`. No fill test.
- **Every turn while tied** (`UpdateLeash` 0x4CE363..0x4CE46A): the same casts, then the fill against 0.6
  (`fcomp qword` [0x8CF7D8], `test ah, 0x41`: strictly above, ordered); the slot set again unless it already holds
  exactly {0x50A760, 0, 0, 0}, with a fresh belief.
- **The slot** is a member-function pointer (INFERRED from the layout) run once by the mind's free choice
  `fn_004F0370` (0x4F048F..0x4F04E4) when there is no forced plan (+0x1C14 = 0), Creature +0x94 = 0, +0x384 = 0,
  `fn_004C74E0` is false and no plan step is left: it is called with (+0x21E0, +0x21F0), then +0x1C14 = 1 and the slot
  is cleared to {0, 0, 0, −1}.
- **0x50A760**: action 0x12C (300, fish for the worship site) when `KnowsAction(NORMAL, 4 KNOWN_ABOUT_FISH)`, then
  action 0xF6 (246, food for the worshippers) when `KnowsAction(MAGIC, 14 MAGIC_TYPE_FOOD)`, which wins; with one,
  `ForceActivityAndForceAction(0x18 ObeyPlayer, none, action, the sign's belief, none, 1, 1)` and +0x1C14 = 1. The
  sign's row is never tested: tied to the rest or the hiding sign above 0.6 it feeds or fishes all the same.
- **openblack.** The rules only, `creature_leash` (`Creature/LeashNeedsSign.h`): `NeedsSignRow`,
  `NeedsSignQualifies`, `NeedsSignFill`, `NeedsSignArmsAtTie`, `NeedsSignArmsThisTurn` (`k_NeedsSignWants` 0.6, the
  float fill compared as a double) and `NeedsSignAction` (`k_FoodForWorshipAction` 246 over `k_FishForWorshipAction`
  300). Nothing calls them: the totem's signs and the mind's kept action are parked (Pending).

### Drawing the leash

The original draws the worn leashes in one stage after the models (`GInterface::DrawAllLeashes` 0x5D9310, stage 6 of
[original-frame.md](original-frame.md#2-draw-stages-in-order)), on the land path only (not in the citadel), in
`leash.raw` / `leasha.raw`, the rope in material mode 15. `fn_008491B0` builds the strip on the land at H + 0.1
([0x8AB22C], [rendering-objects.md](rendering-objects.md)); its body is not decompiled.

openblack (`Graphics/LeashDraw`, `Graphics/RendererLeash.cpp`), compared with raffclar's `RendererLeash.cpp` on
2026-10-08:

- **The same as his.** `leash_rope::BuildRibbon` (the same in both trees) gives the rope's ribbon facing the eye and
  the shadow's ribbon flat on the land at `k_ShadowLift` 0.1 (the original's H + 0.1): black, alpha 0x41, fading to 0
  at both ends. The shadow is drawn first, blended, tested against depth without writing it, in the main view only;
  then the rope.
- **Ours, kept.** Both ribbons go through `world_triangles` with render-mode materials: the rope in mode 15
  (`TexturedChromaAlpha`, alpha tested, writing depth, as the original's leash material), the shadow in
  `AlphaTexturedAlphaNoZWrite` **(inferred)**. The rope's corners take the land's light under them on the CPU, hazed
  **(inferred)**. Both are drawn at once, right after the land and its models, and are not sorted among the
  blended things (`leash_draw::Draw::middle` is computed but nothing reads it). His tree lights the rope in a GPU
  `Leash` shader and also draws it in the sea's reflection at half light; ours draws no leash in the reflection. The scripts' `SET_DRAW_LEASH`
  is one flag the draw reads (`script_control::CameraControl::drawLeash`), as the original's single GScript +0x78;
  his copies it to a `drawn` flag on every creature's leash. Without the leash textures ours draws nothing, where his
  falls back to a white texture.
- No creature wears a leash on Land 1 or Land 2, so nothing is drawn there.

### The temple's leash posts

Read 2026-10-09 in W1.20 (bwdis; bw1-decomp has the headers and symbols, `src/Black/CitadelHeart.h`, no bodies).
In our tree: the creation index skip in `CitadelArchetype.cpp` (5, as below); the pure rules in `Worship/LeashPosts`
(`worship::leash_posts`: the point, the four draws' ranges, the spin step, the sprite cell, the collar band, the
smoke's brightness and colour, the pick's two mappings, the scripts' reading of the pick and the tooltips;
`test_leash_posts`); and the posts themselves, `Worship/TempleLeash` (`worship::temple_leash`, tested in
`test_citadel_plan`). `CitadelArchetype::CreateHeart` calls `CreatePosts` right after the heart goes into the map
cells, with `game_random::crt::Random`: three entities, post 0 first, each with a `Transform` at its point (unturned,
scale 1: `LH3DObject::SetPosition(point, y_angle 0, scale 1)`) and a `LeashPost` {index, owner, heart, spin}, the spin
seeded by its four draws; the heart gets a `TempleLeash` {posts, pick -1}. The posts are not in the map cells and not
saved, and they go when the heart's entity is destroyed (an `on_destroy<TempleLeash>` listener; `Game::LoadMap`
disconnects it before the registry's clear, which takes the posts with everything else). They are drawn, the hand
feels them and taps them, and the leash picked there reaches the creature at the next turn (below). The Citadel
debug window's "Leash posts" node reads out a player's posts (point, spin, smoke cell), which of them show now and the
pick, sets the pick, and has a "Draw the posts" switch (`EngineConfig::drawLeashPosts`; the spin moves on either way).
The collar is one mesh in the cache for every post (`resources::shared_assets::LoadLeashCollar`, from the game's
start, `misc/leash`), its textured primitives retextured (`L3DMesh::SetSkinOfTextured`) with `misc/leash_collar`:
`leash.raw` with `leasha.raw` as its alpha, cut to ARGB4444 (`Texture2DLoader::FromColourAlphaTag`, `Argb4444`), the
game's one texture of the rope's two files (`test_leash_collar`).

**`Data\Misc\leash.l3d`** (2788 bytes, read 2026-10-10): flags 0x22000, two sub-meshes (flags 0xE0000800: LOD mask 7),
each one primitive of 34 vertices and 32 triangles, material type 9 (`TexturedChroma`), ALPHAREF 20, flags 4 (tiling,
one-sided, the object's UV offset added), colour 0xFF31798B, skin 0xFEA6C2F6. The file carries **no skin**: its skin
table starts at the end of the file, as `sun.l3d`'s and `mist.l3d`'s. The two sub-meshes are the same ring of 16 faces round
the z axis (radius about 1.06, z from -0.35 to 0.35), half a face apart: u runs round the ring (0..1, with the seam
doubled), v from 0 to 0.125, one eighth of the texture: the
`SetAnimatedUV(u, v(i))` of the draw moves it onto the leash's band and scrolls it round. (inferred) the skin id stays
in the primitive's texture slot (never resolved, as the mist's and moon's), so fn_0080B250's test for a texture sees
it and replaces it: every primitive of the file takes `leash.raw`.

**The draw in our tree** (frame side, `Graphics/RendererLeashPosts.cpp`, the pure `graphics::leash_post_draw::Build`,
`test_leash_post_draw`). `Renderer::PreDraw` calls `CollectLeashPosts` once a frame outside the temple (the original's
`CitadelHeart::DrawNow` runs for every heart from the landscape draw, fn_00467360, whatever is on screen, and not in
the citadel's draw). For each heart, `worship::temple_leash::ShownPosts` (`leash_posts::Shown`, tested in
`test_citadel_plan`): the heart's `CitadelHeart::drawPercent` (the original's +0x9C, clamped by
`SetHeartDrawPercent`) exactly 1, its player's creature (`LeashSystemInterface::PlayersCreature`) and that creature
knowing the post's leash (`Knows`). For each post shown: `leash_posts::Step` on its `LeashPost::spin` with
`FrameSeconds(game_clock::FrameGameMs())` (0 while paused, as the mists); the spin is a field of `LeashPost`, whose
contents no state hash part reads (`pools` hashes only the storage's entities). Then `Build`: the collar's matrix
`leash_posts::CollarMatrix` (the post's `Transform`, fixed at creation, then `affine::MultiplyReversed` by
`RotationYXZ(0, x turn, z turn)`), its texture offset (scroll, `CollarBand`) and its smoke, a `billboard::Sprite` of
half width 2 at the post's point, cell `SpriteCell`, colour `SmokeColour(SmokeBrightness(land_light::FullLight))`. The
picked post of the local player's temple (`creature::LocalPlayer`): nothing while the hand entity has
`components::NotDrawn`; else the orange smoke, additive, and the collar `leash_posts::CollarOnHand` on the hand's root
bone (`HandSystemInterface::GetHandMatrix()` × the first of `GetBoneMatrices()`) with the hand entity's scale (its
`Transform::scale.y`, the model scale the original's +0x44 is). `DrawPass` queues each post as two Z objects when on
screen, its smoke first (key (x² + y²) + z² at the point, the screen test a sphere of the half width), then its
collar (key (x² + z²) + y² at its origin, `BoxInView` of the mesh). The drain draws the smoke through
`world_triangles::SubmitRaw` (smoke.raw and smokea.raw, `materials::k_Smoke` or `k_SmokeAdditive`) and the collar
with `DrawMesh` on a transient instance row (its matrix, no fade, `frame_anim::PackUvOffset(u, v)`, the land light
alone for its colour).

**The hand in our tree** (`HandSystem::PickObjectAlongRay`, HandPlacement.cpp). Every invisible draw collision is one
test, `ecs::hand_pick::InvisibleSphereAlong` (`test_hand_pick_reject`): the teleport stones' (radius 3, unchanged) and
the posts'. Outside the temple, `worship::temple_leash::HandPosts` gives every temple's shown posts (`ShownPosts`), each
at its point read again this frame (`PointNow`: the special point through the heart's `Transform`, on the land, as at
creation), but the local player's picked post while the hand entity has `NotDrawn`; each is a sphere of
`leash_posts::k_HandCollisionRadius` 1.0, after the stones and before the creature, the nearer hit taking the cursor.
`hand_tap::Register<LeashPost>` (HandSystem.cpp): valid to tap = `temple_leash::InterfaceValidToTap`, the post's owner
the local player (`leash_posts::ValidToTap`); a post needs the hand in the influence (LeashObj keeps
`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1, vt 0x714) and never goes into the hand (it keeps
`Object::ValidForPlaceInHand` 0x402870 = 0, vt 0x6FC, so `HandSystem::ValidForPlaceInHand` refuses it), so a press on
one is a tap at once (the hand's TapOnly branch, as an abode's), and the hand state over it is 18 Over Object
([hand-and-interface.md](hand-and-interface.md#the-text-of-each-state-table-0xbf1c10)). The Citadel window's "Leash
posts" node reads out what the hand feels of the player's posts.

**The tap and packet 0x65 in our tree.** The tap (`temple_leash::InterfaceTap`, from the tap packet at the turn as every
tap; `leash_posts::Tap`): nothing for a post that is not the local player's; the picked post: pick -1, no sound, nothing
sent; any other: sound 42 (the in-game bank, 2D, no owner) for the local interface, the pick = the post at once, then
`game_packets::Type::LeashType` 0x65 with the leash (i + 1) and the sending player (`Packet::player`). Its handler
(`temple_leash::ApplyLeashType`, registered by `temple_leash::RegisterPacketHandler` in `Game::LoadMap` right after the
hand's): refused when the player's creature is under the compassionate or angry spell
(`creature_spells::Spells::IsActive` of `SpellOf(CREATURE_RECEIVE_SPELL 8 / 9)`, `leash_posts::LeashRefused`; tested
only with a creature); otherwise the creature's leash (`LeashSystemInterface::ChangeType`, our home of the original's
`GLeashStatus` +0x1C) and the temple's pick (`SetPick`). The moods are not the handler's: `ChangeType` takes them once at
the change while the leash is on (`UpdateMoodsFromLeash`, below), the one place that sets them at a type change, as the
original reaches them from the type set. Tested in `test_leash_posts`, `test_citadel_plan` and `test_leash_system`. The
Citadel window's node has Tap buttons that tap a post as the local interface.

**The tooltip over a post.** `GetOverwriteInteractableToolTip` (vt 0x194) is read only in hand state 13 Can Select Lock
(fn_005D77C0, 0x5D787E: its text, else 0xE85; the only call through vt 0x194 in `.text`). A post never reaches 13:
fn_005D7F20 gives 13 only through `ValidForLockedSelectProcess` (vt 0x6CC), which LeashObj keeps from Object (0x419330 =
0), and 9 only through `ValidForPlaceInHand`, also 0, so over a post the state is 18 (its IsInteractable, vt 0x190, is
GameThingWithPos's 0x5701B0, IsAvailable). State 18 shows, in the influence and valid to tap, `GetOverwriteTapToolTip`
(vt 0x19C, LeashObj keeps GameThingWithPos 0x5705C0 = 0) or 0xE7A: **over a post the hand shows 0xE7A "Tap"**, and over
another player's post (not valid to tap) the land texts. Ours: state 18 through the same rules
(`HandSystem::SubmitToolTips`); state 13's own-text lookup takes the post's (`leash_posts::ToolTipOf`), which no post
reaches there either.

**Land 1** (start-up answer 3): `LOAD_MY_CREATURE(1850, 1300)` at turn 7 loads the profile's creature
(`C4ba71b36.erc`), and `LandControl1` at turn 30 calls `DEV_FUNCTION(2)` then `DEV_FUNCTION(3)`
([map-loading.md](map-loading.md)): from then on the creature knows all three leashes, and the temple, built by the
skip's `SET_PROPERTY(22, citadel, 1.0)`, shows its three posts. The test runs' locked land view (camera at
(1752, 52, 2650) looking at (1786, 26, 2800)) faces away from the temple at (1915.05, 2508.89), so the posts are off
its screen.

**When and in what order.** `CitadelHeart::Create` 0x464E20 builds the heart (its constructor 0x4649B0, an Object),
then `CallVirtualFunctionsForCreation` 0x4675A0: its 3D object (CITADEL type 8), the **CitadelEntrance** (an Object,
ctor fn_00468EB0 from 0x4676B9), `InsertMapObject`, a camera object (fn_00454960, not an Object), fn_008499C0, and at
0x4677DA `CitadelHeart::CreateLeashes` 0x464950. That does nothing when heart +0xE4 is set; else `new(0x10)`
`TempleLeash(heart 3D object +0x40, heart player +0xB5 = player number)` 0x464650 into +0xE4. A heart therefore takes
**five** creation indices: heart, entrance, post 0, post 1, post 2 (`TempleLeash` itself is not an Object).

**`TempleLeash`** {+0x0 the pick, +0x4..+0xC the three `LeashObj*`}. For i = 0, 1, 2 (one loop, 0x46466D..0x464818):

- **The point.** `GetExtraPos(i, &point)`, LH3DObject vt+0x1CC; for the citadel type fn_008831C0 (vtable 0x9A2BFC;
  the static type's is 0x80FF20). Metric i of the object's mesh (0x30 bytes each, translation p at +0x24) through the
  object's matrix, every step a float: x = ((p.y m3 + p.z m6) + p.x m0) + t.x, y = ((p.x m1 + p.y m4) + p.z m7) + t.y,
  z = ((p.x m2 + p.y m5) + p.z m8) + t.z (m0..m8 the rows at +0x14, t at +0x38; 0x883289..0x8832D0). When the object
  morphs with the land (vt+0x1F4; the citadel's is fn_0080BA90, always 1) y = (H(point) - H(origin)) + y, both heights
  `LH3DIsland::GetAltitude` at x and z taken as (v × 65536) × 0.1 truncated (0x8832E3..0x883377). With no metric i
  (or no metrics) it is the object's own position (0x883238, 0x883388). Then `MapCoords(point)`.
  `B_FIRST_TEMPLE` (and `b_temple00`) has 16 extra metrics; 0..2 are (-15.1034, 5.3527, -12.0655),
  (-17.5088, 5.38235, -14.5343) and (-19.6542, 5.35506, -18.2662), and 15 is the pen point.
- **The post**, `new(0x78)` `Object(MapCoords, GObjectInfo 0xD43368)` (0x636520, the next creation index), vtable
  `LeashObj` 0x8C84A0: +0x6C = the owner's player number, +0x54 = i, +0x58 = the TempleLeash.
  - +0x40 = `LH3DObject::Create(STATIC)`, mesh `MeshPack[0]`, `SetPosition(point, 0, 1)`. Never drawn itself: the draw
    copies its matrix.
  - +0x5C, **the collar** = `LH3DObject::Create(STATIC)` with collar mesh i, `SetPosition(point, 0, 1)`,
    `SetDynamicLighting(1)`, `SetAnimatedUV(0, v)` with v = 0.375 for i = 0, else i × 0.125, then `SetNeedSorting(1)`.
    v is the band of the leash texture for that type: the same v0 as the worn rope's (`creature_leash::LookFor`).
  - The collar meshes (fn_004645D0, lazily into 0xC5E3D8..0xC5E3E0): **three loads** of `Data\MISC\leash.l3d`
    (`LH3DMesh::CreateFromHD`), each retextured by fn_0080B250(mesh, fn_008485E0()). fn_008485E0 returns
    `data\textures\leash.raw`, loaded once with flags 0x41 (ARGB4444) into [0xEF7540], the rope's texture;
    fn_0080B250 replaces the texture of every textured primitive of the mesh. The three copies are identical.
  - **Four CRT draws** (`Random` 0x81D180), in this order: +0x60 = Random(0, 1), +0x64 = Random(0, 2π),
    +0x68 = Random(0, 2π) (2π = 0x40C90FDB), +0x74 = Random(0, 15). 12 per temple, post by post.
  - +0x70, **the sprite** = `LH3DSprite::Create(1, 1)`, texture [0xEA1ABC] (smoke), its two extents × 2 / old size and
    size 2, colour 0x80FFFFFF, position = the point (never moved after).
- At the end the pick = **-1**.

**`LeashObj`**: `SaveObject` 0x464840 returns 0 (never saved; remade with the heart), `ValidAsInterfaceLeashTarget`
0x464850 is 0 (the leash cannot be tied to a post), `GetText` "Leash Obj". The three are deleted by the heart's
**destructor** (0x464BF0, from the scalar deleting destructor 0x464BC0), not its `ToBeDeleted`: after its +0xA8
object, it calls fn_004648C0 on +0xE4 (0x464C1F; each `LeashObj` deleted at once through vt+4 with 1, no
`ToBeDeleted`), frees the `TempleLeash` and clears +0xE4. So the posts live as long as the heart object, an unavailable
heart's included.

**The pick.** Setter fn_004648E0 (`CitadelHeart::SetLeash`) and getter fn_00464920, as in
[Script enum and the temple's selection](#script-enum-and-the-temples-selection).

**The draw** (frame side), fn_00466730 from `CitadelHeart::DrawNow` 0x46733D. Nothing unless the heart's player has a
creature (+0xA4C) and the heart 3D object's +0x9C == 1.0. +0x9C is the percent built, clamped to 0..1 by the citadel's
vt+0x200 fn_00883120, set each turn by `Citadel::Process` 0x462E33 from the heart's `GetPercentBuilt`; reaching 1 also
sets the player's flag [0xEB9A1C + 4 × player]. For each i (t = i + 1):

- **Gate**: post i is skipped (no draw, no collision) unless Creature +0x1108 + 4t is set (+0x110C EVIL, +0x1110 ROPE,
  +0x1114 GOOD: the creature knows that leash).
- The point is read again with `GetExtraPos(i)`; dt = float(`g_game_time_inc`) × 0.001.
- **Spin and scroll**, each a float stored, then wrapped by x − float(trunc(x × 1/P)) × P:
  +0x64 += 0.1 dt and +0x68 += 1.0 dt (P = 2π, 1/P = 0x3E22F983); +0x60 += 0.5 dt wrapped by x − float(trunc(x)).
- The collar: `SetAnimatedUV(+0x60, v(i))` (u scrolls at 0.5 a second); its matrix = the post object's matrix, then
  `MulPre` (fn_007FAE60) by `SetYXZMatrixOnly(0, +0x64, +0x68)` with no translation: it turns in its own frame.
- **The sprite's frame**: +0x74 += 10 dt, wrapped with P = 15 (1/P = 0x3D888889); `ftol(+0x74) & 0x3F` into the
  sprite's flags (low 6 bits): frames 0..14 at 10 a second.
- **Brightness** b = trunc(float(B + G + R of [0xEDDD08]) × 0.16666667), [0xEDDD08] = the terrain light table's entry
  255 ([rendering.md](rendering.md#terrain-light-table-0xedd90c)).
- **The picked post** (pick == i) of the local player's temple:
  - if the hand's 3D object (`GInterface +0x3A0` = `CHand`, `+0x482C`) has +0xAC set, the post is not drawn and gets no
    collision this frame. +0xAC is "the hand is hidden": 0 by `CHand::Show` 0x46C1B0, 1 by fn_0046C2E0;
  - otherwise the sprite is (b << 24) | 0xC18119 in the additive smoke [0xEA1AC4], and **the collar hangs on the hand**:
    at the translation of the hand object's +0x80 matrix (`Morphable::TransformedMatrices`, CHand +0x47F0, set at
    0x46CB10), with that matrix's `GetYXZ` angles, then `MulPre` by diag(0.5 s) with translation (0.05 s, 0.25 s, 0),
    s = the hand object's scale (+0x44, from CHand +0x94) × 173.52948.
- Any other post: the sprite is (b << 24) | 0xFFFFFF in [0xEA1ABC].
- Each post drawn: `LH3DSprite::AddDrawing`, the collar's draw (vt+0x100), then
  **`GInterface::SendInvisibleDrawCollision(post, point, 1.0)`** 0x519960: the hand's pick of it, a sphere of radius 1
  at the point just read (the picked one too).

**The tap.**

- `InterfaceValidToTap` 0x464450 ignores its argument: post +0x6C == `players[GGame::PlayerIndex]` +0xB5 (g_game
  +0x205A59 the index, the players at g_game + 0x18, 0xA60 each). Only the local player's own posts.
- `InterfaceTap` 0x464490: not the local player's post: 1, nothing done. The picked post: pick = -1, 1, no sound and no
  packet. Otherwise sound 42 `G_ClickOnSpell_01` when the status is `MyInterfaceStatus`, pick = i at once (frame
  side), and `SetPacket(0x65, i + 1, 0)`.
- `GetOverwriteInteractableToolTip` 0x464580 (vt 0x194): i = 0 → 0xEC8, 1 → 0xECA, 2 → 0xEC9; only hand state 13
  reads it, which a post never reaches (above).

**Packet 0x65** (turn side, `GPacket::ProcessPacket` → 0x63DCD7): when the player has a creature, the packet is dropped
while it is under the creature spell COMPASSIONATE (8) or ANGRY (9) (`IsCreatureSpellActive`, creature +0x370).
Otherwise `GLeashStatus` (interface status +0x39C → +0x12C) +0x1C = type (0x5E6AEA), then fn_005E6B00: the heart's
`SetLeash(type)` again (the pick, synchronised), `Creature::UpdateMoodsFromLeash(type)` when the player has a creature,
and for the local player it clears a block of g_game +0x25006C (+8, 0x320 dwords; bytes +0xC88, +0xC90, dword +0xC8C).

**`Creature::UpdateMoodsFromLeash`** 0x4CEFC0, once at the change and only while a leash is on
(`GetInterfaceStatusLeashOn`): EVIL → `SetCheatDominantDesire(2 ANGER, 36000, 0)`, GOOD → (1 COMPASSION, 36000, 0),
ROPE → fn_004DCF20 (the clear, [The timed dominant desire](#the-timed-dominant-desire)). Ours:
`LeashSystem::ChangeType` takes the moods as the type changes while the leash is on; packet 0x65 (above) reaches it.

### Audio, status panel and the follow camera

- **`CreatureAudio`** with `src/Audio/Engine/AnimEffectKeys.h`: a creature's sounds sit on moments of its animations
  and are looked up by size (the size it is drawn at, the pen's in its pen), species, ground and action. `VoiceBankStem` gives the species bank's stem (our
  `CreatureBank` builds `audio/sfx/creature/{}.sad`). `SET_CREATURE_SOUND` is 1 after a reset and 0 means only the
  local creature's voice is heard ([audio.md](audio.md)); event modes 1 and 2 map to our Stop and Release (ours: no
  shipped animation uses them). The ground comes from `ecs::sea_cells::GetSurfaceType`, not from his surface rules.
- **`CreatureStatusPanel`**: damage, hunger and tiredness bars and the hand's reward while the hand is over a creature.
- **`CreatureMode`** and **`src/Camera/CreatureFollow`**: Creature Mode's rules (C locks onto the player's creature;
  the double click is our `DOUBLE_CLICK`) and the follow camera, built on our script-camera follow functions
  (`HeadingAndPitchFromPoints`, `PointFromDistanceHeadingAndPitch`, `FollowSeconds`, `ClampFollowDistance`,
  `FollowPitch`, `ThingViewingDistance`): distance 2..1500, least pitch 0.241661, height × 8, pace 2 s easing to 1 s, as
  in [script-camera.md](script-camera.md). Headings run from 0 to 2π.

## The spec file's animation numbers

`Data/ctrspec27.txt`, counted from the first animation after the version line, skipping `=` section lines:

| Number | Animation |
|---|---|
| 0 | stand |
| 16-27 | faces (the first ten pulled when idle) |
| 28-30 | sleep: start, loop, end |
| 31-33 | poo: start, loop, end |
| 34-36 | puke: start, loop, end |
| 37-39 | sit: start, loop, end |
| 52-74 | actions |
| 75 | head turn right / left |
| 76 | head down / up |
| 77, 78 | the same while sitting |
| 96 | eat |
| 106, 107 | faint, get up |
| 200-206 | gestures (202 yawn) |

The morph animation header's fields, from his tree (still `unknown0x0..` in our `MorphFile.h`): duration +0x0,
looping +0x4, stride rate +0x8, stride length +0xC, displacement +0x10..+0x18. `HairHeader`'s +0x0 is the species'
sound-object key.

## Random streams

The game has one random state, described in [engine-math.md](engine-math.md#random-numbers-game_random): the
synchronised GRand (`GameRand` 0x6DE510, `GameFloatRand` 0x6DE530), the local GRand (`LocalRand` 0x6DE570,
`LocalFloatRand` 0x6DE590), the particle systems' choice between the two per effect (`PSysRand` 0x6729E0,
`PSysFloatRand` 0x6729B0) and the C runtime's `rand()` 0x7C8837 with `Random(a, b)` 0x81D180. Our port reaches all of
them through `openblack::game_random` (`src/Common/GameRandom.{h,cpp}`, `Locator::gameRandom`). The turn/draw split is
in [engine-loop.md](engine-loop.md#4-random-streams-between-turn-and-frame): the synchronised stream is the game
logic's; the local, CRT and particle streams are shared by the turn and the frame. `Locator::rng` is only for the
tests' `TestRng`. The rule for the creature (decided 2026-10-07): each draw follows the original's stream; a private
seeded engine only for a draw with no counterpart in the original (visual extras, debug tools); no `random_device`.

Only one creature draw is decompiled:

- **`CreatureDesires::Initialise` 0x4DC100 (M119 0x125EE30)**, bw1-decomp `src/Black/CreatureMentalDesire.cpp:63`: for
  each of the 40 desires, `min + GameFloatRand(max − min)` with min / max at +0x50 / +0x54: **40 draws on the
  synchronised stream**, one per desire, in desire order. Where the drawn number is stored, and what it means, is in
  [The creature tables in info.dat](#the-creature-tables-in-infodat).
- `CreatureDesires::RandomiseIncreaseTime` 0x4DC310 jitters a desire's increase time when the "randomise" cheat flag is
  set; not decompiled, and no normal run reaches it.

Neighbouring facts the creature has to respect:

- **The decompiled eye blink is on the CRT stream.** The high-detail villager's eyes blink every `Random(1000, 5000)` ms
  with a hold of `Random(100, 200)` ms, a glance target of `Random(-0.25, 0.25)` and a squint of `Random(0, 0.3)`
  (0x88358D..0x8836E2; [intro.md](intro.md) "The family in high detail"). The creature's eyes are another object
  (`LH3DCreature`) and their stream is unknown.
- **The mist's frame counter is on the CRT stream.** `LH3DMist`'s constructor 0x7F9560 sets its counter (+0x84) to
  `Random(0, 16) & 15` ([map-loading.md](map-loading.md)); his creature cave's mist uses exactly that formula.
- **The temple's leash draws from the CRT stream when it is created** ([engine-loop.md](engine-loop.md), "Reward and
  TempleLeash creation"): four `Random` per post, Random(0, 1), Random(0, 2π), Random(0, 2π), Random(0, 15), post by
  post, 12 per temple ([The temple's leash posts](#the-temples-leash-posts)). No leash code in his tree draws at all.

In the ported code every one of these draws is passed in by the caller. "His draw" below is the call site in his
systems at `ff1e79d1`, which are not ported yet; "our stream" is what the port must call.

| His draw | Original counterpart | Original stream | Our stream to use | Confidence |
|---|---|---|---|---|
| `CreatureAnimationSystem.cpp:410` eye blink: next open = `random(5000) + 2500` ms (in our `CreatureEyes`: `random(interval) + interval / 2`, interval 5000), one draw per blink, on the frame clock | the creature's eyes are not decompiled; the decompiled blink is the high-detail villager's, `Random(1000, 5000)` ms | CRT for that blink; the creature's unknown | `game_random::crt::Random`, the blink moved onto the turn clock | inferred (approximate) |
| `CreatureMindSystem.cpp:705` `uniform(low, high)` → `creature_desires::Create`, one float per desire (40) | `CreatureDesires::Initialise` 0x4DC100 | synchronised (`GameFloatRand`) | `game_random::GameFloatRange(min, max)` | **checked in bw1-decomp** |
| `CreatureMindLearning.cpp:1117` the same `uniform` in `ClearLearning` | the same function | synchronised | `game_random::GameFloatRange` | **checked in bw1-decomp** |
| `CreatureMindSystem.cpp:702` `random(range)` → `creature_mind::Think`: the idle activity lot, the face, the hang-around direction and distance, the sleep, poo and throw-about places, the need and object-activity choice | not decompiled; every decompiled Living decision draws `GameRand` (`Villager.cpp:1016, 1019`, `VillagerReaction.cpp:180, 259, 326, 805`, `VillagerStates.cpp:737`) | synchronised (turn logic) | `game_random::GameRand(n)` | inferred (strong) |
| `CreatureMindSystem.cpp:914` `ShowFeeling` → `creature_mind::PullFace` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:942` `SitDown` → `creature_mind::SitDown` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1080` `Sleep` → `creature_mind::Sleep` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1117` `Poo` → `creature_mind::Poo` (where it goes) | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:872` `bernoulli(0.5)` in `PlayAction`: whether an action plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureMindLearning.cpp:368` `Random(range)`, used by `creature_plan_actions::Agenda` and `creature_watching::StepMimicry` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindLearning.cpp:373` `Chance()` 0..1, used by `creature_watching::StartMimicry`: drawn at the mimic site, during the turn, only for the player's own creature ([Mimicry](#mimicry-and-the-town-hooks)) | not decompiled (the entry point 0x4EA900 is read, the draw inside `Creature::MimicPlayer` is not) | synchronised (logic) | `GameFloatRand(1.0f)` | inferred |
| `CreatureLocomotionSystem.cpp:667` run-away distance `random(40) + species runAway` | `Creature::RunAwayFromObjectReaction` named, not decompiled; the villagers' flee draws are `GameRand` (`VillagerReaction.cpp:900, 903`) | synchronised (logic) | `GameRand(40)` | inferred |
| `CreatureLocomotionSystem.cpp:844` fidget choice `random(3)` | not decompiled | synchronised (logic) | `GameRand(3)` | inferred |
| `CreatureLocomotionSystem.cpp:857` `bernoulli(0.5)`: whether the confused fidget (the third choice above) plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreaturePhysiologySystem.cpp:324` the poo's yaw, `uniform_real(0, 2π)`, for the `LumpOfPoo` object | not decompiled; objects created by the turn's logic belong to the synchronised stream | synchronised (logic) | `GameFloatRand(2π)` | inferred |
| `CreaturePhysiologySystem.cpp:347-355` the 12 puke drops: two `unit(-1, 1)` and one `green(0.4, 0.9)` each (36 draws) | not decompiled; in the original the puke is not an ECS sprite | unknown | `game_random::psys::FloatRand` inside a `StepScope(Local)` if ported as a particle effect, else a private seeded engine | no counterpart |
| `CreatureFightSystem.cpp:626` `bernoulli(0.5)`: whether the faint, lying and getting up sequence plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureFightSystem.cpp:894` `bernoulli(0.5)`: a taunt, or (his reading of the game) an action past the end of the list that plays nothing, before a fight's Ready stage | not decompiled | synchronised (logic) | `GameRand(2)` | inferred |
| `CreatureFightSystem.cpp:1073` → `fight::ChooseMove`, `fight::ComputerEndsBlock` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureFightSystem.cpp:1421-1431` an unblocked blow's wound on the skin atlas, drawn in this order: u, then v (`:1426`, `:1427`, each `uniform_int(16, 239)` from the texel distribution of `:1424`), then the kind in `fight::WoundKind` (`random(2)`, and `random(3)` more only for a wound of blow type 1), then the column (`random(8)`): 4 or 5 draws per wound, in the order of the designated initialisers | not decompiled | synchronised (kept on the creature) | `GameRand(n)`, the same order and ranges | inferred |
| `CreatureCaveEffects.cpp:202` a cave mist's frame counter, `int(Random(0, 16)) & 0xF` | `LH3DMist` constructor 0x7F9560 | CRT | `game_random::crt::Random(0, 16)` | **checked** |
| `CreatureCaveEffects.cpp:146..314` (about 21 draws) the cave's smoke, spray and mist dome | the creature room is not decompiled (`CreatureRoom::DrawAdditional` 0x788630 is known only for its frame-anim cell, on the real clock) | unknown | a private seeded engine owned by the effect | no counterpart |
| `CreatureSpawner.cpp:936` the debug spawner's facing | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |
| `CreatureSpawnerAppearance.cpp:163, 171` the debug mark place and wound kind | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |

In every row his engine is a private `std::mt19937` seeded from `std::random_device` (his `CreatureAnimationSystem.h:34`,
`CreatureMindSystem.h:87`, `CreatureLocomotionSystem.h:55`, `CreaturePhysiologySystem.h:47`, `CreatureFightSystem.h:93`,
`CreatureSpawner.h:182`), except the cave, which uses `Locator::rng`. None of them survives the port. The state hash
covers the synchronised, local and CRT seeds, so only a private engine is off the references.

## Mimicry and the town hooks

The original reports a player's deed to the creature through `GPlayer::ConsiderMakingCreatureMimicPlayer(status,
action, thing, magic)` at 0x4EA900 (bw1-decomp `src/Black/Player.h:163`; W120 `symbols.txt` line 4883). It is a method
of the **player**: it only concerns that player's creature (player +0xA4C). There are **25 direct call sites** in W120
and no indirect ones (the address never appears in the image as a pointer). Of the 46 deed values
(`DETECTED_PLAYER_ACTION`, bw1-decomp `include/chlasm/CreatureEnum.h:745-805`, the 46 rows of `info.creatureMimic`), 40
can be reported and 6 never are. Unless a row says otherwise, `magic` is 0.

**Evidence.** Every caller was disassembled from the game's `runblack.exe` with capstone (2026-10-08). Its layout matches
the W120 `symbols.txt`, but its SHA-1 is not the one in `build.sha1`. The Mac build was not available for a cross-check.
The four sites ported first (deeds 11, 16, 21, 33) were read again from the same exe before they were ported.

### What openblack does with a deed

- `creature_watching::Deed` (`src/Creature/CreatureDeeds.h`): the 46 deeds in the table's order, `k_DeedCount` = 46,
  pinned to the size of `InfoConstants::creatureMimic`.
- `ecs::creature_mimic` (`src/ECS/CreatureMimic.{h,cpp}`): `Consider(player, deed, object, magic)` publishes
  `events::PlayerDeedForMimic` (`src/ECS/Events/CreatureMimicEvents.h`) with the object's position, read through the
  const registry (the origin for something with no `Transform`). Its handler, registered in `Locator.cpp` with the
  other game handlers, passes the deed to `CreatureMindSystemInterface::PlayerDid(player, deed, point, object)` when
  the mind system is there.
- **`PlayerDid` takes the player, and only creatures whose `Creature::owner` is that player watch** (our change to
  raffclar's, whose `PlayerDid` has no player and walks every creature). This is the original's per-player entry point;
  what the original does with `magic` is not read (Pending).
- The mind's draw is `creature_watching::StartMimicry`'s chance (`GameFloatRand(1.0f)`), drawn synchronously at the
  site, during the turn, as the original draws inside its entry point; it is drawn only for the player's own creature,
  once it passes the conditions checked before the draw.
- **On Land 1** the player's own creature is there, so the deed sites below that run on that land (villagers
  dropped in the sea, water on a player's fields, trees replanted from the hand) reach its mind. The empathy sites
  further down publish an event that has no subscriber yet; nothing is made, moved or drawn by them.

| Deed | Site (original) | openblack | Notes |
|---|---|---|---|
| 11 PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | `HandSystem::Replant` (`HandTrees.cpp`) | after the spot visual, before the tree goes back in its cell; player PLAYER_ONE, as the alignment line beside it ([trees.md](trees.md#dropping-and-replanting)). openblack updates the alignment before the deed, the original after it (Pending in trees.md) |
| 16 DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286 | `Buildings::ReactToPhysicsImpact` (abodes and storage pits; `StoragePit` 0x733730 and `TownCentre` 0x744380 only forward to the Abode's) | before the test of what hit it; with a player and the **building's own body** marked FROM_HAND, which never happens in practice ([physics.md](physics.md)); `creature_mimic::ShouldMimicBuildingHit` |
| 21 THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED | `ecs::HasSunk`, villager branch, through `creature_mimic::ConsiderThrownInTheSea` | after `IsAvailable` (0x750AB5), before the dead test; the player is the last dropper's (`VillagerLastInteraction`, the hand's) |
| 33 CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64 | `water::ApplyWaterSpell`, field branch | after the any-object part (0x528F3A), before the fire test and the sowing; only when the spell has a player; magic always 0x16 (`MagicType::Water`), also for the power-up |

### Corrections to earlier notes

- **Wrong labels in W120 `symbols.txt`.** 0x4EA670 is labelled `DecideOnNewPlan` (line 4882) but is the real
  `Creature::MimicPlayer`: 3 arguments (`ret 0xC`), calls `GetMimickingAction`, called at the end of
  `ConsiderMakingCreatureMimicPlayer`. The Mac symbols put `DecideOnNewPlan` in another TU. 0x4E0DE0 is labelled
  `MimicPlayer` (line 4663) but takes 1 argument (`ret 4`); it is called by `ForceActOnObject` and `fn_005E6BD0` and is
  not a deed site.
- **`Town::UpdateAggressor` is not a deed site.** The code at 0x73CAAA..0x73CB29 calls guidance functions
  (`fn_0071C960`, `fn_0071C9F0`), not the mimic (see [The emergency's creature part](#the-emergencys-creature-part)).
  The claims in our `MapShield.cpp` comment and in [miracles.md](miracles.md) were wrong; the wiki is corrected, the
  code comment is Pending.
- **`Spell::InitWithPos` is not a mimic site.** It has no call to the mimic function. It does make the creature
  empathise (`fn_00721730`, below), which is what our `Spell.cpp` comment says; a `SeeMiracle` call there would be a
  guess.
- **The abode hit tests FROM_HAND on the abode's own body**, not on the hitter's (0x406273 tests the argument's +0x1D8,
  the abode's entry; the hitter is its +0x20). A first reading took it for the hitter's flags. **The port follows the
  exe, not the H1 spec** (H1_SPEC §2.3 M1 had `hit->flags`, which would fire on every hand-thrown rock hitting a house;
  the coordinator's decision of 2026-10-08: follow the exe). The test also comes before the `PhysicallyDestroysAbodes`
  test, so the first guard is split: the valid test, then the mimic, then the destroy test (pure code between them).
- **Fidelity risk at `StoragePit` 0x733844.** When food is put into another player's pit, the original draws
  `GameRand(2)` (bw1-decomp `StoragePit.cpp` line 688) **before any creature check**, so the draw happens even when the
  player has no creature. openblack already draws it (`StoragePitStore::DoCreatureMimicAfterAddingResource`).
- **Disciple off-by-one in `Villager::Landed`.** The deed is disciple type + 21. `VILLAGER_DISCIPLE` has TRADER = 9,
  which has no deed of its own, so the last values shift by one: TRADER gives 30 (CHANGE_HOUSE), CHANGE_HOUSE gives 31
  (WORSHIP), WORSHIP gives 32 (TAKE_OBJECT_HOME), FROM_VORTEX gives 33, NONE gives 21 (THROW_IN_THE_SEA).
  [villagers.md](villagers.md) already states this correctly.
- **The animals' sinking tests availability first.** `Living::HasSunk` 0x5ED370 (the animals') returns 0 when the
  animal is not available (vt +0x2C, 0x5ED375), then reports deed 21 for a last dropper (0x5ED3A9, with no null test of
  the player), then sets it dying and deletes it. openblack's animal branch of `HasSunk` has no availability test
  (Pending).

### Gate in `ConsiderMakingCreatureMimicPlayer` (0x4EA900)

In order:

1. The player has a creature (player +0xA4C) without flag +0x24 & 0x10.
2. The creature's player +0x8E0 is not 2, and creature +0x1268 is at least 3.
3. Leash and table check: per-action table at 0xCAB220, 0xC0 bytes per action, flag at +4.
4. The current reaction's +0x10 is below 0x96.
5. `CanSeePos(thing)`, and the distance is within `fn_004EFC70(+0x160)`.
6. The action's priority (table +0) beats the mimic already running.
7. Then it calls `Creature::MimicPlayer` 0x4EA670.

openblack's `StartMimicry` checks the phase (at least 3), the learning leash, the reaction priority (below 150; the
port passes 0), the sight (within 150) and the chance, in that order; steps 1, 2 and 6 are not all modelled.

### Spell hits: `fn_004E9DF0(thing, magic, player)`

Called from `Spell::ApplyDefaultSpellEffect` at 0x720E8A, only when something is hit, the spell has a player and it is
not a creature cast; the report itself is at 0x720EB7. Returns 46 for "no deed".

- First: if the spell's desire is not ANGER and the nearest town within 100 m belongs to someone else, the deed is 20.
  The desire is `GMagicEffectInfo[magic]` +0x98 (`perceivedPlayerDesire[0]`; table at 0xCC6630, 0x11C bytes per entry).
- Fireball 1-3: 17.
- Lightning and explosion 4-8: 18. 9 (EXPLOSION_ONE_PU_TWO), 12, 13 and 16-20: nothing.
- Heal 10-11: 42.
- Food 14-15: 1 on a worship site, 3 on a storage pit, otherwise nothing.
- Wood 21: 5 on a storage pit, 8 on a building being built.
- Water 22-23: 33 on a field, 34 on something on fire, and **33 on anything else**.
- Flock 24-25: always 20.

### Landing: `ConsiderCreatureMimickingWhenObjectLands` (0x4EAAB0)

Called from `Object::InitialisePhysicsFromHand` (0x637306) and `Object::EndPhysics` (0x63764A). Status = the thing's last
dropper (`GetInterfaceStatusWhoLastDroppedMe`); player = that status's player.

- Within 80 m of the player's citadel (player +0xA48): 36 if the thing's town (`GetTown`) belongs to another player,
  otherwise 32.
- Otherwise: 35 if the thing's town belongs to another player and the nearest town is the dropper's.

### Deed table (complete for W120)

The "openblack" column says what is **ported**; otherwise it gives the site where the report would go, which waits for
the follow-up hooks commit (Pending).

| Deed | Name | Reporting function, call address | Trigger (status / player / thing) | openblack | Conf. |
|---|---|---|---|---|---|
| 0 | PUT_FOOD_IN_WORSHIP_SITE | `WorshipSite::DoCreatureMimicAfterAddingResource` 0x77DF19 | Food added to a worship site; the `MultiMapFixed` test runs first. Dropper's status and player; thing = the site | `ECS/ObjectDelivery.cpp`, `ECS/PotResource.cpp` | High |
| 1 | CAST_MAGIC_FOOD_IN_WORSHIP_SITE | `Spell::ApplyDefaultSpellEffect` 0x720EB7 via `fn_004E9DF0` | Food miracle hits a worship site. Spell's status and player; thing = what was hit; magic = the spell's | `Magic/Core/SpellEvent.cpp` | High |
| 2 | PUT_FOOD_IN_STORAGE_PIT | `StoragePit::DoCreatureMimicAfterAddingResource` 0x733870 | Food put in a pit, status +0x128 is the dropper's own player | ObjectDelivery / PotResource (the deed is computed, not reported) | High |
| 3 | CAST_MAGIC_FOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Food miracle hits a pit, or a workshop (`Workshop` overrides `IsStoragePit`) | SpellEvent.cpp | High |
| 4 | PUT_WOOD_IN_STORAGE_PIT | StoragePit 0x7338B6 | Non-food resource in a pit, own player (after the building-site test) | ObjectDelivery / PotResource (computed) | High |
| 5 | CAST_MAGIC_WOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a pit or a workshop | SpellEvent.cpp | High |
| 6 | BUILD_HOUSE | `Scaffold::EndPhysics` 0x6E87A9 | A thrown scaffold stops. Status = physics object +0x24; player = the **scaffold's** owner; thing = the scaffold | `ECS/Scaffolds.cpp` | High |
| 7 | PUT_WOOD_IN_BUILDING_SITE | `MultiMapFixed::DoCreatureMimicAfterAddingResource` 0x52F239 | Wood added to a building being built; returns true and stops the subclass tests | ObjectDelivery / PotResource (computed for a pit) | High |
| 8 | CAST_MAGIC_WOOD_BY_BUILDING_SITE | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a building being built (not a pit) | SpellEvent.cpp | High |
| 9 | PUT_WOOD_IN_WORKSHOP | `Workshop::DoCreatureMimicAfterAddingResource` 0x77A6AB | Wood added to a workshop (after the building-site test) | ObjectDelivery / PotResource | High |
| 10 | CAST_MAGIC_WOOD_BY_WORKSHOP | **never reported** | Wood cast on a workshop gives 5 instead | none | High |
| 11 | PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | Replanted tree, after `Forest::AddTree`. Player = `PhysicsObject::GetPlayer`; status = +0x24; thing = the tree | **ported**: `HandTrees.cpp` `Replant` | High |
| 12 | GIVE_TOWN_PROTECTION_WITH_SHIELD | **never reported** | Shield 19-20 gives nothing in `fn_004E9DF0` | none | High |
| 13 | BRING_PEOPLE_TO_WORSHIP | **never reported** | — | none | High |
| 14 | MAKE_ARTEFACT | `Town::AddArtifact` (Mac name; W120 `fn_0073FDA0`) 0x73FE49; `WorshipSite::AddArtifact` 0x77DBA1 | A thrown object suitable as an artifact lands within 50 m and joins a town or worship site (`Fixed::EndPhysics` 0x52E047 / 0x52E0BE). Player = the thrower; status = last dropper; not for trees | none: artifacts not ported (`ECS/Town/TownProcess.cpp`) | High (Mac name: Med) |
| 15 | DAMAGE_BY_THROWING | `PhysicsObject::GameTurnUpdate` 0x6463BE | After the impact sound and `ReactToPhysicsImpact`: a body from the hand (+0x1D8 & 4) with a status, a living thing or rock, not a tree. Status = +0x24; thing = the body | `ECS/Physics/PhysicsObjects.cpp`, after `ReactToPhysicsImpact` | High |
| 16 | DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286; `GameTurnUpdate` 0x64643A | Abode hit, with the abode's own body from the hand; thing = the abode. Physics: the same body test when it hit something (+0x20); thing = the thrown body | **ported** for the abode (`Buildings.cpp`); the physics one: PhysicsObjects.cpp | High |
| 17 | DAMAGE_WITH_FIRE | 0x720EB7 via `fn_004E9DF0` | Fireball 1-3 hits something | SpellEvent.cpp | High |
| 18 | DAMAGE_WITH_MAGIC | same | Lightning or explosion 4-8 hits something | SpellEvent.cpp | High |
| 19 | IMPRESS_BY_THROWING | **never reported** | — | none | High |
| 20 | IMPRESS_WITH_MAGIC | same | The desire/town rule, or any flock 24-25 | SpellEvent.cpp | High |
| 21 | THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED; `Living::HasSunk` 0x5ED3A9 | Sinks while available (`IsAvailable` first). Status = last dropper; thing = itself. `Living::HasSunk` does not null-check the player. Also NONE disciple + 21 | **ported** for the villager (`VillagerDrowning.cpp`); the animal's: no last dropper is kept for animals | High |
| 22-29 | MAKE_DISCIPLE_FARMER … CRAFTSMAN | `Villager::Landed` 0x760744 | First turn landed (+0x90 == 1, flags +0xE0 & 0x20 and & 0x200). Status = last dropper, needs a player. Deed = disciple (+0xF2) + 21 if below 46 | `LivingActionSystem.cpp` (`VillagerLanded`); disciples not ported | High (code) |
| 30 | MAKE_DISCIPLE_CHANGE_HOUSE | same | Reached by TRADER (9) + 21 | same | Med |
| 31 | MAKE_DISCIPLE_WORSHIP | same | Reached by CHANGE_HOUSE (10) + 21 | same | Med |
| 32 | TAKE_OBJECT_HOME | `ConsiderCreatureMimickingWhenObjectLands` 0x4EAB49 (+ disciple WORSHIP) | Dropped within 80 m of own citadel; thing's town is not another player's | `ECS/Physics/FromHand.cpp`, `ECS/LivingPhysics.cpp` | High |
| 33 | CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64; also 0x720EB7 via `fn_004E9DF0` | Field: spell has a player; magic always 0x16; thing = the field. `fn_004E9DF0`: water hits a field **or anything not on fire** | **ported** for the field (`SpellWater.cpp`); the spell hit: SpellEvent.cpp | High |
| 34 | CAST_WATER_TO_PUT_OUT_FIRE | 0x720EB7 via `fn_004E9DF0` | Water hits something on fire (not a field) | SpellEvent.cpp | High |
| 35 | STEAL_OBJECT_AND_PUT_IN_TOWN | ObjectLands 0x4EAB8E | Not near own citadel; thing's town is another player's; nearest town is the dropper's | FromHand.cpp / LivingPhysics.cpp | High |
| 36 | STEAL_OBJECT_AND_PUT_BY_CITADEL | ObjectLands 0x4EAB34 | Within 80 m of own citadel; thing's town is another player's | same | High |
| 37 | BREAK_ROCKS | `GInterface` tap handler 0x5DA650 (unnamed), call 0x5DA6C8 | Tapping a rock that is valid to tap. Status = interface +0x39C; thing = the rock | `ECS/Systems/Implementations/HandTurn.cpp` | High |
| 38 | THROW_FOOTBALL_IN_GOAL | **never reported** | — | none | High |
| 39 | CATCH_FOOTBALL | **never reported** | — | none | High |
| 40 | SACRIFICE | `Animal::ApplyThisToObject` 0x41B3DF; `Villager::ApplyThisToObject` 0x752D10 | Animal or villager applied to a `WorshipTotem`. Status = the applying hand's; thing = the victim; followed by the alignment update | `HandApplyToObject.cpp` (sacrifice not ported) | High |
| 41 | PLAY_WITH_TOY | `Object::InitialisePhysicsFromHand` 0x637457 | Status, `IsToy`, and no thrower or the thrower is not a creature (bw1-decomp `Object.cpp:603-606`) | `ECS/Physics/FromHand.cpp` (no `IsToy` yet) | High |
| 42 | HEAL | 0x720EB7 via `fn_004E9DF0` | Heal 10-11 hits something | SpellEvent.cpp | High |
| 43 / 44 | STEAL_FOOD_FROM_FARM / _FROM_STORAGE_PIT | StoragePit 0x7338B6 (shared tail; deed pushed at 0x733861) | Food put in a pit and status +0x128 is not the dropper. Deed = 43 + (`GameRand(2)` != 0) | ObjectDelivery / PotResource (computed, the draw made) | High |
| 45 | STEAL_WOOD_FROM_STORAGE_PIT | StoragePit 0x73389A | Non-food in a pit, status +0x128 is not the dropper | same (computed) | High |

### Town and villager hooks

All of these are empty stubs in bw1-decomp (`TownAttitudeToCreature.cpp`, `Town.cpp`, `Player.cpp`,
`CreatureDirectControl.cpp`, `TownDesire.cpp`, `Spell.cpp` are 0-35 lines; `TownCreatureInfo.h` lists only the
destructor and `GetBaseInfo`). Everything below was read with capstone from the shipped `runblack.exe` using the W120
symbols. The exe matches W120: 0x4C80F0 has the expected bytes, and the Creature vtable at 0x8CC810 resolves to the
right symbols.

#### Town step 16: `Town::UpdateAttitudeToCreature` 0x7437F0 (size 0x210)

Not ported (`TownProcess.cpp`, step 16); the numbers below make it portable (Pending).

- **Call site:** `Town::Process` 0x747380 calls it at 0x74744B, right after `ProcessTownEmergency` at 0x747444
  ([villagers.md](villagers.md)). Declared at `Town.h:353`; TU `Black/TownAttitudeToCreature.cpp` 0x743690-0x743A30
  (`splits.txt:2255`).
- **Data:** Town +0x970 is a singly linked list of `{next, TownCreatureInfo*}` nodes; Town +0x974 is its count. Each
  record: +0x10 the Creature, +0x14 the attitude, +0x18 the turn the attitude was set, +0x1C the turn it was reset.
- **Creation:** `Town::CreateCreatureInfo` 0x743720 on first contact (attitude 0, both stamps = turn), called from
  `Villager::SetupReactToCreature` at 0x767673.
- **Reader:** `Town::GetTownAttitudeToCreature(Creature*)` 0x7436F0 returns +0x14, or 0 if there is no record. Read by
  the villager reactions (0x76767B, 0x767779, 0x76849E), `WatchFightAnimation`, `Living::NumGameTurns…ToCreature`
  (0x5F1BBA, 0x5F1CCE) and `Creature::GetImpressiveValue` (0x47B244).
- **Per record, each turn:**
  1. If the creature's GameThing flags byte +0xA has bit 0 set, every node for it is unlinked and freed and the info is
     deleted (vtable +4, argument 1).
  2. Gate: `GetNearestTown(Town+0x14, 10.0)` 0x6020E0 must return this town (literal, and looks always true since it is
     the town's own position) and `CreaturePlan::IsValid` (mind +0xF48) 0x4F12E0 must be true.
  3. The new attitude comes from a member function in the creature-action info table: index = Creature +0x1120,
     entries 0x50 bytes, function at 0x9D16B8 + idx·0x50, called on `[[mind+0xF58]+0x30]`. The table is filled at run
     time by `crt_xc_fn_CreatureAction_00491B90`. Entries seen: 0x768540 None → 1; 0x768550 Fear → 3 (15 actions, e.g.
     idx 15, 18, 45-47, 95-105, 185, 186, 197); 0x768560 Respect → 4; `GameThingWithPos::…Eating` → 1;
     `Living::AttitudeToCreatureEating` 0x768580 → `vt+0x2C8(0) ? 3 : 1`.
  4. Fear (3) is forced when mind +0xF50 == 4, `[mind+0xF58]` vt+0x2C returns 6, and
     `[Creature+0x160]+0x28 == [mind+0xF58]+0x30`.
  5. A result of 1 does nothing. If the attitude changed, or it is 3, it is stored with +0x18 = turn. Only when it
     changed is `fn_004C9FE0(creature, 0x21 = CREATURE_HELP_TYPE_SHOW_TOWN_ATTITUDE_TO_CREATURE, att, 0, 0, 0)` called.
  6. **Timeout:** while attitude != 1, `secs = (turn − info+0x18) / (1000 / [0xD01A38])` (unsigned integer). Durations
     {0: 15.0, 2: 10.0, 3: 30.0, 4: 10.0}, index ≥ 5 → 0. When `secs > dur`, attitude becomes 1 and +0x1C = turn.
- **Attitude values:** 1 = none, 3 = fear, 4 = respect; 0 and 2 unnamed. Five help texts
  (`HELP_TEXT_TOWN_ATTITUDE_TO_CREATURE_01..05`, 1083-1087) fit values 0-4. No writer of 2 was found.

#### The emergency's creature part

- `Town::ProcessTownEmergency` 0x7477A0, `CallAllVillagersToTownEmergency` 0x747890 and `SetInStateOfEmergency`
  0x7479A0 have **no creature code** (no +0xA4C read, no call that reaches a creature).
- The "creature part" our `TownEmergency.cpp` refers to is in **`Town::UpdateAggressor` 0x73C9B0, at
  0x73CAA3-0x73CB2D** ([buildings.md](buildings.md); `Town.h:247`), after the slot update `fn_0073E0F0`:
  - `obj = EffectValues+0x28`. If it exists and `IsCreature()` (vt+0x34): `c = CastCreature()` (vt+0xA4),
    `p = c->GetPlayer()`. If `p` exists, `c` is not the town player's creature (+0xA4C) and
    `p->IsMemberOfThisPlayer(MyInterfaceStatus)`, it calls `fn_0071C960(guidance, c, effect)`: if `[0xC221CC]` is set
    and `HelpSpritesPlayNow(0xB)`, then `HelpSpiritSay(GetRandomSample(0xD99D08), 0xB)`.
  - Otherwise it calls `fn_0071C9F0(guidance, town, effect)` (sample group 0xD) when the caused player is local.
- **Help-spirit audio only, no game state.** It changes nothing in the verification checks.

#### The satisfy activity and `GetVillagerActivityDesire`

- `Town::SetVillagerActivity` 0x73FF10 (`Town.h:169`): best = the football's vt+0x4C (Town +0xEA4); then tries the
  town player's creature (+0xA4C) and the artifacts (+0x994, next +0x20), each with a strict `>`. If best == 0 it
  returns 0, otherwise it calls the winner's vt+0x50 (matches [villagers.md](villagers.md)).
- Creature vtable `??_7Creature@@6B@` 0x8CC810: +0x4C = **0x401870 `GameThing::GetVillagerActivityDesire`**
  (`fld 0.0; ret 4`), +0x50 = `GameThing::SetVillagerActivity` 0x401880, +0x54 = the GameThing default.
- None of the four 0xB44-sized vtables (Creature, Living, SpecialVillager, Villager) overrides +0x4C, and only
  Creature overrides `CastCreature`, so there is no subclass. The only overrides are Football 0x532220, TownArtifact
  0x4262D0 and Town 0x73FF00. `Creature.h` declares none.
- **The creature's part is always 0 and never wins.** openblack's `CheckSatisfyRelaxation` already gives 0; only the
  comment in `VillagerSatisfy.cpp` needs changing (Pending).

#### `GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire` 0x4C80F0

0x3A bytes; `Player.h:183-187`; TU `CreatureDirectControl.cpp` 0x4C5F80-0x4C8440.

- `c = player+0xA4C`. If `c` exists and `Creature::CanSeePos(pos)` 0x477440, it calls
  `fn_004C8050(&mind[c+0x164]+0x18C80, desire, weight)`, which checks `-1 < d < 17` and sets
  `a[0xA0/4 + d] = clamp(a[...] + weight, 0, 1)`. The sister `MakeCreatureEmpathiseWithPlayer` 0x4C80B0 does the same
  through `fn_004C8000` on `a[0..39]` (CREATURE_DESIRES). Neither tests whether the creature is available or alive.
- **The add** (read 2026-10-09, 0x4C8000 / 0x4C8050): `v = w + v` stored, then `v < 0` **or unordered** (C0 of `fcom`
  with 0) → 0, else `v > 1` → 1. The player guard is `cmp d, 40; jge` only (signed): a negative `d` writes before the
  array. The town guard is `d > -1` and `d < 17`.
- **`Creature::CanSeePos(pos)` 0x477440** (read 2026-10-09): `look = ConvertScawenAngleToGameAngle(GetLookDirection())`
  (the float stored first, 0x477454); `to = GetAngleFromXZ(creature+0x14, pos)`; both `& 0xFFFF`, `d = |look − to|`
  (cdq/xor/sub), `d > 0x400` (unsigned) → `0x800 − d`; seen when `d <= 0x2AA`, or else when the high words of x and z
  of `pos` equal the creature's (+0x16, +0x1A: the same map cell).
  `LH3DCreature::GetLookDirection` 0x480C90: when the look target +0x48C0..+0x48C8 is all 0 (`fn_00482D40`), or equal
  to the 3D position +0x78..+0x80 in all three, the body's yaw +0x84; otherwise `fn_007FAA50` (GetYAngle, see
  [engine-math.md](engine-math.md)) of target − position, each a float subtraction.
- **`GetDominantDesire` 0x4C7ED0** (read 2026-10-09): `result = 40`; for d = 0..39, `CreatureDesires::IsActivated(d)`
  and `v > 0` (not unordered) → `v = 0`, `result = d`. The last one wins and every one found is zeroed.
  `IsActivated` 0x4DCDA0 is true when the desire's own flag is set (mind desires +0x164, `[this + d*4 + 8]`) **or** when
  bit 0x2000 of the game flags (g_game+0x14) is set (0x4DCDAC..0x4DCDB8). The only write of that bit is 0x6433B1, at
  start-up in PCMain: it is set when the registry value `GatheringFlag` (under `Software\Lionhead Studios Ltd\Black &
  White...`, 0x643380) is read and non-zero. openblack reads only the desire's flag (see Pending).
- **The personality scroll** (`CreatureRoom::MakePersonalityScrollText` 0x788D20, read 2026-10-09), after the attention
  line (text 0x4AA): `GetDominantDesire` (0x788F23); when it returns less than 40, `TempleRoom::AddText(0x4AB)`. That
  text's case in `AddText` (jump-table entry 0x2A, 0x79B61B) calls `GetDominantDesire` **again** (0x79B651) and writes
  its line (DB texts 0x4AB with 0x248) only when that one is also below 40; otherwise it jumps to the common tail
  0x79C3DC, which is one `AddNewLine`. The first call has just zeroed every value it found, so the second always returns
  40: the line's text is never written, and a perceived desire only adds one line end. Then, whatever was found, one
  `AddNewLine` (0x788F41), the loop over the creatures it knows (texts 0x4AC..0x4AE), `AddNewLine` (0x788FB3) and the
  four beliefs (0x4B0..0x4B3). The function is called with 0 or 1 (on the texture, or the text in front) from
  `CreatureRoom::Draw` each frame while it is the focused scroll (0x787052, and 0x78706E once the zoom is back at 0.5
  or under), when the focus leaves it (0x786FF6), from `InitEngine` (0x7885C4) and from `fn_00789730` (0x78979A, next
  to `CreatureScrollLikesSet`; not read). Each call is a destructive read.
- **The ring push** (0x4C7F60): the turn test first; then per value, player 0..39 then town 0..16: `ring[index] = v`,
  `index + 1`, back to 0 when it reaches 30 (equality only), `count + 1` while `count < 30` (unsigned). The fade
  (0x4C7F20) runs before the push in the same call.
- **The spell's site** (`fn_00721730`, 0x71FE5D, first thing in `Spell::InitWithPos`): for +0x98 and +0x9C of the
  effect info, each when `< 40` (signed), `GetPlayer` (vt +0x1C) → `MakeCreatureEmpathiseWithPlayer(d, 1.0, pos)`; then
  +0xA0 when `< 40` → `...TownDesire(t, 1.0, pos)`; `pos` is the cast point. No null test of the player.
- **The feedback's site** (0x4E10AE..0x4E10D5): `f > 0` → 1 COMPASSION, else (0 or unordered too) 2 ANGER, weight
  `fabs(f)`.
- **The object** at mind +0x18C80 is a `CreaturePerceivedPlayerDesires`, at +0x40 inside `CreatureAttitudeToPlayer`
  (mind +0x18C40; ctor `fn_004C8130`, called from the `CreatureMental` ctor at 0x4D23BE;
  `CreatureAttitudeToPlayer.h:21-28`). Layout: +0x00 float[40]; +0xA0 float[17]; +0xE4 57 histories of 0x80 bytes
  (30-float ring, index at +0x78, count at +0x7C). The header's 0x20-byte history struct is a placeholder: the 0x80
  stride totals 0x1D64 = 0x1DA4 − 0x40.
- **Each turn:** `Creature::ProcessState` → `fn_00477A00` (0x477A88) → `fn_004C7EB0`: `fn_004C7F20` multiplies all 57
  values by **0.9995** (float at 0x8CF3D4); `fn_004C7F60` pushes them into the rings only on even turns (skips when
  `g_game+0x205A40 & 1`). `fn_004C7EB0`, `fn_00477A00`, `fn_004C7F20` and `fn_004C7F60` have no other caller.
- **The gate** (read 2026-10-09): the call at 0x47338C sits in the block 0x473306..0x47338C, run only when the dword
  at 0xD00DE8 is not 0 (0x4732FA `cmp [0xD00DE8], ebx` with ebx = 0 since 0x472E23; `je 0x473391`). The same dword
  gates `Creature::AddToCheckSum` (0x473B43). 0xD00DE8 is past the end of `.data`'s raw bytes (0xC3D000), so it is 0 at
  load, and **no writer is found**: the whole image holds its address only at the two reads (and as bytes of a call's
  rel32 at 0x78C1AE); nothing in `.text` writes it, no pointer to it is stored in the data, and no indexed or based
  access reaches 0xD00C00..0xD00DE8. Its neighbours 0xD00DE0 / 0xD00DE4 / 0xD00DD8 are read-only the same way
  (0xD00DE4 gates `Update3DCreatureFromAttributes` at 0x4737B5), as is 0xD00DD4 ([miracles.md](miracles.md)).
  The same block also holds `fn_00477130`, the creature's map position following its 3D body (`MoveMapObject` of
  the body's +0x78 position, its only caller), `fn_004EF510` (strength, `ModifyStrength`, its only caller), and the
  calls into mind +0x1AA80 (`fn_004DF620`) and `fn_004D82A0`. A creature whose map position never followed its body
  could not be found where it walks, which the game plainly does; so the flag is read as set at run time, written in a
  way the static scan does not see, and the fade as running every creature turn. **Inferred**, not confirmed by a run
  (Pending).
- **Readers:** the town-desire array (+0xA0) is only saved and loaded (`fn_004E79D0` at 0x4E7DA9 from `SaveMind`;
  `fn_004E84A0` at 0x4E8914, version ≥ 0x10). No gameplay reader of it or its histories was found. The creature-desire
  array is read by `CreaturePerceivedPlayerDesires::GetDominantDesire` 0x4C7ED0 (from
  `CreatureRoom::MakePersonalityScrollText` 0x788F23 and `TempleRoom::AddText` 0x79B651), which returns the last
  activated desire with value > 0 and zeroes those values. Feedback also adds to it
  (`UpdateAttitudeToPlayerFromFeedback` 0x4E10D5, index 1 or 2).
- **The callers of 0x4C80F0**, and what openblack does at each:

  | Caller | Call | Desire, weight | openblack |
  |---|---|---|---|
  | `CheckInteractWithWorshipSite` | 0x7572FD, 0x75738B | 9, 0.5; 7, 0.5 | state 171 not ported; not in bw1-decomp's source |
  | `CheckInteractWithAbode` | 0x7574DF | 5 FOR_ABODES, 0.5 (no player test) | state 172 not ported (disciples) |
  | `CheckInteractWithField` | 0x7575F6 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithFishFarm` | 0x757674 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithTree` | 0x7576E8 | 1 FOR_WOOD, 0.5, with a player | state 175 not ported |
  | `TakeWoodFromTree` | 0x75FBF1 | 1 FOR_WOOD, 0.5, with a player | **published** (`VillagerForester.cpp`) |
  | `fn_00721730` from `Spell::InitWithPos` | 0x71FE5D | `GMagicEffectInfo` +0xA0, 1.0, plus +0x98 / +0x9C into 0x4C80B0 | **ported** (`Magic/Core/Spell.cpp` `InitWithPos`) |

  Indexes 0..16 are `TOWN_DESIRE_INFO_*` (`include/chlasm/Enum.h:1481-1500`, our `TownDesireInfo`). bw1-decomp's source
  shows only five calls (`VillagerCheck.cpp:103,136,152,166`, `VillagerForester.cpp:164`); the two worship-site calls
  and the spell call are from the exe.
- **openblack:** each published site calls `creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), desire,
  0.5, villager)` on its success path only. It publishes `events::CreatureEmpathyWithTownDesire` (player, desire,
  weight, the villager's position) when there is a player. Since R07a (2026-10-09) the mind side is in:
  - `Creature/PerceivedDesires.{h,cpp}` (pure): the 57 values, `Increase` / `IncreaseTown` (the add above), `Fade`,
    `TakeDominant` (`GetDominantDesire`, none for 40), `CanSeePos` and `LookYaw` (`GetLookDirection`). The field is
    `CreatureMindState::perceivedDesires`.
  - The mind turn calls `Fade` right after `++mind.turn`. The 57 rings are not ported: nothing but the mind save reads
    them, and they come with the save (R09).
  - `CreatureMindSystem::EmpathiseWithPlayer` / `EmpathiseWithTownDesire` (the two `GPlayer` functions): the player's
    creature through `LeashSystemInterface::PlayersCreature`, then `CanSeePos` and the add. The look: the head's
    `CreatureAnimation::lookAt` as the 3D look target; the body's yaw is minus our locomotion heading (a heading `h`
    faces `(−sin h, −cos h)`, a body yaw `y` faces `(sin y, −cos y)`), read from the body's rotation before the
    creature first moves. The creature's map position is `FromMetres` of its Transform.
  - The town-desire event is passed to the minds by `creature_mimic::AddMimicEventHandlers`; the spell site in
    `InitWithPos` (only with a player); `ReceiveFeedback` adds 1 or 2 after the slight-feedback test.
  - The reader (R07c): `TempleScrolls::Write` of the mind scroll calls `CreatureFacts::takeGodsDesire` each time it
    writes it, as the game does: our builds are the room's set-up, each turn of the held scroll, the focus changes and
    each frame of the text in front (`TempleScrolls::Create`, `Hold`, `SetFocus`, `AppendFocusedText`), and openblack's
    ImGui cave screen without a temple each frame its page is shown. `CreatureCaveSystem::TakeGodsDesire` is
    `TakeDominant` over the activated desires of the mind. A desire found writes one line end, and the line end at
    0x788F41 is written always (before R07c it was only written with the desire's line, which was never set).
  - The values are in no state-hash part and draw nothing; the scroll is only in the temple, which no check enters.

### Reactions to miracles

A creature is a Living: `Reaction::ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 gives it every reaction spread over
its cell, as it does the villagers and the animals (see [animals.md](animals.md#reactions)). Read 2026-10-09 in the
exe (W120); the table entries are from the CRT initialiser 0x6E0E80, the vcall thunks resolved through the `Creature`
vtable.

- **The spread's body for a creature** (0x6E3FF3..0x6E42E7): `IsAvailableForReaction` (vt +0x984), the shield test
  (`fn_0072B990`), the half-Manhattan distance, the type's turns before reacting again (table +0x40), then
  `IsAvailableForBeliefButNotReaction` (vt +0x988, `Living`'s 0x417090: always 0 for a creature). With no reaction held
  (+0x94): the score (`fn_006E4620`: `isReacting[type]` of the info at +0x28, the species' `CreatureInfo` row, then the
  type's priority within `maxReactionDistance`) > 0 and the records (`fn_006E4340`) → `turnCreated` stamped,
  `StartReacting` (vt +0x994). With one held: the switch rule (the same type only with the table's +0x04 flag and
  another reaction; the new score above the held one's at the distance to the held reaction's cell; the held one lasted
  `max(10, cur / new × 20 − 10)` s, or 1 s for 16), then `SetReactionDoneWhen(new type)`, the creature off the old
  reaction's followers (no record refreshed, +0x94 not cleared), and `StartReacting`.
- **The type table** (0xC09CC0 + 0xC0 × t; +0x10 setup, +0x20 priority, +0x30 turns to react, +0x40 before again, each a
  vcall thunk):

  | Type | Setup | Priority | +0x30 / +0x40 |
  |---|---|---|---|
  | 3 FLEE_FROM_SPELL | `Creature::SetupFleeFromObject` 0x4F2B00 (vt +0x9A4) | `Creature::FleeFromSpellPriority` 0x4F2AB0 (vt +0xA38) | the creature's standard (vt +0xAC0 / +0xAC4) |
  | 13 REACT_TO_MAGIC_SHIELD | `Creature::SetupReactToMagicShield` 0x4F3090 (vt +0x9D4) | `Living::ReactToMagicShieldPriority` 0x5F1860: info[13].priority | `Living` 0x5F1AD0 / 0x5F1AF0 (vt +0xAE4 / +0xAE8), which call the creature's standard |
  | 21 LOOK_AT_NICE_SPELL | `Creature::SetupLookAtNiceSpell` 0x4F2B40 (vt +0x9B0) | `Living::LookAtNiceSpellPriority` 0x5F16A0 (vt +0xA40) | the creature's standard; field +0x00 of this row is 1 |
  | 37 REACT_TO_IMPRESSIVE_SPELL | the same as 21 | the same as 21 | the creature's standard |

  An earlier study read type 21 as having no +0x30 / +0x40 entry: it has them, the same as 3 and 37.
- **Priorities.** `Creature::FleeFromSpellPriority`: 0 if the initiator is a `Spell` whose creator (+0xA0) is this
  creature, else `Living::FleeFromSpellPriority` 0x5F1630: `d = FastDistance(creature, initiator)` (map units);
  `d >= 600000` → info[3].priority; else `priority + (600000 − d) × 100 / 600000` (signed integer division), returned
  as a byte. `Living::LookAtNiceSpellPriority`: 0 for a spell this creature cast, else **info[21].priority**
  (0xD4FEF4), for 37 too.
- **Turns.** `Creature::StandardNumGameTurnsToReactFunction` 0x4F29C0: `ftol(((max − d) × imp / max + 1 − imp) ×
  numGameTurnsForCreatureToReact)`; `...BeforeReactingAgain` 0x4F2A20: `ftol((imp × d / max + 1 − imp) ×
  numGameTurnsForCreatureBeforeReactingAgain)` (max = maxReactionDistance, imp = howImportantIsDistance).
- **Availability** `Creature::IsAvailableForReaction` 0x4F2820, all of: `IsFunctional` (`Living`'s, from
  `IsAvailable`); +0x1060; not in the dance editor; Flags +0x24 without 0x80 and 0x400; mind +0x1C14 clear or +0x380
  set; not `IsFighting`; mind +0xF60 ≠ 21 and its friend's (mind +0x1BE8) ≠ 21; +0x384 (fainted), +0x10AC, +0x10B0 all
  0; Flags without 0x10; while mimicking (mind +0x1C38) only a type whose priority is above 150; mind +0x1C10 and
  +0x10F4 clear. What is known of the fields:
  - +0x1060 is "reactions on": `Creature::Initialise` sets it to 1 (0x4743FA); `GScript::CreatureReaction` 0x6F5600
    (CREATURE_REACTION: the thing popped first, then the value) writes it; the creatures made by the script's create
    path (the CREATE native's creature case, 0x6F14BE, with ebp, cleared at 0x6F11B0, both as the player and as the
    value) and by `CREATURE_CREATE_RELATIVE_TO_CREATURE` (`fn_004F6610`, 0x4F666B) start with 0; `SwapMinds` sets it
    to 1 on one of the two creatures (0x47C3BC) and 0 on the other (0x47C3CA). The challenge script never calls
    CREATURE_REACTION.
    A scan of the exe for the displacement finds no other access: written at 0x4743FA, 0x47C3BC, 0x47C3CA, 0x4F666B,
    0x6F14BE and 0x6F5673, read only at 0x4F2833 (re-read 2026-10-09). The Land 1 creature is made by
    `LOAD_MY_CREATURE` (`Creature::Load` 0x4E7FF0 → `LoadFullyQualified` → `CreateCreature` 0x474B50 → `Create`
    0x474A20 → the constructor 0x473B90, which calls `Initialise` at 0x473D9A), so its reactions are on; a saved
    game's `Creature::Load` (0x4E631A) calls `Initialise` too.
  - The test list (0x4F2820..0x4F2924, re-read 2026-10-09) does not read the mind's "active" flag +0x20CE4 that the
    freeze spell clears: a stilled creature takes a reaction up, and only its learning (`ConsiderLearningAction`
    returns at once with the mind not active) is stopped.
  - +0x10B0 is set when its life runs out, before it faints (`ReduceLife` 0x47DDE2, `ApplyEffect` 0x478EDB..0x478F00):
    knocked out. Mind +0x1C14 is set with it and by `Faint`.
  - +0x10AC (also the hand's locked-select test), +0x10F4, mind +0x1C10, mind +0xF60 = 21 and Flags 0x400 / 0x10: not
    named. Their writers: `StopWhatIAmDoing` clears +0x10AC, +0x10B0 and +0x10F4; `fn_0047CFB0` writes +0x10F4.
- **`Creature::StartReacting`** 0x4F26D0 copies mind +0xF4C..+0xF74 to +0xF7C..+0xFA4 (the plan, kept), then
  `Living::StartReacting` 0x6E4590 calls the type's setup (table +0x10).
- **`SetupReactToNastyMagic`** 0x4F3210 (from type 3, with the spell's magic +0xB4, read without a test that the
  initiator is a spell): `ChangeSource(19, 0.5)`; on a Rope leash (leash +0x12C → +0x1C == 2), or with mind +0x164 ≤ 0
  (or unordered), desire 6 / action 157 `CREATURE_EXAMINE_POS`, else 5 / 156 `CREATURE_RUN_AWAY_FROM_POS`; mind
  +0x1C84 = the initiator's position; `ForceActivityAndForceAction(desire, GetBeliefAboutObject(self), action, 0, 0, 1)`.
  Then: a spell whose creator has a spell icon (vt +0xB8) and whose seed (+0xAC) is gone → **return** (no
  AddReaction). Learning (`fn_004F2930(1, magic)`: `ConsiderLearningAction(1, magic)` unless magic is 6) unless the
  creator is set, not a creature, and the spell's player is of type (+0x8E0) 2 or 3, or the seed's byte +0x71 is set
  (`fn_00721910`); after learning from a spell, the seed's +0x71 = 1 (`fn_007218F0`). A thing that is not a spell
  always teaches. Then, on any leash and with the miracle known (`KnowsAction(1, magic)`), a 64-cell spiral from the
  leash's position for the first object other than itself → `fn_004F8850` (a pending cast of the miracle at it, mind
  +0x21D0..+0x21E0) and +0x380 = 1. Last: +0xBC = the initiator, `AddReaction(reaction, 0)`.
- **`SetupReactToNiceMagic`** 0x4F30B0 (types 21 / 37 with the spell's magic; 13 with 19 SHIELD; also
  `SetupReactToMagicTree` 0x4F2E50 with 21 WOOD): the reaction's player set, not the creature's and not `IsAllied`
  (0x64D5D0: +0x950[other] > 0) → return. Not a spell: magic 21 → return; else examine and learn. A spell: icon without
  seed → return; creator set, not a creature, player of type 2 (not 3) → neither examine nor learn; seed +0x71 set →
  neither. Otherwise `ForceActivityAndForceAction(6, belief, 157, 0, 0, 1)` with +0x1C84 the position, the learning,
  and the seed's +0x71 = 1. Then +0xBC and `AddReaction` (also when it did not look).
- **The seed's +0x71** is cleared when a seed is made (`fn_00728140`, from the three `SpellSeed` constructors) and
  written only by the two setups.
- **`Creature::AddReaction`** 0x4F2680: +0x94 = the reaction, the creature at the head of its followers (+0x18, count
  +0x1C), `UpdateHowImpressed(reaction, 1)` 0x4F2780: nothing for its own reaction; else the initiator's
  `GetImpressiveValue(this, reaction)` (vt +0x1B0); the reaction's player is the creature's → mind +0x18C64 += 4 × it;
  else, the initiator being a creature, `UpdateHowImpressiveIsCreature(it, 12 × value)`.
- **The end.** `Living::ProcessLiving` 0x5EC8C6 runs `Living::ProcessReaction` 0x5F1270 for every Living, creatures
  too, before its `ProcessState`: the reaction not available → `StopReacting`; the object (+0xBC) none or not
  available → `StopReactingAndSetState`; `turn − record turn` (signed) above the type's turns to react (table +0x30, at
  `GetDistanceInMetres` to the object) → `StopReactingAndSetState`. A reaction's `ShutDown` 0x6E4720 calls
  `StopReactingAndSetState` (vt +0x99C) on each follower. For a creature `ResetStateAfterReacting` 0x477F30 is
  `StopReacting`, so all of these are `Living::StopReacting` 0x5F1140: off the followers, the type's record refreshed
  (`fn_005F0FE0`), +0x94 = 0, +0xBC = 0.
- **openblack** (R07b, 2026-10-09):
  - `ecs::effects::reactions` gives a `components::Creature` the `LivingClass::Creature` handler,
    `ecs::creature_reactions` (`ECS/Systems/Implementations/CreatureReactions.{h,cpp}`), registered with the others in
    `Locator.cpp`. The rules are pure in `Creature/CreatureReactionRules.{h,cpp}`. Only types 3, 13, 21 and 37 are
    taken; every other type scores 0 (each has its own `Creature::*Priority` / `Setup*`, separate items).
  - The reaction held is `components::CreatureReaction` (+0x94, its type, +0xBC, and whether it is a follower).
    `ProcessReaction` runs at the start of each creature's mind turn (`CreatureMindSystem::ProcessTurn`), not in the
    living list, which holds no creatures in openblack.
  - The availability models IsAvailable, fighting, our faint activity for +0x384, `CreatureKnockedOut` for +0x10B0 and
    our mimicry for +0x1C38; +0x1060 is always on (every creature openblack makes goes through the paths that set it;
    our CHL CREATE makes no creature); the rest is in Pending. As the original, a stilled mind
    (`CreatureMindState::paused`) is not tested: it takes the reaction up, grows afraid and replans, and `WatchMiracle`
    alone skips the learning.
  - The setups call `CreatureMindSystem::ReactToNastyMagic` / `ReactToNiceMagic` (raffclar's agendas,
    `Creature/CreatureMiracleReactions`) for the forced activity, and `WatchMiracle` (one creature's sighting, the body
    of `SeeMiracle`) for `ConsiderLearningAction`. The fear read is `Desire::Fear` after the source grew (inferred for
    mind +0x164). The seed's +0x71 is `SpellSeed::learnedFrom`. The player's type is `PlayerMagic::playerType`. A
    reaction has no player when its miracle has none (`Spell::hasPlayer`); openblack has no alliances, so any other
    player's nice miracle is ignored.
  - The examine agenda points with the `PointAt` object order (`ObjectOrder::Kind::PointAt`, the hands' `PointAt`).

## Leash natives

**Evidence.** The game folder's exe is W120: the native table's "NONE" entry is at 0x00C0DB98, as
`bw1-decomp/src/Black/ScriptFunctions.cpp:29-30` says. bw1-decomp has the table entries (`ScriptFunctions.cpp:232-401`)
and the symbols (`config/BW1W120/symbols.txt:15400-15423, 15711`) but no bodies; the bodies below were disassembled
from `runblack.exe` with capstone. Mac (BW1M110) names are given in brackets where they differ. All nine are ported
(`Creature/LeashScript`, called from `CHLApi.cpp`); see [What openblack does with them](#what-openblack-does-with-the-leash-natives).

### Common facts

- `GScript::ScriptErrorMessage` (0x6F62B0) is a bare `ret`: every script error message is a no-op in the shipped game.
- VM push types: 1 = Int, 6 = Boolean (our `LHVMTypes.h` `DataType`). `POP` returns the raw 32-bit value; only
  TOGGLE_LEASH converts it (`fld` + `__ftol`).
- "Is a creature" is vtable +0x34; "its player" is vtable +0x1C (`GetPlayer`).
- The leash state is a `GLeashStatus` at `GPlayer::GetLeaderInterfaceStatus()` +0x12C. Fields: +0x14 leash on;
  +0x18 works; +0x1C type in use (a LEASH_TYPE); +0x24 object it is tied to; +0x34 turn it was tied; +0x38 interface
  status.
- There are 9 leash entries, not 8 (TOGGLE_LEASH is the ninth). Ids, names and argument/return counts match our
  `CHLApi.cpp` bindings.

### Script enum and the temple's selection

`bw1-decomp/include/chlasm/Enum.h:556-565` (Lionhead header, "Jonty Barnes 2001-11-15", via Daniels118/chlasm):
`LEASH_TYPE_NONE = -1`, `FIRST = 0`, `EVIL = 1`, `ROPE = 2`, `GOOD = 3`. The original `LAST = 4` is commented out; the
decomp changed it to 3. The game data ships no CHL `Enum.h`.

The temple stores the type as a 0-based index in `TempleLeash::field_0x0` (`CitadelHeart.h:77-80`), -1 for none:

- Setter `fn_004648E0` (= Mac `CitadelHeart::SetLeash(LEASH_TYPE)`): 1 → 0, 2 → 1, 3 → 2; anything else leaves it
  unchanged.
- Getter `fn_00464920`: returns -1 when `heart->leashes` (+0xE4) is null or the index is -1; otherwise 0 → 1, 1 → 2,
  anything else → 3.
- `LeashObj::InterfaceTap` (0x464490) sets the index to -1 when the post already selected is tapped.

### Per native

| Id | Name | W120 function | Pops (first pop = last pushed) | Pushes | Behaviour |
|---|---|---|---|---|---|
| 185 | ATTACH_OBJECT_LEASH_TO_OBJECT | `AttachObjectLeashToObject` 0x6F4480 [Mac `AttachObjectLeachToObject`] | A, then B | — | Swaps A and B if only one is a creature, so either order works. Needs a creature with a player, then `fn_005E6BD0(creature, object)` on the leader's `GLeashStatus`: plays the sound, +0x24 = object, +0x34 = game turn, `SetOn(creature, 1)`, `UpdateLeashLengthWhenAttachedToObject`, then 0x4E0DE0 (labelled `MimicPlayer`), and the TownDesireFlags / TownCentre desire. |
| 186 | ATTACH_OBJECT_LEASH_TO_HAND | `AttachObjectLeashToHand` 0x6F4580 | creature | — | Needs a creature with a player, then `fn_005E6EA0(creature)`: +0x24 = 0, `SetOn(creature, 1)`, a sound for the local player. **No branches**: always unties and turns the leash on (differs from raffclar's tied/Toggle logic). |
| 187 | DETACH_OBJECT_LEASH | `DetachObjectLeash` 0x6F4610 | creature | — | Needs a creature with a player, then `GLeashStatus::SetOn(creature, 0)` (0x5E6F70) on the leader status. Off path: clears +0x14 and +0x24, unsuppresses desires unless +0x1C == 2, clears some creature fields, `StopImmersion(7)`. |
| 222 | IS_LEASHED | `IsLeashed` 0x6F4980 | object | Bool | false if missing, not a creature, or no player; otherwise `Creature::GetInterfaceStatusLeashOn(c) != 0` (0x4CF060): some interface status of the player has +0x14 set. |
| 249 | SET_LEASH_WORKS | `SetLeashWorks` 0x6F4E50 | creature, then value | — | `GLeashStatus+0x18 = raw popped bits` (float bits, unconverted) when it is a creature with a player. |
| 269 | IS_LEASHED_TO_OBJECT | `IsLeashedToObject` 0x6F4FC0 [Mac `ObjectLeashedToObject`] | A, then B | Bool | Same swap as 185. Pushes `GLeashStatus+0x24 == other`; does not test +0x14. Any failure pushes false. |
| 275 | GET_OBJECT_LEASH_TYPE | `GetObjectLeashType` 0x6F5460 | object | **Int** | See below. |
| 305 | SET_DRAW_LEASH | `SetDrawLeash` 0x708C80 | value | — | `GGame+0x250090` (GScript) `+0x78 = raw popped bits` (written at 0x708C9D); matches [intro.md](intro.md). Does not touch the leash state. |
| 354 | TOGGLE_LEASH | `ToggleLeash` 0x6F4430 [Mac `TogglePlayerLeash`] | player (float, `ftol`) | — | See below. |

**TOGGLE_LEASH.** `ConvertScriptPlayerToGamePlayer` (0x6EB9A0) → `GetPlayer` → `GetLeaderInterfaceStatus` →
`GetInterface` → `fn_005D06E0`, which takes `player->creature` (+0xA4C), needs creature vt+0x2C true and Creature
+0x1110 != 0, needs the leash off or on in this same interface status (+0x39C), then calls `fn_005E7140`: if tied
(+0x24) it unties (+0x24 = 0, town desire reset, `StartImmersion(7, 0x80000000)`), otherwise
`SetOn(creature, !+0x14)`.

### GET_OBJECT_LEASH_TYPE (0x6F5460-0x6F54F0)

It does **not** report the creature's own leash. It does `dynamic_cast<Creature>` → `GetPlayer()` → `player->citadel`
(+0xA48, `Player.h:95`) → `citadel->heart` (+0x30) → `fn_00464920(heart)`. It reports the **type selected at that
player's temple**, pushed as Int:

| Case | Pushes |
|---|---|
| Object not found, not a creature, no player, no citadel, or no heart | `0` |
| Heart exists but its TempleLeash is not created, or nothing is selected | **`-1` (`LEASH_TYPE_NONE`)** |
| Selected index 0 / 1 / 2 or more | `1` EVIL / `2` ROPE / `3` GOOD |

**No leash selected pushes -1.** 0 only means the lookup failed. raffclar's "None → 0" is right only for those failure
cases. The port is to push what the original pushes.

### What openblack does with the leash natives

`creature_leash::script` takes the leash service and the things named, so it is tested with a fake
(`test/creature/test_leash_script.cpp`); the natives in `CHLApi.cpp` pop as the table above and push the defaults when
there is no leash service. Both are dormant on Land 1 and Land 2: no leash opcode runs there with the start-up answer 3
(the `SET_DRAW_LEASH` calls are in `CreatureDevLeashIntro` and `AttachToHouse`, which are skipped).

- **A thing**: an id of 0 or an entity no longer valid is none, silently (the original's messages are empty). "A
  creature" is an entity with the `Creature` component, looked up through the const registry, so no storage is made.
  The "with a player" part is not checked: the leash service refuses a creature its player can't lead.
- **185 / 269**: the second pop is the creature and the first the thing; when only the first is a creature, they swap.
  185 ties the leash (`TieTo`, which puts the picked leash on first if none is worn); 269 compares the tie.
- **186**: a tied leash is untied back to the hand; a leash not worn is put on (`Toggle`); one already held stays. The
  original always unties and turns the leash on. Both end in the same state only when the leash service lets the
  leash on: `Toggle` (and `TieTo` for 185, through `PutOn`) goes through `WhyNot`, and refuses, logging it and keeping
  it as the last refusal, a creature that is not the one its player leads or that does not know the leash. The script's
  command then does nothing here (Pending).
- **187**: `TakeOff`. **222**: `IsLeashed`. **249**: `SetWorks`, set when the popped bits are not zero.
- **275**: as the original: the leash picked at the temple of the creature's player (`temple_leash::PickOf`, read
  through `leash_posts::ScriptLeashType`): 0 when the thing is missing or not a creature or its player has no temple
  heart, -1 with nothing picked, else 1, 2, 3. The posts' tap picks at once on the tapping side and packet 0x65 again
  at the next turn, as the original; the debug window can set it too.
- **305**: `script_control::CameraControl::drawLeash` = the popped value as it is; the leash draw reads it.
- **354**: the script player (a float, truncated) through `magic::ScriptPlayerToGamePlayer`, then the leash key for
  that player (`PressKey`, which also refuses a player with no creature to lead). In the port a script player out of
  0..8 does nothing: that range is `ScriptPlayerToGamePlayer`'s own, and the original's conversion's range checks are
  not traced (Pending). The original reaches its own creature checks by another path: the leader's interface status,
  then `fn_005D06E0`'s tests (creature vtable +0x2C, Creature +0x1110, the leash state of that same interface status);
  the port's are `PressKey`'s (`PlayersCreature`, then `WhyNot` in `Carry`). Which creatures each lets through is not
  compared (Pending).

### The leash keys

L, V and B are read once a frame, after the temple's room keys: the first of the three that went down this frame
(down and changed, as the room keys are read) goes to `PressKey` for the local player, only when the player has a
creature they can lead. They are not read while the debug windows have the keyboard (the actions are not framed then)
nor while the world is paused in the citadel. The original's key path is not read (Pending); since at most one turn
runs a frame, a key read at frame time changes the leash before the next turn, as a packet sent for it would.

## The player's creature

The creature a profile brings to a land, read in `runblack.exe` W120 (bw1-decomp has the headers only).

- **`LOAD_MY_CREATURE`** (native 250, `GScript::LoadMyCreature` 0x6FD260) pops z, y and x, drops y, and calls
  `GGame::LoadMyCreatureIntoNewMap(cell x, cell z)` 0x552AC0 (the Mac's name; W120 `fn_00552AC0`) with the high words of
  `ftol(metres × 6553.6)`. It is that function's only caller; the lands' scripts reach it with their own
  `LOAD_MY_CREATURE` (Land 1 ip 68025, Land 2 ip 84316, Land 3 ip 113848, VortexEntry ip 128).
  1. If the local player has a creature (`GPlayer` +0xA4C), nothing.
  2. The file is the local `LHPlayer`'s +4 string (interface +0x1BC), or "Dummy": the profile's registry value
     `HKCU\Software\Lionhead Studios Ltd\Black & White\LHMultiplayer\Profiles\<profile>\file`, read with
     `LHNetGetCurrentProfileString("file")` (0x66BF5B). It is a `C<8 hex>.erc` name the profile keeps; it is not a CRC32
     of the profile's name.
  3. `Creature::Load(name, MapCoords(cells), player, NULL)` 0x4E7FF0 → `LoadFullyQualified(".\scripts\CreatureMind\" +
     name)` 0x4E8040: the version into 0xCAB174, then the species row (< 17), `Creature::CreateCreature(coords,
     &CreatureInfo[row] (0xC60460 + row × 0x394), player)` 0x474B50, +0x1058 = (start data and not 1), the path to
     +0x1228, `CheckValidation` from version 0x15, then the mind (`fn_004E84A0`, which builds the body through
     `Update3DCreatureFromAttributes` at 0x4E8A05). A missing or empty file or a bad row: no creature, no message.
  4. `SetFizz(1.0, 0, false)` then `SetFizz(0.0, 3.0, false)` (0x47AB90): sound tag sfx 0x27 at the creature and the
     fizz at 1, then a fade to 0 at -1/3 per second (+0x12AC lock, +0x12B0 current, +0x12B4 target, +0x12B8 rate). No
     `rand()`.
  `MapCoords(cell, cell)` 0x602FC0 puts the point in the middle of the cell (low words 0x8000), altitude 0.
- **`CURRENT_PROFILE_HAS_CREATURE`** (463, `GGame::CurrentProfileHasACreature` 0x555A30): whether
  `.\Scripts\CreatureMind\<that file>` exists.
- **`CreateCreature`** 0x474B50 returns NULL when the player already has a creature; otherwise `Creature::Create`
  0x474A20 (memory zeroed by `Base::new` 0x4366F0, so the development phase +0x1268 starts at 0). The home +0x1200 is
  the player's +0xA48+0x30 object's position (the citadel's), else the creation point; the creation point is also copied
  to +0x1214. The size is the species' +0x1FC. Its draws are all on the synchronised stream: the Living constructor's
  `GameRand` (0x5EBF89), strength and fat ± 0.1 (`GameFloatRand(0.2)` at 0x4EF3FA and 0x4EF426), each desire's
  initial value and increase time (0x4DC14E, 0x4DC346), two per desire source (0x4DE1B2, 0x4DE21B), about 30 in
  `fn_004E2E70` (0x4E2E93..0x4E33F6), one in `fn_004F1FF0` (0x4F201F) and, on condition, `SpreadReaction` (0x6E3F18);
  a range of 0 draws nothing. Loading from a file adds the mind's sources (0x4E870D).
- **`Physique<file>`** in the same folder is written by `Creature::Save3D` 0x4E70C0 (every 600 ticks for the local
  creature when interface +0x1FC and +0x1058 are 0, on `ToBeDeleted` and on `ClearMap`) and read only by the front
  end's tattoo editor (0x4E7310, from 0x54284A); never by `LOAD_MY_CREATURE`. Layout: int species; floats size
  (LH3D +0x90), strength (physical +0xC), fat (physical +0x14) and alignment; then two counted dword lists (skin marks,
  LH3D +0x5184). `PhysiqueC4ba71b36.erc`: Mandrill, size 2.0, strength 0.84212, fat 0.4, alignment 0.118912, 0 and 3
  entries.
- **`Profiles\<name>\creature.lhp`** is written by `fn_0047B440` with the same 600-tick save: the creature's values
  into `.\scripts\creature_html.tmpl` (`%s\html\%s.html`), then `GenerateCreatureHTML` `fn_00578330` writes an
  "LHNILD" archive with a "CREATURE" section and up to five `creatureshot_%d.jpg`. Nothing reads it back. Its header
  (`fn_005776E0`) is the game's only `srand(time)` (0x577721), followed by a `LocalRand(0x8000)` (0x577731).
- `GGame::GetNextPlayerWithNoCreature` 0x550A30 has no callers (it also ignores its argument).
- **The natives Land 1 reaches on it:**

  | Native | Original | Pops, in order | Does |
  |---|---|---|---|
  | 197 `CALL_PLAYER_CREATURE` | 0x6F3D40 | player | the game player's creature (+0xA4C) through `AddScriptGameThing(c, 0)`; none: "No creature of player %d" and 0 |
  | 223 `SET_CREATURE_HOME` | 0x6F4A20 | z, y, x, the object | a creature: +0x1200 / +0x1204 = `ftol(x × 6553.6)` / `ftol(z × 6553.6)`, +0x1208 = 0 (y dropped) |
  | 208 `SET_CREATURE_DEV_STAGE` | 0x6F4820 | the stage (an int), the object | a creature: `MoveToDevelopmentPhase(stage, 0)` |
  | 205 `DEV_FUNCTION` | 0x6FC130 | the function (an int) | the table below |
  | 282 `CREATURE_IN_DEV_SCRIPT` | 0x6F5680 | the object, the value | a creature: +0x10C0 = the value as popped |

  The object natives print "Thing not found!" / "Thing not creature!" and do nothing for anything else.
- **`DEV_FUNCTION`** (`SCRIPT_DEV_FUNCTION`, bw1-decomp `include/chlasm/ScriptEnums.h`; jump table 0x6FC4E4), on the
  local player's creature:

  | n | Name | Does |
  |---|---|---|
  | 1 | START_DEVELOPMENT_SCRIPTS | `MoveToDevelopmentPhase(0, 0)`, the home copied to +0x11A8, +0x11B4 = 12.0 (no null check) |
  | 2 | ROPE_LEASH_ENABLED | +0x1110 = 1 |
  | 3 | OTHER_LEASHES_ENABLED | +0x1114 = 1, +0x110C = 1 |
  | 4 | LOAD_MY_CREATURE | `SetPacket(0x4E)` |
  | 5 | RESET_ESCAPE_STATE | `TutorialState` = 1 |
  | 6 | MY_CREATURE_POINT_OUT_HIGHLIGHT | a spiral search for a `ScriptHighlight` that is not a did-you-know, then a forced plan |
  | 7 | CLEAR_INTERACTION_MAGNITUDE | mind +0x18C5C = 0 |
  | 8, 9 | MY_CREATURE_CAN_DIE, CANNOT_DIE | +0x1158 = 1, 0 |
  | 10, 11 | CREATURE_HELP_ON, OFF | nothing; 42 calls of `fn_004CA580` |
  | 12 | ENTER_SAVEGAMEROOM | `GoInsideCitadel(5, 0)` |

  Packet 0x4E (handler 0x63D86B, jump table 0x63DDCC) makes or loads the sender's creature at its citadel's home
  (`CLANCREATECREATURE<nn>` start data: `CreateCreature` and phases 0-12; otherwise `Creature::Load`), then sets +0x1108,
  +0x110C, +0x1110 and +0x1114 to 1 and phase 13: multiplayer and skirmish only.
- **`MoveToDevelopmentPhase`** 0x4C59C0: when confined, unleashed and at phase 5 or more, +0x11B4 = 0; the phase to
  +0x1268, +0x126C = 0; every desire unsuppressed and deactivated; then for each phase from 0 to the one asked, its "add"
  list (10 slots) activated and its "remove" list (4 slots) deactivated (42 = empty); +0x194 = 0 and the agenda cleared.
  No draw. The table is `CreatureDevelopmentPhaseEntry[14]` at 0xC84878 (stride 0x84) from `info.dat`'s
  DETAIL_CREATURE_DEVELOPMENT (records of 0x74 bytes from 0x10B6C; the add list at +0x4C in memory, the remove list at
  +0x7C). Adds by phase: 0: 5, 8, 9, 17, 18, 21, 24, 28, 35, 23; 1: 4, 6, 29, 38, 3; 2: 15, 19, 20, 36; 3: 14; 4: 33, 7;
  5 and 6: 16, 37; 7: 31; 8: none; 9: 0, 10, 27, 13; 10: 2; 11: 1; 12: 0, 10, 27; 13: 1, 32, 34, 39, and 29 removed. At
  phase 13, 33 desires are active; off are 11, 12, 22, 25, 26, 29 and 30.
- **`SET_FOCUS` on a creature** (`GScript::SetFocus` 0x6F90B0): a creature is first put under script control (vtable
  +0x440, bit 0x400 of +0x24), then `Creature::SetFocus(LHPoint)` 0x4F6760 (vtable +0x510; other objects take
  `Object::SetFocus` 0x6393A0, the yaw snap). It stores the point in the mind (+0x1D3F4 = +0x1D3F8 = 1, the point at
  +0x1D400), `PrepareCreatureForScriptedAction(1)` 0x4F6A90 (script control, `FinishActionUnsuccessfully`, a scripted
  plan of desire 0x18 and action 0x16 into mind +0xF48, the sub-action agenda at mind +0xFA8 reset), then queues
  sub-action 7 "TurnToFacePos" with `LookAtPosition` 0x4D1460. Each turn its phase 0 (0x501660) is done when the point
  is less than 0.1 away or within π/8 of the creature's heading; otherwise `StopMoving` and `StartTurningAction`;
  phase 1 (0x501730) waits for the body action to end. No draw on this path (the agenda's `GameRand(7)` at 0x4FF086 needs
  a second function, which this sub-action has none of).
- **The tutorial's creatures** (`CreaturesInGlade`, answers 0-2): made by `CREATE` type 12 (`GScript::CreateThing`
  0x6F1B20, case at 0x6F149C) with no player, then `CREATURE_SET_PLAYER` 0x6F41C0 (+0xA4C, +0x1070, the name). Offered:
  the Cow (row 1), the Ape (row 0) and the Tiger (row 2) by (2233, 3152) (ip 44076-44096).
- **The home follows the temple.** `Creature::ProcessState` (0x472E1A..0x472EA5) rewrites the home +0x1200 / +0x1204 /
  +0x1208 on every step, with no state test, whenever the player, `player+0xA44`, the citadel (`player+0xA48`) and its
  heart (`Citadel+0x30`, a `CitadelHeart`) exist: the heart's `Game3dObject` (`Object+0x40`) special point #15 (vtable
  +0x1CC with edx = 15, `Game3DObject::GetSpecialPos` 0x63B040) in world space, x and z × 6553.6 truncated, altitude 0.
  So `SET_CREATURE_HOME` only lasts while there is no heart. On Land 1 point #15 of `b_first_temple.l3d` is the last of
  its 16 extra metrics, local (-9.318, 0, -20.437); with the heart at (1915.05, 2508.89) turned 36.0 rad
  (`CREATE_PLANNED_CITADEL` angle 36000 × 0.001) it is (1895.97, 2520.75), 0.87 m from the script's HomePos.
- **Drawn smaller in the pen.** `Creature::ShrinkDownIfNearCitadel` (its Mac name; W120 `fn_004EFCE0`, called only
  from `Creature::ProcessState` at 0x472F72) takes the real scale (`[+0x160]+0x6C`, read there directly at 0x4EFCEC; `GetScale`, vtable +0x120 0x47B190 →
  `CreaturePhysical::GetUserSize` 0x4EF4F0, reads the drawn +0x90 instead) and, when
  the owner's citadel heart is built (`CitadelPart::IsBuilt` 0x464AD0, vtable +0x890), the distance `d` from the
  creature to its home +0x1200 is at most 16 m (`GUtils::GetDistanceInMetres` 0x74CD70) and the creature is between
  the pen's walls, draws it at `0.22 + (real − 0.22) × (clamp(d, 14, 16) − 14) / 2` (constants 0x8D14C4, 0x8D14C8,
  0x8D1520) through `LH3DCreature::SetSize` 0x480530; elsewhere at its real size. The walls: θ0 = the heart's
  `GetYAngle` (+0x4C, radians as stored) + 3.83, θ1 = θ0 + 0.897598 (2π/7); with dx, dz the drawn creature (+0x78,
  +0x80) minus the heart's point in metres, inside is `cos θ0·dz − sin θ0·dx ≥ 0` and `sin θ1·dx − cos θ1·dz ≥ 0`
  (the heart's arms 0 and 1). The real size is never written, so it comes back as the creature walks out; no draw; no
  state. In the Creature Cave (`CreatureRoom::InitEngine` 0x787BD8) the creature's copy takes the real size. On Land 1
  the home is inside the pen (9.78 and 9.01; 0.87 m), so from turn 8 the creature is drawn at 0.22.
- **Everything SetSize derives follows the pen size (read 2026-10-09).** Because `ShrinkDownIfNearCitadel` calls
  `LH3DCreature::SetSize` 0x480530 every turn (all its paths end at 0x4EFE59..0x4EFE69), with the pen size in the pen
  and the real size `[physical+0x6C]` elsewhere, every value SetSize sets is the pen's while the creature is there.
  With `arg` its argument and `s` = ClampScale(arg) (0.05..4):

  | Address | Field (names from the sync log `fn_0048F830`'s strings) | Value |
  |---|---|---|
  | 0x480533..0x48054F | local rate | 1.6 − (arg × 0.5) × 0.85 = 1.6 − 0.425·arg |
  | 0x480553..0x48055F | local f, stored as a float | (arg × 0.5) × 1.75 + 0.25 = 0.875·arg + 0.25, **arg unclamped** |
  | 0x480599 | +0x90 UserSize | s |
  | 0x4805A1..0x4805B1 | +0x94, the drawn scale | s × 15 / +0x8C |
  | 0x4805B7..0x4805C1 | +0x4844 WalkSpeed | f × 8 |
  | 0x4805C7..0x4805D1 | +0x4848 RunSpeed | f × 20 |
  | 0x4805D7..0x4805F0 | +0x4838 CurrentSpeed | min(current, RunSpeed) |
  | 0x480608 | +0x484C Acceleration | 12 |
  | 0x4805FE..0x480612 | +0x4858 (head look) | rate × +0x485C |
  | 0x480618..0x480622 | +0x4850 BodyTurnAccel | rate × π |
  | 0x480628..0x48063C | +0x498C / +0x4990 breath | 5 / (1 / √s) = 5·√s |
  | 0x480642..0x48064C | +0x5228 radius | +0x94 × +0x5224 ([The creature's radius](#the-creatures-radius)) |
  | 0x480652..0x4806A7 | leash lengths | `fn_005E6980` per untied leash |

  The walk reads them so: the gait 0x48D940 (called from the walk step 0x49145E with the turn's distance) blends stand
  and walk below WalkSpeed with weight speed / +0x4844, and walk and run above it with (speed − walk) / (+0x4848 −
  walk); its stride is the animation's stride × **+0x94** × the weight (0x48D9BA..0x48D9C3, 0x48DA59..0x48DA63), and the
  walk advances by walk duration × distance / stride. The step-or-walk choice divides the distance by +0x94
  (`fn_00483AB0` 0x483CB7), and a step moves IntTimeInc × **+0x94** / duration × displacement a frame (0x490F79). So
  in the pen the creature walks at the pen size's speeds with the small body's stride: at the same fraction of its top
  speed its feet cycle in proportion to (0.875·s + 0.25) / s, 2.011 at 0.22 against 1.125 at size 1 and 1 at size 2.

### A land change: the creature saved and made again

Read in `runblack.exe` W120, 2026-10-09.

- **The save.** `GGame::ClearMap` 0x552BB0 (from `StartPlaygroundGame` 0x552F40, which `LOAD_MAP` calls) begins by
  saving the local player's creature (the player at g_game+0x205A59, its creature +0xA4C; 0x552C3A..0x552D54): only when
  there is one, the player has a leader interface status, and creature +0x1058 is 0. The name is the local `LHPlayer`'s
  +4 string (interface +0x1BC), the profile's `file` value. Then:
  - `CreatureMental::SaveMind(".\Scripts\CreatureMind\" + name)` 0x4E7820 (0x552CE9, the format string 0x9CF968);
  - `Creature::Save3D(".\Scripts\CreatureMind\Physique" + name)` 0x4E70C0 (0x552D54, the format string 0x9CF944).
  So the file is the profile's own mind file, the same one `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE` read,
  written over at each land change.
- **+0x1058** is 0 for a creature of the campaign: `LoadFullyQualified` sets it to "start data given and its byte not
  1" (0x4E8271..0x4E8282; `LOAD_MY_CREATURE` gives none), the constructor's helper fn_00474130 to 0 (0x474136), and the
  multiplayer creature packet to 1 (0x63DA16). So only a multiplayer creature is not saved.
- **The physique file** (`Save3D`): u32 the species row (`CreatureInfo` +0x1F4), f32 the 3D body's size (+0x90, the size
  it is drawn at, so the pen's smaller size when it is there), f32 strength (physical +0xC), f32 fatness (+0x14), f32
  the alignment as saved (fn_004F5FA0: the value before a nice spell, else before a nasty one, else the creature's), then
  fn_00481040: a count and that many u32 from the 3D body's +0x5184 list (+8), and a second count and list (+0x1008).
  `PhysiqueC4ba71b36.erc` is 14, 2.0, 0.84212, 0.4, 0.118912, 0 and 3 entries.
- **The creature goes with the land.** `GlobalGameLists::ClearMap` 0x591520 (0x552DBF) first calls `ToBeDeleted(0)` on
  every creature (the list g_game+0x205BBC, next +0xA4; 0x591537..0x591555), and the creature's destructor fn_00475170
  clears its player's +0xA4C (0x475279). So the next land's `LOAD_MY_CREATURE` finds no creature and loads it from the
  file just saved: Land 2 at ip 76347 (`Land2VortexEntry`), Land 3 at ip 103099 (`SetupLand3`), and `VortexEntry` at
  ip 106. This corrects the note that the creature goes with the player and the next `LOAD_MY_CREATURE` does nothing.
- **openblack** (`ecs::player_creature::SaveMyCreature`, called by `Game::ChangeLand` before the land is loaded):
  - the creature is the one the local player leads (`LeashSystemInterface::PlayersCreature`), the file the profile's
    (`--creature-file`) in `Scripts/CreatureMind`; nothing without a creature, a profile file or a mind set up yet. Our
    creatures have no start data, so +0x1058 is always 0;
  - the mind file is `player_creature::ToMindFile`: what `CreatureMindSystem::SaveMind` gives (the desires, learning,
    opinions, sightings, known skills and miracles, stage, size, strength and alignment, as saved before any spell),
    with the body's fatness (`creature_mind_body::ToMindFile`); the previous fatness, age, energy, needs, tattoos and
    every section the mind does not model are kept from the file the creature was loaded from;
  - it is written through the file system service and the mind cache's entry is dropped
    (`resources::SaveCreatureMind`), so `LOAD_MY_CREATURE` reads it back from the disk;
  - the physique (`components/creaturemind` `PhysiqueFile`, `creature_mind_body::ToPhysique`): the species row, the
    size it is drawn at (`player_creature::PenSizeOf`, the value `ShrinkInPens` draws it with, from the creature's
    turn position, which has not moved since that turn's pen shrink; its own size outside a pen), strength, fatness and
    the alignment as saved, and the skin's two lists kept from the file it replaces (we do not model them). The size
    is the one the 3D body would keep: `LH3DCreature::SetSize` 0x480530 stores at +0x90 (0x480599) 0.05 when the size
    is not above 0.05 (`fcomp` [0x8AC3F4] 0.05f, `test ah, 0x41` at 0x48056F, so also for a NaN), else the size when it
    is below 4 (`fcomp` [0x8AB418] 4.0f, `test ah, 1` at 0x480580), else 4; `creature_morph::ClampScale` (the one size
    clamp for the 3D body, also used on the drawn size before it is written) does the same, in the same order;
  - openblack keeps a one-time `<file>.bak` of the profile's mind file and of its physique file (when one exists)
    before it first writes over them, which the original does not (`resources::KeepBackupOnce`); if the backup cannot
    be made, that file is not written over;
  - the registry's reset then takes the creature, as the original's map clear does.
### Land 2's creatures

Khazar's and Lethys's creatures are not made by `Scripts\Land2.txt` (no creature command among its 39 kinds; it only
switches the two computer players on and builds their citadels at (2509.12, 1779.93) and (1006.36, 3677.02)). The
challenge script makes them: `LandControlAll` runs `LandControl1` (ip 155485) and **then** `LandControl2` (ip 155490),
which runs `EnterLand2` (ip 102763, the arrival scene), `EndEnterLand2` (ip 76137) and
`SetUpComputerPlayersAtStartOfLand2` (ip 75933, script id 268, address 74836):

| What | Khazar | Lethys |
|---|---|---|
| Marker (`CREATE` type 1, subtype 0) | (2540.93, 78.97, 1916.63), ip 74845 | (1050.88, 147.59, 3607.17), ip 74855 |
| `CREATURE_AUTOSCALE` factor | 1.2 | 1.5 |
| Script player | 2 (game PLAYER_TWO) | 3 (game PLAYER_THREE) |
| Runs (async) | `SetupKhazarCreature` (id 87, address 12414), ip 74890 | `SetupLethysCreature` (id 88, address 12779), ip 74894 |
| `LOAD_CREATURE` type | 7 = `CREATURE_TYPE_TORTOISE` | 4 = `CREATURE_TYPE_WOLF` |
| Mind file (data offset) | "KhazarCreature" (348) | "LethysCreature" (363) |
| `SET_CREATURE_NAME` text id | 5002 | 5001 |
| Alignment | = MyCreature's (ip 12768) | = -MyCreature's (`NEG`, ip 13133) |
| `CREATURE_FORCE_FRIENDS(1, it, MyCreature)` | yes (ip 12772) | no |

The species numbers are bw1-decomp `include/chlasm/CreatureEnum.h` (`CREATURE_TYPE`, Ape = 0). Both Setup scripts, in
order: `LOAD_CREATURE(type, file, player, GET_POSITION(marker))`; `It = CALL_PLAYER_CREATURE(player)`;
`SET_CREATURE_NAME`; `SET_CREATURE_DEV_STAGE(It, 13)`; `CREATURE_AUTOSCALE(1, It, factor)`; 45
`CREATURE_SET_KNOWS_ACTION(It, type, action, 1)` (type 0: actions 0-6; type 1: actions 1-6 and 10-41); STRENGTH (17)
and properties 26-33 set to 0.2; `RELEASE_FROM_SCRIPT(It)`; then a loop `while not THING_VALID(MyCreature):
MyCreature = CALL_PLAYER_CREATURE(1); SLEEP 5`; then the alignment (18) and, for Khazar, the friendship; and
`RELEASE_FROM_SCRIPT(It)`. `SetupLand5` runs Nemesis's twin, `SetupNemesisCreature` (ip 140952); `SetupLand3` runs
Lethys's again (ip 103147).

- **The mind files** `Scripts\CreatureMind\KhazarCreature`, `LethysCreature` and `NemesisCreature` are byte-identical
  (5268 bytes, version 25, species row 0, name "Matey"): the species comes from the script.
- **`LOAD_CREATURE`** (native 329, `GScript::LoadCreature` 0x6FD2F0) pops z, y (dropped) and x, and keeps the point as
  `MapCoords{ftol(x × 6553.6), ftol(z × 6553.6), altitude 0}`: the full fixed point, **not** the cell's middle that
  `LOAD_MY_CREATURE` uses. Then the player (`ftol`, `ConvertScriptPlayerToGamePlayer` 0x6EB9A0), the file name and the
  type (the raw integer). A player with a creature (+0xA4C) gets "Player has already his creature loaded, " and the
  native **carries on**. A player without a computer-player object (+0x944) gets "Player has not a tocomputer player
  field" and nothing; `GPlayer::Init` 0x649190 makes that object for every player it sets up (0x6491C0..0x6491FD).
  Else `fn_0065BA40(type, &coords, name)` on that object.
- **`fn_0065BA40`** opens `".\Scripts\CreatureMind\%s"` (0x9CF968); a file it cannot open: nothing. Else
  `Creature::Create(coords, &CreatureInfo[type] (0xC60460 + type × 0x394), owner = the computer player's +0x28)`
  0x474A20 (the type is not range checked), `fn_0064B4F0` (the player's +0xA4C = it, then `fn_0064B510`), the file
  name to +0x1228, `CreatureMental::LoadMindIntoPreexistingCreature` 0x4E8440 (the version to 0xCAB174, the species
  dword read and **ignored**, `CheckValidation`, the mind `fn_004E84A0`, then `fn_004C9FE0(phase + 12, 0, 0, 0, 0)`),
  +0x1108 = +0x110C = +0x1110 = +0x1114 = 1, `MoveToDevelopmentPhase(13, 0)`, and the file closed.
- **`Creature::Create`** 0x474A20 with a player and out of the land's bounds: NULL. With the owner's citadel (+0xA48)
  the home +0x1200 is its heart's (+0x30) special point 0 (vtable +0x1CC, edx = 0) in the fixed point, altitude 0; a
  citadel without a heart, or without the point: NULL (after construction). Without a citadel: the creation point.
  `ProcessState` then rewrites the home to point 15 every step (the home follows the temple, above).
- **`SET_CREATURE_NAME`** (native 343, `GScript::SetCreatureName` 0x6F5BE0) pops the text id, then the object
  (`GetScriptGameThing`: no thing, nothing more). Not a creature: "Thing not creature!" and nothing. A creature:
  `wcscpy` of the help text database's entry into `Creature::name` (+0xE0, 64 UTF-16 characters, not bounded by the
  copy); the entry is the id's when 0 < id < the database's count (unsigned compare), else entry 0. In the W120
  Spanish data text 5001 is `HELP_TEXT_LETHYS_CREATURE_NAME_01` "Laetes" and 5002 `HELP_TEXT_KHAZAR_CREATURE_NAME_01`
  "Khalen"; the name replaces the mind file's ("Matey"), which `LOAD_CREATURE` read just before. The native is not
  Land 2's only: in `challenge.chl` it is also called by `SetupNemesisCreature` (5000), `CreateGuideCreature` (5003,
  the Guide; run by `MeetTheGuide`, `MoveTheGuideAround`, `TheStorm`, `GuideImpressTownLesson`,
  `GuideImpressTownNextLesson` and `CreatureDevGuideTeachesFight`), `CreatureGuardian` (5005; run by `LandControl1`),
  `Land4Ogre` (5005), `BlindWomanMain` (5006, the old woman's gremlin) and `TheBigFightMain` (5007).
- **`CREATURE_AUTOSCALE`** (native 452, `GScript::CreatureAutoscale` 0x6F6080) pops the size factor, the object
  (`GetScriptGameThing`) and the enable flag. No thing, or a thing that is not a creature (vtable +0x34, `IsCreature`):
  "Thing should be valid!". Any thing that is there, a creature or not (the second `IsCreature` call's result is not
  tested), then gets +0x115C = the enable flag (the raw dword) and +0x1160 = the factor. The fields are read only by
  `fn_0047DC50`, called from `Creature::ProcessState` at 0x472FB7, after the creature's spells (`ProcessSpells`,
  0x472EBB) and `ShrinkDownIfNearCitadel` (0x472F72) and only when the creature is not in a locked select (`Flags +0x24
  & 0x10`, tested at 0x472F77: with it the turn goes another way and skips the step). It does nothing in a multiplayer
  game (`GGame::IsMultiplayerGame`), with +0x115C zero, or when the local player (`GGame+0x205A59`) has no creature (its
  player's +0xA4C). Else, in float: `v = (base · factor − own) · 0.5 + own`, where `own` is the creature's real scale
  (`[+0x160]+0x6C`) and `base` = `fn_004F5E60` on the local creature's `CreatureReceiveSpell` (+0x370): while a size
  spell is on it (`IsCreatureSubTypeSpellActive(0)`), its size before big (slot 2, +0x40) if big is on, else before
  small (slot 1, +0x28) if small is on; else its real scale. The result goes through `fn_004F5EA0` on the creature's own
  `CreatureReceiveSpell`: `v` if `v` compares below 2.0 or unordered (`fcom` against 0x8AB478, C0 tested: a NaN is
  kept), else 2.0; written to before big while big is on, else before small while small is on, else to the real scale.
  Land 2's scripts set it to 1.2 (Khazar's creature) and 1.5 (Lethys's). Its other callers in `challenge.chl`: on Land 1
  `CreatureDevGuideTeachesFight` (the Guide on at 1.2, ip 25767, and off with 0, ip 26730) and `CreatureGuardianScale`
  (the Guardian at 1.2, ip 41638); then `SetupNemesisCreature` (its `CreatureSize` local, ip 13168), `Land4Ogre` (1.1,
  ip 121180) and `TheBigFightMain` (1.0, ip 136614).
- **`CREATURE_SET_KNOWS_ACTION`** (native 71, `GScript::CreatureSetKnowsAction` 0x6F3960) pops whether it knows, the
  action, the kind (`CREATURE_ACTION_LEARNING_TYPE`: 0 a skill, 1 a miracle by its magic type), then the object
  (`GetScriptGameThing`). No thing: "No creature for script" and nothing. A thing that is not a creature (the dynamic
  cast to `Creature` fails): "No script for creature", and the call then goes on with a null creature (0x6F39F4), which
  the original does not guard. `Creature::ScriptSetKnowsAction` 0x4F7090 calls
  `CreatureActionsKnownAbout::SetKnowsAction` 0x4E27F0 on the mind's +0x1A9FC: knowing, `AddActionLearnt(creature,
  kind, action, notify 1)` 0x4E28C0, which appends the action to the kind's list (+8 + kind · 8, count at +0xC + kind ·
  8) unless it is in it, and, with a creature, shows the learning message (`fn_004C9FE0` code 6 for a skill, 7 for a
  miracle, the action as its index); not knowing, the action's node is unlinked from the list and freed (absent:
  nothing). The list takes any action number: no check against the tables. Then, knowing and kind 1 only, the mind's
  miracle sightings (+0x17D38 + action · 4) = `__ftol(NumTimesMagicActionMustBeSeenBeforeItIsLearnt(action))` (0x4F8CA0:
  `fild` of the row's times to see, `fmul` by the species' multiplier at `CreatureInfo + 0x37C`, 0 for an action of 42
  or more; on the 24-bit x87 it is the float product, cut towards zero). The miracle's last-seen turn and a skill's
  watching are not touched; a miracle taken away keeps its sightings. Land 2's Setup scripts teach skills 0-6 (the
  table has rows 0-5) and miracles 1-6 and 10-41.
- **`CREATURE_FORCE_FRIENDS`** (native 309, `GScript::CreatureForceFriends` 0x6F5770) pops one object (`a`, the last
  argument: `MyCreature` in `SetupKhazarCreature`), the other (`b`, `It`) and the enable flag, each object through
  `GetScriptGameThing`. Either missing: "Thing not found!" and nothing. Then "Thing not creature!" for `a` if it is not
  a creature (vtable +0x34, `IsCreature`), and again for `b`; either not a creature: nothing. Two creatures with the
  flag 0: nothing (no friend is taken away). Else `a`'s friend list (+0x3D0, a singly linked list of 8-byte nodes
  {next, creature}, its count at +0x3D4) is searched for `b`; absent, a new node is put at the list's **front** and the
  count goes up; then the same for `a` in `b`'s list. So each is in the other's list once, the newest friend first,
  and a creature made its own friend is in its list once. Readers (from the x86's references to +0x3D0, not followed
  further): `Creature::CanBeAttackedByCreature` 0x4E4280 answers 0 when either is in the other's list (after the same
  test on the list at +0x12A0); `CreatureMental::GetActivityObjectUsefulness` (0x4E9A26) gives a usefulness of 0 for
  an object in the list (the branch around it not read); `CreatureMental::RemoveBeliefsAboutDeletedObjects`
  (0x4D8898) unlinks a deleted friend (its `+0xA` bit 0) from its own creature's list. Its callers in `challenge.chl`:
  `SetupKhazarCreature` (ip 12772, after the alignment) and `OgreFamilyControl` (ip 122166 and 122170).
- **`GET_PROPERTY` / `SET_PROPERTY` on a creature** (natives 21 and 22). `GScript::GetProperty` 0x70DAE0 pops the
  object (`GetScriptGameThing`), then the property; no thing: "Thing no longer valid" and a float 0; a script
  container (vtable +0x3F8) with any property but PLAYER (21): a float 0; else `dynamic_cast<Creature>` and the jump
  table 0x70E78C on `property − 1`. `SET_PROPERTY` 0x70F380 pops the value, the object and the property; no thing:
  "Object no longer valid"; a script container: the setter run on each of its members through the type's loop
  function; else `fn_0070E820` (`CastCreature`, vtable +0xA4; jump table 0x70F2BC). The creature's body is
  `CreaturePhysical` at +0x160, and every value is pushed as a float (VM type 2). The cases:

  | Property | `GET` | `SET` |
  |---|---|---|
  | STRENGTH (17) | 0x70E081: physical +0x0C | 0x70EBA6: `CreaturePhysical::SetStrength` 0x4F0100 |
  | ALIGNMENT (18) | 0x70E4F0: [+0x168]+0x08 | 0x70EF0C: [+0x168]+0x08, then `Update3DCreatureFromAttributes` 0x477210 and `UpdateMorphing` 0x618C40 on physical +0x58 |
  | WARMTH (26) | 0x70E210: physical +0x10 | 0x70ED56: +0x10, kept to −1..1 |
  | FATNESS (27) | 0x70E26C: +0x14 | 0x70EDBE: +0x14 |
  | ENERGY (28) | 0x70E2C8: +0x1C | 0x70EDF5: +0x1C, kept to 0..1 |
  | ITCHINESS (29) | 0x70E324: +0x20 | 0x70EE59: +0x20 |
  | AMOUNT_OF_POO (30) | 0x70E380: +0x2C | 0x70EE7C: +0x2C |
  | EXHAUSTION (31) | 0x70E3DC: +0x30 | 0x70EE9F: +0x30 |
  | DEHYDRATION (32) | 0x70E438: +0x34 | 0x70EEC2: +0x34 |
  | FIGHT_HEALTH (33) | 0x70E494: `GetCreature3D` 0x477850 +0x4AA8 | 0x70EEE5: `GetCreature3D` +0x4AA8 |
  | SCALE (8) | 0x70DD4C: `GetScale` (vtable +0x120) = `Creature::GetScale` 0x47B190 → `CreaturePhysical::GetUserSize` 0x4EF4F0: the 3D body's +0x90 | 0x70EA76: `SetScale` (vtable +0x124) = `Creature::SetScale` 0x47B160: `LH3DCreature::SetSize` (vtable +8, 0x480530) with the value (0x47B173), then physical +0x6C = the value (0x47B180) |
  | HEIGHT (19) | 0x70E567: a `PuzzleTotem` (`__RTDynamicCast`) gives its +0xE8, an int, as a float; a totem statue (vtable +0x1E4) its `GetWorshipSpeed` 0x738260; anything else `GetHeight` (vtable +0x42C) = `Creature::GetHeight` 0x477F50: the 3D body's +0x90 × 15 (0x8C2C40) | 0x70EA93: a `PuzzleTotem` fn_006DA6D0; a totem statue its town's `SetWorshipPercentage` 0x73C060 (the statue's own 0x738270 without a town), then +0x88 = `GetWorshipSpeed`; anything else `SetHeight` (vtable +0x144) = `Creature::SetHeight` 0x47DE80: s = value × [0x8C9D38] (0x3D888889, the float one fifteenth, multiplied) to physical +0x6C and to the 3D body's +0x90, within no limit |

  - **The clamps** are the only ones: the value is stored, then `fcomp` with the lower limit (C0, so a NaN too) puts
    the lower limit (−1, 0x8AB678; 0 with `ebx`), else a value above 1 (C0 and C3 clear, 0x8AA390) puts 1. Strength,
    alignment, fatness, the other needs and the fight health are stored as given.
  - **Not a creature**, properties 26-33: "Object not a creature" (0xC20964; `SET` at 0x70F287), and `GET` pushes a
    float 0. STRENGTH on anything else: `GET` gives a one-off spell seed's +0x6C (vtable +0xAC), a spell seed's +0x88
    (vtable +0x4C4), a particle container's +0x34 (vtable +0x4AC) or a spell's +0xE4 (vtable +0x400), else
    "Script-Did you want the strength of this" and its life (vtable +0x11C); `SET` the same fields (the particle
    container through fn_0063E3E0), else "Cannot set strength of this". ALIGNMENT on anything else: `GET` gives its
    player's `GetAlignmentValue` 0x64D6A0 (vtable +0x1C), a float 0 without one; `SET` gives a reward (vtable +0x3F4)
    +0x78, else "UNEXPECTED Alignment change".
  - **`SetStrength`** 0x4F0100: +0x0C = the value; then the body's weak-strong morph (physical +0x58, +0xAC) =
    `fn_004F02B0(value, [[+0x54]+0x28]+0x1F4)` = (`CreatureInfo` row's +0x12C (0xC6058C + row · 0x394) · 0.2 + value ·
    0.8) · 2 − 1, −1 at or below −1 (or NaN), 1 at or above 1.
  - **The fight health** (+0x4AA8 of the `LH3DCreature`) is kept outside fights. `LH3DCreature::Init` sets it to 1
    (0x480227, `ebp` = 0x3F800000 at 0x47FDBC); a fight's start sets it to (life + 1) · 0.5 (0x49BD8E, 0x49BE45);
    `ReduceLife` 0x47DD00 and `IncreaseLife` 0x47DE20 change it only while the creature has an opponent (+0x528C);
    the function at 0x480380 takes its argument off it (its callers not read); fn_004845F0 takes a quarter of what is
    lost off the life at the fight's end and leaves it as it is. A mind file of version 30 or later carries it
    (`fn_004E84A0` at 0x4E8BE3); `Creature::Save` / `Load` keep it.
  - **SCALE and HEIGHT.** Both `GET`s read the 3D body's size +0x90, which `LH3DCreature::SetSize` writes kept to
    0.05..4 (in the pen the pen size, 0.22), not the user size physical +0x6C. `SetSize` also writes the matrix factor
    +0x94 = size × 15 / [+0x8C] (0x4805A1..0x4805B1) and, from the size as given, +0x4838..+0x4858, +0x498C / +0x4990
    and +0x5228, then walks the player's interface statuses for the creature's leash (0x4805B7..0x4806A9; not traced
    further). `SetHeight` writes only +0x6C and +0x90: the matrix factor and the rest wait for the next `SetSize`,
    which `ShrinkDownIfNearCitadel` makes every turn (0x4EFE69). The `SET` of SCALE is the only one through
    `SetSize`.
  - In `challenge.chl` (the `PUSHI property; object; CALL 21/22` pairs): on Land 1, `CreatureDevSeeHome`
    (EXHAUSTION), `CreatureDevLearnToEat` (ENERGY), `GuideStayTired` and `GuideImpressTownNextLesson` (EXHAUSTION),
    `CreateGuideCreature` (STRENGTH), `CreatureDevGuideTeachesFight` (EXHAUSTION, FIGHT_HEALTH); on Land 2 the Setup
    scripts above (all ten), and later `CreatureCurse` / `CreatureCurse2`, `MakeCreatureAlignment`,
    `ReturnCreatureBackToNormal`, `HealCombatants`, `FreezeRayStatue` and others.

### What openblack does with the player's creature

- **The profile's file is a setting:** `--creature-file <name>` (`EngineConfig::profileCreatureFile`), default
  `C4ba71b36.erc`, the game folder's first profile's creature, so every run loads the same one; empty for a profile
  without a creature. `CURRENT_PROFILE_HAS_CREATURE` answers whether that file exists.
- **`LOAD_MY_CREATURE`** (`ECS/PlayerCreature`): nothing when the player leads a creature already (the leash
  system's `PlayersCreature`); else the file through the mind cache, its species, size, alignment and strength
  (`CreatureMindFileBody`), and `CreatureArchetype::Create` at the middle of the point's cell on the ground. The
  fatness starts as the species does (which known field of the file holds it is not checked). The creature faces π, as
  the script-made creatures do (inferred). The fizz is not ported. Our creation's draws are our mind's (the 40 initial
  desires), not the original's list above.
- **`LOAD_CREATURE`** (`ECS/PlayerCreature`, `ScriptLoadCreature`): the pure `PlanScriptLoad` takes the species from
  the script's type as a row of the creature tables (`SpeciesFromRow`: 0 is the Giant Ape; a type out of 0-16 is
  refused, where the original indexes its table unchecked), the body from the file as `LOAD_MY_CREATURE` reads it but
  with the file's own species row ignored (`creature_mind_body::FromMindFile(file, species)`), and x and z through the
  fixed point (`map_coords::Quantise`), on the ground. A player who leads a creature already gets the original's error
  line and the load goes on. The file comes through the mind cache; one that is missing or that our reader refuses
  makes no creature. The creature is made by the same step as `LOAD_MY_CREATURE`'s (`CreatureArchetype::Create`, the
  same draws, facing π), then `SettleScriptLoaded`: `SetLeashable` (+0x110C: it becomes the one its player leads,
  displacing any other), `SetKnown` of the rope (+0x1110), evil and good (+0x1114) leashes, and stage 13. It is not
  held by the script (the native adds no script thing). The script player goes through `ScriptPlayerToGamePlayer`.
- **`SET_CREATURE_NAME`** (`ECS/PlayerCreature`, `SetName`): the help text of the id (`helptext::Get`, entry 0 out of
  range) becomes the mind's name (`Learnt::name`). Our mind takes up its file at its first turn, not as the creature is
  made, so a name given before then waits in `CreatureMindState::scriptName` and is put over the file's once the mind
  has taken it up (the original's order: the file first, then the name). No thing: nothing; anything else:
  "Thing not creature!".
- **`CREATURE_AUTOSCALE`** (`ECS/PlayerCreature`, `SetAutoscale` and `Autoscale`; the pure step
  `creature_size::AutoscaleStep`): the native puts `components::CreatureAutoscale {enabled, factor}` on a creature; no
  thing, or anything but a creature: "Thing should be valid!" and nothing (the original writes the two fields into
  whatever thing it is; a non-creature has no such fields here). Each creature turn, right after the creatures' spells
  step (`spell_creature::ProcessTurn`, the original's order: the spells, then the step), `Autoscale` steps every
  creature that has it on: the local player's creature (`creature::LocalPlayer`, the leash system's `PlayersCreature`)
  gives its size before any size spell (`creature_spells::SizeBeforeSpells`), the step starts from the creature's own
  size now, and the result goes through `creature_spells::SetSizeBeforeSpells` (the size big, else small, puts back as
  it ends, else `Creature::size`). The creature the hand holds (`CreatureHandSystemInterface::GetCreature`, an
  approximate stand-in for the locked select) is skipped. openblack has no multiplayer game, so that test is always
  passed. The walk is on the const registry, so without the native no storage is made.
- **`CREATURE_SET_KNOWS_ACTION`** (`CreatureMindSystemInterface::SetKnowsAction`; the pure
  `creature_watching::SetKnows` and `TaughtSightings`): a skill or a miracle becomes known (`Knowledge::skillsKnown` /
  `miraclesKnown`) or no longer known; a miracle taught also gets its sightings set to the float product of its times
  to see and the species' multiplier (`TimesNeeded`, the same one a sighting is measured against), cut towards zero.
  Our mind sets itself up and takes up its file at its first turn, so what a script teaches before then (or while a
  file waits) is kept, in order, in `CreatureMindState::scriptKnows` and put over the file's once the mind has taken it
  up, as the name is; a mind saved meanwhile is saved with it. No thing: "No creature for script"; anything but a
  creature: "No script for creature" and nothing changed.
- **`CREATURE_FORCE_FRIENDS`** (`ECS/PlayerCreature`, `ForceFriends`; the pure `AddFriend`): the native's messages as
  the original's; two creatures with the flag on each get the other once in `components::CreatureFriends::friends`
  (made when first needed), at the front, `b` into `a`'s list first; off, nothing. Nothing reads the list yet, so it
  changes nothing else.
- **`GET_PROPERTY` / `SET_PROPERTY` on a creature** (`ECS/PlayerCreature`, `GetCreatureProperty` /
  `SetCreatureProperty`; the pure `ClampProperty`): strength, alignment and fatness are `Creature::strength`,
  `alignment` and `fatness`; warmth, energy, itchiness, amount of poo, exhaustion and dehydration are the body's needs
  (`creature_physiology::Needs`), read and written through `CreaturePhysiologySystemInterface::NeedsOf` / `SetNeeds`:
  before the body's first turn `NeedsOf` gives what the species starts it with, and `SetNeeds` marks the body started
  so that its first turn keeps a script's values (the original sets them as the creature is made). The fight health is
  the fighter's (`CreatureFighting::fighter.health`) in a fight, a script's set included, else
  `components::CreatureFightHealth` (made by a script's set outside a fight, or by `CreatureFightSystem::Leave` from
  the fighter's health as the creature leaves any fight, ended or not, as the original's one field stays), else 1.
  Warmth is kept to −1..1 and energy to 0..1 in the original's order (a NaN gives the lower limit); the rest as given.
  The body shows a new strength or alignment from the drawing's next refresh of its morph
  (`creature_morph::FromAttributes`, the same weighting as `SetStrength`), not in the native itself. No thing: "Object
  no longer valid" (`SET`) / "Thing no longer valid" (`GET`, 0); anything but a creature, 26-33: "Object not a
  creature" (and 0); STRENGTH and ALIGNMENT on anything else are not ported (logged once, `GET` gives 0). Only the six
  needs go through the physiology service: a creature whose body has no needs (no service, or no `CreatureNeeds`)
  logs an error for them and `GET` gives 0, which the original, where every creature has its body, never does.
- **SCALE and HEIGHT on a creature** (`CHLApi`; `player_creature::SetCreatureScale` / `SetCreatureHeight`, the pure
  `SizeForHeight`): `SET` SCALE draws the body (its `Transform`) at
  `CreatureArchetype::DrawnScale(species, creature_morph::ClampScale(v))` and then sets `Creature::size` = v, as
  given; `SET` HEIGHT sets `Creature::size` = h × `k_SizePerHeight` (the float one fifteenth, multiplied: 45 gives
  3.0000002, not 3) and leaves the `Transform` as it was (the original's drawn matrix waits for the next `SetSize`).
  The `GET`s on a creature are not ported (SCALE logged once as "GetProperty (Scale)", HEIGHT as the generic
  "GetProperty"; both give 0): the original reads the 3D body's size, which ours does not yet keep apart from
  `Creature::size` (see [Pending](#pending)). SCALE on a villager or an animal stays the `Transform`'s scale; `SET`
  SCALE on anything else is not ported ("SetProperty (Scale)"), `SET` HEIGHT on anything but a creature falls to the
  generic "SetProperty" line, as before. The fidelity, cycle and hand-demo logs have neither a "SetProperty (Scale)"
  nor a generic "SetProperty" line, so no run reaches either `SET` on a creature (among the scripts that would,
  `CreateGuideCreature` and `CreatureDevGuideTeachesFight` set the Guide's SCALE on Land 1).
- **The natives:** `CALL_PLAYER_CREATURE` gives the creature the player leads; `SET_CREATURE_HOME` writes the leash's
  home (`LeashSystemInterface::SetHome`), x and z through the 16.16 fixed point, on the ground; `SET_CREATURE_DEV_STAGE`
  sets the mind's development phase, and the desires follow in the mind's next turn (the original at once);
  `DEV_FUNCTION` 2 is `SetKnown(Rope)`, 3 is `SetKnown(Evil)`, `SetKnown(Good)` and `SetLeashable` (+0x110C read as the
  creature its player leads); the other values are not ported; `CREATURE_IN_DEV_SCRIPT` sets `Creature::inDevScript`.
- **The home:** each creature turn starts with `FollowTemplePens` (`ECS/PlayerCreature`, called from
  `ECS/CreatureLoop`): while its player's temple is built, every creature's home is the temple mesh's pen point
  (`TemplePenPoint`, special point 15), through the fixed point and on the ground. `TempleCreatureHome` (where a
  knocked-out creature is carried) takes the same point; it took the mesh's first point before.
- **The pen:** `ShrinkInPens`, right after `FollowTemplePens`, writes the drawn scale to
  `components::CreatureDrawPose::scale` and the pen size to `CreatureDrawPose::size` (`BetweenPenWalls`,
  `PenDrawnSize`), none outside a pen; `ecs::DrawnModel` draws a creature at it, its eyes, hair and footprints
  following ([Drawing between turns](#drawing-between-turns)). `creature_pose::DrawnSize` is the one reader of the
  size it is drawn at (the pen size, else `Creature::size`). The locomotion takes its walk and run speeds from it and
  its stride, its step-or-walk choice and its steps' travel from the drawn scale (`creature_pose::DrawnScale`), as
  SetSize's +0x4844, +0x4848 and +0x94 ([Everything SetSize derives](#the-players-creature)); `SpeedsFor` takes the
  size unclamped, as SetSize's f. The radius it keeps clear by stays its own size's. Also from `DrawnSize`, as the
  original reads +0x90: every animation's playback rate (`creature_layers::PlaybackRate`, 1.6 − 0.425 s: the body's
  layers in `PoseBody`, the turning, the object actions), the breath period (5·√s), the sound size key, the leash
  lengths, the creature's `object::GetScale` and `GetHeight` (s × 15) and through them every caller of those, the
  hand's touch (its capsules and its height), the follow camera's height, the hair's scale and the footprint's side.
  The duel too, as the original's attack choice and its fight action step read +0x90 and +0x94 (addresses in
  "Pending", "The pen size's readers"): its blows' reaches and heights are measured at the drawn scale, a step's length
  and an action's move take the drawn scale, a blow may land at most 7.5 of the attacker's drawn size deep, the
  opponent's height (which blows reach it, and how high on it one lands) is 15 of its drawn size, a blow lands within
  0.2 × 15 of the attacker's drawn size of the opponent's body, drawn at its drawn scale, and the damage goes by the
  two drawn sizes. The head still looks from its own size's height (the original's look origin is not read).
  `Creature::size`, the Transform and the state hash's size keep the real size; their readers (the fight's arena, its
  places, the distance a fight is picked from, the faint's length, the leash's anger reach, the head's look height) do
  not see the pen size (the hand touches the drawn body, so it touches a smaller one there). The distance is taken from
  the creature's turn position, not its drawn one.
- **Debug window:** Debug > Windows > Creature spawner (`Debug/CreatureSpawner*`, raffclar's window on our creature
  systems, its pure parts in `CreatureSpawnerModel`): spawn, the creatures and the selected one's looks, audio,
  movement, hands, mind, learning, body, leash and fight; and at the top the player's creature (real and drawn size,
  phase, home, leash, desires and plan, needs, alignment, the hand), with "bring to the hand", "set stage" and the
  leashes. His file dialogs, rope drawing, leash posts and editor hosting are not ported. Closed, it does nothing.
- **The "creature" part of the state hash:** each creature's owner, species, leashable, inDevScript, body, size,
  development phase and home.

## Fights: the camera

Read in `runblack.exe` W120 (the creature classes have no bodies in bw1-decomp; the field names are those of
`src/Black/CameraModeNew3.h`). The camera's own side (the flight, the following, the help bits) belongs with the
camera's port.

- **There is no fight camera mode.** The player's camera `CameraModeNew3` has a fight state (`HasFight` +0x44, the
  arena +0x48, `FightDistance` +0x58, `FightTimeLeft` +0x5C, `FightStatus` +0x64).
- **The creature's fight start** (an unnamed function at 0x5036C0, inside the wrongly sized symbol
  `SubStatePerformAddVillagersToDance`): once `LH3DCreature` fn 0x484410 has taken the arena's position and radius,
  it makes reaction 0x13, counts the fight (+0x178), and asks the camera: `GGame::GetCamera` 0x5037D8, its current mode
  `dynamic_cast` to `CameraModeNew3` (0x5037FB). Only the player's own mode calls `StartFight(GArena*)` 0x45A4D0
  (0x50380A); any other mode (a script's) does nothing, not even the remark. There is no owner test there.
- **`StartFight` 0x45A4D0:** nothing when the arena or either fighter (+0x38, +0x3C) is null (0x45A4DD..0x45A4F3).
  It sets `HasFight` and the arena, then a local "watch" flag = 1, cleared when
  `WantToQuitFight(camera position, screen centre +0xF0, 1.0)` 0x45A390 is true (0x45A539). It **watches** when
  (either fighter's `GetPlayer` (vt 0x1C) is the local player's, and either fighter has `Flags` +0x24 & 0x400,
  controlled by script) or the watch flag is still set, that is the camera already looks at the arena
  (0x45A53D..0x45A5AC). Watching sets up the camera's fight state and flies to the arena (0x45A5B2..0x45A71D).
  **Not watching** (0x45A72A..0x45A7B7) clears `HasFight` and the arena and, when `MyInterfaceStatus` is set and a
  fighter is the local player's (the first fighter tested first), plays the spirits' `CreatureFight` remark
  fn_0071CD40 (help list 12, [audio.md](audio.md)) for it; fn_0071CD40 does not use its creature argument. The camera
  does not move.
- **What openblack does:** `creature_fight::ViewAtFightStart` is the rule; `CreatureFightSystem::StartFight` asks it
  through the agreed `camera::FightWatch` that the player's camera model gives (`CameraModel::GetFightWatch`, null for
  every other model) and either calls `FightWatch::StartFight` with the two fighters (the one that made the arena
  first), the arena's middle on the ground and its radius, or plays `audio::guidance::RemarkCreatureFight`. Not while a
  script camera mode is on or the script has the screen (`interface_active`), which stands for the failed
  `dynamic_cast`. The fight system no longer moves the camera itself: the flight to the arena and the following of the
  fighters are the camera's (0x45A5B2.., `Update` 0x45E37E..0x45E8D6). The debug window's camera switch keeps only
  the watching off.
- **The arenas the camera asks for.** Besides the arena `StartFight` is given, the camera reads the game's list of
  arenas (head g_game +0x205C7C, next +0x48, count +0x205C80) when it lingers over one or is double clicked onto one
  (0x45BB64.., 0x45DFC5..), and each frame asks its own arena `IsAvailable` (vt +0x2C, 0x45A9B1..0x45A9CA). Of an
  arena it reads the fighters (+0x38 the creature that made it, +0x3C its opponent, set by fn_00424AB0 at
  0x424AC7..0x424ACE), `GetPos` and `GetRadius` (vt +0x60, 0x424780: +0x30). A new arena (fn_00424820, made by
  fn_00424E70 from the creature's fight start at 0x507566 and from `GScript::GetArena` 0x6F4C36) is pushed at the
  list's head, so the list runs newest first. On the creature's path the arena's position is a MapCoords of the two
  creatures' middle, (FtoL(midX × 6553.6), FtoL(midZ × 6553.6)) with **altitude 0** (0x5074B1..0x50750C): `GetPos`
  gives the middle quantised to a map position, and the land's height there.
  **What openblack does:** `CreatureFightSystemInterface::GetArenas` lists the fights now on, newest first, each as an
  `ArenaView` (maker, opponent, middle, radius), from the `CreatureFighting` of the creature that made the arena
  (`madeArena`), read through the const registry. The view's middle is `map_coords::Quantise` of the float middle
  `MakeArena` keeps (ToFixed then ToMetres), the position as the original's arena stores it; its height is left to the
  reader (the land under it, as the altitude is 0); `ArenaOf` gives the arena a creature made or was called into. The
  order comes from `creature_fight::Arena::serial`, counted by the fight system as each arena is made. Nothing calls
  them yet (the camera will); the count is only written where an arena is made, and no part of the turn state hash
  reads `CreatureFighting`'s contents, so the verification runs are identical.
- **The end of a fight.** fn_0045A800 starts the camera's 3 s linger (if `FightStatus` is 0: `FightTimeLeft` = 3000
  when ≤ 0, `FightStatus` = 1; so a second call is harmless). The creature side calls it, after the same
  `dynamic_cast` to `CameraModeNew3`, from:
  - **fn_004845F0** (0x48466D), the `LH3DCreature`'s end of a fight: it clears the opponent (+0x528C), calls the
    linger with no owner test when the fight state +0x4994 is 0x16, 0x17, 0x18, 0x1A, 0x1B, 0x1D, 0x20, 0x21 or 0x22
    (start, stance, action, the block's start, block and end, the three cast states; not finish, the block recoil,
    faint or get up), and takes `(1 − fight health +0x4AA8) × 0.25` off the creature's life (vt 0x5B0). That last part
    is openblack's `EndFightFor` (`creature_fight::LifeAfterFight`), so `EndFightFor` tells the camera
    (`FightWatch::EndFight`) when `creature_fight::CameraLingersOnEnd` holds for the fighter's state.
  - **the "Fight" action's running sub-state 0x503830** (its sub-states are set up at 0x4F9FC4..0x4FA03A: 0x503600,
    the start 0x5036C0, this one, 0x503A50), at 0x5038A6, reached three ways, each time only for the local player's
    creature: (a) its body action is over (0x503842); (b) the opponent's life (vt 0x11C) is gone and it is not
    script-controlled: the `GeneralGood` remark fn_0071CDF0, reaction 0x28, a win (+0x17C), fn_004845F0, then the
    linger (0x503A1F); (c) its `LH3DCreature` has no opponent any more (+0x528C = 0, 0x5039B6): the `GeneralBad`
    remark fn_0071CD70, fn_004845F0, then the linger. Only (c) is matched in openblack: a duel whose opponent has gone
    or left the duel (`ProcessDuels`) calls `EndFightFor` and then, for the local player's creature, `EndFight`.
  `EndFightNow` is never called by the fight.
- **Can it happen on Land 1 or Land 2?** No. A fight needs two creatures, and on those lands the only creature is the
  local player's, made by `LOAD_MY_CREATURE` (`player_creature::LoadMyCreature`, never a second one); the other makers
  are the playground scripts' `CREATE_CREATURE_FROM_FILE` and the debug spawner. So the verification runs are
  identical.

## Fights: miracles, a tornado and fainting

Read in `runblack.exe` W120. The `LH3DCreature` fight states (+0x4994) are those of openblack's `creature_fight::State`
from `Start` = 0x16 on, in the same order (`Stance` 0x17, `Action` 0x18, `BlockStart` 0x1A, `Block` 0x1B,
`BlockRecoil` 0x1C, `BlockEnd` 0x1D, `Faint` 0x1E, `CastStart` 0x20, `Cast` 0x21, `CastEnd` 0x22): every test below
fits that order (inferred from the fit; no table names them).

- **Blocking** fn_0048A580: state 0x1B; 0x1A once its time (+0x47D0) is past half of animation 0x8E (142, start
  block); 0x1D while before that half. It is `creature_fight::IsBlocking`, asked through
  `CreatureFightSystem::IsBlocking`.
- **A miracle's blow in a fight**, in `Creature::ApplyEffect` 0x478C80: a harmless effect from anything but the
  creature itself or its own player does nothing to a fighter (0x478CA9..0x478CC6); harm from itself does nothing
  (0x478CD5). Fighting, unless [0xC6414C] or [0xC6414D] (not named) is set: blocking, the effect's numbers are taken at
  0.1 (0x478D24); then fn_00484790(184) (0x478D3F); a heal goes to the fight health +0x4AA8 (clamped to 0..1) and
  `TimeWarpHeal(heal × 9600)`, harm comes off it, and at 0 or below the creature faints, its opponent (+0x528C) being
  made to want to show off (desire 0x18, action 0x27, or 0x13 when its `LH3DCreature` +0x9C, read as the alignment, is below −0.5) and counting a win (+0x17C).
- **The reel** fn_00484790(animation): nothing while fn_00489CB0 holds (the fight animation +0x49A0 in 179..193, the
  reels, or state 0x1C or 0x1E); blocking, animation 198 and state 0x1C; in state 0x17, 0x18, 0x20, 0x21 or 0x22 the
  animation given and state 0x18 (jump table 0x48481C / 0x484824); otherwise nothing. It is
  `CreatureFightSystem::Recoil` (animation 184, `k_RecoilMid`, as `ApplyEffect` passes).
- **Fainting** `Creature::Faint` 0x476FA0: nothing if fainted already (+0x384); the spells on it brought to their end
  (fn_004F4B60 on `CreatureReceiveSpell` +0x370: each of the 16 slots' +0x10 = 0, and +0x8 = 0 for one in phase 3,
  holding; true when none is left), `EndAnyAnimsRapidly`, `StopMoving`, the leash let go (`GLeashStatus::SetOn`), a
  forced plan (desire 0x18, action 9), mind +0x1C14 = 1, fainted = 1, fn_004C9FE0(0x25, 3, 0, 0, 0). There is no fight
  code in it. Its callers: `DestroyedByEffect` 0x476F7A, `ApplyEffect` (0x478EF2, 0x478F06), fn_0047CF00 (three),
  `ReduceLife` 0x47DDF5, and fn_00477060, a jump to it that the tornado's pick-up fn_006D21B0 calls for a creature
  (0x6D2464).
- **What openblack does:** `IsBlocking`, `Recoil` and `ForceFaint` are on the fight interface (raffclar's names);
  `ForceFaint` is `Creature::Faint` from outside a fight's loss: fainted already, nothing; a fight it is in is left
  with no winner, then it faints as the knocked-out do. Every faint now brings the spells on it to their end
  (`creature_spells::TryFinishAll`, fn_004F4B60). Nothing calls `IsBlocking`, `Recoil` or `ForceFaint` yet: the
  miracle's side (`ApplyEffect`, inventory R22) and the tornado's (R25) are not in our tree, and they call them in
  their own commits. `IsCameraOnFight` answers from the player's camera (`FightWatch::IsWatchingFight`, the
  original's `HasFight`); its reader is the camera's port. On Land 1 and Land 2 no creature has a spell on it, so a
  faint there is as before.

## The creature's radius

`Creature::GetRadius` 0x4792C0 (vt +0x60) and `Creature::Get2DRadius` 0x477F40 (vt +0x64) both return the float at
`LH3DCreature` +0x5228 (through `[[creature+0x160]+0x58]`), unscaled. `LH3DCreature::SetSize` 0x480530 is its only
writer:

- s = 0.05 if `!(size > 0.05)` (NaN too), else `min(size, 4)` (0x480563..0x480593); +0x90 = s;
- +0x94 = `(s × 15) / +0x8C`, the draw scale (0x4805A1..0x4805B1);
- +0x5228 = `+0x94 × +0x5224` (0x480642..0x48064C).

So **radius = ((ClampScale(size) × 15) / restHeight) × reach**, each step rounded to float. The size is the one SetSize
last got: in the temple's pen the pen size `ShrinkDownIfNearCitadel` passes it each turn (0x4EFE69), else the
creature's own. `LH3DCreature::GetNavRadius` 0x480A60 is the same at the unclamped size: `+0x6C / +0x90 × +0x5228`.

**+0x8C, the rest height.** Set once per load by `Morphable::LoadBase` 0x618360 (0x618602), called through vt +4
(`LH3DCreature::LoadBase` 0x4EAC90) from `Morphable::ReadBinary` 0x617AE0: the return of `LH3DAnim::SetTransform`
0x83A1D0 over the base mesh's (+0xB8) bones, i.e. the y extent, from 0, of the rest pose's bone origins. Init's 1.0
(0x47FDC7) is overwritten before any use; `LoadMesh` and `SelectMesh` drop SetTransform's return. This also settles
the draw scale `size × 15 / rest height`: no other factor and no mesh scale.

**+0x5224, the reach.** Set once per load by `fn_0048F5B0` (from `LH3DCreature::LoadBinary` 0x4EBD32), after
`SelectMesh(0)` (0x4EBCBB): the largest `sqrt(x·x + z·z)` (`fn_006E8160`) over the bones of the stand animation's
keyframe 0 (`GetAnim(0, 0)`, the spec file's first animation), posed under the identity; kept when the kept value is
below the new one, from 0. 0 without bones or without the animation. How the pose is made:

1. **The rest bones.** `SelectMesh(0)` calls `MorphAnims` (vt +0xC, 0x48D790 → `Morphable::MorphAnims` 0x619100). It
   composes the base mesh's bones (`SetTransform`, through `fn_007FAFF0` 0x7FAFF0), blends them towards the two
   variant meshes by the morph weights +0xA0 / +0xA8 (0 at load, from `MorphInit` 0x6173A2 / 0x6173AE), turns them
   back into parent-relative bones (`fn_0083A020`: each times `SetInverse` 0x7FB290 of its parent, last bone first)
   into mesh 0, then composes mesh 0 again into +0x47F4 and inverts each into +0x47F8 (0x61946F, 0x619497).
2. **The stand keyframes.** Mesh 0's stand is built by `CAnim::CAnim` 0x85EF40 (from `GetSetAnim` 0x6196DC..): each
   keyframe's Euler angles become a matrix (`SetYXZMatrixOnly` 0x7FAC10), the three animations' matrices are blended
   by the same weights, and `GetYXZ` 0x7FAB30 (`ArcTanOctant` 0x7FA990) turns the result back into angles.
   Translations are blended the same way. At weight 0 the values are the base's, through those round trips.
3. **`fn_00860E00`** (time 0, so keyframe 0 and fraction 0; no displacement, since argument 7 is 0): per bone, the
   keyframe matrix, its rows normalised by `InverseSquareRoot` 0x841170 (`fn_007FB5C0`), times the bone's rest
   rotation (+0x47F4), times the parent's inverse rest (+0x47F8) unless it is bone 0; the translation is the
   keyframe's. Rest × keyframe adds every cell as `(a2·b2c + a1·b1c) + a0·b0c` (0x8610F2..0x8612B9); × the parent's
   inverse takes rows 0 and 1 in `fn_007FAFF0`'s orders and row 2 in the first order for all three columns
   (0x8612C2..0x861453).
4. **`fn_00839F10`** chains each bone under its parent (`fn_007FAFF0`), the root under the identity.

`fn_007FAFF0`'s orders, per row i (0x7FAFF3..0x7FB177): column 0 `(ai2·b20 + ai1·b10) + ai0·b00`, columns 1 and 2
`(ai0·b0c + ai2·b2c) + ai1·b1c`; the translation row the same, then `+` the parent's translation.

All of it runs at the game thread's 24-bit precision control ([audio.md](audio.md#the-games-fpu-runs-at-24-bits);
`GetSetAnim` sets it again at 0x6196C7). The row normalisation is not exact: `InverseSquareRoot(1)` is 0x3F7FFFA0
(1 − 5.7·10⁻⁶), the same just above 1, and just under 1 it is about 1 − 2.3·10⁻⁵, so each row comes out a little
short and the shortfall compounds down each chain. That makes the reach about 5·10⁻⁵ smaller than an exactly
normalised pose. The same `fn_00860E00` poses the drawn creature (`LH3DCreature::UpdateBuffers` 0x4EC6C3..) and the
hand.

**The values**, emulated operation by operation from the game's files (`Data/CTR/*.CBN`, `Data/CreatureMesh`), in
float, with the hex bits:

| Species | Base mesh | Bones | restHeight (+0x8C) | reach (+0x5224) | radius at size 1 (+0x5228) |
|---|---|---|---|---|---|
| Ape, chimp, gorilla, mandrill | c_ape_Boned_Base, … | 76 | 48.2218704 (0x4240E332) | 21.3905067 (0x41AB1FC2) | 6.6537776 (0x40D4EBBF) |
| Lion, leopard, tiger | c_lion_base, … | 51 | 79.4374542 (0x429EDFFA) | 24.5850506 (0x41C4AE2F) | 4.6423411 (0x40948E0F) |
| Wolf | c_wolf_base | 51 | 79.4374542 (0x429EDFFA) | 24.5858402 (0x41C4AFCD) | 4.6424899 (0x40948F47) |
| Bear | a_bear_boned_base | 56 | 54.2245331 (0x4258E5EC) | 18.5060635 (0x41940C6B) | 5.1192870 (0x40A3D133) |
| Polar bear | c_polar_bear_base | 56 | 54.2245293 (0x4258E5EB) | 18.5060616 (0x41940C6A) | 5.1192870 (0x40A3D133) |
| Tortoise | c_tortoise_base | 60 | 48.1902847 (0x4240C2DA) | 22.1840019 (0x41B178D6) | 6.9051270 (0x40DCF6CD) |
| Cow | c_cow_boned | 48 | 47.9534340 (0x423FD051) | 14.9851294 (0x416FC317) | 4.6874003 (0x4095FF2F) |
| Rhino | c_rhino_base | 48 | 47.9534225 (0x423FD04E) | 14.9851294 (0x416FC317) | 4.6874013 (0x4095FF31) |
| Sheep | c_sheep_base | 48 | 47.7736053 (0x423F182C) | 15.0045691 (0x417012B7) | 4.7111483 (0x4096C1BA) |
| Horse | a_horse_boned | 48 | 55.5035248 (0x425E039C) | 13.2293262 (0x4153AB52) | 3.5752664 (0x4064D12A) |
| Zebra | c_zebra_base | 48 | 56.0161438 (0x42601088) | 13.2293253 (0x4153AB51) | 3.5425479 (0x4062B91B) |
| Ogre | a_greek_boned_base | 63 | 72.9344940 (0x4291DE76) | 10.9056492 (0x412E7D8A) | 2.2428994 (0x400F8BAA) |

At any size s the radius is `r(r(ClampScale(s) × 15) / restHeight) × reach`, which is not always `ClampScale(s)` times
the last column to the bit. Every species' stand rotates and translates every bone, in order, and only bone 0 is a
root, so no bone takes a default. The values do not change when every `fsin`/`fcos`/`fpatan` result is moved by one
64-bit ulp, nor when they are rounded to double. **(inferred)** that nothing between `SelectMesh(0)` and the reach
(`GetSetAnim(0, 1..5, 0)`, `SetAnimTime(0x10..0x1B, 250)`) touches mesh 0's stand, and that no other store sets the
morph weights before it. Not checked against a memory read of the running game.

**In openblack.**
- `creature_morph::ClampScale`, `RestHeight` and `DrawnScale` (`src/Creature/CreatureMorph`) are the clamp, +0x8C and
  +0x94; `creature_morph::Reach` is `fn_0048F5B0`'s maximum, and `creature_morph::Radius` is +0x5228 in SetSize's
  order.
- `creature_cast_moves::BoneReach(stand, parents, restLocals)` (`src/Creature/CreatureCastMoves.cpp`) makes the pose of
  steps 1 to 4 in float, in the original's orders, from the base mesh's bones relative to their parents
  (`L3DMesh::GetBoneLocals`) and the base stand (`CreatureRig::GetAnimation(Base, k_StandAnimation)`): the rest's
  compose, `affine::Inverse` (the port of `SetInverse`) and compose again; the angles through `affine::RotationYXZ` and
  `affine::DecomposeYXZ`; `affine::NormaliseRows`. The lerp to frame 1 at fraction 0 is left out: it gives keyframe 0
  back, signs of zeros aside, which no later step can tell apart. A bone the stand does not key keeps no turn and no
  move; the original's default frame for it (+0x47FC, argument 8) is not read, and no species has such a bone.
- `CreatureArchetype::BodyMetrics` measures the species' base mesh and stand once, when `Create` makes the creature,
  into the `CreatureBodyMetrics` component (rest height and reach), as the original measures once per load. Both are 0
  without the mesh, the reach 0 without the stand.
- `object::Get2DRadius` and `object::GetRadius` (`src/ECS/ObjectMetrics.cpp`) give a creature
  `creature_morph::Radius(drawn size, restHeight, reach)`. The drawn size is `CreatureDrawPose::size`, the size
  `player_creature::ShrinkInPens` draws it at in its pen, or else `Creature::size`. A creature without the component
  (not made by the archetype) keeps the generic mesh-box formula.
- The rest height over `L3DMesh::GetBoneMatrices`, whose parents are put on with glm (not the original's order for the
  translation's sum), has the original's bits for every species, and an emulation of `BoneReach`'s float operations with the same C
  runtime's sine, cosine and arctangent (in double) gives the table's reach bits for every species. `test_creature_rigs_data` pins all three columns to the bit
  with the game's data, through `CreatureArchetype::BodyMetrics` over the base meshes loaded as the game loads them,
  and checks that every species' stand keys every bone of its base mesh; the pure parts are tested with made-up bones.

## The body posed for the turn

Read in W120 (runblack.exe, BW1W120 symbols). The game's logic reads a creature's bones from a skeleton posed once a
game turn, not from the frame being drawn.

**Who poses, and when.** `LH3DCreature::UpdateTime` 0x481DF0 (vt +0x14) is called on a game creature's body only from
`Creature::ProcessState` 0x473939, so the body is posed once per game turn, at the end of the creature's turn: after
`ProcessSpells` (0x472EBB, which reads the hand and its mirror of the pose before), `fn_004EFCE0` (`SetSize`), the
leash, the look, the plans and `ProcessSubAction`, gated by [0xD00DE4]. Its argument is
`ftol(physical+0x48 × [0xD01A38])`, the turn's ms. A held creature (+0x24 bit 0x10, +0x3CC == 1, not
`IsPerformingBodyAction`) goes straight there after the freeze below; +0x3CC == 2 calls `fn_00477D70`, which returns 0.

**The step** (0x481E01..0x481E7C): +0x48BC = the ms when +0x4A90 is set, else
`ftol((1.6 − (size·0.5)·0.85)·ms)`, every constant a float, each operation rounded to a float, `__ftol` cutting
towards zero; then doubled (`shl 1`) when +0x5730 is set. The bounding-sphere and mesh-intersect caches (+0x5274,
+0x57A8) are cleared here and filled by whichever reader comes first afterwards; a reader inside `StateAction`, before
the pose, fills them from the pose before.

**Inside `UpdateTime`**, in order: the sparkles and blood, the sway spring and `UpdateLook` read the pose before;
`StateAction` 0x486A00 sets the slot animations and times (its state handlers `fn_0048A5E0`, `fn_0048BAB0`,
`fn_0048C6C0`, `fn_0048CEB0` and `fn_0048F200` read the pose before); then `UpdateBuffers` 0x4EC590 poses; then the 8
destruction points, a carried creature (put at the leaf mean of this one's hand, then posed itself) and the physics
sphere read the new pose.

**`UpdateBuffers`** rotates the buffers: +0x5174 (the previous pose) := +0x5178, the work goes into +0x517C, and at the
end +0x517C and +0x5178 swap (unless +0x5218, which nothing ever sets: `fn_00480C50`, its only writer besides `Init`,
has no call, jump or pointer anywhere in the image). The first time (+0x51DC == 0) +0x5178 is also copied into +0x5174.
The pose is built in world space under the body's matrix (`fn_004EBE10`). `ResetLook` 0x483269 re-poses through
`UpdateBuffers` too, from `ReconnectToGame`, `ForceMoveMapObjectWithoutWalking` (which then copies +0x5178 into
+0x5174), `FallingSpell::Init`, `fn_00490960` (inside `StateAction`, after a `SetPos`) and the function at 0x4912F0: a
creature can be posed more than once in a turn, and every pose moves the current pose to the previous.

**The frozen copy.** `fn_004806E0` (from `fn_00477D00`, the held branch of `ProcessState`, before the pose) sets
+0x5270 and copies +0x5178 into +0x5180; `GInterface::PlaceObjectInMagicHand` 0x5DA9CA copies it again on every
pick-up. `ReconnectToGame` 0x480730 re-poses and clears +0x5270. `GetSafeBuffer` 0x4842B0 is +0x5180 while +0x5270 is
set, else +0x5178; while frozen, +0x5178 keeps being rebuilt each turn, from one action only.

**The drawn body.** `LH3DCreature::PrepareForDrawing` 0x4ED320, every frame: f = min(turn fraction, 2.0) (the double
constant 2 at 0x4ED33A); the drawn bones +0x47F0 = `fn_004EECD0`(+0x5178, f, first) then (+0x5174, 1 − f, add):
f·current + (1 − f)·previous, float by float, in world space, not normalised again. `fn_004EECD0` is `dst = w·src`
(first) or `w·src + dst`, per float, over bones·12 floats (the count at 0x4ED340 is bones·3·4): the 3×4 matrices,
which have no bottom row. openblack's 4×4 matrices blend the same twelve and keep the bottom row (0, 0, 0, 1).

**The hand's leaf mean** (`fn_0083A0E0`, used by `FillAverageHandPos` 0x48CC80 and for the carried creature): from the
start bone along its following siblings, a bone without children adds its translation to the sum (rounded to a float
after each add) and is counted, a bone with children recurses into its first child; the sum times 1 / count. The
mirror bone is the file's table (`GetMirrorBone` 0x481390, +0x51F0: the creature block's last number for each bone).

### What openblack does with the turn's pose

- `ecs::components::CreatureTurnPose` (`src/ECS/Components/CreatureTurnPose.h`) on the creature: `current` (+0x5178),
  `previous` (+0x5174) and `frozen` (+0x5180 while +0x5270 is set). Empty bones are "no pose yet": a reader then has
  no bones, as a spell then keeps its old hand position.
- `creature_turn_pose` (`src/Creature/CreatureTurnPose.h`), pure: `Step` (the step above), `Advance` (once per pose:
  previous = current, current = the new one, and the first time previous too), `Freeze` / `Reconnect`, `Safe`
  (`GetSafeBuffer`), `BlendInto` (`fn_004EECD0`), `Drawn` (`PrepareForDrawing`'s blend, f taken up to 2), `InWorld`,
  `MirrorBone` (the file's table, `CreatureRig::fileTail`) and `LeafMean` (`fn_0083A0E0`; openblack's skeletons put a
  bone's first child at the lowest index and its siblings in index order). Tested with made-up bones
  (`test_creature_turn_pose`).
- `CreatureAnimationSystemInterface::PoseTurn`, called last in `creature_loop::ProcessTurn` (after the miracles and the
  autoscale, which change the size, as `ProcessSpells` and `SetSize` come before the original's pose), fills `current`
  through `Advance` with the bones the animation system last posed (`CreatureAnimation::boneMatrices`, mesh space),
  placed by the creature's Transform at the scale it is drawn at (`affine::Model`, `creature_pose::DrawnScale`). It
  only reads the pose: no animation clock moves.
- Nothing reads the component yet, and nothing freezes it: the readers move to it one by one, each measured. Until
  then the turn's pose has no effect on the game: the state hash differs only in `pools` (the new storage on each
  posed creature), nothing is drawn from it, and no random number is drawn for it.
- Not yet as the original: the pose is still made per frame at the frame's time, in mesh space, with the slots blended
  as local poses and without the head, hands, feet and eyes scaled by size; the drawn body is still the frame's own
  pose, not the blend of the last two turns. Each of these is its own later change.

## Stand-ins and unverified constants

Values openblack uses where the original's are not known. Each is a stand-in until it is read in the executable.

- **The idle mind** (`CreatureIdleMind.h`), his constants that stand in for what the original's planner decides:
  `k_MinDesireShown` 0.2 (a desire is shown only when it is over all others), `k_ActivityLots` 4 (the idle activities
  weighed by what the creature has learnt), `k_ActOnNeed` 0.3 and `k_ActOnDesire` 0.3 (when a need or a desire beats
  everything else), `k_FaintSeconds` 10 (how long a fainted creature lies out cold). His other idle timings
  (`k_ShowDesireSeconds` 60, `k_FaceRepeatSeconds` 4.2, `k_SitSeconds` 10 + 5, `k_HangAroundDistance` 20, …) have no
  wiki backing either.
- **The mind:** his `MiracleMultiplier` table (17 species) and the planner's and learning's constants.
- **The eyes:** first blink 1000 ms, blink 200 ms, wait `random(5000) + 2500` ms (his; see [Random streams](#random-streams)).
- **Locomotion:** no stand-in left. Read 2026-10-09: walk 8 and run 20 per (0.875 × size + 0.25) and acceleration 12
  (`SetSize`, the size SetSize last got: the pen's in the pen), top-speed margin 1.1 (`SetRequiredSpeed` 0x47FA68),
  slope 0.6 (0x4912E7) within [0.3, 1.1] (0x491300..0x491319), corner π/6 (0x8CF164 at 0x4913D5).
- **Physiology:** growth slope 8 (at most 2), fat burnt below energy 0.5, youth exhaustion 4 − 0.15 × age, warmth
  threshold 0.6 at 0.025 per degree. (Growth ×3 and the 50 turns of sleep are read: [Asleep, sleeping and
  sitting](#asleep-sleeping-and-sitting).)
- **Spells:** frozen colour #8CC8FF. (Big ×1.8 and small ×0.5555556 are read: they apply only with a joint-activity
  partner; see [Pending](#pending), "The size spells with a joint-activity partner".)
- **Fight:** arena 39 per size (at most 60), damage 0.05 / 0.03, AI chances 16 / 8 / 1, a block ends 1 in 8, a counter
  1 in 5.
- **Throw:** gravity 9.81 (to compare with our physics' gravity).
- **Leash:** tied 1.5× within 180..360; pull 0.8; rope 40 nodes at 200 Hz. The hand slack 0.7 × 15 × s + 22 and at
  most 3 × 15 × s + 32 are read (`fn_005E6980` 0x5E69A2, 0x92B360..0x92B36C), s the size SetSize last got.
- **The hand on the creature** (`Creature/CreatureFeedback.h`, `CreatureHandSystem`): the slap's height shares
  `k_FeetBelow` 0.4, `k_WaistBelow` 0.7, `k_SlapAbove` 1.1 and its speeds `k_SlapSpeed` 5 and `k_HardSlapSpeed` 9
  heights a second, and the capsules' radius share 0.12, are openblack's. They take the drawn height (15 × the size it
  is drawn at, the pen's in its pen) since the hand touches the drawn body; the original's slap rules in
  `HandStateCreature::Update` are not traced, so whether they read GetHeight is not known.
- **Follow camera:** the clear-view search, 32 headings × 8 samples; his key, wheel and clear-view constants.
- **Start scale without the tables:** `CreatureArchetype::StartScale` gives 0.22 when `info.dat` is not loaded (his
  `k_UnknownStartScale`); with the tables it is the species row's `startScale`.
- **Height:** 15 per unit of size, as [engine-math.md](engine-math.md) gives it (`k_CreatureHeightPerScale`); his
  copies are pinned to ours by tests.

## Addresses of the original, moved out of the code

The ported code's comments describe behaviour in plain words; the original's names and offsets behind them are kept here.

| Address or name | What it is | openblack |
|---|---|---|
| `GCreatureInfo` +0x1E4..+0x370 (size 0x384) | the creature species row; offsets in [The creature tables](#the-creature-tables-in-infodat) | `GCreatureInfo` named fields, `static_assert`s in `InfoConstants.h` |
| `CreatureActionInfo` +0x0 / +0x4 / +0x8 / +0x98 / +0xC0 | strength gain, energy cost, exhaustion cost, desire, desire multiplier | `CreatureActionInfo` named fields |
| `CreatureInitialDesireInfo` +0x4C / +0x50 / +0x54 | `DesireDecay`, `InitialValueMin` / `Max` in bw1-decomp | `creature_mind` tables (`initialMax`, decay range) |
| `CreatureDesires::Initialise` 0x4DC100 | the 40 initial draws | `creature_desires::Create` (draw injected) |
| `CreatureDesires::FindWeakestDesire` 0x4DC7B0 | reads the desire's value array (set to 0 at start) | `creature_desires` |
| `CreatureDesires::RandomiseIncreaseTime` 0x4DC310 | cheat-flag jitter, not decompiled | none |
| MSVC `rand` LCG 214013 / 2531011, seed 0x913 | the mind file's name cipher | `creaturemind::MindFile` |
| `LH3DMist` constructor 0x7F9560, +0x84 | the mist's frame counter, `Random(0, 16) & 15` | (the cave, not ported) |
| `LH3DCreature` | the creature's drawn body and eyes | `CreatureBody`, `CreatureEyes` |
| `LH3DCreature` +0x8C / +0x5224 / +0x5228, `fn_0048F5B0`, `SetSize` 0x480530 | the rest height, the bones' reach and the radius ([The creature's radius](#the-creatures-radius)) | `CreatureBodyMetrics`, `creature_cast_moves::BoneReach`, `creature_morph::Reach` / `Radius` |
| `LH3DPrimitive::Create` 0x84AB50 | turns a primitive's blend offset (+0x2C, count +0x28) into a pointer when the count is not 0 | `l3d::L3DFile::GetBlendSpan` |
| `fn_0084AC20` (32-byte vertices), `fn_0084AE00` (40-byte) | the blend loop: `[0xC392A8]` set, then for each blend v[a] += (v[b] − v[a])·w on +0x0, +0x4, +0xC (0x84AC40..0x84ACB2, 0x84AE1C..0x84AE8B), then the clip codes and the projection | `vertex_blend::Towards`, `vs_object` (`USE_MORPH`) |
| `fn_0084A6D0` 0x84A886, `fn_0084C3C0` 0x84C718, `fn_0084D2D0` 0x84D7A5, `fn_0084DAA0` 0x84DF34 | the boned primitive draws that call it, after the per-group bone loop and the light (0x84C3F0..0x84C706) | `Renderer::BindBlendSources` |
| `[0xC392A8]` | the blend switch, 1 at start; `fn_00810720` (vt+0x15C) saves it, sets 0 when its first float argument is above 0 (0x81088F) and restores it (0x810C33) | always on |
| `GScript::CreatureSpellReversion` 0x6F4DD0 | POP object (`GetScriptGameThing`), POP flag; "Thing not found!" returns; "Thing not creature!" via `IsCreature` (vtable +0x34), the write itself guarded by `IsCreature` again; stores the raw flag at Creature +0x10BC | `CreatureSpellReversion` (CHLApi.cpp), `spell_creature::SetReversion` |
| Creature +0x10BC | the reversion flag: set to 1 at creation (`fn_00474130` 0x474239), read once, in the per-turn spell step `fn_004F48C0` at 0x4F49FB | `creature_spells::Spells::reversion` |
| `fn_004F48C0` 0x4F49FB..0x4F4A1C | holding phase over with the flag 0: phase 0, the miracle's vtable +0x530 called and the pointer cleared; no finish hook, no event 0x1B, no call to the queue release 0x4F4E60 (called only from the normal finish at 0x4F4AA2) | `creature_spells::Step`, `Phase::Holding` |

## openblack

- `src/InfoConstants.h`: the named creature tables. `src/3D/CreatureBody.{h,cpp}`: mesh names (`ParseMeshName` and
  `GetIdFromMeshName` return `std::optional`), loaded once by the creature-mesh loop in `Game.cpp`.
- `src/3D/SkeletalAnimation.{h,cpp}` (`openblack::skeletal_animation`, beside the unrelated
  `ecs::components::SkeletalAnimation`), with `FromMorph` for the morph parser's animations.
- `components/morph`: his parser, with the header's named fields (`duration`, `looping`, stride, displacement, hair
  groups, sound events) and the creature block that follows the animations in a `.cbn` (action points, leash bone,
  the right eye's bone, eyes, tattoo sites, voice bank, then the numbers the block ends with). The block is read only
  when the header's first field is not 0: in `Data/CTR/hh.HBN` it is 0 (the hand's file), in the `.cbn` files 0x15, so
  the hand reads the same clips as before.
  `components/rawimage`: headerless `.raw` images.
- **Loaders** (`src/Resources/Loaders.cpp`): the mind loader reads through `Locator::filesystem` and never throws (a
  file that cannot be read leaves `ErrCantOpen`). `LoadCreatureRigs` walks `Data/CTR` at start-up, reads each `.cbn`'s
  `Creature` block into `CreatureRig` (`GetCreatureRigs()`, by `creature::GetRigId`) and preloads the skin meshes each
  rig names into `GetL3DFiles()` under `creature/skins/<name>`; nothing is read later. `Game.cpp` also loads
  `Data/Eyeball.l3d`, `Data/Eyelid.l3d`, `Data/C_Ape_Hair(a).raw` and the skin art, which the creature pass
  (`Graphics/RendererCreature.cpp`) draws.
- **Components and archetype**: the creature's components in `src/ECS/Components/` (`CreatureLocomotion` holds a
  `route_planner::RouteFollower` behind a pointer in place of his planner and route; `CreatureDrawPose` is ours, the
  per-frame drawn pose). `CreatureArchetype::Create` keeps our rotation (`affine::AngleY`) and creation index, puts the
  components on, draws the body at `DrawnScale` (size × 15 / rest height; the size alone without the mesh), measures
  its body once (`BodyMetrics`, [The creature's radius](#the-creatures-radius)) and publishes `Teleported`. `SnapTurnStart` sets a creature's `fromPosition` / `toPosition` and drawn pose, looked up
  through the const registry. `ObjectMetrics` reads a creature's scale from `Creature::size`.
- `src/Creature/`: 38 pure modules (body, mind, movement, fight, leash, audio, panel, mode), beside the existing
  `CreatureMind.h` placeholder the loaders use.
  `components/creaturemind/`: the mind file library, linked into `openblack_lib`. `src/Audio/Engine/AnimEffectKeys.h`.
  `src/Camera/CreatureFollow.{h,cpp}`.
- Tests: `test/creature/`, one group `test_creature` (398 tests; one integration test reads saved minds from
  `OPENBLACK_CREATURE_SAVES` and skips without them). None uses the Locator or the game's data.
- **Systems** (`src/ECS/Systems/`, one `<X>SystemInterface.h` and one `Implementations/<X>System` each, all reached
  through `Locator`): animation, skin, hair, footprints, physiology (made with the game) and locomotion (made with each
  land). `ecs::creature_loop` calls them in the turn and the frame; no registry signal is connected. What changed from
  his:
  - the turn length is `game_clock` (`k_TurnSeconds`, `MsPerTurn()`), not his time service;
  - the eyes blink on the turn and the CRT stream; the locomotion's run-away distance, fidget and mirror coin and the
    poo's yaw are `GameRand` / `GameFloatRand` ([Random streams](#random-streams)); the puke's 12 drops are kept by the
    physiology system as draw-only data (`GetPukeDrops`), no entity, scattered by a private seeded generator;
  - the skins are painted only from the mesh files the loaders preloaded, never read at frame time;
  - the footprints' April Fools' day reads an injected date, once per land (`Reset`); the only wall-clock read is in
    `Locator.cpp`;
  - locomotion walks on our `RouteFollower` (one per creature, made at its first move) and the `land_avoid` mask
    (`IsPosValid` at 7.1 for a destination and 7.05 while walking, `land_avoid::NearestValid` to put a stray creature
    back). The follower fills its obstacles through `route_plan_world::CheckSquareFunction`, which now tells a creature's
    follower (its plan's context) from a footpath's holder (no plan, unchanged): a creature avoids another player's map
    shield, and a dead tree only while it burns and no script controls it. His own planner, obstacle gathering and
    corner cap are not ported. The `Transform` moves once a turn (`creature_pose::CommitTurnPose`, through the map
    cells); between turns only `CreatureDrawPose` and the animation slots move. A creature put back on the land it can
    stand on publishes `Teleported`.
- **The other systems**: fight, leash, creature hand, mind (with its learning) and object actions, made with the game;
  `ecs::creature_loop` runs all but the creature hand, which the hand and the debug spawner reach. What changed from his:
  - **the fight is stepped by the turn**: the animation clock, the moves the animations carry the fighters by, the
    blows' moments and the turns to face move on 100 ms at a time at the start of the fight's turn; the frame only
    draws the body between the turn's start and end (`creature_fight::DrawnTime`) and charges a held blow. Its coins and
    choices draw `GameRand`, a wound's texels u then v first; the camera watches a fight by the original's rule
    ([Fights: the camera](#fights-the-camera)), through the player's camera, never by flying the camera itself; the
    player is `creature::LocalPlayer()`;
  - the leash has no posts and no shake: those come with the citadel's leash object and the game's gestures; the keys
    are read once a frame ([The leash keys](#the-leash-keys)); `TakeOffHeldLeash` is the shake's effect, for the
    scribble gesture, which nothing calls yet. Its scans are const and
    in entity order; its tying sounds are the in-game bank's 148 and 149, in turn;
  - the creature hand keeps the hand's pose and the creature under it (his `Game` members); there is no stroking by
    command;
  - the mind's draws are `GameRand` / `GameFloatRand`, the 40 first desire draws `GameFloatRange` in desire order; its
    scans pass over things that are going and take the lower entity on a tie; night is the day / night clock's; water is
    found on the `land_avoid` mask;
  - the object actions play by the turn, and their moments (taking hold, knocking down, letting go, eating) come at
    turn time, the palm sampled in the actions' animations at the moment. Something eaten dies as any death goes
    (`life::Kill` for a villager or an animal, the dead list for the rest); a home struck takes a creature's blow
    (`abodes::OnPhysicalDamage`), a tree is felled, a thing is deleted. What is let go of goes back on the map where it
    is, drawn there at once; it does not fly yet. Animals are food and can be picked up (when a hand may hold them).
- **Creature spells** (`src/Magic/Spells/SpellCreature.{h,cpp}`): the creature miracles (MAGIC_TYPE 26..41) are our
  spell class tree's `SpellClass::Creature`, registered last. Its operations are the plain spell's but for two: cast on
  an object it is the plain cast, then, on a creature, `spell_creature::Receive` (held for the spell's time times the
  caster's tribal power, then the spell runs with no time limit until the creature lets it go; a spell of the same kind
  it replaces is closed down); its close-down is the plain one, then no creature's spell points at it any more. A
  creature miracle cast at a place does exactly what it did before as a plain spell; only the "class not ported" warning
  is gone. `spell_creature::ProcessTurn` moves each creature's spells on by a turn (creatures in entity order, through
  the const registry), applies each event through the pure `creature_spells::Apply` (the body value pulled, the freeze
  and fizz, the mind paused, the desire made dominant then least) and plays the sound action from the creature bank. It
  runs at the end of each creature turn (`ecs::creature_loop::ProcessTurn`). His `MagicSystem` creature methods
  are not taken.
- **Spell reversion** (`CREATURE_SPELL_REVERSION`, native 233): `creature_spells::Spells::reversion` (true unless a
  script turns it off; his field and name) and `spell_creature::SetReversion`, called by the native. The native POPs
  the creature, then the flag; no thing logs "Thing not found!", anything but a creature "Thing not creature!" and
  nothing changes (a creature with no spell on it yet is given its spells' component to keep the flag). Without
  reversion a spell that has held its time goes straight to off: no wearing off, no finish (the creature keeps the
  size, strength or look the spell gave it) and its miracle is let go. Unlike his version, such a spell makes no way
  for the spells waiting for its kind: in the original only a spell that wears off releases the queue. No Land 1 or
  Land 2 script calls the native, so it is dormant there.
- **The creature as a spell target**: `fire::traits::IsCreature` is true for a creature that is still there (the
  mourners pass on fire starters that may be gone); `PhysicsObjects::ObjectInfo` gives a creature its species' row, so
  it has the row's burn multiplier, defence multipliers and weight like any object. A creature takes no harm from a
  spell's damage or from burning yet: both reductions skip it (Pending). Explosions and tornadoes already leave
  creatures alone (`CanBeDestroyedBySpell`).
- **Throws**: what a creature lets go of goes into our physics objects, never his own flight. Thrown at a target, it is
  `PhysicsObjects::AddObject` with the creature as the thrower; put down, dropped, tossed aside or let go of by a
  creature that is gone, it starts as the hand's objects do (`from_hand::InitialisePhysicsFromHand`), but is thrown
  above a speed across the ground of 1 rather than 2 (`from_hand::IsThrown`, [physics.md](physics.md)). An object no
  body can be made for is put down where it is. `ecs::creature_physics` gives the creature its body when the physics
  wants one (a ball as tall as the creature, six points and eight faces, mass 1000, not moved by a hit) and its weight;
  `RegisterPhysicsHandlers` is called from `Locator.cpp` with the other game services; the creature catch hook stays
  unset.
- **Creature audio** (`CreatureAudioSystem`, made with the game, updated each frame by `ecs::creature_loop`): its animations' moments play
  through `Locator::audio`'s `PlayAnimationEffect` with the creature as a tracked owner, from the creature bank
  (`SfxBank::Creature`) or the species' own (`CreatureBank` of `VoiceBankStem`), at the distance from the listener; the
  ground is `sea_cells::GetSurfaceType`. The bank's own filters stand for his temple and wide-screen gates. Whether other
  players' creatures speak is the script's `creatureSound`. Each sound kept on the creature notes its bank and the
  channel it started on (none when filtered out or not started). The footsteps still leave prints.
- **Mimicry and the town hooks** (`src/Creature/CreatureDeeds.h`, `src/ECS/CreatureMimic.{h,cpp}`,
  `src/ECS/Events/CreatureMimicEvents.h`): four deed sites publish the player's deed, which the mind system takes
  through `PlayerDid`, for the player's own creatures only; three villager sites publish the town need a creature may
  share, with no subscriber yet ([Mimicry and the town hooks](#mimicry-and-the-town-hooks)).
- Still his and not ported: the renderer's creature and rope passes, the debug spawner, Creature Mode's wiring and the
  Creature Cave. The mind interface has the casting methods as virtuals that do nothing. His fight interface's
  additions are in ([Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting)); their
  callers come with the miracles and the tornado.

## Pending

- **The size spells with a joint-activity partner** ([The size spells](#the-size-spells-the-start-delay-and-the-targets)):
  with mind +0x1BE8 set, big's target is `min(+0x374, max(1.8 · d, d))` (0x47DB64..0x47DBBC) and small's
  `min(+0x378, min(0.5555556 · d, d))` (0x47DBE4..0x47DC3C), d = the creature's own drawn size (`GetCreature3D` on
  the creature, +0x90, recomputed every turn); 1.8 is [0x8CF030], 0.5555556 [0x8CF034]. Small's is read as written: a
  `min` with the smallest size, so with a partner small never aims above it. What sets +0x1BE8 is not traced; it is
  the mind's, which openblack does not decode, so neither the partner nor this rule is ported.
- **The body's turn while the hand holds the creature** ([Growth, size and age as time
  passes](#growth-size-and-age-as-time-passes)): the original skips the autoscale step, the turn counter and
  `UpdateAttributesAsTimePasses` (age, growth, strength, energy, fat, tiredness, thirst, warmth) while the creature's
  `Flags +0x24 & 0x10` is set (0x472F77 → 0x4737DB). The flag is set by the start packet 0x1B's handler and cleared
  by the end packet 0x1C's ([hand-and-interface.md](hand-and-interface.md), "The locked select on a creature").
  openblack's physiology still runs those turns. The exact stand-in is the hand's own locked flag (set when the
  start packet's apply takes hold through the creature hand, cleared by the end packet's apply), which only the hand
  keeps; the creature hand's held creature (`CreatureHandSystem::GetCreature`, the one autoscale uses) would end the
  skip too early. This is read in openblack's code, not the original's: the hold is dropped by
  `CreatureHandSystem::Release`, called from `HandSystem::LeaveCreatureState` as soon as the render hand leaves its
  CREATURE state (`hand_creature::StepFrame`, the Feedback send, run each frame), while the flag stays set until the
  end packet is applied (`HandSystem::ApplyEndLockedSelect`); that packet is sent by the turn's lock step
  (`hand_creature::StepLock`, WaitingForLockOff, a step after the action's release) and applied at a later turn's
  start. That the drop always comes first is read from this order, not measured. Porting it needs the hand to
  publish that flag. The same skip also passes over `Update3DCreatureFromAttributes` (0x4737D2, [The fatness the body
  shows](#the-fatness-the-body-shows)): the shown fatness does not step while the flag is set, and openblack's
  `ProcessShownFatness` still steps it.
- **The alignment and strength the body shows** ([The fatness the body shows](#the-fatness-the-body-shows)): the
  original writes the evil-good and weak-strong targets once a turn in `Update3DCreatureFromAttributes`, after the
  sub-action, and at once in `SetStrength` 0x4F0100, `ModifyStrength` 0x4F0170 and the script's ALIGNMENT SET
  (0x70EF22, then `UpdateMorphing` 0x618C40). openblack builds the targets every frame from the creature's alignment
  and strength, so a change made inside a turn (a deed's alignment, a spell) reaches the drawn body up to a turn early.
  The per-turn alignment step (`GAlignment::Process` 0x414140, at the end of the turn) is not ported either. Not done
  here: it needs the targets kept on the body between turns and every writer listed, and the alignment's place in the
  turn depends on that step.
- **A script's fatness and the shown fatness** ([The fatness the body shows](#the-fatness-the-body-shows)): openblack's
  game turn runs `ecs::creature_loop::ProcessTurn` (which ends with `ProcessShownFatness`) before the scripts
  (`Game.cpp`, the creature turn and then the ScriptsUpdate stage), so a fatness a script sets in a turn starts to show
  the next turn. The original's order of its script processing against `Creature::ProcessState` in the game turn is
  not traced on this page, so whether a script's fatness shows the same turn or the next is not known. To read: the
  game turn's calls, from the turn loop down to the script runner and to the creatures' process.
- **The shown fatness at load**: the mind file carries phys+0x18 ("previous fatness"); openblack starts
  `CreatureMorph::shownFatness` at the fatness. Carried with the rest of the body's state at load.
- **What the creature thinks its player wants** ([`MakeCreatureEmpathiseWithPlayerTownDesire`](#gplayermakecreatureempathisewithplayertowndesire-0x4c80f0)):
  - The other writers are not wired: `Villager::EndPhysics` 0x5F0B53 (a landed villager, 1 COMPASSION 0.1 or 2 ANGER
    0.5), `Creature::ReactToPhysicsImpact` 0x479B37, `Rock::InterfaceTap` 0x6E7541 (3, 0.5), `GPacket::ProcessPacket`
    0x63D24D and `fn_00653150`+0x1770 (0, 0.5); and the villager sites not ported (worship site, abode, tree).
  - The reader: our scroll builds are not matched call for call with the original's (its `InitEngine` call and
    `fn_00789730` are not read; its per-frame build runs from `Draw` with the zoom tests above). Only how often the
    values are zeroed could differ, which shows as one line end on one build.
  - `IsActivated`'s second case (game flag 0x2000, set at start-up from the registry value `GatheringFlag`, see
    `GetDominantDesire` above) is not ported: openblack has no such start-up option, and the game sets the bit nowhere
    else. Without the value in the registry (as on the machines the checks run on) the two agree. What the option is
    for, and the registry read helper's exact success test (0x64338A..0x643393), are not read.
  - The desire line's own texts (database entries 0x4AB and 0x248) are never written, so they are not named in the
    code. Against bw1-decomp's `HelpTextEnums.h` the entries here sit 4 below the enum (0x49F, the scroll's title, is
    1187 `LIKES_SCROLL_TITLE`), which makes 0x4AB 1199 `PERCEIVED_PLAYER_DESIRE`; whether the same offset holds at
    0x248 (584 `CURRENT_DESIRE_FIRST_PERSON_01` without it, 588 `..._05` with it) is not read.
  - The 57 rings (30 past values each, written on even game turns) are not ported: only the mind save reads them, and
    they come with it (R09). The turn parity is to be read then: whether the original's counter at
    `Creature::ProcessState` has the value of our `game_clock::Turn()` in the same turn.
  - The fade's gate: 0xD00DE8 has no writer the static scan finds (see [the gate](#gplayermakecreatureempathisewithplayertowndesire-0x4c80f0)).
    It is read as set, since the same block moves the creature's map position after its body. To confirm with a
    run-time read of the dword (and of 0xD00DE4) in a game with a creature, or by finding the writer.
  - A negative player desire: the original writes before the array; ours ignores it. Whether `info.dat`'s
    `perceivedPlayerDesire` holds any negative value is not checked (an integration test with the data).
  - The look: inferred that our `CreatureAnimation::lookAt` stands for `LH3DCreature` +0x48C0 and minus our heading for
    the body yaw +0x84 (see the heading convention under Route planning below).

- **The timed dominant desire** ([The timed dominant desire](#the-timed-dominant-desire)):
  - `SuppressDesire` also empties, when it holds a desire down, a per-desire `LHStack<CREATURE_DESIRES,41>` at mind
    +0x718 + 0x30·i (`SetToZero` 0x4F12B0, nine dwords +0x0C..+0x2C); what that stack is, is not known, and
    openblack's `Suppress` does not do it.
  - The setters of the body-backed sources (14 low energy, 0x4DE8F0 writes `1 − v` into the energy; 21, 22, 32, 36,
    37) are not ported: `MakeFullyDominantWithFullSources` only sets the source's value. Only a forced Hunger, Poo,
    Tiredness, Water or AttractAttention reaches them; no leash forces those.
  - The clear's gate is the player's first interface status's leash type (above). The leash's own ROPE call always
    passes it; the other callers of the clear (the end of the Compassion, Anger and Scratch spells,
    `SET_CREATURE_ONLY_DESIRE_OFF`) are not wired, and `ClearDominant` leaves the gate to them.
  - The hand's locked select (Flags +0x24 & 0x10) skips `UpdateLeash`, the home rule and the keep-alive in the
    original; openblack's leash turn and the mind's count run while the hand holds the creature.
  - Two creatures leashed together: the original runs each creature's whole turn in the creature list's order, so the
    other's keep-alive may come before or after the leashed one makes its anger dominant; openblack runs every
    leash first, then every mind, so it is always after.
  - A creature whose desires are not made yet (before its first mind turn) takes no mood from a leash put on then;
    the original's always exist.
  - The tie's `ShowNeedsVisuals` branch (mind +0x21D0 = 0x50A760, `fn_0071A200` > 0.6) is not ported.
  - Not read: the other callers (fn_00474E45, fn_0047CFB0, `CanBeHealedByHealSpell` three times, fn_004EA540,
    0x507113, fn_00653150) and their desires and seconds. The nine under the label fn_004F5754 are the mood and need
    spells', read above (20000 s from [0x8D1598]).
  - `MakeDesireLeastDominant` 0x4DC9F0, which each spell's end calls with 1.3: value = the value of
    `FindWeakestDesire` 0x4DC7B0 (from 2.0, activated desires only, strict `<`, the desire itself included) over the
    factor, then clamped to the floor (info +0x274) and the maximum. openblack's `MakeLeastDominant` leaves the desire
    out, has no floor clamp and gives 0 with nothing to compare; with the others at the floor the original gives the
    floor, ours floor / 1.3. Every species' floor is 0, so the shipped data shows no difference. raffclar's test
    (`WearingOffItIsWantedLeast`) keeps his expectation; fixing it is its own measured commit, since its game callers
    are live.
  - With more than 1000 ms a turn the original's division by the whole turns a second would fail; openblack's
    `StepDominant` then never runs out.
- **Reactions to miracles** ([Reactions to miracles](#reactions-to-miracles)):
  - The other reaction types a creature takes (0, 4, 6 to 10, 16 to 20, 22, 23 and the rest with a `Creature::*Priority`
    or `Setup*`): each its own item; until then the creature scores them 0.
  - The availability fields not named: Flags +0x24 bits 0x80, 0x400 and 0x10, +0x10AC, +0x10F4, mind +0x1C10, mind
    +0x1C14 against +0x380, mind +0xF60 = 21 (its own and its friend's). One of them may be the script holding the
    creature (the hand demos hold it before turn 30); whether that blocks its reactions is not known. +0x1060 (reactions
    on) is not ported as a value: CREATURE_REACTION stays not implemented (no land script calls it), and the creatures
    of the CHL CREATE native (its creature case is not ported) and of `CREATURE_CREATE_RELATIVE_TO_CREATURE` (not
    ported) must start with it off, and `SwapMinds` must move it.
  - A stilled mind (the freeze spell): the availability does not test it, but whether `ChangeSource` and
    `ForceActivityAndForceAction` do anything while the mind is not active (+0x20CE4) is not read. openblack grows the
    fear and replans a paused mind (`CreatureMindSystem::ReactToNastyMagic` / `ReactToNiceMagic` do not test
    `paused`); only the learning is skipped.
  - The forced desire and belief: the setups call `ForceActivityAndForceAction(desire, GetBeliefAboutObject(self),
    action, 0, 0, 1)` with desire 5 FEAR and action 156, or desire 6 CURIOSITY and action 157. openblack replans the
    agenda (`Replan(Activity::Planned, …)`) and forces neither the desire nor the belief, so whatever credits feedback
    or satisfaction to the current desire does not see fear or curiosity as the reason. To port with the forced activity
    (`ForceActivityAndForceAction`'s body, its last three arguments).
  - Actions 156 `CREATURE_RUN_AWAY_FROM_POS` and 157 `CREATURE_EXAMINE_POS`: their bodies are not read. raffclar's
    agendas stand for them (the frightened start 61 one time in two, the run to 5 heights, the turn and point first one
    time in two, the 2.1 s puzzled wait); to pin with a test once the action table is traced. The plan
    `StartReacting` keeps (mind +0xF7C..+0xFA4) is not ported: what reads it back is not read.
  - The fear read at mind +0x164 is taken as `Desire::Fear` (inferred).
  - `UpdateHowImpressed`: the initiators' impressive values (vt +0x1B0) and the two things they feed (mind +0x18C64,
    `UpdateHowImpressiveIsCreature`) are not ported.
  - After learning a miracle on a leash, the pending cast at the first object near the leash's holder (`fn_004F8850`,
    +0x380 = 1) belongs with the creature's casting (R06).
  - Alliances (`GPlayer` +0x950) are not ported: a nice miracle or shield of any other player is ignored.
  - `ProcessReaction` runs in the creature's mind turn, not in the original's living list order among the villagers and
    animals; it draws nothing and changes only the creature's own reaction and records.
- **Learning by watching** ([Learning by watching](#learning-by-watching)):
  - **The turn**: the original compares the game's turn (`g_game`+0x205A40); openblack passes `mind.turn`, the turns
    this mind has thought. They differ after a freeze, before the mind exists, and against a mind file's stored turns.
    To change, measured, in the commit that first reports sightings from the game.
  - **The messages and the meter**: `fn_004C9FE0` (the message for each code) and `fn_0047CDA0` (the meter, at most
    once in 100 turns) are not read in full; openblack keeps both as data. To read before anything shows them.
  - **The species' multiplier**: `MiracleMultiplier` is a fixed 17-row table in `CreatureMindTables.cpp`; the original
    reads `CreatureInfo` +0x37C. The 17 values are not yet checked against `info.dat`.
  - **The mind file's known list**: with known = known about, `SaveMind` writes what the original's list holds
    (INFERRED: that the file's list is `ActionsKnownAbout`; `SaveMind` 0x4E7820 not read).
  - **The scroll's text id**: the original skips a row whose text id (+0 of the magic row) is 0; which rows those are
    is not read, and our scroll lists every known-about row from 1.
  - **The callers** of `ConsiderLearningAction` (0x4D8158, 0x4EA6ED, 0x4F2945, 0x4F2963, 0x50613D, 0x50A4EC) are not
    read: which sightings reach it, and from how far.
- **What a creature looks at** ([What a creature looks at](#what-a-creature-looks-at)):
  - The look point. fn_004D1BB0 gives (x, z, `GetHeight()`), and `Object::GetHeight` 0x638120 is the mesh's +0x28 ×
    scale × 2; how that height becomes the point the head turns to was not traced (the consumers of fn_004D1BB0 and
    fn_004D1C20). openblack looks at an animal where it stands (raffclar's choice) and at the other kinds at heights
    that are not from the original (villager 1.8, abode 4, tree 6, temple 30, a creature 15 × size). To be fixed for
    every kind at once.
  - The candidates. The original weighs every object in the scanned cells, Fixed (0.25), MobileStatic (0.4, toys 0.9)
    and features included, behind the extra fn_004774F0(pos, 6) gate; openblack has six kinds and no such gate.
  - Which of them. The original offers only objects linked into the scanned cells (fn_004D0A20) and skips the
    UNAVAILABLE ones (+0xA & 1). openblack offers every creature, villager, abode, tree, temple and animal with a
    `Transform`: an animal or villager held in the god's hand or in the creature's hand, and one being eaten, dying or
    dead, included. Whether a held object is still linked into its cell, and which of these states set +0xA & 1, was
    not traced; until it is, a creature can turn its head to something the original would not offer it.
  - The look class of CitadelDove (18) and CitadelBat (19). The factory's cases 17-19 create nothing, and no vtable
    for either was found among the class vtables, so vt+0x3E8 for them is unknown. openblack's 0.7 (Animal, as the
    script types and the heal rule also take them) is a placeholder; they are never made, so nothing reaches it today.
  - The `AnimalInfo` of a Goat, Zebra or Vulture object. The factory makes none of them (cases 5, 7, 17); their
    classes are built only by the save loader, by the saved type. openblack takes `AnimalInfo` 5, 7 and 17 as the
    class of the same name (Animal, Animal, Dove), which is assumed by name, not traced. To be read in
    `GameOSFile::LoadInstance`: the saved type that picks each class and the info it gives the object.

- **What frightens a creature** ([What frightens a creature, and the fire it believes](#what-frightens-a-creature-and-the-fire-it-believes)):
  - The other desires' object tests (Impress vt+0x244, Compassion vt+0x23C / vt+0x628, Anger vt+0x234, Play vt+0x240)
    are not ported: openblack has a target per action, which is the original's rule only where one desire owns the
    action. To audit with the same info.dat dump.
  - `IsObjectSuitableForDesire`'s other gates (fn_004D8E10, fn_0047CD20, flag +0xA & 2, `IsSuitableForCreatureAction`
    vt+0x21C, Hunger's town usefulness ≥ 0.1) are not compared.
  - How a spell becomes a candidate of the Fear test. `Spell` answers 1 (0x55CED0), but in bw1-decomp `Spell` derives
    from `GameThingWithPos`, not from `Object`, so it has no `InsertMapObject` and is never in the map cells that
    `Creature::Look` scans before `AddBeliefAboutObject` and `IsObjectSuitableForDesire`. Another route (a reaction, a
    spell's own call to the creature) may offer one; none was traced. Until it is, `mind_detail::Gather` offers no
    spell (`Accepts` still takes one, as the object test does). The original's belief about a spell is not read
    either, and openblack has none.
  - CitadelBat (19): no class is known (see the look class above), so whether it frightens is unknown; openblack says
    no. It is never made.
  - HurtVillager (raffclar's target for the heal cast, life ≤ 0.7) is not on this path; the 0.7 is unconfirmed. It
    belongs with the casting rules, with `CastHealSpell`'s target rule to be read there.
  - Slot 22 of `CreateAttributeArray` built with the dominant-desire vtable (see the side note above).
  - CreatureIsle's info.dat was not read for the Fear rows.

- **Vertex blends** ([The body](#the-body)): which objects reach `fn_00810720` (vt+0x15C) with a non-zero first
  argument, and so are drawn without blends (it also builds a colour from it with `fn_008104B0`); openblack blends
  every creature draw. A creature without `CreatureMorph` is drawn by the plain instanced program, without blends.
  The blend source needs vertex texture fetch of RGBA32F; where a GPU lacks it the seams are not blended. The three
  boned spell meshes with blends (`PUPIL`, `TEACHER`, `Tree_Goddess_Boned`) are drawn without them: to read whether
  their draw reaches the boned primitive draws, and to blend them in the plain boned program if it does. The
  original's sea plane cut (`fn_00858BA0`) does not call the blend loop; openblack's creature draws all take it.

- **The creature drawn between turns** ([Drawing between turns](#drawing-between-turns)):
  - Done 2026-10-08: the leash rope's collar end is the drawn neck, between turns and in the pen, as the original's
    ([The leash's collar end](#the-leashs-collar-end)).
  - Whether the original clamps the creature's turn fraction to 0.99, as for the livings; ours goes to 1.
  - The drawn yaw: ours is lerped over the turn; whether the original turns it at a rate, as the villagers do.

- **The creature's casting rules** (pure, nothing calls them): these numbers are raffclar's, with no reading of the
  original behind them: the one-in-five chance of showing how it feels first, the three-second hold, the 50-unit lightning distance, the clear-area search (26 cells across, a thing
  kept 5 from a cell's middle), the share of sightings at which it tries a miracle (a half) and at which a try stops
  fizzling (against the times needed less one). The energy share each magic type takes when a cast is paid for is a
  table of the game's data, not read yet. Nor is the original's own order of approach, face and pose. The same holds for
  what the section above no longer calls confirmed: the chants-to-energy division and the largest chants it has to give
  (the original's two functions are known by name and address only), the 0.7 cap on how much paying tires it, the energy
  kept to the creature's size or 1, and the 0.85 exhaustion bar. How the original makes a desire dominant is now read
  ([The timed dominant desire](#the-timed-dominant-desire)), and his cheat runs it: the 20000 s are the spells' own
  seconds (0x8D1598), the dominance rule and "be friends" under compassion are the original's.
  original behind them: the mood and need spells' 20000-second cheat time, the one-in-five chance of showing how it
  feels first, the three-second hold, the 50-unit lightning distance, the clear-area search (26 cells across, a thing
  kept 5 from a cell's middle) and the share of sightings at which it tries a miracle (a half; `CanCreatureCastSpell`
  is not read). Nor is the original's own order of approach, face and pose (`ConstructSubActionsForCast*`, the
  GoNear / GetAway / TurnToFace sub-states, the cast sub-action's animation +0x38 and hold +0x48 are not read), and a
  pose that ends before its loop begins: the original's Perform keeps waiting, ours gives the agenda up. Nor that
  making a desire dominant frees it, makes it fully dominant and does the same for "be friends" under compassion. The
  body's formulas, the 0.7 cap, the energy kept to the size or 1, the 0.85 bar, the fizzle against the times needed
  less one at 0.999, the magnitudes and the energy share are read now ([above](#casting-miracles-the-rules-not-yet-called)).
  Still unread in the cast: the thing the release deletes (Creature +0x1084), and where the fight itself casts (only
  the two sub-states call the cast functions).
- **Two known issues the casting step machine is carried with** (his text, taken as he wrote it; neither can be reached
  while nothing builds a casting step, so neither changes the game today). They belong to the commit that first builds
  one, and the original's own behaviour is to be read before either is changed:
  - The rule "a sit nobody is waiting on any more ends" counts only a still step as a sit, so while a casting step runs
    and the body loops it tells the pose to end its loop every turn. A casting pose meant to be held for its turns would
    be told to end on the very turn the miracle is cast. The test of the hold pins this as it is, turn by turn, so the
    commit that first builds a casting step has to change the test on purpose.
  - The turns a cast is held for are rebuilt from the frame's own seconds (one over them), which is zero until the
    senses are filled in, and dividing by it gives no number at all. The count is to come from the fixed time step the
    runs use, not from the frame.
- **The leash's collar end** ([The leash's collar end](#the-leashs-collar-end)): the weights `PrepareForDrawing`
  blends the two bone buffers with are not traced.
- **The leash's tug** ([The leash's tug](#the-leashs-tug)). Its sender has no caller in W1.20, so nothing is left to
  wire; what follows matters only if a caller turns up (another build, a network peer) or for the debug window's tug:
  - The tug with an object: on a tied leash (the tied object) and in the hand with `GInterface` +0x3C8's object
    (what that field holds is not read): 0x4CEA5E..0x4CEAE8 and 0x4CEE70 / 0x4CEE80 (the lead onto an object) are
    not ported; openblack's tug does nothing on a tied leash.
  - 0x4C9FE0(0x28, 7, 0, 0, 0) after a counted pull is not decoded.
  - The plan's floats +0x1C..+0x2C (0.01 at +0x2C for the go-to-hand plan) are not mapped to openblack's plan
    priority, which the forced plans leave at 0; nor the other arguments of `ForceActivityAndForceAction`.
  - The fields +0x10B4 and +0x10B8 (set by the tug), +0x12A8 and mind +0x1C00 (tested) are not known; mind +0x1C00
    has no writer but zero stores.
  - Led and keeping on, the original also sets the required speed of the walk under way
    (`SetRequiredSpeed` before the route test); openblack's locomotion keeps the speed its walk started with.
  - The fade's gate: [0xD00DE4] (read-only in the static scan, as 0xD00DE8 above) and mind +0x20CE8; and whether a
    fighting creature's `ProcessState` reaches `UpdateTime`. openblack fades every turn.
  - The body force (0x4813B0, 0x47FAC0, +0x48AC): never applied in W1.20, so not ported.
- **Tying the leash with the hand** ([Tying the leash with the hand](#tying-the-leash-with-the-hand)):
  - The hand's side is the hand area's: the Action button's double click (whether [0xE85350] bits 0x10 / 0x20 are the
    window's double clicks, through `LHMouse::SetButtons` 0x7E4B80's jump table 0x7E4D88, and `fn_00470B30`'s timed
    +0x6520, probably a key binding's double press), the idle process 0x5D5190's other branches (g_game +0x14 & 4 at
    0x5D5227, and +0x38 & 2), the target pick, and the calls into `ecs::leash_tie`.
  - `GInterface::Tap`'s first call `fn_005D36D0(object)` and the InterfaceAction site 0x5D5608 are not read.
  - Sub-action 0x5D (93), which a tap does not repeat, is not identified; openblack keeps no plan steps on a creature,
    so `LeashTapCheck::alreadyActingForLeash` is never set.
  - Creature +0x380 = 2 (doing what the leash sent it to do) after the three packets has no state in openblack, so
    the walk back is not held back by it (`WalkBackCheck::sentByLeash` stays false). Its end is `StopWhatIAmDoing`,
    which openblack's mind does not report.
  - `ForceActOnMapCoord` 0x4C6540 (packet 0x5E) is not ported: openblack's creature cannot be sent to act on a point,
    so the handler checks and does nothing. `ForceActOnObject` 0x4C5FF0 is the mind's `leash.actOn`, which the planner
    takes up; its own tests (`IsLeashWorking`, the fight, the life, the desire and `KnowsAction` branches) are not
    ported.
  - `FinishActionUnsuccessfully("pulled by leash", 1, 1)` is `PullAwayFromAction`. Mapped: mind +0x1C14 (`obeying`,
    0x475773 and again at 0x475C1F), +0x380 (`Control::Led` to idle, 0x475BD2), +0x11B8 (`returning`, 0x475C4A), the
    stop (`fn_0048F710(1, 1)`, 0x475B76) and the plan given up (agenda `Clear` 0x475A86, desire +0xF50 = 0x28
    0x475A91). Not mapped: the strength change `ModifyStrength([0xC6DAF0])` when the 3D creature's +0x528C is set
    (0x47573E..0x475751), mind +0x1CEA0[+0xF60] and +0x1C18 = 0, the current sub-action's end call (table 0xBE0C18,
    0x4758E9..0x475914), mind +0x21C0, the dance state, the partner's stop (mind +0x1BE8, 3D +0x528C → +0x4834), the
    required speed, mind +0x1BE0 / +0x1BE4 / +0x20CEC / +0x21F4 / +0x1C74 / +0x658 / +0x1C00 / +0x1C3C, Creature
    +0x10D4 / +0x1068 / +0x109C (the smoke) / +0x10A0 / +0x10AC / +0x10B0, `fn_004F81F0`, `fn_004F4C30`,
    `fn_00475CC0`, vt +0x998 when +0x94 is set, the dead 3D creature's `fn_00484E20`, `fn_004F4890` and the plan's
    actions read between 0x47596B and 0x475A7A, the sub-action agenda's `fn_004FF5C0`, and the reason text shown over
    the creature when [0xD00DF8] is set (0x47579E..0x475821).
  - The tie's own sound is 2D for the local player and placed at the object for another (`fn_005E6BD0`); openblack
    plays it 2D always. The hand's untie plays only for the local player, as the original.
  - Whether the player's leash works (`GLeashStatus` +0x18) is the player's in the original and stays while no leash
    is on (`SetOn` never writes it); openblack keeps it on the worn leash, a new leash working. So packet 0x5D after
    the leash came off, with the flag cleared by a script before, acts in openblack and not in the original. Moving the
    flag to the player is a change of behaviour (it also changes `SET_LEASH_WORKS` with no leash on and a leash put on
    again), for its own measured commit.
  - The script's tie (`TieTo`) also tells the mind to act on what it is tied to; the original's `fn_005E6BD0` does
    not (only packet 0x5C's handler does), so a script's tie differs.
- **The leash tied to a needs sign** ([The leash tied to a needs sign](#the-leash-tied-to-a-needs-sign)): only the rules
  are ported. Parked: the worship totem's `ShowNeeds` and its three signs with their desires (they would add three
  objects a totem; whether they take creation indices before or after the totem's own decides the state hash's
  change, to measure); the mind's kept action (mind +0x21D0..+0x21E0, run once at the free choice `fn_004F0370`, then
  cleared to {0, 0, 0, −1}); actions 246 and 300 in the creature's action set; and what `fn_004C74E0`, Creature +0x94
  and +0x384 stand for in the free-choice test, and the second argument (+0x21F0) of the kept call (unused by
  0x50A760). The fill is a float as the game's 24-bit FPU rounds the division; that its precision control is 24 bits
  here too is taken from the engine's setting, not read at this call.
- **The walk back into the kept area** ([The walk back into the kept area](#the-walk-back-into-the-kept-area)):
  - openblack has no state for +0x12A8 (clearing its route of things in its way), +0x380 = 2 (doing what the leash
    sent it to do), mind +0x1C10 (being teleported) or mind +0x1C38 (sent to act by the player), so the walk back is
    never held back by them. They come with the route follower's blocked state, the leash's tie and point gestures,
    a creature's teleport and the forced actions.
  - The kept area of a creature made for an object (+0x1294 / +0x1298) and the field `[+0x160]+8`.
  - `EndAnyAnimsRapidly`: openblack's body layers keep no state number and no playback rate, so the action being
    played is not sped up; which of openblack's layers stand for the states 2..12, 14, 16, 22..25 and 37 is not
    known.
  - `fn_0048F710`'s four state-ending calls at the start of the walk (0x4C7220's second argument, 1): states 6 → 7,
    5 and 2 → +0x5238 = 1, 3 → 4, 8 → 9, and `fn_004845F0`'s +0x528C = 0 and camera check. openblack's body layers
    keep no state number, so they are not ported. The walk back does not stop the creature (the first argument is 0),
    and neither does openblack's. `StopMoving` (0x484260) sets +0x4990 = 5 × √+0x90 and, only in state 1 (the
    walk), calls `GetStandardBreathTime` and clears +0x51B4; it returns whether it was in state 1.
  - A refused walk: the original has cleared it by the end of the same turn (+0x11B8, +0x380, mind +0x1C14, the plan's
    desire), openblack only at the top of the next, so until then it is still walking back, led and obeying. Only
    something acting between turns (the tug, the debug window) can tell the two apart.
  - Of the `ForceActivityAndForceAction` arguments, the desire, the action (1) and the belief are read
    ([The leash's tug](#the-leashs-tug)); the rest are not decoded, nor what the sub-action agenda's `fn_004FF5C0`
    does. openblack's forced plan is `planner.current` with `planActive` false, at priority 0.
  - The mind array cleared at mind[+0xF60 × 4 + 0x1CEA0] when tied.
  - The fight: the original's test has no fight condition; whether a fighting creature's `ProcessState` reaches it
    was not read. openblack keeps the fight out.
  - With the hand off the land openblack has no hand place: the creature stays kept where it was.
- **The tied lengths and the young creature's home** ([The tied lengths, two creatures leashed together, and the young
  creature's home](#the-tied-lengths-two-creatures-leashed-together-and-the-young-creatures-home)):
  - The player's kind (+0x8E0, 2 = a computer player) is not kept by openblack: the home rule takes every player as
    not a computer player. It is to come with the computer players.
  - What clears the young creature's radius once it reaches phase 5 was not found in `ProcessState`; only
    `GLeashStatus::SetOn(off)` (0x5E7062..0x5E712B) is known to set it to 0.
  - The kept area from Creature +0x1294 / +0x1298 (0x472FDF..0x47303C) and the field `[+0x160]+8` are not identified.
  - The creature's `GetHeight` (vt +0x42C) is taken as openblack's 15 × size; the original's override was not read.
  - A creature with no home kept in openblack is not kept anywhere; the original always has one (+0x1200).
  - A leash tied to a tree that is felled: who calls fn_005E6B90, which unties a tied object no longer available, is
    not read; openblack keeps the tie on the dead tree.

- **The hand on a creature**: allies (`IsAllied` 0x64D5D0) once the game has alliances; the creature's Flags bit 0x400
  (byte +0x25 & 4) and mind +0xF60 / +0xFB4 in `ValidForLockedSelectProcess` (openblack's asleep and frozen stand for
  them); `GoIntoStateOfFinishingAction` (0x477B20) at the lock's start, `LH3DCreature::ReconnectToGame` (0x480730) at
  its end and `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476ED0): the 3D creature's fields are not ported.

- **The player's creature** ([The player's creature](#the-players-creature)):
  - `SET_FOCUS` on a creature is not ported: the creature is not turned to the camera at Land 1's turn 8. It needs
    script control in the mind (the scripted plan, the current action finished, the agenda cleared) and the π/8 test
    against our `TurnToFace`.
  - The fizz after `LOAD_MY_CREATURE` (sound 0x27 and the 3 s fade); its sound tag's own values are not read.
  - Which field of the mind file holds the fatness (the physique's unnamed floats), and whether the mind load
    (`fn_004E84A0`) sets the development phase.
  - `MoveToDevelopmentPhase`'s other steps (+0x11B4, +0x194, the agenda cleared) and its immediate effect.
  - `DEV_FUNCTION` 1 and 4-12; who reads +0x10C0 (`CREATURE_IN_DEV_SCRIPT`) and +0x1108; what +0x110C is beyond
    "the creature its player leads".
  - Whether `SET_POSITION` puts a creature in script state 4 as it does villagers and animals.
  - A land change ([A land change](#a-land-change-the-creature-saved-and-made-again)): `LOAD_MY_CREATURE` does not take
    the file's fatness (ERC_DESIGN E2) or its tattoos yet, so the fatness saved is not read back and the live tattoo
    slots stay empty (the save keeps the file's tattoos for that reason); once it takes them, `ToMindFile` should write
    the live slots. The body's needs, age and energy are not written (their units in our model are not checked against
    the file's). When the profile names no file the
    original saves to the profile's string as it is; ours saves nothing.
  - Skirmish: the original carries no profile creature into a skirmish (see re-check pending in game). The start
    mode 4 (`GGame::Init` 0x54F716) runs `ResetAndStartPlaygroundGame` 0x555990 and no CHL; `Creature::Load` 0x4E7FF0
    is called only from fn_00552AC0 (`LOAD_MY_CREATURE`, 0x6FD2D7) and the creature packet 0x4E (0x63DA4F), which only
    `DEV_FUNCTION(4)` sends (0x6FC270) and `challenge.chl` never calls.
    Checked in the game's data and in openblack (2026-10-09): none of the 19 scripts in `Scripts\Playgrounds` calls
    `LOAD_MY_CREATURE` or makes a creature for `PLAYER_ONE`; their creatures are the computer players' and neutral
    ones (`CREATE_CREATURE_FROM_FILE`). Loading "1- The Norse God" in openblack gives one creature, PLAYER_TWO's. So in a
    skirmish the player has no creature of their own, in the original as in openblack. A look at the retail front end
    (a skirmish started with a profile that has a creature) would close the last doubt.
  - The tutorial's own creatures (`CreaturesInGlade`), which our runs do not reach.
  - **`GET_PROPERTY` / `SET_PROPERTY` on a creature** ([Land 2's creatures](#land-2s-creatures)): not ported are
    STRENGTH on spell seeds, spells and particle containers, ALIGNMENT of a thing's player and of a reward, the
    "Did you want the strength" life fallback, and a script container (the original gives `GET` 0 and runs `SET` on
    each member; ours treats it as a thing that is not a creature). The fight health a version 30+ mind file carries
    is not read (ours starts at 1; the profile's version 33 files have it). Which species field `SetStrength` reads
    (the row from `[[+0x54]+0x28]+0x1F4`, the field at +0x12C of the in-memory row) is taken to be our
    `CreatureArchetype::SpeciesStrength`, from the matching formula, not by offset. The 3D body takes a new alignment or
    strength at its next morph refresh, the original in the native. None of our runs reaches them (`challenge.chl` and
    the run logs read 2026-10-09). Every creature-property `GET` in the scripts `LandControl1` and `SetupLand1` run is
    followed by its `SET`, but those in `CreatureDevLearnToEat`, `CreatureDevGuideTeachesFight`, `HealCombatants` and
    `CreatureGuardianFight`; the fidelity, cycle and hand-demo logs (taken before this port, when every creature
    property still fell to `SET_PROPERTY`'s default) have no "SetProperty" line, so none of the former runs. Of the
    latter, `LandControl1` runs the first two only while `IsSkippingCreatureGuide` is 0 (ip 68042, 68102;
    `CreatureDevelopmentSet` and `CreatureGuideSet`, which also run them, are run by no script); our runs' start-up
    answer 3 makes `SetupLand1` set it to 1 (`CAN_SKIP_CREATURE_TRAINING`, ip 22718-22723), so `CreatureDevSeeHome` also
    jumps past its EXHAUSTION `GET` and `SET` (ip 6096). `HealCombatants` is run only by `CreatureDevGuideTeachesFight`;
    `CreatureGuardianFight` only by `CreatureGuardian` (ip 42908), after its ALIGNMENT `GET` and `SET` (ip 42461-42465),
    which wait for `CreatureDevelopment_Fight` = 1, set only by `CreatureDevGuideTeachesFight`. So the one "GetProperty"
    line, logged at turn 9 right after the first "SetFocus (object)" one, is the `GET` of some other property, not
    identified (it is logged once, so later ones are not seen either). It is not `CreatureDevSeeHome`'s YPOS `GET` (ip
    6037): that reads the highlight just made, which logs nothing, and a `SET` that logs (ip 6040) would follow it
    otherwise. A game started with another answer reaches `CreatureDevSeeHome`'s EXHAUSTION first, then
    `CreatureDevLearnToEat`'s ENERGY and the Guide's; not measured.
  - **SCALE and HEIGHT on a creature** ([What openblack does with the player's
    creature](#what-openblack-does-with-the-players-creature)): the `GET`s on a creature are not ported. The
    original's read the 3D body's size (+0x90: kept to 0.05..4 by `SetSize`, the pen size in the pen, but written
    unclamped by `SetHeight`), which ours does not keep apart from `Creature::size` until the drawn size is; then
    SCALE reads it as it is and HEIGHT × 15. `SET` HEIGHT's write of that size is left out for the same reason. After
    a `SET` HEIGHT the original draws the new size from the next turn's `SetSize`; ours from the next change of the
    drawn size, until it is refreshed every turn. In the pen a `SET` SCALE draws the original's body at the set size
    until that turn's pen shrink; ours keeps the turn's pen pose (`CreatureDrawPose::scale`). `SetSize`'s other fields
    (+0x4838..+0x4858, +0x498C / +0x4990, +0x5228 and the leash's interface statuses) are not traced to what reads
    them. Not ported either: HEIGHT on a puzzle totem, a totem statue and every other thing, `GET` SCALE on anything
    but a villager or an animal, `SET` SCALE on anything but a villager, an animal or a creature.
  - **Land 2's creatures** ([Land 2's creatures](#land-2s-creatures)): no run reaches them. The map cycle only loads
    `Land2.txt` while the challenge script stays in `LandControl1`, so `LandControl2` and its `LOAD_CREATURE` never run
    (no "not implemented" line for it in the cycle's log; Land 2's turns hash an empty "creature" part). They need the
    land change through the challenge, which also brings the player's creature: without it both Setup scripts wait on
    `THING_VALID(MyCreature)` for ever and never set the alignment or the friendship. Not ported yet: the
    computer-player natives. What the version 25 files give through
    `fn_004E84A0` (size, strength, alignment) is the same open point as `LOAD_MY_CREATURE`'s. A file the original opens
    but our reader refuses would still make a creature there (with whatever mind `CheckValidation` leaves); ours makes
    none. Nor does ours ever refuse to make the creature where `Creature::Create` gives NULL (out of the land's bounds
    with a player, the owner's citadel without a heart, or the heart without point 0): `CreatureArchetype::Create`
    always makes one, so in those cases ours has a creature where the original has none. The home: `Create` takes the
    heart's point 0 and `ProcessState` point 15 at once, ours point 15 from the first creature turn. Who reads
    +0x1108; `fn_004C9FE0` after the mind load. The texts 5001 and 5002 were read in the Spanish data only. Ours takes
    up the mind file at the mind's first turn, so the stage `LOAD_CREATURE` and `SET_CREATURE_DEV_STAGE` set before it
    is then replaced by the file's (capped at 13), where the original moves to stage 13 after reading the file; the
    name (`SET_CREATURE_NAME`) is kept over the file's, the stage is not yet. The original writes the name into the
    creature at once, whatever its mind is doing; ours keeps a waiting name until the mind's turn, which only walks
    creatures that also have an animation and a transform, so a creature made without them (for instance the Guide or
    the Guardian, should our `CREATE` make them so) would keep the name waiting. A mind saved while the name waits is
    saved with it; a mind not set up yet is not saved at all.
    `CREATURE_SET_KNOWS_ACTION`: (a) ours keeps what is known as one flag per row of the game's tables, so a kind other
    than 0 and 1, or an action past its table (Land 2's scripts teach skill 6, the skill table has rows 0-5), is
    dropped, where the original's list keeps it (and saves it in the mind file); what reads a skill 6 in the original
    is not read. A file's out-of-table entries are dropped the same way when ours takes a file up. (b) The original's
    list is in the order the actions were learnt; ours saves the file's own order, then the rest by number. (c) The
    learning message (`fn_004C9FE0` codes 6 and 7) is not shown: ours has no learning messages yet, for sightings
    either. (d) Ours does nothing for a thing that is not a creature, where the original goes on with a null creature.
    (e) What the script teaches waits in `scriptKnows` until the mind's first turn, and that turn (as for the name above)
    only walks creatures that also have an animation and a transform: a creature made without them never sets up what
    it knows, so the edits wait for ever and none is applied, where the original applies each at once.
    `CREATURE_AUTOSCALE`: ours steps right after the spells, as the original does (`ProcessSpells` 0x472EBB, then
    0x472FB7), so a size spell's easing and its end meet the step in the original's order. Easing reads the spell's
    `before`, and the spell's end writes `size = before`, so the other order would change every value while big or small
    eases or ends on an autoscaled creature, not only the first. Still open: (a) the original runs the whole turn
    creature by creature (`ProcessState`), so a rival processed before the local creature reads the local creature's
    size before that creature's own spells of the turn; ours runs every creature's spells first, then every step. The
    order of the creatures in the original's list is not read. (b) Ours runs the pen (`ShrinkInPens`), the physiology's
    growth and the other creature systems before the spells and the step, the original runs the pen after the spells and
    before the step, and grows the creature after the step (0x473337; see
    [Growth, size and age as time passes](#growth-size-and-age-as-time-passes)). (c) The drawn size: the original
    draws the creature at the size `ShrinkDownIfNearCitadel` gave it before the step, so a step may show a turn later
    there. Ours rewrites the transform's scale only when the physiology changes the size (`TakeShape`), so a step shows
    at the next turn's growth at the earliest; whether some other call in the original sets the drawn size in between is
    not read. (d) Ours takes the hand's grip on the creature for the locked select's flag (`Flags +0x24 & 0x10`, tested
    at 0x472F77; set by packet 0x1B, cleared by 0x1C, with the action states 3..6 in between:
    [hand-and-interface.md](hand-and-interface.md), "The locked select on a creature"; ours takes hold at the start
    through the creature hand's `Grab`); that the flag is set and cleared exactly with that grip is not checked.
    `CREATURE_FORCE_FRIENDS`: (a) the list is stored only; its readers in the original (a friend cannot be attacked,
    `CanBeAttackedByCreature`; the activity choice in `GetActivityObjectUsefulness`) are not ported, and what the list
    at +0x12A0 that `CanBeAttackedByCreature` tests first holds is not read. (b) The original drops a deleted friend in
    `RemoveBeliefsAboutDeletedObjects`; ours keeps the entity of a destroyed creature in the list (nothing reads it).
    (c) Whether the list is saved with the game or the creature is not read; ours saves it nowhere.
- **The initial desires**: the meaning of the fields is now read (decay drawn in [+0x50, +0x54], cap +0x4C; see
  [The creature tables in info.dat](#the-creature-tables-in-infodat)). The original has no draw-free set-up: a mind
  file is read into a creature whose desires were already drawn, and keeps their decays. Still open: whether the 40
  draws fall at the same place on the synchronised stream (the original's inside `Creature::Create`, ours on the
  creature's first mind turn).
- **The creature's own eye blink**: stream, interval, and whether the high-detail villager's hold and squint are shared.
  Until read, the blink goes on the CRT stream, marked approximate.
- **The mind, plans, mimicry, fidgets, fight and poo draws**: no decompiled function, so "synchronised" is a rule, not a
  reading; each must be confirmed as the classes are decompiled.
- **The puke and the cave's smoke, spray and mist dome**: whether they are particle effects in the original, and with
  which `NET_GAME_TYPE`.
- **The temple leash** ([The temple's leash posts](#the-temples-leash-posts)): our heart makes its posts and keeps the
  pick, the frame draws them, the hand feels and taps them, and packet 0x65 sets the leash. Still open:
  - the post's game position: the original's `Object` takes `MapCoords(point)` (x and z in fixed point, the altitude
    above the land); ours keeps only the point, which is what the post's 3D objects and the collision use. Nothing in
    our tree reads a post's map position;
  - the land under a post is read with `GetHeightAt` at x and z through the map's fixed point, for the original's
    `LH3DIsland::GetAltitude` (inferred, as for the hand);
  - whether the original's leash keys go through packet 0x65 (and so set the temple's pick); ours change the
    creature's pick only (`LeashSystem::ChangeType`);
  - the creation index: our heart now skips 5 (heart, entrance, three posts), as the original. The heart's base chain
    (CitadelPart 0x4693F0 → MultiMapFixed 0x52E1E0 → FixedObject 0x52DDC0 → `Object::Object` 0x636520, which counts
    at 0x6365D2) takes one, and `fn_004651D0` makes a `Flock` (0x52F780, a GameThingWithPos, no index). What is
    left: the worship sites (fn_00464F50), made after, are not ported yet;
  - which matrix the hand's +0x80 is (the first of `Morphable::TransformedMatrices`; inferred: the root bone, ours
    the hand's matrix by its first bone);
  - packet 0x65 with no creature: the original still sets the player's leash status type; ours keeps the leash type
    only on the creature (`ChangeType`, which also needs the creature to know the leash, where the original sets it
    unconditionally), so with no creature only the temple's pick moves, and a leash the creature does not know sets no
    mood;
  - the refusal's spell test: the original's `IsCreatureSpellActive` 0x4F51E0 reads the slot's +8 (not 0); ours
    `Spells::IsActive` (phase not Off, a spell waiting to start included). Whether +8 is set while a spell waits is
    not read;
  - the leash's mood "while a leash is on": the original asks any of the player's interface statuses
    (`GetInterfaceStatusLeashOn` 0x4CF060: a leash status with +0x14 set); ours the creature's own worn leash
    (`LeashSystem::ChangeType`);
  - what the block of g_game +0x25006C that packet 0x65 clears for the local player is;
  - the known leashes after `LOAD_MY_CREATURE`: the native's creature is made in zeroed memory and nothing read in
    its path sets +0x1108..+0x1114 (only packet 0x4E, multiplayer, does), so on Land 1 they come from the
    `DEV_FUNCTION(2)` and `(3)` of turn 30, in both trees. Not read: whether the mind's load (fn_004E84A0) writes any
    of them; ours sets none there;
  - `DrawNow` skips the posts' draw (and the lantern) while heart +0xB8 is set and +0xBC > 0 (0x467262..0x46727E,
    read as the heart being destroyed; not identified): ours does not test it;
  - the smoke sprite's screen test (`LH3DSprite::AddDrawing` 0x840C70 not read): ours, a sphere of its half width;
  - `SetDynamicLighting(1)` of the collar, drawn here with the objects' own light (the land's at its origin and the
    engine's one light), inferred; the scroll reaches the shader in 1/256 steps of u (`PackUvOffset`), approximate;
  - the hand's hide: +0xAC is set by fn_0046C2E0 (9 callers) and cleared by `CHand::Show`; ours reads the hand
    entity's `NotDrawn`, which only the INVISIBLE state sets; the other callers are not mapped. The draw and the
    hand's pick read it the same way;
  - the order of the hand's invisible spheres: the original sends each from its owner's draw (the posts from the
    heart's `DrawNow`), and only an exact tie between two hits would show the order; ours tests every mesh (the
    temple's entrance last), then the stones, then the posts, and the creature's posed body after them, each of these
    taking the cursor only when strictly nearer, so on an exact tie the earlier hit keeps it. The original's order
    (the order in which the frame draws the posts' heart, the stones and the creature) is not read;
  - the post's sphere in the original's hand collide: it shares the stones' (approximate) depth reading
    (`hand_pick::InvisibleSphereAlong`: the ray's length to the centre's plane, not the projected w).
- **The leash draw** (`fn_008491B0`, not decompiled; [Drawing the leash](#drawing-the-leash)): the shadow's material
  (the port's `AlphaTexturedAlphaNoZWrite` is inferred), whether the rope takes the land's light at each corner,
  whether the leash is drawn in the sea's reflection (raffclar's tree draws the rope there, ours does not), and where
  the shadow's alpha 0x41 and its fades at the ends are set.
  Whether the original sorts the rope among the blended things; the comment on `leash_draw::Draw::middle` in
  `Graphics/LeashDraw.h` says it is sorted by it, which the port does not do, and needs fixing on the code side.
- **The leash rope in the citadel and while paused** (read 2026-10-08): the rope step (`Leash.cpp` `fn_00848970`,
  sub-steps of 0.005 s, `n = 1 - ftol(dt * -200)`) runs from `GInterface::UpdateAllLeashes` 0x5D9130, called by
  `PostDrawProcess` 0x5CEAB3 every frame with no inside test, so it keeps running in the citadel; only the drawing
  (`DrawAllLeashes` 0x54DF18, on the land path of `Process3dEngine`) stops there. Paused (dt 0) it still takes one
  0.005 s sub-step per frame with its ends pinned; the port's `UpdateLeash` runs ungated and, since 2026-10-09, takes
  that sub-step too (`leash_rope::Step`: 1 + the whole 1/200 s steps, with no cap, as the original; before, 0 steps
  at dt 0 froze the masses while the ends followed the hand and the collar, so a paused rope showed two straight
  pieces, and a frame took at most 200 steps). `GoInsideCitadel` 0x553E10 also turns the player's own leash off
  first (packet 0x5B at 0x553F5C, handler 0x63D3CE → `GLeashStatus::SetOn(creature, 0)`), which the port does not do yet (with
  C34). Not read: when that packet applies while paused, and interface state 0x15 (the drag ropes, the same step).
  The step's physics is read in [The leash's rope](#the-leashs-rope).
- **The rope and the frame rate (an open decision, outside the rope's code)** ([The leash's rope](#the-leashs-rope)):
  the original steps the rope `1 + trunc(200 × dt)` times a frame, so always at least one 5 ms step, and the rope runs
  `5 × n / ms` times real time: ×1.06 at 30 fps, ×1.18 to ×1.25 at 60 Hz (16 / 17 ms frames, n = 4), ×2.5 at 500 fps,
  ×5 at 1000 fps, and with no limit on frames of 0 whole ms. The original presents with vsync by default: the
  `LHScreen` constructor (bw1-decomp `LHScreen.cpp`) sets `flipFlags = 1` (DDFLIP_WAIT) when the BWSetup `VSync`
  registry value is missing or not 0, and the machine it was read on has no such value, so it runs at the
  monitor's rate. openblack drew as fast as it could (hundreds to thousands of frames a second) until vertical sync
  became on by default, as the original's (`EngineConfig::vsync`; `--no-vsync` turns it off, like the original's
  `VSync = 0`, DDFLIP_NOVSYNC, and the automated runs pass it). In the model, with the start jerked 10 to the side,
  the middle mass settles in about 1 s at 1000 fps and still swings after 4 s at 60 Hz (overshoot 7.33 at 1.5 s);
  the rope's own step rule stays the original's. Fixed-step runs advance per frame, so
  no per-turn hash, random trace or screenshot moves; only their wall time can grow (900 frames at the refresh rate).
  Every other per-frame system that depends on the frame rate (the hand's spring, for one) changes with it. Not read:
  whether the user's original runs windowed (a windowed `LHFlip` blits with no vsync, uncapped like ours), and who
  reads a mass's +0x24 in the draw (`fn_008491B0`).
- **How the original picks a species' body meshes**: it does not walk `Data/CreatureMesh` as we do; our skip of names
  whose last part is not an appearance is a port decision.
- **The species file's mirror bones** (read 2026-10-08): the creature block ends with each bone's mirror bone, the bone
  on the other side of the body, as many numbers as the base mesh has bones. The parser keeps the block's last numbers
  (`GetCreatureTail`, at most 512) and its version (`GetCreatureVersion`), which the rig keeps as `fileTail` and
  `creatureVersion`. The bone the eyes are scaled by (`rightEye`, the left being its mirror) is read beside the leash
  bone. Nothing uses either yet: the mirrored animations still pair bones by their rest pose
  (`skeletal_animation::MirrorJoints`). Still to port from his tree: taking the last numbers as the table (the bones
  counted in the mesh, a table whose numbers are not bones refused) and the version 20 rule, where the first bone's
  mirror is taken as the second's with the first its own; that rule is his and is not read from the original. Which of
  the block's other numbers hold what, and where the original reads the table, are not read either.
- **Animation blending**: which angle decomposition the original uses when it blends a creature's variant animations.
- **The creature in the turn**: the original runs it inside `Living::ProcessLiving` ([villagers.md](villagers.md)); the
  port will first run it as one block after the living list (approximate), per creature later.
- **Route planning and walkable land**: his A* lattice is replaced by our route follower and `land_avoid` mask; his
  heading convention against the original's (3D angle + π/2, [engine-math.md](engine-math.md)) is unchecked.
- **The creature's radius** ([The creature's radius](#the-creatures-radius)): `object::Get2DRadius` / `GetRadius`
  are ported. Still to do, each its own measured change: the locomotion's own radius (his default 5, and the measure
  from the mesh's box, half its width plus length, halved, approximate) should become `GetNavRadius` 0x480A60
  (`Creature::size / ClampScale(drawn size) × radius`), which the route plan reads at 0x6384D8; the creature branch of
  `GetRoutePlanRadius`; and where `land_avoid::k_CreatureRadius` (7.1) comes from. Not read: a memory read of +0x8C /
  +0x5224 / +0x5228 in the running game, which would confirm the bits. The timing: the original sets +0x5228 in the
  creature's turn (spells before the shrink, autoscale after), ours follows `Creature::size` at once, so a growth step
  shows a turn earlier than in the original, as the drawn scale already does (not measured). `SampleCycle` normalises
  the drawn bones by their exact length where `fn_00860E00` uses `InverseSquareRoot`, so our drawn bodies differ from
  the original's by up to about 2·10⁻⁵ per bone level (not measured on screen). `BoneReach` gives a bone the stand
  does not key no turn and no move, where the original reads the animation's default frame for it (+0x47FC, argument
  8); not ported, and unreachable on the shipped data, since every species' stand keys every bone of its base mesh.
- **A creature's defence and the pen**: the defence divisor (`Creature` 0x478C00, (clamp(size, 0.001, 2) × 1.5 + 1))
  takes LH3DCreature +0x90, the size it is drawn at, so in its temple's pen the pen size; ours takes
  `creature_pose::DrawnSize` (since 2026-10-10).
- **The needs read back and the temple shots** (measured 2026-10-10): with the mind file's needs a loaded creature on
  Land 1 sleeps longer (its sleep sequence's loop is still playing at turn 105, where the species' defaults had it end),
  so in the temple shots at frame 700 (entered at turns 120 and 120,200) it still lies asleep in front of the cave.
- **The pen size's readers** ([Everything SetSize derives](#the-players-creature)): since 2026-10-09 these follow the
  drawn size (`creature_pose::DrawnSize`): the locomotion's speeds and stride; the animations' playback rate
  (`UpdateTime` 0x481E45..0x481E66, 1.6 − 0.425 × +0x90, also the turning's and the object actions'; the original
  keeps the frame's step as an integer, AnimTimeInc +0x48BC = ftol(ms × rate), where ours keeps the float `ms × rate`:
  `PoseBody`'s playback time and the object actions' `k_TurnMs × rate` a turn, so ours runs up to just under 1 ms a
  frame ahead, separate); the breath period
  (5·√s, `SetSize` 0x480628..0x48063C); the leash lengths (`fn_005E6980`, refreshed each turn); `Creature::GetHeight`
  (0x477F50, +0x90 × 15) and `GetScale` (0x47B190 → `GetUserSize` 0x4EF4F0, +0x90); the hand's holding and touch
  height (`ObtainRequiredHandPosition`, through GetHeight); the follow camera's height (`CameraModeFollow::Update`
  0x44C541, 0x44C6C8, 0x44C744); the sound size key (`fn_00483290` 0x483337..0x483360, clamp(ftol((2 − +0x90) × 1.5 +
  1), 1, 3)); the footprint's side (0x483582) and the hair's scale (0x4ED548). The duel's readers too.
  `AttemptAttack` 0x489CE0 takes each blow's reach and height from a per-blow table times +0x94 (so in mesh units;
  0x489F68, the height against the opponent's), the back (anim 0x8A) and forward (0x89) steps' strides × +0x94
  (0x48A1E3, 0x48A20C), and lands a blow only between 0 and 7.5 × +0x90 deep (0x48A21A). The opponent's height there
  is `fn_004867B0`, its head bone in the drawn bone buffer (`fn_004813B0`, bone +0x51B8) above the ground, so it
  follows the pen size as well; ours stands in for it with 15 × the opponent's drawn size. The fight's action step
  0x48A5E0 moves the body by the animation's displacement × rate × IntTimeInc × +0x94 / duration (0x48A651); its blow
  test casts rays 0.2 × 15 × +0x90 long, twice that for anim 0x82 (0x48A96B), from the blow's bone points against the
  opponent's drawn body (`fn_00483160`, `fn_008683C0`); the height share a blow lands at is the table's height × +0x94
  over `fn_004867B0` (0x48AD89); the damage factor is (+0xAC + 3)/(the opponent's +0xAC + 3) × +0x90 / the opponent's
  +0x90 × the blow's speed, within [0.04, 2], doubled for 0x82 (0x48B064..0x48B0CA). Ours takes `DrawnSize` and
  `DrawnScale` in each. Still different in the duel: ours measures the reaches once a fight, at the drawn scale of
  that moment, where the original multiplies its table by +0x94 at every attempt (a fighter whose pen size changes
  in a fight keeps its first reaches); ours takes the forward step's length for steps back too; ours takes the gap to
  the opponent as the distance less its locomotion radius, where the original marches 11 points spaced
  0.15 × 15 × the opponent's +0x90 (0x489E12) along the way to it against its drawn body; the press's band on the
  opponent is openblack's, its original not traced: ours divides the point on the opponent's body (BodyOf's capsules,
  placed at the drawn scale) by its real height (15 × `Creature::size`), so in a pen the drawn body and the band's
  divisor disagree; which size the original's band reads is to be traced before either is changed. Not
  ported: the blow's dust through `fn_00845C20`, 40 puffs sized by the opponent's +0x90 × 0.1 (0x48AF4A) and 8 by the
  attacker's +0x90 × 7 (0x48B039). Still on the real size, their sources not traced: the fight's arena radius (39 per
  size), the fighters' places in it and their arrival distance, the distance a fight is picked from (60 per size)
  and the faint's length (8 + 4 × size seconds); the leash's anger reach (8 × the creature's height, its
  source not read), the eyes (not read) and the head's look height (the original's look origin not read). In ours the
  breath period still eases towards 5·√s, where `SetSize` writes both its current and its target period every turn.
  Our tied leash's lengths branch on a mobile (7 heights, at most 40), the original's
  `UpdateLeashLengthWhenAttachedToObject` 0x5E6A0F on a tree (6 heights, at most 40): separate. Not ported: the head look rate (+0x4858), the current-speed cap (+0x4838), the turning start (0x48122B), the extra head
  and feet scales (0x4EC450..0x4EC510), the body-point kicks (0x482584), the mass (0x47FA80). In the locomotion, still
  different from the original: the target speed is stored once per `SetRequiredSpeed` call (0x47FA6E, its only
  writer; the walk reads it each step at 0x491325), so a move started outside the pen keeps its larger target inside
  it until the next call, where ours recomputes it from this turn's speeds (which creature states call it when is not
  traced); the run weight is not clamped (0x48DA43), ours is; the walk and stand times are integers (ftol, 0x48D9D1,
  0x48DA71), ours floats; the stride's order of operations: the walk takes (stride × +0x94) × weight and advances by
  duration × (distance / stride) (0x48D9BA..0x48D9CB), the walk and run blend takes (the weighted strides' sum) × +0x94
  and advances by the same duration × (distance / stride) (0x48DA59..0x48DA6F), where ours takes weight × stride × scale
  and (duration × distance) / stride, so in float the advance can differ by an ulp before the original's ftol (its own
  measured change: it reaches the walk outside the pen too); the step's time (0x483E2A..0x483E47: displacement ×
  +0x94 / WalkSpeed × … × 1000) against ours, the animation's duration (not traced to the end); the slope's point ahead and divisor are the pen radius
  +0x5228 (0x491273, 0x4912E1), ours the own size's approximate radius. Whether the creature walks in its pen on
  Land 1's runs is not in the logs (the hash has no creature-position part): to measure.
- **The creature's route** (`RouteFollower::SetDest`): the original's values for a creature are not known. The port
  passes the arrival ring's outer radius and inner radius as the plan's first two values, the creature's radius, and
  (distance + 1) × 5 as the longest route (the footpaths' factor), with 64 search turns a game turn and no obstacle hook.
  When the route ends with a re-plan in flight, or the destination is inside an obstacle, the creature stands.
- **The fight by the turn**: a blow's clock moves 100 ms at a time, and what goes past the end of an animation is lost as
  the next one starts, as in his code. A blow of 1000 ms at the speed of a blow (0.66) takes 16 turns (1.6 s) where
  his 30-frames-a-second play takes 46 frames (about 1.53 s), so a duel goes about 4 % slower; each blow still lands
  once. The original's own fight step is not read.
- **What a creature does to things** (approximate): the force of a creature's blow on a home (his 3.75 × size is
  dropped for the thrown-object damage), whether an eaten villager dies with a reason the town counts, and where the
  original takes a creature's palm. A dead tree it walks round only while it burns and no script controls it.
- **Water for a creature**: the sea is taken as the cells of the walkable mask it can't reach whose middle is at the
  sea's level, searched 8 cells round; the original's search is not read.
- **The desires' floor**: the original holds a desire at or above the species' `desireFloor` (creature info +0x274,
  0x4DC4A8); ours uses 0. Both info.dat files hold 0 in every row, so only a changed info.dat would show it.
- **Footprints on April Fools' day**: when the original reads the date is not read; his code read it at every print,
  the port once per land.
- **The hand on a creature**: the click threshold (his 1000 ms against our 225 ms grab press); the 0xE85 interact
  tooltip's owner rule (his: own awake creature only; check 0x5D7406).
- **Creature audio**: whether `SET_CREATURE_SOUND` 0 mutes only other creatures' voices (his scope) or all their
  events; his species fallback table; the creature music, heartbeat and Guidance ([audio.md](audio.md)).
- **Surface rules**: his snow at depth 27 and material type 0 extras, kept out of the shared `GetSurfaceType`.
- **Fights**: the wound type per blow (his guess) and the tier-1 AI gap he notes.
- **The fight camera** ([Fights: the camera](#fights-the-camera)):
  - which moment of our fight 0x5036C0 is (it runs once fn 0x484410 has taken the arena; ours asks the camera as the
    fight starts), whether both creatures run it (the camera would then be asked twice), and which of our two fighters
    is the arena's +0x38;
  - the end ([Fights: the camera](#fights-the-camera)): of the three ways into the linger at 0x5038A6, (a) the body
    action being over and (b) a win by the opponent's life are not matched: our duel is lost by its fight health, and
    a miracle's knock-out forces a show-off plan on the winner (`ApplyEffect`) instead of running this sub-state, so
    which of our ends (the winner's `Win`, `Leave` after the stages before or after the duel, `AbortFight`) is (a) or
    (b) is not known; none calls `EndFight` for the local creature beyond `EndFightFor`'s own rule. The remarks of (b)
    and (c) (fn_0071CDF0, fn_0071CD70), reaction 0x28, and fn_004845F0's other caller fn_0048F710 (0x48F722, with an
    argument) are not ported either;
  - the readers of the watched flag: `GInterface::ResetActionState` 0x5D2A3A sets the interface state 0x10 (the fight
    music, the user-parameter-4 sound filter) only when fn_005D2990 holds, which needs g_game+0x250538 (not named) as
    well as the interface player's creature fighting; the interface action at 0x5D4B16, `HandStateHolding::Update`
    0x5B5F61 and the draw through fn_00456270 at 0x5CCCCC: what they do is not read. None is ported.
  - the arena list ([Fights: the camera](#fights-the-camera)): the original reuses an arena near the place
    (fn_00424E00, keeping the larger radius) and keeps it as an object of its own until `GArena::ToBeDeleted`
    (0x424960, not read); ours is made for every fight and lasts while its maker's `CreatureFighting` does. Whether
    `IsAvailable` turns false when the fight ends, and so whether the two agree, is not known; nor is the script arenas'
    altitude, nor whether fn_00424FB0's shrink until the place is free matches `ArenaRadius`. The script's arenas
    (`GScript::GetArena` goes into the same list) are not in `GetArenas`: our `GET_ARENA` is not implemented, and a
    land's `CREATE_ARENA` is a `components::Arena` of its own.
  - the arena's middle: `GetArenas` gives it quantised as the original's MapCoords stores it, but the fight's own
    sums (`creature_fight::Arena::centre`, the places the fighters walk to, the range checks, and the middle
    `FightWatch::StartFight` is given) still use the float middle, where the original's `GetPos` on the arena reads the
    quantised one (0x5074B1..0x50750C, then `StartFight` 0x45A646..0x45A68B); they differ in the low bits. Which of the
    fight's own sums read the arena's MapCoords rather than the creatures' positions is not read. Also not read: how
    the camera turns `GetPos` back into metres (x / 6553.6 or the MapCoords getter), which may differ from
    `map_coords::ToMetres` by one unit in the last place.
- **Fainting and the fight's additions** ([Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting)):
  - what becomes of a fight when a fighter faints by something other than a lost duel (the tornado,
    `DestroyedByEffect`): `Creature::Faint` has no fight code, and what its forced plan (desire 0x18, action 9) does to
    the fight and to the opponent is not read. raffclar's `ForceFaint` leaves the fight with no winner; it is taken as
    he wrote it;
  - the rest of `Creature::Faint` against our faint: `EndAnyAnimsRapidly`, the forced plan, mind +0x1C14 and
    fn_004C9FE0(0x25, 3, ...); ours drops what it holds and plays the faint sequence, his model;
  - his two other additions are not taken: a fallen creature waiting, before it rests or gets up, until its spells are
    over (`CreatureKnockedOut::Stage::WaitingForSpells`), and deciding as it faints whether it will rest
    (`CreatureKnockedOut::rest`; ours decides when it has lain long enough). The original has a sub-state at 0x508C10
    that finishes the creature's spells and those of the creature its plan's belief (+0xF58) points at, and is done (2)
    only when neither has any left; which activity runs it, and when the rest is decided, are not read;
  - the flags [0xC6414C] and [0xC6414D] that keep a miracle's blow from a fighter, and `ApplyEffect`'s other
    effects in a fight (the 0.1 share, the heal into the fight health, the opponent's show-off), go with the miracle's
    side (R22).
- **Leash gestures**: the shake gesture's real thresholds; the `AttachLeash` / `DetachLeash` / `FocusLeash` help
  events' trigger sites.
  - **The SCRIBBLE with a leash in the hand** (parked): `GInterface::ProcessPowerUpSystem` (W120 0x5CF300, Mac
    `ProcessPowerUpSystem__10GInterfaceFv`) has no body in bw1-decomp, and its step 4 in [magic.md](magic.md) lists
    only the held object and the most charged icon. Whether a leash held in the hand is shaken off there, and before or
    after the held object, needs that function read. Until then `TakeOffHeldLeash` is not called and the gestures are
    as before.
  - **The SQUARE_SPIRAL leash picker** (parked): raffclar's tree has none, and the wiki has no numbers.
- **The leash keys**: the original's key path (packets 0xF / 0x10, [audio.md](audio.md), not read): whether it acts at
  the next turn or at once, and whether it needs the creature leashable as the port does.
- **Leash natives, openblack's differences** ([What openblack does](#what-openblack-does-with-the-leash-natives)):
  - `GET_OBJECT_LEASH_TYPE` reads the pick at the player's temple, as the original (0 with no temple heart, -1 with
    nothing picked), and the posts' tap and packet 0x65 set it as the original (the tap at once, the packet at the
    next turn).
  - `ATTACH_OBJECT_LEASH_TO_HAND` and `ATTACH_OBJECT_LEASH_TO_OBJECT` put the leash on through `WhyNot`, which refuses a
    creature that is not the one its player leads or that does not know the leash; the original's natives test only
    "a creature with a player" and then turn the leash on. Whether the original's `SetOn` (on path, 0x5E6F70) refuses
    anything is not read, so the refusal stays.
  - `TOGGLE_LEASH`: the port does nothing for a script player out of 0..8 (`ScriptPlayerToGamePlayer`'s range); the
    range checks in the original's conversion are not traced. Its creature checks (`fn_005D06E0`: vtable +0x2C,
    Creature +0x1110, the interface status's leash state) are not compared with `PressKey`'s.
  - The original keeps one leash state per player, not per creature: `IS_LEASHED_TO_OBJECT` and `SET_LEASH_WORKS` on a
    creature of the player that is not the one they lead act on the player's leash there, and on that creature's own
    (empty) leash here. `SET_LEASH_WORKS` while no leash is worn sets the original's flag but nothing here (the port's
    flag lives on the worn leash). Who reads the flag is not known.
  - The "has a player" test (vtable +0x1C) is not ported: which creatures have no player is not read.
  - When both things of 185 / 269 are creatures, the roles are taken from the CHL syntax (the creature pushed first);
    not checked in the x86.
- **Mimicry** ([Mimicry and the town hooks](#mimicry-and-the-town-hooks)): four deeds are reported (11, 16, 21, 33).
  - The follow-up hooks commit, with every number now read: the resource deeds 0, 2, 4, 7, 9, 43-45 (the pit's deed is
    already computed, its draw made, and dropped); the spell hits through `fn_004E9DF0` (1, 3, 5, 8, 17, 18, 20, 33, 34,
    42; needs `GMagicEffectInfo` +0x98 from `info.dat`); the landing deeds 32, 35, 36; BUILD_HOUSE 6 (the scaffold's
    owner); BREAK_ROCKS 37; DAMAGE_BY_THROWING 15 and the physics' own 16 (the thrown body as the thing).
  - Waiting on other ports: PLAY_WITH_TOY 41 (`IsToy` of `Ball` and `MobileStatic`), the disciples 22-31, MAKE_ARTEFACT
    14 (artifacts), SACRIFICE 40 (worship's sacrifice), the animal's THROW_IN_THE_SEA (who last dropped an animal).
  - What the original does with `magic` (`Creature::MimicPlayer` 0x4EA670 is not read), the per-action table at 0xCAB220
    (priority, flags, filled from `info.dat`) and the gate's steps 1, 2 and 6.
  - The name of `GInterfaceStatus` +0x128 (the header's `LeashStatus`, compared with a `GPlayer*`); physics flag +0x1D8
    & 4 (FROM_HAND), creature +0x1268, player +0x8E0 == 2; whether the disciple off-by-one is the original's bug or the
    `VILLAGER_DISCIPLE` enum is wrong; no cross-check against the Mac build.
  - Code comments to correct: `MapShield.cpp` (a creature mimic after the aggression slot: it is help-spirit guidance)
    and `VillagerSatisfy.cpp` (the creature's activity desire is always 0).
  - The animals' `HasSunk` availability test (`Living::HasSunk` 0x5ED375), missing from openblack's animal branch.
  - No test pins the PLANT_TREE site in `HandSystem::Replant`: it is reached only through a real hand system's end of
    physics for a tree (the hand's set-up, the terrain, the smoke and the spot visual). `Consider`'s own tests pin the
    deed's fields; the site's place is checked by reading.
- **The town hooks**: the mind's side of the empathy (0x4C80F0: the clamped add, the ×0.9995 decay per turn and the
  even-turn history), which the three published sites wait for; the worship-site calls (states 171, 9 and 7) and the
  spell's (`fn_00721730`); `Town::UpdateAttitudeToCreature` 0x7437F0 (step 16; the names of attitudes 0 and 2,
  `fn_004C9FE0`, mind +0xF50 / +0xF58, Creature +0x160, the actions at the Fear indexes; the help texts 1083-1087); the
  help-sample groups 0xD99D08 and 0xD99DA0 of the aggressor guidance; possible readers of the town-desire empathy
  array through registers or pointers (only the immediates 0x18C80, 0x18D20, 0x18DDC and 0x1A1DC were scanned).
- **Leash natives** ([Leash natives](#leash-natives)): no native body is in the decomp, everything is our reading of
  the x86. Unnamed: `fn_005D06E0`, `fn_005E6BD0`, `fn_005E6EA0`, `fn_005E7140`, `fn_004648E0`, `fn_00464920`; the
  meanings of `GLeashStatus` +0x18 / +0x1C / +0x24 and Creature +0x1110 are inferred from use; creature vtable slots
  +0x2C and +0x34 not checked (read from the error strings only); who reads `GLeashStatus` +0x18; the range checks in
  `ConvertScriptPlayerToGamePlayer` 0x6EB9A0 and `GGame::GetPlayer`; no shipped CHL `Enum.h`; PPC Mac bodies not
  disassembled.
- **The mind-file cipher**: whether saving a mind file reseeds anything (the `srand(time)` at 0x577721 is in
  `creature.lhp`'s export, not in the mind file's save; [engine-math.md](engine-math.md)).
- **Saving a mind as the creature itself** (`SaveMind`): read `ScriptCreatureCurse::Init` (0x6F6190) and
  `ScriptCreatureCurse::ResolveLoad` (0x6F61E0): what Init takes from the creature (the before-spell values or the
  script's `Original*OfMyCreature` globals), what ResolveLoad puts back, and whether the mind file's save (the "Save
  Mind" path in `CreatureMentalSaveAndLoad.cpp`) reads the record, the script's globals or neither. If neither, the
  curse branch of his rule (ours walks the running tasks for the script's name and the globals by name) is to go;
  whether the values are written straight into the file's physique or into the creature first; whether the save also
  tests the player, and whether the fatness and the tattoos follow the same rule. openblack has nothing that saves a
  mind yet, so the path is unreachable until a saver (the spawner window or a profile save) calls it.
- **Playground creatures** (`CREATE_CREATURE_FROM_FILE`, playgrounds and `comp.txt` only): they now start at their
  species' `startScale` and start body (not 0.3), their `Transform` scale is the drawn scale, and the script's type 0
  is taken as the Giant Ape. The original draws a creature at size × 15 / rest height
  ([The creature's radius](#the-creatures-radius), +0x8C).
- **The creature's row in the map cells' type** (`map_cells::TypeOf`) is now the species' row, inferred.
- **Spells on a creature**: where the turn calls the creature spells (the port will call them from the creature's turn,
  approximate); the rest of a creature's own damage (`Creature::ApplyEffect` in a fight, the fight health in
  `ReduceLife` / `IncreaseLife`, `BeCutAndScarredByEffect`, the opinion of a casting creature; out of a fight miracles
  and fire now hurt it, and a kill faints it: [magic.md](magic.md#a-creature-taking-an-effect));
  the creature as a spell's creator (`MaintainSpell`, `UpdateSpellInfo`, read now:
  [above](#casting-a-miracle-the-originals-steps-w120-read-2026-10-09)); the lightning bolt's creature test (its
  +0x12B0 is not named), a tornado carrying a creature, the three flames on a burning creature (their draw stream is
  not read) and the town's magic stolen by a creature.
- **The body posed for the turn** ([The body posed for the turn](#the-body-posed-for-the-turn)):
  - The gates [0xD00DE4] and [0xD00DE8]: no code or data reference writes them (a scan of every 4-byte constant finds
    only the reads in `ProcessState`; the hit in `CreditsRoom::MakeCreatureText` 0x78C1AE is a misaligned byte match).
    Read as set, since the creature plainly animates; a run-time read settles it.
  - [0xBE007C] (the extremity scaling on, 1 in the file): only the read at 0x4EC0C1 was found; an indirect writer (an
    option) was not searched for.
  - When `Creature::UpdateSpellInfo` (vt) runs against `ProcessState`: its caller was not traced, so whether
    `FillAverageHandPos` reads this turn's pose or the last is open.
  - Several creatures: the original poses each inside its own `ProcessState` (creature A is posed before B's turn
    reads it); openblack runs system by system. Only matters with two creatures (fights).
  - Whether the body's position moves inside `UpdateTime` (`StateAction`) or in `ProcessSubAction`, which decides the
    place the pose is made at; and `fn_004EBE10`'s operand order (rotation then scale, or the reverse).
  - physical +0x48 (the time scale of the ms passed): probably openblack's `playbackScale`, not read.
  - +0x4A90 and +0x5730: which bodies play at the real ms (falling, citadel?) and what doubles the step.
  - The layers: which of openblack's face, gesture and wobble are the original's +0x5738, +0x49AC and the sway layers
    0xC2..0xC5; the sway springs +0x48A0 / +0x4874 are not ported.
  - The purposes of `fn_0048B650`, `fn_0048F200` and `fn_0048BAB0`, and of the +0x526C flag `fn_004806C0` sets after
    the freeze; the caller of the function at 0x4912F0; when `fn_00490460` (from `fn_00478310`) runs in the turn; and
    which buffer the footprints read.
  - The fight's look-ahead (0x48A791..0x48A8C2, in `StateAction`) writes +0x5180 without checking +0x5270, so a held
    creature in the fight state would overwrite its frozen copy; whether that can happen was not checked.
  - The drawn bones' (+0x47F0) readers are not all known. The creature-side ones found: `fn_0048F550` (from `DrawNow`
    0x48E299, the carried object that is not a creature, placed every frame by `fn_0048F280` on +0x47F0),
    `fn_0068E430` (from `DrawNow` 0x48E277), `DrawFightSparkles`, `Creature::Draw` 0x517DC8, `fn_00483290` 0x4834C6,
    `fn_005CFDE0`, `fn_005D3DD0` and `HandStateCreature::Update` (0x5B19C7..0x5B2A41); which matrices each reads was
    not checked, and no search was made outside the creature's code. `fn_0048F280` also runs once a turn
    (`fn_0048C6C0`, on +0x5178), so the carried object is placed twice, from two poses; which one it is drawn at was
    not checked.
  - What `fn_0048F180` does (the mesh points +0x5408 while +0x5430 is set, through `fn_00867400`): its one caller is
    `FallingSpell::Close` 0x526580; which pose it reads and what the closing spell does with the points were not
    followed.
  - Not ported yet: the freeze and reconnect calls, the re-poses inside `StateAction`, the two per-turn caches, and
    every reader.
- **The evil and good sparkles (not ported: the particle kinds they use are missing).** Every frame the creature's
  3D object calls `LH3DCreature::AddEvilGoodSparkles` 0x481410 from `UpdateTime` (0x481FE5, after `UpdateMorphing`,
  before `UpdateBlood`). It does nothing while [0xC64204] (the falling creature's 3D copy, see
  [miracles.md](miracles.md)) is set to another object. The alignment it reads is the 3D object's +0x9C, the size its
  +0x90. Its own draws are all `LocalRand` (no synchronised draw), but each sparkle and each puff it adds draws CRT
  `rand()` in the particle list's add (below): once ported, a creature with |alignment| > 0.2 makes three more CRT
  draws a frame, which changes the state hash's `crt` part and shifts the CRT sequence of its other users (the physics
  dust seeds, rain, chimney smoke), and its particles take slots of the 1024 shared with the dust.
  - **Good, alignment > 0.2** (`fcomp` 0x481430, `test ah, 0x41`): three times (the counter compared as a float with
    3 [0x8C2C50]) `fn_00845C20(pos, (0, 0, 0), colour, 5 · size, kind)`. The draws, in order: x, y, z offsets
    (LocalRand(101) − 50) · 0.02, a bone LocalRand(+0x47B8 bones), the colour LocalRand(6), the kind LocalRand(2).
    pos = bone translation (+0x5178 records of 48 bytes, +0x24/+0x28/+0x2C) + offset · 0.75 · size. Colour
    0xFF9090, 0x90FF90, 0x9090FF, 0xFFFF90, 0x90FFFF, 0xFF90FF (table 0x48172C) with alpha `ftol(a · 32) << 24`;
    kind 1 when the last draw is non-zero, else 2 (`neg; sbb; add 2`).
  - **Evil, alignment < −0.2** (0x4815B6): three times `fn_00845C20(pos, (0, 2, 0), colour, 2 · size, 3)`, the same
    four position draws then the colour LocalRand(6): 0x300000, 0x3000, 0x30, 0x303000, 0x3030, 0x300030 (table
    0x481744), alpha `ftol(−a · 72) << 24`. No kind draw.
  - `fn_00845C20` is the add of the shared particle list [0xEF753C] (`fn_00845D30`: nothing while [0xC38214] is 0,
    at most 0x400 at 0x845D42; `fn_00845FA0`: a kind other than 0 takes CRT `rand() % 16` at 0x845FDE as its
    seed). openblack's only port of that list is the physics dust (`ECS/Physics/Dust`), which has kind 4 alone. The
    update `fn_00846010` gives kinds 1, 2, 3 a life of 2 s (kind 4: 1 s) and moves them at their velocity with no
    gravity, as kind 4. The draw (0x846280): kind 3 takes kind 4's
    path (0x8464A4: cell 16 + ((seed − ftol(−2 · age)) & 15), material [0xEF751C], half size
    size · (L − age) / L · min(1, age / (0.125 · L)), L = 2 for kind 3), so the evil puffs need only a 2 s life in
    the dust. Kinds 1 and 2 have their own: material [0xEF7524] (render mode 0xD on `data\blobs.raw`, 0x845E10),
    cell c = (seed − ftol(−8 · age)) & 15 for kind 1 and 15 − c for kind 2 (rows 0-1 of the sheet), half size
    size · (2 − age) · 0.5 · (age < 0.25 ? 4 · age : 1) · (sin(2π · age) + 2) · 0.33 (0x846443..0x84648E).
    Porting them means adding those kinds to the dust (its life, cell rule, size curve and the additive material to
    the renderer's dust pass), then the sparkles on the creature's posed bones, with a debug switch to show them.
    On Land 1 the creature's physique alignment is 0.119, inside ±0.2, so no particle is added and the checks would
    stay identical there; any other creature outside ±0.2 changes the `crt` hash part.
- **Spell reversion**: whether the creature's save writes the flag (Creature +0x10BC) is not read. Without reversion, a
  spell waiting for its kind stays in the queue until some other spell wears off (as the original's step reads); whether
  a later path releases it is not traced.
- **A creature's body in the physics**: the original's is its bounding sphere with one point for each bone and its
  mass is read from the body; the port's is a ball as tall as the creature, mass 1000. Who catches what is thrown (the
  catch hook) and whether a creature's put-down really goes through the hand's start of physics are not read.
- **What a creature throws**: the thrower's player for the flying-object reaction (the hand's start gives the hand's).
- **What counts as asleep, still open** ([Asleep, sleeping and sitting](#asleep-sleeping-and-sitting)):
  - The action the original's creature carries out while it lies out after a fight's knock-out is not traced;
    openblack still gives that rest (`Rest::Resting`) the slower energy and the sleep's heal, rest and wake, as before.
  - While its mind is paused, led by the leash, fighting or knocked out, openblack's mind keeps the action it last
    carried out: a plan of row 17 still active keeps the slower energy and the ×3 growth. What mind +0xF60 holds then
    in the original is not traced.
  - Actions 59 (rest to get better) and 78 (sleep by something) have no agenda in openblack yet, so only a plan of row
    17 makes the flags apply. The idle mind's own sleep counts as row 313 and gets neither, as that row would in the
    original; which row the original's creature carries out when it goes to sleep by itself (for example
    `SleepAtHome` in its pen) is not traced.
  - Our sitting is the body's sit loop (animation 38 looping), not the play-animation sub-action's own animation
    number (mind +0xFA8, slot [+0xFB4] · 0x60 + 0x38, 0x500AD7). That the two start and end on the same turns is not
    verified: openblack has no play-animation sub-action to read the number from.
  - Our sit rest runs in the mind's turn after it has thought and before our planner (`PlanTurn`); the original's
    plan checks and forced plans (0x4733C0..0x4736F9) come before its sub-actions (0x4737B0), so a plan begun in the
    turn may start or stop the sitting a turn apart from ours.
  - The sleep step's heal, rest and wake still run in the body's turn, from the rest state the mind left the turn
    before, and once a body turn under the debug window's time scale (one a game turn in play); the original's sleep
    sub-action 0x5086D0 runs after the body in the same turn, once a turn.
- **Thirst, fainting and drinking, still open** ([Thirst, fainting and drinking](#thirst-fainting-and-drinking)):
  - The computer player's type: openblack sets `PlayerMagic::playerType` to 1 for the human player and 0 for every
    other; nothing sets 2 yet (the original's `ToggleComputerPlayer` and `SetupPlayers` are not read for it), so the
    computer-player gate never closes today.
  - fn_0047CE80, called under the same gates just before the faint test (a mind counter +0x111C against 15 × the
    turns a second, then mind +0x1C38 and +0x1C14 = 0 and `SuppressDesire`(0x21, 60 s)), is not decoded or ported.
  - The help message fn_004C9FE0(0x1E, reason) each faint calls is not ported.
  - Where in the turn: the original tests for a faint after the mind's plan step of the same turn; openblack's body
    decides it in its own turn and the mind acts on it in the next mind turn.
  - The thirst: a negative info+0x224 would wrap n to a large unsigned number; openblack keeps that wrap, but then
    divides by n rounded to a float first, where the original divides by the exact integer. A NaN or out-of-range
    product follows the SSE2 path of `__ftol` only; on a CPU without SSE2 the original keeps the low half of the
    64-bit result instead. No row of `info.dat` reaches either case (all are 5000).
- **Every constant in [Stand-ins](#stand-ins-and-unverified-constants)**, and the four unread blocks of `GCreatureInfo`.
- **Double click on a creature (the lock-on) and the camera help.** In `CameraModeNew3::Update` 0x45A960 the lock-on is
  part of the double-click flight: the branch needs camera feature 0x10 (0x45DDA3) and clears the interface's
  double-click flag (0x45DDE9). With a thing under the hand (`GInterface+0x3C8`) it first reports
  `CameraHelpCallback(0x306 DoubleClickObject)` (0x45DE3A, help event 31), then asks `IsCreature` (vt+0x34, 0x45DE4B),
  and after the flight's set-up builds `CameraModeFollow(camera, thing, 1.0, 0, 0)` (ctor 0x44B800, at 0x45E34B); a
  fight arena in range starts `StartFight` 0x45A4D0 instead (0x45E07E clears the follow). openblack's Creature Mode
  locks on only with feature 0x10, as the original, but the world camera still reports 0x305 DoubleClickPos (event 30)
  for every double click, on a thing or not: reporting 0x306 needs the thing under the hand (C29), and the fight arena
  is not ported.
- **The C key** (`ZOOM_TO_CREATURE`) does not yet respect the script interaction modes: `IsActionBlocked`
  (`src/Help/InterfaceInteraction.h`) would block it when camera moves or realm zooms are not allowed, but the action
  map does not ask it yet, so Creature Mode still locks on.
- **Drawing the creature (C31), inferred until a creature is drawn:** the hair's ribbon winding against the
  original's one-sided material (see [Drawing in the frame](#drawing-in-the-frame)); whether the
  creature's shadow is also drawn over objects; the shadow projected from the posed base mesh rather than the morphed
  one; whether its eyes and hair are drawn in the water's reflection; and, for a creature that morphs with the terrain,
  that the morph program (which has no height-map version) stands in for the height-map one. A hair with bit 2 of +0x24 switches FILLMODE to wireframe for 10 of every 20 strand draws (counter [0xEF7530],
  0x84757E..0x8475C2, put back at 0x8476C1..): which hair sets that bit is not known (a debug view, **(inferred)**).
- **The Creature Cave without a creature.** openblack's creature room shows its four scrolls blank and opens no cave
  screen while the player has no creature (as raffclar's port); what the original's cave shows in that state has not
  been checked.
