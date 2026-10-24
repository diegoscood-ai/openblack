# Walking round obstacles

## Summary

Villagers and animals walk with one class of the original, `MobileWallHug`: a walk is either a straight walk to the
goal (STEP_THROUGH) or a walk round obstacles, where the walker looks along its step for the first circle it would
enter (a building's, a tree's trunk, water or the land's edge), goes round it (ORBIT) on the side it passes, and
leaves it when it is nearer its goal. The circles are those of the map cell the walker is looking from. A walker that
meets water, the land's edge or a fence while going round may be going the long way: once a turn the game tries one
such walker's walk both ways with a stand-in villager and turns it round if the other way is better. An animal
differs only in re-aiming every turn on the straight walk and in its own turn limit.

openblack ports the rules as pure functions and the villagers' straight line, circles, entry distance, leave test and
the release of a circle that goes; the walk round a circle is still openblack's own float orbit, the look ahead is not
wired, and the animals keep their own copy of the walk (fork issue #82).

## The original

### Moving (MobileWallHug)

`Living::SetupMoveToPos` (0x5F2830) uses the one-argument version of `SetupMobileMoveToPos`: **STEP_THROUGH**, a
straight walk without going around obstacles that in the animals re-aims every turn with their `SetTowardsAngle` (turn
limited by turnAngle). At the step that is one stride away it goes to FINAL_STEP and arrives (0xA) the next turn.
Going around obstacles (LINEAR → ORBIT → EXIT_CIRCLE) is only used by `SetupMoveToWithHug` (0x5F2890): in the
animals, going to the food (19) and fleeing again (6).

- **Circles** (`ObjectCircleIterator`, always those of the 10 m cell of the point being swept): those of each fixed
  object in the cell with collision data (not the fields, which the iterator skips; forests have none; a tree only its
  0.3 m trunk and only in its cell; a multi-cell object in each cell its 7.1 m circle touches). That of a building or
  feature is the one of its box (centre, the largest half-axis × scale, minimum 1 m); if the box is more than 1.4 times
  longer than wide, a row of int(length / width) + 1 circles of the short half-axis. Then, the water cells (or outside
  the map) of the cell and its 8 neighbours, each a 7.2 m circle at its centre.
- **LINEAR** (`MoveToCircleHugLinearSquareSweep` 0x60CA50): the nearest circle that the ray of the step enters (the
  real entry, dot − √disc; behind −0.2 m it does not count); TurnsToObj = distance / step length (0xFF: none or more
  than 255 turns away). It walks straight, and **only re-aims** (`InitStepsXZ`, with the species' limited turn) and
  sweeps again when changing cell. When the count runs out it starts orbiting on the side the step passes (CW / CCW).
- **ORBIT** (0x60B0E4 / 0x60B40E): turns speed / radius × 0.0497 + 1 every turn. `MoveToCircleHugCircleSquareSweep`
  (0x6159F0 CW, 0x614C40 CCW) searches along the orbit for the first intersection with another circle of the cell (or
  the destination if it is inside the circle, 0.1° earlier), TurnsToObj = the arc / (1.5 × speed), and sets the
  tangent heading (towards the centre ± 90°, minus 64 per radius beyond 0.9 radii). Less than one turn away: the
  destination → direct STEP_THROUGH; another circle → switches to it and sweeps again (3 levels). It exits
  (EXIT_CIRCLE, straight outwards from the centre) when it is closer to the destination than when it started orbiting,
  with the destination on the inner side and ahead; outside the circle it goes back to LINEAR_CW / CCW.
- Consequence of the original: since LINEAR only re-aims when changing cell and with turnAngle (6° the cow), an animal
  whose heading no longer points at the destination (or that leaves an orbit outwards) moves away and does not arrive
  [the code; not seen in the original game]; if the destination lies inside the circled circle, it arrives via the
  final STEP_THROUGH. In numbers: the sheep turns at most 64 (11°) per re-aim (`Animal::SetTowardsAngle` 0x418560; the
  almost full turn only inside its turning circle, R = 2 × 0.075 m / 0.196 = 0.77 m) and walks 0.075 m per turn, so it
  re-aims once every ~133 turns. Checked against the code (MoveTo 0x60B095, MoveToCircleHug 0x60D800, InitStepsXZ
  0x60BFA0, SetupMobileMoveToPos 0x60ABC0, AreWeThere 0x60AD60, `Living::MoveToPos` 0x5EC270, ProcessReaction 0x5F1270:
  nothing else re-aims).
- The handlers of `MobileWallHug::MoveTo`: LINEAR 0x60B095, ORBIT_CW / CCW 0x60B0E4 / 0x60B40E, EXIT_CIRCLE 0x60B78F.
  Each returns 6 / 7 when it stepped (same / new map cell) and 1 when it only changed state.
- **Going round with no circle held** (`GetObjectPtr` of +0x70 NULL, as after a `CircleHugInfo::Reset`): ORBIT_CW
  0x60B0F2 and ORBIT_CCW 0x60B41C call `InitStepsXZ`, set the state to 0xD LINEAR_CW / 0xE LINEAR_CCW and sweep from
  the position (`MoveToCircleHugLinearSquareSweep`(Pos) 0x60CA50); EXIT_CIRCLE 0x60B79D does the same, 0x12
  EXIT_CIRCLE_CW to 0xD and 0x11 EXIT_CIRCLE_CCW to 0xE (`cmp [esi+0x5E], 0x12; setne cl; add ecx, 0xD`). Each then
  jumps to the end (0x60B8D8): no step this turn, result 1.
- Constants: a water cell's circle 7.2 m (`ObjectCircleIterator::Init` 0x60D21C); a tree's trunk 0.3 m
  (`Tree::CreateCollideData` 0x74C622); a box more than 1.4 times longer than wide becomes a row of circles
  (`NewCollide::NewCollide` 0x829470); the "behind" bound −0.2 is the double at 0x930668; arc length (MapCoords) /
  radius (m) → game angle is 0.0497359186 = 2048 / (2π × 6553.6) (0x930660); the goal point is taken 0.1° before the
  goal (cos / sin at 0x930680 / 0x93067C); the orbit sweep's recursion budget is 3 (0xBF42C8 / 0xBF42CC).
- `CircleHugInfo::GetObjectPtr` 0x60A660 / `SetObjectPtr` 0x60A770; the rest of SetObjectPtr is the bookkeeping of
  `g_CircleHugStateInfo`.
- A hug handler that reaches FINAL_STEP (4) (0x60B232, 0x60B801..0x60B81E) sets the step to Pos − goal; it is unused,
  since FINAL_STEP snaps to the goal the next turn (0x60AF6C).
- The water test of the iterator is `LH3DIsland::IsWater` 0x60D3A0 (the land cell under a cell index is water, or
  there is none); the neighbours' test in `ObjectCircleIterator::Init(int)` 0x60D0A0 is the same hasWater bit.
- `ObjectCircleIterator` over a map cell: Init 0x60D280 / 0x60D0A0, GetMapChild 0x638560. It takes the cell's fixed
  list in its order (`GetFirstObjectFixed` 0x6034B0); Init skips an object without `GetCollideData` (vt +0x858,
  0x60D29D) and a field by its RTTI (0x60D2B1..0x60D2C8). A multi-cell object is in the cells of its
  NewCollideDescriptor.
- The LINEAR sweep: the entry distance is fn_0060CEE0 (dot − √disc; behind −0.2 it is FLT_MAX), the sweep record
  fn_0060CF20, "nearer than the best" fn_0060CFF0; the step is normalised in metres (0x60CAB9) and turns = distance /
  |step| (0x60CE5E: above 255 none, not above 0 or unordered 0).
- The orbit sweep orders points along the orbit starting from the walker (g = its position − the centre, 0xD3EE70 /
  0xD3EE68): `Point2DCompare::operator<` 0x610180 / 0x6101F0, `Resolve__Point2DCompare` 0x6101C0 / 0x610230 (the half
  the orbit reaches first), the exact cut `IntersectIntervalCircle_1::Resolve` 0x6169F0 / fn_00616C70, the interval
  fn_00616D00 / fn_00616A80 (hi = d, lo = d + perp(d), × the radii's ratio when the other circle is not bigger;
  resolved at once when the bounds come out in the wrong order), `Point2D::Cross` 0x611240.
- The end of the sweep sets the heading along the orbit (0x6166E3 / 0x61593B).
- ORBIT uses the step as it is now, which the sweeps may have re-aimed, not the one it moved by (0x60B2C5).
- Whole units to metres is x 10 / 65536 (`WholeXZToPoint2D` 0x60C1A0, `MapCoordsToPoint2D` 0x60C180). A unit vector
  is x * (1 / length) (0x60CAFD-0x60CB0B, `Point2D::Normalize` 0x611380). The orbit sweep's turns are single precision
  at each step (0x616543-0x616587), and its `acos` is not clamped (a NaN takes the "less than one turn" branch, `test
  ah, 1` at 0x6165C1). The side taken at the handover uses the circle's middle truncated to whole units
  (0x60D8A4-0x60D8C0). The leave test (`GetMetresDistanceSq`, and the differences as whole units), the exit test and
  the goal-inside test measure from the circle's MapCoords. The original reads `GetObjectPtr` without looking.

`GUtils::Spiral` (dir 1, count 1: (−1, 0), (0, −1), (+1, 0) × 2, (0, +1) × 2...) in all the spirals; `Collide(1)` is
only water (the `hasWater` bit of the terrain cell) or outside the map, `Collide(0)` nothing; `CalcRandomPos`
(0x5ED080): two random points, each with a spiral of 25 cells, accepting if it is on the map, without collision,
outside the turning circles and (birds) over a terrain block; otherwise, the centre if it is outside its turning
circles, and otherwise, its own position. Angles with the trunc(65536 cos) tables and `LHArcTan`. Initial heading 0
(+x). The flock list is in reverse order of creation: the leader is the first one that entered; the birds' formation
counts from the newest. The animals are processed from the newest to the oldest.
- CircleHugInfo::GetObjectPtr (0x60A660). A circle whose fixed object was deleted is none [inferred: the original drops the huggers of a deleted object through g_CircleHugStateInfo]
- CircleHugInfo::SetObjectPtr (0x60A770); the rest of it is g_CircleHugStateInfo's bookkeeping
- FINAL_STEP (4) from a hug handler (0x60B232, 0x60B801..0x60B81E): the step is Pos - goal (unused: FINAL_STEP snaps to the goal next turn, 0x60AF6C)
- the land cell under a cell index is water, or there is none (LH3DIsland::IsWater 0x60D3A0; the neighbours' test in ObjectCircleIterator::Init(int) 0x60D0A0 is the same hasWater bit)
- ObjectCircleIterator over the map cell of p (Init 0x60D280 / 0x60D0A0, GetMapChild 0x638560): the collide circles of the objects of the cell's fixed list in its order (MoveToCircleHugLinearSquareSweep 0x60CA50 takes GetFirstObjectFixed 0x6034B0 at 0x60CAA0) that have any (Init skips an object without GetCollideData vt +0x858, 0x60D29D, and a field by its RTTI, 0x60D2B1..0x60D2C8), then the water circles of the cell and of its 8 neighbours in the original's order. Which objects are in the cell is the list's: a multi-cell object is in the cells of its NewCollideDescriptor.
- `MakeLinearSweep`: fn_0060CF20
- `LinearSquareSweep`: 0x60CAB9: the step in metres, normalised; its length (0 for no step) divides the distance into turns
- `LinearSquareSweep`: 0x60CE5E: turns = distance / |step|; above 255 none, not above 0 (or unordered) 0
- Resolve__Point2DCompare (0x6101C0 / 0x610230): the half the orbit reaches first
- Point2DCompare::operator< (0x610180 / 0x6101F0): a is reached before b
- fn_00616C70 / IntersectIntervalCircle_1::Resolve (0x6169F0): the exact cut
- fn_00616D00 / fn_00616A80 (and the same code inline for the first ones): hi = d, lo = d + perp(d) (x the radii's ratio when the other circle is not bigger); resolved at once when the bounds come out in the wrong order
- the end of the sweep (0x6166E3 / 0x61593B): the heading along the orbit, towards the centre + / - 90 degrees, less 64 per radius the animal is off 0.9 x the radius
- EXIT_CIRCLE (0x60B78F): straight on until out of the circle, then LINEAR_CW / CCW again
- MobileWallHug::MoveTo's LINEAR (0x60B095), ORBIT_CW / CCW (0x60B0E4 / 0x60B40E) and EXIT_CIRCLE (0x60B78F) handlers: 6 / 7 when it stepped (same / new map cell), 1 when it only changed state
- MobileWallHug::MoveTo 0x60AF20 for this villager (its state function's call, Living::MoveToPos 0x5EC280)

### Walking round obstacles (MobileWallHug)

The villagers walk with the same `MobileWallHug::MoveTo` 0x60AF20 as the animals ([above](#moving-mobilewallhug)).
The game logic runs with the FPU at 24 bits, so every x87 operation below rounds to a float.

- **The walker's fields** (`MobileWallHug.h`, offsets from the disassembly): the u16 speed +0x5A, the game angle
  +0x5C (a u16, which `SetGameAngle` 0x60DA90 stores whole before it sets the 3D angle from `ConvertGameAngleTo3D`
  0x74DC50), the move state +0x5E, the step +0x64 / +0x6C (whole map units a turn, written by `InitStepsXZ`
  0x60BFA0) and the step-rebuild counter +0x78 (an s8).
- **The straight walk to the goal** (STEP_THROUGH, 0xB, `MoveTo`'s handler at 0x60B02A): an animal (vt +0x454
  `IsAnimal`) aims its step at the goal (`InitStepsXZ`) every turn. Any other walker counts +0x78 down (`dec byte`) and
  aims only when it reaches 0, setting it to 0x7F (127) first. Then the step is taken, and `AreWeThere(0)` gives
  FINAL_STEP. The counter is set to 1 by both `SetupMobileMoveToPos` (0x60ABA4, and 0x60ACB5 for state 0xB) and by
  `SetupMobileMoveToObject`, so a villager's walk aims on its first turn (from where it was set up, so the same step)
  and then every 127 turns. ARRIVED that is not there (0x60B022) sets it to 0x10 and falls into STEP_THROUGH in the
  same turn, which counts it at once: the aim comes 16 turns on. The look along a circle sets 0x10 when it sends the
  walker straight to its goal.
- **Setting a walk up** (`SetupMobileMoveToPos(pos, state)` 0x60ABC0): the goal (+0x80), `InitStepsXZ`, out of the
  completed lookahead set and the extended entry distances, then `AreWeThere(0)` gives ARRIVED (the circle-hug info
  left as it is). Else LINEAR (0xC) sweeps at once from the position (`MoveToCircleHugLinearSquareSweep` 0x60CA50) and
  STEP_THROUGH (0xB) sets +0x78 = 1; then the state is set. Its callers: `Living::SetupMoveToWithHug` 0x5F28C3 and the
  footpath start `SetupMoveOnFootpath` 0x5EDCE2 pass 0xC, `Living::MoveOnFootpath` 0x5EDF64 / 0x5EE052 pass 0xB from
  node to node, and the hug editor's keys and mouse. The footpath's rebuild does not go through it:
  `GFootpathNode::ClearFromPreviousNode` 0x538B10 (and `ClearFromNextNode`) sets each follower's state to 0xC and
  sweeps from its position with the step it has, with no new aim and no arrival test; `ClearForDeletion` 0x538A40
  sets 0xC, the new node, the goal = its position, `InitStepsXZ` and the sweep, with no arrival test.
- **The turn round a circle** (ORBIT CW 0x60B10D, CCW 0x60B40E): `fild` of the u16 speed (+0x5A), `fdiv` the radius
  (`Obj` +0xC), `fmul` the float 0.0497359186 (0x930660, bits 0x3D4BB7E4), `__ftol`; the angle +0x5C becomes angle −
  (u16) turn − 1 (`dec` at 0x60B147), & 0x7FF. Three float operations (× 2048, / 2π, / 6553.6) give another value:
  speed 33891 at radius 15.05 is 112 in the game, 113 that way.
- **The entry distance** (0x60D9F0-0x60DA6C): `GetMetresDistanceSq` 0x605FB0 (each difference through
  `ConvertWholeDistanceToMeters` 0x74DCC0, then dz² + dx²), `fsqrt`, `fmul` the double 128.0, `fsub` the double 1.0,
  all at 24 bits; not above 0 (or unordered) gives 0, else `__ftol`. 0xFFFF or more stores 0xFFFF at +0x76 and the
  value in `g_CircleHugStateInfo` 0xD3EE78. 37.83 m (the squared distance 1430.967) gives 4841; in double it would be
  4840.
- **The leave test** (CW 0x60B25C-0x60B312): limit = e × 2^-14 × e (e = +0x76 by `fild qword`, the product stored as
  a float); the walker leaves only when `GetMetresDistanceSq` < limit (an unordered compare passes). dx and dz are a
  `fild qword` of the u32 position less a `fisub` of the s32 goal: exact, then rounded once. side = sx · dz − sz · dx <
  0 (CW) and dx · sx + dz · sz < 0. Beyond 2^24 units (2560 m) taking each coordinate as a float first rounds twice: at
  (16777217, 16777216), goal (1, 0), step (−1, −1) the game does not leave, the double rounding would.
- **The look along a circle** (CW 0x6159F0, CCW 0x614C40):
  - Both normalisations take one over the length first and multiply each coordinate by it: the inline one of the
    walker's direction (0x615AAE-0x615ADE) and `Point2D::Normalize` 0x611330. The length is kept for overlap = length
    / radius − the double 0.9 (0x615AE0-0x615AEE).
  - angle = `__CIacos` of `Point2D::DotProduct` 0x611310, stored as a float; the long way round (2π − angle, the
    float 0x40C90FDB) when `Point2D::Cross` 0x611240 is above 0 (CW, 0x61652A) or below 0 or unordered (CCW,
    0x615782). The `__CIacos` (0x616515, 0x61576D) works on the x87 stack at 24 bits and its result is stored as a
    float. `__CIacos` 0x7C76E0 → fn_007C76FD computes fpatan(sqrt((1 + x) × (1 − x)), x) (0x7C7718..0x7C7726), and
    outside [−1, 1] gives the x87 indefinite, a NaN (0x7C7781). It is not `acosf`: 1870 of 20808 inputs differ by one
    ulp from MSVC's `acosf` (oracle); the original equals `f32(atan2(f32(sqrt(f32(f32(1+x) × f32(1-x)))), x))`, every
    step rounded to float (20808 of 20808).
  - turns = angle × radius × 65536 over the u16 speed × 10 × the double 1.5 (0x616547-0x616587), each a float
    operation, the quotient stored as a float.
  - Above 255 gives 0xFF (+0x74). Below 1, or unordered, gives the step through or the handover (0x6165B5); a
    handover copies the interval's landscape-or-fence byte to `g_CircleHugNeedsLookahead` 0xD3EED0. Below 4 gives 0;
    else `__ftol` into +0x74.
  - The heading (0x6166E3-0x616720, CCW 0x61593B-0x615978): `GetAngleFromXZ` 0x74D240 from the position to
    `CircleHugInfo::GetObjCoords` 0x609CC0, + 0x200 − `__ftol`(overlap × 64) (CW) or − 0x200 − `__ftol`(overlap ×
    −64) (CCW, the float at 0x930688), & 0x7FF, through `SetGameAngle` 0x60DA90 and `RebuildMoveByStep` 0x609D10.
  - The handover budget: the static depth (CW 0xBF42C8, CCW 0xBF42CC) starts at 3 and goes down at each entry; a
    handover with the depth at 0 or less sets +0x74 = 0xA, else it sweeps the new circle.
  - A water or land's edge circle never replaces a nearest end of the arc that is the goal's mark
    (MobileWallHug.cpp:1542).
  - `GoesRoundClockwise`'s operand order is at 0x60D985.
- **The straight line** (`MoveToCircleHugLinearSquareSweep` 0x60CA50): the step is normalised as above (0x60CAB9);
  turns = near / |step| stored as a float (0x60CE5E): above 255 gives 0xFF, not above 0 or unordered gives 0, else
  `__ftol`; 0xFF counts as none. A walker leaving a circle that goes keeps the turns its sweep found after letting go
  of the circle, and its LINEAR handler sweeps again from its position on its next turn (TurnsToObj != 0xFF with no
  circle, 0x60B095).
- **The circles a walker comes across** (`ObjectCircleIterator`, bw1-decomp Collide.h:24-61 and
  MobileWallHug.cpp:1164-1271): only those of the map cell of the point the sweep is given, never its neighbours'
  objects. First the cell's fixed list from its head (`GetFirstObjectFixed` 0x6034B0, then `GetMapChild`):
  `Init(Object*)` 0x60D280 skips a thing without collide data (vt +0x858, 0x60D29D) and a field (its RTTI,
  0x60D2B1-0x60D2C8); a thing with a child list (`IteratorList`, the row of circles `NewCollide` 0x829470 makes for a
  box more than 1.4 times longer than wide) yields each child, in order, and not the circle round them; any other thing
  its one circle. Then the landscape (`Init(int)` 0x60D0A0, below). The straight line keeps the nearest of them
  (`IntersectIntervalLine` operator<, 0x60CFF0), whatever the list's order. The point is the walker's own position
  when a walk is set up (`SetupMobileMoveToPos` 0x60ABC0) and when it leaves a circle (EXIT, 0x60B78F), and the next
  position (the position plus the step it had, before the new aim) when the step crosses into another cell
  (`MoveToCircleHug` 0x60D800, MobileWallHug.cpp:1369-1374); the line itself is always looked along from the
  position.
- **Water and the land's edge** (`ObjectCircleIterator::Init(int)` 0x60D0A0, after the last thing of the list): the
  cell of the point and its eight neighbours, in the order of the jump table at 0x60D254 (the same cell, +x, -x, +z,
  -z, then -x -z, -x +z, +x -z, +x +z), each offset applied to the 16-bit cell index of the point's MapCoords (`inc` /
  `dec word`, so one below 0 is 65535). A cell is blocked when its index is not below the map's width or depth
  (g_game +0x59C8 / +0x59C4, 0x60D11D-0x60D142; `GMap::InBounds`' answer at 0x60D146 is thrown away), when
  `LH3DIsland::GetCell` 0x516AA0 gives no land cell, or when the land cell has the water bit (+6 & 0x10, 0x60D15B). A
  blocked cell yields a circle of 7.2 m (the float 0x40E66666 at 0x60D21C) round its middle
  (`GLandscape::ConvertMapCoordToLandscapePoint` 0x613750 of the index with the low word 0x8000; x 10 / 65536, so
  exactly 10 i + 5: oracle, cells -2, -1, 0, 1, 255, 511, 512), made once per cell (`MakeSubCollideBlockPos` 0x7370B0
  keys it by x >> 13 on each axis, kept in `g_CircleHugStateInfo.LandscapeBlockers` until `ResetBlockersForClearMap`
  0x60A400), so a cell's circle is always the same one. For a point below zero on an axis (its cell 0xFFFF) the cells
  are -2, -1 and 0, with circles at -15, -5 and 5 m, and cell 0 is tested for water. Its landscape-or-fence byte is
  set (`Collide.h:144`).
- **When a circle goes** (`CircleHugStateInfoT::OnDeletionOfNewcollideObj` 0x60A680, jump table 0x60A748;
  MobileWallHug.cpp:296-337): `NewCollide::Obj::~Obj` 0x82AE60 calls `g_collide_del_callback` (0xEB9A98, set to
  0x60A670 at 0x609884), which runs it at once for every walker hugging the circle (`ObjToMwh`). LINEAR, LINEAR_CW
  and LINEAR_CCW sweep again from the position with the step they have; ORBIT_CW / CCW face the goal (`InitStepsXZ`),
  become LINEAR_CW / CCW and sweep; EXIT_CW / CCW do the same and then let go (`SetObjectPtr(NULL)`) of what the sweep
  found; any other state lets go (`OnDeletionOfNewcollideObj` runs from the circle's destructor). A thing's circles go only with its collide data, released by `RemoveMapObject`
  (`SingleMapFixed::RemoveMapObject` 0x52E600 releases at 0x52E60A, after it is out of its cell at 0x52E603; `MultiMapFixed`
  0x52E820, after the cells and the footpaths) and by each `CreateCollideData` before it builds the new one
  (`SingleMapFixed` 0x52F513, `Tree` 0x74C5F6, `MultiMapFixed` 0x52F553, `CitadelHeart` 0x468FBD, `WorshipSite`
  0x77E499, `BigForest` 0x439580: the callers of `ReleaseCollideData` 0x52F6A0 / 0x52F6D0). `CreateCollideData`
  itself is reached only from `InsertMapObject` (`SingleMapFixed` 0x52E633 through vt +0x864, `MultiMapFixed`
  0x52E669 through vt +0x908; the other callers of those slots are on classes where the slot is another function), so
  a rebuild is always a removal and an insertion. Before its release, `MultiMapFixed::RemoveMapObject` (0x52E7EF)
  calls `ProcessRemoveFromMap` 0x60F850 (MobileWallHug.cpp:2034-2087): from the thing that goes, it looks through each
  one's cells for the other `MultiMapFixed` whose circles overlap one of its own (`ObjectsCollide`: squared distance
  below the squared sum of the radii, each circle of a row), through each other, and gives every walker hugging a
  circle of any of them the entry distance 0xFFFF with the extended distance 0x7FFFFFFF in the map (0x60FAE8,
  0x60FB23), so that it leaves at its next leave test. Only ORBIT's leave test reads it (0x60B1EC, 0x60B21A, limit at
  0x60B25C); on leaving a -1 is set back to 0. The citadel heart and the worship site build their collide data with
  their own `CreateCollideData`, with a radius per child circle (the heart: a 15.05 m circle at its centre and 32
  circles of 3.311 m on 7 spokes; the worship site: 10 circles of 1.82 / 2.6 / 3.9 m from a fixed table, no centre
  circle; the layout is on [Temple and citadel](../magic/temple-and-citadel.md)).
- **The flag for trying both ways round** (below): reaching a water or land's edge circle on the straight line sets
  it (`IsLandscapeBlocker`, MobileWallHug.cpp:1356, compares the circle with the blocker kept for the cell of its
  middle: a fence does not count there), and every hand-over of the look along a circle copies the new circle's
  landscape-or-fence byte (MobileWallHug.cpp:1605).

#### Trying both ways round (DoWallHuggerLookahead)

A walker that comes up against water, the land's edge or a fence while going round may be going the long way round.
The game tries such a walker's walk both ways with a stand-in villager, and turns the walker round if the other way
is better. Every number below was read from bw1-decomp `MobileWallHug.cpp` and checked against the exe.

- **Who waits for a trial.** `MobileWallHug::MoveTo` 0x60AF20 clears `g_CircleHugNeedsLookahead` 0xD3EED0 first. The
  flag is then set in two places:
  - The straight line (`MoveToCircleHug`): when the turns to the circle ahead (+0x74) run out and that circle is a
    landscape blocker (`IsLandscapeBlocker`: the 7.2 m water or land's edge circle of `g_CircleHugStateInfo`'s
    `LandscapeBlockers` map). A fence does not count here.
  - The look along a circle: every handover copies the circle's landscape-or-fence byte (`Collide.h`
    `IsLandscapeOrFence` = `DoIPointAtLandscapeMaterial || IsFence`). A later handover in the same turn
    overwrites it.
  At the end of `MoveTo`, a walker with the flag goes into the pending set (`PendingLookahead`, 0xD3EE98). This is
  skipped when the walker is already in the completed set (`CompletedLookahead`, 0xD3EEA8) or a trial is running
  (`EvaluatingLookahead` 0xD3EEB8). Animals walk with the same `MoveTo`, so they are tried as well (animals come from
  the same object heap, inferred: the same base class).
- **Leaving the sets.** The completed set loses the walker when it leaves a circle (the ORBIT leave test, CW and CCW),
  in both `SetupMobileMoveToPos` (0x60AAD0, 0x60ABC0) and in `CircleHugInfo::Reset` 0x60A9F0. Only `Reset` also takes
  it out of the pending set. `Reset` runs from the first `SetupMobileMoveToPos` when the walker is not there yet, from
  the look along a circle when it sends the walker straight to its goal, from the exits of the reaction (Living and
  Animal), in-script, dance-in-script, MOVE_TO_POS and move-on-path states, and from the walker's constructor and
  destructor (the direct callers of 0x60A9F0).
- **When.** `DoWallHuggerLookahead` 0x609A50 runs once a turn from the game's turn (`GGame::Loop`, after
  `ProcessGameCode` and fn_005557D0, bw1-decomp Game.cpp:2097). It takes one walker, the first of the pending set, and
  moves it to the completed set. The set is a `std::set<MobileWallHug*>` ordered by pointer, so the first is the
  walker at the lowest heap address.
- **The trial** (`evaluate` 0x609D50; the first call is inlined at 0x609AA3-0x609BBE):
  - The stand-in is `Villager::Create(Pos, &GVillagerInfo::InfoList[0], 21, false)` (at 0x609AB8), made with 0x15 and
    the list at 0xDA6BE8 (row 0, the Celtic housewife), even for an animal. Making it runs everything
    `Villager::Create` 0x74FBE0 does: its game random draws (GameRand(10) among them, so the look ahead moves the
    random trace), its creation number and the chance of a named villager.
  - It copies the walker's turns to the circle (+0x74), its entry distance (+0x76, and the extended one when that is
    0xFFFF), the hugged circle (`SetObjectPtr`), the move state (+0x5E), the goal (+0x80), the step (`GMoveBy::Init`
    0x609CA0 with x, z and the height part) and the game angle (+0x5C).
  - Its speed is `Villager::SetSpeed(speed << 2, 1)` 0x750ED0 (at 0x609B80): four times the walker's u16 speed
    (+0x5A), scaled by the stand-in's own pace factor ((creation index x 47 % 31 - 16) / 100 + 1, less when young or
    old), so an animal's trial walks by the villagers' rules.
  - The step-rebuild counter (+0x78) is not copied. The trial never reads it before writing it: every way into
    STEP_THROUGH sets it to 16 first.
  - It calls `MoveTo` once, then up to 1500 (0x5DC) more times until one returns 10 (arrived) (0x609B87-0x609BA7, the
    same at 0x609F9C). The turns to spare are 1500 when it arrives on the first call, 1500 - k when it arrives k calls
    later, and -1 when it never does (1501 calls in all). The distance is `goal.GetMetresDistanceSq(Pos)` 0x605FB0.
    Then `ToBeDeleted(0)` (vt +0xC).
  - Turned round (`evaluate` with true): `swap` runs on the stand-in first. When that leaves it in STEP_THROUGH, the
    trial gives 0 turns and the largest float, with no walk (0x609F88).
- **The choice** (0x609BD7-0x609BF6, `performance::operator<`): the walker is turned round when the turned-round trial
  has more turns to spare. With as many, it is turned round when it ends nearer (`fcomp`, C0; unordered counts as
  nearer). A tie keeps the walker as it goes.
  - Not turned round in ORBIT_CW (0xF) or ORBIT_CCW (0x10), it is faced afresh: `SetGameAngle`(`GetAngleFromXZ`(Pos,
    the circle's coordinates) + 0x200, or - 0x200, & 0x7FF) and then `RebuildMoveByStep` 0x609D10
    (0x609C14-0x609C80).
- **Turning round** (`swap` 0x60A0B0, `swapDirection` 0x60A030):
  - LINEAR_CW and LINEAR_CCW, ORBIT_CW and ORBIT_CCW, and EXIT_CIRCLE_CW and EXIT_CIRCLE_CCW change to the other way.
    Plain LINEAR, STEP_THROUGH and the rest stay as they are.
  - An ORBIT that still hugs a circle then looks along it again the new way from its position.
  - `swapDirection` moves the walker to Pos + 3 x step through `MoveMapObject` (vt +0x55C), with no check of where it
    lands. Then `step.Init(-x, -altitude, -z)`: x becomes -x, z becomes `__ftol`(-height part), and the height part
    becomes float(-z). The game angle becomes (angle - 0x400) & 0x7FF.
  - A step that was just aimed has no height part, so after the turn its z is 0 until it is next aimed: a walker on
    the ground turns round walking along x only. Turned round twice before that, its z comes back. The exe's results
    on six walkers were taken with the oracle (the move captured from the `MoveMapObject` call).
  - The step's height part (+0x68) is read only by these copies and turns. Among the functions of `MobileWallHug`,
    `CircleHugStateInfoT` and `GMoveBy`, the others only write 0 to it: the constructor, `RebuildMoveByStep`,
    `InitStepsXZ`, the wander and three places in `MoveTo`.

### Sources

- bw1-decomp: `src/Black/MobileWallHug.cpp`, `Collide.h`, `Game.cpp`; the rest read in the exe with the xref index;
  the turning round and the water circles checked with the oracle.

## openblack

**Partial.** The rules are pure functions in `ECS/WallHugRules` (`wall_hug::`) and `src/ECS/WallHugBookkeeping`; the
villagers' straight walk, straight line, circles, entry distance, leave test and the release of a circle that goes are
ported; the walk round a circle is openblack's own float orbit; the look ahead is not wired; the animals keep their own
copy (`ECS/AnimalWallHug.*`).

- **The walker** (`components::WallHug`): `step`, `gameAngle` and `turnsUntilStepRebuild` hold the original's fields.
  Its walk still reads `stepMetres`, the same step in metres (`InitStepsXZ` writes both), which openblack's own float
  orbit writes alone until the walk round circles works in whole units.
- **Going round with no circle held**: `PathfindingSystem::Step`'s first look at the walk does what the original's
  ORBIT and EXIT_CIRCLE handlers do (`InitStepsXZ`, LINEAR_CW / LINEAR_CCW the same way round, the look ahead from
  where it stands, no step) ✅ (`PathfindingGoneObstacleTest.AnOrbitHoldingNoCircleHeadsStraightOnWithoutAStep`,
  `AnExitHoldingNoCircleHeadsStraightOnAndLooksAhead`). Before, it gave the walk up to a straight walk to the goal
  (STEP_THROUGH).
- **The straight walk**: `PathfindingSystem::Step` counts and aims the same way; its own stand-in for the walk round
  circles' unported branches (STEP_THROUGH from wherever it gives up) also sets 16 (`PathfindingGoneObstacleTest`:
  `StepThroughAimsOnTheFirstTurnThenEvery127Turns`, `ArrivedNotThereAimsAgainOnItsSixteenthTurn`).
- **Setting a walk up**: `villager::SetupMobileMoveToPos(entity, goal, state)` for `SetupMoveToWithHug`, the footpath
  start, the worship walk and the debug bar's Move To Point; `living_footpath::RePlanHug` and `MoveToNode` as the two
  rebuilds; the restart after a teleport aims afresh and sweeps (`PathfindingSystem::SweepAhead`, the circles of the
  walker's own cell), and the walk no longer waits for its first step to aim and sweep. The script's walk
  (`SetupMobileMoveToPos(pos)` 0x60AAD0, in `ECS/Villager/VillagerScript.cpp`) resets the hug (`CircleHugInfo::Reset`
  0x60AB9F) only when the villager is not there yet: one already there keeps the circle it held (0x60AB91 returns
  first) ✅. Both setups bring an entry distance kept apart in the original's map (0xFFFF or more in openblack's
  `entryDistance`) back to 0 (0x60AB7D, 0x60AC6E), the circle kept
  (`VillagerCoreTest.ScriptSetupMoveToPosResetsTheHugOnlyWhenNotThere`).
- **The numbers**: `OrbitTurn`, `EntryDistance`, `LeavesCircle`, `ScanLine`, `SweepCircle` and
  `k_TurnsAfterTooManyHandovers` hold the rules above.
- **The arc's angle**: `wall_hug::ArcAngle` takes the runtime's arccosine as `__CIacos` works it out
  (`gutils::x87::Acos`, see [Floating point](../engine/floating-point.md#the-runtimes-arctangent-arccosine-power-and-logarithm)),
  stored as a float, then the long way round as above: 20 000 of 20 000 arcs (100 directions × 100 ends, clockwise
  and anticlockwise) equal the original's own `Point2D::Normalize` 0x611330, `Point2D::DotProduct` 0x611310,
  `__CIacos` 0x7C76E0 and `Point2D::Cross` 0x611240 run in place from 0x616500 (CW) and 0x615758 (CCW) in the oracle
  (`test_wall_hug_arc`). The animals' copy takes the same `ArcAngle`, unclamped, and counts turns that are not a
  number as too close (0x6165B5), as the villagers' does.
- openblack differs: `map_collide::Shape` has one radius for every child and builds the heart and the worship site
  from the mesh's box (`src/ECS/MapCollide.h:26`), a fix row.
- **The circles**: `thing_circles::CirclesOf` (ECS/ThingCircles) reads a thing's circles from the collide data the map
  built for it (`map_cells::CollideDataOf`: a tree's 0.3 m trunk, a mesh's shape and its row), and `CirclesInCell`
  walks `map_cells::ForEachFixed`. `PathfindingSystem`'s straight line is `wall_hug::ScanLineFrom` over them (in
  metres, from the walker's position as openblack keeps it), and the hug keeps the circle it found
  (`WallHugObjectReference`: the thing, which of its circles, the centre and radius). openblack's own look along a
  circle (`OrbitScanForObstacle`, until the port of `CircleSquareSweep`) reads the same circles of the cell of its
  point, both radii as the collide data has them
  (`PathfindingGoneObstacleTest.TheLineHeadsForTheNearestCircleNotTheFirstOfTheList`,
  `TheLineMeetsTheCircleOfALongThingsRow`, `AFieldIsWalkedOver`, `OnlyTheCellOfTheLookCounts`; `ThingCircles.*`).
  Water and the land's edge: `thing_circles::CirclesInCell` adds them after the things (in
  `wall_hug::k_LandscapeCellOrder`'s order, round `wall_hug::LandscapeCircleCentre`, pinned in
  `test/test_wall_hug_bookkeeping.cpp`), blocked by `sea_cells::InBounds` and `sea_cells::IsWater` (no land cell is
  water); the hug names such a circle by its cell, which stays the same circle as long as the map
  (`ThingCircles.AfterItsThingsACellHas…`, `OnlyTheCellsOfWaterOrOffTheLandHaveACircle`,
  `PathfindingGoneObstacleTest.TheLineHeadsForTheWaterOfANeighbouringCell`).
- **When a circle goes**: `map_cells::RemoveMapObject` publishes `events::CollideDataReleased` once the thing is out
  of its cells and its footpaths are told, with `OverlappingMultiCellThings` for a thing of many cells (worked out
  before its link goes); every removal and every re-insertion (a move, turn or change of size on the map) goes through
  it. `PathfindingSystem::OnCollideDataReleased` (wired in the game's event handlers) marks the walkers of the
  overlapping things (`WallHugObjectReference::entryDistance`) and applies `wall_hug::OnCircleGone` to each walker
  holding one of the thing's circles. The walker's own guard against a thing that went is gone: until the map lets a
  destroyed thing go, the walker goes on with the circle as it found it (`PathfindingGoneObstacleTest`:
  `AnOrbitWhoseCircleGoesHeadsStraightOnTheSameWayRound`, `AnExitWhoseCircleGoesLetsGoOfWhatItFindsNext`,
  `AStraightLineWhoseCircleGoesLooksAgainWithTheStepItHas`, `AnyOtherWalkOnlyLetsGoAndOtherCirclesStay`,
  `TheMapTellsWhenAThingsCollideDataGoes`, `AThingOfManyCellsThatGoesLetsItsOverlappingNeighboursBeLeft`). The
  animals follow it too: `animal_ai::OnCollideDataReleased`, on `events::CollideDataReleased`, runs
  `detail::OnHuggedCircleGone` for every animal hugging a circle of the thing, `detail::MarkLeaveAtOnce` on the
  event's `overlapping` before the let-go, and `Orbit` clears an entry distance of 0xFFFF or more as it leaves.
- **The look-ahead flag** is `WallHug::needsLookahead`, cleared at the start of each step, set at the line's hand-over
  and at its own orbit's hand-over in the same way (`AWalkerThatReachesWaterIsOneToTryBothWaysRound`,
  `AWalkerThatReachesAThingIsNotOneToTryBothWaysRound`); nothing reads it until the look ahead is ported. The pure
  rules are `wall_hug::TurnsToSpare`, `TurnedRoundIsBetter`, `TurnRound` (with the step's height part, pinned in
  `WallHugRules.AWalkerTurnedRoundJumpsThreeStepsOnAndFacesBack`) and `HeadingRound`; and, for the animals' copy,
  `wall_hug::RunTrial`, `DidBetter`, `TurnRound` and `HeadingWhenKept`. Nothing runs them yet.
- **The animals' walk** (`ECS/AnimalWallHug.*`): the animals' move states are the shared ones
  (`AnimalBrain::moveState` holds a `wall_hug::WalkState` number, and the `k_Move*` names of `AnimalAIDetail.h` and
  `AnimalWallHug.h` are its values). `CellCircles` walks `map_cells::ForEachFixed`, the cell's fixed list from its
  head, which keeps the original's head insertion, and takes the circles from `thing_circles::CirclesInCell`, each
  child circle of the shape when it has any, else its one circle; a multi-cell object is in the cells of its descriptor
  (`map_cells::DescriptorCells`: the 7.1 m circle of each cell against the shape is the original's own test).
  `CellCircles` walks `k_LandscapeCellOrder` over the cell index wrapped to 16 bits, as the original, and stands each
  circle at `LandscapeCircleCentre`. In Land 2 with a pile next to them (`OPENBLACK_TEST_FOOD_PILE`) only 2 of 25
  hungry sheep get to eat, and the food reaction lasts 2000 turns. The plan to bring the two walks onto one is in the
  refactor plan of fork issue #82; openblack's port for villagers (`PathfindingSystem`) has its own bugs.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/AnimalWallHug.cpp`: `HugObj`, `SetObjectPtr`, `SetFinalStep`, `WaterCell`, `CellCircles`, `MakeLinearSweep`, `LinearSquareSweep`, `AimAlongOrbit`, `ExitCircle`, `AnimalBrain::HugCircle`.
- `src/ECS/AnimalWallHug.h`: `HugMoveTo`.
- `test/mobile_wall_hug/test_mobile_wall_hug.cpp`: `ExeReaimOf`, `Locator::pathfindingSystem::value`.

### Test hooks

- `OPENBLACK_TEST_FOOD_PILE` (a food pile next to the animals, for their walk to it).
- The test `test/mobile_wall_hug/test_mobile_wall_hug.cpp` runs the walk's cases.

## Pending

- **The walk round a circle** is still openblack's float orbit (`PathfindingSystem::Step`'s phases): the original's
  `MoveTo` handlers in whole map units and its look along a circle (`CircleSquareSweep`) are port row N-C-16 B1c. With
  that look come two rules of the circles that openblack's orbit has no place for yet: a water or land's edge circle
  never replaces a nearest end of the arc that is the goal's mark (MobileWallHug.cpp:1542), and the hand-over's
  three-deep budget. Two parts of the release wait for it too: a walker leaving a circle that goes keeps the turns its
  sweep found after letting go of the circle, and its LINEAR handler sweeps again from its position on its next turn
  (`TurnsToObj` != 0xFF with no circle, 0x60B095), where openblack's walker keeps no turns without a circle and looks
  again only at its next change of cell; and the entry distance the release's flood sets
  (`WallHugObjectReference::entryDistance`) is read by the original's leave test, which openblack's float orbit does
  not have (it leaves as if the distance were always reached).
- **The bit-for-bit traces**: neither have `GoesRoundClockwise`'s operand order at 0x60D985 and `AreWeThere` 0x60AD60 been traced bit for bit.
- **Trying both ways round** is not wired into the walk. It needs the villagers' walk as the original's (B1c). The
  circles of water and the land's edge are in, and the walk keeps the flag. Wiring it also needs:
  - a stand-in that is made exactly as `Villager::Create` makes one (its draws, its creation number, the named
    villager);
  - the waiting walkers taken in some order. The original takes the walker at the lowest heap address, which cannot
    be reproduced; the lowest entity is the nearest stand-in, and that remains to be decided;
  - the step's height part kept on the walker.
- **A thing that leaves the map** through openblack's once-a-turn `map_cells::Sync` (an owner without its own hook: a
  deletion, a move, the hand or the physics) lets its walkers go at that turn's sync, not at the moment it went: the
  map cells' own approximation, gone with each owner's hook. Still inferred: an object whose owner calls no hook is
  filed by `map_cells::Sync` in creation index order, so its place in the cell's list is not proven.
- openblack differs: the citadel heart and the worship site build their collide data with their own
  `CreateCollideData` (`CitadelHeart` 0x468FBD, `WorshipSite` 0x77E499), with a radius per child circle (the heart: a
  15.05 m circle at its centre and 32 circles of 3.311 m on 7 spokes; the worship site: 10 circles of 1.82 / 2.6 /
  3.9 m from a fixed table, no centre circle; see [Temple and citadel](../magic/temple-and-citadel.md)), where
  `map_collide::Shape` has one radius for every child and builds both from the mesh's box (`src/ECS/MapCollide.h:26`),
  a fix row (then both shapes, with the temple's ring of port row N-C-16 B3).
- **The animals' units**: the original holds Pos and the goal in whole map units and steps by adding whole units.
  openblack keeps the animal in its Transform in metres and adds step / 6553.6f (`MoveBy`, AnimalAI.cpp:585-600), with
  the goal in metres (AnimalBrain.h:40), so the position drifts from the original's. Then, each against the exe:
  `AreWeThere` is integer differences and `<`, ours metres and `<=` (AnimalAI.cpp:502-507); whole units to metres is
  ours / 6553.6f (`StepMetres`, AnimalWallHug.cpp:84-87); a unit vector is ours x / length (`LinearSquareSweep`,
  `CircleSquareSweep`); the orbit sweep's turns are ours in double (AnimalWallHug.cpp:507-508); the handover side,
  the leave, exit and goal-inside tests measure ours in metres (`Orbit`, `ExitCircle`, `MoveToCircleHug`). The
  headings already go through MapCoords (`gutils::GetAngleFromXZ`).
- **The animals' extra checks**: two openblack checks that the original does not do: no circle at the start of the
  orbit or the sweep, only possible if its object was deleted. +0x76 is always set to 0 when preparing the walk (the
  original only with an entry in `ExtendedEntryDistances`; nobody reads it before rewriting it) and the field +0x78
  (1 / 0x10) is not carried (it is read only on a villager's STEP_THROUGH, 0x60B02A).
