# The leash's rope

## Summary

The leash's rope runs from the creature's collar to the hand (or to what it is tied to): 40 masses on springs,
stepped at 200 Hz every frame, drawn as a lit ribbon with a shadow on the land. Its lengths follow the creature's drawn
size in the hand and the thing's size or distance when tied. On the game turn the rope is never read: the creature is
kept within the leash's full length of the hand or the tied thing, and walks back when it strays. The tug that would
pull it is in the code but nothing sends it.

openblack's rope (`Creature/LeashRope`) is bit for bit the original's model; the collar is the drawn neck, the lengths,
the two leashed creatures' attitudes, the young creature's home and the walk back are ported, and the draw is the
original's one strip in one material. Left: the rope's seed, a felled tree's untie, the fighter's walk back and some
of the draw's gate. The leash's rules and the timed desire are in [The leash](leash.md#leash-and-the-hand).

## The original

### The leash's collar end

- **The original takes it from the drawn 3D creature.** `GInterface::UpdateAllLeashes` 0x5D9130 (every frame, from
  `PostDrawProcess`), for each interface status whose leash is on, gets the 3D creature (`Creature::GetCreature3D`
  0x477850) and reads its collar from `TransformedMatrices` (the pointer at Morphable +0x47F0) at the leash bone's
  index +0x51D4 (0x30-byte LHMatrix, translation at +0x24; 0x5D91A1..0x5D91CC), then `GetLeashStartPoint` 0x5D8EA0 for
  the other end, then steps the rope `fn_00848970(GInterface+0x198, collar, start, dt)` (the collar at the rope's +0
  end, [Leash and the hand](leash.md#leash-and-the-hand)) (the drag ropes +0x19C / +0x1A0 alike in interface state
  0x15). Laying or tying the rope (`GLeashStatus::SetOn` 0x5E6FC3..0x5E6FEE, `fn_005D4350` at 0x5D44B2) seeds it from
  +0x5178 `[+0x51D4]`, and `GetLeashStartPoint` takes a creature the leash is tied to from +0x5178 / +0x47F0
  `[+0x51D4]` (0x5D8ED8..0x5D8EE6). The same point serves the rope's simulation, its tension, its drawing
  (`fn_008491B0`) and the hand's pick of it (`fn_00848F90`, from `UpdateInterfaceCollide`).
- **Those bones carry the drawn size and place (confirmed).** `LH3DCreature::SetSize` 0x480530 writes the size
  clamped to 0.05..4 to +0x90 (0x480599) and size × 15 / [+0x8C] to +0x94 (0x4805A1..0x4805B1).
  `LH3DCreature::UpdateBuffers` 0x4EC590 (from `UpdateTime` 0x481DF0 at 0x482498 and 0x48257F) builds the 3D object's
  world matrix with `fn_004EBE10`: the yaw from +0x84 and a lean of at most ±0.3, every rotation cell × [+0x94]
  (0x4EC024..0x4EC08B), the translation the object's own position +0x78 / +0x7C / +0x80 (0x4EC08E..0x4EC0A9). The
  skeleton is posed onto that matrix into the buffer at +0x517C: `fn_00839F10` (misnamed `LH3DTech::GetCameraPosition`
  in the symbols) multiplies each bone by its parent's matrix, a root bone (parent -1) by the matrix passed
  (0x839F46..0x839F66); `LH3DAnim::SetTransform` 0x83A1D0 at 0x4ED0F0 when there is no animation. +0x517C is then
  swapped into +0x5178 (0x4ED106..0x4ED118; the frame before stays in +0x5174). `LH3DCreature::PrepareForDrawing`
  0x4ED320 writes `TransformedMatrices` as a weighted sum of +0x5178 and +0x5174 (`fn_004EECD0` 0x4EECD0 at 0x4ED450
  and 0x4ED46B, 0x4ED43A..0x4ED46B): f·+0x5178 + (1 − f)·+0x5174, f = min(the turn fraction, 2), the first pass
  setting and the second adding ([The body posed for the turn](pose-and-animation.md#the-body-posed-for-the-turn)); `UpdateAllLeashes`
  reads the collar from that buffer after the drawing (`PostDrawProcess` 0x5CEAB3). So in the pen, where
  `ShrinkDownIfNearCitadel` calls `SetSize` with 0.22 ([Drawn smaller in the pen](players-creature.md#the-players-creature)), the collar
  is the small body's neck, and while the creature walks it is the neck where the body is drawn.
- **The lengths follow the drawn size.** The rope's slack and full length are `GLeashStatus` +0x28 / +0x2C, copied to
  the rope (+0x65C / +0x660) by `SetLocalLeashFromInterfaceStatus` 0x5D9510. `LH3DCreature::SetSize` 0x480530 sets
  them (0x480652..0x4806A7, `fn_005E6980` 0x5E69A2) for each of the owner's interface statuses whose leash is not tied
  (+0x24 = 0): slack 0.7 × 15 × +0x90 + 22, full 3 × 15 × +0x90 + 32 (0x92B360..0x92B36C). Since the pen shrink calls
  `SetSize` every turn with the pen size, the pen does change them.
- **The tension.** `fn_00848970` writes it to rope+0x18: `clamp((|node − start| − slack/41) / (max/41 − slack/41),
  0, 1)`. **(inferred, static scan)** No game-turn code reads it: every load of `[reg+0x198]`, `[reg+0x19C]`,
  `[reg+0x1A0]` with the 9 instructions after it, the rope functions 0x848600..0x849FFF and `GetLeashStartPoint` were
  read; a read through a copied pointer would not be found. Its one other reader, 0x5D95E0, is called only by the tug's
  sender, which nothing calls ([The leash's tug](#the-leashs-tug)). The turn side, `Creature::UpdateLeash` 0x4CE320,
  keeps the creature to the hand's x, z (`GInterfaceStatus` +0xC8 / +0xD0, × 6553.6 at 0x4CE57B) at the leash's length
  `GLeashStatus` +0x2C (to Creature +0x11B4), never reading the rope
  ([The walk back into the kept area](#the-walk-back-into-the-kept-area)).

### The leash's hand end

- **Stepped from the drawn hand's wrist.** `UpdateAllLeashes` 0x5D9130 asks `GInterface::GetLeashStartPoint`
  0x5D8EA0 for the rope's other end with the flag 0 (`push 0`, call at 0x5D91DD) and steps the rope with it
  (`fn_00848970` at 0x5D91FA), once a frame after the drawing. Untied (the status's leash +0x24 = 0, 0x5D8EB3..0x5D8EB8)
  on the local interface (`GGame::MyInterface`, 0x5D907B..0x5D9088), the point is the translation (+0x24..+0x2C) of the
  first matrix of the render hand's `TransformedMatrices`: `GInterface` +0x3A0 (`hand`, the CHand, a `Morphable`),
  its +0x47F0, index 0 (0x5D90B9..0x5D90F3). That matrix is the hand model's bone 0 (`Hand_Boned_Base2.l3d`) as the
  hand is drawn that frame, in world space: the wrist, the bone the hand's casting bands also fly to
  ([Casting from the hand](../magic/hand-casting.md)). It is not the hand's position CHand +0x78, the index fingertip
  the hand is placed by, which only the drag ropes of interface state 0x15 take (0x5D9253).
- **Laid from the hand's point.** `GLeashStatus::SetOn` 0x5E6F70 lays the rope with the flag 1 (`push 1` at 0x5E6FB0,
  call at 0x5E6FBE): untied and local, that is `GInterfaceStatus` +0xC8..+0xD0 (0x5D9092..0x5D90B6), the hand's
  point the turn side keeps the creature to. On another player's interface the point is the status's +0xB0
  (0x5D90FD..0x5D911E) with either flag.
- **What reads it.** The rope's +0xC end and its masses; the tension (rope +0x18) is not read by the game turn
  ([The leash's collar end](#the-leashs-collar-end)), so the wrist moves only the drawn rope.

### The leash's rope

Read from runblack.exe (the functions below), with a float32 model of them (one rounding per operation: the game
thread's x87 runs at 24 bits, [Floating point](../engine/floating-point.md#the-x87-arithmetic)). The rope object is
`GInterface` +0x198 (the drag ropes +0x19C / +0x1A0): +0x00 start, +0x0C end, +0x18 tension, +0x1C the 40 masses of
0x28 bytes (+0x00 position, +0x0C velocity, +0x18 force, +0x24 stretch), +0x65C slack length, +0x660 full length,
+0x668 half width, +0x66C v0, +0x670 v1, +0x674 a byte, +0x678 the texture's scale along it, +0x67C the material.

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

### The tied lengths, two creatures leashed together, and the young creature's home

Read with the disassembler.

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
- **A felled tree.** In the original the dead tree is a new object and the tree is deleted; `fn_005E6B90` drops the
  tie when the tied object is not `IsAvailable` (vt +0x2C, 0x5E6B9C..0x5E6BBC, then `StartImmersion(7)`).
  fn_005E6B90 is `GLeashStatus::Validate`: its only caller is `GInterfaceStatus::Process` 0x5DC5EB, from
  `ProcessGameInputs` 0x54C40D → `GInterface::Process` 0x5CEC10 → fn_005D2250 0x5D262F → fn_005CEBB0, each input
  pass for the 18 statuses of every player. The forester's felling (`Villager::ForesterChopsTree` 0x75FAE2) makes a
  `FelledTree` and deletes the tree (`ToBeDeleted` down to `GameThing` 0x56FB70: +0xA |= 1, so `IsAvailable` 0x401810
  is 0), and the next pass unties the leash 📼 1.20 only.
- **The land number** the home rule tests is g_game +0x205A08, which `SET_LAND_NUMBER` writes
  ([Changing land](../scripts/land-change-and-saving.md)).
- **Two creatures leashed together.** `Creature::UpdateLeash` 0x4CE5D2..0x4CE6CF: the step is −0.1 (0xBDCCCCCD,
  0x4CE678) on leash type 1 (aggression), +0.1 (0x3DCCCCCD, 0x4CE5FE) on type 3 (compassion), 0 on type 2 (learning).
  With the tied object cast to a creature (`CastCreature` vt +0xA4), when the other's mind (+0x164) +0x18C3C is 0 or
  the turn (g_game +0x205A40) minus it is above 600 (unsigned, `jbe`), it is set to the turn and
  `UpdateHowNiceIsCreature` 0x4C4D50 is called on both minds with the step, 0 included. The stamp lives on the other
  creature's mind: every creature leashed to it shares it, and untying or tying again leaves it.
- **`UpdateHowNiceIsCreature`** 0x4C4D50(creature, step): the attitude found by `GetAttitudeToCreature` 0x4C4C80 (none:
  skipped) gets howNice + step clamped to −1..1 (0x4C4D6E..0x4C4DA0); then two lessons of ±0.5: a step below 0 teaches
  anger (2) +0.5 and befriending (16) −0.5, any other step (0 included) the reverse (0x4C4DB6..0x4C4EA8).
- **The young creature's home.** `Creature::ProcessState` 0x473042..0x4730D2, every turn after `UpdateLeash`: when
  `GetInterfaceStatusLeashOn` is null, the development phase +0x1268 is below 5 (signed), `GetPlayer` is set, the
  player's +0x8E0 is not 2, `GGame::IsMultiplayerGame` 0x552F80 is 0, the player is the local one (g_game +0x18 +
  0xA60 × the byte g_game +0x205A59) and the land number g_game +0x205A08 is 1, the kept point +0x11A8 = the home
  +0x1200 and the radius +0x11B4 = 10.0 (0x41200000). When it fails nothing is reset. `IsConfinedToArea` 0x47CCE0
  then holds while the radius is above 0, and, leashed, while the leash works (`IsLeashWorking` 0x4CF0A0). A creature
  always has a home (+0x1200; `Create` takes the heart's point 0, `ProcessState` point 15).
- **Grown to phase 5**: `MoveToDevelopmentPhase` 0x4C59C0, when the creature is confined (`IsConfinedToArea`),
  has no leash on and the new phase is 5 or more, sets the radius +0x11B4 to 0 before it stores the phase
  (0x4C59C6..0x4C59E3; in the v1.42 exe too, whose index lists `IsConfinedToArea` and `GetInterfaceStatusLeashOn`
  among the function's callees); only the radius is written, not the walk back. Two callers then clear it whatever,
  and neither loads a mind file: `Config::ProcessOneGameTurn` (0x46B462, +0x11B4 = 0 at 0x46B46D: makes the local player's creature at a fixed point if it has none, phase 13) and
  a packet that puts the local player's creature at its citadel's point +0xC8 (`GPacket::ProcessPacket` 0x63CDDB,
  +0x11B4 = 0 at 0x63CDE2, phase 13). The mind-file loads clear nothing more (read 2026-10-18): `LOAD_MY_CREATURE`
  (`GScript::LoadMyCreature` 0x6FD260 → fn_00552AC0 → `Creature::Load` 0x4E7FF0, then two `SetFizz`) keeps the
  file's phase (read into +0x1268 by fn_004E84A0, 0x4E89DF) and writes no radius; packet 0x4E's load (0x63DA4F) and
  the computer creatures' (fn_0065BA40, 0x65BBC1) move to phase 13, which clears the radius only by the rule above.
  A made creature's radius starts at 0 (`Creature::SetToZero` 0x47422D). openblack's `LOAD_MY_CREATURE` keeps the
  file's phase and leaves the radius alone, as the original does.
  `ProcessState` writes the 10 only below phase 5, so a creature that grew up is no longer kept at home.
- **`GetHeight`** is Creature vt +0x42C = 0x477F50, 3D +0x90 × 15: the drawn size. The walk's arrival
  (0x4C72ED..0x4C7332) is the larger of the radius and that height.
- **Player +0x8E0** is the player's kind: `TOGGLE_COMPUTER_PLAYER` sets 2 (a computer player, fn_0064C400
  0x64C418; off leaves it), `GPlayer::Init` 0x64919C its PLAYER_TYPE argument, `GPlayer::SetToZero` 0x64934B 0, and
  the end of a land (fn_006572C0 0x6572E2, from `GPlayer::OnEndOfClearMap`) turns a 2 back to 0; 0 is a player who is
  not playing (`GGame::GetNextActivePlayer` 0x5508D0). Details in
  [The player systems](../magic/influence-and-alignment.md#the-player-systems-list-of-player-entities).
- **Another kept area before it** (0x472FDF..0x47303C): with Creature +0x1294 and +0x1298 set, the kept point is the
  position of the object at +0x1298 and the radius min(`[+0x160]+8` as an unsigned integer, 150) + 50. `[+0x160]+8` is
  not identified. The only writers of +0x1294 / +0x1298 are `Initialise`'s zeros (0x474330 / 0x474336) and
  `ResolveLoad` 0x4E6FEB, a saved game's (an operand scan of the whole disassembly), so with no saved games it is
  never set.

### The walk back into the kept area

Read with the disassembler. One rule keeps a creature near the hand, near what its leash is tied to and, young, near
its home. Fields: Creature +0x11A8 the kept point (a `MapCoords`: x, z, altitude), +0x11B4 the radius, +0x11B8
walking back, +0x380 led, +0x12A8, +0x1294 / +0x1298; mind (+0x164) +0x1C10, +0x1C14 the forced plan, +0x1C38,
+0x1C78 where it walks.

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
  `GCreatureInfo::walkSpeed`), `LH3DCreature::EndAnyAnimsRapidly` 0x48F750, `fn_004C7220(point)`, +0x11B8 = 1, and
  +0x380 = 1 when a leash is on. Tied, before it, mind[+0xF60 × 4 + 0x1CEA0] = 0 (0x4731E4..0x4731FF).
- **`fn_004C7220`** (CreatureDirectControl.cpp): mind +0x1C78 = the point; the 3D creature's `fn_0048F710(0, 1)`
  (0x4C7289; 1 pushed, then 0). In `fn_0048F710` the second argument gates four state-ending calls (0x48F71B..0x48F730:
  `fn_004848F0` state 6 → 7, state 5 → +0x5238 = 1; `fn_004845F0` +0x528C = 0 and a camera check in states 22..24, 26,
  27, 29, 32..34; `fn_004848C0` state 3 → 4, state 2 → +0x5238 = 1; `fn_00484AE0` state 8 → 9) and the first gates
  `LH3DCreature::StopMoving` 0x484260 (0x48F739..0x48F73F), so the walk back's own call does not stop the creature;
  `ForceActivityAndForceAction` 0x4C4450 with desire 0x18 (ObeyPlayer) and the belief about itself; mind +0x1C14 = 1;
  the sub-action agenda's `fn_004FF5C0`; +0x11B8 = 1; and a move sub-action whose `SubArgumentPointAndFloat` holds the
  point and max(`GetHeight`, radius) (0x4C72ED..0x4C731E: the radius when the height is below it).
- **The end of the walk.** +0x11B8 and +0x380 are cleared by `Creature::StopWhatIAmDoing` (0x475C4A, 0x475BD2) and at
  creation (`fn_00474130`); `SetOn(off)` clears +0x380 and the radius, not +0x11B8, so a walk back under way goes on.
- **A point it can't stand on.** The move is sub-action 8, `MoveToPos` (named and filled in by the sub-action table's
  initialiser, 0x4F9267..0x4F9373: step 0 is 0x5018A0, then 0x501950, which waits on `LH3DCreature::IsMoving`). Step 0
  calls 0x47A880, whose move 0x483FE0 first tests the point with `fn_00483850` (`fn_00483890(point, 7.1)`, the
  creature's radius: the immediate 0x40E33333 at 0x483854, also at 0x54B07A `CheckSquareFunction` and 0x46AC23, not
  scaled by size). Refused, it returns 0 and step 0 calls `FinishActionUnsuccessfully("Stopping because navigation
  failed")`: mind +0x1C14 = 0, then `FinishActionGeneric`'s `StopWhatIAmDoing` (+0x11B8, +0x380 and mind +0x1C14 to
  0, the plan's desire +0xF50 to 0x28). The failure is 0x47A880 returning 0 or 2 (tested at 0x4FFAF9 and 0x4FFC2D),
  and the call is `FinishActionUnsuccessfully("Stopping because navigation failed", 1, 1)` (0x4FFC9C..0x4FFCA7): the
  two 1s pass through `FinishActionGeneric` to `StopWhatIAmDoing`, whose `fn_0048F710(1, 1)` (0x475B76) runs both the
  state-ending calls and `StopMoving`. That is what stops the creature on a refused walk. Nothing picks another point
  or tries again within the walk. The sub-action runs in `ProcessSubAction` (0x4737B0), after the mind's choice
  (`fn_004F0370`, 0x47369D), which chooses nothing while the plan has a sub-action left (agenda +0xFB4 below +0xFBC,
  step +0xFB8 below 3). So every turn the test sends it again, its mind stays out and the move fails: the creature
  stands where it is for as long as the point stays where it can't stand.
- **`EndAnyAnimsRapidly`** sets the 3D creature's +0x5730 for its animation states (+0x4994) 2..12, 14, 16, 22..25
  and 37 (the byte table at 0x48F780); `UpdateTime` then doubles the animation step +0x48BC (0x481E6C) and
  `StateSet` 0x484EC0 clears it (0x484EFC). So the action it was playing runs at twice its speed until its next state;
  one layer halves it back (0x4824CA..0x4824DF, rounding toward 0).
- **What the fields are.** +0x12A8: set by 0x47D9A0 (called at 0x48D39D, the 3D creature's route update, when the
  route follower's state +0x64054 is 6) with a forced ObeyPlayer plan to pick up or kick one of up to three things in
  its way; cleared by `StopWhatIAmDoing`. +0x380 = 2: set by `GPacket::ProcessPacket` after the tie (fn_005E6BD0, then
  `ForceActOnObject` on the tied thing, 0x63D49C..0x63D50B) and after the leash sends it to act on a thing or a point
  (0x63D520..0x63D5B9): doing what the leash sent it to do. Mind +0x1C10: set by `MoveByTeleport` 0x47A18F and
  `SetupReactToTeleport` 0x4F3AFE. Mind +0x1C38: written by `ForceActOnObject` (0) and `ForceActOnMapCoord`.
  +0x1294 / +0x1298: a creature made by 0x474BA0 for an object (+0x1298), kept within min([+0x160]+8, 150) + 50 of
  it (0x472FDF..0x47303C).
- **A fighter** 📼 1.20 only: `Living::ProcessLiving` calls `ProcessState` (vt+0x620) for every living with no test
  (0x5EC8A8..0x5EC8D9); the only early exit before the walk back is the locked select (0x472F77), and the walk back's
  gate (read whole) has no fight or faint test. Its start ends a duel: fn_004C7220's fn_0048F710 (0x4C7289) with the
  second argument 1 runs the state-ending calls.

### The leash's tug

Read in the 1.20 exe (with a scan of every `E8` / `E9` rel32 in `.text` and every dword of the image).

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
  onto an object). The pull, `LH3DCreature` +0x48B0, is written only by `Init` (0), the fade in `UpdateTime`,
  `SetOn(off)` (0) and 0x47FBF0 (`SetPull`: below 0.2 it stores 0), which only 0x4CE6E0, 0x4CECF0 and the tug
  (0x4CEBC0) call. So the tug never happens and the pull stays 0: a creature in the hand moves only by
  [the walk back](#the-walk-back-into-the-kept-area). The method's taut test is on its argument, not on the tension
  it reads. A scan of every call into the `SetPacket` overloads (0x550900..0x551800, the type pushed or moved in the
  six instructions before) finds type 0x57 sent only at those two places, and no hand drag of the rope sends it
  (`HandStateTug` 0x5B7DF0 / 0x5B8070 is the tree uproot,
  [Tug](../hand/grab-hold-and-throw.md#tug)).
- **The leash drag is set by no running path** (checked with the exe's cross-reference index). `AnalyseLeashForcePacket`
  0x47FBF0 (bw1-decomp's name for the pull setter above, which writes +0x48B0) has 5 calls: 0x4CE70E, 0x4CE72E and
  0x4CE745 in the function the index starts at 0x4CE6DB (the pull setter 0x4CE6E0 above, and the packet's sender
  0x4CE750 after it, are inside that range), 0x4CED16 in 0x4CECF0, and 0x4CEBC0 in the tug fn_004CE8E0. 0x4CE6DB,
  0x4CE6E0, 0x4CECF0 and 0x4CE750 each have 0 calls or jumps and 0 direct references (no vtable entry and no stored
  pointer in the index). fn_004CE8E0's one caller is `GPacket::ProcessPacket` at 0x63CF0B (packet 0x57), whose only
  sender is 0x4CE750. So +0x48B0 stays 0, `UpdateTime`'s pull branch (above 0.05) never runs, and with it neither its
  `FadeLeashDrag` nor its call of `ApplyForce`. The limit: a pointer built in a register from an offset shows in the
  index only as a reference to that offset, and none was found for these addresses.
- **The packet** (`GPacket::ProcessPacket` 0x63C420: the type byte +1 minus 6 indexes the jump table; type 0x57 is case
  0x51 at 0x63CEE4): the object from the packet (0x63DF70), the player +0x14, that player's creature (+0xA4C), then
  0x4CE8E0(object). The float is not read.
- **0x4CE8E0(object)**, in order: nothing when +0x12A8, +0x11B8 (walking back), or the creature's Flags bit 0x400 (held
  by a script) unless [0xC64150] (only `Creature::OnClearMap` writes it, 0); +0x10B4 = 1; nothing when mind +0x1C00
  (only 0 is written to it, by `Creature::Initialise` and `StopWhatIAmDoing`); an object not `IsInteractable` (vtable
  +0x190) is dropped; +0x380 = 0: 0x4CEC00 (pulled away, below); no player or no leash on: nothing more; +0x380 set
  with a valid plan (`CreaturePlan::IsValid`, mind +0xF48) whose belief's object (mind +0xF58, +0x30) is not itself:
  tied, nothing more; the 3D body's +0x78 within 10 of the hand (+0xC8, 3D) (`fcomp`, C0), nothing more;
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
  0.01 at +0x28 (the action's priority; its own priority +0x2C stays 0), made current with
  `ConstructCurrentPlanFromPlan(plan, 1)` 0x4F1560.
- **Who clears the forced plan** (mind +0x1C14; every write found by scanning `.text` for the offset): to 0 by
  `StopWhatIAmDoing` (0x475C1F), `FinishActionUnsuccessfully` (0x475773, before its `FinishActionGeneric`, which calls
  `StopWhatIAmDoing`), the mind's choice `fn_004F0370` (0x4F03C3, once the plan has no sub-action left);
  `fn_0047CFB0` (0x47D126), `fn_0047D950` (0x47DA59), `ReduceLife` (0x47DDCB) and `GScript::DevFunction` also write
  it from a register, not followed. `StopWhatIAmDoing` also sets the plan's desire (+0xF50) to 0x28 (0x475A91). So
  when the leash's walk ends, by arriving or by failing, the plan left has no desire and a tug counts nothing for it.
- **0x4C7220 (sent walking)**, the walk back's too: `ForceActivityAndForceAction` 0x4C4450 with (0x18, no belief,
  action 1 (walk to a point), the belief about itself, none, 1, 0) (0x4C7294..0x4C72A7); arriving within
  max(`GetHeight`, +0x11B4) (0x4C72ED..0x4C731E).
- **The fade.** `LH3DCreature::UpdateTime` 0x481DF0 (vtable +0x14; also in `CreatureFalling`'s and
  `CreatureCitadel`'s vtables) is called once a creature turn, from `Creature::ProcessState` at 0x473939 with
  `ftol([+0x160]+0x48 × ms per turn)`, when [0xD00DE4] and mind +0x20CE8 are set (0x4737B5..0x4737F3). At
  0x482062..0x4821E8, while the pull is above 0.05 (C0 | C3 clear): the body force below, then pull × 0.95 stored, and
  0 when that is below 0.3. Led or not.
- **The body force.** In the same block: towards the leashed creature's interface status point +0xC8 (or its own
  +0x78 with no player), mass (`GetMass` 0x47FA80) × pull × 15 along the normalised direction; that force goes to
  0x4813B0, whose result goes to 0x47FAC0 (which divides by the mass and writes +0x48AC); −0.9 × the force is kept in
  locals. 0x47FAC0 is `LH3DCreature::ApplyForce`, which a strike also calls
  ([Struck by a thrown thing](hits-and-catch.md#struck-by-a-thrown-thing)); with the pull always 0 this caller never runs.

### Drawing the leash

The original draws the worn leashes in one stage after the models (`GInterface::DrawAllLeashes` 0x5D9310, stage 6 of
[the frame's draw stages](../rendering/frame.md#draw-stages-in-order)), on the land path only (not in the citadel), in
`leash.raw` / `leasha.raw`, the rope in material mode 15. `fn_008491B0` builds the strip on the land at H + 0.1
([0x8AB22C], [Lighting the models](../rendering/model-lighting.md)). `fn_008491B0` has no caller outside `DrawAllLeashes`
(0x5D943E / 0x5D948C / 0x5D9497): no leash is drawn in the sea's reflection.

#### The rope's light

Read in the 1.20 exe; the helpers fn_00848CC0 / fn_00848D20 are not matched in bw1-decomp. Each of the rope's 42
points, i = -1 (the start) to 40 (the end), gets one colour for both of its corners: `fn_00848CC0` takes the point
(`fn_00848D20`: the start +0 for i < 0, the end +0xC for i >= 40, else node +0x1C + 0x28 i) through `fn_00801C90` and
keeps only its second output, the land light table's entry for the brightness of the four cells around the point,
bilinear (the models' base colour). For i = -1 and i = 40 the colour is `& 0xFFFFFF` (0x848CE6..0x848CFD): **alpha 0
at both end points**; inside, the table's entry as it is, alpha included. There is no haze and no land colour:
`Draw3DWorldTriangle` copies the diffuse as it is and writes the specular 0 (0x81C2BA..0x81C2BF), calls no haze
routine, and D3D fog is never on ([the spells page](../magic/spell-core.md)). The first loop of `fn_008491B0` (0x8491E1..0x84925A) also builds a
colour per node (blue / 4, alpha (1 - min(stretch, 0.7)) x 255) into a stack array that nothing reads: dead code.

#### The draw call

`fn_008491B0` draws each leash with **one** `Draw3DWorldTriangle` 0x81C090 call (0x849997): 168 vertices,
164 triangles, the leash's material (+0x67C), the world-to-clip matrix as it is. That call transforms on the CPU and
rasterises at once (`LH3DRender::DrawTriangle` 0x82F810, 0x81C5B1): no Z object, not sorted. Around it the mode table
is switched to the global alpha one, 0xC387C8 (0x84996E), and back to 0xC38728 (0x84999F).

- **The material** (the leash's init `fn_00848600`, 0x848777..0x8487A7): `CreateMaterial(15, leash.raw)`, ALPHAREF
  (+4) **0x38**, flags (+5) |= 4 (tiling) | 1 (two-sided). Mode 15 (0x82E470): SRCALPHA / INVSRCALPHA, alpha test on,
  Z write on, colour and alpha texture x diffuse; ZFUNC the frame's LESSEQUAL. Through the global alpha table mode 15
  stays 15, and its ALPHAREF becomes `max(0, ftol(0x38 x A / 255 - 5))` (0x82E557..0x82E5C9,
  [Lighting the models](../rendering/model-lighting.md)), A the alpha of the last object drawn's diffuse: 51 for A = 255.
  `Draw3DWorldTriangle` takes CULL from flag bit 0 (CULL NONE here, 0x81C30C, 0x81C556..0x81C58F), does no back-face
  discard of its own, and wraps from bit 2 (0x81C4B0 / 0x81C551).
- **The shadow is in the same draw and the same material**, so it is alpha tested and writes depth like the rope. Its
  corners are 0x41000000, 0 at the two end points (0x8498EA..0x84991B).
- **The order.** Per point, two rope corners, then two shadow corners; per segment the indices `5,4,0, 0,1,5, 7,6,2,
  2,3,7` (+4 a segment, 0x849840..0x8498D4): the rope's quad, then the shadow's, segment by segment along the rope.
- **UV** (0x849913..0x849957): u = travelled x [+0x678] 2.5 x 0.05 on all four corners, v = [+0x670] 0.25 on corners 0
  and 2, [+0x66C] 0.125 on 1 and 3; half width [+0x668] 0.15; the shadow at `GetAltitude` + 0.1 (0x8497E8, 0x849818).
- **The data.** `leasha.raw`'s rows 40..55 (v 0.156..0.219) are 255, rows 32..39 and 56..63 are 0, row 40 about 192.
  With ALPHAREF 51 the rope's inside passes, the shadow passes (0x41 x 255 / 255 = 65), and the two end segments are cut
  where the alpha, faded to 0 at the end points, falls below 51.

#### The ribbon's geometry

`fn_008491B0` 0x849318..0x8496FD 📼 1.20 only: the tangent t = P(i+1) − P(i−1) normalised, (0, 0, 1) when |t|² is below 1e-6
([0x9A2BAC]); v = P − eye normalised, left as it is when zero; the side = normalise(cross(t̂, v̂)) × [+0x668], left
zero when exactly zero; corner 0 = P + side; the shadow's side = normalise(−t̂z, 0, t̂x), a zero kept, at `GetAltitude`
+ 0.1. The light table's entry keeps its argument's alpha byte (fn_00869790, 0x869836), the flash pass ORs 0xFF000000
(0x869CA5). A ([0xC37D8C]) is the current diffuse, rewritten by some 40 mesh draws (fn_0080C050 0x80C105 and others);
before `DrawAllLeashes` 0x54DF18 come `GLandscape::Draw` and the local creature's `DrawFightSparkles`, so its value is
the frame's content.

#### The gate

In `DrawAllLeashes`, `SET_DRAW_LEASH` (GScript +0x78) is checked only when [game+0x25005C]+0x45E8 and +0x45EC are both
set, or for the local interface while its render hand +0x483C is set; the leash pull (interface state 0x15) draws two
more leashes (+0x19C, +0x1A0); a creature test `fn_0047ACC0` ≥ 0.4.

### Sources

- The rope functions 0x848600..0x849FFF, `UpdateAllLeashes`, `DrawAllLeashes` and the creature's turn read in the exe
  with the xref index; bw1-decomp for `CreatureDirectControl.cpp` and the names.

## openblack

**Done**: the rope's simulation (bit for bit against the float32 model), the collar from the drawn body, the lengths,
the two leashed creatures' attitudes, the young creature's home, the walk back and the draw; the tug, unreached in the
original, is not kept. The port decisions taken from raffclar's code are in
[Port decisions: the creature](../project/port-decisions-creature.md).

### The collar end and the lengths in openblack

`LeashSystem`'s `CollarPoint` gathers the inputs of the pure `creature_leash::CollarAt`: the matrix the body is drawn
with (`ecs::DrawnBodyModel`: the hand's and the physics' poses, between turns, the pen's scale, the slope shear), the
posed bones and the rig's leash bone, the creature's height and `creature_pose::DrawnSizeShare`. With no bone the
collar is the drawn translation lifted by 0.7 of the height × the share. The posed bones are
`creature_turn_pose::BonesDrawn`: the turn blend the body is drawn with that frame (`drawnBones`), as the original's
+0x47F0, or the frame's own pose when it has none (in the hand, in flight, before its first turn). `LengthsOf` takes
the size it is drawn at (`creature_pose::DrawnSize`): in the hand `creature_leash::InHand(pen size)`, refreshed every
frame the leash is held, and tied to a mobile its height (15 × the pen size), so the pen shortens them as in the
original. The rope's laying, stepping, tension and drawing and a creature tied to another all go through it. Out of
the pen and standing, the point is the one the Transform gave up to float rounding (`affine::Model` writes the
position, `creature::PlacementMatrix` translates by R·(p·R)); with no bone it is bit for bit the same. The turn side
keeps the creature to the hand without reading the rope, as the original; raffclar's pull at a tension above 0.8 is
gone.

### The hand end in openblack

`LeashSystem::Update`, once a frame after the hand is animated and placed, steps a held rope from the hand's wrist:
the pure `creature_leash::WristAt(hand matrix, posed bones)`, the hand model's first bone under the hand's matrix
(`HandSystemInterface::GetHandMatrix() × GetBoneMatrices()[0]`, the same root bone the temple's leash posts and the
hand's casting bands take), with the hand's point as the fallback when the hand has no posed bones. The rope is still
laid at the first frame it is worn from the hand's point (`GetPlayerHandPositions`, the status's point of the
original's lay). It used to be stepped from the hand's point too: the index fingertip the hand is placed by. With the
hand off the land the rope keeps its last place, as before. Nothing on the game turn reads the rope, so the state hash
does not move; only the drawn rope does. `test_leash_rules.cpp` `AHeldLeashMeetsTheHandAtItsWrist` pins the wrist on a
fake two-bone hand.

### The rope in openblack

`Creature/LeashRope` takes the same steps, bit for bit against the model: the cap 300 (it was 150), lengths times 1/41
(they were divided by 41), gravity's force `k_Mass × k_GravityAcceleration` in one rounding (it was the literal
−7.848f, one ulp off), the z, y, x sums, the drag and the springs in the original's order of operations, the ends moved
by an equal share added each sub-step (they were placed at k/n of the way), `Create` laying over the land as above (it
ignored the ground; `LeashSystem` passes the land's height), and the next segment's stretch kept. The lay uses the
default look's half width 0.15, the one the original writes as it lays, whatever look the leash then takes; each step
then uses the look of the leash's type (`creature_leash::LookFor`, the three looks above, set before every step), which
matches the original's per-turn rewrite.

- `test_leash_rope.cpp` pins the model's bits; the pins assume no fused multiply-adds (MSVC x64 `/fp:precise` and
  x86-64 GCC and Clang do not fuse, and the configure rejects the flags that would). `LeashRope.cpp` is built with
  `-ffp-contract=off` on GCC and Clang, as the MPEG audio decoder.
- Kept as ours: the tension's guard when full ≤ slack (0), the unordered edges of the clamps and the spring
  (`std::clamp`; a length of 0 adds nothing), and `Create` still works out the tension (the original leaves +0x18 as
  it was; nothing reads it before the first step).
- None of the old differences made the rope faster; the cap made it slower when yanked. What makes the rope look
  faster is the frame rate (Pending, "The rope and the frame rate").

### The tied lengths, the attitudes and the home in openblack

- **The tied lengths at the tie only.** `LeashSystem::TieTo` sets them; `Update` recomputes the lengths only while the
  leash is held in the hand. `creature_leash::TiedToTree` and `TiedToObject`; `LeashSystem`'s `LengthsOf` tells a tree
  by the `Tree` component and takes `gutils::GetDistanceInMetres`. There is no class of moving things any more.
- **A felled tree.** openblack's felling (`ecs::trees`, the hand's dead tree) keeps the entity and swaps the `Tree`
  component for `DeadTree` (and `FelledTree`), so a leash tied to the tree stays tied to the dead tree (Pending).
- **The land number.** `MapScriptGlobals::landNumber` is what `SET_LAND_NUMBER` writes, as the original's g_game
  +0x205A08, so the home rule's land test reads the same value.
- **Two creatures leashed together.** `creature_leash::AttitudeStepDue` and `AttitudeStep`; the stamp is
  `CreatureLeashAttitude` on the other creature, made the first time a creature is leashed to it.
- **The young creature's home.** `creature_leash::KeptAtHome(HomeKeeping)`, called by `LeashSystem::ProcessTurn` for a
  creature with no leash and a home; `LeashSystemInterface::HomeKeepingOf` gives its inputs. openblack keeps no player
  kind there and has no multiplayer, so both are passed as false. On Land 1 today the creature is at phase 13
  (`LOAD_MY_CREATURE`), so the rule does not hold. A stage move (`SET_CREATURE_DEV_STAGE`, `DEV_FUNCTION` 1) lets a
  creature kept within a radius and on no leash go from phase 5 on (`creature_leash::ReleasedFromHome`,
  `LeashSystemInterface::ReleaseFromHome`: `confinementRadius` = 0 only, `returning` stays); the mind load does not
  clear it yet (Pending).
- **The player's kind** is kept as `PlayerMagic::playerType`; the script command `TOGGLE_COMPUTER_PLAYER` sets 2
  (Land 2's PLAYER_TWO and PLAYER_THREE).
- **The height.** `WalkBackArrival` is the larger of the radius and the height, as the original, but openblack's
  `CreatureHeight` takes 15 × `Creature::size` (the tied lengths, the arrival, the anger reach), which differs in the
  pen (Pending).
- **Another kept area** (+0x1294 / +0x1298) is never set without saved games, as in openblack.

### The walk back in openblack

`creature_leash::ShouldWalkBack(WalkBackCheck)` in the original's order, `WalkBackRestartDistance`, `WalkBackHurry`
and `WalkBackArrival`; `LeashSystem::ProcessTurn` takes the point and radius (the hand on the ground, the tied object's
`Transform`, the home) and its `KeepWithin` reads the script control from `ScriptHeld`, the cell from `land_avoid::At`,
and starts `CreatureLocomotionSystemInterface::WalkBack` (the species' walk fraction + the hurry). The walk sets
`CreatureLeash::returning` and `returningTo` (+0x11B8, mind +0x1C78) and, with a leash on, `Control::Led` (+0x380 =
1); the mind's plan becomes the forced one (ObeyPlayer, action 1, itself; [The leash's tug](#the-leashs-tug)) and
`obeying` stands for mind +0x1C14. The walk is over when the locomotion stops (arrived, or given up), which stands for
`StopWhatIAmDoing`. The rope's tension and slack trigger nothing.

- **A point it can't stand on**: `WalkBack` is refused by `StartMove`'s `IsValid(point, k_CreatureRadius)`
  (`land_avoid::k_CreatureRadius`, the original's 7.1); `KeepWithin` then stops the creature (the failed move's
  `StopMoving` above) and sends it all the same, so its mind stays out for the turn; stopped, the walk is over at the
  top of the next turn and it is sent again.
- **A fighter**: openblack keeps a fighting or knocked-out creature out of `KeepWithin`, its own guard (Pending).
- **`EndAnyAnimsRapidly`**: openblack has no rate doubling (Pending).

### The tug in openblack

openblack has no tug, as the original has none that runs (2026-10-22): the tug's rules, the pull that sped the
creature up and faded each turn, the count of pulls away from each desire and the creature spawner's "Tug" button were
removed, with the sway's leash drag (`LeashForce`) that only that pull fed. Listed in
[Not in the original](../project/not-in-the-original.md#creatureleash).

### Drawing the leash in openblack

`Graphics/LeashDraw`, `Graphics/RendererLeash.cpp`, compared with raffclar's `RendererLeash.cpp`:

- **The same as his.** `leash_rope::BuildRibbon` (the same in both trees) gives the rope's ribbon facing the eye and
  the shadow's ribbon flat on the land at `k_ShadowLift` 0.1 (the original's H + 0.1): black, alpha 0x41, fading to 0
  at both ends. It has the original's side, shadow and corner order, but takes 1e-6 on the length (1e-3 on the square
  in the original) and falls back to (1, 0, 0) where the original keeps zero (Pending).
- **Ours, kept, and matched to the original's draw**: the rope lit point by point, unhazed, and rope and shadow as one
  strip in one material, through `world_triangles::SubmitRaw` with the global alpha table. It is drawn at once in the
  main view, right after the land and its models, and not sorted among the blended things, as the original's, so the
  draw keeps no sort point. Not taken from his tree: his `Translucent` view sorted by the rope's middle, his GPU
  `Leash` shader with the haze and the land colour, his blended rope and shadow without alpha test or depth write
  (shadow first, then rope), and his rope in the sea's reflection at half light. The scripts' `SET_DRAW_LEASH` is one
  flag the draw reads (`script_control::CameraControl::drawLeash`), as the original's single GScript +0x78; his copies
  it to a `drawn` flag on every creature's leash. Without the leash textures ours draws nothing, where his falls back
  to a white texture. openblack gates every leash on `drawLeash` alone (Pending).
- **The light.** `leash_draw::Build` asks `LightAt` once per point (`leash_rope::Point`) and gives both corners that
  colour, alpha included, with the alpha cleared at points 0 and `k_PointCount - 1`; `Renderer::DrawLeashes` answers
  with `land_light::At(CurrentTable(), xz).diffuse`, unhazed (white while the land is not lit); the alpha byte is kept
  as openblack's `Ramp` keeps it.
- **The draw call.** `leash_draw::k_Material` (mode 15, two-sided | tiling, ALPHAREF 0x38) and `leash_draw::k_Table`
  (`render_modes::Table::GlobalAlpha`, A = 255, so 51); `leash_draw::StripOf` interleaves the rope's and the shadow's
  corners per point and `leash_draw::StripIndices` gives the original's indices, so `Renderer::DrawLeashes` makes one
  `world_triangles::SubmitRaw` call per rope (its `table` argument selects the mode and scales ALPHAREF;
  `fs_world_textured` tests texture x vertex alpha).
- Tested in `test_creature_draw` (`LeashDraw.*`); shown on the testbed scenario `examples.leashed_creature`. No
  creature wears a leash on Land 1 or Land 2, so nothing is drawn there.

### Test hooks

- Testbed scenario `examples.leashed_creature` (the rope and its draw).

## Pending

- **The leash's collar end** ([The leash's collar end](#the-leashs-collar-end)): both take the collar from the drawn
  blend f·+0x5178 + (1 − f)·+0x5174 (0x4ED43A..0x4ED46B) now; left: the rope's seed, laid by the original from +0x5178
  as the leash is put on or tied, by openblack at the first frame it is worn, from the drawn body.
- **The walk back into the kept area** ([The walk back into the kept area](#the-walk-back-into-the-kept-area)):
  - +0x12A8, +0x380 = 2, mind +0x1C10 and mind +0x1C38 hold the walk back in the original; they come with the route's
    blocked state, the leash's tie and point gestures, the teleport and the forced actions;
  - a fighter: the original's walk back has no fight test and its start ends the duel (fn_004C7220 → fn_0048F710);
    queued as a fix;
  - `EndAnyAnimsRapidly`: +0x5730 doubles the animation step until the next `StateSet`; queued as a fix;
  - a refused walk is cleared the same turn in the original (+0x11B8, +0x380, mind +0x1C14, the plan's desire); queued
    as a fix;
  - the rest of `ForceActivityAndForceAction`'s arguments and the sub-action agenda's `fn_004FF5C0` are not decoded;
  - the mind array cleared at mind[+0xF60 × 4 + 0x1CEA0] when tied is not ported;
  - with the hand off the land openblack has no hand place: the creature stays kept where it was.
- **The tied lengths and the young creature's home**
  ([The tied lengths](#the-tied-lengths-two-creatures-leashed-together-and-the-young-creatures-home)):
  - the home keeping's computer-player test (player +0x8E0 == 2), now that `TOGGLE_COMPUTER_PLAYER` sets the type:
    queued as a fix;
  - the mind-file loads (0x46B462, 0x63CDDB) clear the young creature's radius whatever (the stage move's own clear at
    phase 5 is ported): queued as a fix;
  - the creature's `GetHeight` is its drawn size × 15 (0x477F50); openblack takes 15 × `Creature::size`: queued as a
    fix;
  - a creature always has a home (+0x1200; `Create` takes the heart's point 0, `ProcessState` point 15): queued as a
    fix;
  - a felled tree unties the leash (`GLeashStatus::Validate` 0x5E6B90): queued as a fix.
- **The leash draw** ([Drawing the leash](#drawing-the-leash)):
  - A, the diffuse alpha the leash's ALPHAREF is scaled by ([0xC37D8C]): its last writer before `DrawAllLeashes` is
    whatever mesh the frame drew last; the port takes 255, and only a run-time read settles it;
  - the route through [0xEA9EB4] to `fn_007ACD90` inside `fn_00801C90` is not read;
  - the degenerate cases of the ribbon (a near-zero tangent, the eye on the rope's line, a vertical tangent for the
    shadow): queued as a fix;
  - the gate in `DrawAllLeashes` (`SET_DRAW_LEASH` only with [game+0x25005C]+0x45E8 / +0x45EC or the render hand
    +0x483C, the pull's two more leashes, `fn_0047ACC0` ≥ 0.4): queued as a fix;
  - `leash_rope::RibbonIndices` is no longer used by the draw;
  - meshes stuck to the ground: the leash draw's material, shadow and rope light are read and ported; port the creature
    quads (`fn_0081F360`) when they have a home in openblack (the other ground meshes are on
    [Lighting the models](../rendering/model-lighting.md)).
- **The drag ropes** ([The leash's rope](#the-leashs-rope)): interface state 0x15's ropes are not read.
- **The hand end** ([The leash's hand end](#the-leashs-hand-end)): with the hand off the land openblack has no hand
  point and the rope keeps its last place; the original always steps it to the drawn hand's wrist. Where the
  original's hand is drawn then is not read.
- **The rope and the frame rate** ([The leash's rope](#the-leashs-rope)): vertical sync is on by default, as the
  original's. Not read: whether the user's original runs windowed (a windowed `LHFlip` blits with no vsync), and who
  reads a mass's +0x24 in the draw (`fn_008491B0`).
