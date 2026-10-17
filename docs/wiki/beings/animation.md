# Skeletal animation

## Summary

Villagers and animals are skinned meshes posed from clips (`Data\AllAnims.anm`, the format in
[Clips](../formats/media-files.md#clips)). A clip plays by the game time of the frame, or, in the states that move, by
the ground the walker covers, so the feet keep step; a change of clip is an instant cut. Each state picks its clip
through the state table or an animation function (the gait by the speed, the hurt walk, the builder's tool). The
simulation moves the walkers once a turn and the draw places them between their turn-start and turn-end positions.
Villagers carry tools and loads at the hand's bone, hide inside their homes, and the intro's family is drawn in high
detail with eyes that blink. This page owns the common rule of drawing between turns (the creature's own is on
[Drawing](../creature/drawing.md)).

openblack reproduces the playback, the villagers' advance by the ground, the clip choice, the speed, the creation
index, the drawing between turns, the carried props and the high-detail family; the animals still use an older float
advance, and the clip advances every frame rather than inside the draw.

## The original

### Playback

- Pose (`LH3DAnim::GetPose` 0x839980): i = n·t/period; **linear interpolation of the 12 floats** between key i and the
  next one, without quaternions or re-orthonormalising; W = L · W(parent) (the root under the object's matrix).
- Time (fn_005167D0): + ms of game time of the frame (0 when paused); with loop modulo duration, without loop it stays
  at the end. States that move (info `field0x14`): advances by distance, metres / scale / stride × duration
  (fn_0051AF00). The exact steps are in [The clip's advance by the ground](#the-clips-advance-by-the-ground).
- Clip change (`Living::SetAnim` 0x5ECBA0): **instant cut** (there is no blending even though the engine has it), the
  same clip does not restart. Per-object state: clip and time in ms.
- The distance advance of moving states (metres / scale / the clip's stride × its duration) is computed by
  fn_00516840.
- `LH3DAnim::GetPose` 0x839980: evenly spaced keys (a looping clip wraps its last key to the first, a one-shot one ends
  on it); every element of the 3×4 matrices lerped, no re-orthonormalisation. The blend sampler fn_00839BC0 is the
  same but the next key after the last is always the first (0x839C1C), one-shot clips too.
- moving states: metres moved / scale / the clip's stride * its duration (fn_00516840); clips with a stride under 0.05 (flag 0x200) always advance with time
- The SuperVillager's clip cross-fade (ECS/SuperVillager.h; fn_00825530 0x8256C8..0x825755, 0x8257C4..0x825A09). crossFadeMs > 0: a clip change blends the clip drawn before it (frozen at its last time) into the new one over that many ms (300, [0xC383D8]); 0, every other villager: the instant cut of Living::SetAnim 0x5ECBA0. With DrawPosition::followSnap (SuperVillager +0x30 bit 2, 0x8257C4) the fade counts down but is not drawn. Set by ECS/SuperVillager every frame

#### The clip's advance by the ground

`Villager::Draw` 0x51B940 (0x51BA74, 0x51BAD5) and `Living::Draw` 0x51AEC0 (0x51AEEC) call fn_0051B3D0 with the object's
scale, which calls fn_0051AF00 (0x51B3DA) once per frame drawn; `Animal::Draw` 0x51C310 (0x51C34C), `Dove::Draw`
0x41F680 (0x41F777) and `SpellWolf::Draw` 0x51C560 (0x51C579) call fn_0051AF00 directly. A villager's clip moves only when the landscape's draw lists draw it: `Villager::Draw`
(Villager vt +0x610) has one direct caller (`SpecialVillager::Draw` 0x71F3D6) and five vt +0x610 sites,
`GLandscape::Draw` 0x5E4BC2 (the SuperVillager draw list, `g_first` 0xEB9A08, each entry drawn when `IsAvailable`),
fn_005E5CD0 (0x5E6086 / 0x5E610F / 0x5E6165, called only from GLandscape::Draw 0x5E4EDD) and `Town::DrawDesireFlags`
0x73D8B5. The object's Game3DObject
(+0x40) holds the clip (vt +0x184), its time in **whole signed milliseconds** (get vt +0x18C, set vt +0x188, the value
in edx).

- **Gate** (0x51AF0A..0x51AF3C): vt +0x8B8 (`Villager::IsMovingForAnimation` 0x756C60: the top state's byte +0x8C
  indexes the state table, entry 0x114 bytes, the dword at 0xDB9E8C, i.e. `field0x14`; for animals
  `Object::IsMoving`), a clip, and the clip's flag 0x200 clear. Otherwise (0x51B0B6..0x51B204) the time advances by the
  frame's ms g_game +0x205D48 (the same whole ms as +0x250540, see [Coordinates](../engine/coordinates-and-angles.md))
  through fn_005167D0: the same placement as the ground's below, with the frame's ms as the play (0x51B1DD..0x51B204).
- **0x200**: set by the clip loaders when `fabs(stride +0x28) < 0.05` (0x83A679..0x83A68B, 0x83AB0C..0x83AB34, the
  double 0.05000000074505806 = 0.05f); a 0x200 already in the file's flags stays (the header is copied whole). A NaN
  stride also sets it (the unordered compare sets C0).
- **Metres** (0x51AF42..0x51AF84): `u16 speed +0x5A` zero-extended × the frame's ms g_game +0x250540 (`imul`, the low
  32 bits, loaded as an unsigned qword by `fild`), stored to float; `Object::GetScale` 0x402520 × 655350.0f
  [0x8D8748] (rounded to float, the FPU being at 24 bits); the first over the second, stored to float. The speed is the
  one +0x5A has at the draw, whether or not the villager moved that turn.
- **Played** (fn_00516840 0x516840): `metres / stride` (float), `fimul` by the play time +0x20 (int32), `__ftol`
  0x7A1400: towards 0 (cvttsd2si; NaN or out of range gives 0x80000000).
- **Placed** (fn_005167D0 0x5167D0): `next = time + played` (int32, `lea`); `next < playTime` (signed): the sound
  events in [time, next) (fn_00516510: a list at clip +0x48, an event fires when from ≤ t < to), time = next.
  Otherwise the events in [time, playTime); a looping clip (0x100) takes `next % playTime` (`idiv`), a one-shot one
  `playTime`; if that is not 0 the events in [0, it) fire too: a one-shot clip held at its end fires all its events
  every frame.
- **Below 0** (fn_0051B3D0 0x51B3E6..0x51B3FC): after the draw's matrices, a negative time is set to 0.
- A clip of no play time would divide by zero.

### Which clip plays

`Villager::GetAnimId` (0x750110):

- State 0 or ≥ 255: `P_STAND` 385. If the state has an animation function (fixed table 0xD09198, slot 0x60) it
  decides; if not, the state's clip in info.dat (`villagerStateTable.field0x0`; −4 = not drawn).
- Walking (`MoveToPosAnimation` 0x423400): life ≤ 0.15 crawls, ≤ 0.30 limps; by speed (info.dat threshold
  `speedThreshold` 1 men 3/4 m/s, 0 women 2/3): WALK / RUN / SPRINT `_MAN`/`_WOMAN`; carrying a tool or load
  `CARRY_AXE` / `CARRY_OBJECT_RUN`. The carried object is looked up first (+0xF1, 0x423406..0x423426); the wounds are
  not tested for a villager controlled by a script (+0x25 & 4, 0x42342B: it goes straight to its gait, 0x423477), so a
  scripted villager walks or runs however hurt it is. The crawl tests the life (vt +0x11C) against info +0x380
  (`lifeWhenCrawlsWounded`, → 225), the limp against +0x37C (`lifeWhenWalksWounded`, → 426), both ≤.
- The gait (0x423477..0x423527): the villager info's +0x1F8 picks the limits, 0 → `speedThreshold[1]`
  (SPEED_THRESHOLD_VILLAGER_MAN: walk dword 0xD38370, run 0xD38374), otherwise `speedThreshold[0]`
  (SPEED_THRESHOLD_VILLAGER_NORMAL: 0xD38358 / 0xD3835C). The array is the 10 `GSpeedThreshold` of 0x18 bytes at
  0xD38348 (`GSpeedThreshold::GetBaseInfo` 0x606B70; the static constructor 0x606B40 steps 0x18), the two limits at
  +0x10 / +0x14 of each. The u16 whole speed +0x5A (map units a turn), zero-extended, is compared with each dword by
  `cmp` / `jg` (signed, the limit included): speed ≤ walk → WALK 427 / 431 (carrying: 215 `CARRY_AXE`), else ≤ run →
  RUN 351 / 353 (216), else SPRINT 383 / 384 (216). The carry flag (+0xF1 − 2 through the byte table 0x423530) is set
  for objects 2, 3, 4, 6 and 9..15; the values outside 2..15 carry nothing. info.dat: men 1966 / 2621 (3 / 4 m/s),
  women 1311 / 1966 (2 / 3 m/s). A script's SET Speed 0.3 m and 0.4 m are 1966 and 2621 (m / 10 × 65536, truncated),
  exactly on the limits: at 0.3 a man walks and a woman runs, at 0.4 a man runs. On Land 1 the tutorial `FollowUs`
  sets the son to 0.3 and, later, the father to 0.4.
- State change (`SetTopState` 0x5F28E0): exit clip of the previous state (except `field0xf0`), otherwise that of the
  new state and its entry clip. While one of those plays the state logic waits (turns × 100 ms ≥ duration). Quirk of
  the original: after an exit clip the entry clip is not seen, the state's clip is repeated.
- `Living::SetAnim` 0x5ECBA0, called through GetAnimId (vt +0x900) at 0x5ECB85: a negative id or the clip it already
  has → nothing; otherwise the clip (LH3D +0x180) and, with n and not dancing, its time from 0 (+0x188,
  0x5ECBDB..0x5ECBF8). A state can name its own clip instead of GetAnimId's (`Villager::AmazedByMagicShieldReaction`
  0x765FA5 with 395 TALKING_AND_POINTING). `Living::SetStateAnim` 0x5ECB10 and the script's OVERRIDE_STATE_ANIMATION
  (0x6F9FD3..0x6FA00D) start a new clip from 0 with no dancing test. A dance move's clip step
  (`UpdateGroupBasedOnDancerAction`) is on [Dances](dances.md#the-dances-turn).
- `Villager::CallOutofAnimationFunction` 0x756620 returns −1 when the next state has no out-of clip (state table file
  +0xF0, 0xDB9F68). Otherwise it calls the current TOP's into / out-of function with (0, next).
- An out-of clip sets the flags 0x800 / 0x1000 of Villager +0xE0 (+0xE1 |= 0x18, 0x756699).
- `CallIntoAnimationFunction` 0x756590 calls the TOP's function (Villager +0x8C, 0x756598) with (1, entered). An into
  clip sets 0x800 and clears 0x1000 ((flags | 0x800) & ~0x1000, 0x7565F5).
- `BuildingAnimation` 0x423E20: the working clip after its into clip, otherwise one of the three at random (GameRand
  0x6DE510), and it sets the carried object: hammer, saw or heavy mallet. The into clip is only looked at while the
  transition flag 0x800 is set (`test byte [+0xE1], 8`, 0x423E23, the same test as `SitDownAnimation` 0x424213) and a
  clip is present (vt +0x184, 0x423E39): 284 `P_INTO_HAMMERING` → 276 `P_HAMMERING`, 292 `P_INTO_SAW_WOOD` → 354
  `P_SAW_WOOD`, 293 `P_INTO_SLEDGEHAMMER` → 380 `P_SLEDGEHAMMER`, with no draw. Otherwise (flag clear, no clip, or any
  other clip, a working clip included) `GameRand(3)` (Animations.cpp line 0x27B): 0 → 276 with the hammer (+0xF1 =
  8), 1 → 354 with the saw (5), 2 → 380 with the heavy mallet (9).
- `LookAtLargeObjectAnimation` 0x423A80 (state 152 LOOK_AT_MAGIC_TREE, set at 0x5A6519 into the function table
  0xD09198 + 152 × 0x90 + 0x60): `(byte +0x59 >> 1) < byte +0x58`, unsigned bytes → 311 `P_LOOK_AT_HAND`, else 310
  `P_LOOKING_FOR_SOMETHING`. The two bytes are the state counter's (+0x58, u16):
  `PerformBewilderedByMagicTreeReaction` 0x764B50 sets both to `GameRand(ftol(1000 / [0xD01A38] × 3.0)) − ftol(1000 /
  [0xD01A38] × −2.0)` (0x764CE3/0x764CE6, a look of 20 to 49 turns), and `LookAtMagicTree` 0x764D70 plays the state's
  clip again (vt +0x8FC, `Living::SetAnim(1)` 0x5ECB80) when the low byte equals half the high byte (0x764DAD), then
  decrements the low byte (0x764DBB) and at 0 goes back to 150 (vt +0x8E8, 0x764DC9). The tree is a MagicTree (a Tree of 0x74 bytes, vtable
  0x92CA30) whose reaction 8 carries the spell's player, its `GetPlayer` 0x5FD060 ([Forest](../magic/forest.md#magictree)). So a villager looks at its hand
  for the first half of the look and about it for the rest.
- `LookAtFlyingObjectAnimation` 0x423AD0 (states 12 LOOK_AT_FLYING_OBJECT_REACTION and 161
  WATCH_FLYING_OBJECT_REACTION): the info.dat clip of the PREVIOUS state (byte +0x8E, `Infos` clip 0xDB9E78 + 0x114 ×
  state) is −4 → −4 (hidden, 0x423AF4). Else the reaction object (Living +0xBC, `GameThingWithPos*`, the object the
  reaction's setup stores, null → 385 `P_STAND`, 0x423AFE): its altitude (+0x1C, the MapCoords' altitude at +0x14)
  minus `LH3DIsland::GetAltitude` 0x803090 at its MapCoords (+0x14) (`fsubr dword`, rounded to a float: the game
  thread's FPU runs at 24 bits, see [Floating point](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)),
  compared with the float 3.0 (`fcomp dword` 0x8C2C50): **strictly greater** → 311 `P_LOOK_AT_HAND`, else (equal,
  lower, or unordered) 385. No draw. A previous state outside the table would read past the table's 255 rows.
- `MoveToPosIntoOutofAnimation` 0x4242B0 (the moving states' into / out-of clip): nothing (−1) when the other state is
  a death state (vt +0x970 = `Living::IsDeathState` 0x5ECAA0: 13..17 unsigned, so SET_DYING, DYING, DEAD, DROWNING and
  DOWNED) or the current clip (read through the villager's 3D object +0x40, 0x4242CC..0x4242D9) is not 225
  `P_CRAWL_INJURED`; otherwise into 226 `P_CRAWL_INJURED_INTO`, out of 330 `P_OUT_OF_SLEEP`.
- `PauseForASecondAnimation` 0x424080 (state 239): a poisoned villager (vt +0x4A4 = `Living::IsPoisoned` 0x416F90,
  +0xB4 bit 1) plays 342 `P_POISONED` with no draw (0x42408C); otherwise `GameRand(2)` (0x42409E) gives 333
  `P_OVERWORKED2` for 1 and 332 `P_OVERWORKED1` for 0.
- The animation functions span 0x423400..0x424442. The reactions' own clips (162, 12 and 161) are on
  [A thing thrown from the hand](villager-reactions.md#a-thing-thrown-from-the-hand-reaction-9).
- BuildingAnimation (0x423E20): the working clip after its into clip, else one of the three at random and it sets the carried object: hammer, saw or heavy mallet (GameRand 0x6DE510, as every draw here)
- Villager::SitDownAnimation 0x424210: only while an into / out-of clip plays (+0xE1 & 8 = flag 0x800, 0x424213) the current clip decides (367 -> 369, 370 -> 372); else GameRand(2) (Animations.cpp 0x35A): 0 -> 369, 1 -> 372
- Villager +0xE1 |= 0x18 (0x756699): the flags 0x800 / 0x1000 of Villager +0xE0
- `VillagerSetStateClip`: 0x5ECB85: GetAnimId (vt +0x900); 0x5ECBA0: a negative id or the clip it has -> nothing; else the clip (LH3D +0x180) and, with n and not dancing, its time from 0 (+0x188(0), 0x5ECBDB..0x5ECBF8)
- Living::SetAnim(clip, n) 0x5ECBA0 with a clip of the caller's, not GetAnimId's (the states that name one: Villager::AmazedByMagicShieldReaction 0x765FA5 with 395)
- Living::SetAnim(clip, n) (0x5ECBA0) with a clip the state names itself instead of GetAnimId's (the amazed villager's 395 TALKING_AND_POINTING, 0x765FA5)
- LandedAnimation 0x423730: by the landType (Villager::EndPhysics 0x5F0BA1)
- Villager::LandedAnimation 0x423730 (LANDED's clip function, slot 0x60), the landType = (+0xB4 >> 4) & 3: 0 -> +0xF1 == 1 (nothing carried) ? 308 P_LANDED_FROM_FEET : 309 P_LANDED_FROM_FEET_CARRY_OBJECT (0x423743..0x423751); 1 -> 306 P_LANDED; 2 and 3 -> 307 P_LANDED_FROM_BACK (0x423757..0x423760)

### Hidden villagers

`Villager::Draw` does not draw the villager if the info.dat clip of its current state is −4 (`ANM_DONT_DRAW`, e.g.
AT_HOME) nor with the at-home flag (+0xE0 & 4, `ArriveHome` / `LeaveHome`). The draw test is in `Villager::Draw`
0x51B940 (the current state's clip −4).

### Size and mesh by age

`Villager::SetAge` (0x7528C0): child if the age is less than `grownUpAge` (13), with the meshes `childMeshHigh..Low`; an
adult is at least 18 years old. Scale (`InitialiseScale` + `SetScaleForAge`): adult 0.9 and then 1.05 − rand(0.1) (in
(0.95, 1.05]); child `ageToScale[age − 1]` + rand(0.75 × the distance to `ageToScale[age + 1]`). The mesh level drawn
is LOD 1.

### Walking speed

**Units**: 1 world unit = 1 m (MapCoords 6553.6 per metre). The speed u16 (+0x5A) is what it advances **per turn** in
MapCoords (`GetSpeedInMetres` 0x60C070 = u16 / 6553.6), and the info.dat tables (speedGroup) are in those units: 1475
= 0.225 m/turn = 2.25 m/s. A normal man does 2.25 / 1.16 ≈ 1.94 step cycles per second, with the feet synchronised
(the clip advances with the same distance).

**Computation**:

1. On each state change (`SetStateSpeed` 0x753760) the speed comes from the speedGroup entry requested by the final
   state (index in `villagerStateTable.field0x24`), × 0.85 if it has a town (base of the town's needs) × the wood and
   food loads (no load: 1); injured: 0.4-0.6 × speed4 or 0.5-0.75 × speedDefault.
2. `SetSpeed` (0x750ED0) multiplies it by f = 1 + ((creation index × 47) % 31 − 16) × 0.01, minus: child min((13 −
   age) × 0.02, 0.4); old (> 60) min((age − 60) × 0.02, 0.4); adult 0.1 × life (and hunger cubed) and 0.2 for women;
   it is truncated to u16.
3. The map scale (`GLandBalance::Values[4]`, `SET_GLOBAL_LAND_BALANCE`: 1.5 in Land2, 1.25 in Land3), times the town
   player's Indian tribal power (at least 0.1), and for a town of a player other than the neutral one times 1 + the
   belief scale × the town's belief in its player beyond the town's neutral belief, multiplies everything
   ([Speed](villager-speed-and-scripts.md#speed)). Nothing in SetStateSpeed reads a wonder term.

Result: a normal man goes at ~1.7-1.9 m/s and a woman at ~1.4-1.6.

### Creation index

The counter of `Object::Object` (0x636520, g_game+0x205A48) used by `SetSpeed`.

- Every thing derived from Object takes the next number when created (buildings, trees, features, rocks, villagers,
  animals, pots, the creature, lanterns; the TotemStatue is one); towns, forests, fogs, roads, rivers, planes and the
  hand do not count.
- Things that take a number: 7 TownDesireFlags per CREATE_TOWN, the ScriptHighlight of storehouse / creche / workshop
  / wonder / graveyard (except the African ones), ShowNeedsVisuals and the workshop's woodpile, the spell icons of the
  town centre (one per distinct seed, up to 6), dispenser + seed, and heart + entrance + three leash posts of the
  temple, 5 indices in that order ([The temple's leash posts](../creature/temple-leash-posts.md#the-temples-leash-posts);
  the heart's base classes take one index, its alignment flock none).
- Resets to 0 when loading a map (ClearMap → `GData::Reset` 0x510750) and starts at 2 on the first map after start-up
  (two HelpSpirits).
- The old-age gate `(turn + id) % 800` uses the unique key heap's id, and the did-you-know highlight `(turn + index) &
  7` the index.

### Drawing between turns

The simulation moves villagers and animals once per turn; every frame the original draws them between their position
at the start of the turn (Living +0x2C, copied in `Living::ProcessLiving` 0x5EC810) and the one at the end, with the
turn fraction (0..0.99; fn_0051AF00): one turn behind, without extrapolating, with both heights interpolated. When
paused the fraction freezes and the ms are 0 ([Game clock](../engine/game-clock.md#game-clock)).

- Only in the states that move for the animation and with a stride clip (animals: if they moved).
- Turning: the villager turns its drawn yaw towards the real one at 0.003 rad/ms (more than 90°: 0.012·|d|/π rad/ms;
  `Villager::Draw` +0x108); animals turn per turn (their AI limits turning, [Animals](animals.md)). Birds bank in
  turns (`Dove::Draw`).
- Terrain slope: on the ground (altitude ≤ 0.2) it is sheared with the slope: row0 += a·row1, row2 += b·row1 with a, b
  the rise of the terrain at one unit along its x, z axes (±0.3; fn_0051B220).
- The creature blends its bones instead: the drawn bones are `f·current(+0x5178) + (1 − f)·previous(+0x5174)`,
  `f = min(turn fraction g_game +0x205D64, 2.0)`, element by element over the 3×4 matrices (`PrepareForDrawing`
  0x4ED32E..0x4ED376, `fn_004EECD0`). There is no yaw rate, no slope shear and no separate position lerp; its parts
  are drawn from the same matrices ([Drawing between turns](../creature/drawing.md#drawing-between-turns)).

### Objects in the hand

The villager's `CARRIED_OBJECT` (+0xF1, set by `SetStateCarriedObject` and by `BuildingAnimation`: hammer, saw or
mallet) is drawn with its mesh (table of `CarriedObject::Init` 0x462530: axe 342, fishing rod 355, crook 354, saw 383,
bag 343, ball 344, hammer 367, mallet 378, scythe 384, shovel 390, wood 406, branches 347-349; the table is at
0xC5E19C, none for 0, 1 and out of range) attached to bone 15 of the pose (the grip at the end of the arm −X), with the
bone's −X, −Z, −Y axes and no offset (`LH3DStaticObject::SetLinkedPosition` 0x815FC0). `Villager::Draw` reaches it
through fn_0051BAF0, which draws `CarriedObject::Get3DCarriedObject(+0xF1)`. It is not drawn in the hand nor in the
hidden states. `Villager::CreateDroppedResource` 0x750993 makes the dropped log from the same table.

### The family in high detail (SuperVillager)

SET_HIGH_GRAPHICS_DETAIL 290 (0x708CE0) makes a villager a SuperVillager (fn_00825F20): its mesh is swapped for the
high-detail one — `Data\MISC\Intro\nors_man.l3d` (501, father), `nors_woman.l3d` (498, mother), `nors_boy.l3d` (439, son
and every Celtic / Norse male child), `Data\MISC\sable.l3d` (420, the creature trainer, info.dat row 77) — with 22 bones
identical to the villager meshes', so the villagers' own clips drive them unchanged. Its own draw path (fn_008254A0 →
fn_00825530): a 300 ms clip cross-fade, a second yaw smoothing at 5π/4 rad/s (3.92699 rad/s, [0x9A392C]; turned on a
local copy of the drawn matrix, 0x8255AB), the default sun as light, no haze, and the eyes: one `eye_ball.l3d` drawn twice and four lids on bone
8, blinking every Random(1000..5000) ms (the CRT stream) with a shared glance. Released every frame unless a script
holds the wide screen (0x5E4B3A). THING_JC_SPECIAL 349 gives the cinematic's orders (7 follow the intro hand's grip, 8
snap, 9 mirror the yaw, 16/17 ±π/2).

Its shadow is a TemporaryShadow (fn_00825090, list 0xEB99FC): a projected shadow (ShadowInfo) lit by the fixed sun
0xEA1C88 and falling on the land only, updated by `GLandscape::Draw` 0x5E4BFB. While it lasts the villager loses its
ground blobs (SetHumanShadowed(0)) and its fade with the distance (SetDisappear(0), the same as feature 19); both come
back with it (fn_008250F0).

- A SuperVillager is 0x34 bytes, in the list g_first [0xEB9A08] (newest first).
- `SET_HIGH_GRAPHICS_DETAIL` 0x708CE0: POP the object (0x708CEF), then the bool (0x708D0A); no thing → "Thing not
  found!" (0xC0CFAC, 0x708D13). With "off" it calls fn_00825440 (Release).
- `THING_JC_SPECIAL` (`GScript::ThingJCSpecial` 0x709000): POP the object, the feature (0x70902C), then the bool
  (0x70903E); no thing → "Object no longer valid" (0x709047). Feature 9 uses the heading setter and getter
  (`MobileWallHug::SetYAngle` vt+0x524 (0x639260), `Object::GetYAngle` vt+0x508 (+0x4C)).
- Release is fn_00825440 / `SuperVillager::Release` 0x826180. fn_00825E70 deletes the Eyes whatever the thing's state
  (fn_00884570).
- Every SuperVillager is released and the list emptied from four places: `GGame::ClearMap` 0x552BE7,
  CleanGameForScriptReboot 0x6EBBC2, the script camera release 0x6ECEA6 and `GLandscape::Draw` 0x5E4B6D.
- The on-screen test: fn_00825400 0x82541D runs `LH3DBoundingBox::CheckRegionOnScreen` on the drawn mesh's box. Only
  when it passes does fn_00825530 run (0x825422 je 0x82543D): the fade, the yaw stage and the eyes, Random draws
  included.
- The feature-7 position is set in the driver loop at `GLandscape::Draw` 0x5E4BC8..0x5E4BEE, after `Villager::Draw`
  0x5E4BC2.
- The HD mesh comes from `LH3DMesh::CreateFromHD(path, 0)` 0x8067F0. The EBone block is flagged 0x200000 (0x826004 /
  0x826122).
- fn_00825530 0x82555D lights the drawn position with fn_00801C90 into the host's +0x4C / +0x50 (both written on every
  path, 0x8020E4 / 0x802107..0x802109).
- fn_00825530 calls no fn_007FEB30, so the haze that `Villager::Draw` 0x5E4BC2 put there is overwritten and the family
  has no haze.
- The yaw stage is fn_00825530 0x8255C1..0x8256BF, 0x825A13 (not with bit 2, 0x8255B8); the swim branch poses with the
  object's own matrix. Once a frame it wraps its own yaw into [−π, π] (fn_007FAAF0 0x82556F / 0x82557C, once). Equal to
  the wrapped target, or snapped, it takes the target with no turn (0x8255F8); else it steps `(ms × 0.001 [0x8AC418]) ×
  rate [0x9A392C]` towards it, taking it when the difference is not more than a step (0x8256C1). The turn is
  `own yaw − Wrap(target)` (0x825626): the copy of the sheared object matrix (0x8255AB `rep movsd`, 12 dwords) is turned
  about its own Y by it (0x825626..0x8256BB, `lh_matrix::RotateY`, rows 0 and 2), never the object itself. A new
  SuperVillager starts with the object's yaw (fn_00825F20 0x825FBC: +0x14 = obj+0x48).
- The SuperVillager's cross-fade state (fn_00825530 0x8256C8..0x825755, once a frame): the first clip is only kept
  (0x8256CF..0x8256DD); another clip starts a fade from the clip and time drawn last, with the fadeMs left and weight 1
  (0x8256EA..0x825716); the same clip counts the fade down by the frame's ms (0x825722..0x82574F; below 0 it is 0,
  otherwise weight = left / fadeMs).
- The blend is drawn while there is fade left and +0x30 bit 2 is clear (0x8257B6 / 0x8257C4; the same bit as the yaw
  stage's 0x8255B8); the last time drawn is stored at 0x825E2D..0x825E36.
- The cross-fade itself (fn_00825530 0x8259B0..0x825A09): the blend sampler fn_00839BC0 of the new clip stored with
  weight 1 − [+0x90] (`fld 1, fsub` 0x8259B6; each float `((b − a)·f + a) × weight`, stored when the weight is not 0,
  0x839C9A), then the old clip's at its own time with weight [+0x90] added (0x839D91, `fadd [edi]` 0x839E6B), in the
  bones' local matrices, no normalisation; then each bone under its parent (fn_00839F10). The blend sampler is
  `LH3DAnim::GetPose`'s except that the key after the last is always the first (0x839C1C), one-shot clips too. A clip
  for another skeleton leaves the rest pose.
- The root of the bone buffer [0xC37D9C] is fn_00825530's turned local copy of the matrix (0x8257CD..0x8259AC).
  fn_00883560 reads that buffer after the body.
- Each SuperVillager's eyes (fn_008254A0, in fn_005E5CD0 +0x585 = 0x5E6255) are drawn after its body, on screen only,
  after the fish (0x5E4B2B) and `PetitNavire::PostDraw` 0x5E6250. Before that, the draw loop draws the `Random(0, 2π)`
  of the swimming rings [0x5E4CB4..0x5E4CC2].

The eyes (`Eyes`, 0x54 bytes): ctor fn_00884100, update and draw fn_00883560, lids fn_00883EA0, dtor fn_00884570.

- The files are in Data\MISC\Eyes, slots 0..4: r_paupe_up, r_paupe_down, l_paupe_up, l_paupe_down, eye_ball
  (0xC3A21C..0xC3A340).
- Eye types 0 and 1 use the same files. Type 2 uses the "*2" ones (0x88415F..0x884393).
- Each file is `LH3DMesh::CreateFromHD(Data\MISC\Eyes\<file>.l3d, 1)` (0x884431).
- The eyeball's first material texture becomes [0xEA1A90] .\data\Textures\misc0.raw (0x884456..0x884466).
- Each lid's first material texture becomes the host mesh's first material texture (0x884491..0x88449F; mesh +0x10
  [0] +8 [0] +8, 0x88440B..0x884415).
- For 0 ≤ type < 3, fn_008840E0 doubles every vertex uv of every primitive (fadd st, st; 0x8844B8..0x884533).
- Objects (0x8843E3..0x884553): `LH3DObject::Create(0)`, an `LH3DStaticObject` (vtable 0x9A2974), then vt+0x98(0) and
  vt+0x58(0), which turn off obj+4 bits 8 and 0x20. vt+0x98 is SetDisappear (bit 8: no fade with the distance) and
  vt+0x58 = fn_008168C0 is SetDynamicLighting (bit 0x20, set only with a non-zero argument, so cleared here);
  fn_007F97D0 reads it back, and the object draw fn_0080DB30 tests it at 0x80DF17: false goes to the unlit fn_00856D40
  with the object's colour as it is, no N·L and no ambient 90. So the eyes are **unlit**: their colour, the host's land
  light × each eye's shade from the default sun (fn_00883560, below), is drawn as it is, inside the default-sun bracket
  only by place. 📼 1.20 only: that AddDrawing fn_00815A70 reaches fn_0080DB30 for them, traced to 0x815F05.
- The eyeball's SetAnimatedUV is (k / 8, 0.75), with the iris cell k = {4, 3, 5}[type] ([0xC3A208], ctor 0x88446B,
  fmul [0x8AB620]).
- The lids' SetAnimatedUV (fn_00884000) is zeroed by fn_008840E0 for these types, so they get none.
- The eyeball (+0x4C) is one object drawn twice (0x883B22 eye 0, 0x883E6B eye 1). Slots +0x3C..: 0 r_up, 1 r_down, 2
  l_up, 3 l_down.
- Timers: +0x38 hold = 200 at creation (0x884153). +0x28 is zeroed when a blink ends (0x8835AA); no reader has been
  found.
- The shared state: glance target [0xFAA7E8], squint [0xFAA7EC], squint clock [0xFAA7F0].
- The step (0x88356A..0x883712) starts with closure = 0 (0x883574). Closing and opening take 50 ms each (0x8835B2 /
  0x8835D0), and the blink ends after hold + 100 ms (0x883583).
- The closure changes by 0.02 [0x8CB930] per ms.
- When a blink ends, the next hold is Random(100, 200) ms (ftol, 0x88358D..0x8835AD). When the countdown drops below 0
  a blink starts and the next is Random(1000, 5000) ms away (0x883614..0x88363F).
- The glance moves towards the shared target at 0.7 [0xC3A214] rad/s, scaled by 0.001 [0x8AC418]
  (0x883642..0x8836C0). On arrival there is a new target, Random(−0.25, 0.25) (0x8836AF / 0x8836B6).
- The squint is Random(0, 0.3) (0x8836E2) every 300 ms [0xC3A218] of its clock. The clock resets to 0, not to −300
  (0x8836C6..0x883712). The squint is the closure's floor.
- Eye k's matrix (0x883712..0x883802; eye 1 at 0x883BD6..0x883C07) is v R E_k bone, in rows.
- The bone is copied from [0xC37D9C] + 0x30 × (+0x304) (0x88371C..0x883735) in camera space and taken back to the
  world with [0xEA9DE0] = SetInverse(g_world_to_clipping [0xEA9E40]) (0x819BB0; fn_007FAFF0 0x883742). Then come E_k
  (fn_007FAE60 0x883753) and R (0x883802).
- The ground blobs' fn_0081FFF0 makes the same round trip (0x8200E5..0x82065E).
- R (0x883758..0x883802) uses c = cos π and s = sin π of the double 3.1415927410125732 [0x8D45D0]. Its rows are (1,
  0, −s), (0, 1, 0), (−s, 0, −1): a mirror of z (det −1), not a turn.
- The glance (0x883A98..0x883B1A) turns the eyeball about its own Y when the glance is not 0: r0' = c r0 + s r2, r2' =
  c r2 − s r0. c is stored as a float (0x883AB5; eye 1 0x883DF6) and s stays on the FPU stack.
- The lids (fn_00883EA0) turn rows 1 and 2 by closure × 0.47 [0x8C7A4C] (upper) or closure × −0.35 [0x9A3D80]
  (lower): r1' = c r1 − s r2, r2' = c r2 + s r1 (0x883ED5..0x883F42).
- c is stored as a float there too (0x883ECF upper, 0x883F72 lower). The upper lid is slot 2k and the lower is 2k + 1.
- The shade (0x883865..0x8839CE; eye 1 0x883C0C..0x883D5B): the table 0xC3A1D8 (type × 16 + eye × 8) gives the yaw a
  and pitch b of the eye's lighting normal n = (sin a cos b, −sin b, −cos a cos b).
- cos a, −sin a and cos b are stored as floats (0x88386F, 0x88387C, 0x883889; eye 1 0x883C1B, 0x883C28, 0x883C35).
  sin b is rounded by `0 + sin b` (0x8838C9 / 0x883C71).
- n M is summed as (y r1.x + z r2.x) + x r0.x for both n0 and n1 (0x88390F..0x883950; eye 1 0x883CB9..0x883CE4).
- n2 is (x r0.z + z r2.z) + y r1.z for eye 0 (0x883950..0x88396C) but (z r2.z + y r1.z) + x r0.z for eye 1
  (0x883CE7..0x883CF8).
- |n|² is (ny² + nz²) + nx² for eye 0 (0x88396F..0x883982) and (nx² + ny²) + nz² for eye 1 (0x883CFB..0x883D0F). It
  is normalised with InverseSquareRoot 0x841170 (0x88398B / 0x883D18).
- The default sun [0xEA1C88] is normalised as (y² + z²) + x² (0x88381B..0x883838; InverseSquareRoot 0x883841; stored
  0x883846..0x883860).
- The dot is (nz sz + ny sy) + nx sx (0x8839A9..0x8839BD / 0x883D36..0x883D4A), then I = fistp(255 × dot).
- Colour (0x8839D0..0x883A3B): the host's colour × the eye's factor, alpha kept. The host's specular goes to all five
  objects. They are written into the objects' +0x4C / +0x50 (0x883A3D..0x883A82), which are drawn instead of the land
  light.
- `UpdateAnimations`: fn_00825400 0x825422 je 0x82543D: no fn_00825530 this frame
- `UpdateAnimations`: 0x8257B6 / 0x8257C4: while +0x94 != 0 and not +0x30 bit 2 (DrawPosition::followSnap, the same bit as the yaw stage's 0x8255B8).
- `UpdateAnimations`: 0x825E2D..0x825E36
- `StepCrossFade`: 0x8256CF..0x8256DD
- `StepCrossFade`: 0x8256EA..0x825716: from the clip and time drawn last
- `StepCrossFade`: 0x825722..0x82574F
- fn_00825530 0x8256C8..0x825755, a SuperVillager's fade state (SkeletalAnimation::crossFade) once a frame: the first clip is only kept; another clip starts a fade from the one drawn last (old clip, its last time, fadeMs left, weight 1); the same clip counts the fade down by the frame's ms (below 0: 0, else weight = left / fadeMs, fild / fidiv). True while there is fade left (the blend is drawn, 0x8257B6)
- fn_00883560 0x88356A..0x883712, before the matrices: closure = 0; the blink (Random(100, 200) when one ends, Random(1000, 5000) when one starts), the glance towards the shared target at 0.7 rad/s (a new Random(-0.25, 0.25) target when it is there), the shared squint (Random(0, 0.3) every 300 ms of this counter) as the closure's floor
- The eye k matrix of 0x883712..0x883802 (0x883BD6..0x883C07 for k = 1): v R E_k bone in rows, the bone in the world (the original's camera-space bone x [0xEA9DE0], the camera to world) = boneWorld E_k R in glm
- `ShadeIntensity`: 0x883865..0x8839CE (eye 1: 0x883C0C..0x883D5B): the table 0xC3A1D8 (type x 16 + eye x 8) gives the yaw a and pitch b of the eye's lighting normal n = (sin a cos b, -sin b, -cos a cos b); n M normalised (InverseSquareRoot 0x841170, the exe's sum orders, which differ between the two eyes), dot the normalised default sun [0xEA1C88], I = fistp(255 dot) (model_light::Intensity)
- Eyes ctor 0x88446B: the iris cell of misc0, u = k / 8 with k = {4, 3, 5}[type] ([0xC3A208]), v = 0.75
- The first material's texture: primitive 0 of submesh 0 (the original's mesh +0x10 [0] +8 [0] +8, 0x88440B..0x884415)
- `DoubleUvs`: 0x8844D9..0x884533: every vertex uv of every primitive doubled (fadd st, st)
- The intro family's high detail models. SET_HIGH_GRAPHICS_DETAIL (op 290) makes a villager a SuperVillager (engine class, list g_first [0xEB9A08], 0x34 bytes): drawn by its own path (fn_008254A0 -> fn_00825530) with the HD mesh of Data\MISC\Intro, a 300 ms clip cross-fade and a second yaw smoothing, and mesh eyes (ECS/SuperVillagerEyes.h). THING_JC_SPECIAL (op 349) sets its flags. They live only while a script holds the wide screen (GLandscape::Draw 0x5E4B3A..0x5E4B93). With the mod game.skip-intro "free start" SET_WIDESCREEN is skipped (CHLApi.cpp), so the family is released every frame: test and take screenshots with --mod game.skip-intro=off.
- Once a frame before ECS/MobileDrawing: GLandscape::Draw 0x5E4B3A..0x5E4B93 (no script wide screen: all go;
- LH3DMesh::CreateFromHD(Data\MISC\Eyes\<file>.l3d, 1) (0x884431) with the ctor's material and uv edits, once per file (and host skin for the lids) into the mesh manager. 🔍 inferred the original's per-Eyes copies are identical. (pending) it makes the meshes' bgfx buffers from op 290 / 349 during the turn (game logic), not before the game or through the GPU queue
- `LoadEyeMesh`: 0x884456..0x884466: the first material's texture = [0xEA1A90], .\data\Textures\misc0.raw ("raw/misc0")
- `LoadEyeMesh`: 0x884491..0x88449F: the host mesh's first material texture;
- `LoadEyeMesh`: 0x8844B8..0x884533: fn_008840E0(type) (0 <= type < 3): every uv doubled
- `EyeMatrix`: ⚠️ approximate composed in the world. The exe copies the bone [0xC37D9C] + 0x30 x (+0x304) (0x88371C..0x883735), in the space of g_world_to_clipping [0xEA9E40] (the affine LH3D camera with the field of view in its rows, UpdateCamera 0x819B60..0x819BA1), back to the world with [0xEA9DE0] = SetInverse(W2C) (0x819BB0; fn_007FAFF0 0x883742), then E_k (fn_007FAE60 0x883753) and the mirror (0x883802). fn_0081FFF0 (the ground blobs) does the same round trip (0x8200E5..0x82065E). (pending)
- One float operation per statement: the FPU runs at 24 bits (each fadd / fmul rounded to a float), but fsin / fcos give extended values, so it matters which of them are stored. cos a, -sin a and cos b are stored as floats (0x88386F, 0x88387C, 0x883889; eye 1 0x883C1B, 0x883C28, 0x883C35); sin b is rounded by `0 + sin b` (fadd, 0x8838C9 / 0x883C71) before its store
- `ShadeIntensity`: x -1 (0x8838EE)
- `ShadeIntensity`: 0x8838FA
- `ShadeIntensity`: 0x883903
- n M with the LHMatrix rows r0, r1, r2 (glm's columns), in the exe's sum orders: n0 = (y r1.x + z r2.x) + x r0.x and n1 the same (0x88390F..0x883950; eye 1 0x883CB9..0x883CE4); n2 = (x r0.z + z r2.z) + y r1.z for eye 0 (0x883950..0x88396C) but (z r2.z + y r1.z) + x r0.z for eye 1 (0x883CE7..0x883CF8)
- |n|^2: (ny^2 + nz^2) + nx^2 for eye 0 (0x88396F..0x883982), (nx^2 + ny^2) + nz^2 for eye 1 (0x883CFB..0x883D0F); InverseSquareRoot 0x841170 (0x88398B / 0x883D18), then each component times it, stored
- the default sun [0xEA1C88] normalised: (y^2 + z^2) + x^2 (0x88381B..0x883838), InverseSquareRoot 0x883841, each component times it, stored (0x883846..0x883860)
- `ShadeIntensity`: (nz sz + ny sy) + nx sx (0x8839A9..0x8839BD / 0x883D36..0x883D4A), x 255 stored, fistp
- `IrisU`: fild, fmul [0x8AB620]

### Addresses

The original's addresses the code's comments used to quote, with the openblack symbol each corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `fn_00825530 0x8255C1..0x8256BF, 0x825A13` | the SuperVillager yaw stage (not with bit 2, 0x8255B8); the swim branch poses with the object's own matrix | `Update (DrawPosition follow)` |

### Sources

- The executable, read with the xref index; bw1-decomp `Villager.cpp`, `Animations.cpp`; the clip format on
  [Media files](../formats/media-files.md#clips); the clip sounds on [World sounds](../audio/world-sounds.md#clip-sounds).

## openblack

**Done** for the villagers' playback, advance and clips, the speed, the creation index, the drawing between turns, the
carried props and the high-detail family. **Partial**: the animals' advance; when the clip advances. Progress:
[Villager looks, voices and special villagers](../project/progress/), [animal](../project/progress/),
[Creature animation](../project/progress/). Clips and meshes load once through the resource caches; per-object
animation state lives in ECS components (`components::SkeletalAnimation`, `components::DrawPosition`); the playback and
speed formulas stay pure functions, unit tested with fakes ([Internals](../project/internals.md)).
openblack differs: every clip advances every frame, not only when the draw lists draw it (`src/ECS/Animations.cpp:111`),
and animals advance by time, not by the ground (`src/ECS/Animations.cpp:124`), fix rows.

- **Playback**: `L3DAnim::SampleLocal` (`SampleLocal` with `blendSampler` is the blend sampler), `graphics::ComputePose`
  (`3D/SkeletalPose`); the SuperVillager's cross-fade is `ComputeBlendedPose` there, in the exe's float order.
- **The advance by the ground**: `ecs::villager_clips::PlaysByGround`, `MovingPlay`, `FramePlay`, `AdvanceClip` in
  `ECS/VillagerClips`, used by `ecs::UpdateAnimations` for villagers, whose moving states give
  `SkeletalAnimation::groundSpeed` = `ecs::WholeSpeed(WallHug)`: every step as above, in single precision, the time
  kept whole, by the ground when the gate passes and by the frame's ms when it fails; both place the clip and fire its
  events through `AdvanceClip`, so a one-shot clip held at its end fires all its events every frame either way. Before,
  the ground fed the step actually taken (and 1e-6 m/s while standing in a moving state, so the clip froze), in float
  without the truncation (the clip ran 1-2 % ahead), and neither advance made the one-shot replay. A clip of no play
  time stays at 0. A villager clip that the `OPENBLACK_TEST_ANIM` hook freezes (speed 0) keeps still and silent
  outside the moving states. What the exact advance moved in a run (measured against the float advance, by a
  per-entity dump of the state hash's transforms): on Land 1 and in the hand demos nothing; in the land cycle a single
  entity, the prop carried in a walking villager's hand on Land 2 from its turn 48, which follows its grip bone and so
  the walking clip's time. The random trace is the same. Animals still use the older float advance, by the ground moved
  (`distanceSpeed`) and by time: for whole ms the time advance places the clip the same, but it does not replay a held
  one-shot clip's events.
- **Which clip plays**: which function a state uses is `villager_animation::ClipChoiceOf` / `TransitionOf` and the clip
  rules are pure functions of `ECS/VillagerAnimation`; `villager_animate::StateClip`
  (`ECS/Systems/Implementations/VillagerAnimate`) applies them to a villager, and `ECS/VillagerAnimations` plays the
  clips of a state change, called from `LivingActionSystem::VillagerSetState` and `Update`. The functions that need
  what does not exist yet (landing type, water, vortices, dances, fights, football, creatures) take the original's
  branch for its absence. The hurt walk is `villager_animation::HurtWalkClip(controlledByScript, …)`, given
  `script_held::IsControlledByScript`; the gait `villager_animation::GaitOf(WholeSpeed(wallHug), walk, run)`, whole
  numbers, both limits included (before, it compared metres a second with the limits less half a unit, so a speed
  exactly on a limit took the faster gait). `villager_animate::SetAnim` (and `PlayStateClip`, the state's clip through
  it) asks `villager_clips::PlaceOnSetClip`, so a dancer's new clip takes over at the same time; `SetStateAnim` and
  `OverrideAnim` have no dancing test; a dance move's clip step is `villager_clips::OnDanceMove`. The builder's clip is
  `villager_animation::BuildingClip(transitionPlaying, current, random)`, given `SkeletalAnimation` `transitionFlags &
  0x800` as for the sitting clip (before, it followed an into clip whether or not the flag was set). The magic tree's
  look is `villager_animation::LookAtLargeObjectClip` with `LivingAction::turnsUntilStateChange` (low byte the turns
  left, high byte the look's length); states 149..152 (the magic-tree reaction) are not ported, so the state is only
  reached by a script setting it. The flying object's look is `villager_animation::LookAtFlyingObjectClip(previousStateClip,
  heightAboveLand)`; `VillagerAnimate` gives it the previous state's table clip and, for +0xBC,
  `villager_reactions::SlotObject` (the `LivingReaction` slot's object): its `Transform` y minus `GetHeightAt` at the map
  point `map_coords::FromWorld` makes of it, in float (`villager_animation::HeightAboveLand`): a difference just above 3
  in double that rounds to 3.0f stands. No height without a slot object or a Transform. A previous state outside the
  table (255, none yet) is taken as not hidden. openblack enters neither state yet (no setup sets 12 or 161), so the
  clip is only reached by a script setting the state; before, it always gave 385. The moving states' into / out-of
  clip is `villager_animation::TransitionClip` with `IsDeathState` (13..17); the pause clip
  `villager_animation::PauseForASecondClip(poisoned, …)` with `life::IsPoisoned`.
- **Hidden villagers**: openblack removes the mesh in the meantime (`SkeletalAnimation::hiddenMesh`), so the villager
  cannot be picked up either.
- **Size and mesh by age**: openblack once used 18 for `grownUpAge`; it is 13 now. Deliberate difference (user, 2026-10-15): the villagers' high meshes
  are drawn only at detail level 6, the top setting ([Meshes and LOD](../rendering/meshes-and-lod.md)).
- **Walking speed**: `ECS/VillagerSpeed`, `LandBalance.h`. openblack moved `WallHug::speed` = 2.25 per turn (10 times
  too fast): now it is m/s × 0.1.
- **Creation index**: `ECS/ObjectCreationIndex`. What openblack does not create yet reserves its number (the list
  above; its places of worship not yet) (**not checked** that each one is still missing). Checked: the 55 villagers of
  Land1 have the same index as a simulation of the original's counter. Trace `OPENBLACK_OBJECT_INDEX_TRACE=1`.
- **Drawing between turns**: `ECS/MobileDrawing` (the birds' bank too); the fraction and the frame ms come from
  `game_clock`; `UpdateMobileDrawing` runs it once a frame (the SuperVillager's yaw stage only when on screen,
  `followFrozen` otherwise), and `StepYawFollow` is the yaw stage's step, keeping `DrawPosition::follow*`
  (`DrawPosition::followYaw`, `DrawPosition::followDrawnTurn` the turn of the drawn copy, applied by `DrawnBodyModel`).
  `components::DrawPosition` only changes the drawing (RenderingSystem, object in the hand, foot shadows
  with the animated pose); the logic uses the Transform. When picking up or landing it is adjusted without sliding.
  The creature is drawn through `CreatureDrawPose` and `DrawnModel`.
- **Objects in the hand**: `ECS/CarriedProps` (`UpdateCarriedProps` after the poses, `CarriedObjectMesh` the table, also
  used for the dropped log); the carried object is `SkeletalAnimation::carriedObject`. It is an entity
  of its own (Transform + Mesh + `CarriedProp`) that moves every frame. Today almost no openblack state sets an object
  (the jobs are missing).
- **The family in high detail**: `ECS/SuperVillager*`, reached from the two natives in `CHLApi.cpp` and updated, made
  to follow the intro hand and drawn every frame from `Game.cpp`; the shadow is a `components::DynamicShadow` on the
  thing, as the launched boat's. The eyes are composed in the world, not through the camera space and [0xEA9DE0] as the
  exe does: the same matrix to a few ulp until the LH3D camera matrix is ported bit for bit. openblack gives the six
  eye entities `components::Unlit`, and the object shader's set-colour branch draws such a colour without the model
  light.
- **Rendering**: each villager with a pose (`components::SkeletalAnimation`) is drawn separately with its bones
  (`ecs::PosesByInstance` in the instance loop of `Renderer.cpp`); the rest is still instanced. Object reflections
  (`DrawObjectReflections`: what the hand holds and what is thrown) also use the pose.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Animations.cpp`: `UpdateAnimations`, `StepCrossFade`.
- `src/ECS/Animations.cpp`, `UpdateAnimations`: (openblack) a fade from a clip that is not loaded draws the plain pose
- `src/ECS/Animations.h`: `StepCrossFade`.
- `src/ECS/VillagerAnimations.cpp`: `StateFunctionAnim`, `OutOfClip`, `VillagerSetStateClip`, `VillagerSetClip`, `AnimFn::Landed`.
- `src/ECS/VillagerAnimations.h`: `VillagerSetClip`, `VillagerLandedClip`.
- `src/ECS/SuperVillagerEyes.h`: `Step`, `EyeMatrix`, `ShadeIntensity`, `IrisU`, `FirstSkinId`, `DoubleUvs`.
- `src/ECS/SuperVillager.h`: `SetIntroHandGrip`.
- `src/ECS/SuperVillager.h`, openblack draws the body with the villager entity itself (RenderingSystem, the swapped Mesh); the fade and the yaw stage are parameters of the villager's smooth drawing (components::SkeletalAnimation::crossFade*, DrawPosition:: follow*, ECS/Animations.h and ECS/MobileDrawing.h), set here every frame.
- `src/ECS/SuperVillager.h`, `Update`: a thing gone: it goes), the swim rings of 0x5E4C7D..0x5E4D4B (a swimmer, one clock for all, Random(0, 2 pi) every 1000 ms, ecs::AddWaterRing), fn_00825400's on-screen test (SuperVillager::onScreen: LH3DBoundingBox::CheckRegionOnScreen 0x868C80 = region_on_screen::SphereOnScreen) and this frame's parameters of the villagers' smooth drawing (frozen off screen)
- `src/ECS/SuperVillager.h`, `SetIntroHandGrip`: fn_005DFCE0's grip of the intro hand while [0xD19C34] != 0 (nullopt else): Game.cpp sets it once a frame, before Update, from ecs::ial::Grip
- `src/ECS/SuperVillagerEyes.cpp`: `LoadEyeMesh`, `ShadeIntensity`, `IrisU`, `FirstSkinId`.
- `src/ECS/SuperVillagerEyes.cpp`, `LoadEyeMesh`: openblack's mesh takes the skin with it
- `src/ECS/SuperVillagerEyes.cpp`, `EyeMatrix`: openblack has no such matrix (its camera is bgfx's view-projection), so the round trip would add another rounding, not the exe's: the same matrix to a few ulp.
- `src/ECS/Components/SkeletalAnimation.h`: `crossFadeMs`.

### Test hooks

- `OPENBLACK_TEST_ANIM="clip[,ms]"`: that clip on all villagers (locked; with ms, frozen at that instant; with
  `OPENBLACK_START_PAUSED=1` they do not move).
- `OPENBLACK_TEST_CARRY=<type>`: the carried object.
- `test_super_villager_eyes.cpp` pins the original's float bits: the glance targets 0x3D023908 and 0x3D2E1960, the
  squint 0x3D6D8842; `test_super_villager_smooth.cpp` the rate's bits 0x407B53D2 (3.92699 rad/s).
- Traces: `OPENBLACK_ANIM_TRACE=1`, `OPENBLACK_DRAW_TRACE=1`, `OPENBLACK_OBJECT_INDEX_TRACE=1`.

## Pending

- **Creation index**: since the heart takes five indices, Land 2's land cycle moves from its turn 5 (the villagers made
  after its two temples walk at their own speed factor, read from the index as `Villager::SetSpeed` 0x750ED0 does).
  Which villager moves first at turn 5 is not traced (a run with the object-index and villager traces would name it).
  Two readers of the index are approximate and could move a later run: the old-age gate `(turn + id) % 800` through
  `UniqueId`, where the original uses its unique key heap (queued fix), and the did-you-know highlight's `(turn +
  index) & 7` (queued fix).
- **Clips**: the jobs (objects in the hand) and the `GetAnimId` branches that depend on what does not exist yet (see
  [Which clip plays](#which-clip-plays)) (with the villager jobs' port rows).
- **The animals' clip advance by the ground** (fn_0051AF00 from `Animal::Draw`, gated on `Object::IsMoving`) is not yet
  the exact one villagers use; and the time advance does not replay a held one-shot clip's events as fn_005167D0 does
  (queued fix).
- openblack differs: it advances every `SkeletalAnimation` entity every frame, where the original moves a villager's
  clip inside `Villager::Draw` (fn_0051B3D0 → fn_0051AF00) only for the villagers the landscape's draw lists draw
  (`src/ECS/Animations.cpp:111`), a fix row. It would move the shots and the clip sounds, not the hash.
- **The first clip of an animal**: openblack chooses it in the frame after its creation (`UpdateAnimalAnimations`);
  wiring `ecs::living::AnimalFirstClip` at the creation is issue #12's animal part.
