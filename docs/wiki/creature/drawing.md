# Drawing the creature

## Summary

A creature is drawn once a frame in the models stage: its body (the morphed, boned mesh), the shadows that fall on
other objects over it, its hair and its eyes, in that order, at once and never through the transparent queue. The
body is posed once a game turn and drawn between its last two turn poses, a turn behind the game. Around it the
alignment sparkles (good motes, evil smoke) are added once a turn, and a single "fizz" per creature fades it out of
sight and back in through static: when it is carried home after a faint, under the invisible spell, and when a script
loads, fades in or deletes it, or a vortex takes it.

openblack draws the body, shadows, hair and eyes in the original's order, blends the drawn bones between the turn
poses as the original does, starts the hair from the never-used-heap look (user, 2026-10-15), ports the sparkles and
the fizz with both of its static passes, and pins the numbers to the oracle. The Creed glows are not ported. What is
still open is in [Pending](#pending).

## The original

#### Drawing in the frame

- **Where.** A creature is drawn in the models stage, stage 4m of
  [Draw stages in order](../rendering/frame.md#draw-stages-in-order): `Creature::Draw` 0x517910 (vt+0x610) points the
  game object's 3D object (+0x40) at the body ([+0x160]+0x58 → +0x482C, 0x517AD1..0x517AE5), then calls
  `LH3DCreature::AddForDrawing` 0x48E1C0 (vt+0x1C, 0x517AED). That one lights the body (`fn_00801C90`, haze
  `fn_007FEB30`), runs `CheckRegionOnScreen` and calls `DrawNow` 0x48E260 at once. Other callers of `DrawNow`:
  `FallingSpell::Draw` 0x526A42, `fn_00640054` 0x640386 (the creature snapshot) and `CreatureRoom::DrawAdditional`
  0x7888B2 (the Cave).
- **The body** is an animated 3D object: `Morphable::MorphInit` 0x61731A makes it with `LH3DObject::Create(3)`, which
  is vtable 0x9A3068 (0x80B5DE; the hand's body is made the same way). `DrawNow` calls its `AddDrawing` (vt+0x100 =
  `fn_00813340`, 0x48E2A6): off screen or with +0xB8 set nothing; queued whole (`NewZObject` 0x813413, callback
  0x7FA980, key (dx² + dy²) + dz² from +0x38..+0x40) if its Z-sort bit (vt+0x44, from the mesh's 0x200) **or** its
  global-alpha bit (vt+0x4C, +4 & 0x80) is set; otherwise its Draw at once (vt+0x108 = `fn_00813BA0`, 0x8133C4). No
  creature base mesh in `Data/CreatureMesh` has the 0x200 flag (only the Lion's six variants and the three hand meshes
  have it; the body gets the base), and the global-alpha bit is never set on a body [V]: its only setter is vt+0x48 `fn_007F9D60`
  (`or edx, 0x80` at 0x7F9D67; getter vt+0x4C `fn_007F9D80`), which has no direct caller, and none of the
  `call [reg+0x48]` sites that can pass true is in creature code or on the body (+0x482C). So a creature is drawn at
  once, in the draw list's order, never through the Z-sorter.
- **Inside the body's Draw** (`fn_00813BA0`): the mesh, then the shadows cast on objects (the list [0xFAA7E0],
  `fn_0080B050`, 0x813F8E..0x813FE0; a shadow is not drawn on its own caster, +0x464), then the **hair**
  (`LH3DObjectHair` at +0x88, `fn_00848390` 0x814029) only while +0x94 and +0xA0 (the body's cross-fade state) are
  both below 0.2. The hair strands go through `Draw3DWorldTriangle` 0x81C090 with the hair group's material (+0x78), or
  `fn_0081BC10` untextured (`fn_00847060` 0x847696 / 0x8476BC): drawn at once, no Z object. A hair with bit 2 of
  +0x24 switches FILLMODE to wireframe for 10 of every 20 strand draws (counter [0xEF7530], 0x84757E..0x8475C2, put
  back at 0x8476C1..): which hair sets that bit is not known (a debug view, **🔍 inferred**).
- **The eyes** come after the body's `AddDrawing` returns: the shared eyeball and eyelid objects ([0xC64224] /
  [0xC64228], `LH3DObject::Create(0)`, `Data\eyeball.l3d` / `Data\eyelid.l3d`, 0x48E2DA..0x48E364), two eyes each,
  drawn at once with vt+0x108 (`fn_0080DB30`), or the cross-fade draws vt+0x15C / vt+0x154 when the body's +0x94 /
  +0xA0 is set (0x48E9A2..0x48E9DD, 0x48EFD3..0x48F00E). So the order is body, shadows over it, hair, eyes; a queued
  body (not seen) would have its eyes drawn before it.
- **The eyelids' colour** (oracle: 336 runs of 0x48EECE..0x48EF98, all matching). The skin under a lid is read by
  `ObtainPointColour` 0x866BA0: the ARGB4444 texel at trunc(u · w), trunc(v · h), an index outside the skin taken as
  its last (unsigned compare), as `0xFF000000 | R << 20 | G << 12 | B << 4` (each byte nibble × 16, at most 0xF0).
  Once per `DrawNow` (0x48ED7E..0x48ED94, the flag cleared at 0x48E37D and tested at 0x48EC50) the eyelid object takes
  that skin with specular 0; before each lid's draw (0x48EECE..0x48EF98) its colour becomes trunc(lid · B / 255) per
  channel (the 0x80808081 divide), alpha 0xFF, and its specular the body's Bs, where B and Bs are the body's colour and
  specular, the land's light at the body's origin hazed (`AddForDrawing` 0x48E1C0: `fn_00801C90`, `fn_007FEB30`). The
  product stays on the eyelid object, so the second lid drawn takes B twice (the first eye without a lid: the second
  once). The static draw then lights the set colour with the model light only.
- **No shadow falls on the body.** The receiver flag (+4 & 0x40) is set only through vt+0x78 with 1; no creature code
  calls it (none of the 33 vt+0x78 call sites is on the body), and a bare 3D object starts with +4 = 0x10009
  (0x816537). The game object's own 3D object (`Object::Create3DObject` 0x6365F0) is the one that asks, and it is
  replaced by the body before the draw.
- **The creature's own shadow** is a projected shadow (`CreateDynamicShadow`,
  [Projected shadows](../rendering/shadows-and-reflections.md#projected-shadows-shadowinfo)), drawn with the land
  blocks (stage 4i) and over the objects that receive.
- **The hair's material**: one material for every creature, made the first time a creature loads (`fn_006186B0`,
  called from `LH3DCreature::LoadBase` 0x4EB11C and `Morphable::ReadBinary` 0x617DA4): `data\c_ape_hair.raw` (format
  0x41), `CreateMaterial(9, texture)` 0x618701 into [0xD3F008], then +4 (ALPHAREF) = 0x32 (0x618713); +5 stays 0
  (`CreateMaterial` 0x82FD56), so it is one-sided (CCW) and clamped. `PrepareForDrawing` passes it to every hair group
  (0x4ED539 / 0x4ED5AC → `fn_00848350` → `fn_00847C90`, group +0x78). A group without its texture (+0x60 ≠ 1) goes
  through `fn_0081BC10` with no material, so the triangle draw's default one ([0xEA9EC4], `CreateMaterial(0, 0)` at
  0x819011): mode 0, opaque. Both write depth, so the hair is never covered by what is drawn after it and behind it,
  whatever the draw-list order.

##### Where a strand starts

Vanilla bug 132. The strand points are never read from the file and never set before the first step reads them:

- `HairGroup::ReadBinary` 0x848180 (callers `Morphable::ReadBinary` 0x617C23 / 0x617D6B, at a creature's load) reads
  the group, +0x5C = points per strand M (the editor's setter 0x847D90 keeps it in 2..11), then calls
  `LH3DHair::ReadBinary` 0x847890 for each strand (0x84830E). That reads the strand's flags +0x24, its
  `MeshIntersect` +0x38 and three points +0x00..+0x20, then `operator new(36 · M)` 0x84791D into +0x34, and writes
  nothing into the block. The block is three arrays of M points: positions at 0, the step's copy of the positions at
  12M, speeds at 24M. The editor's point-count change 0x847D20 also frees and reallocates without clearing
  (0x847D4C); only the editor's add path 0x846750 clears its block (0x846797..0x8467AA).
- The step `fn_008467C0` (only caller 0x847CC5 in `fn_00847C90`, once a frame per strand) writes position 0 from the
  mesh every frame (0x846897 / 0x846AE1), copies positions 1.. into the copy array (0x846BAD), then for each point
  after the root reads its position and speed: speed += (spring − gravity) · dt, capped, times
  `pow((damping · length)², dt)` (`__CIpow` 0x846BD9, applied 0x846D56..0x846D71); position += speed · dt
  (0x846E32..0x846E94); each point put back one segment from the one before (0x846E9F..0x846F30); speed = moved / dt
  (0x846F36..). For a damping of 0 the factor is 0, so the speed is 0 every frame and the start positions are only
  dragged after the root: the strand keeps the direction its first leftover position gave it.
- The allocator. `operator new` 0x7C64EE → `_nh_malloc` 0x7C665E → `_heap_alloc` 0x7C668A. The CRT's heap kind
  [0xFACCAC] is written only by `_heap_init` 0x7CE4FA (`_set_sbh_threshold`, the other writer, has no caller):
  `___heap_select` 0x7CE38A returns 1, the system heap, on Windows NT 5.0 and later (`GetVersionExA`, platform 2 and
  major ≥ 5, 0x7CE3B3..0x7CE3C8); the CRT's small-block heaps (kinds 2 and 3) serve only Windows 9x and NT 4. So
  the block comes from `HeapAlloc([0xFACCA8], 0, (size + 15) & ~15)` (0x7C6759..0x7C6771): no
  `HEAP_ZERO_MEMORY`. The heap is the CRT's own, `HeapCreate(0, 0x1000, 0)` from `_WinMainCRTStartup` 0x7CA4F8
  (`_heap_init(1)`, so no `HEAP_NO_SERIALIZE`): a growable Windows heap, which Windows serves through its
  low-fragmentation front end once a block size is in heavy use. Oracle: `operator new(216)` (M = 6) with the system
  heap selected calls `HeapAlloc(heap, 0, 224)`; `_heap_init(1)` calls `HeapCreate(0, 0x1000, 0)`.
- What the block holds is therefore not the game's: a block carved from memory the heap never used is zero (fresh
  pages from the system); a reused one holds what its previous owner left (anything the game allocated and freed
  earlier at that size: land, scripts, file buffers, a replaced creature's own hair), with the heap's free-list links
  over its first bytes; with the heap's checks on (global flags, some debuggers) it is filled with 0xBAADF00D. Which
  block is handed out depends on every allocation before it, many of them set by the machine (display modes, sound
  devices, the files in the save and profile folders), and since Windows 8 the low-fragmentation front end picks a
  free slot at random. The start of a strand therefore differs from run to run and machine to machine, and cannot be
  reproduced exactly. A strand with damping above 0 forgets it within its first seconds; a strand with damping 0 (the
  neutral tiger's whiskers) keeps it.
- The step works in world space: position 0 is the mesh point's world position (`GetWorldPositionAndInwardNormal`
  0x867400 at 0x846AE1, or 0x867740 at 0x846897, both into +0x34), sunk along the inward normal. So a zero block is
  every point at the world's origin, and a damping-0 strand, dragged after its root, points from its root towards the
  origin.

##### The evil and good sparkles

Read in the v1.42 exe and pinned with the oracle. Once a creature turn the creature's 3D object calls
`LH3DCreature::AddEvilGoodSparkles` 0x481410 from `UpdateTime` (0x481FE5, after `UpdateMorphing`, before
`UpdateBlood` and the blink's `LocalRand` at 0x482028; `UpdateTime` itself is called once a turn from
`Creature::ProcessState` 0x473939, see
[The body posed for the turn](pose-and-animation.md#the-body-posed-for-the-turn)). It does nothing while
`_SparklingCreature` [0xC64204] is set to another object: `FallingSpell::Init` 0x526124 sets it to the falling film's
copy and `FallingSpell::Draw` 0x5268FF / 0x526912 to the copy (or its second body at +0x57B8), `FallingSpell::Close`
0x5264BD, `LH3DCreature::Init` 0x47FCF2 and `ResetCameraModeNew3` 0x460BFE clear it (xref: no other writer, one
reader). The alignment it reads is the 3D object's +0x9C (the evil-good target, the alignment held to ±1), the size its
+0x90; the bones are +0x5178 (the pose of the turn before, not the hand's kept copy), +0x47B8 of them. Its own draws
are all `LocalRand` (no synchronised draw), but each mote and each puff it adds draws CRT `rand()` in the particle
list's add (below), and its particles take slots of the 1024 shared with the dust. The streams are described in
[Random numbers](../engine/random.md#random-numbers).

- **Good, alignment > 0.2** (`fcomp` 0x481430, `test ah, 0x41`): three times (the counter compared as a float with
  3 [0x8C2C50]) `AddLiquidParticle` 0x845C20 `(pos, (0, 0, 0), colour, 5 · size, kind)`. The draws, in order: x, y,
  z offsets (LocalRand(101) − 50) · 0.02, a bone LocalRand(+0x47B8), the colour LocalRand(6), the kind LocalRand(2).
  pos = bone translation (records of 48 bytes, +0x24/+0x28/+0x2C) + (0.75 · size) · offset, each axis rounded to a
  float (0.75 at 0x8CF138: bw1-decomp writes it `GetStandingHeight() · 0.05`). Colour 0xFF9090, 0x90FF90, 0x9090FF,
  0xFFFF90, 0x90FFFF, 0xFF90FF (table 0x48172C) plus alpha `ftol(a · 32) << 24`; kind 1 when the last draw is
  non-zero, else 2 (`neg; sbb; add 2`). Exactly 0.2 makes none and draws nothing.
- **Evil, alignment < −0.2** (0x4815B6): three times `AddLiquidParticle(pos, (0, 2, 0), colour, 2 · size, 3)`
  (0x481702; bw1-decomp 1.20's C drops this call), the same four position draws then the colour LocalRand(6):
  0x300000, 0x3000, 0x30, 0x303000, 0x3030, 0x300030 (table 0x481744), alpha `ftol(−a · 72) << 24` taken once before
  the loop. No kind draw.
- `AddLiquidParticle` is the add of the shared particle list [0xEF753C] (`fn_00845D30`: nothing while [0xC38214] is
  0, at most 0x400 at 0x845D42; `fn_00845FA0`: a kind other than 0 takes CRT `rand() % 16` at 0x845FDE as its seed).
  Particle: +0 position, +0xC velocity, +0x18 kind, +0x1C seed, +0x20 colour, +0x24 age, +0x28 size. The update
  `fn_00846010` adds the time to the age, removes a particle whose age is past its life (2 s for kinds 1-3
  [0x8AB478], 1 s for kind 4 [0x8AA390], 3 s for kind 0; at its life it stays) by moving the list's last into its
  place, and moves the others at their velocity with no gravity (kinds 1-4 alike, 0x84623B). The draw (0x846280):
  kinds 3 and 4 (0x8464A4) cell 16 + ((seed − ftol(−2 · age)) & 15), material [0xEF751C] (mode 6), half size
  ((L − age) / L · (age < L/8 ? age / (L/8) : 1)) · size in that order, L = 2 for kind 3 and 1 for kind 4. Kinds 1
  and 2: material [0xEF7524] (render mode 0xD, additive, on `datalobs.raw`, whose RGB is white, 0x845E10), cell c =
  (seed − ftol(−8 · age)) & 15 for kind 1 and 15 − c for kind 2 (rows 0-1 of the sheet), half size
  ((sin(2π · age) + 2) · 0.33 · size) · ((2 − age) · 0.5 · (age < 0.25 ? 4 · age : 1)) (0x846443..0x84648E; the
  `fsin` result kept to the x87's precision until the + 2).
- What it moves: a creature with |alignment| > 0.2 draws 18 (good) or 15 (evil) more `LocalRand` and 3 more CRT
  `rand()` a turn, which change the state hash's `random` and `crt` parts and shift the CRT sequence of its other
  users (the physics dust seeds, rain, chimney smoke). On Land 1 the creature's physique alignment is 0.119, inside
  ±0.2, so nothing is drawn there.

##### The Creed glows

Sound actions 95..109 (`SoundAction::CreedGlow*`) light the creature's **hand glows** (`HandGlows`): a visual effect,
not a sound.

- **The key → glow.** `fn_0068E6E0`, called from `PlayASoundEffect` 0x4834A9 with the action: action − 95 indexes two
  tables, the type by the byte table 0x68E7C4 → 0x68E7B8 {0, 2, 1}, the power by 0x68E7D4. Actions 95..99 (Left) are
  type 0, 100..104 type 2, 105..109 (Centre) type 1; within each five the power is 0, 0.25, 0.5, 0.75, 1. The body's
  `HandGlows` (`LH3DCreature` +0x57B0) is made on first use (`fn_0068E230`: 4 null slots). Power 0 (`fcomp 0; test
  ah, 0x40`) → `fn_0068E2D0(type)`: the slot's HandGlow deleted. Else `fn_0068E300(type)` (make the slot's HandGlow
  when absent) and `SetScalePowerTime(type, 1.0f, 2 × power, 1.0f)`.
- **`HandGlows`.** `SetScalePowerTime(type, scale, power, time)` 0x68E340: power 0 or NaN deletes the slot; else it
  makes it if absent, HandGlow +0 = power (`fn_0068E1F0`), +4 = scale (`SetScale` 0x68E200); the time is unused.
  `IsAnyActive` 0x68E3F0; `fn_0068E3A0(type, &a, &b)` gives the slot's +4 and +0 (0, 0 when none), for the save.
  `Release` 0x68E260, from `~LH3DCreature`, deletes the 4 slots.
- **The draw.** `Draw(LH3DCreature*)` 0x68E430, from `LH3DCreature::DrawNow` 0x48E277, per non-null slot: slot 0 at
  the grasp point (`LH3DAnim::GetGraspPoint` of the mesh of the body's current LOD +0x98/+0xB4) of the **mirror** of
  bone +0x51C8 (`GetMirrorBone`); slot 2 of bone +0x51C8 itself; slot 1 at bone +0x51C0's matrix; slot 3 at bone
  +0x51B8's matrix replaced by a spin about y, angle HandGlow +8 += g_game_time_inc × 1 × 0.001, at the body's
  position (+0x482C +0x38) raised by 1.3 × drawn size (+0x90) × 15. The bone matrix is copied (48 bytes) and
  `NormaliseMatrixOnly`; slots 0 and 2 take the grasp point as position; every term × the body's scale (+0x482C +0x44
  × 1.0), then the third row, the z axis (M+0x18..M+0x20,
  +0x48, +0x4C, +0x50 of the local frame), × −1 ([0x8AB678], 0x68E68D..0x68E6AF in `HandGlows::Draw` 0x68E430) [V], then
  `HandGlow::DrawAt(matrix, scale)` 0x68E120.
- **`HandGlow`** (0x14 bytes, constructor 0x68E070): +0 = 1.0, +4 = 1.0, +8 = 0 (the spin angle); +0xC = an
  LH3DObject with mesh 0x20D of the mesh pack (0 when the pack has fewer); +0x10 = `fn_0083EFE0(object,
  fn_0057D540([0xC028B0] = 3))`, a scrolling effect object (the same class as the citadel heart's destruction,
  `fn_00465C70`, and `fn_0069CE90` / `sub_69D220`). `DrawAt(matrix, s)`: the object's matrix is the matrix with each
  of its 9 axis terms × +4; object +0x44 = s × +4; effect +8 = 2 × +0, +0xC = 2.0, +0x10 = 0.2f, +4 =
  `fn_0057D540(3)`; then `fn_0083F270(g_delta_time × 0.001)` and `fn_0083F100`, the transparent queue
  ([The single transparent queue](../rendering/transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter)).
- **Who else drives it.** `Creature::SetCreedPower(type, a, b, c)` 0x4F8A40 → `SetScalePowerTime`; when `IsAnyActive`
  changes, a looping `SoundTag::Create(this, 0xA6, …)` is made (on) or removed (off). Callers:
  `GScript::SetCreedProperties` 0x6F5BC4 (the native 338 SET_CREATURE_CREED_PROPERTIES), `Creature::Load` 0x4E694C;
  the save `fn_004F8AE0` (0x4E58F2) reads them back through `fn_0068E3A0`. The falling spell's copy too
  ([The falling spell](../magic/falling-spell.md#the-falling-spell-what-fallingspell-draws)).

#### Drawing between turns

- `Creature::Draw` 0x517910 sets no matrix and does not call the Living's per-frame lerp (`Living::PreDrawScale`
  0x51AF00, see [Drawing between turns](../beings/animation.md#drawing-between-turns)) [V]: the creature has no
  start-of-turn copy (+0x2C) or position lerp of its own; the drawn bones below place it.
- **The drawn bones** 📼 1.20 only (`LH3DCreature::PrepareForDrawing` 0x4ED32E..0x4ED376): f·+0x5178 + (1 − f)·+0x5174,
  f = min(the game's turn fraction g_game +0x205D64, 2.0), the fraction being `resto` × 0.01, 0..0.99 (0x54D392),
  element by element (0x4EECD0 at 0x4ED450, 0x4ED46B): no clamp beyond the fraction's, no yaw rate and no separate
  lerp. The blend itself is in [The drawn body](pose-and-animation.md#the-drawn-body).
- **The hair and the footprints take the drawn size.** The hair's scale is `GetExtraHeadScale` × +0x90 × 15
  (`PrepareForDrawing` 0x4ED548..0x4ED55E; `GetExtraHeadScale` 0x4EC450 = 1.8 − 0.5 × clamp(+0x90, 0, 2)); the
  footprint's side is 4 × +0x90 (`fn_00483290` 0x483582).

### Fizzing out of sight and back in

A creature fades out of sight and back in through static. All the cases set the one fizz each creature has.

- **The fields.** `Creature` +0x12AC is the "goes for good" lock (a byte), +0x12B0 the fizz now (0 in full sight, 1
  gone), +0x12B4 the target and +0x12B8 the rate per second. Save and Load write all four (0x4E5252..0x4E5328,
  0x4E6491..0x4E653A); fn_00474130 zeroes them (0x4741BC..0x4741D4); fn_0047ACC0 reads the fizz now.
- **`Creature::SetFizz(target, seconds, goesForGood)`** 0x47AB90:
  - once the lock is set it does nothing at all, the sound included (0x47AB93..0x47AB9B);
  - a target of exactly 1.0, tested before it is clamped, makes the sound `SoundTag::Create` at the creature's
    MapCoords (+0x14), sample 0x27 (39, `G_SpellTeleportEnergiseGo`), not tracked, mode 2, no loops, 3D, the in-game
    bank, no delay (0x47ABA1..0x47ABC6);
  - then the lock |= goesForGood; the target is clamped to 0..1; with seconds ≠ 0 and the target ≠ now the rate is
    ±1/seconds towards the target, otherwise the rate is 0 and now = target at once (0x47ABCE..0x47ACA1).
- **The step,** in `Creature::ProcessState` 0x472DC0, after `ProcessSubAction` (0x4737B0) and
  `Update3DCreatureFromAttributes` (0x4737D2), before `UpdateTime` (0x473939); gated by [0xD00DE4] and mind +0x20CE8
  (0x4737DB..0x4737F3), both taken as set:
  1. the fizz now is copied to the body's 3D object (+0xA0 of [+0x482C]), and when it is not 0 the object's material
     +0x9C becomes `GlobalTextures::GetFizzMaterial()` (0x4737F9..0x473823);
  2. only when the rate ≠ 0: now = ms per turn [0xD01A38] × rate × 0.001 + now, stored as a float; it stops at the
     target only once **past** it (rate > 0 and now > target, or rate < 0 and now < target), then now = target and
     the rate 0 (0x473829..0x4738A9);
  3. still inside that branch: now == 1.0 with the lock set calls `ToBeDeleted(0)` (vt +0xC) and the function returns
     3 (0x47383A..0x4738D3). A creature put at 1.0 at once (seconds 0, or already there) is never deleted by it.
- **Callers.** Carried home (sub-action 112, below); the invisible spell (start 0x4F58B0 and end 0x4F5970
  SetFizz(0, 0, false), each turn 0x4F58D0..0x4F5933 SetFizz(0.75 × clamp(ratio, 0, 1), 0, false)), so the spell and
  the carry home share one fizz; sub-action 93 TransportedByTeleport (fn_00507265; stages 0x507210 / 0x507250 / 0x507350):
  it fades out over N = trunc(trunc(1000 / turn ms) × 2) turns ([0x8D1664] = 2.0) with SetFizz(1 − c/N, 0, false)
  (0x5072BD), then makes sound 38 at the destination and moves there, then fades back in with SetFizz(c/N, 0, false)
  (0x5073B7) and SetFizz(0, 0, false) (0x5073C8); its abort is 0x508B20 too [V]; `LOAD_MY_CREATURE` (0x552B86,
  0x552B96); the vortex (0x5FDA06); `OBJECT_DELETE` (0x6F93A3); `SET_OBJECT_FADE_IN` (0x6FEFCB, 0x6FEFD7).
- **The eyes' static** moves once a frame from `LH3DRender::StartFrame` (0x82F1FE → fn_00813770): [0xEA1B1C] and
  [0xEA1B20] += 0.1 and 0.2 × the frame's game ms × 0.001, fmod 1, whether anything fizzes or not.

#### Carried home: sub-action 112

- **The sub-action.** Its name, "TranportHome" (the game's own spelling, 0xBE5E40), is written to entry 112 of the
  sub-action table (0xBE4A98, (0xBE4A98 − 0xBE0B98) / 0x90; 0x4FD910..0x4FD921), with Initiate 0x507D70 at +0x50,
  Perform 0x507DC0 at +0x60, Finish 0x507E80 at +0x70 and 0x508B20 at +0x80 (0x4FD972..0x4FD9DD).
  `CreatureAgenda::ConstructSubActionsForDeath` 0x49B770 adds it after the faint and its wait, only when `GetPlayer`
  (vt +0x1C) is not null (0x49B880..0x49B8B9), then a 3 s wait and sub-action 139.
- **Initiate 0x507D70:** the counter `Creature` +0x58 = ftol(int64(1000 div ms per turn) × 2), 20 turns at 100 ms;
  then `SetFizz(1, 2, false)`, the fade out with the energise sound. It returns 2.
- **Perform 0x507DC0:** `--[+0x58]`; at 0 the creature is moved home (`ForceMoveMapObjectWithoutWalking`, home
  +0x1200, 0x507DDE), the counter is set to 20 again, `SetFizz(0, 2, false)` starts the fade back in
  (0x507DE3..0x507E16), and the faint's caps are put on its body there and then: +0x30 ≤ 0.9, +0x34 ≤ 0.9 and
  +0x1C ≥ 0.1 (0x507E1B..0x507E68). It returns 2, otherwise 0.
- **Finish 0x507E80:** `--[+0x58]`; at 0 the counter is set to 20 and `SetFizz(0, 2, false)` runs once more
  (0x507EC6), by then a no-op: the fade in took the same 20 steps. It returns 2, otherwise 0.
- **0x508B20** at +0x80 is `SetFizz(0, 0, false)`: stopped part way, the creature is back in full sight at once. It is
  the sub-action's abort, reached when what the creature is doing is stopped [V]: a row's +0x80 slot is called only by
  `Creature::StopWhatIAmDoing` (0x475902..0x475914), and `ProcessSubAction` 0x4FEDA0's one indirect call (0x4FF0F9)
  reaches only the stage slots +0x50..+0x70.

#### Drawn through the static

- **The step's copy.** Each turn the fizz now goes to the body's 3D object (+0xA0), and while it is not 0 the
  object's material +0x9C is `GlobalTextures::GetFizzMaterial()` 0x57D500 (its only caller is that step, 0x47381E):
  `.\data\textures\S_Static.raw` with its alpha `S_Statica.raw`, render mode 0x12 (ChromaJustZ), material byte +5 |= 4
  (tiled).
- **The body,** in the creature object's Draw fn_00813BA0: frozen, its vertex colour is tinted first
  (0x813D37..0x813E07); with the fizz strictly above 0 it is drawn through vt +0x158, fn_00813430
  (0x813E09..0x813E26), else as it is; frozen, the ice sheen fn_00814350 after either (0x813F60..0x813F7D). The hair
  (+0x88) is drawn only while freeze < 0.2 and fizz < 0.2 (0x813FE2..0x814029).
- **fn_00813430:** the level is ftol(fizz × 255); at 255 nothing is drawn (0x813436..0x81344D). Then the creature's
  own static slides on: seconds = g_game_time_inc × 0.001 (a float), +0xA4 = fmod(+0xA4 + 0.1 × seconds, 1) and
  +0xA8 = fmod(+0xA8 + 0.2 × seconds, 1) (0x81347E..0x8134EB); the texture offset is on while either is not 0.
  - Pass 1: OverrideMaterial = 1, OverrideRenderMode [0xECA65C] = the level (0x81349F..0x8134A9), CULLMODE CCW from
    the fizz material's bit 0, every mesh with the fizz material (fn_00813740). Mode 18 (0x82E2A0) is ZERO/ONE with
    the alpha test, and takes ALPHAREF from OverrideRenderMode while the override is on (0x82E3C0..0x82E3E2): only Z,
    where the static's alpha reaches the level.
  - Pass 2: offset and override off, ZFUNC EQUAL (0x813606..0x813628), the object's alpha byte +0x4C = 255 − level
    (0x813642..0x81365A), drawn through the global alpha table 0xC387C8 primitive by primitive (fn_0084C3C0), then
    the normal table and ZFUNC LESSEQUAL again (0x8136EF..0x81372B).
  - Pass 2's alpha is the vertices' diffuse alpha, not a factor after the test: the byte goes into the global diffuse
    [0xC37D8C] too (0x81365D), whose alpha byte fn_0084C3C0 takes (0x84C596..0x84C5CA) and puts into each vertex's
    colour (0x84C6A5..0x84C6B6). The global alpha table's modes 10, 11, 15 and 16 set stage 0 ALPHAOP MODULATE
    (TEXTURE, DIFFUSE) (0x82E8ED..0x82E907, 0x82EA7E..0x82EA95, 0x82E511..0x82E528, 0x82E75E..0x82E775), so their
    alpha test sees texture × (255 − level). The byte replaces the object's own alpha (+0x4C is put back at
    0x8136F9): a fading creature is not faded twice.
  - [0xECA660] is 1 from 0x813499 to 0x813705, across both passes (fn_008137D0, the eyes', sets it at 0x81383B and
    clears it at 0x813B62; fn_007FA300 also writes it, 0x7FA372, 0x7FA598, 0x7FA76F). Its only reader is fn_00858BA0
    (0x859290..0x8592B7): set, `g_NoBackfaceCull` [0xEA9EB8] is 1 for every primitive instead of the material's byte
    +5 bit 0, and its triangles are listed without the CPU's back-face test (0x8592C0..0x859356). fn_00858BA0 is
    called only from fn_0080C050 (vtable slot +0x11C), fn_00811C70 and fn_00825530. Neither pass reaches it: their
    primitives go through fn_0084C3C0 (0x8136B1, and fn_00813740 at 0x813759), which sets `g_NoBackfaceCull` from the
    material's own bit itself (0x84C732..0x84C73E), and the direct calls under fn_0084C3C0 and fn_00813740 never reach
    fn_00858BA0 (14 functions; their only indirect calls are the device's, slots +0x50 and +0x94). The eye pieces'
    plain draw, slot +0x104 of the vtables that hold fn_008137D0 (fn_007FA790, fn_00815980, or fn_008156A0, which
    returns at once), and the draws it calls through slot +0x108 (fn_007FA300, fn_0080DB30, fn_00882A40, fn_0080E550)
    reach it by no direct call either (63 functions), and none of them calls slot +0x11C. So the flag changes nothing
    that the fizz passes draw: the culling is the GPU's CULLMODE alone (CCW in pass 1, the material's in pass 2). What
    the flag is for, on a path that draws through fn_00858BA0 while it is set, is not known.
- **The eyes,** in `LH3DCreature::DrawNow` 0x48E260, each a shared static object: frozen (+0x94 ≠ 0), the frozen
  draw vt +0x15C; else with the fizz not 0, vt +0x154 = fn_008137D0 with the fizz and the fizz material, through the
  one static [0xEA1B1C]/[0xEA1B20] that fn_00813770 slides on once a frame from `LH3DRender::StartFrame` (0x82F1FE);
  else the plain draw. fn_008137D0 returns at once at level 255 (0x8137E5..0x8137EB).

## openblack

Status: the frame's draw, the drawn bones between turns, the hair's start, the sparkles and the fizz are ported; the
Creed glows are not (the native 338 SET_CREATURE_CREED_PROPERTIES is not implemented, and the hand-glow draw,
`fn_0083F270`, is not ported). Files: `Graphics/RendererCreature.cpp`, `Renderer::DrawPass`,
`Renderer::DrawCreatureParts`, `src/Creature/` (`creature_draw`, `creature_hair`, `creature_pose`, `creature_fizz`,
`creature_alignment_motes`), tests `test_creature_draw`, `test_dust_puff_kinds`, `test_creature_alignment_motes`.
The porting decisions are in [Port decisions: the creature](../project/port-decisions-creature.md).

### The frame's draw in openblack

- `Renderer::DrawPass` and `Renderer::DrawCreatureParts`: the body in the models' loop at once (the mesh's 0x200
  decides, as for any model), then the shadows over it, the hair and the eyes.
- The hair in `render_modes::materials::k_CreatureHair` (mode 9, ALPHAREF 50; the world quads' shader drops the
  texels under ALPHAREF) and `k_CreatureHairPlain` (mode 0). **⚠️ approximate** both two-sided: the ribbons are built
  facing the camera in openblack's own winding. **⚠️ approximate** the models are drawn per mesh, not in the
  original's draw-list order (near blocks first); with the hair writing depth this only changes which background its
  blended edges show. Before, the hair was in mode 6 (no depth write), so a model drawn after it and behind it (the
  temple) covered it.
- The eyelids: `SkinColourAt` reads the skin as `ObtainPointColour` does, `creature_draw::Eyes` gives each lid its
  count, `LidColour` multiplies, and `EyeRow` sets the colour (`PackInstanceColour`) with the body's specular. Before,
  the skin was read × 17 with the index wrapped, and both lids were a tint over the land's light before the haze,
  (L · t) >> 8: up to 14 a channel brighter on the first lid and 69 on the second.
- `ReceivesDynamicShadow` leaves out the creatures. The creature's own shadow is a complex caster in
  `graphics::shadow_list` in half rows, as the hand (`CreatureShadow`).
- **The hair's start** (`creature_hair::Fresh`, `CreatureHairSystem::Update`; user decision, 2026-10-15): the
  never-used-heap look, one of the original's possible looks. Every position and speed starts at 0, in world space;
  then `creature_hair::Step` runs unchanged. Our step keeps no copy of the positions between frames: it takes its copy
  (`before`) at the start of every step, after setting the root, as the original copies positions 1.. before reading
  them, so the original's copy array never reaches the result and only the positions and the speeds need a start.
  `creature_hair::Straight` (laid straight out, at rest) is kept for the step's tests.

### The sparkles in openblack

`creature_alignment_motes::PuffsOf` (the draws and the puffs, pure) runs from `CreatureAnimationSystem::PoseTurn`,
after the shape takes its targets and before the new pose, for every creature with a turn pose, unless the falling
spell is running (the original's falling copy is not ported, so nothing sparkles then). The puffs go into the
physics' pool (`DynamicsSystem::AddPuff` with a `physics::turn::PuffKind`: Dust is kind 4, Smoke 3, Mote 1,
MoteReversed 2), whose `PuffLife`, `DustPuffSize` and `DustPuffFrame` are the update and draw above, every kind and
edge pinned to the oracle (`test_dust_puff_kinds`, `test_creature_alignment_motes`). The motes are added in the dust
pass with their premultiplied tint (blend ONE / ONE, the same as mode 13's SRCALPHA / ONE on a white sheet). The
dust's own size takes the original's order of products; before, `size · (1 − age) · min(1, age / 0.125)` was a float
step off in 3 of 50 pinned ages, during its first eighth of a second.

### Drawing between turns in openblack

A creature's Transform moves once a turn (`creature_pose::CommitTurnPose`); every frame the locomotion writes
`components::CreatureDrawPose`, the position lerped from the turn's start to its end by the turn fraction and the
heading lerped the short way round (`CreatureLocomotionSystem::Update`). It is the villagers' "one turn behind" scheme
without their slope shear or their yaw rate.

- `creature_turn_pose::Drawn` is the original's blend on the same fraction (`game_clock::TurnFraction`); the
  locomotion's clamp to 1 is never reached. When `drawnBones` are empty (the hand, the physics, the first turn)
  openblack falls back to the Transform lerp, where the original always has its two buffers.
- `creature_pose::BetweenTurns` gives the pose while it belongs to the turn that put the Transform where it is (the
  locomotion started, `toPosition` equal to the Transform's position, no hand or physics pose); otherwise the
  Transform. `ecs::DrawnModel` takes it, decided per row, after the hand's and the physics' poses and before
  `DrawPosition`, with the scale it already had (the pen's drawn scale, else the Transform's): one matrix for the
  whole drawn creature. A standing creature (pose equal to its Transform) keeps the Transform's matrix bit for bit.
  Every reader takes that matrix (`ecs::DrawnBodyModel`): the body's row (`RenderingSystem::WriteEntityRow`), hence
  its shadow and the hair's light; the eyes (`CreatureAnimationSystem::PlaceEyes`), the hair roots, what it holds
  (`UpdateHeldDraw`, its `PhysicsDrawPose` only), the footprints, the hand's touch and pick (`CreatureHandSystem`:
  `CreatureAlong` and the strokes and slaps, the original's hand colliding the drawn, posed body), the leash's collar
  end ([The leash's collar end](leash-rope.md#the-leashs-collar-end)), and fire, spell seeds and tree bending
  (`DrawnModel` / `DrawnPosition`). `creature_pose::DrawnPlacementOf` gives the same sources' rotation alone, for
  where the eyes look ahead and the turn of what it holds. On the Transform, because the turn or the hash reads them:
  the head's look in `PoseBody` (its bones place the held object's Transform in the turn), the held object's
  Transform, the audio positions and the fight's hits.
- **⚠️ approximate** the hand's touch on a creature changes the game (which creature is picked, strokes and slaps that
  reach its mind): it now reads the drawn body, so it can differ from before by up to one turn's step while the
  creature walks, and in its pen it finds the smaller body that is drawn there.
- **The pen's drawn size reaches every part.** The eyes' size and how deep they sit shrink by
  `creature_pose::DrawnSizeShare` (the drawn scale over the Transform's, 1 outside a pen); the reflection's bounding
  radius takes `creature_pose::DrawnScale`. The hair and the footprints take the size it is drawn at
  (`creature_pose::DrawnSize`), as the original: in the pen the strands' length, width and root depth are
  `HairScale(pen size)`, larger than the body's share of its own size's (at size 2 in the pen 5.58 against the share's
  2.64); the footprint is `creature_footprints::Side(pen size)`. The hair's stiffness follows its scale, as for a
  smaller creature. **⚠️ approximate** the original's eyes under a drawn size are not read: they are scaled as one with
  the body here.
- **The eyes are drawn where they are placed.** `PlaceEyes` gives world matrices, and the eyes are drawn with the
  body's instance row (for its land light, fade and the lid's tint), which the instanced shader applies after
  `u_model`; so `creature_draw::InBodySpace` takes each eye into the body's space first (the row's inverse, its w
  components dropped as the shader drops them). Before, the body was applied twice: the eyes were drawn about 700 m
  off and at a sixth of their size. `Eyeball.l3d` and `Eyelid.l3d` have no bones, so the one `u_model` is enough.

### The fizz in openblack

- `creature_fizz` (`SetFizz`, `Step`, `TurnsOf`) holds the rules and `CreatureFizzSystemInterface`
  (`Locator::creatureFizzSystem`) the per-creature `CreatureFizz` component; the drawn value is
  `CreatureSpells::fizz`. `ProcessTurn` runs in the creature loop after the shown fatness and before the pose, as the
  step's place in `ProcessState`: it copies the fizz now to `CreatureSpells::fizz` first, so the drawn fizz trails the
  stepped one by a turn (`SetFizz` shows nothing at once), then a creature back in full sight with nothing left to do
  gives its component up, and only a fizz that moves is stepped and, having just reached 1.0 with the lock set, sends
  the creature to `creature_removal::RemoveFromGame`. The step takes `game_clock::MsPerTurn()`. The invisible spell's
  start, ease and end (`magic::spell_creature`) set the same fizz through `SetFizz(…, 0, false)`, so a carry home's
  fade is cut short as in the original. The sound goes through `audio::tags::CreateAtMapCoords` with the original's
  arguments. `UpdateFrame` moves only the eyes' static; each creature's own static moves in its fizzed draw, which is
  the renderer's.
- **Carried home.** `CreatureKnockedOut::fizzTurns` is the counter, `creature_fizz::TurnsOf(2, MsPerTurn())`.
  Lying → FadingOut sets it and calls `SetFizz(1, 2, false)`; FadingOut counts down and at 0 places the creature at
  `knockedOut.home`, calls `SetFizz(0, 2, false)` and `WakeFromFaint`, and sets it again; FadingIn counts down and at
  0 calls `SetFizz(0, 2, false)` and goes on to Waiting. `KillPermanently` and `Resurrect` of a creature fading out or
  in call `SetFizz(0, 0, false)` (raffclar's `StopCarryingHome`). A neutral creature is never carried home. The calls
  go through `Locator::creatureFizzSystem`.
- **The static draw** (`Graphics/RendererCreature.cpp`): `creature_draw::Body` carries `CreatureSpells::fizz` and
  `freeze`. `DrawCreatureBody` draws nothing at level 255, the body as it is at 0, and otherwise slides the
  creature's static on (in the world view, by the frame's game ms × 0.001; the renderer keeps it per creature) and
  calls `DrawFizzed`: the programs `ObjectMorphInstancedSpellLook` / `ObjectInstancedSpellLook`
  (fs_object_creature.sc, fs_object with `USE_CREATURE_SPELL_LOOK`), pass 1 in mode 18 with CCW culling (CW in a
  mirroring view), the shader testing `raw/S_Statica` slid by the creature's static against the level; pass 2 through
  the global alpha table with the alpha byte 255 − level and ZFUNC EQUAL, the shader taking (255 − level) / 255 as the
  vertex alpha, in place of the object's own and before the alpha test. Both passes' states and shader values come
  from the pure `creature_draw::FizzPasses` (tested in `test_creature_draw`). The eyes: not frozen,
  `DrawCreatureParts` draws each piece through `DrawFizzed` too, with the eyes' one static
  (`CreatureFizzSystemInterface::EyeStaticScroll`, read once a frame); at level 255 they are not drawn; frozen they
  are drawn as they are, the frozen look not being drawn. The hair is drawn only while the fizz and the freeze are
  both below 0.2 (`creature_fizz_look::HairShown`).

### Test hooks

None of the page's own; the creature's debug controls are listed in [Debugging](../project/debugging.md).

## Pending

Each known difference from the original is queued as its own measured fix; what is still unknown says what blocks it.

- **The hair's start** (done, [Where a strand starts](#where-a-strand-starts)): the original starts each strand from
  whatever its uncleared heap block holds, which depends on the machine and the run. User decision 2026-10-15: the
  never-used-heap look, one of the original's possible looks: every point and speed starts at 0
  (`creature_hair::Fresh`).
- **The evil and good sparkles** ([The evil and good sparkles](#the-evil-and-good-sparkles)), ported. Left: the
  falling film's copy sparkles in the original while the real creature does not (ours stops the real one, the copy is
  not ported); the Creature Cave's copy makes the draws (`creature_tattoo_editor::DrawCopyRest`) but no puffs, so its
  three CRT draws a moment are missing; the update removes a particle by moving the list's last into its place, ours
  keeps the order (only the draw order of overlapping puffs differs); a creature with no pose yet makes none, where
  the original always has one.
- **Drawing the creature, inferred until a creature is drawn:** the hair's ribbon winding against the original's
  one-sided material (see [Drawing in the frame](#drawing-in-the-frame)); whether the creature's shadow is also drawn
  over objects; the shadow projected from the posed base mesh rather than the morphed one; whether its eyes and hair
  are drawn in the water's reflection; and, for a creature that morphs with the terrain, that the morph program (which
  has no height-map version) stands in for the height-map one; which hair sets bit 2 of +0x24 (the wireframe strands).
- **The eyes under a drawn size** in the pen: not read in the original.
- **The Creed glows** (not ported): the key table, the slot rules and the save of the four slots as a component on
  the creature (`CreatureCreedGlows`, 4 slots {a, b, angle}), keys 95..109 wired beside the dust, the native 338 and `SetCreedPower`'s looping sound 0xA6 (to name);
  then the draw (mesh 0x20D at the bones above, the scrolling effect: read `fn_0083EFE0`, `fn_0083F270`,
  `fn_0083F100`, `fn_0083F210`, `fn_0084F910`) in the transparent queue. Which animations carry the keys 95..109: grep the animations' effect keys.
- **What the flag [0xECA660] is for** on a path that draws through fn_00858BA0 while it is set.
