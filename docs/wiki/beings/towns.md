# Towns

## Summary

A town is a group of homes, civic buildings and villagers that belongs to one player (or to nobody, the neutral
player). Once a turn the town looks after itself: its buildings work, its villagers' needs (the town desires) are
weighed, worshippers are sent to the temple, a damaged building is chosen for repair, a fire or an attack starts an
emergency, the temporary resource piles are tidied, and its missionaries act. Then the belief its villagers gave each
player this turn is added up. Needs the owner leaves unmet wear the owner's belief down, and when another player's
belief becomes the strongest the town changes hands.

The town also counts its villagers and buildings (TownStats), keeps a meeting point, plans and repairs its buildings,
runs its workshops and graveyard, and records who attacked it and how hard (aggression), which feeds its desires for
protection and mercy.

openblack reproduces the town's turn, the belief fold, the temporary pots, the stats, the lists and the aggression.
This page owns the processing of the town desires; what a villager does with them is on
[Town desires and distribution](villager-decisions.md#town-desires-and-distribution), and the creature's side on
[Mimicry](../creature/mimicry.md). The building sites themselves are on
[Building sites](../world/building-sites.md).

## The original

The x87 chains below are float steps: the game logic runs with the control word at 24 bits (fn_007DEE00, `and cw,
0xFCFF` at 0x7DEE0D), so each fadd / fmul / fdiv rounds to float. No random number is drawn in the town's turn except
where a `GameFloatRand` or `GameRand` is named.

### The town's turn

#### Town::Process step by step

**The player loop.** `GPlayer::ProcessPlayers` 0x649A20 walks the players in the order of `GetNextPlayerAndNeutral`
0x550980 (slots 0..7) and calls `GPlayer::Process` 0x6494E0 for each. That runs `Citadel::Process` 0x462D70 first
(+0xA48, 0x649525), then each town of +0xA50 (next +0x75C) → `Town::Process` (0x649551). The player's alignment
(0x6496C5) comes after its towns.

**A town that changes hands in its fold.** When step 19 converts a town (`GPlayer::TakeOverTown` fn_00649810 →
fn_0064C090), the town moves to the tail of the new owner's list with next = 0 (0x64C0C6). The old owner's walk reads
that next (0x649545 `mov edi, [edi + 0x75C]`) and ends there, so its later towns wait until the next turn. The taken
town is processed again this turn when the new owner's slot comes later (always for the neutral player).

**The steps of `Town::Process` 0x747380**, in order:

| Step | Where | What it does |
|---|---|---|
| 1 | Town::Process 0x747390 | +0x5E4 = 0 ("a plan was asked for this turn") |
| 2 | 0x747396 | fn_43BD00(&+0x790) 0x43BD00 prunes the building sites: for each node (next read first) whose root is null, not available, or built and repaired, the site's ToBeDeleted when it is available |
| 3 | 0x7473A0 | +0x5C8 = GetBaseInfluence 0x73FD40 |
| 4 | 0x7473AD | fn_747600: when turn % GTownInfo +0x4C processAbodeEvery == 0 (unsigned div, 0x747615), for each structure of +0x754 (newest first) its Process (vt +0x5FC) and, unless +0x5F8, +0x5C8 += GetInfluence (vt +0x868) |
| 5 | 0x7473BD | with a player, +0x5C8 ×= g_game +0x250078 |
| 6 | 0x7473D7 | `TownDesire::Process` 0x745AE0 (see [the desires](#process-sorting-and-queries)) |
| 7 | 0x7473DE | fn_747780: `TownArtifact::Process` 0x425FB0 of the artifacts +0x994 (next +0x20) |
| 8 | 0x7473E3..0x7473FE | every 10 turns (0x7489FE jle) fn_7489F0: n = GetWorshipersNeeded(1, 0, null) 0x73C860; n > 0 → AdjustWorshipersWorshipping(n, 1, 0) 0x73C0F0 |
| 9 | 0x747405 | fn_747660 walks the list +0x770: CallState == 5 → taken off and deleted; not available → taken off. Only the ctor, fn_73C710 and `Town::Load` write that list |
| 10 | 0x74740A..0x74741E | the football +0xEA4 (vt +0x5FC) when FootballEnabled |
| 11 | 0x747428 | fn_747750: the town's spell icons +0x778 (next +0x110), vt +0x5FC |
| 12 | 0x74742F | fn_73D850: each available desire flag +0x9B0[i]: Process and +0x58 = R(+0x5C); a flag not available → null |
| 13 | 0x747436 | `Town::ProcessPlayerInteract` 0x73DEC0: Protection +0xEC0 / Mercy +0xEBC from the aggression slots (see [Town aggression](#town-aggression)) |
| 14 | 0x74743D | `Town::ProcessTownRepairs` 0x747DE0: at most one new site (see [Repairs](#repairs-and-the-towns-building-sites)) |
| 15 | 0x747444 | `Town::ProcessTownEmergency` 0x7477A0 (+0xF1C, +0xEC4; see [Emergency](#the-emergency)) |
| 16 | 0x74744B | `Town::UpdateAttitudeToCreature` 0x7437F0 |
| 17 | Town::Process 0x747450..0x7474A0 | the temporary pots (see [The town's temporary pots](#the-towns-temporary-pots)) |
| 18 | 0x7474A2..0x7474FD | the missionaries +0x99C: `MissionaryControl::Process` 0x7567E0, the unavailable ones taken off, --+0x9A0 |
| 19 | 0x7474FF..0x747506 | the belief fold fn_4383D0(+0x798, town), unconditional (see [The town's belief](#the-towns-belief)) |
| 20 | 0x74750B..0x747523 | with a player, fn_4141F0(player +0x60, town): the alignment from the desires (TownDesire fn_7466D0 with GetDesire, info +0x4C / +0x50 / +0x54 and AlignmentTurns +0x410; fn_414660; `CAlignmentHistory::Add` 0x414D40) |
| 21 | Town::Process 0x747528..0x747544 | the pulse: +0x5EC != 0 → +0x5E8 = 0; then +0x5EC = +0x5E8 |
| 22 | Town::Process 0x747536..0x747574 | +0xF20 != 0 → --; at 0 `Town::SetTownEmpty` 0x741080 (0x747559); with people → 0. Town::RemoveVillager 0x73E2CD sets it to 50 (0x73E2CD) |
| 23 | 0x747574..0x74759E | not neutral: fn_555240(+0x5C8, +0xF24): abs(a − b) > 0.01 → g_game +0x250174 = 1 (redraw the influence) |
| 24 | 0x7475A3..0x7475E8 | ShuffleDue → `ShuffleVillagersAroundAbodes` 0x741540, one move a call (see [Moving villagers between houses](villager-life.md#moving-villagers-between-houses-shuffle-step-24)) |

**ShuffleDue (step 24)** is ftol((u64) town id +0x5B4 × 20 [0x8C7658] + turn) % GTownInfo +0x168
shuffleVillagersEvery == 0. In detail: 0x7475A3..0x7475D2 `fild qword` of the (u32) id, `fmul` 20 (a float, 0x7475C8),
`fiadd` of the (int32) turn, `__ftol`; then 0x7475DA..0x7475E4 an unsigned div by info +0x168.

**Football.** `GGame::FootballEnabled` [0xD0196C] is 0 in .data (its static initial value, a single-player start).

#### What the buildings do in step 4

- **Who runs `Abode::Process` 0x404440** as its vt +0x5FC: Abode, StoragePit, Creche, Wonder and Graveyard. Field
  0x529020, TownCentre 0x743DF0, Workshop 0x7797F0 and SpellDispenser 0x722A70 override it:
  - Workshop::Process 0x7797F0 does its own part first and `Abode::Process` at the end (see [Workshops](#workshops));
  - `SpellDispenser::Process` 0x722A70 calls `Abode::Process` first (0x722A73), then its own part;
  - the Field's override is its second caller; the first is ProcessFieldsTurn, every turn.
- **`Abode::Process`** starts with `MultiMapFixed::Process` 0x52F700 (0x404443): with a building site (+0x74), the
  site's Process (vt +0x100). `StandardBuildingSite::Process` 0x43D8D0: +0x644 && its IsAvailable != 1 → +0x644 = 0;
  `CitadelBuildingSite::Process` 0x43D660 does the same for its six slots.
- **`TownCentre::Process` 0x743DF0**, after the site's Process:
  - the villager part of `Abode::Process` (TownCentre::Process 0x743DF4);
  - 0x743DF9..0x743E05: the totem +0xCC's Process (vt +0x5FC, `TotemStatue::Process` 0x737F40);
  - 0x743E0B..0x743E20: the town (vt +0x48) and its +0x5F8;
  - 0x743E22..0x743E51: when the head of the centre's site's scaffolds (site +0x74, +0x20, node +4) is still adjustable
    (ValidForPlaceInHand(0), vt +0x6FC, == 1) the flag is kept;
  - otherwise 0x743E53..0x743E63: +0x5F8 = 0 and `GGame::ForceNeedUpdateInfluence` 0x555270.

#### Repairs and the town's building sites

The site objects themselves are on [Building sites](../world/building-sites.md); this is the town's side.

**Choosing a repair (step 14).** `Town::ProcessTownRepairs` 0x747DE0 runs ChooseTownRepair (0x747DE6..0x747E68), two
loops sharing one best (0 to start, the strictly larger wins, the first on ties):

1. the plans with +0x30 (wasBuilt), oldest first, by GetDesireToBeRepaired (vt +0x514, 0x648910);
2. then (0x747E2A..0x747E68) the abodes of +0x754 (next +0x9C) with no site (+0x74, 0x747E39) and no +0x58 bit 2
   (0x747E3F), by Abode::GetDesireToBeRepaired 0x406970 (vt +0x8D8) against the SAME best: an abode must beat the
   best plan.

The abode wins over the plan; both empty: nothing. Then:

- an abode (0x747E6A..0x747E7F) → +0x58 |= 4 (0x747E6E) and `Town::AddBuildingSite(MultiMapFixed*)` 0x73B8E0
  (0x747E75): its site copies the bit (+0x638 = 1, a repair site for GetBestRepairBuildingSite), and return, also when
  a plan was better before;
- else a plan (0x747E80..0x747E87) → `AddBuildingSite(PlannedMultiMapFixed*)` 0x73B860 (CreatePlanned WITH the fixed
  check; a rebuild plan's building gets bit 2 at 0x4057CC, so its site is a repair site too).

At most one site a town turn.

**Adding sites.**

- `Town::AddBuildingSite(MultiMapFixed*)` 0x73B8E0: Create(building), InsertBuildingSite. A site on an existing
  building; its other callers are `Abode::ReduceLife` 0x405E7A and `Villager::SetupBuildingObject` 0x758565. +0x638 is
  the building's +0x58 bit 2, so a site is a repair site only after ProcessTownRepairs (0x747E6E) or a rebuild plan's
  conversion (0x4057CC); a rock-damaged house's is an ordinary one.
- `Town::AddBuildingSite(PlannedMultiMapFixed*)` 0x73B860 (AddBuildingSiteFromPlan): b = plan->CreatePlanned(0.0) (vt
  +0x500, WITH the fixed check); site = b->CreateBuildingSite; AddBuildingSite(site). Null when the plan could not be
  built.
- `AddBuildingSiteNoFixedCheck` 0x73B8A0 (from `Scaffold::TryToBuildPlannedBuilding` 0x6E914C), with a plan that is in
  no town list (a scaffold's `PlannedAbode::CreateNoInit` 0x4055C0 one): CreatePlannedNoFixedCheck(0.0) (vt +0x504,
  0x405770) steps 1..4, then the site. The plan's own step 5 (PlannedAbode::ToBeDeleted) finds it in no town list and
  is the caller's.
- `Town::AddBuildingSite(BuildingSite*)` 0x73B910 (InsertBuildingSite): already in the list → nothing (no pulse);
  else TownStats::Add(site) 0x749AA0 when its GetTown == this, a node at the head of +0x790 (+0x794++), and the pulse
  +0x5E8 = 1, +0x5EC = 0 (0x73B96D..0x73B977).
- `Town::RequestBestPlanned` 0x73A650 (0x73A650..0x73A684): GetBestPlanned(mask 4) → AddBuildingSiteNoFixedCheck;
  true when a site was made. Caller CheckSatisfyCivicBuildings 0x758ECA; the +0x5E4 flag is the caller's (`Town::requestedPlanThisTurn` in openblack).
- `Town::RequestANewAbode(type)` 0x73B330: GetBestPlanned(mask 2) → AddBuildingSiteFromPlan (with the fixed check);
  the type is not used. Caller CheckSatisfyAbodesDesire 0x758E6C.

**Removing sites.**

- fn_73B990 0x73B990: every node of the site out (TownStats remove fn_749B50). A site's ToBeDeleted calls it in its
  step 2 for every town of g_game +0x205C84 that lists it; its step 5 empties its own list (LHLinkedList::Remove 0x43DBB0
  until empty) and does NOT reset +0x634.
- `Town::RemoveBuildingSite(MultiMapFixed*)` 0x73BA20: the first node whose site's GetBuilding is the building → its
  ToBeDeleted(0) (vt +0xC), true; else false.

**Queries.**

- `Town::IsBuildingHappening` 0x73E2F0: +0x794 != 0 (caller `Villager::CheckNeededForBuilding` 0x75834A).
- `Town::GetBuildingSiteInList` 0x73CE40: the site whose GetBuilding is the building, or null.
- `Town::IsBuildingSiteValid` 0x73CF00: in the list, GetBuilding != 0 and !(IsBuilt && IsRepaired) (vt +0x890 /
  +0x88C).
- `Town::GetBestBuildingSite(pos, includeFull)` 0x73CF60: the head first; score = GetDistanceInMetres(pos, the
  building's GetNearestEdgeToPos(pos)) × (GetDesireForVillagers × 0.9 + 0.1), below 99999 (0x47C34F80 = 99999.0, the
  start score; strict); the minimum wins (literal: it favours the sites that need FEWER builders). includeFull is
  `villager +0xF2 == 4` (BUILDER).
- `Town::GetBestRepairBuildingSite` 0x747EA0: among the sites with +0x638, the strictly largest GetDesireToBeRepaired
  above 0 (first on ties).
- What TownDesire reads of the sites and plans, in the list orders (+0x790 head first, +0x9A8 oldest first):
  GetDesireForVillagers 0x43BD70 (To_Build 0x748640), +0x634 and fn_43BBD0 (ModificationToBuild 0x746400), the plans'
  GetDesireToBeRepaired 0x648910 (Repair 0x7486B0). The pruned sites are out of them.
- fn_73B620 0x73B620 (caller `Villager::Building` 0x758D71): TownStats +0xEC (Town +0x6FC) += u (float), the wood used
  building.

#### The building site's own functions

The town reads these; the full rules are on [Building sites](../world/building-sites.md).

**Making a site.** `MultiMapFixed::CreateBuildingSite` 0x52F590 (`new 0x648`, Fixed.cpp line 0x50C) →
BuildingSite(MultiMapFixed*) 0x43B7E0: +0x638 = building +0x58 bit 2; root; building +0x74 = it (fn_52E3F0); +0x640 =
GetLife, or 1.1 × life − 0.1 ([0x8AB230], [0x8AB22C]) when the building has no DestructionMesh (+0x90) and IsBuilt;
the ring (fn_43CDB0 → PosBuilder::Process 0x43AE10). It is not put in a town: InsertBuildingSite does that. A
workshop's CreateBuildingSite (0x77A6D0) is a WorkshopBuildingSite (vtable 0x8C6F24). fn_43CDB0 0x43CDB0 (both
ctors): GetBuilding && its 3D object → PosBuilder::Process(ring, +0x40, IsFootball vt +0x464).

**Reading a site.**

- GetBuilding 0x43BC70: +0x14 && its IsAvailable (vt +0x2C) ? GetBuildingObject (vt +0x8BC = this) : 0.
  GetRootBuilding 0x43BCA0 = +0x14.
- GetTown 0x43C0B0: root ? root->GetTown : 0. The citadel heart's and a worship site's GetTown are MultiMapFixed's
  0x4220A0 = 0.
- GetResource 0x43C5B0: pile ? pile->JustGetResource(type, 0, 0) (vt +0x94) : 0; a site pile answers its own amount
  (PotStructure::GetResource 0x66EF47). `CitadelBuildingSite::GetResource` 0x43D320: the sum of the six slots'
  JustGetResource(type, 0, 0) (vt +0x94).
- GetWoodForStats vt +0x104 0x43C5E0 = GetResource(WOOD); `WorkshopBuildingSite::GetWoodForStats` 0x43DB90 = 0: a
  workshop's site adds nothing to TownStats +0x100.
- `BuildingSite::GetBuildersNeeded` 0x43BC00 (signed, may be negative). fn_43BBD0 0x43BBD0: GetBuilding ? its info
  +0x110 MaxVillagerNeededToBuild : 0.
- GetDesireForVillagers 0x43BD70: clamp((float)GetBuildersNeeded / (float)fn_43BBD0 + +0x63C, 0, 1); a 0 maximum
  divides by 0 (literal).
- GetDesireToBeRepaired 0x43BE00: GetBuilding ? GetDesireForVillagers × the ROOT's GetDesireToBeRepaired (vt +0x8D8) :
  0.
- GetClearAreaRadius 0x43BDE0 (factor [0x8C6C98] = 1.2; caller SetupBuildingObject 0x7584EE). GetRadius 0x43D050 (vt
  +0x60), caller 0x759688.
- SetRepairBase writes +0x640 (`Abode::ReduceLife` 0x405EA7). SetDesireBoost: +0x63C = boost (fn_464F50 0x464FCE,
  ForceBuildingOfPlannedAtPos 0x73E560).

**Builders and scaffolds.**

- AddBuilder 0x43BE40 (caller EnterBuilding 0x759794): the duplicate search is dead (literal): a node at the head,
  +0x634++. RemoveBuilder 0x43BE90 (caller ExitBuilding 0x7597F7): every node of the villager out, then +0x634-- once
  (also when it was not there).
- AddScaffold 0x43BEF0 (caller `Scaffold::TryToBuildPlannedBuilding` 0x6E917A): a node at the head of +0x20, +0x24++;
  the scaffold's +0x68 = this (0x43BF18). RemoveScaffold 0x43BF20: every node of the scaffold out (+0x24-- each); the
  scaffold's +0x68 is not cleared (the callers do).
- fn_43D080 0x43D080: GetBuilding->BuildBy(x) (vt +0x900, abodes::BuildBy 0x52ED40). Caller Building 0x758D62, x = u / GetWoodValue,
  the wood value read BEFORE RemoveResource.
- `BuildingSite::ShouldIGetWood(villager)` 0x43C680 (callers 0x75871A, 0x758F94): "close to the building" is [0x8C6CA4]
  = 50.0; its GetDistanceModifier maximum is 0x459C4000 = 5000.0. `resourceDropoffPos` is the villager's
  GetResourceDropoffPos(WOOD) 0x753E20, asked only at 0x43C78E.

**Where the builders stand.**

- GetRandomBuildPos(out, Object* o, int* idx) 0x43CDE0 (vt +0x128; one GameFloatRand; callers GotoBuildingSite
  0x758A7F, ReenterBuildingState 0x758FB3): GameFloatRand(`push 0x3FC90FDB`, pi / 2, 0x43CE08; BuildingSite.cpp line
  0x37C) − [0x8C6C9C] = 0x3F490FDB (pi / 4, 0x43CE1F), so +-45 deg.
- `BuildingSite::GetNearestEdge` 0x43CE40's bounds: [0x8C6CB4] = 0xC196CBE4 (−6 pi: below → index 0, 0x43CE47) and
  [0x8C6CB0] = 0x4196CBE4 (6 pi: above → pi, then straight to the multiply 0x43CE71 → 0x43CEC0, the same result).
- GetNextPosFromIndex(out, int* idx) 0x43CF40 (vt +0x124; GameFloatRand then GameRand(2); caller Building 0x758DC0):
  step = 2.0 / (Get2DRadius × 2 pi × 0.0078125).
- The ring entry ArrivesAtBuildingSite 0x758B2B..0x758B6A reads directly: (ftol(x × 6553.6), ftol(z × 6553.6), 0);
  only for 0 <= index < 128 (0x758B12..0x758B25).

**The ring (PosBuilder::Process 0x43AE10).**

1. Every entry = o +0x38 (the translation); the static best-distance array 0xC58CD4[128] = 0.
2. Without a mesh, 0x43AEE8..0x43AF74: a_i accumulated (fadd), x = sin a × R + c.x, z = cos a × R + c.z (the mirror of
   the mesh branch, literal), y = GetAltitude(MapCoords(ftol(x × 65536 × 0.1), ftol(z × 65536 × 0.1))); no post-pass.
   The two products before the ftol (0x43AF18 / 0x43AF1E) are each rounded to float (the 24-bit FPU, 0x7DEE0D).
3. With a mesh, 0x43AF77..0x43B3EA: every triangle of every primitive of every sub-mesh, in file order; the lowest (L)
   and the second lowest (S) vertex by local y (0x43B014..0x43B0A4); to world (0x43B0D0..0x43B1F2); u = normalize(S −
   L) in 3D (0x43B1F6..0x43B29C); 0x43B2A2..0x43B316: a = (float)i × step (fild; fmul), P2 = (c.x + R cos a, c.y,
   c.z + R sin a).
4. 0x43B3F0..0x43B4D2: each entry pushed 1 m outwards along its 3D direction from c (no hit: from the translation).

The helpers: fn_43B5A0(A, B, P) 0x43B5A0: P inside the XZ box of A..B, both ends inclusive, y not tested.
fn_43B630(L, S, u, c, P2, &hit) 0x43B630: the ray segment c → P2 against the edge L → S in XZ. fn_43B4F0 0x43B4F0: the
line L + t u against the vertical plane through c with normal N = (−D.z k, 0, D.x k); the hit's y is interpolated
along the edge; 1 / sqrt is exact (the InverseSquareRoot approximation does not move it).

**The wood piles.**

- vt +0x108 GetPileWood(pos): Standard 0x43D6E0 = +0x644 (the position ignored). CitadelBuildingSite::GetPileWood 0x43D500: pos NULL → 0; else the slot nearest to pos, GetDistance(pos, pile +0x14) (fn_74CCE0) as (float)(uint32)
  strictly below the best (FLT_MAX [0x8C6B50]: the first on ties; no IsAvailable test).
  `WorkshopBuildingSite::GetPileWood` 0x43D9B0: root && root->IsWorkshop (vt +0x310) ? root +0xD0 : 0.
- CreatePileWood (Standard) 0x43D760: a StandardBuildingSite (dynamic_cast) that is available and has no pile
  (GetPileWood(0)): a "Magic Wood" pot (GPotInfo 0xD4D1C4 = 9) at GetResourcePosAndYAngle(WOOD, −1).
  `WorkshopBuildingSite::CreatePileWood` 0x43D9F0: no GetPileWood(0) (the workshop's +0xD0) and IsAvailable → pos =
  GetResourcePosAndYAngle(WOOD, −1, &angle) (the workshop's special point 4, fn_779690, as there is no pile);
  Pot::Create(pos, 9, 0, GetBuilding, 0, 0, angle, 1.0, 1): the pile's PotStructure +0x78 is the workshop, so its wood
  goes to the workshop (PotStructure 0x66ED70 / 0x66EE10 / 0x66EF00); then SetPileWood (vt +0x10C, a no-op 0x43D9E0)
  leaves +0xD0 null. Workshop::CreatePileWood at its Create normally made one.
  `CitadelBuildingSite::CreatePilesOfWood` 0x43D2A0 (its ctor 0x43D1E0, AddResource 0x43D370): for each slot i,
  GetResourcePosAndYAngle(&pos, WOOD, i, &angle = 0) (vt +0x114, 0x43D2C9), and an empty "Magic Wood" pot there when
  the slot is empty or not available.
- GetResourcePosAndYAngle(out, type, index, float* angle) (vt +0x114; `angle` may be null):
  - Standard 0x43C220: for WOOD and root->IsWorshipSite (vt +0x304, WorshipSite 0x55DCA0), StandardBuildingSite::GetResourcePosAndYAngle 0x43C24D..0x43C25D: the
    local point (9, 0, −50) (0x41100000, 0, 0xC2480000) through the root's 3D object matrix (0x43C26A..0x43C2CF) as a
    MapCoords (0x603160); the angle (when asked) its GetYAngle (vt +0x508). WOOD without a pile (0x43C361..0x43C426):
    a = Get3DAngleFromXZ(door, p) (door → centre), + 0.392699 − GameFloatRand(0.785398) (after the angle); d =
    GetDistanceInMetres(p, door again) + 4; p + GetPosFromAngle(a, d); the Y angle a − pi ([0x8C36A0]). The pile is on
    the far side of the building from the door: [0x8C6CA0] = 0x3EC90FDB (pi / 8, 0x43C3B8) − GameFloatRand(`push
    0x3F490FDB`, pi / 4, 0x43C3AE; BuildingSite.cpp line 0x2B8), the pile's +-22.5 deg, and 4.0 m past the door's
    distance. The door is `MultiMapFixed::GetDoorPos` 0x52E370 (vt +0x864).
  - `CitadelBuildingSite::GetResourcePosAndYAngle` 0x43D470 (any type): root->GetCitadel (vt +0x114) →
    GetWorshipSiteAngle(index) 0x463610 − 1.1424 ([0x8C6DEC] = 0x3F923A14), the root's MapCoords +
    GetPosFromAngle(that, 22.0) 0x74D580 (`push 0x41B00000`, 0x43D4B9), so slot `index` is 22 m out; the angle
    out-parameter is not written. `Citadel::GetWorshipSiteAngle` 0x463610: the heart's Y angle (vt +0x508) + slot ×
    0.897598 ([0x8C836C] = 0x3F65C8FA, 2 pi / 7).
  - `WorkshopBuildingSite::GetResourcePosAndYAngle` 0x43DA80: WOOD with a pile (GetPileWood vt +0x108) → its position
    and, when asked, its Y angle (vt +0x508); WOOD without one and a root → the workshop's fn_779690 (the special point
    4; the root is taken as a workshop, literal); anything else the root's position, the angle not written.
- The site whose pile this is: PotStructure's IsLinkedToThisBuildingSite 0x43D830 through the pile's structure +0x74
  (0x66EF12 / 0x66EE1E / 0x66EDB9), or null. `CitadelBuildingSite::IsLinkedToThisBuildingSite` 0x43D580: one of the
  six slots, while the piles are still linked to the heart (ToBeDeleted 0x43D220's SetMultiMapFixed(0) unlinks them).

**Wood in and out.**

- vt +0x9C AddResource (Standard 0x43C490): WOOD only; the pile (made when missing, CreatePileWood) JustAddResource
  (vt +0x8C). `pos` is the villager's position (ArrivesAtBuildingSite 0x758BEB); the Standard site ignores it. Returns
  what was added.
- `CitadelBuildingSite::AddResource` 0x43D360: pos NULL → nothing (0x43D370); else the nearest slot (GetPileWood(pos)
  vt +0x108), none → CreatePilesOfWood and once more; JustAddResource(type, n, poisoned) (vt +0x8C, any type); then
  GetTown (0 for the heart's site) +0x710 += it.
- `WorkshopBuildingSite::AddResource` 0x43DB20: root ? root->AddResource(type, n, status, b, pos, 0) (Workshop
  0x779E00) : 0.
- RemoveResource 0x43C530: WOOD: pile = GetPileWood(status ? status->vt +0x100 : 0) → JustRemoveResource(WOOD, n, 0)
  (vt +0x90); the stats' −= (float) removed. `CitadelBuildingSite::RemoveResource` 0x43D3F0: with an interface
  (0x43D3F6) `BuildingSite::RemoveResource` 0x43C530 (WOOD: GetPileWood(IS->GetPos), the nearest slot,
  JustRemoveResource); without one the slots in order, each available pile JustRemoveResource(type, what is left, 0)
  (vt +0x90, any type) while something is left (0x43D422). `WorkshopBuildingSite::RemoveResource` 0x43DB60: root ?
  root->RemoveResource(type, n, status, bool*) (Workshop 0x779EC0) : 0; the site's callers pass no status.

**Deleting a citadel site.** `CitadelBuildingSite::ToBeDeleted` 0x43D220 first, for each of the six slots:
SetMultiMapFixed(0) (vt +0x868); with wood (JustGetResource(WOOD) vt +0x94) and !(g_game +0x14 & 0x8000)
Pot::SetupReaction 0x66D660, else its ToBeDeleted; SetPileWood(0) (vt +0x10C, the no-op 0x43D180: the slots keep the
piles, literal). Then `BuildingSite::ToBeDeleted` 0x43B960, whose pile step finds GetPileWood(0) = 0.

#### Plans

A plan is a building the town means to build. It is only data: the planned objects are invisible
(`PlannedMultiMapFixed::Draw` 0x648930 is a bare `ret`).

**Making plans.**

- The script: CREATE_PLANNED_ABODE 0x715629: `PlannedTownCentre::Create` 0x7444D0 when the abode's type is 0x404
  (TownCentre), else `PlannedAbode::Create` 0x405600; Town::AddPlanned.
- From a building: `PlannedAbode::Create(Abode*)` 0x405660 (from `Abode::MoveAbodeToPlannedAbodes` 0x40453E): a plan
  where the building stands. PlannedAbode(Abode*) 0x405580 → PlannedMultiMapFixed(MultiMapFixed*) 0x648820 (+0x48 = 0,
  then Init's town): its position, GetYAngle, GetScale, info, the creation turn and +0x30 = the building's +0x58 bit 3
  (0x6488B4..0x6488CB: a rebuild plan when it was built). Always a PlannedAbode (a town centre's too). 0x40568C..0x405696:
  +0x38 = b +0x64, then b +0x64 = 0 (the footpath link). 0x40569D..0x4056A3: `PlannedAbode::Init`(b->GetTown) 0x4055A0:
  +0x48 = town → `Town::AddPlanned` 0x73D080, which writes the creation turn +0x3C (0x6488AB).
- A citadel heart's plan: PlannedTownCitadelHeart (vtable 0x8C9D4C, ctor 0x467DD0): its info is the
  GCitadelHeartInfo (+0x40 = 0xC5E270 + N3 × 0x158). GetAbodeType 0x467E30 = 0x804, PlannedTownCitadelHeart::IsCivic 0x467E10 = 0 (`xor eax,
  eax`), GetTown 0x56FF10 = 0. Planned citadels belong to the town's player, not to the player named in
  CREATE_PLANNED_CITADEL (0x467EF0).

**The plan's fields.**

- +0x30 wasBuilt: "was a built building" (a rebuild plan: the building's +0x58 bit 3, 0x6488B4); 0 for the script's
  (0x648803). Read by GetDesireToBeRepaired 0x648910 and CreatePlannedNoFixedCheck 0x4057C5.
- +0x3C: the creation turn (g_game +0x205A40, 0x6487F6); its age 0x6488E0 = turn − it.
- GetDesireToBeRepaired vt +0x514 0x648910 = +0x30 ? info +0x118 desireToBeRepaired : 0.
- GetAbodeType: `PlannedAbode::GetAbodeType` 0x4061E0 = info +0x120 (PlannedTownCentre the same); PlannedMultiMapFixed's
  0x465570 = 1 is never a town's plan; PlannedTownCitadelHeart 0x467E30 = 0x804. The info's GetAbodeType (vt +0x40) is a
  GAbodeInfo's +0x120, the citadel heart's 0x464380 = 0x804.
- IsCivic vt +0x50C: PlannedAbode 0x4060C0 = the type in {0x14, 0x24, 0x44, 0x84, 0x100, 0x204, 0x404, 0x1004,
  0x2004} (jump table 0x406118 / 0x406120); PlannedTownCentre::IsCivic 0x55DBE0 = 1; the citadel heart's 0x467E10 = 0 (its type
  0x804 has the civic bit, but TownStats +0x24 does not count it).

**Building a plan.**

- `PlannedAbode::CreatePlanned(life)` 0x405710 (vt +0x500): `GAbodeInfo::IsOkToCreateAtPos` 0x404B10 (the fixed check)
  or null; else CreatePlannedNoFixedCheck. PlannedTownCitadelHeart::CreatePlanned 0x467EA0.
- CreatePlannedNoFixedCheck(life) vt +0x504: PlannedAbode::CreatePlannedNoFixedCheck 0x405770 and PlannedTownCentre 0x744550. The life argument is
  ignored (underConstruction = 1). Returns the building or null (the plan survives). A PlannedTownCitadelHeart is
  converted before, by its caller (CitadelArchetype::CreatePlannedNoFixedCheck 0x467EF0, the life passed on). The steps:
  1. PlannedAbode 0x4057AC: Abode::Create(&+0x14, info, town, angle, GetScale, food 0, wood 0, life, 1, 1) 0x402E20 →
     the class ctor → MultiMapFixed ctor 0x52E1E0(..., percent = life, underConstruction = 1), Abode::Init (food and
     wood 0; no MakeFunctional as it is not built) and CreateAbodeSurroundingObjects 0x403E00. PlannedTownCentre
     0x744550: TownCentre::Create 0x743C90 (pos, info, town, angle, scale, life, 1) directly.
  2. PostCreatePlanned 0x648C50 (the footpath link moves to the building).
  3. With the plan's town, `Town::CheckWhenNewBuildingCreated` 0x741500 (see [the congregation
     point](#congregation-point-and-clear-areas)).
  4. 0x4057C5..0x4057CC (PlannedAbode only, not PlannedTownCentre): +0x30 → building +0x58 |= 4.
  5. The plan deleted (PlannedAbode::ToBeDeleted 0x4056B0 → `Town::RemovePlanned` 0x73D0D0: unlink, +0x9AC--, the later
     indexes move down; TownStats remove fn_749B10).

**Choosing what to build.** `Town::GetDesireToBeBuilt` 0x73A1A0 (0x73A1AC b = info +0x114; 0x73A1BF type =
info->GetAbodeType) uses the constants 0.8 [0x8C4A04], −10 [0x8C7670], 0.2 [0x8AB244], 0.6 [0x8C7BDC], 10 [0x8AB414] and
0.3 [0x8AB23C], reads TownStats +0x4C (the free adult places), u = (adults + children) / 10 + 1, unsigned (div by
0xCCCCCCCD >> 3), and the types 0x204 and 0x2004; a wonder with 7 scaffolds or more is 1.0 (0x73A446). The same holds
for any GMultiMapFixedInfo; `abode` is the GAbodeInfo when it is one (the case 0x2 reads it).

#### Scaffold plans and wonders

- `Town::GetNewPlannedBuilding(out, pos, &angle, scale, n, under, tribe, limit, force)` 0x73D980: best = 0; the abode
  numbers 0..15 (`mov [esp + 0xC], 0x10`), or only `limit` when it is not −1. For each: the town's tribe (GetTribe
  0x73C840 +0x10); for the WONDER (10) the scaffold's `tribe` when not −1 and the scale WonderScale(pos) fn_73DC40; the
  info `GAbodeInfo::Find`(tribe, number). Skipped when its ScaffoldsRequired is 0 or above n (unsigned), or, unless
  `force`, when the position does not suit its mesh (fn_73DAA0(info): the static Game3DObject with the info's mesh;
  !force → pos.IsSuitableForFixed(mesh, *angle, s, under) 0x603DC0, 0x73DA34). The strictly larger
  GetDesireToBeBuilt(info, n) wins: out's info, angle (*angle) and scale. The scale is reloaded every iteration
  (0x73D9C4..0x73D9F2). Returns the best desire (0: none).
- `Town::GetWonderPower(pos)` 0x73DAF0: raw = GetRawDesire(14 For_Wonder) 0x73E420; r = 15 × raw > 15 ? 15 × raw : 15
  (`test ah, 0x41; jne` takes 15 on <=, and on NaN); plus what the objects around pos are worth
  (GetTownArtifactValue + GetImpressiveValue × 0.1, within r); at least 0.25. Constants [0x8C2C40] = 15.0 (the radius
  factor and floor), [0x8AC404] = 0.1, [0x8AB3D4] = 0.25. The walk is GetMapCellSpiralSizeFromRadius(r)
  0x74F520 cells; literal: GetFirstIterator 0x6034D0 is called on `pos` itself every time (`mov ecx, esi` 0x73DB67), not on the
  spiral's copy that GUtils::Spiral / operator+= 0x605470 move (0x73DBF7..0x73DC04), so the position's own cell is
  walked n times. Each object (fixed then mobile, GetMapChild vt +0x53C) with GetDistanceInMetres(pos, o) < r (`test
  ah, 1; je`): sum += GetTownArtifactValue (`GameThing::GetTownArtifactValue`, vt +0xC8), then sum +=
  GetImpressiveValue (vt +0x66C) × 0.1.
- GetDesire(14) 0x73E400 against `TownDesire::GetInfo`(14) +0x18 DesireTriggersVillagerAction.
- `Wonder::Create` 0x778E80 after the abode is made: the ctor 0x778E00 (SetToZero 0x779060), SetPower (vt +0x8C8,
  0x778ECB..0x778ED0) with edi = Create's fifth argument [esp + 0x20] (0x778EA3; inferred: the scale, as in
  Create(pos, info, town, yAngle, scale, ...), Workshop::Create's argument order), CallVirtualFunctionsForCreation (vt
  +0x658), then IsBuilt (vt +0x890) → AddToPlayer (vt +0x8DC). A script's whole wonder counts at once, a plan's from
  Built.
- `Wonder::SetPower` 0x779070: +0xC4 = power (`Scaffold::TryToBuildPlannedBuilding` calls it with GetWonderPower).
- `Wonder::AddToPlayer` 0x778FC0: player = GetPlayer (vt +0x1C), tribe = GetTribe (vt +0x928, 0x779040:
  &GTribeInfo[info +0x158], never null for a wonder with an info record); a player, a tribe and IsBuilt (vt +0x890)
  → GPlayer +0xB8[tribe] = power + it (fadd, a float) (0x778FC6..0x778FE9). `Wonder::RemoveFromPlayer` 0x778F50: the
  same tests (0x778F56..0x778F79) → GPlayer +0xB8[tribe] = it − power (fsubr). GPlayer +0xB8[tribe] is the sum of the
  built wonders' power of that tribe the player owns. Inferred: an abode's GetPlayer is its town's (Town +0x2C): none
  without a town.
- `Wonder::DeleteDependancys` 0x778E60 (vt +0x910; ToBeDeleted 0x778E40 calls it first): RemoveFromPlayer, then
  `Abode::DeleteDependancys` 0x403F00 (the caller's).

#### The emergency

`Town::ProcessTownEmergency` 0x7477A0 (step 15, 0x747444): while in the emergency the attack of this turn refreshes it,
and a worship percentage != 0 is saved in +0xEC4 (0x7477EA) and set to 0; else the storage pit or the town centre on
fire starts it; else the saved percentage comes back when the current one is 0 (0x747864) and +0xEC4 = +0xF1C = 0
(0x747869). +0xF1C is the turn the emergency started (`Town::IsInStateOfEmergency` 0x747970 reads it; 0 = none),
written by `Town::SetInStateOfEmergency` 0x7479A0 and cleared at 0x747873.

#### Process, sorting and queries

The town desires: how they are computed and sorted each turn.

**Computing.**

- `TownDesire::Process`: +0x164 = (u64)(adults + children − on the way +0x5CC − worshipping +0x5C4) in u32 arithmetic
  (0x745AE7..0x745B1E).
- ProcessDesire (fn_745CA0): doingNow +0x4DC[d] < 0 (or NaN) → 0; copies to +0x454 / +0x498; Amount / Desired read as
  unsigned qword; +0x118[d] = CallDesireFunction(d) (0x745CAC..0x745D56). It adds each desire to the network checksum
  [0xDA2770] (`GNetwork::ResetStateDebug`) (0x745D5D..0x745D6E).
- doingNow +0x4DC (town +0x510): the sum of ± amount (state table file 0x08) of the villagers' final states serving it
  (`Villager::AdjustTownModifier` 0x7535BE).
- CallDesireFunction: the tribe index of the TribeMultiplier is (GetTribe 0x73C840 − 0xDA57A8) / 0x1C
  (0x745DC6..0x745DFF). `Town::GetTribe` 0x73C840 is +0x10 (inferred: the same TRIBE_TYPE as Town +0x5B8).
- The average, every 50 turns: (2·R0 + raw1 + boost1 + boostA1 + max(R3, R4) + max(R5, R6)) / info +0x158
  divisorForAverageDesires; the warning uses info +0x15C thresholdForAverageDesiresHelpSprites and passes avg − 0.6
  (0x745BF5..0x745C8A). The player's GameStats: +0xA44 +0x70 += 1 and +0x6C += 1 − average (0x745C2A..0x745C46).
- The town-needs sum (fn_00747150) sums in this order the raws 13, 12, 9, 7, 6, 5, 4, 3, 1, 0 (+0x19C ... +0x168), ×
  0.2 ([0x8AA3AC]), clamped to [0, 1].
- Protection 0x7488A0 / Mercy 0x7488B0 (the desires' inputs) read +0xEC0 / +0xEBC.
- The TownDesire block is Town +0x34, 0x564 bytes (bw1-decomp TownDesire.h). The town object is zero-filled at
  creation (`Base::operator new` 0x4366F0 → fn_436870 `rep stosd`), so all start at 0. +0x08 / +0x4C / +0x15C / +0x234
  have no known use.

**Sorting.**

- The comparator 0x746110: a < b or unordered → 1, equal → 0, otherwise −1 (descending, NaN first).
- Sort 1 (fn_746140): elements {+0xD4 + +0x90, +0x118 + +0xD4 + +0x90, d}; sort 2 (fn_746190): {+0x90, +0x168 + +0xD4
  + +0x90, d}; both with _qsort of 17 elements of 12 bytes (0x74614F..0x7461D3).
- VC6 _qsort 0x7C7E64: after the pivot, the recursion goes to the shorter part (the test `higuy − 1 − lo >= hi −
  loguy` in bytes, 0x7C7F43..0x7C7F51); _shortsort 0x7C7FB8 is a selection of the maximum towards the end.

**Queries.**

- CheckVillagerNeededForTownDesire: t = trigger + info +0x18, capped at 1, stored as float (0x746031..0x74605F); the
  caller's IsChild is vt +0xAF8 (0x74601F).
- GetDesireSignificanceToVillager 0x746660: max(D(d) − info +0x18, 0).
- GetMostDesired 0x745E50: best = 0, index −1; desire[d] > best wins.
- GetMostSignificantRawDesire 0x745EA0: if order2[0].value (+0x348) < m (or NaN) → −1; otherwise its index.
- GetSortedDesire 0x7465D0 = &order1[k].

**Script commands.**

- GET_DESIRE: an object that is not a town → PUSH 0 with no message (0x6FCD3C..0x6FCD48); no object → "Object no
  longer valid" (0xC0D428) (0x6FCCF2..0x6FCD3B); "Invalid desire" is at 0xC0CFC0.
- SET_TOWN_DESIRE_BOOST: "Thing not valid!" at 0xC0DA54; the message goes out through ScriptErrorMessage 0x6F62B0.
- The map command TOWN_DESIRE_BOOST finds the town with FindTownWithID 0x552FA0 and the desire by name (fn_747270),
  and writes at 0x717A26.

**Without behaviour.** The debug trace 0x7457C0 and the functions without calls (0x745E80, 0x745FA0, 0x7461E0,
0x746220, 0x7465F0, 0x7466B0, 0x7468E0) do nothing in the game.

#### TownStats

The town's counts at Town +0x610. The original keeps them incrementally: Add(Villager) 0x7492E0 / Remove 0x7493C0,
Add(Abode) 0x7498C0 / Remove 0x749990, Add(plan) 0x749A60 / Remove fn_749B10, Add(site) 0x749AA0 / Remove fn_749B50.

**Villagers.** Add(Villager) (0x7492EF..0x7493A4):

- IsChild (vt +0xAF8): a child → +0x0C (and +0x3C); an adult → +0x08;
- 0x74930C..0x74931E: +0x54[info +0x1F8 sex]++ (children too): +0x54 / +0x58 are the men / women of the town (Town
  +0x664 / +0x668, TownStats::Add 0x749315);
- 0x749320..0x749341: +0xE4 += (u32) info +0x2D8 foodReqiredForDinner (fild qword: unsigned);
- 0x749347..0x749377: +0xF8 / +0xFC += (movsx) +0xF4 / +0xF6, the food and wood it carries;
- 0x74937D..0x7493A4: a disciple (flags & 0x200): NumDisciples +0xC8[+0xF2]++ (a byte).

`Town::ChildToAdult` 0x73AF50 → `TownStats::ChildToAdult` 0x749490: with an abode +0x4C − 1, +0x50 + 1; children − 1,
adults + 1 (0x7494AE..0x7494B9).

**Abodes.** Add(Abode) runs once, in MakeFunctional (0x404818..0x40483C, +0x7C bit 1), so an abode under construction
does not count. It does (0x7498C3..0x749988):

- 0x7498C3..0x749916: +0x4C += MaxVillagers, +0x50 += MaxChildren, +0x30 += both, +0x44++; dynamic_cast<Wonder*> →
  +0x48++;
- 0x749928..0x74995F: with places, +0x10++, +0x34 += MaxVillagers, +0x40 += MaxChildren;
- 0x749962: IsCivic (vt +0x8C0) → +0x1C++;
- 0x749973..0x749988: byte +0x108[info +0x124 abode number]++.

+0x4C, the free adult places GetDesireToBeBuilt 0x73A1A0 reads, is moved later in MoveIntoAbode / MoveOutOfAbode
0x7494C0 / ChildToAdult 0x749490.

**Plans and sites.** Add(plan) 0x749A60: +0x14, civic (the plan's IsCivic vt +0x50C) +0x24, wonder +0x28. Add(site)
0x749AA0: +0x18, civic +0x20, wonder +0x2C, +0x100 += GetWoodForStats (vt +0x104), only if the site's GetTown is this
town; the sites' AddResource 0x43C490 / RemoveResource 0x43C530 move +0x100 too (the same piles).

**Which abodes are civic.** `Abode::IsCivic` 0x405FF0 (vt +0x8C0) by ABODE_TYPE (+0x120): Totem 0x14, StoragePit 0x24,
Creche 0x44, Workshop 0x84 (the jump table 0x40604C for 0x14..0x84), Wonder 0x100 (0x405FFE je), Graveyard 0x204
(0x406027), TownCentre 0x404 (0x40602E), FootballPitch 0x1004 (0x406020), SpellDispenser 0x2004 (0x406038) (tables
0x406044 / 0x40604C); not Field 0x4004 nor Citadel 0x804.

**The abode info.** `GAbodeInfo::Find` 0x405B30 (0x405B3A..0x405B65) walks _AbodeInfos 0xC3C690 (stride 0x1C8), the
tribe at +0x158 of the record (ecx starts at +0x158), the number at +0x124 ([ecx − 0x34]); the first that matches
(0x405B67).

#### Abodes and their villagers

How a house counts and swaps its people. The town's homeless are in the list +0x768 / +0x76C.

**Counts and room.**

- `Abode::GetRoomLeftForAdults` 0x404660: info +0x174 − (u8) +0xB4; GetRoomLeftForChildren 0x404680: info +0x178 −
  (u8) +0xB7 (max − count, signed).
- `Abode::IsTooCrowded` 0x4046C0 (0x4046C7..0x404712): MaxVillagers 0 → 1; else fild +0xB4, fidiv MaxVillagers, fcomp
  percentTooCrowded (+0x1A0): not below (`test ah, 1; jne`) → 1.
- `Abode::GetPercentAbodeFullWithAdults` 0x407050 (vt +0x89C, 0x407057..0x407081): MaxVillagers 0 → 1; else
  GetNumAdultsInAbode 0x4070D0 ((float) +0xB4) / MaxVillagers (fidiv).
- `Abode::GetPercentAbodeFullWithChildren` 0x407090 (vt +0x8A0, 0x407097..0x4070C2): MaxChildren 0 → 1; else (u8)
  +0xB7 / MaxChildren with `div`, an INTEGER division (0x4070B3: 0 or 1), then fild.
- `Villager::IsMaleVillager` 0x55CAC0 (vt +0x44C): info +0x1F8 == 0 (no child test); IsFemaleVillager 0x55CAE0: == 1.
- VillagersOf: +0xA0 (next = villager +0xE4) / +0xA4, the head first (AddVillagerToAbode 0x40415A inserts at the head).

**Scores for choosing a house.** `CalculateScoreForAddingVillagerToAbode` (0x404B4F..0x404CA7):

- IsChild (0x404B4F): the children's places (+0x178, +0xB7) or the adults' (+0x174, +0xB4);
- max 0 → 0 (0x404B64 / 0x404BAC);
- 0x404B8A..0x404BE9: f = count / max (fild, fidiv), not below 1 → 1;
- 0x404BEF..0x404C04: room = 1 − f (fst); room <= 0 (`test ah, 0x41; jne`) → room;
- 0x404C0A..0x404C41: sameSex = the villagers of the list (+0xA0, the children too, and the villager itself if it is
  there) whose info +0x1F8 sex is the villager's, a float sum (fadd 1);
- 0x404C42..0x404C74: sex = 1 (0x3F800000); with a list (+0xA4 != 0): ((1 − sameSex / count) + 1) × 0.5;
- 0x404C78..0x404CA7: (GetDistanceModifier(d, 500) + 1) × 0.5 × sex × room, with d = fn_00605CD0(abode +0x14, villager
  +0x14) (0x404C7F..0x404C86); 500 is 0x43FA0000.

**Desires to gain people.**

- `Abode::CalculateDesireToGainMale` 0x4074A0 (MaxVillagers 0 or no town → 0, 0x4074A6..0x4074C4), with the town's
  TownStats +0x54 / +0x58 (0x4074BB..0x407531): (males + 0.001) / (females + 0.001) − (adultMales + 0.001) / ((adults −
  adultMales) + 0.001). The town part 0x4074C6..0x40750F: fild qword (u32) + 0.001, the women the same, fdivp; the abode
  part 0x4074F6..0x407531: (u8) +0xB5 + 0.001 over ((u8) +0xB4 − +0xB5) + 0.001 (a signed int sub), then fsubp.
- `Abode::CalculateDesireToGainVillager` 0x407540 (MaxVillagers 0 or no town → 0, 0x407546..0x407564), with the town's
  +0x618 / +0x644 (0x407566..0x40759E): (townAdults + 0.001) / (townAdultPlaces + 0.001) − percentAdults. 0x407566..0x407594:
  (fild qword +0x618 + 0.001) / (fild +0x644 + 0.001), stored (fstp); 0x407598..0x40759E: − GetPercentAbodeFullWithAdults
  (fsubr). The 0.001 is [0x8AA3B0].

**Moving in and out.**

- `AddVillagerToAbode`:
  - 0x404079..0x404154: out of the town's homeless list (+0x768, --+0x76C); 0x4040DE..0x4040F8: out of its old abode;
    0x4040FA..0x404151: out of the global vagrants (g_game +0x205BFC / +0x205C00) if it is there;
  - 0x404181..0x4041B6: the abode's town: AddVillagerToTown when it is not the villager's town, then VillagerMoveIntoAbode
    (the places left, TownStats +0x4C / +0x50 / +0x30);
  - 0x4041BB..0x404210: a child ++ChildCount; an adult: MaleFemale[sex] when empty, ++AdultCount, AdultMaleCount +=
    IsMaleVillager.
- The unlink every writer inlines (AddVillagerToAbode 0x404085, CheckHomelessMoveIntoAbode 0x76137D, RemoveVillager
  0x73E259): out of the list if it is there, -- +0x76C; returns whether it was there.
- `RemoveAliveVillagerFromAbode`: 0x404346..0x40435A: inside → SetTopState(163): ExitAtHome 0x761B40 does the
  LeaveHome (row 163: +0xC0 = 0); 0x404360..0x4043B6 the counts (0x40436F..0x4043B6, never below 0), 0x4043BC..0x404406
  the list, 0x404406 SetAbode(0); 0x40440F..0x40442A with a town, `TownStats::VillagerMoveOutOfAbode` 0x7494C0 (the
  places left).
- `RemoveDeletedVillagerFromAbode`: 0x404227..0x40424D: MaleFemale[sex] == the villager → MaleFemale[sex == 0] and
  MaleFemale[sex] both 0; 0x404254..0x4042A8 the counts (0x404263..0x4042A8, never below 0), 0x4042AE..0x4042F4 the list,
  0x4042F4 SetAbode(0); 0x4042FC..0x404326 with a town, `Town::RemoveVillager` 0x73E210 and VillagerMoveOutOfAbode.
- `Abode::RemoveAllVillagersFromAbode` 0x404560 (the abode is destroyed; 0x404560..0x40457D): from the head, the next
  (+0xE4) read before `Villager::HomeDeleted` 0x7611F0 (which takes it out of the list).
- `Abode::ChildToAdult` 0x404CC0 (0x404CC3..0x404CFF): --ChildCount when not 0, ++AdultCount, AdultMaleCount +=
  IsMaleVillager; 0x404D05..0x404D1A with a town, `Town::ChildToAdult` 0x73AF50.

**Swapping between houses** (the shuffle's moves).

- `Abode::SwapMaleForFemaleFrom(y)` 0x407620 on x: 0x407628..0x40765F the first of y's list that IsMaleVillager and is
  not inside (+0xE0 & 4); 0x407663..0x407696 the first of x's list that IsFemaleVillager and is not inside;
  0x407696..0x4076A5 ForceMoveVillagerToAbode(man → x), then (woman → y); 1. No such pair: 0.
- `Abode::TakeVillagerFrom(y, male)` 0x4075B0 on x (0x4075B6..0x407600): y's list from the head, male ? IsMaleVillager
  : IsFemaleVillager, and not inside → ForceMoveVillagerToAbode(→ x) (0x407603); 1. None: 0.

<a id="home-food-sleep-homeless-and-age-v4"></a>
#### Positions around an abode

- `Abode::GetArrivePos` 0x401770 = `MultiMapFixed::GetDoorPos` 0x52E370 (vt +0x864): Game3DObject::GetDoorPosition 0x63AFE0
  (0x52E396: 1 with a door point; the world door point × 6553.6, ftol; y 0); when there is none, or its x or
  its z is 0 (0x52E3A4 / 0x52E3AC), the abode's position (+0x14). GetDoorPos serves any MultiMapFixed:
  `Child::ChildGotoCreche`'s creche door, the MultiMapFixed branch of ArrivePosOf, the site's
  GetResourcePosAndYAngle.
- `Abode::GetPosOutside(p1, p2, p3)` 0x4072E0: door (0x4072EF) + GetPosFromAngle(Get3DAngleFromXZ(pos, door) (0x40734F)
  + GameFloatRand(2 pi / p1) − pi / p1, GameFloatRand(p3) + p2) (0x407387..0x407394). The two draws: 0x407317..0x40734B
  GameFloatRand(2 pi / p1) (Abode.cpp 0x94A) − 2 pi / (p1 + p1); 0x40736B GameFloatRand(p3) (0x94B) + p2.

#### Moving villagers between houses

The step-24 shuffle (the rules are on
[Moving villagers between houses](villager-life.md#moving-villagers-between-houses-shuffle-step-24)):

- `ShuffleVillagersAroundAbodes` 0x741540: n <= 0 → nothing (0x74158B). 0x741549..0x741629: the structures +0x754
  (newest first) that are functional with MaxVillagers + MaxChildren != 0; 0x741604..0x74161E: {abode,
  CalculateDesireToGainMale, 0.5 [0x8AA3B4] × CalculateDesireToGainVillager}; 0x741638 _qsort(list, n, 12, 0x7417C0)
  (VC6 _qsort 0x7C7E64); 0x74163D..0x741798: i = 0 .. n − 2, one move a call (the first that returns 1 ends it,
  0x74177C jne 0x74179E). It reads fn_00438B20 at 0x741945.
- The comparator 0x7417C0: |b.m² + b.v²| < |a.m² + a.v²| (fcompp; `test ah, 1`) → −1, else 1 (never 0: the larger
  first).
- The pair loop 0x74163D..0x74179E on the sorted list:
  - 0x74165B..0x741670: best = |a.m² + a.v²|;
  - 0x741685..0x7416B9: the c after a with |(c.m + a.m)² + (c.v + a.v)²| strictly below the best (fcom; `test ah,
    1`);
  - 0x7416C3..0x7416D9: masV = a.v > b.v (fcomp; `test ah, 0x41`);
  - 0x7416DB..0x741723: a.v × b.v < 0 (opposite signs): the winner (masV ? a : b) not below 100% full of adults (vt
    +0x89C < 1, `test ah, 1`) keeps the swap; below it, no swap;
  - 0x741725..0x74173A: masM = a.m > b.m (`test ah, 0x41`);
  - 0x74173E..0x74175A: masM ? a.SwapMaleForFemaleFrom(b) : b.SwapMaleForFemaleFrom(a);
  - 0x74175C..0x741777: masV ? a.TakeVillagerFrom(b, masM) : b.TakeVillagerFrom(a, !masM).

#### Villagers joining and leaving the town

- `Town::AddVillagerToTown` 0x73A090: uninhabitable (+0x5F4) → 0; TownStats::Add (0x73A0A7..0x73A0B6); SetTown; an
  abode of this town → done; another one → RemoveAliveVillagerFromAbode, SetAbode(0); FindAbodeWithSpaceInTown(v, 0) →
  AddVillagerToAbode, 1 (0x73A0E4..0x73A101, no worship site check); none → MakeHomelessNoStateChange; then adults +
  children == 1 → CheckAddWorshipSite 0x740BF0. Returns 1.
- `Town::RemoveVillager` 0x73E210: 0x73E231 `TownStats::Remove` 0x7493C0; 0x73E238..0x73E24C an abode →
  RemoveAliveVillagerFromAbode, SetAbode(0); else out of the homeless list. 0x73E2A7..0x73E2B2: flags (+0xE0) & 2 →
  `Villager::RemoveVillagerFromWorshipSite` 0x76C440, which needs the town still set (fn_0073E3F0 through GetTown). It
  takes the villager off the town's worshipper count (fn_0073E3F0) when it is at the site, off the site's own count
  (0x77D0A0) and out of its dance (vt 0x978 / 0xB08), and clears +0xE0 & 2; it sets no state. The worship states call
  it too ([Worship](../magic/worship.md#the-worship-percentage-and-the-villagers)). Then the empty countdown +0xF20 =
  50 (0x73E2CD).
- The town picks its worshippers (`Town::AdjustWorshipersWorshipping` 0x73C0F0) among those for which
  `Villager::IsAvailableForWorshipSite` 0x752820 holds: IsVillagerAvailable (the state table's availability bit,
  field0xa8 & 1), not flagged 0x200 on the first pass, and not `Villager::IsAtOrOnTheWayToWorshipSite` 0x752860
  (flagged at the site, +0xE0 & 2, or in state 59 / 46).
- The homeless: `Town::IsVillagerInHomelessList` 0x73B580, the walk from +0x768 by +0xE4. MakeHomelessNoStateChange
  0x761290 puts the villager at the head of +0x768, ++ +0x76C (0x7612F9..0x761312).
- The vagrants: `Villager::IsVagrant` 0x7531A0. Villager::ReleaseFromScript 0x7532F8 puts it at the head of g_game +0x205BFC,
  ++ +0x205C00, unless it is in the list already (0x7532F8..0x75331A / 0x7532A2..0x7532CA); true when added. The unlink
  is in AddVillagerToAbode 0x4040FA / MakeHomelessNoStateChange 0x761290 / DeleteDependancys 0x74FE4B. Inferred: the
  GGame ctor 0x54B3A0 zeroes the list.
- UseFood: 0x73B5EA..0x73B600 fild qword (u32), fadd +0x6F8, fstp; 0x73B606..0x73B60F GetPlayer (vt +0x1C) +0xA44 →
  +0xA4 += n.

#### Town lists and queries

**The global town list** g_game +0x205C84: the Town ctor inserts at the head (0x739637 `lea esi, [edx + 0x205C84]`;
0x73964D..0x739656 node->next = head, head = node, count++), so the newest town first. The town's id is the script's
CREATE_TOWN argument, not that order (maps create towns out of id order, and one repeats id 0).

**Finding towns.**

- `GGame::FindTownWithID` 0x552FA0: the players in the order of GetNextPlayerAndNeutral 0x550980 (slots 0..7), each
  one's towns from +0xA50; the first with +0x5B4 == id. +0x5B4 is the script's CREATE_TOWN number, or 0xABA52 for every
  town a scaffold founds (`Scaffold::BuildBuilding`, the 6th argument of the Town ctor 0x739350), so several towns can
  share it.
- CREATE_ABODE / CREATE_TOWN_CENTRE with no town (cases 8 and 9: 0x7156A6, 0x7157FF) take the nearest town with
  fn_00552FF0: the first of the global list always, then distance < best, only with x / z.
- `Town::GetStoragePit` 0x73B5B0: +0x30 if IsAvailable (vt +0x2C) (0x73B5BC).
- `Object::GetTown` (vt +0x48) by class: Villager 0x751F00 (+0x12C), Field 0x528960 (+0x118), FishFarm 0x52C450
  (+0x8C), Abode 0x401730 (+0x98; storage pits, centres, workshops, creches, wonders, dispensers and graveyards have no
  override), Scaffold 0x55E120 (+0x8C), TotemStatue 0x738480 (that of its centre, +0x7C), PotStructure 0x66EF60 (the
  structure from IsPartOfStructure 0x66DA00 = +0x78 if available), TownArtifact 0x425D60 (+0x18); the rest 0: Object
  0x419950, MultiMapFixed 0x4220A0, GameThing 0x56FF10.
- What a town's HelpSprites read (GGuidance 0x71CA60 / 0x71CAF0 / 0x71CC40): adults + children (+0x618 + +0x61C),
  GetStoragePit and its IsFunctional (vt +0xD4), the position +0x14.

**The town's lists of its own.**

- The abodes +0x754 (next +0x9C), newest first: `Town::AddStructureToTown` inserts at the head (0x7399C3 `mov
  ecx,[esi+0x754]`; 0x7399C9 new->next(+0x9C) = head; 0x7399CF head = new). That order decides GetCongregationPos's y
  (the last one read, 0x7409C1..0x7409DB: the oldest), the base of its fallback when there is one abode, and which ones
  the ring of 100 keeps with more than 100 (0x74091F..0x740933).
- The building sites +0x790 / +0x794, the head first (Town::AddBuildingSite 0x73B910 inserts at the head).
- The homeless +0x768 / +0x76C, the head first (MakeHomelessNoStateChange 0x7612F9 inserts at the head, next = villager
  +0xE4).
- The workshops +0x74C / +0x750, the head first (`Town::AddWorkshop` 0x740180 pushes at the head when not there;
  RemoveWorkshop fn_7401D0; next = Workshop +0xD8; the code 0x740180..0x740380).
- The animals +0x984 / +0x988 whose +0xE0 is this town, the head first (SetTown fn_00417C50 inserts at the head;
  `Animal::DeleteDependancys` 0x417BA4..0x417BF6 unlinks). Literal: an animal that moved to another town stays listed
  here (SetTown does not unlink), so a reader must test each entry (valid, and its +0xE0 == this town if it matters).
  `Town::DeleteDependancys` 0x739762 does SetTown(animal, 0) for each.

**"Something changed" pulse** (Town +0x5E8 = 1, +0x5EC = 0), written inline by: `StoragePit::AddResource` 0x73316D,
Town::InsertBuildingSite 0x73B96D, the field 0x52964E, fn_0073E440 (a villager's death), Workshop MakeFunctional 0x7797CA /
RemoveScaffold 0x779D0E / AddResource 0x779E37, `Scaffold::BuildBuilding` 0x6E93AF. Town::Process step 21 reads and
clears it.

**The Town constructor.** `GBelief::Init` at 0x739523 while +0x5D8 is still 0 (the neutral belief is 0 until the first
fold); then +0x5D8 = GTownInfo +0xB8 beliefInNeutralPlayer (0x73966C) and +0x5DC = 1.0 (0x739672); Town::SetPlayer
fn_0073A8F0 (0x739597) puts it at the tail of the owner's list (fn_0064C090). +0x2C, `Town::GetPlayer`, is the player
given to CREATE_TOWN (the neutral player when none, 0x739545).

**Other fields of the town.**

| Field | Meaning |
|---|---|
| +0x30 | the storage pit: `Town::SetStoragePit` 0x73EA60 (from `StoragePit::MakeFunctional` 0x732F30; the last one wins) |
| +0x5E4 | "a plan was asked for this turn": 0 at 0x73961B; Town::Process clears it (0x747390); CheckSatisfyAbodes / Civic set it |
| +0x5E8 / +0x5EC | the building / resource pulse (above) |
| +0x5FC | the town has had a centre, a storage pit and a house (fn_404960 0x404960, set once) |
| +0x600 / +0x604 | the temporary pots, FOOD and WOOD (0x73EA11) |
| +0x728 / +0x72C, +0x734 / +0x738 | the town rectangle (see [the town area](#the-town-area-and-placement)) |
| +0x744 | the creche: `Creche::MakeFunctional` 0x50AB50 sets it when it is still null (the first one wins) |
| +0x748 | the graveyard (see [the graveyard](#the-graveyard)) |
| +0xEAC / +0xEB0 | the last aggressor and its turn (see [Town aggression](#town-aggression)) |
| +0xEC4 | the worship percentage (+0x5C0) saved during the emergency; 0 at creation |
| +0xF10 | the congregation point's cache (below) |
| +0xF1C | the turn the emergency started |
| +0xF20 | the empty town's countdown (set to 50 by `Town::RemoveVillager`) |

#### Workshops

The workshop is bw1-decomp's `Workshop.cpp` (0x779190..0x77A720): it turns 2500 wood into one scaffold every 200 town
turns, at most 3 waiting at its slots (the mesh's special points 6, 7, 8). A villager's drop-off adds the wood it
carries (0x759520).

**Its numbers.**

- GScaffoldInfo::GetInfo 0x779A50 → the global 0xD959D0; +0x6C WoodValue is read from info.dat (2500), the wood one
  scaffold costs.
- At most 3 waiting is a literal (GetSpaceInStore 0x779B05 `mov edx, 3`; info +0x1A8 MaxNumMobileObjectsToProduce is
  not read).
- The mesh's special points: 4 the wood pile (fn_779690 0x7796B1 `push 4`), 6 + slot the slots (fn_779A60), 10 the drop
  area centre (IsPosWithinScaffoldAreas 0x77A3D0 `push 0xA`); 9 is the ShowNeeds icon (GetShowNeedsPos 0x77A638).
- [0x8C6CA4] = 50.0: IsPosWithinScaffoldAreas 0x77A3B2, the distance from the workshop itself. 0x40000000 = 2.0:
  CheckSnapToPoint 0x77A488..0x77A4AB, the snap distance to a slot. 0x43FA0000 = 500.0: Town::GetBestWorkshop 0x74028E,
  GetDistanceModifier's maximum. [0x8BF518] = 0.0001: GetVisualWoodDesire 0x779BD3 / 0x779BDB, added to the wood and
  to the wood value.

**Life of a workshop.**

- `Workshop::Create` 0x779590: the ctor's fields (0x779350 / fn_779450: all 0), CallVirtualFunctionsForCreation
  0x7793E0's ShowNeedsVisuals, then CreatePileWood (0x7795EA; also for a workshop made from a plan: the pile exists
  during its construction). In the map cells, CallVirtualFunctionsForCreation 0x7795E2 (the workshop's InsertMapObject,
  MultiMapFixed 0x52E890) runs before CreatePileWood 0x7795EA, so the pile enters the cells after the workshop (the
  head of each cell's list).
- `Workshop::CreatePileWood` 0x779630: when +0xD0 is null, a "Magic Wood" pot at the special point 4
  (GetResourcePosAndYAngle) with 0 wood. fn_779690(out, type, index, float* angle) 0x779690: WOOD with a 3D object
  whose special point 4 exists → that point (and its Y angle); else the workshop's own position and angle 0.
- `Workshop::MakeFunctional` 0x7797B0 (vt +0x914): `Abode::MakeFunctional` 0x4047E0 first (0x7797B3), then
  0x7797BC..0x7797E8 with a town the pulse +0x5E8 = 1, +0x5EC = 0 and `Town::AddWorkshop`(this) 0x740180 (when not
  already in the list, at the head: +0xD8 = head, +0x74C = this, +0x750++).
- `Workshop::DeleteDependancys` 0x7794F0 (vt +0x910): with a town `Town::RemoveWorkshop` (0x7794F6..0x779509); then
  every owned scaffold, head first, loses its owner (+0x58 = 0 when available) and all its nodes go (no +0xC8 / slot
  update, literal). Then `Abode::DeleteDependancys` 0x403F00. `Town::RemoveWorkshop` fn_7401D0 0x7401D0 (callers
  DeleteDependancys 0x779509, RemoveStructureFromTown 0x739B12): out of the list (+0x750--, +0xD8 = 0) only when
  found.
- `Workshop::RemovePotFromStructure` 0x779750 (0x77978E `MultiMapFixed::RemovePotFromStructure` 0x52F160): the pile →
  DoResourceRemoving(its type, its amount, 0, 0) (the mirror) and +0xD0 = 0.

**Its turn.** Workshop::Process 0x7797F0:

- 0x77981E..0x779845: GetSpaceInStore != 0 and GetResource(WOOD) (vt +0x98, the mirror) >= WoodValue (jb, unsigned) →
  fn_7798A0 0x7798A0 (StartScaffold): RemoveResource(WOOD, WoodValue, 0, 0) (vt +0xA0, Workshop 0x779EC0: from the
  pile, then the mirror); +0xC4 = ftol(info +0x1AC TimeEachMobileObjectTakesToProduce, 200.0); SoundTag::Create(this,
  0x4A, 0, 2, −1, 0, 1, 1, 0) 0x71E840 (the working loop: track 0, mode 2, loops −1, is3D 1, InGame bank, no delay).
- When the countdown ends, fn_779910 0x779910 (FinishScaffold): slot = the first free one (fn_779610); the slot's point
  (fn_779A60); sc = Scaffold::Create(pos, GScaffoldInfo, GetTown, this, the point's Y angle, 1.0) fn_6E8460; sc →
  SetScaffoldValue(1) 0x6E9D40, AddScaffold (fn_779C10), its slot bits (0x779981..0x7799A5), slot byte 2 and
  G_ScaffoldReady at the workshop (GAudio::PlaySoundEffect 0x429E30: owner this, is3D 1, track 0, at GetPos's LHPoint).
  Always +0xC4 = 0 and SoundTag::Remove(this, 0x4A, 1) 0x71EBE0. When the point is missing, fn_779910's local angle is
  not initialised (0x779922..0x779939).
- 0x77987B..0x77988C: (+0xD4)->fn_719E80(GetVisualWoodDesire) (ShowNeedsVisuals +0x5C; no null test, literal).

**Space and desire.**

- `Workshop::GetSpaceInStore` 0x779AF0 (0x779AF3..0x779B0F) = 3 − (+0xC4 != 0) − fn_779600 (= +0xC8), signed.
- `Workshop::GetVisualWoodDesire` 0x779B90 = 1 − min((wood + 0.0001) / (WoodValue + 0.0001), 1), in float; the wood
  value is read exactly (fild qword, 0x779BB6..0x779BCA).
- `Workshop::GetDesireToBeSupplied` 0x779B60: GetSpaceInStore != 0 → 1.0 (0x779B63..0x779B6A); else GetVisualWoodDesire
  against 0, `test ah, 0x41; je` → 1.0 only when above 0, else 0.0 (0x779B6E..0x779B7E).
- fn_779B20 (WoodWanted; caller `GInterface::CalculateOverObject` 0x5CA1E8, the hand-over tooltip): space =
  GetSpaceInStore, <= 0 (jle) → 0 (0x779B24..0x779B2D); else w = WoodValue × space − GetResource(WOOD) (imul, sub: a
  wrapping uint32); (0 < w) ? w : 0 (cmp edx, ecx; sbb; and), which is w itself: a wood above the space's value returns
  the wrapped value (literal) (0x779B32..0x779B55).
- IsResourceStore 0x77A650: `MultiMapFixed::IsResourceStore` 0x52F1F0 (WOOD and +0x74) || WOOD || ANY (−2).
- IsWorkshop vt +0x310: Workshop 0x7792C0 = 1 (GameThingWithPos 0 for anything else).

**Wood in and out.**

- `Workshop::AddResource(type, n, status, b, pos, k)` 0x779E00 (vt +0x9C): n == 0 → 0. The pulse when the workshop had
  none of the type. WOOD: no pile → CreatePileWood; an available pile → its JustAddResource (vt +0x8C). Then
  DoResourceAdding(type, added, 0 (no status: no desire / alignment / belief part), b, pos, k) (the mirror +0xBC).
  Returns what was added (0 for any other type: only the mirror's DoResourceAdding with 0). Callers: an AddResource on
  a workshop or its pile (PotStructure::AddResource 0x66EDE6), the WorkshopBuildingSite 0x43DB20, the villager's
  drop-off.
- PotStructure::RemoveResource 0x66EE10 on the workshop's pile (a pile part of a structure that is not a storage pit):
  over = CalulateAmountOverMaximum (vt +0x8EC: MultiMapFixed 0x4015D0 = 0 for a workshop); r = the pile's
  JustRemoveResource(n) (it stays when emptied: part of a structure); r != 0 → the workshop's DoResourceRemoving(type,
  r, status); r < n → r += the workshop's RemoveResource(n − r).

**Choosing a workshop.** `Town::GetBestWorkshop(pos, useDesire, onlyFunctional)` 0x740250: best = 0; each workshop head
first: v = GetDistanceModifier(GetDistanceInMetres(pos, its pos), 500) × (useDesire ? GetDesireToBeSupplied 0x779B60 :
1.0) (0x740271..0x74028E); strictly above best (`test ah, 0x41 jne`) and (!onlyFunctional || IsFunctional) → kept.
Null when none. Callers CheckSatisfySupplyWorkshop 0x7593BE, ArrivesAtStoragePitForWorkshopMaterials 0x7594A0 (state
211; a wood capacity of 0 goes to 163) and ArrivesAtWorkshopForDropOff 0x759542 (state 210: the villager asks again on
arrival), all (pos, 1, 1); without a town 210 and 211 return 0 before asking.

**Slots and scaffolds.**

- The slot bytes +0xDC + i, i in 0..2: 0 free, 1 taken away (the place stays reserved), 2 occupied. The byte +0xDC + 3
  is the class's padding +0xDF: a slot value 3 from the scaffold's 2 bits writes there (0x779CEB / 0x77A5FD /
  0x77A622). `Scaffold::CanBecomeAPhysicsObject` 0x6EA910 reads them (`cmp byte [ecx + eax + 0xDC], 2`).
- fn_779A60(out, slot, &a, &y, &c) 0x779A60: the special point 6 + slot (0 → 6, 1 → 7, 2 → 8, any other: the argument
  itself, literal; 0x779A63..0x779A88) through `Game3DObject::GetSpecialPos` 0x63B1A0, with tmp zeroed first
  (0x779A8F..0x779A93: no point leaves (0, 0)); out = (tmp.x, tmp.z, 0), the altitude 0, on the land (0x779ACC).
  `yAngle` gets the point's Y angle (null allowed). fn_77A530 maps the slot the same way (0x77A548..0x77A566) but takes
  8 for 2 and 3.
- The special points: Game3DObject::GetSpecialPos 0x63B060 (the position), 0x63B0B0 (and the Y angle) and 0x63B1A0 (and three
  angles, GetYXZ 0x7FAB30), the metric through the Transform (0x63B040). 0x63B1A0's angles are GetYXZ's (y, x, z) in
  its 3rd, 4th and 5th arguments (0x63B1C5..0x63B1ED).
- fn_779610 0x779610 (GetFirstFreeSlot): the first i < 3 with byte 0; none → 0 (literal: slot 0 even when it is taken).
- `Workshop::IsCloseToEP(n, p, r)` 0x77A420: GetDistanceInMetres(p, special point n) < r (strict; no point: (0, 0,
  0)).
- `Workshop::IsPosWithinScaffoldAreas(p)` 0x77A3A0: dist(own pos, p) <= 50 (`test ah, 0x41 jne`: also NaN) and (the
  drop area IsCloseToEP(10, p, 10) | IsCloseToEP(6 / 7 / 8, p, 5)), all four asked and ORed (0x77A3C9..0x77A40C).
- `Workshop::CheckSnapToPoint(s)` 0x77A480: 0x77A484..0x77A4BE s within 2 m of the special point 6, 7 or 8
  (IsCloseToEP(6 / 7 / 8, s +0x14, 2.0), all three asked), none → 0; 0x77A4C0..0x77A503 another owner's (s +0x58 !=
  this) → 0 when GetSpaceInStore == 0, else s->RemoveFromWorkshop 0x6E9200, AddScaffold(s), s +0x58 = this, s slot
  bits = fn_779610 & 3; then the snap fn_77A530 (moved to its slot's special point, slot byte 2). True when snapped.
- fn_779C60 0x779C60 (RemoveScaffold; only caller `Scaffold::RemoveFromWorkshop` 0x6E923E): s not in the list →
  nothing (0x779C60..0x779C83); every node of s out; +0xC8--; slot[s bits 6-7] = 0; +0xC8 == 2 now and a town → the
  pulse.
- fn_779DD0: s is in the +0xE0 list (`Scaffold::RemoveFromWorkshop` 0x6E9200 asks it).
- fn_779D30(i) 0x779D30 (caller 0x6EA836): i < 3 → slot[i] = 0.
- `Workshop::ScaffoldMoved(s)` 0x77A610 (vt +0x5E0; `Scaffold::InterfaceSetInMagicHand` 0x6E975A, InitialisePhysics
  0x6EA87D): slot[s bits 6-7] = 1 (taken away: the place stays reserved).

**The town's workshop queries.**

- `Town::IsScaffoldAwayFromWorkshops(s)` 0x740300 (0x740300..0x740337): the first workshop with
  IsPosWithinScaffoldAreas(s +0x14) → 0; none → 1. Callers `Scaffold::ProcessInHand` 0x6E874C, ChoosePlan 0x6E8D34,
  TryToBuildPlannedBuilding 0x6E909B.
- `Town::CheckScaffoldSnapToPoint(s)` 0x740340 (0x740340..0x740373): r |= CheckSnapToPoint(s) for every workshop, head
  first (none skipped). Caller `Scaffold::EndPhysics` 0x6E87D1.

#### The graveyard

- `Graveyard::MakeFunctional` 0x595E00: `Abode::MakeFunctional` 0x4047E0 first (0x595E03), then with a town whose +0x748
  is null SetGraveyard(this) (0x595E0C..0x595E33), then AddDead (0x595E3A, literal: one dead counted).
- fn_0073D690 (`Town::SetGraveyard`): +0x748 = g when +0x748 is null or g is null (0x73D690..0x73D6A2): the first
  functional graveyard stays, a null clears it; otherwise only a null argument is written (0x73D69C..0x73D6A2).
- fn_00595E50 (`Graveyard::AddDead`): with a town and IsFunctional (vt +0xD4) (0x595E58..0x595E6B), and (float)+0xC4 <
  50 (fild qword, unsigned; fcomp [0x8C6CA4], 0x595E6D..0x595E8E, 0x595E7F..0x595E8E): +0xC4++, stage = ftol(+0xC4 ×
  0.18) ([0x9003E0]), 0 with someone buried → 1 (0x595E91..0x595EBD, 0x595EB5..0x595EBD); LH3DObject vt +0x1D8(stage)
  = fn_007F9C40: +4 bits 21..23 = stage & 7 (0x595EBE..0x595EC5: stage 8 shows as 0). Callers
  `Graveyard::MakeFunctional` 0x595E3A and the town's death book-keeping fn_0073E440 0x73E48B.
- `Graveyard::DeleteDependancys` 0x595CE0 (from ToBeDeleted 0x595CB0, vt +0x910): with a town whose +0x748 is this one,
  the town's abode list (+0x754, next +0x9C) is searched for the first other functional abode whose ABODE_TYPE (info
  +0x120) has bit 2 or bit 9 (`test ..., 0x204`, 0x595D17: literal, so any civic building such as the storage pit
  passes, not only a graveyard), IsFunctional (vt +0xD4), not this one (0x595D14..0x595D3D). Then (0x595D3F..0x595D59)
  SetGraveyard(0) and SetGraveyard(esi): esi is the one found, 0 when the town's graveyard was another one (esi = 0
  from 0x595CF1) or nothing was found. Then `Abode::DeleteDependancys` 0x403F00.

#### The town area and placement

**The town rectangle** +0x728 / +0x72C and +0x734 / +0x738: MapCoords x / z (min, max); the cells are the high words
+0x72A / +0x72E / +0x736 / +0x73A. Min 0x7FFFFFFF, max 0 = empty.

- `Town::SetTownArea` 0x73AAF0: min = 0x7FFFFFFF, max = 0 (0x73AB00..0x73AB1B), then fn_73AC90 for every abode (+0x754,
  next +0x9C, 0x73AB32) and every field (+0x780, 0x73AB51). Then 0x73AB5C..0x73AC7F: a camera point at +0x598, from
  +0x740's position (else the storage pit's, else fn_0073AE10's), at a distance from the rectangle's size
  (`GCamera::SetPointFromPointDistanceHeadingAndPitch` 0x442810). Called from AddStructureToTown 0x739A0A and
  RemoveStructureFromTown 0x739BB6 (the abode already out of the town).
- AddStructureToTown 0x739A0A runs in the Abode ctor (0x4013BB), before the abode's 3D object exists (+0x40 = 0 from
  the Object ctor 0x63657B): Get2DRadius Object 0x638180 → 0 (0x6381E9), so the newcomer is a point.
- fn_73AC90 0x73AC90 (ExtendTownAreaBy; also alone from the Field ctor 0x527E82): the object's GetRadius (vt +0x60:
  Object 0x638110 → Get2DRadius vt +0x64) around its position, in metres, widens the rectangle. 0x73AC95: the object's
  +0x14 x and z as they are (fild, × 10, × 2^-16): no ground height is asked. 0x73AC95..0x73AD3D on x: obj.x × 10 × 2^-16
  (fild; fmul 10; fmul 2^-16: the second product is exact, so there is one rounding) − r; below the minimum in metres
  (`test ah, 1`) → min.x = ftol(v × 65536 / 10). 0x73AD40..0x73ADFA: the same on z with +0x72C / +0x738.
- fn_0073AE10 0x73AE10 (the rectangle's centre): x = ftol((maxX × 10 / 65536 + minX × 10 / 65536) × 0.5 × 65536 / 10), z
  the same with +0x738 / +0x72C, altitude 0. In detail: 0x73AE10..0x73AE3B fild +0x738 (maxZ), fild +0x72C (minZ), each ×
  10 [0x999AA0] × 2^-16 [0x8AC41C]; faddp; × 0.5 [0x8AA3B4]; 0x73AE41..0x73AE7B the same with +0x734 (maxX) and +0x728
  (minX); 0x73AE81..0x73AEA8 × 65536 [0x8AC408] / 10 [0x999AA0], __ftol; x into +0, z into +4. Read by
  `Town::MakeScenicForest` 0x741B8C.
- Nothing but `Town::Save` / Load (0x73ED70 / 0x73F490) reads the rectangle otherwise.

**May a building stand here?**

- `GAbodeInfo::IsOkToCreateAtPos(pos, angle, scale, town)` 0x404B10: IsSuitableForFixedAbodeInTown(pos, the info's mesh
  (vt +0x2C), town, angle, scale) != 0 (`neg; sbb; neg` 0x404B2E..0x404B32).
- IsSuitableForFixedAbodeInTown: only with a town (0x603865); 0x60387B GetNearestTown(&near, &dist, excluded = town,
  tribe = −1): the nearest OTHER town by the octagonal cell distance and its rectangle's code; 0x603880..0x60388C: code
  1 ("inside the nearest other town's rectangle") → 0xE, which 0x404B10 turns into "accepted" (literal: no fixed
  tests); 0x6038A0.
- `MapCoords::IsSuitableForFixed(pos, mesh, angle, scale)` 0x6038B0 → fn_604020: the static Game3DObject g_tmp
  (0xD38330) with the mesh, angle and scale at the position (0x6038B0..0x603AF6); the mesh's NewCollideDescriptor cells
  (the shape NewCollideDescriptor 0x46AAD0 takes; reach scale × half diagonal + 1). For each: the position's own cell on
  the map and not water, and no MultiMapFixed of the cell's fixed list with d < R_obj + scale × max(hx, hz) (strict).
  No mesh: g_tmp has no 3D object and 0x603B10 returns 0.
  - 0x604037..0x604087: R = scale × max(mesh +0x24, +0x2C), the inline Get2DRadius.
  - 0x60412E..0x604170: the cell's fixed list (+4, GetMapChild vt +0x53C), unfiltered; (+0x24 & 2) &&
    IsSolidToNewAbode (vt +0x7D8) is exactly the MultiMapFixed class.
  - GetNext 0x46AD80 returns NULL at a marked cell off the map and the loop then ends with 1 (0x46ADEC..0x46AE0A);
    fn_601E00 can never fail (a dead test).
- `MapCoords::IsSuitableForFixed(Game3DObject*, angle, scale, under)` 0x603DC0: the same, with its own SetPosition of
  the object (0x603DD0..0x603E0B, the ground + altitude) and R the same scale × max(+0x24, +0x2C) (0x603E1E..0x603E61);
  the position's cell must not be on the coast line (lc +6 & 0x20, `MapCoords::IsCoastal` 0x6036A0;
  0x603F16..0x603F75), and `under` (the building under the touching scaffold) is not an obstacle (0x603F96). fn_604020
  (refuseCoast false, no `under`) and 0x603DC0 (refuseCoast true, `under` skipped) share this loop. Its callers: the
  scaffolds (fn_006E8D20 0x6E8E37, ProcessInHand 0x6E873C, TryToBuildPlannedBuilding 0x6E9118) and
  Town::GetNewPlannedBuilding 0x73DA34.
- The position's own cell test (fn_604020 0x6040A9..0x604125 / 0x603DC0 0x603E8C..0x603F10): the POSITION's landscape
  cell in 0..0x1FF, a block, not water (lc +6 & 0x10).

#### Town features

- `Town::AsssignTownFeature` 0x73EAC0 (static): MakeScenicForest 0x741B40 for every town of g_game +0x205C84 (newest
  first) at its town rectangle's centre (fn_0073AE10), then AssignForestsToTown 0x73EB00 for every town in the same
  order: two separate passes (0x73EAC6..0x73EADC and 0x73EAE4..0x73EAFA). Called at the end of
  `GSetup::LoadMapFeatures` 0x71813D (after the map's script).
- `Town::AssignForestsToTown` 0x73EB00's reference point (0x73EB06..0x73EB4D): the storage pit's position (+0x14) when
  GetStoragePit 0x73B5B0 gives one (0x73EB17..0x73EB3A; the position, not its arrive point); else
  (0x73EB40..0x73EB4D) GetTemporaryResourceStorePotOrPos(town +0x14, &ref, WOOD 1), the pot's nearest edge to the
  town's position, which makes the town's MagicWood pot when it has none; metres, altitude 0.

#### Congregation point and clear areas

**The cache** +0xF10: `Town::GetCongregationPos` 0x7408B0's result, MapCoords x / z (6553.6 per metre) and y; (0, 0,
0) = not computed yet. Zeroed by the constructor (0x739501, fn_0073C710 0x73C81D); written by GetCongregationPos and
by SET_TOWN_CONGREGATION_POS (MapCommandProcess case 6, 0x7155A3..0x7155B9).

- The cache is valid unless x == 0 && z == 0 && y == 0.0 (0x7408B6..0x7408EF).
- It uses a ring of 100 elements: the abodes that are not fields and, with fewer than 3, the plans
  (0x7408F5..0x740983); 32-bit x / z sums read as unsigned qword (0x74099C..0x7409E3).
- 0x740A25..0x740A5D: FindClearArea(pos, pos, 130, 3, 10, BlocksTownClearArea (0x743690), none).
- With a single unread element in the ring that one is used, otherwise the town's position (+0x14), copying its three
  dwords (0x740A71..0x740ABC).
- In the fallback the distance is drawn before the angle: d = GameFloatRand(10) + 10, then angle = GameFloatRand(2π)
  (Town.cpp 0x11EC) (0x740AA6..0x740AFD).
- The meeting point's mean is on [Deciding what to do and leisure](villager-decisions.md#deciding-what-to-do-and-leisure).

**A new building near it.** `Town::CheckWhenNewBuildingCreated(b)` 0x741500 (from PostCreatePlanned 0x648C50):
GetDistanceInMetres(b, +0xF10) − Get2DRadius(b) < 7.5 ([0x8C7798]) → the cache goes back to (0, 0, 0).

**Clear areas.**

- BlocksTownClearArea (Object vt +0x534, thunk 0x743690): Object 1 (houses, Fixed, Feature, Field, stores, centre…);
  Mobile 0 (villagers, animals, piles, pots), MobileStatic 0 (fallen tree, rock, bonfire…), Tree 0.
- `Town::CheckForClearArea` 0x7413D0: walks max(ftol(0.2 r), 1)² cells in a `GUtils::Spiral` 0x74D7E0 from pos's cell
  (only those on the map, `MapCoords::InBounds` 0x6042C0); not clear if an object, except the excluded one and if it
  passes the filter, has distance − Get2DRadius < r (0x74144A..0x741475).
- `Town::FindClearArea` 0x7412F0: GetIncrementSpiralSizeFromRadius(a, b) points (0x741319) from `start`
  (SpiralIncrement, steps of b metres, dir = count = 1, 0x741320..0x74132E; 0x741363); the first one with
  CheckForClearArea(p, r) goes to `result` (0x7413A8 result.Set(pos), 0x741383..0x741384) and it returns true. None:
  `result` is left as it was (0x741390 only resets the local to start) and false.

#### Deviations and visible effects

- At night Sleep is the first one (raw up to 6.25, desire 1) and CheckSatisfySleep sends to 36, then into the home
  (37 → 38 → 119 → 120, see [Home, food, sleep, homeless and age](villager-life.md#home-food-sleep-homeless-and-age)).
  By day the distribution cuts off at Relaxation/Playtime (CheckSatisfy 0) and the villager goes on with its own
  desires and leisure.
- The worship code receives, every 10 turns, the worshipper adjustment from `Town::Process` (fn_7489F0).
- What openblack does differently is listed under [openblack](#openblack).

### The town's belief

The town's per-turn belief fold, `GBelief::ProcessOncePerTurn` and DrawBelief.

#### The belief record

Town +0x798 is a GBelief (Belief.h, 0x1D0 bytes; GBelief : Base, its vtable +0x0 and Base +0x4). Every row is indexed
by the player number: 8 slots per player (GPlayer +0xB5), the neutral one = 7 (g_game +0x205A5B, GPlayer::IsNeutral 0x64AC08
; `GGame::SetupPlayers` 0x550458 and `GGame::Load` 0x554988 `mov byte [+0x205A5B], 7`, no other writer).

| Offset | Field | Rules |
|---|---|---|
| +0x8 [player] | BeliefInPlayer (GBelief::GetBeliefInPlayer 0x437E70 / 0x437E90) | SetBelief 0x4387D0 (capped), ReduceBelief 0x437FD0 (not clamped: it can go below 0); 0 in Init but the neutral slot (= +0x5D8) |
| +0x28 [player] | recent, a float (bw1-decomp says uint32) | += f (0x437ED5); only decays (× GPlayerInfo +0x48 0.997 a turn, fold 0x438644..0x43864C) and is read only by the computer player (fn_00438A40). Not reset by Init |
| +0x48 [player] | the turn of the last f != 0 (0x437EF5) | no reader |
| +0x68 [player] | BeliefInPlayerMax | 10.0 (Init 0x437DE1); SET_TOWN_BELIEF_CAP (SetBeliefInPlayerCap 0x438A00) |
| +0x88 [player] | addedThisPeriod | += the folded amount (fold 0x438424); 0 every 10 turns (ProcessOncePerTurn 0x438158). Not reset by Init |
| +0xA8 [player] | ReduceBelief's accumulator for its (never shown) draw | 0x437FFD, 0x438041; 0 in Init |
| +0xC8 [player] | pending: what was added since the last fold | += f (0x437EC7); the fold multiplies it by +0x5DC into +0x8 (SetBelief 0x4387D0) and +0x88, then 0 (0x4384A6) |
| +0xE8 [REACTION] | BoredomMultiplier (41 reactions) | 1.0 (Init 0x437E40); the fold adds ReactionInfo +0x4C below 1; `Villager::UpdateHowImpressed` 0x7635B9 calls AddToBoredomMultiplier 0x438790; read by GetBoredomMultiplier 0x56FE70 |
| +0x18C [TOWN_DESIRE] | the desire above which the owner loses belief (17) | the fold reads it at 0x438565; GTownDesireInfo +0x3C in Init and on every fold with something pending (fn_00437E50), then −= +0x48 a fold while above GBeliefInfo +0x1C (0.25) |

Town +0x5D8 BeliefInNeutralPlayer (fn_0073E4B0): GTownInfo +0xB8 (0.5) after Init in the Town ctor (0x73966C);
SET_TOWN_BELIEF of the neutral player (`Town::SetBeliefInPlayer` 0x73BA87). The fold pins belief[NEUTRAL] to it.
Town +0x5DC: the scale of the pending belief (1.0, Town ctor 0x739672); SET_TOWN_BALANCE_BELIEF_SCALE (case 98,
0x717BBA) and LHVM SET_OBJECT_BELIEF_SCALE (`GScript::SetObjectBeliefScale` 0x6FF8B2) set it.

The defaults are the zero-filled allocation (`Base::operator new` 0x4366F0 → fn_00436870, `rep stosd`) with Init's cap
and boredom.

#### Setting and reading the belief

- `GBelief::Init` 0x437DD0: belief, pending, the reduction accumulator and lastAddedTurn to 0, the cap 10.0
  (0x437DE1); belief[neutral] = Town +0x5D8 (fn_0073E4B0, no cap) (0x437DF6..0x437E2C); boredom[0..40] = 1.0
  (0x437E30..0x437E40, 0x437E44); the thresholds = GTownDesireInfo[d] +0x3C desireAffectsBeliefAfter (fn_00437E50:
  GTownDesireInfo 0xDA2930 + d × 0x90, so 0xDA296C .. 0xDA32FC step 0x90; 0 without the info). recent and
  addedThisPeriod are NOT reset. Called by the ctor (0x739523) and SetTownEmpty (0x74108E).
- `GBelief::SetBelief` 0x4387D0: belief[n] = min(cap[n], v), with no lower limit. GBelief::SetBeliefInPlayerCap 0x438A00
  / GetCap 0x438A20 (P): cap[P.number].
- Town::SetBeliefInPlayer 0x73BA70 (P, f) (SET_TOWN_BELIEF 0x71542B): the neutral player → Town +0x5D8 = f (0x73BA87);
  then SetBelief(P.number, f).
- `GBelief::AddToBelief` 0x437EB0 (P, f, thing, draw, GUIDANCE_ALIGNMENT) of the town's +0x798: +0xC8[n] += f
  (pending), +0x28[n] += f (recent), f != 0 → +0x48[n] = the turn (0x437EC0..0x437EFC); the player is indexed with
  `GPlayer::GetPlayerNumber` 0x64A790, unchecked (0x437EBB). With a thing (0x437F0A..0x437F2A): DrawBelief
  0x438800(f, thing, P) when draw (0x437F19), then `GGuidance::BeliefSFX` 0x437F40.
- GBelief::ReduceBelief 0x437FD0 (P, town, r): belief[n] −= r (0x437FE3..0x437FEB, no clamp: it can go below 0); with a
  town centre (+0x9A4) reduceAcc[n] += r and, above 0.005 (double 0x8C5890), DrawBelief(−reduceAcc[n], centre,
  GetPlayer(n)) (0x43803C) then reduceAcc[n] = 0 (0x437FEF..0x438041). That draw has a negative amount and so never
  shows anything (0x438817 jle).
- GBelief::AddToBoredomMultiplier 0x438790 (reaction, f): b = boredom[reaction +0x24] + f; < 0 → 0 (0x4387A2 fcom 0;
  C0 → 0). `GameThingWithPos::GetBoredomMultiplier` 0x56FE70: a town → boredom[reaction], none → 1.0.
- GBelief::GetMaxBeliefMeNotIncluded 0x4389B0 (n): the largest belief[i], i ≠ n, from 0, strict >.
- fn_00438910 (P, thing): if the thing's player is P, the belief of the slot i ≠ P with the largest belief[i] −
  belief[P] (from −9999, 0xC61C3C00, strict >); otherwise belief[the thing's player]. It returns belief[i] itself, not
  the difference (0x438970..0x438975 fstp of GetBeliefInPlayer(i), returned at 0x438983).
- fn_00438A40 (P) (the computer player, 0x66ABD8): over the slots 0..5 not P and not allied (0x438A40), the largest
  recent[i] above 0.008 (0x3C03126F, strict >); none or belief[P] == 0 → 0; else x = 2 belief[i] / belief[P] and
  min(x², 1) (0x438AC9..0x438AFD: fld st0; fld st0; fmulp).
- fn_00438B20 (P) (`Town::ShuffleVillagersAroundAbodes` 0x741945): the largest belief of the active non-allied players
  ≠ P (from 0, strict >) / belief[P]; 0 when belief[P] == 0. "Active" = GetNextActivePlayer 0x5508D0 (the slots 0..6
  with +0x8E0 != 0).
- GetAddedThisPeriod 0x438060 (and 0x437E80, no direct caller): addedThisPeriod[n]. GetAddedThisPeriodRatio 0x438070
  (no direct caller): added[n] == 0 ? 0 : added[n] / (belief[n] × 10).

#### The fold (step 19)

fn_004383D0, each turn for each town:

1. For each player, d = Town +0x5DC × pending. d == 0 or NaN → skipped and pending is NOT cleared; otherwise added +=
   d, with a centre a belief sprite of ftol(d × 10000) ([0x8C58A4], 0x438449) in the player's colour,
   ResetDesireThresholds (fn_00437E50 0x43847B, whoever the player), SetBelief(belief + d), pending = 0
   (0x4383FA..0x4384B4).
2. Boredom: x = LostTownScale (0xBF33F0) × ReactionInfo[k] +0x4C + boredom[k]; stored only if x < 1
   (0x4384BA..0x4384EE).
3. If the owner is not neutral, for each desire s = GetDesire (an x87 sum in the original): if s > threshold,
   ReduceBelief(owner, (s − threshold) × GTownDesireInfo[j] +0x38 desireToBeliefScale) and, if the threshold >
   GBeliefInfo +0x1C minimumThreshold (0.25), threshold −= +0x48 (it can go below once) (0x4384F0..0x4385BE).
4. belief[neutral] is fixed to +0x5D8 via SetBelief (0x4385CA..0x4385F7).
5. The strongest player: slots 0..7 in order, only < skips (a tie goes to the higher slot, owner included), and recent
   ×= decay (0x4385FC..0x43865F).
6. If the strongest is not the owner: SetBelief(new, GBeliefInfo +0x14 claimedTownBeliefMultiplier 1.5 ×
   belief[new]); if the old one is not neutral, each of its towns (this one included) SetBelief(P, +0x18
   lostATownBeliefInPlayerMultiplier 0.9 × belief[P] × LostTownScale); the old owner local → HelpSpritesGeneralBad,
   else the new one local → GeneralGood (0x438704..0x43875D); then `GPlayer::TakeOverTown` fn_00649810(newP, town)
   (0x438733 / 0x43875D) (0x438661..0x43875D).

LostTownScale [0xBF33F0]: SET_LOST_TOWN_SCALE (case 104 of the land script, 0x717E85); 1.0 in GLandBalance::Init 0x5E28A9
.

ftol(f × 10000) truncates toward 0 (the product of a float and 10000 is exact in a double).

#### Changing hands and emptying

- `GPlayer::TakeOverTown` fn_00649810 (newP, town): fn_0073A7D0 (0x6498EA) sets +0xF20 = 0 (0x73A7F7) and takes the
  town out of the old owner's list (fn_0064C0E0); SetPlayer fn_0073A8F0 (0x6498F2, +0x2C at 0x73A904) → fn_0064C090:
  the tail of the new owner's list. 0x6498F7..0x649910: the new player's GameStats (+0xA44) +0x68 += adults + children
  (Town +0x618 + +0x61C). The rest: the sound 0xCD (0x6498C9), SetWorshipPercentage(0) 0x6498DC, fn_0073A9C0, +0x98C /
  +0x5D0 = 0, the worship site and citadel unlinked, the magic types, Reaction 0x1F (0x649937), and the code after
  0x64995E.
- `Town::SetTownEmpty` 0x741080's belief part (Town::Process step 22, 0x747559): Init; SetBelief(i, +0x5D8) for the 8
  slots; an owner other than the neutral player → TakeOverTown(neutral, town).

#### Once every 10 turns: ProcessOncePerTurn

`GBelief::ProcessOncePerTurn` 0x4380B0 (static; row 27 of `GGame::ProcessTurn`, 0x54E6DF) runs only when g_game
+0x205A40 % 10 == 0 (unsigned div, 0x4380B6..0x4380CB, 0x4380C0 div 0xA):

- 0x4380D1..0x438100: the local player's number (+0xB5).
- 0x43810C..0x43816E (0x438114, 0x438128 GetAddedThisPeriod 0x438060, 0x438131..0x438150): it walks the towns from
  newest to oldest, sets addedThisPeriod to 0 (0x438158) and sums each player's belief (0x438163 fadd [ebx + edi]
  (+0x7A0 + 4p), fstp [esp + 0x14]).
- 0x438176..0x438187: after the towns (none: je 0x438176), GetPlayer(p) +0xA44 fn_0056A3B0(sum): the player's world
  belief maximum (+0x1070) / minimum (+0x1074) in its GameStats.
- 0x43819C..0x4381AB: if the local player's added belief > 0.001 ([0x8AA3B0]): v = sum × 1000 (0x4381B1, [0x8AB228]);
  `ToolTips::ForceToolTips`(0xEE1 «Conseguidos %3.0f Creyentes», "%3.0f believers gained", v) (0x4381C9, with a
  leftover ecx: ToolTips does not use `this`, 0x5C9C60).
- 0x4381D0..0x438281: a new ValueSpinner (0xA0, Belief.cpp line 0xAD) "%4.0f" of v at the point under the hand
  (MyInterface +0x3B8), colour bytes 00 FF FF FF, `ValueSpinner::Init` 0x833A00 (+0x18 = 5.0, +0x14 = [0xECCD08]). The
  value is the float v widened to double for `sprintf` "%4.0f" ([0x9CD27C]) (0x438207..0x43821B), then made wide by
  fn_008300A0. The point is the interface's hover collide (`GInterface` +0x3B8,
  [the hand's placement](../hand/animation-and-placement.md)): x = fild(+0x3B8) × 1/6553.6 ([0x8AA3A4]), z =
  fild(+0x3BC) × 1/6553.6, y = `LH3DIsland::GetAltitude` 0x803090 of it + its altitude +0x3C0 + 8, which the hover
  write leaves 0 (0x438229..0x438281). So it is the land under the cursor as map coordinates hold it, on the ground, or
  the map's corner (0, ground, 0) when the cursor found no land. The colour bytes 00 FF FF FF are 0xFFFFFF00 as a
  dword, yellow (0x43825D..0x438270). The floating number itself is on
  [The floating numbers](../world/buildings.md#the-floating-numbers-valuespinner).
- Then fn_00438340 for the local player (0x4382B9), inside the % 10 block: the "losing belief" help sprite; nothing on
  land 1 (g_game +0x205A08, 0x438345); 0x43837B..0x43838A; fn_0071CCC0(t) 0x4383C4. fn_0071CCC0 (town):
  HelpSpritesPlayNow(19 LosingBelief) && adults + children (+0x618 + +0x61C) → list 10.

#### Drawing the belief

- GBelief::DrawBelief 0x438800 (f, thing, P) (`ret 0xC`; 0x438800..0x43880E): v = ftol(f × 10000) (0x438806); v <= 0
  → nothing (0x438815 jle); the point is {x / 6553.6, GetAltitude(pos) + pos +8 + GetHeight (vt +0x42C), z / 6553.6},
  the thing's GetLHPoint + GetHeight (0x438827..0x438864); 0x43886C..0x43887B: g_game +0x14 & 0x4000 → the debug "B n"
  ValueSpinner (Belief.cpp line 0x15B, fn_00833A50), off in a normal game; colour = P's GetPlayerColour, else
  0xFFFFFFFF (0x4388DD..0x4388EA, 0x4388E5); fn_0069B550(thing, &pos, v, colour) (0x4388F5). Without a thing the point
  is uninitialised.
- The belief-sprite queue fn_0069B550 takes no entry when [0xC02A08] != 1 (a constant 1) or once it holds 400
  ([0xD4ED30] >= 400; 0x69B550..0x69B56B, 0x69B565). `UR_BeliefSprite::ModifyAtomCollection` 0x69B750
  (SF_BeliefSprite) takes the LAST entry (0x69B781, LIFO).

### The town's temporary pots

`Town::GetTemporaryResourceStorePotOrPos` 0x73E900: where villagers drop resources while the town has no working
storage pit.

**Getting the pot.**

- The pot of Town +0x600[type] (FOOD +0x600, WOOD +0x604) if it is not NULL and available (vt +0x2C, IsAvailable
  GameThing 0x401810) (0x73E90D..0x73E91F).
- Otherwise a new one (0x73E95F..0x73EA11):
  1. GetCongregationPos 0x7408B0, += GetPosFromAngle 0x74D580(0, WOOD ? 5 : 0) (operator+= 0x605410, 0x73E96E);
  2. moved by FindClearArea 0x7412F0(pos, pos, 45, 1.5, 2, 0x73EA50, NULL) away from MultiMapFixed objects
     (0x73E99E..0x73E9D4; the arguments at 0x73E9A6..0x73E9C2 are 45.0 (0x42340000), 1.5 (0x3FC00000), 2.0
     (0x40000000)); result and start are the same local; the filter 0x73EA50 is `GameThingWithPos::Flags` +0x24 bit 1,
     the MultiMapFixed bit; the bool it returns is not tested;
  3. `Pot::Create` 0x66CF10(pos, GPotInfo 0xD4C660 + (FOOD ? 10 : 9) × 0x144, 0, NULL, this, 0, 0, 1, 0)
     (0x73E9D9..0x73EA11): GPotInfo 10 MagicFood / 9 MagicWood and **amount 0**, the town as `this`. For those two infos
     `Pot::Create` calls the MagicFood 0x5FA9F0 / MagicWood 0x600E20 constructors (player NULL; the town is not passed
     on) and CallVirtualFunctionsForCreation (vt +0x658); the pot is kept in +0x600[type].
- Either way the point returned is the pot's GetNearestEdgeToPos(from) (vt +0x83C, Object 0x636DA0)
  (0x73E921..0x73E94E / 0x73EA0C..0x73EA34).
- There is always a pot: `Pot::Create` fails only when its allocation does (0x66CF81), and 0x73EA18 uses the result
  unchecked. `Pot::Create` has no check against an empty pile.

**When the town gets a storage pit.** `Town::SetStoragePit` 0x73EA60 (from `StoragePit::MakeFunctional` 0x732F30): +0x30
= the pit; each temporary pot +0x600 / +0x604 that is available and holds its resource → `Pot::SetupReaction` 0x66D660;
otherwise ToBeDeleted (`PileFood::ToBeDeleted` 0x66E100 closes its speed-up visual; `Pot::ToBeDeleted` 0x66D110 →
`Pot::RemoveReaction` 0x66D6A0); the slot = 0 (0x73EA79). `StoragePit::DeleteDependancys` 0x732CD0 → SetStoragePit(the
other pit) 0x73EA60: its empty temporary pot goes.

**Step 17 of `Town::Process`** (0x747450..0x7474A0), each temporary pot (+0x600 FOOD, +0x604 WOOD):

- not available (IsAvailable vt +0x2C != 1, 0x747461..0x747469) → the slot = 0 (0x74749A);
- 0x74746B..0x747470: the amount +0x70 != 0 → it stays;
- 0x747472..0x747490: no GetStoragePit 0x73B5B0 → it stays; with one that is functional (vt +0xD4), the empty pot →
  ToBeDeleted(0) (vt +0xC; `PileFood::ToBeDeleted` 0x66E100, then `Pot::ToBeDeleted` 0x66D110 → RemoveReaction
  0x66D6A0) and the slot = 0 (0x747492..0x74749A);
- otherwise it stays.

**Taking resources back.**

- Town::GetGameTurnResourceLastRemovedModifier 0x740030 (player, type): player >= 8 or type not FOOD / WOOD → 0
  (0x740037..0x740049); never taken → 1 (0x740061); else r = min((turn − last) / maxGameturnsForBeliefAfterRemoving
  FromStoragePit (GTownInfo +0x100, 1000), 1) (0x74006F..0x7400B4) and r³ (0x7400BA..0x7400C8). How much a player
  giving back what it has just taken counts.
- Town::SetGameTurnResourceLastRemoved 0x7400D0 (player, type): the same bounds, then +0xEC8[player][type] = the turn
  (0x7400E6..0x7400F9). Only `Abode::DoResourceRemoving` 0x404FD7 calls it.

### Town aggression

#### The record

The town keeps one 0x80-byte slot per game player (all 8, the neutral one included) at Town + 0x9F4 + n × 0x80:

- +4 the aggression (float), +8 the turn of the player's last attack;
- +0x10 the trust (bw1-decomp `PlayerTownInteract::Trust`, 1 at the start; written by Process, never read in play: the
  slot base is taken only by the Town constructors, `UpdateAggressor` 0x73C9F4, `ProcessPlayerInteract` 0x73DF3B, Save
  0x73F342 and Load 0x73FAAC, and no direct +0xA04 read exists);
- +0x40.. a copy of the last attack's EffectValues (type, applier, numbers, players).

Then +0xEAC the last aggressor (GPlayer*), +0xEB0 its turn (0 = never: as in the original, turn 0 is "none"; read by
`Villager::ReactToMagicShieldPriority` 0x765C28), +0xEB4 the protection multiplier, +0xEB8 the mercy multiplier,
+0xEBC this turn's mercy, +0xEC0 this turn's protection. The multipliers start at 1.

#### Recording an attack

Town::UpdateAggressor 0x73C9B0 (values, value):

1. The aggressor is `EffectValues::GetCausedPlayer` 0x525910 (the values' player +0x3C, else the applier's
   `GetPlayer`), and with none the neutral player (`g_game` +0x205A5B, the neutral player index, 7: bw1-decomp
   `GGame::NeutralPlayerIndex`; not the interface's player) (0x73C9BC..0x73C9DF, 0x73C9C7).
2. 0x73C9E3..0x73CA76, the slot's weight: when the slot's aggression is exactly 0 the value gets GTownInfo +0xAC
   `firstTimeDamageDoneAddition` (0x73C9ED..0x73CA15). The aggressor equal to the town's player (a pointer compare, so
   a neutral town attacked by the neutral player counts as its own): value × +0xEB8, then +0xEB8 × 0.9 (0x8C5844);
   else value × +0xEB4, then +0xEB4 × 0.9 (0x73CA1D..0x73CA5C). `GGuidance::TownAttackSFX` 0x71B7C0.
3. +0xEAC = the aggressor (0x73CA82), +0xEB0 = the game turn (`g_game` +0x205A40, 0x73CA98).
4. fn_0073E0F0 on the slot (0x73CA9E): +8 = the turn, the EffectValues copy, +4 += value (0x73E0F0..0x73E14E).
5. The help spirit's remark about a creature's attack (0x73CAA3..0x73CB29, 0x73CAAA..0x73CB29).

No random number is drawn by any of these (the guidance sound may).

#### Who calls it

Its three callers (no other `call` or `jmp` reaches it):

- **`Object::ApplyEffect` 0x637B7D**: a destructive effect (`IsDestructive`), the object's `GetTown` (vt +0x48), an
  applier that is a `GameThingWithPos`, and `ConvertTemperatureToDamage(burn) + damage != 0`; the value is
  `Object::GetAggressorValueFromDamage` 0x639610 = that harm × the object's info `AggressorValue` (only Object has it).
  After ReduceLife, DestroyedByEffect and the crush reaction, before the alignment. Every damage effect reaches it: the
  miracles' effects, a building's physical destruction (`Abode::ApplyEffectsDueToPhysicalDestruction`, with +0x3C the
  hand's player), and a Living's physics impact: `Living::ReactToPhysicsImpact` 0x5ED3E0 builds `EffectValues(CRUSH 3,
  GetGameObjectWhoHitMe, PhysicsObject::GetPlayer)` × ((G − 2) × 0.03) and calls ApplyEffect (vt +0x5CC) when
  `IsEffectReceiver` (vt +0x774). A creature that struck it counts one more object destroyed on a kill (Creature
  +0x11C0) and moves its own alignment (+0x168) rather than a player's; the owner keeps the harm per player (`owner
  +0x94[4 × p.+0xB5] += harm`, 0x637BC5..0x637BEA).
- **`Object::ReduceLifeDueToBurning` 0x637C91**: unless the object is not hurt by fire, after ReduceLife, a town and a
  damage != 0: `EffectValues(BURN, temperature, no applier, 1.0, the fire's player)` and the value damage ×
  `AggressorValue`. The Field override 0x52A050 does not reach it.
- **`PhysicalShield::ReactToPhysicsImpact` 0x72D783**: the spell's town (+0x60 → +0xFC), `EffectValues(2, 0, the
  hitter, 1.0, PhysicsObject::GetPlayer of the hitter's body)` and value 0 (so only the first attack's addition).

#### Players, effects and towns of the objects involved

- The player a hitter answers (`GetPlayer`, when no hand threw it): Villager 0x7502F0 its town's player (+0x2C of the
  town), none without a town; Creature 0x473F20 +0x1070; Tree 0x55D8C0 none; Rock 0x6E77A0 +0x90, the player it was
  made with or the neutral one (its constructor 0x6E6F70; Fragment and the felled tree are rocks); MobileStatic
  0x6088B0 its holder object's (+0x7C) player or the neutral one; MobileObject 0x607230 (pots, seeds) the neutral one;
  an animal (Living, MobileWallHug, Mobile, Object: none of them has its own) `GameThing::GetPlayer` 0x570130, the
  neutral one.
- The impact effect for a Living: `EffectValues(3, ...)` copies info.dat effect 3 (the table at 0xCC94C8, 0x34 a row:
  numbers +0x10, multiplier +0x2C) (0x524FE0), times the scale. `Villager::IsEffectReceiver` 0x751D70: with a heal
  above 0 `!IsDead` (vt +0xAF4), otherwise `IsReachable` (vt +0x530, 0x756460); `Object::IsEffectReceiver` 0x4029E0
  gives 1 to the rest. `Object::GetDamageEffect` 0x637D00: the burn to the fire, then the crush and hit (numbers 1 and
  2) each times `FillInEffectDefenceMultiplier` 0x637900 (the info's multipliers at +0x90), added when above 0.
- A field's info: the Field constructor 0x527DD0 hands its GAbodeInfo to the Abode constructor 0x401350, which hands it
  on to MultiMapFixed and so to Object +0x28; the GFieldTypeInfo goes to +0x120. So the aggressor value
  (`GetAggressorValueFromDamage` 0x639610: Object +0x28 → +0xEC) is the GAbodeInfo row's.
- Which objects have a town (`GetTown` overrides): Villager (+0x12C), Abode (+0x98), Field, FishFarm, Flock,
  TownArtifact, Scaffold, BuildingSite, PlannedAbode, MultiMapFixed, TotemStatue, PotStructure, PrayerSite, PrayerIcon,
  TownSpellIcon, TownDesireFlags, Meeting, PlaytimeElement, Town; Object and GameThing answer none (0x419950,
  0x56FF10), so no animal, tree or creature.

#### The fade and the desires (step 13)

`Town::ProcessPlayerInteract` 0x73DEC0 (`Town::Process` step 13, 0x747436, after the desires of step 6, so the desires
read last turn's sums): mercy = protection = 0; for each of the 8 players in array order (`GetNextPlayerAndNeutral`
0x550980):

- fn_0073E190 fades the slot: aggression × 0.999 (0x8CB92C), and the +0x10 value + GTownInfo +0xA0 while it is under 1
  (`PlayerTownInteract::ReduceTrust` fn_0073E160 would lower it by +0x9C, floored at 0, but has no caller);
- then, unless the town is neutral, the town's own player's aggression is added to the mercy, everyone else's to the
  protection (a neutral town: all to the protection).

Protection < 0.1 (0x8AB22C): +0xEB4 × 1.001 (0x99A02C), and if that is under 1 × 1.001 again, else 1; the same for the
mercy and +0xEB8 (0x73DF8D..0x73E034). GTownInfo +0x94 `playerInteactDecay` is not read: the fade is the 0.999 literal.

#### The script's question

`GScript::GetTimeSinceObjectAttacked` 0x6FF990 (native 442): POP the object (`GetScriptGameThing`), the largest float as
the default (0x942210), POP the script player (`ftol`, `ConvertScriptPlayerToGamePlayer` 0x6EB9A0: 0 → the neutral
player, n → n − 1; `GGame::GetPlayer` gives none from 8 up). A player and a town (vt +0x1B8 `IsTown`) whose slot
aggression is above 0.15 (a double, 0x8CF188; `test ah, 0x41`): (turn − slot +8) as a signed integer × the ms per turn
[0xD01A38] (`fimul`) × 0.001 (0x8AA3B0) (0x6FF9F2..0x6FFA65). Otherwise the largest float. The challenge script's
KhazarAI (Land 2) asks it for Khazar's two towns and player 1, against 3 seconds.

### Artefacts

🧪 From raffclar's research (bw1-decomp `docs/openblack/town/artefacts.md`, read in Ghidra on the 1.20 executable); not
yet checked against the frozen v1.42 exe.

- **Which things.** A fixed object whose kind has an artefact multiplier above 0 (rocks and other mobile statics, toys,
  lanterns, fences, dead trees, the idol, gate totems), at the end of its physics with the insert back into the map, put
  down from the hand (FROM_HAND and LANDED) by a player who is not the neutral one (`Fixed::EndPhysics`). The nearest
  object suitable for an artefact within 50 m (`Pos.FindObject(IsSuitableForArtifact, 50)`): an abode gives its town
  (`Town::AddArtifact`), a worship site itself (`WorshipSite::AddArtifact`). One already worth more than 1 that last
  belonged elsewhere makes a LOOK_AT_OBJECT reaction. A hand taking it (`Fixed::InterfaceSetInMagicHand`) takes it out of
  its town, and the hand's player owns it.
- **The town's turn** (step 7 above): its artefacts, newest first. A dance that has started sends its dancers to
  ARTIFACT_DANCE (230); a dance in a town that isn't the owner's gives belief (`Town::GiveBeliefForArtifactIfNecessary`:
  when the turns since the last gift plus `GameRand(gap / 4)` pass the gap, `belief.AddToBelief(player, value, artefact,
  1, GOOD)`); a dance that is over raises the worth in the owner's town and sends the dancers back to
  WAIT_FOR_ARTIFACT_DANCE (253) while the town most wants relaxation, else to their lives; a dance left with no dancers
  stops.
- **Relaxation.** `Town::SetVillagerActivity`, called by `Villager::CheckSatisfyRelaxation`: the best of the football
  pitch's, the player's creature's and each artefact's activity desire, and that thing's own SetVillagerActivity. The
  dance is chosen by the object's width (under 7 m the small dance, under 10 m the medium one, otherwise the one for
  objects wider than 20 m); the wish falls with distance (to 250 m) and with how full the dance is. Waiting dancers stand
  at a clear spot within 12 m for the villager info's time, then the dance starts.
- **Worth.** A dance in the owner's own town adds the town's modifier (`Town::GetModifierForArtifacts`: the belief in
  the owner, up to 2 whole and a third of the rest, 4 at most; 1 for the neutral player) × the kind's multiplier × the
  share of the most dancers × the share danced × land balance 6, × 300, divided by the worth once it is over 1. A
  worship site powers up its artefacts every 1000 turns by its table's multiplier and its share of the player's people.
- **Look.** Worth over 1 starts the magic effect on the object (MAGIC_FX_ON_OBJECT) and makes it 1 + its worth times as
  impressive; worth over 2 shows the owner's symbol over it at half size, while it is in a town.
- **Wonders.** A planned wonder adds each nearby thing's artefact worth (`GetTownArtifactValue`, see the wonder's power
  above).

### Sources

- bw1-decomp: `src/Black/Town.cpp`, `TownDesire.cpp`, `Belief.cpp`, `Abode.cpp`; the rest read in the exe with the
  xref index.

## openblack

**Done**: the town's turn, the belief fold, the temporary pots, TownStats, the lists and queries, the meeting point
and the aggression. The network checksum and the debug trace 0x7457C0 (replaced by `OPENBLACK_TOWN_TRACE`) are not
ported (no behaviour).

- **The town's turn**: `Town/TownProcess.{h,cpp}` (`town_process`), `Town/TownStats.{h,cpp}`,
  `Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`; the desires in `ECS/TownDesire.{h,cpp}` (see
  [Town desires and distribution](villager-decisions.md#town-desires-and-distribution)); the town-needs sum is
  `TownNeedsSum`. Differences, each marked:
  - **⚠️ approximate** TownStats is recomputed at the start of `Town::Process` from the entities (the original adds when
    adding and removing); same counts, the float sums in a different order.
  - **⚠️ approximate** all script homes count as functional, and `Town::storagePit` (+0x30) / `Town::creche` (+0x744)
    are set by script creation (AbodeArchetype) instead of StoragePit / Creche::MakeFunctional (the last storehouse
    wins; the first creche).
  - **⚠️ approximate** the turn's influence (`magic::ProcessTurn`) is computed after the desires and not inside each
    `Town::Process` (the desires do not read it).
  - **⚠️ approximate** `SET_TOWN_DESIRE_BOOST` with negative d does not write (the original writes outside the array).
  - **🔍 inferred** +0x90 is 0 in a new game (only Load writes it); TRIBE_TYPE = the `Tribe` enum for TribeMultiplier;
    "local player" = PLAYER_ONE; the list +0x770 is empty; fn_555240 (step 23) needs no call (the influence drawing
    recomputes it).
- **The belief**: `src/ECS/Town/TownBelief.*` (`town_belief`). The believers-gained number is made by
  `town_belief::ProcessOncePerTurn` with `FloatingNumberArchetype::Create(..., 0xFFFFFF00, 4)` at `map_coords::ToWorld`
  of `HandSystemInterface::GetHoverLandPoint` (the corner without one). Testbed: `objects.believers_gained`.
- **The temporary pots**: `ecs::town_stores::GetTemporaryResourceStorePotOrPos(town, from, type)`, the slots
  `components::Town::temporaryPots`, the pile `magic::objects::CreateMagicResourcePile(..., allowEmpty = true)`.
  openblack's `PotArchetype::Create` makes no pile of amount 0 unless `allowEmpty`: an openblack rule, not the
  original's. **⚠️ approximate** `town_queries::GetCongregationPos` returns x and z only; the cache's altitude (+0xF18)
  is dropped (the pile stands on the land anyway).
- **Aggression**: `ECS/TownAggression.{h,cpp}` (the pure rules, from raffclar's tree), the record in
  `components::Town::aggression`, `town_emergency::UpdateAggressor` / `AttackTown` (`ECS/Town/TownEmergency.cpp`), the
  turn in `town_process` step 13, the script native `GET_TIME_SINCE_OBJECT_ATTACKED`. The hitter's player is
  `town_emergency::HitterPlayer`; the Living's impact effect `villager_physics::ImpactEffectOf` and `ImpactDamage`.
  Tests: `test/test_town_aggression.cpp`, `test/test_repairs.cpp`. The guidance sound `TownAttackSFX` and the help
  spirit's creature remark are not ported.
- **Worship**: `ECS/Systems/Implementations/VillagerWorship.h` holds `IsAvailableForWorshipSite`,
  `IsAtOrOnTheWayToWorshipSite` and `RemoveVillagerFromWorshipSite` as above (the last called from
  `ECS/Town/TownVillagers.cpp`'s RemoveVillager). **⚠️ approximate** the walks to the site and to the dance place use
  openblack's WallHug pathfinding; the original walks on the footpath (`Living::SetupMoveToOnFootpath`) and then with
  `Villager::SetupMoveToPos` ([Worship](../magic/worship.md#the-worship-percentage-and-the-villagers)).
- **Artefacts** (raffclar's lane, unverified against v1.42): his pure rules `ECS/ArtefactRules` (test
  `test_artefact_rules`) and `ArtefactSystem` behind `Locator::artefactSystem`, refitted onto ours: the trigger
  `object_physics::ConsiderArtefact` at the end of physics (`PhysicsGameHooks::EndPhysics`, the dead trees' and the
  mobile statics'), the nearest building by `map_cells::FindNearestInSpiral`, the turn at `town_process` step 7, the
  belief through `town_stores::AddToBelief`, the offer through `villager::CheckSatisfyRelaxation`, the states 230 and
  253 in `VillagerArtefactDance` (the dance step shared with the scripts' dance, `villager_dance::PerformDance`), the
  power-up at the end of the worship site's turn, the glow through the spot visuals
  (`ParticleSystemInterface::SetSpotVisualMagnitude`), the symbol beside the town belief symbols (`psys::town_belief`)
  and the worth in the wonder's power (`TownArtifactValue`). Debug: Debug > Windows > Artefacts lists them and raises
  one's worth. **🔍 inferred** the hand's player is PLAYER_ONE, as for the dead tree's reaction.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

#### The turn and the queries

- `src/ECS/Town/TownProcess.cpp`: `TownCentreProcess`, `ProcessTown`, `PruneSites`, `ProcessTownRepairs`,
  `ProcessTemporaryPots`, `ShuffleDue` (`k_ShuffleIdFactor` is the × 20).
  - `ProcessTown` step 0 (openblack): the TownStats of the turn (+0x610) from the entities, after the pruning (nothing
    between 0x747390 and `TownDesire::Process` reads them).
  - Steps 3-5 are not called in `ProcessPlayers`: openblack runs them for every town in its own turn hook
    (`influence::ProcessTurn` → `influence::ProcessTowns`, InfluenceSources.cpp, inside `magic::ProcessTurn`), every
    turn, the Process part in the same order. So the influence of the turn is computed after the desires instead of
    just before each town's; the desires do not read it (**⚠️ approximate**). The player's alignment comes after its
    towns (For_Children reads the last turn's): `ecs::effects::alignment::ProcessPlayers` in `magic::ProcessTurn`.
  - Step 4 runs `Abode::Process` for the classes that do not override it; the Workshop, TownCentre and SpellDispenser
    overrides run below, each with its Abode part, and the Field's too. The site's Process runs here for every abode,
    before `abode_villagers::ProcessAbode`'s part. `ecs::workshops::Process` does all of Workshop::Process
    (`RunsAbodeProcess` in `src/ECS/Town/AbodeVillagers.cpp`: ProcessAbode at its end).
  - Steps 9-12: the list +0x770 is empty in a new game, not ported; the football is out of the plan; the spell icons'
    step is to confirm (whether their icons' step covers it). The 7 town desire flags (raffclar's lane, unverified
    against v1.42) are made, moved and shown each frame by `TownDesireSystem::UpdateFlags`, from his pure rules
    `ECS/TownDesireFlags` (test `test_town_desire_flags`) and `TownDesireFlagArchetype`: one per desire with a slot, at
    the store's special point 12 + slot (`town_queries::GetStoragePit`, shown while it is functional and the level is at
    least `showsAfterPercent`), easing to min(raw desire, 1) over 1 s on a `Zoomer` restarted every frame, rising 4.4 m,
    facing the camera (`billboard::YawToEye`), hidden by `NotDrawn` and skipped by the hand while hidden, no object
    shadows; the hand's "Need" tooltip 0xE88 in the flag's own words (0xECE + desire, `hand_tooltips::TownDesireFlagToolTip`).
    **⚠️ approximate** they read the live raw desire every frame, not step 12's field; they are made by the first frame
    that sees the town, not by CREATE_TOWN; "out of view" is a clip-space test, not the draw list. Protection and Mercy
    (step 13) stay 0 where the slots are not ported.
  - Step 15 reads the fire through `ecs::fire::IsOnFire`. Step 19 is `ecs::town_belief`. Step 21 has no function yet.
  - Step 22: `Town::SetTownEmpty`'s belief part is `town_belief::SetTownEmpty`; who sets 50 (`Town::RemoveVillager`
    0x73E2CD) is too. (pending) not called yet: nobody starts the empty countdown (+0xF20).
  - The conversion: `TownsOf` keeps the list's order (`Town::ownerListStamp`), so the taken town is the new owner's
    last.
- `src/ECS/Town/TownQueries.{h,cpp}`: `TownsNewestFirst`, `FindClearArea`, `GetCongregationPos`, `TownAbodes`, `TribeOf`,
  `Pulse`, `HelpTownOf`, `GetCreche`.
  - `TownAbodes` orders by the object creation index (+0x3C), descending, as the order of AddStructureToTown
    (**⚠️ approximate**: an abode joins its town when it is made).
  - `TownsNewestFirst` sorts by `Town::creationStamp` from high to low (the order `TownArchetype::Create` ran in = the
    script order): a counter that only goes up, the land's own counter (RegistryContext, 0 again with every land), not
    a process-wide static. Empty without a registry.
  - `GetCreche`: Town +0x744 (`Town::creche`) if it is a valid entity, else entt::null.
  - `town_queries::ScriptIdOf` is +0x5B4 (unset: `Town::id`). `Town::id` stays openblack's unique key for the map and
    the abodes (the exe's Abode +0x98 is a Town* pointer, which a unique key stands for).
  - `TribeOf` / Town::GetTribe: the town entity's Tribe component (TownArchetype), Tribe::NONE without one
    (`town_queries::TribeOf`; 🔍 inferred the same TRIBE_TYPE as Town +0x5B8).
  - `Pulse`: nothing for anything that is not a town; `TownProcess` reads and clears it. `HelpTownOf` reads
    `Town::stats` and the Transform.
  - town_queries::GetStoragePit 0x73B5B0 reads +0x30 (`town_stores::SetStoragePit`).
- `src/ECS/Town/AbodeQueries.{h,cpp}`: `GetArrivePos`, `GetPosOutside`, `GetDoorPos`. **⚠️ approximate**
  `GetArrivePos` reads the door point of the L3D (extra point 0, HasDoorPosition) through the abode's Transform: the
  engine's transform (LH3D vt +0x1C4) is not read. GetArrivePos reads only its Transform and Mesh.
- `src/ECS/Archetypes/TownArchetype.cpp`: `TownArchetype::Create`.

#### TownStats and the abodes

- `src/ECS/Town/TownStats.{h,cpp}`: `AddVillager`, `AddAbode`, `IsCivic`, `FindAbodeInfo`, `Compute`.
  - `town_stats::Compute` recomputes the stats at the start of each Town::Process: `Add(Villager)` 0x7492E0 for each
    villager of the town (Villager::town) and `Add(Abode)` 0x7498C0 for each of its abodes (Abode::townId; the fields
    too, they are Abodes) that MakeFunctional has counted (Abode +0x7C bit 1, Abode::addedToTownStats); the civic plans
    (+0x24) and the wood at the town's building sites (+0x100). Only the counts the original adds at once
    (AddVillagerToTown's, `TownStats::Add(Villager)` / Remove 0x7493C0: adults / children +0x08 / +0x0C and the sexes
    +0x54 / +0x58) are kept between turns; the places (+0x4C / +0x50) are recomputed. So every remove (fn_749B10,
    fn_749B50, VillagerMoveOutOfAbode, ChildToAdult) is a recompute.
  - **⚠️ approximate** +0x100 is summed in list order, the original in the order of the changes.
  - `AddAbode`: +0x50 / +0x44 / +0x48 are not kept (no reader). +0x4C is recomputed as max villagers − the adults
    housed (+0xB4); (not verified) that it equals the book-keeping (the AbodeVillagers owner to confirm).
  - The civic set is `town_stats::IsCivic`'s (the plans' jump table too).
  - TownStats +0xEC (`Town::woodUsedForBuilding`, Town +0x6FC) is kept, as the stats are recomputed.
- `src/ECS/Town/AbodeVillagers.{h,cpp}`: `k_Thousandth` (0.001 and 500), `IsMaleVillager`, `DropCounts`,
  `GetRoomLeftForAdults`, `GetRoomLeftForChildren`, `IsTooCrowded`, `GetPercentAbodeFullWithAdults`,
  `GetPercentAbodeFullWithChildren`, `ScoreForAdding`, `CalculateScoreForAddingVillagerToAbode`, `DesireToGainMale`,
  `CalculateDesireToGainMale`, `DesireToGainVillager`, `CalculateDesireToGainVillager`, `AddVillagerToAbode`,
  `RemoveAliveVillagerFromAbode`, `RemoveDeletedVillagerFromAbode`, `RemoveAllVillagersFromAbode`, `ChildToAdult`,
  `SwapMaleForFemaleFrom`, `TakeVillagerFrom`, `RunsAbodeProcess`, `RemoveFromHomelessList`, `RemoveFromVagrants`,
  `VillagersOf`, `AbodeType::TownCentre`, `AbodeType::Workshop`. `VillagersOf` gives an empty list for anything that
  is not an abode.
- `src/ECS/Town/TownVillagers.{h,cpp}`: `k_ShuffleIdFactor`, `CountVillager`, `IsVillagerInHomelessList`,
  `AddVillagerToTown`, `UseFood`, `RemoveVillager`, `ShuffleDue`, `ShuffleCompare`, `PlanShuffle`,
  `ShuffleVillagersAroundAbodes`, `DesireToGainMale`, `CalculateDesireToGainMale`, `SortShuffle`, `AddToHomelessList`,
  `RemoveFromHomelessList`, `IsVagrant`, `RemoveFromVagrants`, `AddToVagrants`, `ChildToAdult`, `ClearVagrants`,
  `AddVillager`.
  - `CheckAddWorshipSite` is `worship::town::CheckAddWorshipSite`.
  - The shuffle sorts with the VC6 `_qsort` as `town_desire::MsvcQsort`; the pair loop is also exposed without the
    moves, for the tests (which pair it tries first and how: `first` the one whose function runs, `male` its argument;
    `swap` = SwapMaleForFemaleFrom, else TakeVillagerFrom).
  - `ChildToAdult` (openblack guard): the children not below 0 (a cache rebuilt each Town::Process).
  - `ClearVagrants`: a new map (Script::BeginMapFeatures) and the tests start with no vagrants.
  - `females` reads +0x58 (Town +0x668); ShuffleVillagersAroundAbodes reads them. Changed only by `ecs::town_villagers`.

#### Building sites, plans and placement

- `src/ECS/Town/BuildingSites.{h,cpp}`: `k_MinusSixPi`, `k_RandomBuildSpread`, `k_NextPosMetres`, `k_ClearAreaFactor`,
  `k_CloseToSite`, `k_WoodDistanceMax`, `k_PileHalfSpread`, `k_BestSiteStart`, `k_CongregationClearance`, `k_HouseCap`,
  `k_CitadelPileAngle`, `k_WorshipSlotAngle`, `k_WorshipSitePile`, `CheckWhenNewBuildingCreated`,
  `CreateBuildingFromPlan`, `InsideXZ`, `EdgeHit`, `PosBuilderProcess`, `ComputeRing`, `plans::RemovePlanned`,
  `plans::GetAbodeType`, `plans::IsCivic`, `plans::GetDesireToBeBuilt`, `plans::CreatePlanned`,
  `plans::CreateFromBuilding`, `building_sites::Create`, `building_sites::ToBeDeleted`, `building_sites::Process`,
  `building_sites::InsertBuildingSite`, `building_sites::AddBuildingSiteFromPlan`,
  `building_sites::AddBuildingSiteNoFixedCheck`, `building_sites::RemoveBuildingSite`,
  `building_sites::ChooseTownRepair`, `building_sites::ProcessTownRepairs`, `building_sites::RequestBestPlanned`,
  `building_sites::RequestANewAbode`, `building_sites::GetBuilding`, `building_sites::GetTown`,
  `building_sites::GetDesireForVillagers`, `building_sites::GetDesireToBeRepaired`, `building_sites::GetResource`,
  `building_sites::GetWoodForStats`, `building_sites::AddResource`, `building_sites::RemoveResource`,
  `building_sites::GetPileWood`, `building_sites::SiteOfPile`, `building_sites::CreatePileWood`,
  `building_sites::GetResourcePosAndYAngle`, `building_sites::CreatePilesOfWood`, `IsCivic`, `Abode::k_Built`,
  `BuildingSite::Kind::Standard`, `BuildingSite::Kind::Workshop`, `CreatePlanned`, `GetBuildingSiteInList`,
  `GetRootBuilding`, `GetMaxBuilders`, `GetPileWood`, `GetDoorPos`, `RemovePlanned`, `GetAbodeType`,
  `GetDesireToBeRepaired`, `GetDesireToBeBuilt`, `CreatePlannedNoFixedCheck`, `Create`, `Process`,
  `IsBuildingHappening`, `InsertBuildingSite`, `RemoveBuildingSiteFromList`, `AddBuildingSite`,
  `AddBuildingSiteFromPlan`, `AddBuildingSiteNoFixedCheck`, `RemoveBuildingSite`, `IsBuildingSiteValid`,
  `GetBestBuildingSite`, `GetBestRepairBuildingSite`, `ChooseTownRepair`, `ProcessTownRepairs`, `RequestBestPlanned`,
  `RequestANewAbode`, `PruneSites`, `GetBuildersNeeded`, `GetDesireForVillagers`, `GetClearAreaRadius`, `GetRadius`,
  `SetRepairBase`, `ShouldIGetWood`, `GetWoodForStats`, `AddResource`, `BuildBy`, `GetRandomBuildPos`,
  `GetNextPosFromIndex`, `GetBuildPos`, `AddBuilder`, `RemoveBuilder`, `RemoveScaffold`, `SiteOfPile`,
  `CreatePileWood`, `GetResourcePosAndYAngle`, `CreatePilesOfWood`, `SetDesireBoost`, `CreateFromBuilding`,
  `ProcessTown`, `AddWoodUsedForBuilding`, `AddScaffold`.
  - `k_FootballEnabled`: openblack has no football pitch (GetDesireToBeBuilt's case 0x1004 then returns 0).
  - `CreateBuildingFromPlan`: **⚠️ approximate** both plan kinds are `AbodeArchetype::Create` under construction (it has
    no Init / surrounding-objects split); PostCreatePlanned's footpath link (+0x64 moves to the building) is not ported.
  - `plans::CreateFromBuilding`: `PlannedAbode::Init`'s +0x48 = town is not ported; `plans::AddPlanned` writes the
    creation turn; `plans::CreatePlannedNoFixedCheck` then deletes the plan from the list. The plan component leaves
    out +0x34 (no known use) and +0x38 GFootpathLink* (footpaths not ported); `ecs::plans` converts the plans into
    abodes under construction (CreatePlannedNoFixedCheck 0x405770 / 0x744550) and runs CheckWhenNewBuildingCreated
    after PostCreatePlanned. A citadel heart's plan is `CitadelArchetype::CreatePlan`, its `info` None and its info the
    GCitadelHeartInfo of `heartInfo`; `CitadelArchetype::CreatePlannedNoFixedCheck` converts it. Its owner
    `Town::owner`.
  - `building_sites::ToBeDeleted` step 3 takes the site out of g_game +0x205CAC in the original: openblack keeps no
    global list.
  - `building_sites::GetTown`: `TownOf` answers null for anything that is not an abode.
  - `building_sites::GetResource`: no IsAvailable test: openblack skips a pile already gone. A site pile answers its
    own amount through `object_resources::GetResource`.
  - `building_sites::CreatePileWood`: **⚠️ approximate** openblack's piles keep no +0x78 (WorkshopOfPile finds a
    workshop's pile by its +0xD0): the pile is made as the workshop's +0xD0 (Workshop::CreatePileWood 0x779630, the same
    point), so it is owned and deleted with the workshop instead of one lost entity per call. No openblack caller
    reaches this (the site's AddResource goes to the workshop).
  - `building_sites::RemoveResource`: the stats' −= is recomputed. Wood-for-stats and site adds and removes are counted
    by `town_stats::Compute`.
  - `AddWoodUsedForBuilding`: the player's GameStats (player +0xA44 +0xA8) are not ported.
  - `AddScaffold`: the scaffold's +0x68 = this (0x43BF18) is written by the caller (`ecs::scaffolds`,
    `components::Scaffold`'s writer).
  - The site's entities are those with `components::BuildingSite`; the town's site list is changed only by
    `ecs::building_sites`.
- `src/ECS/Town/ScaffoldPlans.{h,cpp}`: `k_AbodeNumbers`, `k_WonderRadius`, `scaffold_plans::GetNewPlannedBuilding`,
  `scaffold_plans::GetWonderPower`, `TownArtifactValue`, `SuitableForFixed`, `IsSuitableForFixedObject`,
  `TownDesireInfo::ToBuildWonder`, `GetNewPlannedBuilding`, `GetWonderPower`.
  - `GetNewPlannedBuilding`'s tribe is 🔍 inferred the same TRIBE_TYPE as Town +0x5B8 (`town_queries::TribeOf`).
  - `TownArtifactValue`: (pending) neither value is ported (TownArtifact, Scaffold 0x6EA770, the abodes...): 0 for every
    object, so the wonder's power is the raw desire (at least 0.25).
- `src/ECS/Town/TownPlacement.{h,cpp}`: `k_AreaMinStart`, `ExtendTownAreaBy`, `SetTownArea`, `GetTownAreaCentre`,
  `PositionCellIsBuildable`, `SuitableForFixed`, `IsSuitableForFixedObject`, `IsSuitableForFixedAbodeInTown`,
  `ExtendTownArea`, `k_InsideOtherTown`, `IsSuitableForFixed`, `IsOkToCreateAtPos`.
  - `SetTownArea` (`town_placement`) is called only where the original calls it: AddStructureToTown
    (`AbodeArchetype::Create`: the `newcomer`, whose radius is 0 there) and RemoveStructureFromTown. The fields, which
    openblack keeps as abodes of the town, are in AbodesOf once (a second pass over them would not move the
    rectangle). The camera point at +0x598 is not ported.
  - `ExtendTownAreaBy`: x × 10 × 2^-16 is `map_coords::ToMetres`' one rounding.
  - `GetTownAreaCentre`: **⚠️ approximate** the x87 sum and product are done in float.
  - `PositionCellIsBuildable`: 0x603DC0 only (0x603F16..0x603F75): a land cell on the coast line → 0.
  - `SuitableForFixed`: openblack builds its NewCollide shape directly (`map_collide::FromMesh`), its cells are
    `map_cells::DescriptorCells`, cut at a marked cell off the map the same way as GetNext. `sea_cells::IsWater`: no
    landscape cell is water (1) too.
  - `IsSuitableForFixedAbodeInTown`: the nearest other town is `map_cells::GetNearestTownCells` (the rectangles as they
    are kept).
  - `IsSuitableForFixedObject`: `under` is a MultiMapFixed, or entt::null.
- `src/ECS/Town/TownFeatures.{h,cpp}`: `ForestReference`, `AsssignTownFeature` (newest first through
  `town_queries::TownsNewestFirst`; called from Game::LoadMap after the map's script).

#### Belief, stores and the emergency

- `src/ECS/Town/TownBelief.{h,cpp}`: `k_Neutral`, `Amount`, `PlayHelp`, `Reduce`, `ResetDesireThresholds`, `Init`,
  `SetCap`, `GetCap`, `SetBeliefInPlayer`, `AddToBoredomMultiplier`, `GetBoredomMultiplier`, `RivalRecentRatio`,
  `Fold`, `TakeOverTown`, `ProcessOncePerTurn`, `CheckLosingBelief`, `DrawBelief`, `QueueBeliefSprite`,
  `PopBeliefSprite`, `LostTownScale`, `Locator::entitiesRegistry::has_value`, `k_Desires`, `k_ProcessEvery`,
  `k_AmountScale`, `k_ToolTipMinimum`, `k_MaxBeliefSprites`, `k_NoPlayerColour`, `k_RelativeStart`,
  `k_RivalRecentStart`, `GetAddedThisPeriod`, `GetAddedThisPeriodRatio`, `ReduceBelief`, `GetMaxBeliefMeNotIncluded`,
  `RelativeBelief`, `SetTownEmpty`, `BeliefRatioVsRivals`.
  - `PlayHelp`: fn_0071CD70(&town.pos) 0x43872B and fn_0071CDF0(&town.pos) 0x438755 (the help sprites of the fold).
  - The `components::Town` belief block keeps the GBelief without its vtable and Base, the defaults as above, so that a
    town made without Init still caps at 10; Town +0x5D8 and +0x5DC, read only by the belief, are kept here too. It is
    changed by `ecs::town_belief` and `ecs::town_stores::AddToBelief`. LHVM SET_OBJECT_BELIEF_SCALE is not ported.
  - `GetBeliefInPlayer`: 0 for a slot out of range (openblack's guard).
  - `RivalRecentRatio`: `allied` (pending) may be empty.
  - `BeliefRatioVsRivals`: **⚠️ approximate** openblack keeps no active flag: the 7 non-neutral slots.
  - `ProcessOncePerTurn`: the tooltip is `help::tooltips::Force`; the yellow floating number (pending): openblack has
    no ValueSpinner. Each player's summed belief goes to its GameStats (`game_stats::WorldBelief`); Game.cpp calls it
    every turn.
  - `TakeOverTown`: the list move fn_0064C090 is `town_process`' ProcessPlayers (**⚠️ approximate**: TownsOf orders by
    `Town::id`). The population goes to the new owner's GameStats (`game_stats::TownTakenOver`). (pending) the rest of
    fn_00649810 / fn_0073A7D0 listed above.
  - `DrawBelief`: `town_belief::DrawBelief` feeds the belief-sprite queue; without a thing openblack uses 0 (the
    original's point is uninitialised); the debug "B n" ValueSpinner of g_game +0x14 bit 0x4000 is not ported (a debug flag no one sets). The
    BeliefSFX is (pending).
  - `PopBeliefSprite`: (pending) openblack has no SF_BeliefSprite rule yet: nobody drains the queue, so nothing is
    drawn.
  - `LostTownScale`: kept by `land_balance` (LostTownScale / SetLostTownScale), reset with it when a map loads.
- `src/ECS/Town/TownStores.{h,cpp}`: `GetTemporaryResourceStorePotOrPos`, `AddToBelief`, `SetStoragePit`,
  `ProcessTemporaryPots`, `k_ClearAreaA`, `GetGameTurnResourceLastRemovedModifier`, `SetGameTurnResourceLastRemoved`.
  - `AddToBelief`: (openblack) the player bound is a guard of the array; the original indexes with GetPlayerNumber
    unchecked (0x437EBB). The pending part is (pending).
  - `SetStoragePit`: the reaction is `animal_ai::SetupPotReaction`; then the common deletion (`ecs::ToBeDeleted`). The
    pit is set for a whole (script) pit at its creation, a plan's when built (MakeFunctional); likewise the creche.
  - The slots are changed only by `ecs::town_stores`.
- `src/ECS/Town/TownEmergency.h`: `ProcessTownEmergency`, `ProcessTown`, `UpdateAggressor`.
  - `UpdateAggressor` (Town::UpdateAggressor 0x73C9B0; `effects::ApplyEffect` calls it) ports the record only (+0xEAC = `causedPlayer`, +0xEB0, what
    ProcessTownEmergency's refresh while attacked reads), written also by a physical shield's impacts
    (Magic/Objects/MapShield). (not ported) the per-player aggression slot fn_0073E0F0 (town + n × 0x80 + 0x9F4, plus
    GTownInfo +0xAC when 0, × +0xEB8 for the town's own player else +0xEB4, which then × 0.9; 0x73C9E3..0x73CA76), the
    guidance TownAttackSFX 0x71B7C0 and the creature part 0x73CAA3... The other aggressions (damage, fire, buildings
    crushed) still do not reach it.
  - The emergency turn +0xF1C is changed by `ecs::town_emergency`.
- `src/ECS/Town/Graveyard.{h,cpp}`: `SetGraveyard`, `AddDead`, `MakeFunctional`, `DeleteDependancys` (`ecs::graveyard`;
  `abodes::MakeFunctional` runs the class part after `Abode::MakeFunctional`).
- `src/ECS/Town/Wonders.{h,cpp}`: `PowerSlot`, `wonders::Create`, `wonders::DeleteDependancys`, `SetPower`,
  `RemoveFromPlayer`, `Create`, `AddToPlayer`, `DeleteDependancys`. `PlayerWonderPower` is `PlayerMagic::wonderPower`
  (GPlayer +0xB8[tribe]); (pending) who reads it (inferred: a wonder bonus per tribe) and who clears it (the GPlayer
  ctor / OnEndOfClearMap 0x64CD00 are not read for it).

#### Workshops

- `src/ECS/Town/Workshops.{h,cpp}`: `k_ScaffoldAreaReach`, `k_SnapRadius`, `k_BestWorkshopDistance`, `k_DesireEpsilon`,
  `ScaffoldWoodValue`, `SpecialPointOf`, `SlotPoint`, `SetSlotByte`, `StartScaffold`, `FinishScaffold`,
  `workshops::IsWorkshop`, `workshops::Create`, `workshops::OnInsertedInMap`, `workshops::MakeFunctional`,
  `workshops::DeleteDependancys`, `workshops::GetBestWorkshop`, `workshops::IsScaffoldAwayFromWorkshops`,
  `workshops::CheckScaffoldSnapToPoint`, `workshops::Process`, `workshops::GetSpaceInStore`,
  `workshops::GetVisualWoodDesire`, `workshops::GetDesireToBeSupplied`, `workshops::WoodWanted`,
  `workshops::IsResourceStore`, `workshops::GetSlotPos`, `workshops::GetFirstFreeSlot`, `workshops::CheckSnapToPoint`,
  `workshops::RemoveScaffold`, `GetPileWood`, `GetDesireToBeSupplied`, `GetSpaceInStore`, `GetVisualWoodDesire`,
  `Workshop::k_Slots`, `IsCloseToEP`, `OwnsScaffold`, `k_MaxWaiting`, `CreatePileWood`, `MakeFunctional`,
  `DeleteDependancys`, `AddWorkshop`, `RemoveWorkshop`, `GetBestWorkshop`, `IsScaffoldAwayFromWorkshops`,
  `CheckScaffoldSnapToPoint`, `WoodWanted`, `AddResource`, `RemoveResourceFromPile`, `GetSlotPos`, `GetFirstFreeSlot`,
  `IsPosWithinScaffoldAreas`, `CheckSnapToPoint`, `RemoveScaffold`, `FreeSlot`, `ScaffoldMoved`, `GetSlotState`,
  `OnInsertedInMap`, `GetResourcePosAndYAngle`, `RemovePotFromStructure`.
  - `k_PilePoint`: the special point 4; 9 is the ShowNeeds icon ((not ported) the icon).
  - `IsWorkshop`: the entity has a `components::Workshop`.
  - `Create`: for an abode `AbodeArchetype` has just made (its Abode, Transform and Mesh in place); the ShowNeedsVisuals
    takes one creation index ((not ported) the object). `OnInsertedInMap` (openblack): Create already made the pile, to
    keep the creation index order; it is taken out of its cells (RemoveMapObject vt +0x548) and put back (InsertMapObject
    vt +0x544) once AbodeArchetype has put the workshop in its map cells, so the pile is after it, as in the original.
  - `MakeFunctional` and `DeleteDependancys`: `abodes::MakeFunctional` runs the Abode part first and this after it;
    `abodes::DestroyedByEffect` runs `Abode::DeleteDependancys` as its own part.
  - `FinishScaffold`: (pending) the angle when the point is missing; the scaffold's slot bits are written through
    `ecs::scaffolds`' component. (not verified) that `SpecialPoint::yAngle` is fn_779910's Y angle.
    `worship::GetSpecialPoint` reads the special points.
  - `GetSlotState`: `k_SlotFree` / `k_SlotTakenAway` / `k_SlotOccupied`; 0 outside 0..2 or without a workshop; the
    padding byte is not kept. The scaffold's slot bits and owner +0x58 go through `scaffolds::GetSlot` / `SetSlot` and
    `scaffolds::GetOwner` / `SetOwner` (ECS/Scaffolds.h).
  - `AddResource`: callers through `object_resources::AddResource` on a workshop or its pile. `RemoveResourceFromPile`
    is PotStructure::RemoveResource on the pile, for ObjectResources.
  - `workshops::RemovePotFromStructure`: `MultiMapFixed::RemovePotFromStructure` (pending) not read.
  - `workshops::IsPosWithinScaffoldAreas`: the four tests, all asked, ORed.
  - `GetResourcePosAndYAngle`: `angle` may be null (openblack guard).
  - The workshop list is changed only by `ecs::workshops`.

#### The town component

- `src/ECS/Components/Town.h`: `wasBuilt`, `creationTurn`, `citadelHeart`, `females`, `doingNow`, `FilledArray`.
  - `doingNow` is brought back to >= 0 by fn_745CA0.
  - The order stamps (openblack): the order the Town ctor ran in (`TownArchetype::Create`, 1 up; 0 for a town made
    elsewhere), and one counter that reproduces the insertion order of the owner's list GPlayer +0xA50 (fn_0064C090
    0x64C090 appends at the tail, from the Town ctor 0x739597 and `GPlayer::TakeOverTown` 0x6498F2), so sorting by it
    (`map_cells::TownsOf`) walks the towns as that list does.
  - The homeless list is changed only by `ecs::town_villagers`.
  - The congregation cache is cleared by CheckWhenNewBuildingCreated (a building made within 7.5 m) through `ecs::plans`.
  - The aggression: only the record is ported (+0xEAC / +0xEB0); the per-player slots (town + n × 0x80 + 0x9F4,
    fn_0073E0F0), the 0.9 decay of +0xEB4 / +0xEB8 and the guidance SFX are not; (not ported) readers.
  - The animal list: (pending) `Town::DeleteDependancys`' SetTown(animal, 0) for each (`animal_ai::SetTown`).
  - The graveyard +0x748 is `ecs::graveyard`'s.

### Test hooks

- `OPENBLACK_TOWN_TRACE=1[,<every>][,raw]`: per town, every `<every>` turns (50) and whenever the first of order 1
  changes: `town <id> turn <t> pop <p>: [16 Sleep 1.000 raw 6.250] [15 Relaxation 0.100] …` (the 17, order 1) and `avg
  <a>` on the 50-turn turns; with `,raw` also order 2.
- `OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<town>]"`: on turn 2, SetBoost like SET_TOWN_DESIRE_BOOST (re-sorts order
  1) in all towns or in the one with that id.
- `OPENBLACK_TEST_TOWN_EMERGENCY="<town id>"`: on turn 2 `town_emergency::SetInStateOfEmergency` of that town.

## Pending

- **The town attack guidance sound** (`TownAttackSFX` 0x71B7C0) and the help spirit's creature remark
  (0x73CAAA..0x73CB29): not ported.
- **The slot's copy of the last attack's EffectValues**: no reader found yet.
- **The hitter's player**: a rock, a fragment or a felled tree answers the player it was made with (`Rock` +0x90), and
  a mobile static its holder object's player (+0x7C): openblack keeps neither, so they answer the neutral player.
  Which of the original's makers pass a player (`Reward::InterfaceTap`, `Rock::SplitInTwo`, the scripts' creation,
  `FelledTree::Create`) is still to read; the fragments' makers pass none (queued fix).
- **A Living's physics impact** runs the whole of `Object::ApplyEffect` (see [Physics](../physics/physics.md)): what is
  still missing is the creature striker's kill count and own alignment (openblack's creatures keep neither, so a
  creature's blow moves no alignment, a TODO in `effects::ApplyEffect`), and the per-player harm, which openblack
  does not keep for any effect (queued fix).
- **Artefacts not yet ported**: the impression an artefact's worth adds to what villagers see (our reactions only
  impress for miracles so far); the football pitch and the creature as relaxation; the footpaths to a moved dance; the
  creature dancing round one; the aura's particles; the comparison of the whole lane with the v1.42 exe.
- **Objects with no town in openblack**: `town_queries::GetTown` has no town for town artifacts, building sites,
  planned abodes, flocks, prayer sites and icons, spell icons, desire flags, meetings and playtime elements, so their
  harm attacks no town.
- **The empty-town countdown**: nobody starts +0xF20 yet, so `SetTownEmpty` is not called.
- **The approximations** under [openblack](#openblack) (TownStats recomputed, script homes functional, the influence
  order, the negative boost).
