# Routes and footpaths

## Summary

A villager walking to an object may take a footpath: the object's footpath link finds the nearest path within 40 m,
the villager walks to its first node and then from node to node (state 29 MOVE_ON_PATH), with a little random spread
at each node. Footpaths are made and bent round obstacles by the route planner (RPlan), a best-first search over
routes that go straight between obstacle circles and round them along their arcs; its obstacles (RPHolder) are kept
in a grid of 80 m squares and added by each object (the temple's heart and arms, the Norse gate's row of circles).
The creature follows its routes with its own follower (RPFollow).

openblack ports the footpath queries, the walk from node to node (row 29), the planner, its obstacles (with the temple
and the animated statics) and the creature's follower; the walk to an object takes the object's footpaths, from
every caller of the original's 37 that its game reaches, the worship walks among them (see [Pending](#pending), W4).
The creature's walk uses the follower as the original does: its longest route, its search turns (a share of 32 a
turn among the creatures searching, 256 on a walk's first), its end codes and its give-up.

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
- A deleted thing's link: `MultiMapFixed::ToBeDeleted` 0x52E2B0, after `RemoveAllReactionsInitiatedByObject`
  (0x52E2B7), asks vt +0x74 and, with a link, calls its vt +0xC (`GFootpathLink::ToBeDeleted`, 0x52E2D7) and clears
  +0x64 (0x52E2DA): the link's list goes, the footpaths stay in the game's list (the other end's link, a storage pit's,
  still lists them). Every multi-map fixed class reaches it (Abode 0x402CA6, AnimatedStatic, BigForest, CitadelPart,
  FishFarm, MobileStatic, SpellIcon, TotemStatue, WorshipSiteUpgrade call it; Feature, Flowers and PFootball have it
  as their vt +0xC). An abode that leaves a rebuild plan has given its link away first: `Abode::ToBeDeleted` 0x402C60
  runs `MoveAbodeToPlannedAbodes` (vt +0x90C, 0x402C8A) before the multi-map fixed part (0x402CA6), and
  `PlannedAbode::Create(Abode*)` 0x405660 moves abode +0x64 to the plan's +0x38 and clears it (0x40568C..0x405696).
  `PlannedAbode::ToBeDeleted` 0x4056B0 does not delete the plan's link. A dance's link (+0xE8) is never deleted:
  `Dance::ToBeDeleted` 0x50B970, `GroupBehaviour::ToBeDeleted` 0x596320 and `Dance::~Dance` 0x50B960 do not read it.
  Nor is a forest's (+0x30): `Forest::ToBeDeleted` 0x539C60 does not read it.
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
- The link follows its building (no caller of its own: vtable slots):
  - a plan built (`PlannedMultiMapFixed::PostCreatePlanned` 0x648C50, vt +0x508 of every planned class, called by the
    plans' CreatePlannedNoFixedCheck, the temple heart's 0x467EF0 among them): the new building's own link
    (+0x64) is deleted (vt +0xC, 0x648C58..0x648C63), the plan's link (+0x38, none included) is written into the
    building (0x648C66..0x648C69), the plan's slot cleared (0x648C6C), then, with the plan's town (vt +0x48), the
    town's CheckWhenNewBuildingCreated 0x741500;
  - an abode made a plan (`PlannedAbode::Create` 0x405660, from ConvertToPlanned 0x405050 and the abode's
    MoveAbodeToPlannedAbodes 0x404520): the plan takes the abode's link (0x40568C..0x40568F) and the abode's slot is
    cleared (0x405696). The plan copied from a building (0x648820) starts with no link (0x648866);
  - a fixed object deleted (`MultiMapFixed::ToBeDeleted` 0x52E2B0): after the reactions it started, its link, if any
    (vt +0x74), gets ToBeDeleted(0) and the slot is cleared (0x52E2C3..0x52E2DA), before the global list and the
    building site;
  - a plan deleted: its destructor (`PlannedAbode` 0x405110, `PlannedMultiMapFixed` 0x55CBF0) is GameThing's, so a
    plan's link is not deleted with it.
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
  dance 0x50B94F and sub_535CD0. sub_535CD0 is reached only from sub_5356F0, from sub_641570 (0x6417CE), a key
  handler (it reads LH_MOD_CTRL / ALT / SHIFT) that nothing calls: no call or jump reaches it nor its switch
  sub_641080, and neither address is stored anywhere in the exe's image (searched whole), so its eight routes are never
  made in play.
- `GFootpathFinder` (ctor 0x538EC0, one `GameThing` in the global game lists) plans the same route over several
  turns. The ctor makes the footpath at once (`GFootpath(from, to)`, `AddPos`(to's arrive point), then `AddPos`(from's):
  the head is from's, both nodes temporary) and `Init` 0x5392F0 sets the planner going as GenerateRoutesBetween does:
  each end pushed out of its own multi-map fixed object (From first), SetStart(head, 0.5, its own holder, −1, −1, 0),
  SetDest(tail, 0, 0, 0, −1, 0, (range + 1) × 5, 1). `GameTurnProcess` 0x539040, every turn from
  `GlobalGameLists::Process` after the puzzle games (the list's head first): From, To or the footpath unavailable →
  deleted with its footpath; still searching (state 2) → up to 20 (0x14) `GameTurnUpdate(0)`, stopping when the state
  changes; found (5) with a route → `Done` 0x539240: `ConvertCreaturePlanToFootpath` between the head and the tail, the
  nodes that are not hidden lose their temporary bit, `From->AddFootpath`, `To->AddFootpath`, and the finder goes
  (the footpath kept); anything else → deleted with its footpath. Its only maker is `WorshipSite::AddTown`
  ([worship](../magic/worship.md)). openblack: `footpaths::StartFootpathFinder` / `ProcessFootpathFindersTurn`
  (`components::FootpathFinder`, each with its own obstacle grid and plan; tests
  `FootpathRoutesTest.AFinderMakesItsFootpathAtOnceAndGivesItToBothEndsWhenPlanned`, `AFinderWhoseEndWentIsDeletedWithItsFootpath`),
  from `Game`'s turn after the puzzle games. Saving a finder (`Save` / `Load` 0x539150 / 0x5390E0) waits for the
  saved games (#186, #187).
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
- With a creature (vt +0x7BC, the creature's `GetHeight` vt +0x42C):
  - Object 0x638740 and MobileStatic 0x609010: the thing's height ≥ 0.1 × the creature's (0x8AB22C), or on fire;
  - MultiMapFixed 0x52F490: begun (`GetPercentBuilt` vt +0x880 > 0.01 (0x8C5840), or its building site +0x74 has
    scaffolds, +0x24 > 0) and that height test, or on fire;
  - Tree 0x74C0E0: ≥ 1.5 × (0x8AB24C), or on fire;
  - DeadTree / FelledTree 0x510E30, Field 0x5280C0, FieldCrop 0x608440, PileFood 0x66E1F0: the creature not
    controlled by a script (its own +0x24 & 0x400) and on fire;
  - Fragment 0x76F7F0: on fire;
  - AnimatedStatic 0x422EC0 and the chess pieces: 1;
  - Creature 0x47D4B0: another creature whose route follower state (3D +0x5198 → +0x64054) is 0, 1 or 2;
  - the rest as with no creature.
  Ours: `route_plan_world::MustAvoidByRule` with `AvoidRuleOf` / `AvoidFactsOf`. Pending: the other creature's case
  (openblack does not tell its follower state yet: never avoided).
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
- Layout: +0x6402C position, +0x64034 speed, +0x64038 distance along the current node, +0x6403C target speed (5.0; the creature's
  required speed sets it, and its straight move and the follower's speed step sub_8648B0 read it; openblack's `_targetSpeed`), +0x64040 heading, +0x64044 next plan's start (the route's end while following), +0x6404C dest radius.
- Layout (continued): +0x64050 last step, +0x64054 state, +0x64058 dest pending, +0x6405C turns per update, +0x64060
  context, +0x64064 / +0x64068 / +0x6406C callbacks, +0x64070 current node, +0x64074 dest, +0x6407C / +0x64080 /
  +0x64084 SetDest's a / b / c.
- Layout (end): +0x64088 found once (1 at a found end, 0 at a new plan), +0x6408C the adopted main plan, +0x64090..
  plans, +0x640A4.. the route node each re-plan starts from, +0x640B8 plan count, +0x640BC best plan (−1 after case 1).
- Callbacks: +0x64064 done(context, code), with code 2 = re-planning after a dest-inside end, 1 = after a
  failed one, 0 = no pending dest and the dest inside an obstacle, 3 = no pending dest and failed (only `OnFailed`
  calls it: a found plan reports nothing). +0x64068 first step(context, 0, the first node's length ≥ 0.01).
  +0x6406C how far the walker is bound to go along the route before it can turn off it (the creature's
  `FollowerCallbackGetStopDist` 0x47F280, [below](#the-creatures-walk-on-the-follower)).
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
  (0x864495..0x8644D1); r = the stopping distance (0 without the callback) and L = the current node's length
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

##### The creature's walk on the follower

✅ Read in the v1.42 exe (xref notes #3298–#3305, #3328–#3333) and bw1-decomp `Creature3D.cpp`.

- **The walk's last argument is the plan's longest route, not a time.** The moves to an object (0x47A7A0) and to a
  point (0x47A880) pass 2 ([0x8CC568]) × distance + 5000 ([0x8CC56C]) as `StartMovingToPoint`'s last argument, which
  `RPFollow::SetDest` stores at +0x64084 and hands to every `RPlan::SetDest` as `maxLength` (above). A route longer than
  it is rejected by the visited-points test (fn_0086E890); nothing counts time while a plan searches, so a walk with a
  way there plans until the search finds it or runs out of routes.
- **The search turns.** The creature's ctor (0x47F770) gives the follower 16 search turns an update (field_0x571c =
  0x10, passed to `Init` 0x863AC0, stored at +0x6405C). At the start of every turn, before any living's turn,
  `Living::ProcessLiving` 0x5EC810 counts the creatures (`Creature::CreatureList`) whose follower's state (+0x64054)
  is 2 (planning) or 4 (re-planning), takes 1 for none, and writes 32 / count (`mov eax, 0x20; cdq; idiv`,
  0x5EC84C..0x5EC852, rounded towards zero), at least 1 (0x5EC856), into +0x6405C of every creature's follower
  (0x5EC878), searching or not. It is an equal share, not a pool: no update counts it down, so the order the creatures
  are served in doesn't change it, and a creature that starts a search later in the turn is not counted until the
  next. The constructor's 16 is overwritten at the next turn's start. A walk's first update after
  `StartMovingToPoint` (+0x519C set, `DoMovingAction` 0x48D2A0) runs 256 turns with the encounter hook installed,
  which notes the obstacles the search meets; later updates pass no hook and 0x80, which `GameTurnUpdate` 0x864040
  replaces by +0x6405C (0x864049..0x86404F), so they run the turn's share. With obstacles noted, `TrappedInEnclosedSpace` 0x47D9A0 (unless the creature is
  already marked trapped) picks one with `GameRand`, forces plan 0x18 with that obstacle's removal action and marks
  the creature trapped; the action is `FindSuitableActionToRemoveObstacle` 0x47DAA0's: none for a citadel heart, a
  storage pit or a town centre, 0x107 a pick-up, 0xEC a kick, else a magic action it knows.
- **A walk from standing starts afresh.** `StartMovingToPoint` on a creature that is not walking calls `RPFollow::SetPos`
  0x863B00, which deletes the route and every plan (0x863A80) and empties the follower's own grid (`RPHolder::Empty`),
  so the squares are filled again by the new search; an earlier walk's obstacles are not kept.
- **A new destination while planning** goes to the same follower (`RPFollow::SetDest` 0x863B70, by its state, above);
  only a walk from standing calls `SetPos`.
- **Where a route re-planned on the way turns off** ✅. The splice (`ConcatenateRoutes` 0x864420, steps 4-5 above)
  keeps a straight node of r along the old way before the new part when r is more than 0.01; r is the creature's
  `GetStopDist` 0x47F2A0 (bw1-decomp Creature3D.cpp:175, the frozen exe's bytes the same): 0.05 ([0x8AC3F4]) while it
  turns on the spot to set off (motion +0x5190 = 4); while it steps off (5), the step blend's move +0x5220 → +0x10 as
  √((z × z + y × y) + x × x), × the drawn scale +0x94, − the follower's distance along its node +0x64038, each a
  float (0x47F2B7..0x47F2F3); 0 otherwise, walking (6) included, so a creature sent elsewhere as it walks turns off
  where it is. The step's blend (`CAnim::CAnim(a, frame, b, frame, w)` 0x85E9D0, 0x85EA15..0x85EAD2) blends the
  strides +0xC and each part of the moves +0x10 as (b − a) × w + a, then, when any part is on either side of 0 (a NaN
  counts as none), multiplies each by stride / √((z × z + y × y) + x × x). Oracle 2026-10-29 (the constructor up to
  its first allocation, `GetStopDist` on a fake body): (0.5, 0.05, −1.9) of 2 and (1.7, 0, −2.6) of 3.1 at 0.37 give
  0x3F76D5FB 0x3D03C906 0xC00D2213, and at a scale of 1.3 after 0.4 along 0x402EA993. openblack:
  `FollowerStopDistance` in `CreatureLocomotionSystem` with `creature_locomotion::BlendedMove` and
  `SteppingStopDistance` (the step's two base-mesh animations and their stride, `skeletal_animation::Animation::stride`;
  tests `CreatureLocomotion.AStepsBlendedMoveTakesTheBlendedStridesLengthAsTheOriginal`,
  `WhileItStepsOffItIsBoundToGoTheRestOfItsStepAsTheOriginal`,
  `CreatureLocomotionSystemTest.SentElsewhereAsItWalksTheNewRouteTurnsOffWhereItIs`,
  `SentElsewhereAsItStepsOffItKeepsToTheRestOfItsStepFirst`). Until 2026-10-29 it gave the creature's radius (5 unless
  its mesh was measured) in every motion, so a re-planned route kept 5 straight on first.
- **The radius its plans start from** ✅. `StartMovingToPoint` gives `RPFollow::SetDest` the creature's nav radius
  (`GetNavRadius` 0x480A60 at 0x4841BE, the call 0x4841DC); the follower keeps it (+0x6404C) and starts every plan
  with it (`SetDest` cases 1 and 3, the splice's local plan at 0x86464B, the re-plans after a blocked route). The plans
  only keep it (`RPlan::SetStart` 0x86E200 writes +0x4; its only reader is `RPlan::Shortcut` 0x86F560, which starts its
  nested plans with it), so it changes no route. openblack: `CreatureLocomotionSystem::StartMove` passes
  `ecs::object::GetNavRadius` at both its `SetDest`s since 2026-10-29 (test
  `CreatureLocomotionSystemTest.ItsRoutePlansStartFromItsNavRadius`); before, the creature's own radius.
- **A walk stopped on the way brakes along its route.** `LH3DCreature::StopMoving` 0x484260 (bw1-decomp
  Creature3D.cpp:2237) resets the breath time and, only while the body is moving (`MoveState` 1), calls
  `RPFollow::StopMoving` 0x863EF0 and clears +0x51B4; `MoveState` stays 1, so `IsPerformingBodyAction` stays true
  and `StartTurningAction` 0x481160 (and every body action gated on it) is refused until the walk has ended.
  `RPFollow::StopMoving` by the follower's state: planning (2) deletes the route and the plans and goes to 1, so the
  next `DoMovingAction` 0x48D2A0 stands the creature (motion 0, Creature3D.cpp:6146..6162); following (3) or
  re-planning (4) drops the re-plans, goes to 3, and cuts the route at d = (v / 12) × 12 × (v / 12) × 0.5 (v the
  follower's speed +0x64034, 12 [0x9A3BC0], each a float), or the stop distance callback's value when that is larger,
  along the current node: when d < (its length − the distance along it) − 0.01 the node's end becomes the point d past
  the follower (`FillPosAndHeading`), every node after it is deleted and it is the route's tail and end. Any other
  state is left alone. Its callers: `Creature::Faint` 0x476FA0 (0x476FD6), `CheckAllCreaturesForCatching`
  (0x47CC72), `LH3DCreature::EndAnyActions` 0x48F710 (0x48F73F; from `StopWhatIAmDoing` 0x4758D0, so every action
  finished, failed or reconsidered through `FinishActionGeneric`, and from `EndAnyActionsThenTurnToHeading`,
  `ForceGoToMapCoords`, the tattoo editor and the network packets), fn_004F00E0 (stop, then `StartTurningAction`: the
  nine turn-to-face sub-action stages 0x5012D0..0x503FC0), the freeze spell's start (sub_4F5430, "frozen by spell"),
  the copy of a player's spell (sub_4F88AA), the sub-action stages sub_503BD0 and sub_5057B0, and sub_47C780, which
  nothing calls. Oracle 2026-10-29 (`RPFollow::StopMoving` on a fake follower with one straight node from (100, 100)):
  to (300, 100), 2 along at a speed of 9, the node ends at 0x42D2C000 (105.375); to (250, 180) at 7.3 with a stop
  distance of 0.05, at 0x42CF72A3 0x42CBF8DF; at 1 with 2.5, at 0x42CFF0F1 0x42CC3C3C; to (110, 100) at 20 it keeps
  its end; planning (2) goes to 1, idle (1) and given up (6) stay. openblack, since 2026-10-29:
  `RouteFollower::StopMoving` and `CreatureLocomotionSystem::BrakeToStop` (`CreatureLocomotion::braking`: it goes on
  along the cut route until it stands, `IsMoving` stays true, so the agenda's steps and the body actions that wait on
  it wait, and `TurnToFace` / `TurnToHeading` are refused; a walk still planning stands at its next turn), wired only
  at the end of what the mind was doing (`CreatureMindSystem::StopWhatItWasDoing`, the original's `StopWhatIAmDoing`
  through `EndAnyActions`); tests `RouteFollowerTest.StoppedOnItsWayTheRouteEndsWhereItCanStopAsTheOriginal`,
  `StoppedWhileReplanningItDropsTheReplansAndBrakes`, `StoppedWhileItPlansItDropsThePlanAndIsIdle`,
  `CreatureLocomotionSystemTest.StoppedOnItsWayItBrakesAlongItsRouteToAStop`,
  `StoppedWhileItsRouteIsPlannedItStandsAtItsNextTurn`, `NotOnAWalkTheStopOnItsWayIsTheStop`; a readout in the
  creature spawner's Movement section. Every other caller of `CreatureLocomotionSystem::Stop` still stands the
  creature at once ([Pending](#pending)).
- **Which of openblack's stops brake**, each matched to the original's call (2026-10-29):
  - The mind. The player's feedback brakes: `RespondToPlayerFeedback` 0x4E06A0 starts with
    `FinishActionUnsuccessfully("player feedback", 1, 1)` (0x4E06AA..0x4E06C1) → `FinishActionGeneric` →
    `StopWhatIAmDoing` (0x47588F) → `EndAnyActions` (0x475B76) → `StopMoving` (0x48F73F); openblack's
    `ReceiveFeedback`. A forced plan brakes: `ForceActivityAndForceAction` 0x4C44B0 calls
    `FinishActionUnsuccessfully("stopping because new action forcing me", a7, a7)`, and every direct caller pushes 1
    for both but `sub_5073E0` (0x507457: 1, 0, still 1 for the walk) and those passing a register (`ApplyEffect`,
    `SwapMinds`, `TrappedInEnclosedSpace`, `ReduceLife`, fn_0047A1D0: not read); openblack's `Replan`. The stop step
    brakes: the sub-action the table names "StopMoving" (its row at 0xBE1BE8, the name copied from 0xBE62EC by `crt_xc_fn_CreatureSubAction_004F8D70`) has stage 0 sub_503BD0, `StopMoving` alone, and stage 1 sub_503BF0 is done once
    `IsPerformingBodyAction` is false (until then it calls `StopPointing`); openblack's `Step::Kind::StopMoving` now
    ends once the body neither moves nor plays an action (it ended once no action played). The wait for orders'
    turn brakes: 0x502E70 turns through fn_004F00E0 (`StopMoving`, then `StartTurningAction`, asked again each turn);
    openblack's `WatchPlayer`. Instant, as the original: the forced animation of a stroke or a slap
    (`ForceAction`; `LH3DCreature::ForceIndividualAction` puts the body in its individual action state at once, so the
    walk is over); the walk step's time up and the faint without a fight are not matched yet.
  - The fights stay instant. The start (`CreatureFightSystem::StartFight`) stands for the forced Fight plan, whose
    forcing brakes, but the original's plan then turns each creature to face the other (fn_004F00E0, refused until
    it stands), plays an individual action and only then walks to the arena (`ConstructSubActionsForFight` 0x49BCD0,
    [fights](../creature/fights.md#fights-a-fight-a-script-forces)); openblack's fight walks to the arena at once, so
    a brake alone would not give the original's order. The faint (`CreatureFightSystem::Faint`): `Creature::Faint`
    0x476FA0 brakes (0x476FD6), but the faint's start reaches `StartDeathAction` 0x484D30, whose `RPFollow::SetPos`
    (0x484D7D) drops the route at once; how long the brake lasts before it is not read. The taunt's turn: the
    original's turn to face is asked again every turn until the body stands, openblack's taunt asks once. The duel:
    `StartFighting` 0x484410 (from the FIGHT sub-state's start sub_5036C0, 0x5037A6) is refused while
    `IsPerformingBodyAction` and stops nothing.
  - The leash's refused walks brake: a walk back (`LeashSystem`'s keep within) or a walk to a tapped point, to put a
    thing down or to throw it, refused because the point can't be stood on, ends the action as the original's failed
    move does: `MoveToPos`'s stage 0 calls `FinishActionUnsuccessfully("Stopping because navigation failed", 1, 1)`
    (0x4FFC9C..0x4FFCA7) → `StopWhatIAmDoing` → `EndAnyActions(1, 1)` → `StopMoving`
    ([the walk back](../creature/leash-rope.md#the-walk-back-into-the-kept-area)). The tie to a thing pulls the
    creature away from its action and brakes it too (`LeashSystem::PullAwayFromAction`): packet 0x5C's handler, with
    an object, ties it (fn_005E6BD0) and calls `FinishActionUnsuccessfully("pulled by leash", 1, 1)` (0x63D49C..
    0x63D4AE, the string at 0xBFDFB4) before `ForceActOnObject` (0x63D504); with none it only lets the leash go back
    to the hand (`SetOn`, fn_005E6EA0), with no stop, as openblack's `ReturnToHand`. (The tug's own pull-away,
    `LeashMakesCreatureStop` 0x4CEC00, finishes with 0, 0 and stops nothing, but the tug never happens:
    [the leash's tug](../creature/leash-rope.md#the-leashs-tug).)
  - The casting brakes: the walk to what it casts at stops once near enough as GoNearObject's Perform
    sub_5057B0 does (`StopMoving` within float(`Get2DRadius(creature)` + r + d) × 1.1, then done,
    [casting](../creature/casting.md#going-near-backing-off-and-turning)), and the turn to face it as stage 0x5012D0 does (fn_004F00E0 at 0x501398:
    `StopMoving`, then `StartTurningAction`, asked again every turn until it starts; openblack's phase 0 also asks
    again every turn).
  - The catch brakes: `CheckAllCreaturesForCatching` calls `StopMoving` (0x47CC72) and forces the catch plan, whose
    start sub_507C70 waits while `IsPerformingBodyAction` ([hits and catch](../creature/hits-and-catch.md));
    openblack's `GamePhysicsHooksWorld::SetAboutCatching`. The freeze brakes: its start sub_4F5430 calls
    `FinishActionUnsuccessfully("frozen by spell", 1, 1)` and `StopMoving` (0x4F545A); openblack's freeze in
    `SpellCreature.cpp`. A forced plan that can't be made brakes: the forcing has ended what it did, walk included,
    before the plan is built ([casting](../creature/casting.md#the-fizzle-shown)); openblack's `ForceActivity`. Instant,
    as the original: an action on a thing begins only on a still body (`StartObjectKeepAction` and its kin are refused
    while `IsPerformingBodyAction`), so its stop (`BeginPlaying`) finds no walk. Not matched: the removal of a creature
    (`StopActing`).
- **How a plan's end reaches the creature.** The done callback is `LH3DCreature::FollowerCallbackFunction` 0x47F1F0. It
  calls `Creature::CantFindRoute` on the codes 0 and 3 only. `OnFailed` 0x8641F0 gives 0 for a destination inside an
  obstacle (`RPlan::SetDest`'s DestInside, which ends the plan with no route) and 3 for a plan whose search failed; 1
  and 2 are a pending destination planned anew (the walk waits for that plan) and the callback ignores them. A found
  route reaches the creature through `OnFound`, not through the callback.
- **`CantFindRoute`** 0x4753F0: the creature's navigation-blocked mark is set, its turns since its action was done are
  set to 0, `FinishActionUnsuccessfully("Cannot find route", 1, 1)` ends what it was doing as a failure, and it plays
  sound 56 Refusal (0x47543E, [the creature's sounds](../audio/creature-and-interface-sounds.md)).
- **What a scroll on the land adds.** A did-you-know scroll (`ScriptHighlight`, a `SingleMapFixed`) is a route obstacle
  like any object: `CheckSquareFunction` 0x54AFB0 adds it with `ScriptHighlight::AddToRoutePlan` 0x70AE40
  (`SimpleAddToRoutePlan`: its `Get2DRadius` / `GetRoutePlanRadius` circle, above). Nothing removes it when it is read
  (refs/chl/DidYouKnow.txt:16–22: the script makes the highlight, waits on it and releases it from the script, and
  the highlight stays on the land).
- **A grown creature can't get out of the Land 1 pen to the Norse crèche** ✅ (the Pied Piper's walk,
  [the quest](../project/progress/story/silver_scrolls/the_pied_piper.md)). The holder a creature's follower fills
  for a walk from its home point (1895.97, 2520.75) to the piper by the crèche (1868.0, 2729.4, near 41.9) holds the
  coast's and the steep cells' 7.1 m land-avoid circles (`CheckSquareFunction` 0x54AFB0: with a follower only LandAvoid
  1 cells), the heart's ring and every tree the creature must avoid: `Tree::CreatureMustAvoid` 0x74C0E0 (the tree's
  `Object::GetHeight` 0x638120 at least 1.5 × `Creature::GetHeight` 0x477F50, the 3D body's +0x90 × 15, which the pen
  draws at 0.22: 3.3 whatever the creature's own size), one circle each (`Tree::AddToRoutePlan` 0x74A180 →
  `Object::SimpleAddToRoutePlan` 0x638B00) of `Tree::GetRoutePlanRadius` 0x74A140 = min(0.25, 2D radius × 0.1) plus the
  holder margin, the nav radius at the creature's own size (`GetNavRadius` 0x480A60). The Land 1 script's trees 38 to
  60 m round the crèche clearing (mostly kinds 3 and 4 at about scale 1) close the clearing when the margin is large.
  The original's planner itself, run natively through the oracle on the 3362 circles of an openblack holder
  (tools/oracle, with the holder, the plan and the heap in the oracle's arena;
  `RPlan::GameTurnUpdate` 0x86E9A0 stepped until it ends), with the margin taken at each size (species row 14, r =
  6.654 × size; the trees 0.25 + r, the other margin-bearing circles moved by the same amount): sizes 0.22 to 1.5 find
  the way in 6 to 87 steps; sizes 1.6 to 2.0 fail (state 4, the search exhausted, after 4412 to 4484 steps). Without
  the trees it finds the way in 3 steps, without the trees within 60 m of the crèche in 11, without the land-avoid
  circles in 148; dropping any other kind still fails. A failure that late is not the trapped give-up:
  `RPFollow::GameTurnUpdate` 0x864040 turns a failed plan into the trapped state only inside the walk's first update
  of 256 search steps (`DoMovingAction` 0x48D2A0 → `TrappedInEnclosedSpace` 0x47D9A0), so the walk ends as a failure
  (end 3) and the creature stays at home. A new creature starts at the species' start scale, 0.22 for all 17 rows of
  info.dat, and grows at most 2 / (growUpMinutes × 600) a turn (`CalculateHeightIncrease` 0x4EFA60: 0.5 an hour at 240
  minutes during its first 3 hours of age, three times that asleep), so a natural game reaches the Piper's scroll with
  a creature of 0.22 to about 0.7; only a creature kept from a profile near its grown size (2 at most) meets the
  stall. The script's walk up to the piper (refs/chl/piedpiper.txt:351, `MOVE_GAME_THING` 0x6F8E80 →
  `Creature::ScriptMoveToPos` 0x4F6B60, two `MoveToPos` sub-actions at radius 10) plans a route like any walk. xref
  notes #3954 to #3972.

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
- `RPFollow::SpliceAndShortcut`: 4. 0x864517..0x864557: r = the stopping distance (0 without the callback); L = the current node's length
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
- `DoneFn`: +0x64064 `void (*)(int, int)`: the plan's end. code: 2 = re-planning after a dest-inside end, 1 = after a failed one, 0 = no pending dest and the dest inside an obstacle (only `OnFailed` calls it: a found plan reports nothing), 3 = no pending dest and failed (fn_008641F0)
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
- `StopDistanceFn`: +0x6406C `float (*)(int)`: how far the walker is bound to go before a re-planned route turns off (the creature's stopping distance)
- `_state`: < +0x64054
- `_turnsPerUpdate`: < +0x6405C (ctor 1)
- `_done`: < +0x64064
- `_step`: < +0x64068
- `_stopDistance`: < +0x6406C
- `s_obstacleHook`: [0xFA51D0] / [0xFA51C4]

### Sources

- bw1-decomp: `src/Black/Footpath.cpp`, `Living.cpp`; the planner (`RoutePlan.cpp` of the exe) read with the xref
  index; the footpath walk and the arrive test checked with the oracle.

## openblack

**Done**: the footpath queries, MOVE_ON_PATH 29 (fork issue #74, W1 and W2), the footpath setups' move states (0xB
STEP_THROUGH, 0xC LINEAR: N-C-16 B1b), the planner `src/RoutePlanner/` (the original's
RPHolder is `route_planner::ObstacleGrid`, RPlan is `route_planner::RoutePlan`, RPFollow is
`route_planner::RouteFollower`, and `Point2D`), its world side `src/ECS/RoutePlanWorld.cpp`. **Wired**: the walk to an
object takes the object's footpaths (W3). **Not wired**: the save / load of row 29 (the world
writer keeps a walker's footpath, `src/ECS/SavedWorld/WriteLivingParts.cpp`; the load makes no villager that is on
one, `src/ECS/SavedWorldLoad.cpp`).

- **The walk to an object** (`villager::SetupMoveToOnFootpath`, #74 W3): `footpath_walk::ChooseWalkToObject` on the
  villager's position, the object's `living_footpath::GetArrivePos` and the target (both altitudes 0, as a landed
  villager's is); standing on the arrive point and sent elsewhere it walks straight; else, with the object's link
  (`footpaths::GetFootpathLink`), `footpaths::UseFootpathIfNecessary(link, villager, target, final, object)`, and
  without one the plain walk. It answers whether the walk was set up (the walk taken's own answer). Tests:
  `test_use_footpath`.
- **The worship walks** (#74 W4, `VillagerWorship.cpp`): setting off (`GotoWorshipSiteForWorship`, 58) walks to the
  site's gate on its footpaths with 59 to follow when the site's distance from the villager
  (`object::GetDistanceFromObject`) is above 0 (`worshipper_rules::WalksToTheGate`), else sets 59 at once; 59 is the
  walk's end and walks to the gate again while farther than 10 m; the hiding walk to the door (213) takes the site's
  footpaths and counts the villager at the site only when it is set up; 213 walks back to the door with a hug; the
  dance place's walk is `SetupMoveToPos(place, 60)`. `ExitMoveToWorshipSite` takes the whole rule and
  `IsAtOrOnTheWayToWorshipSite` reads the final state (a teleport reaction's stored state). (guard) A site whose mesh
  has no gate point: 59 at once, counting as there. Tests: `WorshipperTest.*`, `WorshipperRules.*`.
- **The footpaths** (`ECS/Footpaths.{h,cpp}`, `ecs::footpaths`): a footpath is a `components::Footpath` entity and a
  node a `Footpath::Node`, named by its `NodeId` (`Footpath::Node::id`: its creation order in the footpath, never
  reused), which stands for the node pointer a Living keeps.
  - Lists and nodes: `Create` (at the head of the game's list, ordered by `creationStamp`), `FootpathsFromHead` (the
    newest first), `PositionFromHead` (only the tests call it; the saved games number the footpaths through their own lists), `MakeNode` (a node with the
    footpath's next id), `AddOccupant` (the Livings on a node, head first), `DeleteNode`.
  - Queries, on list places: `GetNextNode` (`NextIndex`), `GetNearestPos` (`NearestIndex`), `NearestNodeBelow`,
    `NextNodeNear`, `StepNextNode`, `NodeDistance`, `GetNearestPathTo`, `GetNearestPathToQuick`.
  - Links: `GetFootpathLink` (`components::FootpathLinkOf`, entt::null for none), `SetFootpathLink` and
    `DeleteLink` (an abode's, a worship site's or a citadel heart's link goes with it in `MultiMapFixedToBeDeleted`;
    the rebuild plan made from a building takes its link first, `plans::CreateFromBuilding`), `TakeFootpathLink` and
    `DeleteFootpathLinkOf` (any other multi-map fixed object's link goes with it, in `ecs::ToBeDeleted` after the
    buildings' part; a dance's and a forest's are left, as the original leaves them), `AddFootpath` (the link made on
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
- **The links follow their buildings** (#74 L1): a plan built hands its link to the building after deleting the
  building's own (`CreateBuildingFromPlan` in `ECS/Town/BuildingSites.cpp`, and the temple heart in
  `CitadelArchetype::CreatePlannedNoFixedCheck`); an abode made a plan hands its link to the plan
  (`plans::CreateFromBuilding`); a fixed object's ToBeDeleted deletes its link (`MultiMapFixedToBeDeleted` in
  `ECS/Abodes.cpp`). A plan removed keeps its link alive, as the original's. Tests: `test_repairs`,
  `test_citadel_plan`. In openblack the fixed objects outside the abodes, the temple heart and the worship sites
  (fields, fish farms, forests) do not go through `MultiMapFixedToBeDeleted`; their links stay when they go.
- **Row 29**: the state is `living_footpath::MoveOnFootpath` on the walk's result (MOVE_TO_POS's reading of the step:
  0xA when the walker stands on its goal after FINAL_STEP or ARRIVED), which applies `footpath_walk::ReachNode`'s
  outcome (every entry out of the old node, the new node's occupants at the head, `SetupMobileMoveToPos(goal,
  k_MoveToNextNode)`, the STEP_THROUGH walk since N-C-16 B1b, or SetTopStateToFinal and 0xA); the entry returns 1 as
  EnterMoveOnPath; the exit is `living_footpath::ExitMoveOnPath` (the circle hug reset is openblack's
  WallHugObjectReference dropped, as in the reactions' exit). The oracle runs are pinned in
  `test_footpath_walk_rules`. A BigForest's virtual arrive point (vt +0x104, the door) is
  `living_footpath::GetArrivePos`. The walks to an object enter it (W3, above).
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
- ✅ **The creature's walk** (`src/ECS/Systems/Implementations/CreatureLocomotionSystem.cpp`) follows
  [the original's](#the-creatures-walk-on-the-follower) since 2026-10-28: the plans' longest route is
  2 × distance + 5000 with no planning time limit (`creature_locomotion::MaxRouteLength`); the end codes 0 and 3 are no
  way there, and 1 and 2 let the walk wait for the newer plan; with no way there the creature gives its action up as a
  failure and refuses; a new destination while on its way goes to the same follower, and a walk from standing starts
  with a fresh one; the search turns are each turn's share of 32 (`ShareSearchTurns`, called first in
  `ecs::creature_loop::ProcessTurn`, before any creature's turn; `creature_locomotion::SearchTurnsShare`), 16 from a
  creature's making until the first share, and a walk's first search runs 256 with the obstacle hook
  (`k_FirstSearchTurns`). Tests `CreatureLocomotion.TheSearchingCreaturesShareThirtyTwoSearchTurnsATurn`,
  `CreatureLocomotionSystemTest.TheCreaturesSearchingAsTheTurnStartsShareItsSearchTurnsAndEveryFollowerTakesTheShare`,
  `CreatureLocomotionSystemTest.AWalksFirstSearchRunsLongerNotingWhatItMeetsAndTheOthersRunTheFollowersOwn`. Not
  ported: what the hook is told and `TrappedInEnclosedSpace`'s forced plan
  ([progress](../project/progress/creature/locomotion.md)).
- ✅ **A grown creature in the Land 1 pen** stalls as [in the original](#the-creatures-walk-on-the-follower): with a
  size-2 creature openblack's follower fills the holder by the same rules (`route_plan_world::CheckSquareFunction`,
  `CreatureMustAvoid`, `SimpleAddToRoutePlan` in `src/ECS/RoutePlanWorld.cpp`) and its walks to the crèche fail, and
  the original's planner fails on that very holder. The Pied Piper's quest run starts with a glade newborn (0.22), as
  a natural game reaches the scroll.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/RoutePlanWorld.cpp`: `k_MinRadius`, `k_CircleRadiusFactor`, `Emit`, `IsPileFood`, `SimpleAddToRoutePlan`, `CitadelHeartAddToRoutePlan`, `CreatureMustAvoid`, `AddToRoutePlan`, `ToPoint2D`, `InstallCallbacks`, `ObjectType::BigForest`, `ObjectType::MobileObject`, `ObjectType::MobileStatic`.
- `src/ECS/RoutePlanWorld.cpp`, `OnLandLoaded`: the second is the creature's (pending).
- `src/ECS/RoutePlanWorld.cpp`, `CreatureMustAvoid`: Football 0x5312F0, ArenaSpellIcon 0x425310, PrayerSite 0x670A10, TownDesireFlags 0x746DE0 and FieldCrop 0x608440 also give 0: (pending) openblack tells them apart by type only where it can
- `src/ECS/RoutePlanWorld.h`: `CircleFn`, `ToPoint2D`, `ToMapCoords`.
- `src/ECS/LivingFootpath.h`: `k_MoveToFirstNode`, `SetupMoveOnFootpath`, `IsFollowingNode`, `RePlanHug`, `ArrivePosOf`.
- `src/ECS/LivingFootpath.h`, `ArrivePosOf`: Dance 0x50CB40, +0x14 with x + 0x8000 (`k_DanceArriveOffset`), z and the altitude kept: the dance's `place`
- `src/ECS/LivingFootpath.cpp`: `LeaveNode`, `ArrivePosOf`, `SetupMoveOnFootpath`, `AbodeType::Creche`, `AbodeType::Graveyard`, `AbodeType::Wonder`, `VillagerStates::MoveOnPath`.
- `src/ECS/LivingFootpath.cpp`, `SetupMoveOnFootpath`: 1 -> SetAnim(1) (vt +0x8FC: the state change picks the clip in openblack)
- `src/RoutePlanner/Point2D.h`: `GetLength`, `SetSize`, `Normalize`, `GetHeading`, `OctantAtan`.
- `src/RoutePlanner/RouteFollower.cpp`: `RouteFollower::RouteFollower`, `RouteFollower::Init`, `RouteFollower::ArcLength`, `RouteFollower::Length`, `RouteFollower::Cumulate`, `RouteFollower::CutAfter`, `RouteFollower::RouteMeets`, `RouteFollower::PlanRouteMeets`, `RouteFollower::PositionAt`, `RouteFollower::FreePlans`, `RouteFollower::OnFailed`, `RouteFollower::OnFound`, `RouteFollower::SpliceAndShortcut`, `RouteFollower::Update`, `RouteFollower::MoveAlongRoute`, `RouteFollower::OnObjectAdded`, `CutAfter`, `GetHeading`, `State::Following`, `State::None`, `State::Idle`, `SpliceAndShortcut`, `RoutePlan::State::Searching`, `State::Found`, `RoutePlan::State::Found`, `State::Replanning`, `ObstacleHook`, `_hasMainPlan`, `RouteMeets`, `PlanRouteMeets`.
- `src/RoutePlanner/RouteFollower.cpp`, `RouteFollower::OnFound`: (openblack) the exe keeps plan[0] as the route's owner (+0x6408C);
- `src/RoutePlanner/RouteFollower.cpp`, `RouteFollower::OnObjectAdded`: (pending) 0x864D46..0x864DD2's box (+0x5000C..+0x50018, in tens) has no writer in the exe: if they stay 0 the box is [0, 10] x [0, 10] and almost no obstacle makes the follower re-plan. Not tested here: every new obstacle is taken as inside it (the tests' re-plans rely on that)
- `src/RoutePlanner/RouteFollower.h`: `DoneFn`, `StepFn`, `StopDistanceFn`, `Init`, `Update`, `SetIdle`, `GetHeading`, `GetPlanStart`, `Length`, `ArcLength`, `CutAfter`, `RouteMeets`, `PlanRouteMeets`, `SpliceAndShortcut`, `_state`, `_turnsPerUpdate`, `_done`, `_step`, `_radius`, `ObstacleHook`, `State::Idle`, `State::None`, `_hasMainPlan`.
- `src/RoutePlanner/RouteFollower.h`, `_hasMainPlan`: +0x6408C, the adopted main plan: (openblack) its route's head / tail and the fields the exe still reads
- `src/RoutePlanner/ObstacleGrid.cpp`: `InstallCallbacks`, `SetObstacleHook`, `ObstacleGrid::FillSquare`, `ObstacleGrid::Link`, `ObstacleGrid::StartObstacleSidePoint`, `ObstacleGrid::GetTangent`, `ObstacleGrid::GetFirstObject`, `ObstacleGrid::ArcBlocked`, `FillSquareFn`, `ObstacleHook`.
- `src/RoutePlanner/ObstacleGrid.h`: `RouteObstacle::ContainsCircle`, `FillSquareFn`, `ObstacleHook`, `InstallCallbacks`, `NotifyObstacle`, `SetMargin`, `SetEnds`, `SetPlan`, `GetTangent`, `GetFirstObject`, `ArcBlocked`.
- `src/RoutePlanner/RoutePlan.cpp`: `RoutePlan::NewRoute`, `RoutePlan::CopyRoute`, `RoutePlan::GetArcLength`, `RoutePlan::DropRoute`, `RoutePlan::Visit`, `RoutePlan::SetDest`, `RoutePlan::Shortcut`, `CopyNode`, `State::Failed`, `State::Found`.
- `src/RoutePlanner/RoutePlan.h`: `SetStart`, `GetDestRadius`, `CopyNode`, `CopyRoute`, `DeleteRoute`, `GetArcLength`, `SetCost`, `DropRoute`, `CheckArrived`, `ArcLength`.

### Test hooks

- The testbed scenario `creature.walks_past_creature_gate` (above).

## Pending

- **A walk stopped on the way** (found 2026-10-29, [above](#the-creatures-walk-on-the-follower)): the braking stop is
  ported and wired where the original brakes (the list [above](#the-creatures-walk-on-the-follower)). The other
  callers of `CreatureLocomotionSystem::Stop` still stand the creature at once, each to be matched to its original's (the braking
  stop, a hard reset through `RPFollow::SetPos` as `StartDeathAction` 0x484D30 and `ResetLook` 0x4831B0 make, or
  none, [above](#the-creatures-walk-on-the-follower)): the mind's walk step whose time is up and its faint without a
  fight; the fights' arena, faint, taunt and duel; removal; the editor and the debug tools (not the original's).
  Also left: the original's `StopMoving` resets the breath time on every call; its `StartTurningAction` is refused
  whenever the body moves, not only while it brakes; and a walk sent elsewhere after a stop while still planning keeps
  the same follower (`SetDest` from idle), where openblack starts a fresh one.
- **A new obstacle on a moving creature's route** (`RouteFollower::OnObjectAdded`'s box test): not reached, since no
  openblack caller adds an obstacle with notify 1. The original's callers: `Fixed::InsertMapObject` and
  `MultiMapFixed::InsertMapObject` (bw1-decomp Fixed.cpp:375 and :450: a fixed object put on the map adds itself to
  every planning, following or re-planning follower of a creature that must avoid it) and the creature's stop in
  `DoMovingAction` (Creature3D.cpp:6141: a creature that has just stopped adds itself to the moving creatures'
  followers that must avoid it). This line goes when those callers are wired.
The footpath walk, fork issue #74 (done: W1, W2 and the move states). Fix rows left:

- **W4** (done, audited 2026-10-23): every live caller of SetupMoveToOnFootpath 0x5EDD20 is on the footpaths in
  openblack: the worship walks (GotoWorshipSiteForWorship 0x76BD43, ArrivesAtWorshipSiteForWorship 0x76BEA4,
  GotoStoragePitForWorshipSupplies 0x76C049, StartHidingAtWorshipSite 0x76C5B7, GotoAltarForRest 0x76CB4A;
  `VillagerWorship.cpp`, with the exe's final states 59, 43, 213 and 62), the workshop's (0x7593A0, 0x759450,
  0x759520; `VillagerSatisfy.cpp`) and the others above. Two callers are never reached in the original, so they
  stay unported ([Not in the original](../project/not-in-the-original.md)): the housewife's four (0x761D3A,
  0x761E4D, 0x761EDC, 0x76215F; nothing sets state 100, and ShallIWaitForDinner 0x7611B0 has no caller), and
  AddMoveToPosWithFootpathReaction 0x5F1067, whose only caller, the town celebration 0x767153, gets priority 0
  because GPlayer::ClaimTown makes reaction 31 from the town centre itself. What is left of the footpath walk is
  saving state 29 with the saved games (#186).
- **The walking villager's altitude**: the arrive test (`MapCoords::operator==` 0x605660) and the next footpath's
  nearer end (GetMetresDistanceSq 0x605FB0 at 0x5EDFA5 / 0x5EDFB2) read Living Pos +0x14 with its altitude;
  openblack's `living_footpath::PosOf` gives altitude 0. Pos.altitude (+0x1C) is 0 after any landing
  (`Villager::EndPhysics` 0x5F0BA8) and the ground walk (`MobileWallHug::MoveTo` 0x60AF20) never writes it (only the
  flyers' `MoveTo3D` does), so openblack's 0 matches for a landed villager. Still open: a villager's altitude at
  creation, from its creation MapCoords (the oracle on a fresh villager's Pos after `SetupMobileMoveToPos`).
- **An empty next footpath**: the original reads through a null node (0x5EDFA2); `footpath_walk::ReachNode` ends the
  walk instead (a guard). Whether a link's list can hold an empty footpath is not shown.
- **Save / load of row 29** (`Villager::SaveFootPath` / `LoadFootPath`): with the saved games.
- **GenerateRoutesBetween**: the dance's (0x50B94F, `DanceSystem::Create`) and the moved town artifact's (0x426560,
  `ArtefactSystem::JoinDance`) are ported; sub_535CD0 is never reached (above). Its ends use
  `living_footpath::GetArrivePos` (#74 F).
- **The stops still instant after the walk series (the braking stop at the action end and its callers)**: a walk step whose time is up, the faint without a fight, the fights'
  start, faint, taunt and duel (the original's forced fight brakes but then turns and plays an action before walking to
  the arena, its faint resets the route as the faint starts, its taunt's turn is asked again every turn, its duel waits
  for a standing body), and a creature's removal: each is matched to the original's before it changes.
- **A new obstacle on a moving creature's route** (`RPFollow::OnObjectAdded`,
  [RPFollow](#rpfollow-the-creatures-route-follower)): openblack's `RouteFollower::OnObjectAdded` is ported, but no
  caller adds an obstacle with notify, so the follower's test of the box (+0x5000C..+0x50018, which has no writer in the
  exe) and its re-plan are never reached; the line goes when the original's callers are wired.
- **The Pied Piper's near-41.9 walks** ([the quest](../project/progress/story/silver_scrolls/the_pied_piper.md)): in a
  quest run with a size-2 profile creature, after the script's walk up to the piper (near 10) the creature is sent
  again and again to the piper by the crèche with a near of 41.9; what sends these walks is not identified (no script
  native comes just before them in the log). Their failure is the original's at that size
  ([the walk on the follower](#the-creatures-walk-on-the-follower): a grown creature can't get out of the Land 1 pen).
- **A creature's route re-aimed from inside the pen** ([RPFollow](#rpfollow-the-creatures-route-follower)): in Land 1's
  leash lesson the creature sent back by the leash from inside the pen walks off westward. The leash's walk back is
  the original's (the send, the restart guard, the end, and a send while moving, which only gives the live follower a
  new destination: [the walk back](../creature/leash-rope.md#pending), xref notes #3335 to #3338), so the
  difference is in how the follower re-aims its route from a start inside the pen. Not yet compared with the
  original's `RPFollow` (the replan from the pen's start point and the way it picks); a trace of the follower's
  points in both games is the next step, after the written re-plan turn-off (above) lands.
