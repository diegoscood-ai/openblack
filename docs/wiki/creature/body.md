# The creature's body

## Summary

A creature's body is a base mesh of its species pulled towards three pairs of variant meshes (evil-good, thin-fat,
weak-strong) by its alignment, fatness and strength, drawn at its size. The size grows with age, is kept within 0 and
2 by the growth, and can be pushed beyond by the size spells; the drawn size is kept within 0.05 and 4. The body's
radius, which the walk, the route plan and the hand use, comes from the rest pose's height and the stand animation's
reach, both measured once per load.

openblack draws the body with the original's mesh blend and seams, eases the shown fatness and redraws the shape in
the original's steps once a turn, ages and grows the creature as the original, runs the size spells' start delay and
targets, and gives every species the original's radius to the bit. Not yet ported: the locked-select skip of the
body's turn, the size spells' joint-activity partner and the immediate redraw by scripts and setters (see
[Pending](#pending)).

## The original

### The body

- **Seams (vertex blends).** Each primitive of a mesh lists blends of 8 bytes (the vertex that moves and the vertex
  it moves towards, both counted in the primitive, then a float weight). The original applies them in its boned
  primitive draws, after every vertex of the primitive has been placed by its own bone into camera space and lit, and
  before the clip codes and the projection: in list order, in place, `v = v + (p − v)·w` for x, y and the depth, the
  weight as the file has it, not clamped. Only the position moves. A global switch, on at start, gates them; one object
  draw turns it off while it draws with a non-zero first argument (the addresses are in the
  [creature's address table](overview.md#addresses); see [Pending](#pending)).
- **Which meshes carry blends** (counted with the l3d component over the game's data): every mesh in
  `Data/CreatureMesh` (all species and their six variants, 144 to 418 blends each, weights 1e-14 to 0.5, 33 090 in
  all) and three spell meshes in `Data/Spells/Anims` (`PUPIL.l3d` and `TEACHER.l3d`, 224 each, and
  `Tree_Goddess_Boned.L3D`, 1); nothing else: no mesh of `AllMeshes.g3d` (626, 167 of them boned: villagers,
  animals), not the hand (`Hand_Boned_*2.l3d`, `hand_intro.l3d`), not the eyes. In the data no vertex moves twice,
  none moves towards a vertex that moves, none names a vertex outside its primitive or itself, so one partner per
  vertex, read from the vertices as placed, gives the original's result. The three spell meshes are unused data: no
  string "PUPIL", "TEACHER" or "Goddess" is in runblack.exe (a case-insensitive scan of the image) and no game data
  file names them [V], so the original never loads or draws them.
- **The morph**: the body is the base mesh pulled towards one mesh on each of three axes (evil-good, thin-fat,
  weak-strong); threshold and vertex blend as the hand's (0.03; base + |a|(m − base),
  [hand-and-interface.md](../hand/animation-and-placement.md)). How the targets become the drawn shape is in
  [The fatness the body shows](#the-fatness-the-body-shows).
- **The default tattoo symbols** come from `Data\Textures\OriginalChooseSymbol.raw`, 256×256 RGB (196 608 bytes),
  the exe's own file: `runblack.exe` names `PlayersSymbols`, `ChooseSymbol` and `OriginalChooseSymbol` and loads them
  at 0x5DE410 / 0x5DE425 / 0x5DE43D ([rendering.md](../rendering/frame.md), the `k_AlphaFlagStems` list), each with its `a.raw`
  alpha. `I_PLAYER_SYMBOLS_.raw` and `I_PLAYER_SYMBOLS_A.raw` are a repack's copies that the exe never loads: neither
  name occurs in `runblack.exe`; `I_PLAYER_SYMBOLS_.raw` (65 536 bytes, one grey byte a texel) is byte for byte
  `ChooseSymbola.raw`, the alpha, and `I_PLAYER_SYMBOLS_A.raw` (196 608 bytes, RGB) is byte for byte
  `OriginalChooseSymbol.raw` in the Complete Collection's `Data\Textures`. The tattoos themselves are on
  [Tattoos](tattoos.md).
- **Footprints are never taken away** 📼 1.20 only: the two functions that would empty the list and free it, 0x81F2C0 and
  0x81F2D0, have no call, jump or pointer to them (the exe's cross-reference index, and a byte scan of the 1.20
  executable for their addresses and for relative calls and jumps to them), and the count [0xEB9978] is written only
  by them, the maker `fn_0081F360` (0x81F7FD) and `fn_0081F820` (0x81F9B0). So the prints laid on one land stay through
  a load or a land change and show at their old places on the next land until they fade, up to 256 of them for at most
  about five seconds, floating or buried where the land differs: the original's behaviour, kept.

### Growth, size and age as time passes

`phys` is the creature's physical part (creature +0x160).

- **Where in the turn.** `Creature::ProcessState` 0x472DC0: the spells (0x472EB4), the pen's size
  (`ShrinkDownIfNearCitadel`, 0x472F72), then, unless the creature is in a locked select (`Flags +0x24 & 0x10`,
  0x472F77, which jumps to 0x4737DB), the autoscale step (0x472FB7), the turn counter phys+0x70 `++` (0x472FC2) at
  every development phase, and, inside the block run only when [0xD00DE8] is not 0 (0x4732FA `cmp [0xD00DE8], ebx`,
  0x473300 `je 0x473391`; see [the gate](mimicry.md#gplayermakecreatureempathisewithplayertowndesire); 1, set by the GGlobal constructor at start-up, 0x590F2A [V]), when [0xD00DE0] is also set (1 from the GGlobal
  constructor, 0x590F42 [V]) and the phase (creature
  +0x1268) is at least 1, `UpdateAttributesAsTimePasses` 0x4EF510 (0x473337).
- **Age.** Inside it (0x4EF510..0x4EF53E): phys+0x08 `++` when phys+0x70 % ((1000 / [0xD01A38]) × info+0x210) == 0,
  all unsigned 32-bit integers: `div` of 1000 by the ms a turn (0x4EF517), `imul` by info+0x210 (0x4EF52E), `div` of
  the counter (0x4EF538). The turns a second are cut to a whole number before the multiply, so at 30 ms a turn the
  period is 33 × info+0x210. The counter runs from phase 0, so turns before phase 1 count towards the first tick.
  With more than 1000 ms a turn the period is 0 and the original's `div` faults.
- **Carrying.** 0x4EF541..0x4EF5DA: when phys+0x28 (what it holds 📼 1.20 only) is set and `IsAvailable` (vt +0x2C), and
  `LH3DCreature::IsMoving` 0x484300 on phys+0x58 is true, the share is `GetWeight` (vt +0x638) of what it holds over
  `GetWeight` of the creature (phys+0x54): 0 when the creature's weight is 0 or compares with nothing (`fcom 0`,
  `test ah, 0x40`), else the quotient, 0 below 0 or when it compares with nothing (`test ah, 1`), 1 above 1
  (`test ah, 0x41`; 0x8AA398 = 0.0, 0x8AA390 = 1.0). Then `ModifyStrength` 0x4F0170 by `0.5 / (info+0x22C × 600)`
  times the share (0x8AA3B4 = 0.5, 0x8AB5F4 = 600.0). In the Creature vtable (0x8CC810) vt +0x638 is
  `Creature::GetWeight` 0x47CD60 = `LH3DCreature::GetMass` 0x47FA80 on `GetCreature3D` (creature +0x160): the mass
  by the drawn size +0x90 and the thin-fat and weak-strong targets ([Struck by a thrown
  thing](hits-and-catch.md#struck-by-a-thrown-thing), step 5). The physics body stays at 1000 (`Creature::SetUpPhysOb` pushes
  0x447A0000 at 0x479BD5 and calls no `GetWeight`). Anything else held weighs `Object::GetWeight` 0x638480:
  `GetScale` (vt +0x120, +0x50) cubed times the info's weight (info +0xAC).
- **Growth.** 0x4EF5DF..0x4EF60A: `LH3DCreature::IsMoving` is called and tested (`test eax, eax`), but the
  conditional jump after it is two NOPs (0x4EF5E9, 0x4EF5EA): the moving test is patched out and the creature grows
  moving or not. It grows when the phase is at least 3 (0x4EF5EE), by `CalculateHeightIncrease` 0x4EFA60.
- **The size step** 0x4EFCA0: phys+0x6C += the growth; below 0 it is set to 0, above 2 (0x8AB478) to 2. It runs every
  turn the growth does, so a creature made bigger than 2 by other means is put back to 2 by its next growth.
- **The size when made.** The `CreaturePhysical` constructor (0x4EF3A6..0x4EF3B8) calls `LH3DCreature::SetSize`
  0x480530 (vt +8 of the 3D body) with info+0x1FC, the species' start scale (the file row's +0x1EC; 0.22 for all 17
  species in `info.dat`, read from the data), then stores the same value, as it is, in phys+0x6C: the creature's own
  size is not clamped. `SetSize` works out its speeds from the value as given (× 0.5, 0x480538..0x48055F), then keeps
  it for the body (+0x90) between 0.05 (0x8AC3F4; at or below it, or when it compares with nothing, `test ah, 0x41`)
  and 4 (0x8AB418; at or above it, `test ah, 1`), and draws at that × 15 (0x8C2C40) / the rest height +0x8C (+0x94).

> **Which exe this is.** The two NOPs at 0x4EF5E9 are the community **Creature Growth Patch**, which the original used here includes; the plain published v1.42 (sha1 `9c9aea96…`) keeps the moving test, so its creature grows only while it stands still. The patched exe stays the original (user, 2026-10-17); see [Reading this wiki](../project/reading-the-wiki.md).

### The size spells: the start delay and the targets

`tps` = 1000 / [0xD01A38], an integer.

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
- **The handler table** [V]: the spell state machine's per-kind handlers are at 0xBE0380, 16 rows of 0x68 filled by
  0x4F3CD0: kind +0x00, start +0x10, ease +0x20, beginFinish +0x30, finish +0x40, hold +0x50, sound +0x60. Only Itchy
  has a hold (0x4F5DF0); Freeze has none; Nice and Nasty have start, ease and finish; the five needs only start and
  finish.
- **The freeze's handlers** [V]: the freeze's row of the handler table (0x4F3CE2..0x4F3D08: start 0xBE0390,
  ease 0xBE03A0, beginFinish 0xBE03B0, finish 0xBE03C0) holds 0x4F5430, 0x4F5470, 0x4F54E0 (a bare `ret`) and
  0x4F54F0. The start calls `FinishActionUnsuccessfully("frozen by spell", 1, 1)` (0x4F5431..0x4F543C), writes 0 to
  the mind's "active" flag +0x20CE4 (0x4F5449) without reading it, stops the walk (`GetCreature3D` 0x4F5453,
  `LH3DCreature::StopMoving` 0x4F545A) and sets +0x12BC, the frozen amount, to 0 (0x4F545F). The ease clamps the
  ratio (fn_004F5360 on +0x370 + 4) to 0..1 and writes 1 − ratio to the physical's +0x48 and the ratio to +0x12BC
  (0x4F54C3..0x4F54CE). The finish writes 1 to +0x20CE4 (0x4F54F6), again without reading it, 1.0 to the physical's
  +0x48 (0x4F5506) and 0 to +0x12BC (0x4F550D). The other writers of +0x20CE4 are the `CreatureMental` constructor
  (0x4D2480), two packets in `GPacket::ProcessPacket` (0 at 0x63D049, 1 at 0x63D189), and `Creature::Load`, which
  reads it from the save at 0x4E63A9..0x4E63B9 when the file's version [0xCAB174] is at least 0x1A (`Creature::Save`
  writes it out at 0x4E514F), so a mind turned off before a freeze is turned off again by it and runs again at the
  thaw.
- **A miracle that goes** [V]: at each turn's start (0x4F490C) the pre-pass 0x4F4D20 takes each slot whose miracle
  (+0x14) is no longer available: the miracle is set to 0 and 0x4F4B40 cuts the spell short (hold = 0, and the counter
  = 0 when it is in state 3). A waiting entry (+0x184, count +0x18C, stride 0xC, +8) only loses its miracle.
- **The start sound** [V]: the start (0x4F4968..0x4F4995) plays the kind's sound through `SamplePlayAnimEffect` from
  the Spells bank (GGlobal +0x3B4) at distance 0.
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
  Property %d" / "Cannot Set Property %d" (0x70E741, 0x70F287); the GET gives 0. With a joint-activity partner (mind
  +0x1BE8 set), big's target is `min(+0x374, max(1.8 · d, d))` (0x47DB64..0x47DBBC) and small's
  `min(+0x378, min(0.5555556 · d, d))` (0x47DBE4..0x47DC3C), d = the creature's own drawn size (`GetCreature3D` on
  the creature, +0x90, recomputed every turn); 1.8 is [0x8CF030], 0.5555556 [0x8CF034]. Small's is read as written: a
  `min` with the smallest size, so with a partner small never aims above it.
- **The partner.** Mind +0x1BE8 is the joint-activity partner, a Creature pointer, not a flag [V]. Sub-action 89
  RequestPartner (stage 1 0x503A90) sets it on both creatures when the other accepts (0x503B82: this creature's
  +0x1BE8 = the partner at 0x503B88, the partner's = this one at 0x503B95). The partner is the agenda row's object, and
  its answer comes from one of four functions picked by the row's type (types 2, 3, 6, 16: 0x4F62D0 / 0x4F6340 /
  0x4F6410 / 0x4F64D0); a refusal finishes the action unsuccessfully and clears the plan's +0x1CEA0 slot
  (0x503B52..0x503B74). `Creature::FinishActionGeneric` clears it (0x4758BB), and so does
  `CreatureMental::RemoveBeliefsAboutDeletedObjects` when the partner is deleted (0x4D896B).
- **The ease.** Big 0x4F55F0: `v = target ≥ before ? target : before`; small 0x4F5540: `v = target ≤ before ? target
  : before` (`test ah, 0x41`). Then the size phys+0x6C = before + (v − before) · ratio, the target read again every
  eased turn.

### The fatness the body shows

`phys` is the creature's physical part (creature +0x160), `3d` its drawn body (phys+0x58).

- **Where in the turn.** `Creature::ProcessState` calls `Update3DCreatureFromAttributes` 0x477210 at 0x4737D2: after
  the time-passing body (0x473337) and after `ProcessSubAction` 0x4FEDA0 (0x4737B0), the step in which a meal is
  eaten, so the fat a meal puts on starts to show the same turn. It runs when [0xD00DE4] is not 0 (0x4737B5; 1 from the GGlobal
  constructor, 0x590F30 [V]), right after the exhausted slow-down `fn_004EFC40` (0x4737CB), and is
  skipped with the rest of that block under the locked select (the jump from 0x472F77 lands at 0x4737DB, past the
  call).
- **The shown fatness** phys+0x18 (the mind file's "previous fatness"): `d = clamp(phys+0x14 − phys+0x18, −0.01,
  0.01)` (0x477217..0x477243; a `d` below −0.01 is −0.01, above 0.01 is 0.01), then `phys+0x18 = clamp(phys+0x18 +
  d, 0, 1)` (0x477249..0x477278; below 0 is 0, above 1 is 1). The thin-fat target 3d+0xA4 = clamp(2 · phys+0x18 − 1,
  −1, 1) (0x477284..0x4772B8). With the FPU at 24 bits each `fsub` / `fadd` rounds to float, as openblack's float
  arithmetic does.
- **The other two targets, in the same function.** The evil-good target 3d+0x9C = clamp(alignment, −1, 1) (creature
  +0x168 → +0x08, 0x4772BE..0x4772FC); the weak-strong target 3d+0xAC = clamp(`ConvertStrengthTo3D` 0x4F02B0 (phys+0x0C,
  and a second argument read at [creature+0x28]+0x1F4), −1, 1) (0x477302..0x477368).
- **The shape drawn from the targets, once a turn and in steps.** `Morphable::UpdateMorphing` 0x618C40 runs in
  `LH3DCreature::UpdateTime` (0x481FD8, after `TimeWarpHeal`, before `AddEvilGoodSparkles` and before the turn's pose,
  `UpdateBuffers` at 0x48257F), once a creature turn. With the last applied values at 3d+0xA0, +0xA8 and +0xB0 and
  0.03 at [0x900AD4]: when |evil-good target − applied| ≥ 0.03, all three axes take their targets (and the texture and
  the animations are redone); else when the thin-fat gap is ≥ 0.03, thin-fat and weak-strong take theirs (the
  animations redone); else when the weak-strong gap is ≥ 0.03, weak-strong alone. Each time the vertices are baked
  again on the CPU (`MorphVertices` 0x618D10: base + |w|·(mesh − base) per axis, the mesh of the weight's sign, the
  normals renormalised). Drawing never blends the shape: `PrepareForDrawing` 0x4ED320 uses the turn fraction for the
  pose only and never reads the morph. So the body's shape moves in steps, as the original draws it: thin-fat, whose
  target moves 0.02 a turn, about every two turns; evil-good and weak-strong in one step when the alignment or the
  strength moves by 0.03 or more. The other callers: `Creature::SwapMinds`, a script's ALIGNMENT set,
  `Creature::CanLoad3D` at load, two attribute setters (0x4E9138 and 0x5077F9, whose purpose is not read), and
  `CHand::PrepareForDrawing` on the hand's own morph (not the creature's).
- **A script's fatness** 📼 1.20 only: `GGame::ProcessTurn` runs `Living::ProcessLiving` 0x54E65B (the creatures'
  `ProcessState` from 0x5EC8CF) before `GScript::Process` 0x54E693, and SET CREATURE_FATNESS (property 27, jump table
  0x70F2BC index 26 → 0x70EDBE) writes phys+0x14 only (0x70EDEA), where ALIGNMENT (index 17 → 0x70EF0C) also calls
  `Update3DCreatureFromAttributes` and `UpdateMorphing`. So a script's fatness first shows the next turn.

### The creature's radius

`Creature::GetRadius` 0x4792C0 (vt +0x60) and `Creature::Get2DRadius` 0x477F40 (vt +0x64) both return the float at
`LH3DCreature` +0x5228 (through `[[creature+0x160]+0x58]`), unscaled. `LH3DCreature::SetSize` 0x480530 is its only
writer:

- s = 0.05 if `!(size > 0.05)` (NaN too), else `min(size, 4)` (0x480563..0x480593); +0x90 = s;
- +0x94 = `(s × 15) / +0x8C`, the draw scale (0x4805A1..0x4805B1);
- +0x5228 = `+0x94 × +0x5224` (0x480642..0x48064C).

So **radius = ((ClampScale(size) × 15) / restHeight) × reach**, each step rounded to float. The size is the one SetSize
last got: in the temple's pen the pen size `ShrinkDownIfNearCitadel` passes it each turn (0x4EFE69), else the
creature's own. `LH3DCreature::GetNavRadius` 0x480A60 is the same at the unclamped size: `+0x6C / +0x90 × +0x5228`.
The creature's height (`Creature::GetHeight` 0x477F50) is the 3D body's +0x90 × 15, the drawn size, as `GetUserSize`
0x4EF4F0.

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

All of it runs at the game thread's 24-bit precision control
([audio.md](../engine/floating-point.md#the-games-fpu-runs-at-24-bits); `GetSetAnim` sets it again at 0x6196C7). The row normalisation
is not exact: `InverseSquareRoot(1)` is 0x3F7FFFA0 (1 − 5.7·10⁻⁶), the same just above 1, and just under 1 it is about
1 − 2.3·10⁻⁵, so each row comes out a little short and the shortfall compounds down each chain. That makes the reach
about 5·10⁻⁵ smaller than an exactly normalised pose. The same `fn_00860E00` poses the drawn creature
(`LH3DCreature::UpdateBuffers` 0x4EC6C3..) and the hand.

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
64-bit ulp, nor when they are rounded to double. **🔍 inferred** that nothing between `SelectMesh(0)` and the reach
(`GetSetAnim(0, 1..5, 0)`, `SetAnimTime(0x10..0x1B, 250)`) touches mesh 0's stand, and that no other store sets the
morph weights before it. Not checked against a memory read of the running game.

#### The nav radius and the route plan

**`LH3DCreature::GetNavRadius` 0x480A60.** With an owner (+0x4834) whose +0x12A8 is 0, `([owner+0x160]+0x6C / +0x90)
× +0x5228`, left to right (fld, fdiv, fmul), each step a float; else +0x5228. +0x6C is `CreaturePhysical`'s user size
(after spells and growth, unclamped), +0x90 the size `SetSize` last got (0.05..4, the pen's in the pen). So the nav
radius is the radius at the real size: in the pen (real / pen) × r(pen), above 4 (s / 4) × r(4), below 0.05
(s / 0.05) × r(0.05), and inside 0.05..4 out of the pen exactly +0x5228. +0x12A8 is set by 0x47D9A0 while the creature
has given up a route and is sent to pick up or kick what is in its way
([The walk back into the kept area](leash-rope.md#the-walk-back-into-the-kept-area)).

**The holder's margin is the nav radius.** `RPFollow::Reset(pos, r)` fn_00863B00 frees the plans and the route, calls
`RPHolder::Empty`, and sets +0x6404C and the margin +0x50020 to r, the position and plan start, state 1. All four
callers pass `GetNavRadius()`: the move 0x483FE0 when the 3D state +0x4994 != 1, after the stray check (0x48407E); the
route update when the follower gave up (0x48D33F); `ResetLook` (0x483219); `GetBodyActionFraction` (0x484D7D).

**`Object::GetRoutePlanRadius(creature)` 0x6384C0** (vt +0x7C4; Tree 0x74A140 and CitadelHeart 0x4680C0 override it and
drop the creature). With a creature, 0x6384D8:

```
hT   = this->GetHeight()
low  = creature->GetHeight() * 0.8f                 [0x3F4CCCCD]
keep = (hT > low) ? 0.4f                            [0x3ECCCCCD]
                  : 0.7f - (hT / low) * 0.3f        fdiv, fmul [0x3E99999A], fsubr [0x3F333333]
return this->Get2DRadius() - creature->GetCreature3D()->GetNavRadius() * keep
```

The test is `fcomp` + `test ah,0x41` + `jne`: the curve when hT < low, hT == low or unordered (a NaN height). The
creature's height is +0x90 × 15 (`Creature::GetHeight` 0x477F50).

**Who reads it.** Four `call [reg+0x7C4]` sites, no direct call:
- `Object::SimpleAddToRoutePlan` 0x638B00 (0x638B3A), the circle of the MobileObject 0x607410, MobileStatic 0x608AC0
  (fences go to `Object::AddToRoutePlan`), ScriptHighlight 0x70AE40 and Tree 0x74A180 overrides of vt +0x7C0. r =
  (GetRoutePlanRadius(creature) + margin) + extra, extra 5.0 when `[obj+0x44]` (its fire) and the creature are both
  non-null, stored to float; r = 0.05 when r < 0.05 (`test ah,1`: NaN too).
- the creature's walk up to an object, 0x483F40 (0x483F9E): `GetRoutePlanRadius(creature) + GetNavRadius()`, then the
  move 0x483FE0 with R + a, R + b; reached from 0x47A7A0 (`SubStatePerformAddVillagersToDance`, ObserveObject's two
  stages, FollowInterestingObject).
- EnterCitadel's second stage 0x4B3C80 (0x4B3EF1), on the citadel heart, whose override drops the creature.
- ObserveObject's second stage 0x5057B0 (0x50580E), on the plan's target.

`Object::AddToRoutePlan` 0x638790, the footprint version (Abode, Feature, fences, ...), has its own inlined copy of
the curve (0x6387CD..0x63881C): with a creature its margin is `margin - keep × margin`, and its extra is 5.0 when the
thing has a fire and there is a creature. The creatures reach vt +0x7C0 through `CheckSquareFunction` 0x54AFB0
(0x54B147: the follower's creature, or 0 with no follower), `AddSpecialRPObjects` 0x54AF60 (0x54AF9F),
`Fixed::InsertMapObject` / `MultiMapFixed::InsertMapObject` (0x52E5A7, 0x52E791: a thing put on the map while
creatures plan) and the route update 0x48D48D (each other creature, whose own `AddToRoutePlan` 0x47D500 is
(2D + margin) × 1.15). The footpaths (0x535BBA) pass 0. So in a creature's plan a Simple thing's circle is
2D + (1 - keep) × Nav (+5 with a fire), a footprint thing's margin is (1 - keep) × Nav.

### Sources

- The v1.42 `runblack.exe` through the xref index (the functions above); the game's data (`Data/CTR/*.CBN`,
  `Data/CreatureMesh`, `Scripts\info.dat`) for the counts, the start scale and the radius table.
- The radius specification and its draft for this page (the creature studies' `RADIUS_SPEC.md` and `RADIUS_WIKI.md`),
  merged here.

## openblack

Port decisions for the creature, row by row: [Port decisions: creature](../project/port-decisions-creature.md).

### Meshes, morph, layers and skin

- **Meshes** (`src/3D/CreatureBody`): a creature mesh's file name gives the species, the appearance (base, evil, good,
  thin, fat, weak, strong) and the Ogre variants. The mesh folder `Data/CreatureMesh` also holds files whose last name
  part is not an appearance (`A_Tiger2_Base - Realistic Face.l3d`, `Eyeball.l3d`, `Eyelid.l3d`); those are now
  skipped, while an unknown species still names a mesh and logs an error. **This is the one change to running code:**
  before, `Eyeball` or `Eyelid` (whichever loaded first) took the id `creature/0/0`, the Unknown species' base mesh,
  so a creature of type 0 (`CREATURE_TYPE` 0, only the Playgrounds use it) drew an eyeball and now has no mesh. Nothing
  looks these meshes up by name and no creature of type 0 is made on Land 1 or Land 2, so no verification check
  changed. Our rule is a port decision, not a reading (see [Pending](overview.md#pending)).
- **Morph** (`CreatureMorph`): the original's three axes, threshold and blend.
- **Seams (vertex blends)** (`src/3D/VertexBlend`, `L3DSubMesh::GetBlendSource`, `vs_object` with `USE_MORPH`):
  openblack blends in world space in the vertex shader, which is the same affine image as the original's camera-space
  blend; the partner is morphed like the body and placed by its own bone. openblack blends only the creature's body.
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
- **The default tattoo symbols**: his `Game.cpp:1738` reads the defaults from `I_PLAYER_SYMBOLS_.raw`; openblack's
  skin-art loader (`CreatureSkinArtLoader`, loaded at start-up under `creature_skin::k_ArtId`) reads them from
  `OriginalChooseSymbol.raw`, the original's file, as his code reads its file: RGB, the level of a texel is its blue
  >> 4, 64×64 cells, 4 to a row. The players' symbols (`PlayersSymbols.raw`) are optional; a cell left blank there
  takes the default.
- **Hair and footprints** (`CreatureHair`, `CreatureFootprints`): tufts of strands as spring chains whose look moves
  from neutral towards evil or good; a print under the lower foot at each footstep, fading over about five seconds.
  As the original, nothing takes the prints away: `FootprintSystem::Reset` (from `creature_loop::OnLoadMap`,
  `Game::PrepareNewLand`) only reads the date for the new land (was: it emptied the trail).

### Growth, age and the size

- `creature_physiology::TickTurn`, `Grow`: the turn counter `Needs::turns` goes up at every phase, the age only from
  phase 1; from phase 3 the creature grows whether `Turn::moving` or not, and `Grow` keeps the size within 0 and 2.
  With more than 1000 ms a turn, where the original's `div` faults, ours never ages. The locked-select skip is not
  ported (see [Pending](#pending), "The body's turn while the hand holds the creature").
- **Carrying**: `ecs::creature_physics::Weight` (the same mass as a strike's) is read by the carry share,
  `physics::living::CarriedShare`, which `CreatureNeeds::carriedWeight` holds; the physics' `WeightOf` stays the
  body's `creature_physics::k_Mass` ([physics.md](../physics/physics.md)). The creature tables' weight (82.5 for every species in
  `info.dat`) is not read. The testbed scenario `growth.carrying_strength` shows it.
- **The size when made**: `CreatureArchetype::Create` keeps the size it is given in `Creature::size`;
  `creature_morph::ApplySize` gives the body that size (the walk going by it as given, drawn within 0.05..4), and the
  `Transform` takes the drawn scale. `CreatureArchetype::StartScale` is the species' row as it is. The testbed scenario
  `growth.oversized_brought_back` shows a young and a grown creature made at 6.

### The size spells

`creature_spells::Receive`, `Step`, `StartDelayTurns`, `SizeTarget`; `ecs::components::CreatureSizeLimits`:

- every spell waits the 2 s (`k_StartDelaySeconds`) before it starts, a queued one too, counted in whole turns a second
  (`WholeTurnsPerSecond`, as are the hold, start and finish times); a waiting spell is let in at the slot that wears
  off, inside the turn's slot loop;
- big and small aim for the creature's own largest and smallest size, the defaults 2.4 and 0.2 when it has none
  (`SizeLimits`), never shrinking it (big) or making it bigger (small). The script's CREATURE_MIN_SIZE and
  CREATURE_MAX_SIZE get and set them (`player_creature::GetCreatureProperty`, `SetCreatureProperty`; a set gives the
  creature the component);
- the freeze: `creature_spells::Apply` gives up what the creature was doing at the start (`SideEffects::abandon`,
  `Abandon::Always`, run through `CreatureMindSystemInterface::AbandonAction`), pauses the mind
  (`CreatureMindState::paused`) at the start and starts it again at the finish whatever it was, stops the walk, and
  eases the frozen look and the playback speed;
- a miracle that goes: done as the miracle closes down (`creature_spells::FinishEarly`), not at the next turn's start;
  nothing reads the slot in between 📼 1.20 only;
- the start sound: `SfxBank::Spells`, distance 0, as the original;
- the joint-activity partner and its targets: not ported (see [Pending](#pending)).

### The fatness and the drawn shape

- **The drawn shape** (`creature_morph::RefreshDrawn`, `CreatureAnimationSystem::PoseTurn`): the same rule and the
  same order of the axes, taken once a turn at the start of the turn's pose, after the shown fatness has taken its
  step; the frames draw the shape as it is. Not ported: the other callers' redraw at once (a script's ALIGNMENT,
  `SwapMinds`, the two attribute setters): with them the shape waits for the next turn.
- **The shown fatness** (`creature_morph::EaseFatness`, `CreaturePhysiologySystemInterface::ProcessShownFatness`):
  `CreatureMorph::shownFatness` takes its step last in the creature turn (`ecs::creature_loop::ProcessTurn`), after
  the minds, the actions, the fights, the spells and the autoscale, so a change to the fatness made by these in a turn
  (the time-passing burn, a meal, a spell) is followed the same turn, as in the original. A script's change is
  followed the next turn, as in the original: `Game.cpp`'s creature loop, ending with `ProcessShownFatness`, runs
  before the scripts, and `ScriptEnums.h` numbers the two properties the same. The shown fatness is kept within 0 and
  1 even when the fatness is not (a script may set the fatness to any value). The alignment and strength targets, and
  the locked-select skip, are not yet as the original (see [Pending](#pending)).

### The radius in openblack

- `creature_morph::ClampScale`, `RestHeight` and `DrawnScale` (`src/Creature/CreatureMorph`) are the clamp, the rest
  height and the draw scale; `creature_morph::Reach` is the reach's maximum, and `creature_morph::Radius` is the
  radius in SetSize's order.
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
  (not made by the archetype) keeps the generic mesh-box formula. The height takes `creature_pose::DrawnSize`, the
  same size as `GetScale` ([The player's creature](players-creature.md#the-players-creature)).
- The rest height over `L3DMesh::GetBoneMatrices`, whose parents are put on with glm (not the original's order for the
  translation's sum), has the original's bits for every species, and an emulation of `BoneReach`'s float operations
  with the same C runtime's sine, cosine and arctangent (in double) gives the table's reach bits for every species.
  `test_creature_rigs_data` pins all three columns to the bit with the game's data, through
  `CreatureArchetype::BodyMetrics` over the base meshes loaded as the game loads them, and checks that every species'
  stand keys every bone of its base mesh; the pure parts are tested with made-up bones.

### The nav radius and the route plan in openblack

- `object::GetNavRadius(creature)` (`src/ECS/ObjectMetrics.cpp`) is `(Creature::size /
  creature_morph::ClampScale(creature_pose::DrawnSize)) × Get2DRadius`, in that order; anything that is not a
  creature gets `Get2DRadius`. openblack has no state for the given-up route flag, so the first branch is always
  taken.
- `creature_cast_moves::RoutePlanKeep(height, creatureHeight)` is the curve, the division first and NaN on the curve;
  `creature_cast_moves::RoutePlanRadius` is `radius - creatureRadius × keep`. `object::GetRoutePlanRadius(object,
  creature)` gives a tree's and the temple's (the citadel heart) as with no creature, and anything else
  `RoutePlanRadius(Get2DRadius, GetHeight, GetHeight(creature), GetNavRadius(creature))`; with no creature (or one
  that is gone) it is `GetRoutePlanRadius(object)`, bit for bit. `object_measures::RoutePlanRadius` is that function.
- `route_plan_world::AddToRoutePlan(object, holder, notify, creature, ...)`: `CheckSquareFunction` passes its plan's
  creature, the footpaths `entt::null`. `SimpleAddToRoutePlan` takes the creature's route plan radius, 5.0 more for a
  thing with a fire (`fire::Find`) in a creature's plan, and 0.05 for anything below 0.05 or not a number.
  `ObjectAddToRoutePlan` takes the curve on the margin and the same 5.0. The animated statics' and the citadel heart's
  take no notice of the creature.
- The creature branch of `GetRoutePlanRadius` and `NavRadius` are ported (the object-size notes that once listed
  them as open now point here).
- `CreatureLocomotionSystem::StartMove` sets the follower's margin to `GetNavRadius` where it makes a follower or
  restarts an idle one, as the move's Reset. A follower that gave up is made again at the next move.

### Test hooks

- The debug window's body panel shows the size, the age and this turn's growth, and has a "Grow now" step
  (`CreaturePhysiologySystemInterface::GrowthOf`, `GrowNow`).
- The same panel reads out both size spell targets and casts big or small on the creature.
- The debug window's selected creature shows the fatness and the fatness shown, with sliders for the alignment, the
  fatness and the strength.
- Testbed scenarios: `growth.carrying_strength`, `growth.oversized_brought_back`.

## Pending

- **The body's turn while the hand holds the creature** ([Growth, size and age as time
  passes](#growth-size-and-age-as-time-passes)): while Flags +0x24 & 0x10 is set (packets 0x1B / 0x1C) the original
  skips 0x472F77 → 0x4737DB: the autoscale, the turn counter, `UpdateAttributesAsTimePasses`, `UpdateLeash`, the
  home rule, the keep-alive and `Update3DCreatureFromAttributes`; the stand-in is the hand's own locked flag, which the
  hand must publish. Queued as a fix.
- **The size spells with a joint-activity partner** ([The size spells](#the-size-spells-the-start-delay-and-the-targets)):
  openblack has no partner (mind +0x1BE8), so neither it nor its targets are ported (a port row).
- **The other redraws of the shape**: a script's ALIGNMENT, `SwapMinds` and the two attribute setters (0x4E9138,
  0x5077F9) redraw at once in the original; ours waits for the next turn. Queued as a fix with the alignment and
  strength targets ([the creature's Pending](overview.md#pending)).
- **Vertex blends** ([The body](#the-body)): which objects reach `fn_00810720` (vt+0x15C) with a non-zero first
  argument, and so are drawn without blends, is not read; where a GPU lacks vertex texture fetch of RGBA32F the seams
  are not blended. The sea plane cut `fn_00858BA0` (from fn_0080C050, fn_00811C70, fn_00825530) does not call the
  blend loop; its callers for a creature are not traced, and openblack's creature draws all take the blend.
- **The creature's radius** ([The creature's radius](#the-creatures-radius)):
  - the locomotion's own radius should be `GetNavRadius` 0x480A60, `MoveToObject`'s ring 0x47A7A0 / 0x483F40 /
    0x483FE0's, and `AddSpecialRPObjects` 0x54AF60 with the live adds of `Fixed` / `MultiMapFixed::InsertMapObject`
    (0x52E5A7, 0x52E791) ported, with the timing of +0x5228: queued as fixes (three);
  - +0x12A8's `GetNavRadius` branch and its writer 0x47D9A0 wait on the route's blocked state; the citadel heart's
    worship-site circles wait on the worship sites;
  - the bits of +0x8C / +0x5224 / +0x5228: only a memory read of a running original confirms them;
  - the drawn bones are normalised by `InverseSquareRoot` (`fn_00860E00`), ours by the exact length (`SampleCycle`),
    so our drawn bodies differ from the original's by up to about 2·10⁻⁵ per bone level (not measured on screen):
    queued as a fix;
  - `BoneReach`'s default frame (+0x47FC, argument 8): unreachable on the shipped data.
- **Playground creatures** (`CREATE_CREATURE_FROM_FILE`, playgrounds and `comp.txt` only): they now start at their
  species' `startScale` and start body (not 0.3), their `Transform` scale is the drawn scale, and the script's type 0
  is taken as the Giant Ape. The original draws a creature at size × 15 / rest height
  ([The creature's radius](#the-creatures-radius), +0x8C). Its owner is read as the original reads it (the feature
  script's command 0x716F5C, `GPlayer::GetPlayerFromText` 0x64B5E0): the player names compared ignoring case, an
  unknown name taken as the neutral player. So "Neutral" (The Ultimate Sandbox Map, lines 1004-1005) is the neutral
  player, as "NEUTRAL" is (`FeatureScriptCommands::PlayerFromText`).
