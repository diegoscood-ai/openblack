# Routes and footpaths

## Summary

A villager walking to an object may take a footpath: the object's footpath link finds the nearest path within 40 m,
the villager walks to its first node and then from node to node (state 29 MOVE_ON_PATH), with a little random spread
at each node. Footpaths are made and bent round obstacles by the route planner (RPlan), a best-first search over
routes that go straight between obstacle circles and round them along their arcs; its obstacles (RPHolder) are kept
in a grid of 80 m squares and added by each object (the temple's heart and arms, the Norse gate's row of circles).
The creature follows its routes with its own follower (RPFollow).

openblack ports the footpath queries, the walk from node to node (row 29), the planner, its obstacles (with the temple
and the animated statics) and the creature's follower; the walk to an object does not use the footpath yet (the
object branch of the mover is still to wire).

## The original

### Routes and footpaths

#### The footpath and its nodes

- A footpath is made by `GFootpath::GFootpath` 0x534EB0: active, no node, pushed at the head of the game's list
  g_game +0x205C64 (0x534ED0..0x534EE9, the count +4 incremented). Every maker of footpaths goes through it: the map
  script's CREATE_FOOTPATH, the .fot load (GFootpath::LoadAll) and the planner's routes. fn_00534F90 gives a
  footpath's place in that list from the head (0 = the newest; next +0x1C), −1 if absent; its other callers are the
  footpath and link saves.
- A node is `GFootpathNode(coords, p, q)` 0x534CF0: its coords at +0x18 and the flag bits 0 (p) and 1 (q); bits 2
  and 3 hide it. AddPos 0x534FC0 inserts a node at the head. A Living keeps its node pointer (+0xCC), valid while the
  footpath lives: the only unlink, 0x535440, has no caller. The Livings on a node are its list +0x28 / +0x2C, head
  first. `GFootpathNode::GetDistance` 0x534D90 (named `LHFastPointer::Get` by the symbols) is
  GetDistanceInMetres(node +0x18, at). `GFootpathNode::ToBeDeleted` 0x538970 frees the node's entries, then the node.
- The queries (Footpath.cpp):
  - `GFootpath::GetNextNode`(node, dir) 0x5351A0: dir ≠ 0 the node before it in the list (none before the head,
    0x5351B3), dir = 0 the one after (+0x24) (0x5351AA..0x5351CA); a hidden one (bit 2 or 3) is stepped over in the
    same direction (0x5351D1..0x5351DE).
  - GetEndNonHiddenNode 0x535120.
  - `GFootpath::GetNearestPos`(from, dir) 0x5352C0: best = 1e7 (0x4B189676); each non-hidden node from the head whose
    distance is strictly below the best is kept (0x5352CE..0x535304); then next = GetNextNode(nearest, dir), taken
    when GetDistance(next, nearest's coords) > GetDistance(next, from) (fcomp; test ah, 0x41) (0x53530A..0x53533D);
    none without a node.
  - fn_005353F0(from, float& best): the nearest non-hidden node strictly below `best`, `best` updated
    (0x5353F2..0x535429); none for none.
  - fn_005351F0(pos, &node, &out, dir, maxDist) 0x5351F0: n = GetNearestPos(pos, dir); none, or GetDistance(n, pos) >
    maxDist → 0; GetNextNode(n, dir) none → 0x2D (the end of the footpath); else node = it, out = its coords, 1.
    fn_00535270(&node, &out, dir) 0x535270 is the same step from a node.
- The link: GetFootpathLink (vt +0x74) is none for a GameThing 0x405170, +0x64 for a MultiMapFixed 0x401620, +0x30
  for a Forest 0x539AF0 and +0xE8 for a Dance 0x50B720. AddFootpath (vt +0x84) of a MultiMapFixed is
  `MultiMapFixed::AddFootpathLink` 0x52EE10: the link (a GameThing, Fixed.cpp line 0x3D2) made on first use, then
  `GFootpathLink::AddFootpath` 0x536070 puts the footpath at the head of the link's list. `GFootpathLink::ToBeDeleted`
  0x536010 (vt +0xC) frees its list entries, not the footpaths.
- `GFootpathLink::GetNearestPathTo` 0x536110 (from, to, best, dir, node): for each active footpath of the link (+0x20,
  from the head), dir = (its tail farther from `to` than its head) ? 1 : 0, then GetNearestPos(from, dir); a node
  strictly nearer `from` than `best` is kept and `best` updated. The last kept, or none. Quick 0x5361F0 is the same
  with the ends' squared distances to `to` (MapCoords::GetMetresDistanceSq 0x605FB0) and the far end as the entry node,
  with no GetNearestPos.
- Loading a link (GFootpathLinkSave's resolve 0x536FA0, from GFootpathLink::LoadAll 0x536C80, per link of the .fot):
  the link goes to the first object of its MapCoords' cell (MapCoords::FindType(ANY) 0x6045C0, from the head) within
  GetMetresDistanceSq < 0.0025 (the double [0x8DE1B8], 0x536FCE / 0x53702E) that is a MultiMapFixed (+0x64); else to
  the first planned abode within it (0.05 m; the towns of g_game +0x205C84 newest first, each town's plans +0x9A8
  oldest first, plan +0x38), and the old link of that slot is deleted. None: the link is deleted.
- A new abode's footpath (Abode::MakeFunctional 0x40487E..0x404947): a new footpath from the abode's arrive position to
  the town's storage pit's (AddPos twice, the head at the pit's), then AttemptRerender(head, tail, none); found →
  AddFootpath on the abode, then on the pit; else the footpath is deleted.

#### The walk to an object and on a footpath

- The walk to an object (SetupMoveToOnFootpath 0x5EDD20): the object's own arrive point is its virtual GetArrivePos
  (vt +0x104: a BigForest's is MultiMapFixed's 0x421FE0, the door GetDoorPos vt +0x864, not `BigForest::GetArrivePos`
  0x439360, which is not virtual). Standing on it (`MapCoords::operator==` 0x605660, the altitude compared too, NaN
  counting as equal: oracle) with the target elsewhere: SetupMoveToWithHug. Else vt +0x80: MultiMapFixed 0x52EEC0
  (Abode, StoragePit, Field, FishFarm, WorshipSite, BigForest, Creche, TownCentre, Wonder, Graveyard...), Forest
  0x53AD60 and GameThingWithPos 0x570350 (Dance, PlannedAbode, Villager...) all do the same: the link (vt +0x74, above)
  → `GFootpathLink::UseFootpathIfNecessary`(living, pos, final, this); none → SetupMoveToWithHug.
- `GameThingWithPos::GetArrivePos` (vt +0x104) by the thing's class:
  - the door (MultiMapFixed::GetArrivePos 0x421FE0 → vt +0x864 `MultiMapFixed::GetDoorPos` 0x52E370): an abode (Abode
    0x401770), a storage pit (StoragePit 0x55CCB0) and every other MultiMapFixed;
  - +0x14 (`MapCoords::Set` 0x603320): GameThingWithPos 0x405260, Creche 0x50A990, Graveyard 0x55DF90, Wonder 0x5611B0,
    TownCentre 0x7449E0, Field 0x529330, FishFarm 0x52C490;
  - WorshipSite 0x77CED0: GetSpecialPos(9) 0x77CEB0 (vt +0x7F8) when it has it (0x77CEE9: it returns 1), else +0x14;
  - BigForest 0x439360 (its own signature, given the living).
- Its callers: the worship walk (GotoWorshipSiteForWorship 0x76BD43 and the four other worship ones), the housewife's
  four, the workshop's three, the hiding and fleeing ones, SetupBreederDisciple, GotoAltarForRest,
  AddMoveToPosWithFootpathReaction 0x5F1067, the dance 0x502147, and the home's.
- `UseFootpathIfNecessary` 0x5362E0: GetNearestPathTo(+0x14, pos, 40) and SetupMoveOnFootpath ≠ 0 → 1
  (0x5362F5..0x536331). Otherwise the owner's town (vt +0x48: an abode's +0x98; a Forest's and a Dance's not read),
  none → pos.GetNearestTown(FLT_MAX [0x8DDF90]) 0x6020E0 (0x536342..0x53635F; the exe calls the owner's GetTown with
  no null check). Nothing within 40 m, no owner and no town → SetupMoveToWithHug(pos, final) (0x536417..0x53642F); the
  footpath fields are not touched.
- `Living::SetupMoveOnFootpath` 0x5EDC10(fp, dir, final, node): +0x7C = 0 (0x5EDC22), +0xC8 = fp (0x5EDC29); out of
  the old node's occupants, every entry of this living (0x5EDC31..0x5EDC66); +0xB4 bit 3 = dir & 1
  (0x5EDC6E..0x5EDC8B); no node → GFootpath::GetNearestPos(this, dir) 0x5352C0 (0x5EDCA3), none → 0 (0x5EDCAC); into
  the node's occupants at the head (0x5EDCB8..0x5EDCD1); SetupMobileMoveToPos(node, 12) (0x5EDCE2: the goal is that
  node); SetCurrentAndDestinationState(29 MOVE_ON_PATH, final) (0x5EDCF2) and on 1 SetAnim(1). Returns that
  (0x5EDD13).
- The move states (MobileWallHug +0x5E) the walk on a footpath gives SetupMobileMoveToPos 0x60ABC0: 12 on the first
  node (0x5EDCD2) and in the reroute (+0x5E = 0xC), 11 between nodes (0x5EDF5B / 0x5EE04A).
- MOVE_ON_PATH (29) is `Living::MoveOnFootpath` 0x5EDDE0 (the villagers' and the animals' tables: address taken at
  0x59EC22 and 0x5AE664), entry EnterMoveOnPath 0x5EDDD0 (1), exit ExitMoveOnPath 0x5EE090 (out of the node, every
  entry; +0xCC = 0; `CircleHugInfo::Reset`; the footpath +0xC8 kept). Each turn: MoveTo 0x60AF20; anything but 0xA is
  returned. On 0xA, with the reverse bit +0xB4 & 8:
  - no node (+0xCC): GetNextPos(Pos, node, out, reverse, 0.03) 0x5351F0 (0x3CF5C28F);
  - a node: out of its followers (every entry), then GetNextPos(node, out, reverse) 0x535270;
  - result 1: into the new node's followers (at the head). A node after it as well (a second 0x535270 on a copy):
    out.x += GameRand(0x2000) - 0x1000, then out.z the same (0x5EDF21, 0x5EDF44, Living.cpp lines 1753/1754, the
    synced stream). SetupMobileMoveToPos(out, 0xB STEP_THROUGH) 0x5EDF64; 1;
  - else, a next footpath (+0x7C): +0xC8 = it; reverse = GetMetresDistanceSq(head, Pos) > (tail, Pos), strict
    (GetMetresDistanceSq 0x605FB0 at 0x5EDFA5 / 0x5EDFB2, on Pos with its altitude); the node = the tail when reverse,
    else the head (raw ends, hidden or not); GetNextPos(node, out, reverse) (its result not read: a one-node footpath
    stays on that node); into its followers; SetupMobileMoveToPos(the node's coords exactly, 0xB) 0x5EE052; +0x7C =
    0; 1. An empty next footpath reads a null node (0x5EDFA2), not reachable in the game as far as known;
  - else +0xCC = 0, SetTopStateToFinal 0x5ECA80, 0xA.
  Oracle runs of MoveOnFootpath on hand-made footpaths (MoveTo stubbed to 0xA) pin the goals, nodes, directions and
  draws.
- Save / load of row 29 are `Villager::SaveFootPath` / `LoadFootPath`.

#### Footpaths round obstacles

- A follower of a node is a living in state 29 MOVE_ON_PATH on that node of that footpath (+0xC8, +0xCC).
  `GFootpathNode::PurgeFollowerList` 0x5389D0 drops the entries whose Living no longer follows the node.
  ClearFromPreviousNode 0x538B10 (after the purge, the followers going away from the head; the buildings code calls it
  from the reroute, 0x5376E5 / 0x53771E) and fn_00538B50 (those going toward it) re-plan their walk and keep their
  entries: +0xB4 & 8 decides which; +0x5E = 0xC (LINEAR) and `Living::MoveToCircleHugLinearSquareSweep`(+0x14)
  0x60CA50 re-plan it from where the living stands towards the goal it has, keeping the footpath, the node and state
  29 ([Walking round obstacles](wall-hugging.md#walking-round-obstacles-mobilewallhug)).
  `GFootpathNode::ClearForDeletion`(b, a) 0x538A40 moves its followers onto a or b by their way (into that node's
  occupants) and frees its own entries.
- `GFootpath::EraseNodesBetween`(a, b) 0x538B90: every node strictly between a and b gets ClearForDeletion(b, a); the
  hidden ones (0xC) are kept in their order, chained after the new ones, and the rest are deleted. With no b, up to the
  tail.
- `GFootpath::AttemptRerenderFootpathWithCreatureRP`(a, b, c) 0x5387D0 plans the stretch between two nodes again; true
  when found, the nodes between replaced through `GFootpath::ConvertCreaturePlanToFootpath`(holder, plan, a, b, c)
  0x538340: the plan's best route as nodes between a and b, the arcs in 18-degree steps (cos [0x8DE2D4], sin
  [0x8DE2D8], floats) while more than 0.35 r (the double [0x8DDFA8]) from the arc's end, the old nodes between deleted
  but the hidden ones kept after the new ones; with c, one more hidden node at c.
- `GenerateRoutesBetween` 0x535850 drives the planner as SetStart(start, 0.5, holder, −1, −1, 0), SetDest(dest, 0, 0,
  0, −1, 0, (range + 1) × 5, 1), then GameTurnUpdate(0) while it searches, at most 128 times (the same run at
  0x535974..0x5359EC and 0x53881F..0x538894). Its ends are each object's virtual GetArrivePos (0x535895, 0x5358AB), a
  big forest's door, each pushed out of its own object toward the other end by fn_00535A80(p, thing, ax, az): a
  holder of the thing's own circles (margin 0.5), p exactly on the thing's point first moved 0.01 toward the other end
  (0x535B09..0x535BA9), then the callback 0x535BD0 moves p, inside a circle (c, r + 0.1), out along the ray from the
  thing's point O (the globals [0xCD0468] O and [0xCD0478] p). Each call puts an RPHolder (0x6402C bytes) on the stack;
  two are live at once (GenerateRoutesBetween's and fn_00535A80's). Its callers are the town artifact 0x426560, the
  dance 0x50B94F and sub_535CD0.
- `GFootpath::SendFootpathsAroundObsticle`(r, pos) 0x537290 (from Fixed / `MultiMapFixed::InsertMapObject` 0x52E558 /
  0x52E744) bends every footpath round a new obstacle; nothing while g_game +0x14 & 0x8000 or `GameOSFile::Loading`
  [0xD01AA0] (0x537290..0x5372C5). On each footpath (0x5372D5..0x537C6B):
  1. the obstacle's box and squared radius in map units, R² × 1.01 (0x5372D5..0x537331);
  2. the inner nodes inside the circle are hidden, a visible copy (bit 1) after each (0x53733B..0x53744F);
  3. the pairs of visible nodes (0x537453..0x5377CE; the next node with no bit of 0xC, 0x5377B3 / 0x538246, on at
     0x5377A6..0x5377CE). A pair whose first node has no bit 1 starts a pending stretch up to fn_00537DD0(n), the first
     node from n (included) with no bit of 0xE. A pair meets the box when fn_00537260(a, b, lo, hi) holds on x and z:
     min(a, b) < hi and lo < max(a, b), signed and strict. One end inside the circle (0x53776C prev, 0x53773C cur):
     when that end has bit 1 and a stretch is pending, AttemptRerender(from, to, &pos) (0x5377D3; 0x5377E6), found →
     this footpath is done with the hidden node c at pos (bits 8 and 2). Both ends outside: the closest point F of the
     segment and h = r² − |Q − F|² (0x5375B1..0x537734); the followers of both ends re-plan (0x537719 / 0x537720); h
     above 0.0001 ([0x8C7CA8], qword) makes a detour: a pending rerender first, else the segment's direction
     (0x5377F6..0x537844), the arc's entry and exit √h either side of F (0x537846..0x537913), the arc limit
     (r × 0.35)² (0x53793B..0x53795E: `fld r; fmul qword 0.35; fld st0; fmul st1`, each product a float, the same as
     ConvertCreaturePlanToFootpath's), the side of the turn (0x537960..0x537999), then nodes on the circle in 18-degree
     steps (cos [0x8DE2C8], sin [0x8DE2C0], qwords) until within 0.35 r ([0x8DDFA8]) of the exit, and the exit itself:
     prev → arc → exit → cur (0x537C50..0x537C57). The nodes that were between prev and cur (only hidden ones) fall
     out of the list and are leaked (0x537C54). The count is recounted and the footpath is done (0x537C5A..0x537C6B).
  Nodes made by the planner have bit 1, so they start no pending stretch.
- `GFootpath::StopGoingRoundObsticle`(r, pos) 0x537DF0 (from RemoveMapObject 0x52E5E7 / 0x52E816) is its undo, on each
  footpath (0x537E28..0x538321), for the nodes within L = 1.03 r + 0.4 ([0x8DDFB4], [0x8DDFB0]; 0x537E28..0x537E74):
  - pass 1 (0x537E58..0x537FA6): after a node with no bit 1, the hidden near nodes of the inner walk are un-hidden and
    the nodes before them erased and re-planned (0x537F79..0x537FA6); the inner walker's last node starts pass 2
    (0x537FBE);
  - pass 2 (0x537FBE..0x538072): the near node with bits 8 and 2 is deleted and the stretch re-planned without it;
  - pass 3a (0x538082..0x538102): the near nodes with bits 8 and 2 (and a node before them) deleted; next footpath;
  - pass 3b (0x538107..0x538318): the near nodes with bit 1. A pending stretch is re-planned first; else the run from
    the node (0x5381E2..0x538257, near by distance only, no box: 0x5381FF → 0x534D90) gives the first node no longer
    near and the last near one; unless fn_00537D10(a, b, c) says the turn is small (0x53825D..0x5382BF) the run is
    replaced and the followers re-plan (0x5382E5..0x538315); the walk goes on where the run stopped (0x5382C1,
    0x5382CB..0x5382E3). When the stretch's two ends are at the same coords the exe goes round again with the same
    node (0x5381DC jne 0x5382C1), which only ends if a rerender succeeds.
- fn_00537D10(a, b, c) → fn_00537CA0(A, B, C) on the world points (x × 10/65536, the ground + altitude, z ×
  10/65536): u = A − C, v = B − C, `(vx uz − vz ux) / ((vz uz + vx ux) + vy uy) < 0.01` ([0x8C7620], qword).
- An RPFollow is 0x640C0 bytes (allocated on the heap).

#### Route plan flags of the objects

- §21: 0 for Living 0x5EDBA0 (villagers, animals) and Creature 0x47D4B0 (no creature given), Field 0x5280C0, FishFarm
  0x52C460, PileFood 0x66E1F0, MapShield 0x72C170, GStreetLantern 0x734D50, Fragment 0x76F7F0.
- `CreatureMustAvoid`: BigForest 0x438F50: only on fire. Object 0x638740, MultiMapFixed 0x52F490, Tree 0x74C0E0,
  MobileStatic 0x609010, ChessPion, LandscapeVortexIn: 1.
- `GetPlanStart`: +0x64044: the next plan's start (SetDest case 1), the route's end while following.

#### The route planner (RPlan)

- The planner lives in RoutePlan.cpp of runblack.exe: RPHolder 0x83B280..0x83C990, RPFollow 0x8639E0..0x864FB1,
  Route / RouteNode 0x869100..0x869451, RPAvoid 0x869020..0x8690FB, RPlan 0x86E0D0..0x86FC00, Point2D
  0x86FD00..0x86FEB0.
- Everything runs with the x87 at 24 bits: every add / sub / mul / div is a float operation in the exe's order;
  fpatan / fsin / fcos keep full precision until the next store.
- Point2D (metres on x / z): length fn_0086FD50 = sqrt(x·x + z·z); `GetNormSq` 0x86FDA0; `GetRange` 0x86FDC0 = sqrt((z
  − b.z)² + (x − b.x)²); `SetSize` 0x86FD70 (f = len / length); `Normalize` fn_0086FE80 (f = 1 / len, returns len).
- `Point2D::GetHeading` 0x86FD00: 0 when x² + z² ≤ 1e-6 ([0x9A2BAC]), else fn_0086FC30(−z, x).
- fn_0086FC30(a, b) = atan2(b, a) by octants: a ≥ b and −b ≤ a → atan(b/a) (0x86FC30..0x86FC5C); b ≥ a and −a ≤ b →
  π/2 − atan(a/b) (0x86FC5D..0x86FC91); −b ≥ a and a < b → atan(b/a) + π, or − π when b < 0 (0x86FC92..0x86FCDC); else
  −π/2 − atan(a/b) (0x86FCDD..0x86FCF3).
- Constants: π [0x9A3C08], π/2 [0xFA51D4] = [0x9A3C08] × 0.5 (fn_0086FC10). The planner's float π is also stored at
  [0x9A3BCC] / [0x9A3BB8].

##### RPHolder (the obstacles)

- A 64 × 64 grid of 80 m squares ([0x9A3AE8]) over the 512 × 512 cell map. Each square keeps a chain of the obstacle
  circles that touch it.
- A square is filled the first time a search reads it, so the chain order follows the search. `FillSquare`
  fn_0083B380 marks the square filled, then calls the fill callback for its 8 × 8 cells (0x83B3A5..0x83B3D3).
- `RPHolder::InitialiseSystem(cb1, cb2)` 0x83B300 (static) installs [0xEDDD14] / [0xEDDD18]: the world's
  `CheckSquareFunction` 0x54AFB0 for each map cell of a square, and `AddSpecialRPObjects` 0x54AF60 (the creature's).
- Layout: +0x50000 object count, +0x50004 / +0x50008 the plan's dest and start (kept outside every obstacle), +0x50020
  the margin `Object::AddToRoutePlan` adds to every circle, +0x50024 the follower (fn_0083B320; null for the
  footpaths' holders), +0x50028 the squares, +0x54028 the chains, +0x64028 the chain count.
- Ctor 0x83B2E0: +0x50024 = 0, then `Empty` 0x83B330 (no objects or chains, no dest / start, margin 0, every square
  unfilled).
- RPAvoid (0x14 bytes, fn_00869020): +0 object id (−1 anonymous), +4 active (0 once a later circle swallows it), +8
  centre, +0x10 radius.
- `RPAvoid::PointIsTotallyInside(p, r)` 0x8690A0: k = (this.r − r) − 0.001, below 0 → 0 (0x8690A4..0x8690C7); true
  when k² > dz² + dx² (0x8690D4..0x8690EF).
- `AddObject(id, c, r, notify)` 0x83B450: an obstacle with the plan's dest or start strictly inside it is not added
  (0x83B45B..0x83B4C7, dest checked first).
- `AddObject` with a follower (0x83B4DD..0x83B517): an obstacle within 0.1 ([0x8AB22C]) of the follower is not added.
  Otherwise its radius is capped at that distance, and the exe overwrites the argument, so every later use sees the
  capped radius.
- `AddObject` then checks the squares it covers (inside the grid, room for chain entries; 0x83B519..0x83B5AF), tests
  every entry already in those squares (0x83B5B5..0x83B6D8), and adds the entry and its chain entries
  (0x83B6DE..0x83B719).
- With notify == 1 it calls the follower's fn_00864D30 (0x83B716..0x83B728), with no null check.
- fn_0083B770 links the entry into every square of its circle's box, z outer, x inner.
- `SquareDoesNotContain(id, cellX, cellZ)` 0x83B3E0: true when no chain entry of that cell's square is `id`.
- `GetSidePointOfStartObject(obj, p, out, side)` 0x83B860: the tangent point from p to the circle. p on the centre →
  the circle's top (0x83B894..0x83B8BA); p inside or on the circle (r² + 0.001 > n²) → pushed out to it
  (0x83B8C4..0x83B920); tangent points at 0x83B923..0x83B9D8.
- fn_0083B9E0(objA, sideA, outA, objB, sideB, outB): the tangent between two circles. No tangent when h ≤ 0
  (0x83BA87..0x83BA9F).
- `GetFirstObject(from, to, ignore, hit, side, r)` 0x83BB60: the first obstacle on from → to, or −1. With r > 0 the
  segment is cut short (0x83BBAF..0x83BC1D). Square 0 counts as off the grid (0x83BCE4 / 0x83C1FB). Jump tables
  0x83C284 (the last square) and 0x83C2A8 (the next square). Nothing hit → hit = from (0x83C23C / 0x83C263).
- fn_0083C2D0(obj, side, end, start, hit): 1 when another circle cuts the arc start → end around obj. Circles apart are
  skipped, concentric ones ([0x9A2BAC] = 1e-6) give 0 (0x83C40C..0x83C45D). The cut shrink is 0.005 ([0x8C7674]).
- The search's obstacle hook [0xFA51D0] / [0xFA51C4] is called as hook(context, id) for an id != −1 (from
  `RPlan::SetDest` 0x86E4BA and `GameTurnUpdate`).

##### RPlan (the search)

- A best-first search over routes that go straight between the obstacle circles and around them along their arcs.
- RouteNode is 0x2C bytes (ctor 0x869150(parent, a, from, to, obj, side)). The copy ctor 0x869100 copies +0..+0x20 and
  sets +0x18 / +0x24 / +0x28 = 0.
- Route is 0x10 bytes (ctor 0x869380: +0 = +4 = +8 = 0, +0xC = 1). +0xC = shortcut done: fn_0086F560 skips the route
  while it is set. The copy `Route(Route*)` 0x8693A0 copies the nodes in order, linked both ways (0x8693A3..0x86940A).
  Dtor 0x869430.
- `RouteNode::GetArcLength` 0x8691C0 uses the PREVIOUS node's circle and side (+0x28). arc == 1 → from this.from to
  this.to; otherwise from the parent's `to` to this.from (0x8691F7..0x869225).
- Node length fn_008691A0: type 1 → the arc, else `GetRange(from, to)`. Cost fn_00869330: the previous node's +0x20 (0
  without one) + the node's own length.
- fn_00869460 deletes every node after `node` and makes it the tail.
- RPlan ctor 0x86E0D0 (+0x50..+0x70 = 0), dtor 0x86E0F0 → `FreeRoutes` 0x86E100. `SetStart(start, r, holder, objA,
  objB, sideB)` 0x86E200. `SetDest(dest, r, near, startCost, destObj, destSide, maxLength, linkHolder)` 0x86E250.
  `GameTurnUpdate(depth)` 0x86E9A0 = one search step.
- `SetDest`: a dest inside an obstacle → DestInside (0x86E319..0x86E35E). Found at once at 0x86E438..0x86E4B5.
  Otherwise it goes around the obstacle one way in this route and the other way in a second route
  (0x86E4EF..0x86E627). Starting on an obstacle is at 0x86E62C..0x86E79F.
- `GameTurnUpdate`: the best route by estimate, where a later route must beat it by 0.001 (0x86E9C8..0x86EA2C). Arc
  from the tip around the tip's object (0x86EB48..0x86EC31), then the leg to the target (0x86EC36..0x86EC8A).
- `GameTurnUpdate`: if obj2 overlaps the tip's circle, go around obj2 the same way (0x86EE64..0x86EF38). If they are
  apart, first check whether another circle cuts the tip's whole way around (0x86EF3D..0x86F07D), then go around obj2
  one way here and the other way in a copy (0x86F07D..0x86F1BB).
- fn_0086E7C0: the best route's tip within `near` of the dest → `Finish` fn_0086E7F0 (frees every route but the best;
  Found).
- fn_0086F390 removes a route from the list; with none left the plan has Failed (0x86F3D2..0x86F413).
- Visited points fn_0086E890(p, cost, limit): 1 rejects (too long, or the point already seen), else it is added.
  Blocks of 256 entries are searched from the newest (0x86E8F4..0x86E942); fn_0086FB80 adds a new block when the
  newest is full.
- Shortcut fn_0086F560(route, depth): plans the route's last stretch again from its tip back and splices the result in
  when found.
- Shortcut details: B is the tip (or its parent when the tip is not an arc) and E goes back to the first arc or the
  route's first node (0x86F5B6..0x86F61E). The nested plan's dest is a zeroed local (0x86F62C) with a dest object. At
  most 16 steps while it searches (0x86F72D, 0x86F726..0x86F761), and 4 tries with an earlier E (0x86F777,
  0x86F770..0x86F7BB).
- Splicing: the splice point is at 0x86F7C0..0x86F8F1, the nested route is reversed onto this one
  (0x86F8F1..0x86FAC6), and the route's last node is set at 0x86FAC8..0x86FB0B. A plan's +0x1C is written only by a
  parent plan's fn_0086F560.
- Fields RPFollow reads from a plan: +0x3C / +0x40 / +0x4C / +0x5C / +0x60 (in fn_00864420, fn_00864040,
  fn_00864D30).

##### The temple in the route plan

The temple itself is on [The temple and the citadel](../magic/temple-and-citadel.md); here is its part in the route
plan. What is drawn inside it is on [The temple interior](../interface/temple-interior.md#the-temple-interior).

- `CitadelHeart::AddToRoutePlan` 0x4680D0 (vt +0x7C0 of the heart): a centre circle of radius 21.5 ([0x8C8428]) × 0.64
  ([0x8CA28C]) + 1.1 ([0x8AB230]) at the heart's MapCoords in metres (× 0.000152588, 0x4680E5..0x46810E), then seven
  arms from the heart's Y angle + 3.83 ([0x9CE958]), 2π/7 apart, with circles of radius 21.5 × 0.11 ([0x8CA280]).
- Arms 0 and 1 have six circles each (three straight, then three turned by ±0.06, ±0.16, ±0.27). The other five arms
  have `count` circles: the heart's player (`CitadelHeart::GetPlayer` 0x468020 through vt +0x1C at 0x46815B: the
  citadel's player when +0x80 is set, else the player of the town at +0x94) is read at +0x60 → +8, its `GAlignment`
  value (0x46815E..0x468161), the same field `GPlayer::GetAlignmentValue` 0x64D6A0 returns. Then `fcom` with −0.7
  ([0x8CA288]): below → 2 (0x468173); else `fcomp` with −0.3 ([0x8CA284]): below → 3, else 4 (0x46817D..0x468192).
  Both tests are strictly below (C0 of `fnstsw`), so −0.7 gives 3 and −0.3 gives 4. There is no null check on the
  player.

##### The animated statics in the route plan

- `AnimatedStatic::AddToRoutePlan` 0x422ED0 is labelled `ChessPion::AddToRoutePlan` in the symbol map, as are other
  functions of AnimatedStatic's code (0x422180..0x423105; `Draw` 0x422770 is too). It tests for the Norse gate, so it
  is the gate's, and the chess pieces' too (it is at vtable +0x7C0 of AnimatedStatic (0x8C1A10) and of the six chess
  classes alike: King, Queen, Mad, Knight, Tower, Pion; newer symbol maps name it `AnimatedStatic::AddToRoutePlan`). It
  takes (holder, creature, notify, callback). It never reads the creature, and never adds the holder's margin
  (+0x50020).
- `fn_004230D0`: 1 when the 3D object's mesh (obj +0x40, vt +0xF8) is entry 212 of the mesh list ([0xE9FE34] + 4,
  entry 0 when the list has 212 entries or fewer), `BuildingNorseGate`; otherwise 0. Of the 16 animated-static records
  in info.dat (file 0x3E2BA, 0x11C bytes each, mesh at +0x110) only the Norse gate names mesh 212 (plinth 365, Piper
  cave 155, chess pieces 16/19/25/28/32/38, phone box 304).
- The gate (0x422EF8..0x42303B):
  - step = `GetScale` (vt +0x120) × the mesh's half width (+0x24) × [0x8C2C2C] (0x422F04..0x422F1C; 0.13333334, bytes
    89 88 08 3E in runblack.exe), then the world matrix (`GetWorldMatrix`, vt +0x63C; `Object::GetWorldMatrix`
    0x638200 for an AnimatedStatic).
  - 15 circles, i = 0..14 (0x423022..0x42302E). An odd i adds the step to the running offset and uses it, an even i
    uses its negation, so i = 0 is the middle (0x422F3A..0x422F53).
  - The centre is row 0's (x, z) × the offset + the translation's (x, z) (0x422F5F..0x422F7B). The translation is the
    map coordinates in metres: `Object::GetWorldMatrix` 0x638200 loads +0x14 and +0x18 with `fild` and multiplies each
    by [0x8AA3A4] 0.000152587890625 (10 / 65536) (0x638224..0x638239), so the middle sits on the fixed grid.
  - The radius is 2.5 (0x40200000, pushed at 0x422FA4 / 0x422FCA / 0x422FE6 / 0x42300C).
  - While the gate is open (+0x7C == 1) and `IsMoving` 0x422CD0 gives 0, the circles i ≤ 4 are left out
    (0x422F5C..0x422F91).
  - Each circle goes to the callback(object, centre, 2.5, notify) when there is one (0x422FB6 / 0x422FF8), else to
    `AddObject`(object, centre, 2.5, notify) 0x83B450 (0x42301D). Nothing else is added: the function returns after
    the loop, so the row stands in for any bounding circle.
- Any other animated static (0x42303E..0x4230C0): one circle at the world matrix's translation (x, z). Its radius is
  the mesh's half diagonal (+0x30, unscaled) × 0.7 ([0x8AB238], 0x42304D..0x423050), through the callback or
  `AddObject`.
- The gate's place and resting place move with its clip on the frames it is drawn (`AnimatedStatic::Draw`, see
  [the map script reader](../scripts/map-script-reader.md)).

##### RPFollow (the creature's route follower)

- RPFollow is the creature's: Init from its ctors 0x47F527 / 0x47F80C, Update from fn_0048D250, MoveAlongRoute from
  fn_004907C0. It is its own RPHolder (+0..+0x6402B) and plans with up to 5 RPlans (the main one and the re-plans of
  parts of the route ahead).
- Ctor 0x8639E0: RPHolder, then the fields (+0x6404C = 1.0, +0x6403C = 5.0, +0x6405C = 1, the rest 0), then
  fn_0083B320 sets the holder's follower to itself (0x863A5F).
- Layout: +0x6402C position, +0x64034 speed, +0x64038 distance along the current node, +0x6403C (5.0, reader not
  known), +0x64040 heading, +0x64044 next plan's start (the route's end while following), +0x6404C dest radius.
- Layout (continued): +0x64050 last step, +0x64054 state, +0x64058 dest pending, +0x6405C turns per update, +0x64060
  context, +0x64064 / +0x64068 / +0x6406C callbacks, +0x64070 current node, +0x64074 dest, +0x6407C / +0x64080 /
  +0x64084 SetDest's a / b / c.
- Layout (end): +0x64088 found once (1 at a found end, 0 at a new plan), +0x6408C the adopted main plan, +0x64090..
  plans, +0x640A4.. the route node each re-plan starts from, +0x640B8 plan count, +0x640BC best plan (−1 after case 1).
- Callbacks: +0x64064 done(context, code), with code 2 = re-planning after a found / dest-inside end, 1 = after a
  failed one, 0 = no pending dest and found, 3 = no pending dest and failed. +0x64068 first step(context, 0, the first
  node's length ≥ 0.01). +0x6406C the radius ahead (the creature's nav radius).
- `Init(context, done, step, radius, turns)` 0x863AC0 (0x863AC0..0x863AF2).
- The creature's fn_0048D250 (0x48D3BF) writes +0x64054 = 1 before its first `SetDest`.
- `SetDest(dest, a, b, r, c)` 0x863B70: +0x64084 = c first (0x863B7E..0x863B8E), then r → +0x6404C (the creature
  passes `GetNavRadius`, 0x4841BE). c is the plans' max length.
- `SetDest` by state: case 1 makes a new main plan (0x863C94..0x863D57). Cases 2 / 4 re-plan when d1 × 0.5 < d2, with
  d1 = |plan[0] dest − follower| and d2 = |plan[0] dest − new dest| (0x863BB8..0x863C77, 0x863BE0..0x863C29): the dest
  moved by more than half the remaining way.
- `SetDest` case 3 re-plans the parts of the route ahead, every quarter of what is left (0x863D5C..0x863ED1): always
  the tail, otherwise while fewer than 4 re-plans (0x863DE6) and the node's cost is at least the threshold
  (0x863DE2..0x863E00).
- `Update` fn_00864040(hook, turns): up to `turns` (or Init's) search turns of the best plan. The hook is installed for
  these turns only (0x864053, with the creature as context) and cleared at the end (0x86415D / 0x86416D).
- `Update`: the best plan is the lowest estimate, a later one only when strictly lower (0x86408C..0x8640CB); state
  jump table 0x864178. With a hook and the dest farther than the plan's worst rejected cost, the plan is given up
  (0x8640FF..0x864134). While re-planning it waits for the next turn (0x86413F).
- `OnFailed` fn_008641F0: states 0, 1 and 3 do nothing. A pending dest is planned now; done(2), or done(1) for a failed
  plan (0x86423D..0x864284). Without a pending dest the code is ((failed != 4) − 1) & 3: 0, or 3 for a failed plan
  (0x864290..0x8642A6).
- `OnFound` fn_008642C0: +0x64088 = 1, then by state (jump table 0x864404; 0, 1 and 3 return at once, 0x8643FF). The
  main plan becomes the route (0x8642E4..0x864396). The next plan's start = the route's end (0x86431F..0x86432E). The
  first node's length is at least 0.01 (0x864361..0x86438D). Then the splice (0x864398..0x8643AD). With no plan left
  and a pending dest, it plans now (0x8643B7..0x8643FA).
- The exe moves the plans' RouteNodes into its route by pointer and keeps the adopted plan (+0x6408C) only as their
  owner.
- `FreePlans` fn_00864190: ~RPlan and delete of every plan, count = 0.
- Splice and shortcut fn_00864420(at), steps 1-6: delete every node after `at` (0x86445F..0x86446F); `at` → the
  sub-plan's route, the route's tail = its tail (0x864478..0x86448C); the main plan's end = the sub-plan's
  (0x864495..0x8644D1); r = the radius ahead (0 without the callback) and L = the current node's length
  (0x864517..0x864557); the look-ahead point P (0x864561..0x8645EE); the current node starts at the follower, and so
  does the previous one at both ends (0x8645F3..0x86463F).
- Splice and shortcut, steps 7-8: a local plan from P (0x864642..0x864668) to the target's end, cost from the current
  node + L (0x86468E..0x8646BE), up to 4 candidates (0x864683) and up to 32 turns while searching (0x8646CA,
  0x8646C3..0x8646F4; jump table 0x864894: 2 → a turn, 3..5 → stop). SetDest's near distance is 0.001 (0x3A83126F).
  If found and the plan's tip is the target's circle (or none), that is the shortcut (0x8646F6..0x864719).
- Splice and shortcut, steps 9-12: the new tip takes the target's side, circle and place (0x86471F..0x86476D); the
  route starts with the new nodes and +0x64044 = its tail's end (0x864770..0x8647A7); with the look-ahead, a straight
  node from the follower to P goes first (0x8647AF..0x86480C); the current node is the head again and the old nodes up
  to the target go (0x86480F..0x86484D); then the next candidate (0x86484F..0x86485E).
- `MoveAlongRoute` 0x864990: step = 0.1 ([0x9A3BBC]) × speed (0x864990..0x8649B0).
- `MoveAlongRoute`: still on this node → `PositionAt` (0x864B08..0x864B27). The first re-plan's node, once behind, is
  dropped (0x8649F3..0x864A62). Moving to the next node deletes the previous one; the new head gets +0x1C = 0 and its
  start = its end (0x864AAE..0x864AEA). The route's end is at 0x864B31..0x864B9B.
- `PositionAt` fn_00864BA0(&+0x6402C, &+0x64040, t): an arc with no circle before it is treated as straight
  (0x864BAC..0x864BBF).
- `PositionAt` on an arc: θ = ta + (tb − ta) × t / length (0x864C6D..0x864C79); the heading is θ + π when going the
  other way around (side 2, 0x864C81..0x864C99); position = c + (cos θ, sin θ) × r with fcos / fsin at full precision
  (0x864C9B..0x864CB4).
- `PositionAt` on a straight node: only when longer than 0.001 (0x864CBD..0x864D24); heading from
  `Point2D::GetHeading` 0x86FD00.
- fn_008694A0(list, entry, from, holder, &pos): does the route from `from` meet the new obstacle? Each straight node
  from its start (the follower's position for `from`) to its end; the first obstacle on it, ignoring the circle before
  it (0x8694CB..0x86951E). The hit must be this entry (holder + index × 0x14, 0x869514..0x86951C).
- On a hit, n ends there, the route is cut after it (0x869582..0x8695A9). An arc on the previous node's circle means
  the new circle overlaps it (0x869520..0x86956B). The first node ends at the follower (0x8695AC..0x8695D5), then the
  cut (0x8695D8..0x8695F1).
- `OnObjectAdded` fn_00864D30 (from `RPHolder::AddObject` with notify 1, 0x83B728): state 1 → nothing. State 4 → the
  main route, then the re-plans; 2 / 6 → only the re-plans; 3 → the route (0x864DD8..0x864DEE).
- `OnObjectAdded`: if the main route meets it, a fresh plan from its tail (0x864DF4..0x864E80). Each plan's routes
  (+0x60, next +8) are tested from their first node (0x864EA9..0x864ECE); all plans are freed on a hit. In state 3, the
  route's end becomes the next plan's start and SetDest re-plans ahead (0x864F3F..0x864FA7).

- `Emit`: 0x6389BA / 0x638AA6 / 0x638B9F: the callback when given (fn_00535A80's push-out), else AddObject
- `CitadelHeartAddToRoutePlan`: 0x4680D9..0x46810E: the position x 10 / 65536 (GetAltitude computed and dropped)
- the GGame ctor 0x54B7CA: InitialiseSystem(CheckSquareFunction, AddSpecialRPObjects); LandAvoid itself is the central land_avoid (Game::LoadLandscape)
- The push-out callback of fn_00535A80 (callback 0x535BD0): instead of AddObject, each circle the object would add
- `GetLength`: fn_0086FD50: sqrt(x x + z z)
- SetSize(len) 0x86FD70: f = len / GetLength, x *= f, z *= f
- fn_0086FE80: len = GetLength, f = 1 / len, x *= f, z *= f; returns len
- GetHeading 0x86FD00: 0 when x x + z z <= 1e-6 [0x9A2BAC] (test ah, 0x41); else fn_0086FC30(-z, x), atan2(x, -z) by octants. fpatan keeps its full precision; the divisions are rounded (24-bit FPU)
- fn_0086FC30(a, b): atan2(b, a) in four octant pairs, [0xFA51D4] = [0x9A3C08] x 0.5 (fn_0086FC10)
- `RPFollow::RPFollow`: 0x8639E8..0x863A59: the fields' initial values are the members' (+0x6404C 1.0, +0x6403C 5.0, +0x6405C 1, the rest 0); 0x863A5F fn_0083B320: the holder's planner is this one
- `RPFollow::Init`: 0x863AC0..0x863AF2
- RouteNode::GetArcLength 0x8691C0: the circle and the side are the PREVIOUS node's (+0x28)
- `RPFollow::ArcLength`: 0x8691F7..0x869225: arc == 1 -> from this.from to this.to; else from prev.to to this.from (not reachable from fn_008691A0)
- `RPFollow::Length`: fn_008691A0: type 1 -> the arc; else GetRange(from, to)
- fn_00869330: the previous node's +0x20 (0 without one) + the node's own length; type 0 straight, else the arc
- fn_00869460: every node after `node` deleted, its +0x24 = 0, the list's tail = it
- fn_008694A0(list, entry, from, holder, &pos): the nodes from `from` on
- `RPFollow::RouteMeets`: 0x8694CB..0x86951E: the straight node from its start (the follower's position for `from`) to its end; the first obstacle on it, ignoring the circle before it
- `RPFollow::RouteMeets`: 0x869514..0x86951C: the hit is this entry (holder + index x 0x14 == entry)
- `RPFollow::RouteMeets`: 0x869582..0x8695A9: n ends at the hit, round it, and the route is cut after it
- `RPFollow::RouteMeets`: 0x869520..0x86956B: an arc on the previous node's circle: the new circle overlaps it
- `RPFollow::RouteMeets`: 0x8695AC..0x8695D5: the first node ends at the follower
- `RPFollow::RouteMeets`: 0x8695D8..0x8695F1
- fn_008694A0 on a plan's route, without the cut (RouteMeets above); `to` is a copy: the plan is freed on a hit
- `RPFollow::PositionAt`: fn_00864BA0(&+0x6402C, &+0x64040, t)
- `RPFollow::PositionAt`: 0x864BAC..0x864BBF: an arc with no circle before it is straight
- `RPFollow::PositionAt`: 0x864BE6..0x864CBA
- `RPFollow::PositionAt`: 0x864C6D..0x864C79: theta = ta + (tb - ta) x t / length
- `RPFollow::PositionAt`: 0x864C81..0x864C99: the heading turns a half when going the other way round
- `RPFollow::PositionAt`: 0x864C9B..0x864CB4: fcos / fsin in full precision, x r, + c
- `RPFollow::PositionAt`: 0x864CBD..0x864D24: straight, only when longer than 0.001
- `RPFollow::PositionAt`: Point2D::GetHeading 0x86FD00
- `RPFollow::FreePlans`: fn_00864190: ~RPlan + delete of plan[0..count), count = 0
- `RPFollow::OnFailed`: fn_008641F0: states 0, 1 and 3 -> nothing
- `RPFollow::OnFailed`: 0x86423D..0x864284: the pending dest planned now; done(2) unless the plan failed (1)
- `RPFollow::OnFailed`: 0x864290..0x8642A6: ((failed != 4) - 1) & 3: 0, or 3 for a failed plan
- fn_008642C0: +0x64088 = 1, then by the state (jump table 0x864404: 0, 1 and 3 return at once, 0x8643FF)
- `RPFollow::OnFound`: 0x8642E4..0x864396: the main plan becomes the route
- `RPFollow::OnFound`: 0x86431F..0x86432E: the next plan's start = the route's end
- `RPFollow::OnFound`: 0x864361..0x86438D: the first node's length, at least 0.01
- `RPFollow::OnFound`: 0x864398..0x8643AD
- `RPFollow::OnFound`: 0x8643B7..0x8643FA: no plan left and a pending dest -> planned now
- `RPFollow::SpliceAndShortcut`: fn_00864420(at)
- `RPFollow::SpliceAndShortcut`: 1. 0x86445F..0x86446F: every node after `at` deleted
- `RPFollow::SpliceAndShortcut`: 2. 0x864478..0x86448C: at -> the sub-plan's route, the route's tail = its tail
- `RPFollow::SpliceAndShortcut`: 3. 0x864495..0x8644D1: the main plan's end = the sub-plan's
- `RPFollow::SpliceAndShortcut`: 4. 0x864517..0x864557: r = the radius ahead (0 without the callback); L = the current node's length
- `RPFollow::SpliceAndShortcut`: 5. 0x864561..0x8645EE: the look-ahead point P
- `RPFollow::SpliceAndShortcut`: 6. 0x8645F3..0x86463F: the current node starts at the follower; so does the previous one (both its ends)
- `RPFollow::SpliceAndShortcut`: 7. 0x864642..0x864668: a local plan from P
- `RPFollow::SpliceAndShortcut`: 0x86468E..0x8646BE: to the target's end, its cost from the current node + L
- `RPFollow::SpliceAndShortcut`: 0x8646C3..0x8646F4: up to 32 turns while searching (jump table 0x864894: 2 -> a turn, 3..5 -> stop)
- `RPFollow::SpliceAndShortcut`: 8. 0x8646F6..0x864719: found, and the plan's tip is the target's circle (or none): the shortcut
- `RPFollow::SpliceAndShortcut`: 0x86471F..0x86476D: the new tip takes the target's side and circle and its place before what follows
- `RPFollow::SpliceAndShortcut`: 0x864770..0x8647A7: the route starts with the new nodes; +0x64044 = its tail's end
- `RPFollow::SpliceAndShortcut`: 9-10. 0x8647AF..0x86480C: with the look-ahead, a straight node from the follower to P first
- `RPFollow::SpliceAndShortcut`: 11. 0x86480F..0x86484D: the current node is the head again; the old nodes up to the target go
- `RPFollow::SpliceAndShortcut`: 12. 0x86484F..0x86485E: the next candidate
- fn_00864040: the hook installed for this update only, with the creature as its context
- `RPFollow::Update`: 0x86408C..0x8640CB: the plan with the lowest estimate (a later one only when strictly lower)
- `RPFollow::Update`: jump table 0x864178
- `RPFollow::Update`: 0x8640FF..0x864134: with a hook and the dest farther than the worst rejected cost: given up (0x864119..0x864121: range <= worst -> the failure end)
- `RPFollow::Update`: 0x86413F: while re-planning, the next turn
- `RPFollow::Update`: 0x86415D / 0x86416D: the hook cleared
- `RPFollow::MoveAlongRoute`: 0x864990..0x8649B0
- `RPFollow::MoveAlongRoute`: 0x864B08..0x864B27: still on this node
- `RPFollow::MoveAlongRoute`: 0x8649F3..0x864A62: the first re-plan starts from this node, which is now behind: it goes
- `RPFollow::MoveAlongRoute`: 0x864AAE..0x864AEA: the previous node deleted; this one is the head, its +0x1C = 0, its start = its end
- `RPFollow::MoveAlongRoute`: 0x864B31..0x864B9B: the route's end
- `RPFollow::OnObjectAdded`: fn_00864D30: state 1 -> nothing.
- `RPFollow::OnObjectAdded`: 0x864DD8..0x864DEE: state 4 -> the main route, then the re-plans; 2 / 6 -> the re-plans only; 3 below
- `RPFollow::OnObjectAdded`: 0x864DF4..0x864E80: the main route meets it -> a fresh plan from its tail
- `RPFollow::OnObjectAdded`: 0x864EA9..0x864ECE: fn_008694A0 on each route of the plan (+0x60, next +8), from its first node. Its cut of the plan's route does not matter: the plans are all freed on a hit
- `RPFollow::OnObjectAdded`: 0x864F3F..0x864FA7: the route meets it -> its end becomes the next plan's start and SetDest re-plans ahead
- `DoneFn`: +0x64064 `void (*)(int, int)`: the plan's end. code: 2 = re-planning after a found / dest-inside end, 1 = after a failed one, 0 = no pending dest and found, 3 = no pending dest and failed (fn_008641F0)
- Init(context, done, step, radius, turns) 0x863AC0
- fn_00864040(hook, turns): up to `turns` (or Init's) search turns of the best plan; the hook installed for them only
- The creature's fn_0048D250 (0x48D3BF) writes +0x64054 = 1 before its first SetDest
- `GetHeading`: +0x64040: the heading fn_00864BA0 gives
- fn_008691A0: a node's length (arc 1: GetArcLength 0x8691C0, else GetRange)
- RouteNode::GetArcLength(holder) 0x8691C0, on the previous node's circle and side
- fn_00869460: every node after `node` deleted, the route's tail = node
- fn_008694A0(list, entry, from, holder, &pos): the route from `from` meets the new obstacle; it is cut there
- fn_008694A0's test on a plan's own route (the plans' routes in OnObjectAdded), without the cut
- fn_00864420(at): the finished re-plan spliced in at `at`, then the shortcut search ahead
- `s_fillSquare`: [0xEDDD14] / [0xEDDD18] (InitialiseSystem 0x83B300)
- `RPHolder::FillSquare`: 0x83B3A5..0x83B3D3: z outer (sz x 8 + i), x inner (sx x 8 + j)
- `RPHolder::Link`: fn_0083B770: the box's squares, z outer, x inner
- `RPHolder::GetSidePointOfStartObject`: 0x83B894..0x83B8BA: p on the centre -> the circle's top
- `RPHolder::GetSidePointOfStartObject`: 0x83B8C4..0x83B920: p inside or on the circle (r^2 + 0.001 > n2) -> pushed out to it
- `RPHolder::GetSidePointOfStartObject`: 0x83B923..0x83B9D8: the tangent points
- `RPHolder::GetTangent`: 0x83BA87..0x83BA9F: no tangent
- `RPHolder::GetFirstObject`: 0x83BB6E..0x83BB9C
- `RPHolder::GetFirstObject`: 0x83BBAF..0x83BC1D: cut r short
- `RPHolder::GetFirstObject`: 0x83BC3A..0x83BCE0: the squares and the direction code
- `RPHolder::GetFirstObject`: 0x83BCE4 / 0x83C1FB: square 0 counts as off the grid
- `RPHolder::GetFirstObject`: 0x83C204..0x83C239: the nearest of the square
- `RPHolder::GetFirstObject`: 0x83BF05..0x83BFB6 (table 0x83C284): the last square
- `RPHolder::GetFirstObject`: table 0x83C2A8: the next square
- `RPHolder::GetFirstObject`: 0x83C23C / 0x83C263: nothing on the way
- `RPHolder::ArcBlocked`: 0x83C40C..0x83C45D: apart -> skip; concentric -> 0
- `RPHolder::ArcBlocked`: 0x83C546 / 0x83C697: where the circles cross on the arc's side
- PointIsTotallyInside(p, r) 0x8690A0: k = (this.r - r) - 0.001; k >= 0 and k^2 > |p - c|^2
- The square grid's two callbacks (InitialiseSystem 0x83B300, [0xEDDD14] / [0xEDDD18]): the world's CheckSquareFunction 0x54AFB0 for each map cell of a square, and AddSpecialRPObjects 0x54AF60 (the creature's)
- The search's obstacle hook [0xFA51D0] / [0xFA51C4]: installed by RPFollow's update (fn_00864040, 0x864053, with the creature as its context) for that update's search turns only, and cleared at its end (0x86415D / 0x86416D)
- RPHolder::InitialiseSystem(cb1, cb2) 0x83B300 (static)
- The hook, when installed: hook(context, id) for an object id != -1 (SetDest 0x86E4BA, GameTurnUpdate)
- `SetMargin`: +0x50020: the margin Object::AddToRoutePlan adds to every circle (0 after Empty)
- `SetEnds`: +0x50004 / +0x50008 (SetDest's last argument): the plan's dest and start, which AddObject keeps outside
- `SetPlan`: +0x50024 (fn_0083B320): the follower this holder is (RPFollow's ctor); nullptr for the footpaths' holders
- fn_0083B9E0(objA, sideA, outA, objB, sideB, outB) : the tangent between two circles
- GetFirstObject(from, to, ignore, hit, side, r) 0x83BB60 : the first obstacle on from -> to, or -1
- fn_0083C2D0(obj, side, end, start, hit) : 1 when another circle cuts the arc start -> end round obj
- `RPlan::NewRoute`: ctor 0x869380: +0 = +4 = +8 = 0, +0xC = 1
- `RPlan::CopyRoute`: 0x8693A3..0x86940A: the first node, then every next one, chained both ways
- `RPlan::GetArcLength`: 0x8691F7..0x869225: arc == 1 -> from this.from to this.to; else from the parent's to to this.from
- `RPlan::DropRoute`: 0x86F3D2..0x86F413: nothing left
- `RPlan::Visit`: 0x86E8F4..0x86E942: the blocks from the newest, each from its first entry
- `RPlan::Visit`: fn_0086FB80: at the newest block, a new one when it is full
- `RPlan::SetDest`: 0x86E438..0x86E4B5: found at once
- `RPlan::Shortcut`: 0x86F5B6..0x86F61E: B (the tip, or its parent when the tip is not an arc) and E (up to the first arc, or the route's first node)
- `RPlan::Shortcut`: 0x86F646..0x86F696
- `RPlan::Shortcut`: 0x86F69B..0x86F721
- the dest argument is a zeroed local (0x86F62C) with a destObj
- `RPlan::Shortcut`: 0x86F726..0x86F761: at most 16 steps while it searches
- `RPlan::Shortcut`: 0x86F770..0x86F7BB: another, earlier E
- `RPlan::Shortcut`: 0x86F7C0..0x86F8F1: the splice point
- `RPlan::Shortcut`: 0x86F8F1..0x86FAC6: the nested route reversed onto this one
- `RPlan::Shortcut`: 0x86FAC8..0x86FB0B
- SetStart(start, r, holder, objA, objB, sideB) 0x86E200
- `GetDestRadius`: +0x3C / +0x40 / +0x4C / +0x5C / +0x60, read by RPFollow (fn_00864420, fn_00864040, fn_00864D30)
- RouteNode(const RouteNode*) 0x869100: +0..+0x20 copied, +0x18 / +0x24 / +0x28 = 0
- `CopyRoute`: Route(Route*) 0x8693A0: the nodes copied in order
- `DeleteRoute`: The Route dtor 0x869430 + free
- RouteNode::GetArcLength(objects) 0x8691C0
- `SetCost`: fn_00869330: cost = parent's + range (arc 0) or arc length
- `DropRoute`: fn_0086F390: the route out of the list; none left -> Failed
- fn_0086E7C0: the best route's tip within `near` of the dest -> Finish
- `k_MinRadius`: < [0x930A58]
- `k_CircleRadiusFactor`: < the double [0x931300]
- PileFood 0x66E1F0 (and MagicFood / PuzzleGrain): the pots whose info potType is PileFood
- `SimpleAddToRoutePlan`: 0x638B62..0x638B9B: +0x14 x 10 / 65536 (GetAltitude computed and dropped)
- `CitadelHeartAddToRoutePlan`: < [0x8CA27C] / [0x8C7C80] / [0x8CA274]
- DeadTree 0x510E30: 0 without a creature (0x510E34 je 0x510E57); with one, on fire and +0x25 & 4 clear (not asked here)
- Tree 0x74A180, MobileObject 0x607410, ScriptHighlight 0x70AE40: SimpleAddToRoutePlan
- `AddToRoutePlan`: MobileStatic 0x608AC0: a fence -> Object's, else Simple
- ConvertMapCoordsToPoint2D 0x535780: x, z x 10 / 65536 (the altitude read and dropped)
- ConvertPoint2DToMapCoords 0x5357C0: ftol(x x 6553.6), ftol(z x 6553.6), altitude 0
- `RPFollow::OnFound`: its nodes are copied
- `StepFn`: +0x64068 `void (*)(int, float, float)`: the first step of a route, (context, 0, the first node's length >= 0.01)
- `RadiusFn`: +0x6406C `float (*)(int)`: the follower's radius ahead (the creature's nav radius) for the shortcut search
- `_state`: < +0x64054
- `_turnsPerUpdate`: < +0x6405C (ctor 1)
- `_done`: < +0x64064
- `_step`: < +0x64068
- `_radius`: < +0x6406C
- `s_obstacleHook`: [0xFA51D0] / [0xFA51C4]

### Sources

- bw1-decomp: `src/Black/Footpath.cpp`, `Living.cpp`; the planner (`RoutePlan.cpp` of the exe) read with the xref
  index; the footpath walk and the arrive test checked with the oracle.

## openblack

**Done**: the footpath queries, MOVE_ON_PATH 29 (fork issue #74, W1 and W2), the footpath setups' move states (0xB
STEP_THROUGH, 0xC LINEAR: N-C-16 B1b), the planner `src/RPlan/` (RPHolder, RPlan, RPFollow, Point2D), its world side
`src/ECS/RoutePlanWorld.cpp`. The planner now lives in `src/RoutePlanner/` under new names: RPHolder is
`route_planner::ObstacleGrid` (`ObstacleGrid::HasObstacleHook`), RPlan is `route_planner::RoutePlan`
(`RoutePlan::State`) and RPFollow is `route_planner::RouteFollower` (`RouteFollower::State::Idle`,
`RouteFollower::State::Replanning`); the notes below keep the file names from before the rename. **Not wired**: the object branch of the walk to an object (W3), the other callers (W4),
the save / load of row 29.

- **The walk to an object**: `footpath_walk::ChooseWalkToObject` holds the choice and `footpaths::UseFootpathIfNecessary`
  the link's, not called yet: SetupMoveToOnFootpath always walks directly (see
  [Assumptions](villager-states.md#assumptions), item 23).
- **The footpaths** (`ECS/Footpaths.{h,cpp}`, `ecs::footpaths`): a footpath is a `components::Footpath` entity and a
  node a `Footpath::Node`, named by its `NodeId` (`Footpath::Node::id`: its creation order in the footpath, never
  reused), which stands for the node pointer a Living keeps.
  - Lists and nodes: `Create` (at the head of the game's list, ordered by `creationStamp`), `FootpathsFromHead` (the
    newest first), `PositionFromHead` (only the tests call it: the saves are not ported), `MakeNode` (a node with the
    footpath's next id), `AddOccupant` (the Livings on a node, head first), `DeleteNode`, `DeleteLink`.
  - Queries, on list places: `GetNextNode` (`NextIndex`), `GetNearestPos` (`NearestIndex`), `NearestNodeBelow`,
    `NextNodeNear`, `StepNextNode`, `NodeDistance`, `GetNearestPathTo`, `GetNearestPathToQuick`.
  - Links: `GetFootpathLink` (`components::FootpathLinkOf`, entt::null for none), `AddFootpath` (the link made on
    first use), `AttachLoadedLink` (a .fot link, within `k_SameSpotSq`), `MakeAbodeFootpath` (called from
    `ECS/Abodes.cpp`; entt::null when none was made), `UseFootpathIfNecessary` (it checks the owner with
    `registry.Valid`, where the exe calls GetTown with no null check).
  - Routes and obstacles: `RunPlan` (the planner run the footpaths' way), `HolderLease` (a small stack of reusable
    obstacle grids, each emptied when taken, for the two live at once), `PushOutOfThing` / `PushOutOfCircle`,
    `AttemptRerender`, `ConvertCreaturePlanToFootpath`, `EraseNodesBetween`, `PurgeFollowerList`,
    `ClearFromPreviousNode` / `ClearFromNextNode`, `ClearForDeletion`; `RerouteFootpathsAroundObstacle` (the
    original's `SendFootpathsAroundObsticle`, per footpath `SendAround` with `SpanMeets`, `FirstClean` and
    `NextVisible`) and `StopReroutingAroundObstacle` (`StopGoingRoundObsticle`, per footpath `StopGoing` with
    `SmallTurn`). The constants (`k_CosStep`, `k_AroundCos`, `k_DetourMin`, `k_StopScale`, `k_AngleTanMax`) are the
    exe's values. The arrive points are `abode_queries::GetArrivePos` (= MultiMapFixed's door) and, for a big forest,
    `object::BigForestGetArrivePos(forest, living)`.
  - Differences: **⚠️ approximate** `SendAround` frees the hidden nodes its arc cuts out of the list (their followers
    moved as for an erased node) where the exe leaks them; `StopGoing` moves on when a stretch's two ends are at the
    same coords, where the exe goes round again (a guard). The loading test of the reroute is taken as clear
    (pending): openblack loads the .fot after the map script, so the land's own objects do not bend its footpaths.
- **Row 29**: the state is `living_footpath::MoveOnFootpath` on the walk's result (MOVE_TO_POS's reading of the step:
  0xA when the walker stands on its goal after FINAL_STEP or ARRIVED), which applies `footpath_walk::ReachNode`'s
  outcome (every entry out of the old node, the new node's occupants at the head, `SetupMobileMoveToPos(goal,
  k_MoveToNextNode)`, the STEP_THROUGH walk since N-C-16 B1b, or SetTopStateToFinal and 0xA); the entry returns 1 as
  EnterMoveOnPath; the exit is `living_footpath::ExitMoveOnPath` (the circle hug reset is openblack's
  WallHugObjectReference dropped, as in the reactions' exit). The oracle runs are pinned in
  `test_footpath_walk_rules`. A BigForest's virtual arrive point (vt +0x104, the door) is
  `living_footpath::GetArrivePos`. Nothing enters 29 yet (W3, below).
- **The temple**: `CitadelHeartAddToRoutePlan` (`src/ECS/RoutePlanWorld.cpp`) reads the owner's alignment with
  `ecs::effects::alignment::Get(Temple::owner)`, the per-player value kept for the whole game (see
  [Player alignment](../magic/influence-and-alignment.md#player-alignment)). It used to read an `Alignment` component
  on the player entity, which no player entity has, so every temple had 4 circles per arm; a difference against the
  old count only shows while the owner is below −0.3.
- **The animated statics**: `AnimatedStaticAddToRoutePlan` (`src/ECS/RoutePlanWorld.cpp`), for anything with
  `components::AnimatedStatic`, ahead of `ObjectAddToRoutePlan` (before, they fell to it: one circle of `Get2DRadius` +
  margin, or 2..5 along the long side). openblack tells the gate by its type (`AnimatedStaticInfo::NorseGate`), the
  original compares the drawn mesh with mesh 212: they agree (the info.dat records above).
  - The gate: `AnimatedStaticSystem::RouteCircles` → `animated_static::GateRouteCircles`
    (`src/Scenery/AnimatedStaticRules.cpp`), its middle `animated_static::GateRouteMiddle`: the `Transform`'s x and z
    through the fixed grid (`map_coords::FromMetres`, then `map_coords::ToMetres`, one rounding as the FPU does), as
    the world matrix's translation is (before, it was the `Transform`'s position as it is, which differs by less than
    0.00016 off the grid).
  - The rest: `animated_static::ModelRouteCircle` of `object::MeshHalfDiagonal`, at `ToPoint2D(MapCoordsOf)`.
  - The place and resting place are the gate's `AnimatedStaticPose`, which its clip's step moves on the frames the
    object draw list draws it. An open gate keeps all 15 circles while it is still opening and leaves out the middle
    five once it rests at its resting place; one with no `AnimatedStaticPose` is taken at place 0. The testbed
    scenario `creature.walks_past_creature_gate` walks a creature past the gate, shut and then opened.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/RoutePlanWorld.cpp`: `k_MinRadius`, `k_CircleRadiusFactor`, `Emit`, `IsPileFood`, `SimpleAddToRoutePlan`, `CitadelHeartAddToRoutePlan`, `CreatureMustAvoid`, `AddToRoutePlan`, `ToPoint2D`, `InitialiseSystem`, `ObjectType::BigForest`, `ObjectType::MobileObject`, `ObjectType::MobileStatic`.
- `src/ECS/RoutePlanWorld.cpp`, `OnLandLoaded`: the second is the creature's (pending).
- `src/ECS/RoutePlanWorld.cpp`, `CreatureMustAvoid`: Football 0x5312F0, ArenaSpellIcon 0x425310, PrayerSite 0x670A10, TownDesireFlags 0x746DE0 and FieldCrop 0x608440 also give 0: (pending) openblack tells them apart by type only where it can
- `src/ECS/RoutePlanWorld.h`: `CircleFn`, `ToPoint2D`, `ToMapCoords`.
- `src/ECS/LivingFootpath.h`: `k_MoveToFirstNode`, `SetupMoveOnFootpath`, `IsFollowingNode`, `RePlanHug`, `ArrivePosOf`.
- `src/ECS/LivingFootpath.h`, `ArrivePosOf`: - (pending) Dance 0x50CB40, +0x14 with x + 0x8000: openblack has no Dance entity
- `src/ECS/LivingFootpath.cpp`: `LeaveNode`, `ArrivePosOf`, `SetupMoveOnFootpath`, `AbodeType::Creche`, `AbodeType::Graveyard`, `AbodeType::Wonder`, `VillagerStates::MoveOnPath`.
- `src/ECS/LivingFootpath.cpp`, `SetupMoveOnFootpath`: 1 -> SetAnim(1) (vt +0x8FC: the state change picks the clip in openblack)
- `src/RPlan/Point2D.h`: `GetLength`, `SetSize`, `Normalize`, `GetHeading`, `OctantAtan`.
- `src/RPlan/RPFollow.cpp`: `RPFollow::RPFollow`, `RPFollow::Init`, `RPFollow::ArcLength`, `RPFollow::Length`, `RPFollow::Cumulate`, `RPFollow::CutAfter`, `RPFollow::RouteMeets`, `RPFollow::PlanRouteMeets`, `RPFollow::PositionAt`, `RPFollow::FreePlans`, `RPFollow::OnFailed`, `RPFollow::OnFound`, `RPFollow::SpliceAndShortcut`, `RPFollow::Update`, `RPFollow::MoveAlongRoute`, `RPFollow::OnObjectAdded`, `CutAfter`, `GetHeading`, `State::Following`, `State::None`, `State::Idle`, `SpliceAndShortcut`, `RPlan::State::Searching`, `State::Found`, `RPlan::State::Found`, `State::Replanning`, `ObstacleHook`, `_hasMainPlan`, `RouteMeets`, `PlanRouteMeets`.
- `src/RPlan/RPFollow.cpp`, `RPFollow::OnFound`: (openblack) the exe keeps plan[0] as the route's owner (+0x6408C);
- `src/RPlan/RPFollow.cpp`, `RPFollow::OnObjectAdded`: (pending) 0x864D46..0x864DD2's box (+0x5000C..+0x50018, in tens) has no writer in the exe: if they stay 0 the box is [0, 10] x [0, 10] and almost no obstacle makes the follower re-plan. Not tested here: every new obstacle is taken as inside it (the tests' re-plans rely on that)
- `src/RPlan/RPFollow.h`: `DoneFn`, `StepFn`, `RadiusFn`, `Init`, `Update`, `SetIdle`, `GetHeading`, `GetPlanStart`, `Length`, `ArcLength`, `CutAfter`, `RouteMeets`, `PlanRouteMeets`, `SpliceAndShortcut`, `_state`, `_turnsPerUpdate`, `_done`, `_step`, `_radius`, `ObstacleHook`, `State::Idle`, `State::None`, `_hasMainPlan`.
- `src/RPlan/RPFollow.h`, `_hasMainPlan`: +0x6408C, the adopted main plan: (openblack) its route's head / tail and the fields the exe still reads
- `src/RPlan/RPHolder.cpp`: `s_fillSquare`, `s_obstacleHook`, `RPHolder::FillSquare`, `RPHolder::Link`, `RPHolder::GetSidePointOfStartObject`, `RPHolder::GetTangent`, `RPHolder::GetFirstObject`, `RPHolder::ArcBlocked`, `FillSquareFn`, `ObstacleHook`.
- `src/RPlan/RPHolder.h`: `PointIsTotallyInside`, `FillSquareFn`, `ObstacleHook`, `InitialiseSystem`, `NotifyObstacle`, `SetMargin`, `SetEnds`, `SetPlan`, `GetTangent`, `GetFirstObject`, `ArcBlocked`.
- `src/RPlan/RPlan.cpp`: `RPlan::NewRoute`, `RPlan::CopyRoute`, `RPlan::GetArcLength`, `RPlan::DropRoute`, `RPlan::Visit`, `RPlan::SetDest`, `RPlan::Shortcut`, `CopyNode`, `State::Failed`, `State::Found`.
- `src/RPlan/RPlan.h`: `SetStart`, `GetDestRadius`, `CopyNode`, `CopyRoute`, `DeleteRoute`, `GetArcLength`, `SetCost`, `DropRoute`, `CheckArrived`, `ArcLength`.

### Test hooks

- The testbed scenario `creature.walks_past_creature_gate` (above).

## Pending

The footpath walk, fork issue #74 (done: W1, W2 and the move states). Fix rows left:

- **W3 (the mover)**: SetupMoveToOnFootpath's object branch (VillagerHome.cpp: the arrive test reads the door for
  every object, ignores the altitude and always hugs): `ChooseWalkToObject(Pos, GetArrivePos(object), pos)`, then the
  object's footpath link → `footpaths::UseFootpathIfNecessary`, else the hug. After N-C-16 B1b; measured alone.
- **W4**: the callers of SetupMoveToOnFootpath still walking directly in openblack, one row each as it is ported: the
  worship walk (GotoWorshipSiteForWorship 0x76BD43 and the four other worship ones), the housewife's four, the
  workshop's three, the hiding and fleeing ones, SetupBreederDisciple, GotoAltarForRest,
  AddMoveToPosWithFootpathReaction 0x5F1067, the dance 0x502147.
- **The walking villager's altitude**: the arrive test (`MapCoords::operator==` 0x605660) and the next footpath's
  nearer end (GetMetresDistanceSq 0x605FB0 at 0x5EDFA5 / 0x5EDFB2) read Living Pos +0x14 with its altitude;
  openblack's `living_footpath::PosOf` gives altitude 0. Pos.altitude (+0x1C) is 0 after any landing
  (`Villager::EndPhysics` 0x5F0BA8) and the ground walk (`MobileWallHug::MoveTo` 0x60AF20) never writes it (only the
  flyers' `MoveTo3D` does), so openblack's 0 matches for a landed villager. Still open: a villager's altitude at
  creation, from its creation MapCoords (the oracle on a fresh villager's Pos after `SetupMobileMoveToPos`).
- **An empty next footpath**: the original reads through a null node (0x5EDFA2); `footpath_walk::ReachNode` ends the
  walk instead (a guard). Whether a link's list can hold an empty footpath is not shown.
- **Save / load of row 29** (`Villager::SaveFootPath` / `LoadFootPath`): with the saved games.
- **GenerateRoutesBetween** has no caller in openblack yet (the town artifact 0x426560, the dance 0x50B94F and
  sub_535CD0 are not ported); its ends use `living_footpath::GetArrivePos` (#74 F).
