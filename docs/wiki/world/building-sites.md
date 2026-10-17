# Plans and building sites

## Summary

A town in Black & White does not invent buildings. Something first lays down a **plan**: the script, a building
that was destroyed and must be repaired, a scaffold the player drops, or the rival AI. When the town wants that
building, the plan turns into a **building site**: the building stands there under construction, villagers gather
around it, bring wood to a pile beside it and build it up until it is finished and starts working. The citadel's site
is special and has six piles of wood. The **workshop** turns wood into **scaffolds**; the player carries a scaffold in
the hand, and where it is dropped it chooses which building to raise (and, for a wonder, how strong it will be). The
page also covers where a building may be placed, the buildings the scripts create directly, and the graveyard.

openblack ports the plans, the sites, the workshop and the scaffolds from the original; the citadel as a plan and a
few workshop details wait (see [Pending](#pending)).

## The original

### Plans and building sites

The life of a building, in order. Each step has its details in the sections below.

- **Plans** (PlannedMultiMapFixed, a GameThingWithPos: not drawn, Draw 0x648930 = `ret`; not in the map cells) live in
  the town's list, oldest first (Town::AddPlanned 0x73D080). The town never invents them: the script
  (CREATE_PLANNED_ABODE), destroyed buildings (repairs), scaffolds and the rival AI make them.
- **Choosing**: GetDesireToBeBuilt 0x73A1A0 per type (houses by free adult places, civic buildings once, wonder by the
  For_Wonder desire...), GetBestPlanned 0x73A140 (strictly better, the first on ties), RequestBestPlanned 0x73A650
  (mask 4, no fixed check; fields pass too), RequestANewAbode 0x73B330 (mask 2, with the fixed check
  IsSuitableForFixedAbodeInTown 0x603860: inside the nearest other town's area widened by 4 cells it is accepted with
  no other test).
- **Converting** a plan (CreatePlannedNoFixedCheck 0x405770): the abode is created under construction (+0x58 bit 1,
  percent 0, life 1.0 from the Object ctor), then its building site (BuildingSite 0x43B7E0: the 128 builder positions of
  PosBuilder 0x43AE10 around the mesh, the repair base 1.1 x life - 0.1 for a damaged built building). CHL
  BUILD_BUILDING 130 (GScript::BuildBuilding 0x6FAB30 -> ForceBuildingOfPlannedAtPos 0x73E560) converts the plan at the
  point within the same call, with the desire boost x 5.
- **Building**: BuildBy 0x52ED40 adds to the percent built; at 1 Built 0x52EBB0 / Abode::Built 0x404720 deletes the
  site (builders back to state 163, the wood pile released) and MakeFunctional 0x4047E0 runs (the town's statistics count
  the abode from then on: Add(Abode) 0x7498C0). On a built but damaged building BuildBy repairs (IncreaseLife 0x405ED0,
  Repaired 0x4047B0).
- **Drawing**: DrawBuilding 0x517F90 draws the partial model with GetPercentForDrawBuilding 0x52EFD0, nothing at
  exactly 0 % but the ground footprint stays (SetFootPrintOnTexture 0x52EA33), no static shadow until built
  (SetShadowOnTexture 0x1000), and no land haze (only fn_00801C90). The inner walls of the partly built draw are pushed
  along the L3D vertex normal (+0x14) (fn_0085C0E0 0x85C0EB..0x85C111); the rest of the partial draw is in
  [Sounds, dust and the look of impacts](../physics/physics-impacts.md#sounds-dust-and-the-look-of-impacts).
- **Graveyard**: Town +0x748; AddDead fn_00595E50 counts the dead below 50 and sets the graves stage
  max(1, ftol(n x 0.18)) & 7 on the 3D object (details under [Graveyard](#graveyard)).

#### Plans

##### The plan object

- PlannedMultiMapFixed ctor: +0x3C = creation turn (g_game +0x205A40) (0x6487F6); Town::AddPlanned puts them at the
  end, +0x9AC++ (0x73D080).
- Town::RemovePlanned (0x73D0D0) (PlannedAbode::ToBeDeleted 0x4056B0): unlinks and +0x9AC--.
- GetAbodeType: PlannedAbode (0x4061E0) = info +0x120 (PlannedTownCentre the same); PlannedTownCitadelHeart (0x467E30)
  = 0x804 (bit 2 makes it eligible with mask 4).
- IsCivic (vt +0x50C): PlannedAbode (0x4060C0) = the type is in the civic list (table 0x406118 / 0x406120);
  PlannedTownCentre (0x55DBE0) = 1; PlannedTownCitadelHeart (0x467E10) = 0 (its type 0x804 has the civic bit but
  TownStats +0x24 does not count it).
- GetDesireToBeRepaired of a plan (vt +0x514) (0x648910) = +0x30 (was built) ? info +0x118 : 0.
- The mesh of an info is its GetMesh (vt +0x2C, MeshPack 0xE9FE34); 0 for none.

##### Turning a plan into a building

- PlannedAbode::CreatePlanned(life) (0x405710) (vt +0x500): GAbodeInfo::IsOkToCreateAtPos(info, pos, angle +0x28,
  GetScale, town +0x48) (0x404B10) or null; otherwise CreatePlannedNoFixedCheck.
- CreatePlannedNoFixedCheck (0x405770): Abode::Create(pos, info, town, angle, scale, food 0, wood 0, life, 1, 1)
  (0x402E20, 0x4057AC) → MultiMapFixed ctor (0x52E1E0) with underConstruction 1 → Abode::Init (food and wood 0, no
  MakeFunctional) → CreateAbodeSurroundingObjects (0x403E00); the life argument is ignored.
- MultiMapFixed ctor 0x52E1E0(pos, info, angle, scale, percent = life, underConstruction): 1 → +0x58 bit 1 and +0x5C
  = 0 (bit 3 clear); 0 → bit 3 and +0x5C = life (0x52E20B..0x52E234).
- After creation: PostCreatePlanned (0x648C50) moves the footpath link (+0x64 ← plan +0x38) and, with the plan's town,
  CheckWhenNewBuildingCreated (0x741500); PlannedAbode only: +0x30 → building +0x58 |= 4 (0x4057C5..0x4057CC); then the
  plan is deleted.
- PlannedTownCentre::CreatePlannedNoFixedCheck (0x744550) calls TownCentre::Create (0x743C90) directly and does not do
  the +0x30 step.
- The way back, PlannedAbode::Create(Abode*) (0x405660) (from Abode::MoveAbodeToPlannedAbodes 0x40453E): a plan where
  the building is (PlannedMultiMapFixed(MultiMapFixed*) 0x648820): +0x14 pos, +0x28 GetYAngle, +0x2C GetScale, +0x40
  info, +0x30 = bit 3 of +0x58 (built) (0x6488B4..0x6488CB); always PlannedAbode (also from a centre, 0x405681); the
  footpath link moves to the plan (+0x38 = b +0x64, b +0x64 = 0); Init(GetTown) (0x4055A0) → AddPlanned; it only fails
  when allocating memory (`new 0x4C`, Abode.cpp 0x560).
- PlannedAbode::CreateNoInit(pos, 0, 0, 0, 1.0) 0x4055C0 (the scaffolds' plans): no info, angle 0, scale 1.0, in no
  town list. **🔍 inferred** the argument order (pos, info, town, angle, scale): its body is (pending), as is that of
  Init 0x4055A0, taken as "sets its town" (spec workshop_scaffold_spec.md §2.6).

<a id="the-desire-to-be-built"></a>
##### The desire to be built

- GetDesireToBeBuilt (0x73A1A0): b = info +0x114; type = the info's GetAbodeType (vt +0x40); s = building sites of
  +0x790 whose building is of that type; b == 0 → 0 (0x73A1AC..0x73A1FD).
- Constants: 0.8 (0x8C4A04), −10 (0x8C7670), 0.2 (0x8AB244), 0.6 (0x8C7BDC), 10 (0x8AB414), 0.3 (0x8AB23C); a wonder
  with 7 or more scaffolds is worth 1.0 (0x73A446).
- Cases by type: LIVING_QUARTERS 0x73A2EF (u = (adults + children) / 10 + 1 unsigned; the comparison of free places > u
  is signed, and only with no scaffolds); TOTEM 0x73A279; STORAGE_PIT 0x73A2BE (GetStoragePit); CRECHE 0x73A2E4
  (+0x744); WORKSHOP 0x73A23C (byte +0x721 = stats +0x111 = abodes of number 9); WONDER 0x73A446 (GetDesire(14) against
  GTownDesireInfo(14) +0x18); FOOTBALL 0x73A502 returns 0 at once with football off.
- Correction by abode number: m = byte stats +0x108[number]; b −= b / max(m + 1, 10) × m (0x73A3F4..0x73A43D).
- The "undivided" exits only skip the common division r / s (0x73A557); with scaffolds c = (float)(u32)(n − info
  +0x10C ScaffoldsRequired) × 0.3 × r and r −= min(c, r): with n below ScaffoldsRequired the subtraction wraps around
  and r ends up 0 (literal) (0x73A571).
- Football: GGame::FootballEnabled 0xD0196C is 0 in .data (single-player game). Its one writer is GGame::Init 0x54F53F..0x54F54F = BWCheckFeatureIsEnabled("football") 0x5259D0 (the build's feature table). It is read by
  GetDesireToBeBuilt 0x73A4DF, Town::Process 0x747415 and ValidToApplyThisToObject 0x6E9924 (the max is info +0x124 -
  (FootballEnabled ? 0 : 1)).
- GetPlannedAtPos: best = r; with no plans (+0x9AC == 0) → 0 (0x73E4C6); the radius is fn_636E30(info, scale) = scale
  × max(mesh +0x24, mesh +0x2C) (MeshPack 0xE9FE34; the citadel heart uses info +0x124, GetMesh 0x464370).

#### Building sites (BuildingSite)

<a id="building-site-details"></a>
##### The site object

- BuildingSite (0x644 bytes) / StandardBuildingSite (0x648, vtable 0x8C6DF4) of runblack.exe (spec
  dev\docs\collected\documentacion\edificios\V6_spec.md §2.1). It is kept by the town's list Town +0x790 (head first)
  and by the building's +0x74. Fields named on this page: +0x00..+0x13 GameThing (+0xA bit 0 is "being deleted"), +0x14
  the root building, +0x1C the builders' node count, +0x20 / +0x24 the scaffolds' list and its count, +0x28..+0x30 the
  scaffolds "on the way" (no caller in the exe: workshop_scaffold_spec.md §2.7), +0x634 the builder count, +0x638 the
  repair flag, +0x63C the desire boost, +0x640 the repair base, and on a StandardBuildingSite +0x644 Pot* WoodPile
  (GetPileWood 0x43D6E0 / SetPileWood 0x43D6F0). Every site is also in the global list g_game +0x205CAC (head insert 0x43B809..0x43B82D).
- MultiMapFixed::CreateBuildingSite (0x52F590) (`new 0x648`, Fixed.cpp 0x50C) → BuildingSite(MultiMapFixed*)
  (0x43B7E0): +0x638 = bit 2 of the building's +0x58 (repair site), root, building +0x74 = the site (fn_52E3F0), +0x640
  = life or 1.1 × life − 0.1 (0x8AB230, 0x8AB22C) if it has no DestructionMesh (+0x90) and is built; ring with
  fn_43CDB0 → PosBuilder::Process.
- The site's class (its vtable) by building: a workshop creates WorkshopBuildingSite (Workshop::CreateBuildingSite 0x77A6D0, `new 0x644`, vtable 0x8C6F24: no pile of its own, its pile is the workshop's +0xD0;
  workshop_scaffold_spec.md §1.7); CitadelHeart::CreateBuildingSite (0x468DC0) (`new 0x65C`) creates
  CitadelBuildingSite (ctor 0x43D1E0, then CreatePilesOfWood 0x43D2A0); the rest, worship site included,
  StandardBuildingSite (vtable 0x8C6DF4).

##### The ring of builder positions

The ring has 128 entries (PosBuilder::Process `mov edi, 0x80` 0x43AEF0, GetNearestEdge `& 0x7F`,
ArrivesAtBuildingSite `cmp eax, 0x80` 0x758B20; the bw1-decomp header's 0x7F is wrong).

- PosBuilder::Process (0x43AE10): 1) the 128 entries = the translation and the static array of best distances
  0xC58CD4[128] = 0; 3) with no mesh, x = sin a × R + c.x, z = cos a × R + c.z (a accumulated), y = GetAltitude
  (0x43AEE8..0x43AF74).
- With a mesh: each triangle of each primitive of each submesh in file order; the two vertices with the lowest LOCAL y
  (strict < 2.5, 0x8C581C); to world; u = normalize(S − L) (0x43AF77..0x43B3EA). A ray c → P2 = (c.x + R cos a, c.y,
  c.z + R sin a) with a = i × 2π/128 (0x8C6B64 = 0x3D490FDB) is cut against the edge L → S in XZ (fn_43B630,
  fn_43B4F0; inclusive XZ box fn_43B5A0); the hit's y is interpolated along the edge (0x43B2A2..0x43B316).
- 5): each entry is pushed 1 m outwards along its 3D direction from c (with no hit, from the translation)
  (0x43B3F0..0x43B4D2); ComputeRing fn_43CDB0 passes IsFootball (vt +0x464).
- GetNearestEdge (0x43CE40): angle < −6π (0x8C6CB4) → index 0; > 6π → π; normalises to [0, 2π] (2π stays); 0 → 0;
  otherwise ftol(angle × 1/2π (0x8C6CAC) × 128) & 0x7F.
- GetRandomBuildPos (0x43CDE0) (vt +0x128): a = Get3DAngleFromXZ(building, villager) + GameFloatRand(π/2) − π/4
  (BuildingSite.cpp 0x37C; ±45°).
- GetNextPosFromIndex (0x43CF40) (vt +0x124): with no building (0, 0, 0); step = 2.0 / (Get2DRadius × 2π × 0.0078125);
  k = ftol(GameFloatRand(step × 0.5) + step) (line 0x3B7), then GameRand(2).
- GetBuildPos: ArrivesAtBuildingSite reads the entry directly as (ftol(x × 6553.6), ftol(z × 6553.6), 0) only for 0 ≤
  index < 128 (0x758B12..0x758B6A).
- GetClearAreaRadius (0x43BDE0) = Get2DRadius × 1.2 (0x8C6C98); GetRadius (0x43D050) (vt +0x60).

##### Wood

- ShouldIGetWood (0x43C680): "near the building" = 50.0 (0x8C6CA4); GetDistanceModifier maximum 5000.0; m =
  GetResource(WOOD); GetResourceDropoffPos(WOOD) (0x753E20) is only requested at 0x43C78E.
- Wood pile position (GetResourcePosAndYAngle 0x43C220, vt +0x114): type not WOOD → angle 0 and the root's position;
  worship site → the local point (9, 0, −50) through the 3D object's matrix and its Y angle (0x43C24D..0x43C2CF).
- Pile with no worship site: a = Get3DAngleFromXZ(door, centre) + π/8 (0x8C6CA0) − GameFloatRand(π/4) (line 0x2B8);
  d = distance(centre, door) + 4; centre + GetPosFromAngle(a, d); Y angle = a − π (0x8C36A0): the pile ends up on the
  other side of the door (0x43C361..0x43C426).
- CreatePileWood (Standard) (0x43D760): only an available Standard site with no pile; Pot::Create(pos, GPotInfo
  0xD4D1C4 (9 "Magic Wood"), 0, building, town 0, 0, angle, 1.0, 1); the MagicWood ctor (0x600E20) ignores angle and
  scale (0x600E30..0x600E48), although the angle draw is consumed.
- GetResource (Standard) (0x43C5B0): the pile's JustGetResource or 0; GetWoodForStats (0x43C5E0) = GetResource(WOOD);
  WorkshopBuildingSite::GetWoodForStats (0x43DB90) = 0.
- AddResource (Standard) (0x43C490): WOOD only; with no pile it creates one; if available JustAddResource; returns what
  was added. RemoveResource (Standard) (0x43C530): WOOD only, JustRemoveResource(WOOD, n, 0) of GetPileWood(status ?
  status->vt +0x100 : 0).
- GetPileWood: Standard (0x43D830) = +0x644; BuildingSite (0x43D0A0) = 0.
- GetWoodValue (0x43C0C0) = (float)(u32) info +0x6C × GetScale / TribalPower[5] of the site's GetPlayer, which is
  GameThing::GetPlayer (0x570130) = the neutral player for every site.
- GetWoodNeededToBuild (0x43C5F0) = (root built ? 1 − life : 1 − percentage) × GetWoodValue − the pile's wood (caller
  IsInterestedInWoodObject 0x76501C).
- fn_73B620 (0x73B620) (from Villager::Building 0x758D71): TownStats +0xEC += wood used; also the player's GameStats
  +0xA44 +0xA8.

##### Builders, scaffolds and the building

- AddBuilder (0x43BE40): the duplicate search is useless (dead loop): node at the head, +0x1C++, +0x634++.
  RemoveBuilder (0x43BE90): each of the villager's nodes removed (+0x1C-- each) and +0x634-- once even if he was not
  there.
- AddScaffold (0x43BEF0): node at the head of +0x20, +0x24++ (the scaffold's +0x68 = site is written by the caller,
  0x43BF18); RemoveScaffold (0x43BF20): each node removed, without clearing the scaffold's +0x68.
- GetBuilding (0x43BC70): +0x14 available ? GetBuildingObject (vt +0x8BC, MultiMapFixed 0x401520 = this) : 0;
  GetRootBuilding (0x43BCA0) = +0x14; GetTown (0x43C0B0): the root's GetTown (0 for the citadel heart and a worship
  site).
- BuildBy (site) fn_43D080: GetBuilding()->BuildBy (vt +0x900).

##### Desire and progress

- GetDesireForVillagers (0x43BD70): clamp(GetBuildersNeeded / max (fn_43BBD0 = info +0x110 MaxVillagerNeededToBuild)
  + boost +0x63C, 0, 1); a maximum of 0 divides by 0 (literal). GetBuildersNeeded (0x43BC00) is signed.
- GetDesireToBeRepaired (site) (0x43BE00): with a building, GetDesireForVillagers × the ROOT's GetDesireToBeRepaired
  (vt +0x8D8); otherwise 0.
- GetPercentBuilt (site) (0x43BCB0) = GetPercentRepaired × the building's GetPercentBuilt.
- SetDesireBoost: +0x63C (fn_464F50 0x464FCE, ForceBuildingOfPlannedAtPos 0x73E560); SetRepairBase: +0x640
  (Abode::ReduceLife 0x405EA7).

##### The town's list of sites

- Town::AddBuildingSite(BuildingSite*) (0x73B910): already in the list → nothing (no pulse); otherwise node at the
  head, +0x794++ and pulse (0x73B96D..0x73B977). Town::IsBuildingHappening (0x73E2F0) = +0x794 != 0.
- Town::AddBuildingSite(MultiMapFixed*) (0x73B8E0): CreateBuildingSite (vt +0x4D4) and insertion; used by
  Abode::ReduceLife 0x405E7A, ProcessTownRepairs 0x747E75 and SetupBuildingObject 0x758565.
- Town::AddBuildingSite(PlannedMultiMapFixed*) (0x73B860): CreatePlanned(0.0) with the fixed check, then the site.
  0x73B8A0 (from Scaffold::TryToBuildPlannedBuilding 0x6E914C, plan from PlannedAbode::CreateNoInit 0x4055C0 in no
  list): CreatePlannedNoFixedCheck and the site.
- Town::RemoveBuildingSite (0x73BA20): the first site whose GetBuilding is b → ToBeDeleted(0). GetBuildingSiteInList
  (0x73CE40). IsBuildingSiteValid (0x73CF00): in the list, building != 0 and !(IsBuilt && IsRepaired).
- Town::GetBestBuildingSite (0x73CF60): from the head, score = distance(pos, the building's GetNearestEdgeToPos) ×
  (GetDesireForVillagers × 0.9 + 0.1) below 99999 (strict); the LOWEST wins (literal: it favours sites that need FEWER
  builders); a GetBuilding of 0 midway aborts with 0 (0x73CFB6).
- Town::GetBestRepairBuildingSite (0x747EA0): among the sites with +0x638, the largest GetDesireToBeRepaired > 0
  (strict; the first on a tie).
- Town::ProcessTownRepairs (0x747DE0): best = 0; plans with +0x30 by GetDesireToBeRepaired (strict, NaN never wins),
  then abodes of +0x754 with no site (+0x74) nor bit 2 of +0x58 against the SAME best; abode → +0x58 |= 4 (0x747E6E)
  and AddBuildingSite (repair site +0x638 = 1); otherwise plan → AddBuildingSite(plan) with the fixed check; at most one
  site per turn (0x747DE6..0x747E87).
- Pruning fn_43BD00 (step 2): each node (reading the next one first) with a null root, not available, or built and
  repaired → ToBeDeleted if the site is available.

##### Process and deletion

- Process (Standard) (0x43D8D0): pile +0x644 not available → +0x644 = 0.
- BuildingSite::ToBeDeleted (0x43B960): 2) out of g_game +0x205CAC; 3) out of each town of +0x205C84 that lists it
  (fn_73B990); the available builders (Villager::IsAvailable 0x751D50) to 163 moving out to Get2DRadius + 2
  (0x8AB478); 5) list emptied, +0x634 is NOT reset; 6) each scaffold of +0x20 → ToBeDeleted(0) (on a copy); 9)
  GameThing::ToBeDeleted (0x56FB70, 0x43BB97).

#### Citadel building site (six piles)

CitadelBuildingSite (0x65C bytes, vtable 0x8C6CC0) is the citadel heart's site (CitadelHeart::CreateBuildingSite
0x468DC0, ctor 0x43D1E0): a BuildingSite whose own pile (+0x644 of a StandardBuildingSite) is not used. Instead it has
six wood piles 22 m out (CreatePilesOfWood 0x43D2A0), in +0x644..+0x658 Pot* [6] (slot i at
GetResourcePosAndYAngle(WOOD, i), 0x43D470).

- Where the piles are: CitadelBuildingSite::GetResourcePosAndYAngle (0x43D470): GetWorshipSiteAngle(index) − 1.1424
  (0x8C6DEC) and GetPosFromAngle(that, 22.0) from the root; it does not write the output angle.
  Citadel::GetWorshipSiteAngle (0x463610): the heart's Y angle (+0x30, vt +0x508) + slot × 2π/7 (0x8C836C = 0.897598).
- CreatePilesOfWood (0x43D2A0): an empty "Magic Wood" in each empty or unavailable slot (Pot::Create 0x43D303).
- GetResource (0x43D320): sum of the six JustGetResource with no IsAvailable test. AddResource (0x43D360): pos NULL →
  nothing; otherwise the nearest slot (with none, CreatePilesOfWood and again), JustAddResource of any type, and
  GetTown (0) +0x710 += what was added.
- RemoveResource (0x43D3F0): with an interface, BuildingSite::RemoveResource (0x43C530) on the pile nearest to
  IS->GetPos; without it the slots in order while any remains (0x43D410..0x43D422).
- GetPileWood (0x43D500): pos NULL → 0; otherwise the nearest slot with (float)(u32) distance strictly less than FLT_MAX
  (0x8C6B50); IsLinkedToThisBuildingSite (0x43D580) = one of +0x644[0..5].
- The slots are emptied only by Process 0x43D660 and RemovePotFromStructure 0x43D5B0. CitadelBuildingSite::Process 0x43D660 does for its six slots what the Standard one does for its pile (inferred: not reached,
  CitadelHeart::Process 0x4665A0 does not call MultiMapFixed::Process).
- CitadelBuildingSite::ToBeDeleted (0x43D220): for each slot SetMultiMapFixed(0) and, with wood and without g_game +0x14
  & 0x8000, Pot::SetupReaction (0x66D660), otherwise ToBeDeleted; SetPileWood(0) is a no-op (0x43D180) (the slots keep
  the piles).
- A citadel site's town: its GetTown (0x43C0B0) is the heart's `MultiMapFixed::GetTown` (0x4220A0), which is 0, so the
  site belongs to no town.

#### Workshop

<a id="workshop-details"></a>
##### The workshop object

The Workshop class of runblack.exe (Workshop.cpp 0x779190..0x77A720, 0xE8 bytes, vtable 0x99BB20; an Abode, 0xC4
bytes, plus +0xC4..+0xE7; spec dev\docs\collected\documentacion\edificios\workshop_scaffold_spec.md §1.1). Its fields:
+0xC4 the production countdown, +0xC8 the scaffolds waiting, +0xCC the "ghost scaffold" Game3DObject drawn on a
taken-away slot (Draw 0x51CBF0 / callback 0x51CA10), +0xD0 the wood pile, +0xD4 the ShowNeedsVisuals, +0xD8 the next
workshop in the town's list, +0xDC the three slot bytes, +0xE4 its scaffolds.

- It converts 2500 wood (GScaffoldInfo +0x6C WoodValue, global 0xD959D0, GetInfo 0x779A50) into a scaffold of value 1
  every 200 town turns (info +0x1AC TimeEachMobileObjectTakesToProduce); at most 3 waiting (literal `mov edx, 3` at
  0x779B05; info +0x1A8 is not read). GScaffoldInfo::GetInfo 0x779A50 returns the single global 0xD959D0 (static
  init 0x6E8310).
- Special points of its mesh: 4 the wood pile, 6 / 7 / 8 the slots, 9 the ShowNeeds icon (GetShowNeedsPos 0x77A638), 10
  the centre of the drop-off area.
- Workshop::Create (0x779590) (`new 0xE8`, line 0x82): ctor (0x779350) all to 0 (fn_779450);
  CallVirtualFunctionsForCreation (0x7793E0): +0xCC = Game3DObject::Create(0) (0x63ABB0) and a ShowNeedsVisuals
  (0x719D60) (GShowNeedsInfo 0xD99A74, an Object with a creation index); then Abode::CallVirtualFunctionsForCreation
  (0x403200); CreatePileWood (0x7795EA) after InsertMapObject (0x7795E2), so the pile enters the cells after the
  workshop.
- Workshop::CreatePileWood (0x779630): with +0xD0 null, Pot::Create at point 4 with 0 wood and last argument 0, so
  PotStructure +0x78 = workshop and +0x80 = 0 (no SetupReaction on creation; the Standard site passes 1).
- fn_779690 (0x779690): WOOD with a 3D object and point 4 → that point (altitude above the ground kept) and its angle;
  otherwise the workshop's position and angle 0.
- Workshop::MakeFunctional (0x7797B0): after Abode::MakeFunctional, with a town the pulse and Town::AddWorkshop
  (0x740180) (at the head if not there: +0xD8, +0x74C, +0x750++). Town::RemoveWorkshop (0x7401D0) (+0x750--, +0xD8 = 0)
  from DeleteDependancys 0x779509 and RemoveStructureFromTown 0x739B12.
- Workshop::DeleteDependancys (0x7794F0): RemoveWorkshop; each of its own scaffolds loses its owner (+0x58 = 0 if
  available) and all its nodes, without touching +0xC8 or the slots (literal).
- Workshop::ToBeDeleted (0x779480): DeleteDependancys; the pile SetMultiMapFixed(0) (Pot 0x55D570) and ToBeDeleted: its
  wood is lost (no pick-up reaction); the ShowNeedsVisuals ToBeDeleted and +0xD4 = 0.

##### Making scaffolds

- Workshop::Process (0x7797F0): functional → --(+0xC4) and at 0 FinishScaffold (fn_779910); otherwise, with room and
  wood ≥ WoodValue → StartScaffold (fn_7798A0); then only the head scaffold (Scaffold +0x98 is only written by the
  ctor, to 0) is released if it is no longer adjustable (fn_6EAA40 → RemoveFromWorkshop 0x6E9200); ShowNeeds +0x5C =
  GetVisualWoodDesire with no null test (fn_719E80); at the end Abode::Process (the workshop's site has an empty
  Process 0x43DBA0).
- StartScaffold fn_7798A0: RemoveResource(WOOD, WoodValue) (from the pile and the mirror), +0xC4 = ftol(200.0),
  SoundTag::Create(this, 0x4A, 0, 2, −1, 0, 1, 1, 0) (0x71E840) (work loop of the InGame bank).
- FinishScaffold fn_779910: first free slot (fn_779610); Scaffold::Create(pos of the point, GScaffoldInfo, GetTown,
  this, the point's Y angle, 1.0) (fn_6E8460); SetScaffoldValue(1) (0x6E9D40); AddScaffold; the scaffold's slot bits;
  slot byte = 2; G_ScaffoldReady 150 (0x779A03) at the workshop; always +0xC4 = 0 and SoundTag::Remove(this, 0x4A, 1)
  (0x71EBE0).
- GetSpaceInStore (0x779AF0) = 3 − (+0xC4 != 0) − +0xC8 (signed).
- GetVisualWoodDesire (0x779B90) = 1 − min((wood + 0.0001) / (WoodValue + 0.0001), 1) (0x8BF518 = 0.0001);
  GetDesireToBeSupplied (0x779B60) = room != 0 or GetVisualWoodDesire > 0 ? 1 : 0.
- fn_779B20 (tooltip over the workshop, GInterface::CalculateOverObject 0x5CA1E8): room > 0 ? WoodValue × room − wood
  (uint32 that wraps around, literal) : 0.

##### Wood in and out

- Workshop::AddResource (0x779E00): n == 0 → 0; pulse if it had none of that type; WOOD: with no pile it creates one,
  available pile → JustAddResource; then DoResourceAdding with status 0 (only JustAddResource 0x404D40: +0xBC[type] +=
  added; no desire, alignment, belief or mimicry).
- Workshop::RemoveResource (0x779EC0): WOOD with a pile → JustRemoveResource and DoResourceRemoving(WOOD, r, status);
  otherwise DoResourceRemoving(type, n); there is no redirection to a site (Abode's 0x404F1A is not called).
- PotStructure::RemoveResource (0x66EE10) on the workshop's pile: over = CalulateAmountOverMaximum (MultiMapFixed
  0x4015D0 = 0); r = JustRemoveResource (the pile stays even if emptied, 0x66D9D3); r != 0 → the workshop's
  DoResourceRemoving; r < n → r += the workshop's RemoveResource(n − r).
- Workshop::IsResourceStore (0x77A650) = MultiMapFixed 0x52F1F0 (WOOD with a site) || WOOD || ANY (−2).
  Workshop::RemovePotFromStructure (0x779750): DoResourceRemoving(the pile's type and amount) and +0xD0 = 0, then
  MultiMapFixed::RemovePotFromStructure (0x52F160).
- WorkshopBuildingSite: AddResource (0x43DB20) and RemoveResource (0x43DB60) go to the root (Workshop 0x779E00 /
  0x779EC0); GetPileWood (0x43D9B0) = root workshop ? +0xD0 : 0; GetResourcePosAndYAngle (0x43DA80); CreatePileWood
  (0x43D9F0) creates a pile whose +0x78 is the workshop and then SetPileWood (no-op 0x43D9E0) leaves +0xD0 null.
- Town::GetBestWorkshop(pos, useDesire, onlyFunctional) (0x740250): best = 0; v = GetDistanceModifier(distance, 500) ×
  (useDesire ? GetDesireToBeSupplied : 1), strictly greater and (not onlyFunctional or functional); callers
  CheckSatisfySupplyWorkshop 0x7593BE, ArrivesAtStoragePitForWorkshopMaterials 0x7594A0 (state 211) and
  ArrivesAtWorkshopForDropOff 0x759542 (state 210), all (pos, 1, 1).

##### Slots and snapping

- The workshop slot of a scaffold is its +0x88 bits 6-7: (+0x88 >> 6) & 3 (0x779CDD..0x779CE8, 0x77A616..0x77A61F,
  0x6EA910); 0 for anything that is not a scaffold. It is written as +0x88 = (+0x88 & 0xFF3F) | (slot & 3) << 6
  (0x779981..0x7799A5, 0x77A4E9..0x77A503, the split 0x6EA030..0x6EA058).
- IsPosWithinScaffoldAreas (0x77A3A0): distance ≤ 50 (0x8C6CA4) and (IsCloseToEP(10, p, 10) | IsCloseToEP(6 / 7 / 8, p,
  5)), all four evaluated. IsCloseToEP (0x77A420): distance to the point < r (with no point, (0, 0, 0)).
- Town::IsScaffoldAwayFromWorkshops (0x740300) (ProcessInHand 0x6E874C, ChoosePlan 0x6E8D34, TryToBuildPlannedBuilding
  0x6E909B); Town::CheckScaffoldSnapToPoint (0x740340) (Scaffold::EndPhysics 0x6E87D1): CheckSnapToPoint of all
  workshops, results ORed.
- CheckSnapToPoint (0x77A480): within 2 m (0x40000000) of point 6, 7 or 8 (all three evaluated); from another owner
  with no room → nothing; otherwise RemoveFromWorkshop, AddScaffold, owner = this, slot = first free & 3; then snapping
  fn_77A530. It reads and writes the scaffold's owner (MobileObject +0x58): Workshop::CheckSnapToPoint 0x77A4C0..0x77A4F6, as
  DeleteDependancys does at 0x77950E..0x779573.
- Snapping fn_77A530: the slot's point (0 → 6, 1 → 7, 2 and 3 → 8); RemoveMapObject, altitude 0, SetPos, SetXYZAngles
  with the point's GetYXZ angles (the Transform's scale is kept), InsertMapObject, slot byte = 2; with no point (0, 0)
  and angles 0.
- AddScaffold fn_779C10: node at the head, +0xE4++, +0xC8++ always (no duplicate test). RemoveScaffold fn_779C60 (only
  from Scaffold::RemoveFromWorkshop 0x6E923E): if it is there, nodes removed, +0xC8--, slot = 0; with +0xC8 == 2 and a
  town, the pulse.
- fn_779610: first slot with byte 0, otherwise 0 (literal: 0 even if occupied); fn_779A60: point 6 + slot (any other
  value is passed as-is), x / z and altitude 0; fn_779D30: slot < 3 → 0; ScaffoldMoved (0x77A610)
  (InterfaceSetInMagicHand 0x6E975A, InitialisePhysics 0x6EA87D): slot = 1 (reserved);
  Scaffold::CanBecomeAPhysicsObject (0x6EA910) reads +0xDC + slot == 2.
- The byte +0xDF (padding after the three slots) receives the write of a slot value of 3 (0x779CEB / 0x77A5FD /
  0x77A622).

##### The needs sign

- ShowNeeds.cpp: ShowNeedsVisuals 0x719D60..0x71A230, GShowNeedsInfo 0xD99738, 4 rows of 0x114 bytes; the workshop's
  row is 3, 0xD99A74, pushed by Workshop::CallVirtualFunctionsForCreation 0x779420. The ShowNeedsVisuals is an Object
  (the next creation index) that copies its owner's MapCoords as they are (Object::Object 0x636547..0x63658D);
  CallVirtualFunctionsForCreation 0x719E00 gives it the info's mesh, +0x54 = 0 and SetScale(1); SetDesire fn_00719E80
  writes +0x5C; ShowNeedsVisuals::ToBeDeleted 0x719DD0 runs from its owner's ToBeDeleted.
- The sign is drawn by Workshop::Draw (0x51CC41..0x51CC67) only while the workshop is functional and not on fire
  (FireEffect::IsOnFire 0x730360) → fn_00719E90: with the desire not above ShowNeedGreater (+0x10C) the height Zoomer is
  set to the desire and nothing is drawn (0x719EB0..0x719EC1, 0x71A16C..0x71A19F); otherwise, every frame,
  SetDestinationWithSpeedAndTime(min(desire, MaxNeedValue) / MaxNeedValue, 0, 1.0) (fn_0071A200, 0x719ED0..0x719EEB),
  the Zoomer advanced inline by the frame ms × 0.001 (0x719EF0..0x719F77, its own sum order, not Zoomer::Update
  0x442720's), height = value × MaxHieght (+0x110) (0x719F7E..0x719F90).
- Where the sign stands: the owner's GetShowNeedsPos (vt +0xE4; Workshop 0x77A630 = GetSpecialPos(9) 0x63B060) with
  the height as its altitude (GetAltitude 0x803090 is computed and dropped, 0x719FBF), turned to face the camera
  (atan2(dz, dx) + π/2, 0x8C78D8, 0x719FC1..0x71A02A), LH3DObject::SetPosition(point, yaw, 1.0) 0x423140 (0x71A051).
  Below L = (MaxNeedValue − ShowNeedGreater) × 0.2 the 3×3 is scaled for the draw only by (value − ShowNeedGreater) /
  (L − ShowNeedGreater) (0x71A056..0x71A0E4, restored at 0x71A116).

#### Scaffolds: choosing the building and the wonder's power

##### The building a scaffold chooses

- Town::GetNewPlannedBuilding (0x73D980): best = 0; abode numbers 0..15 (or only `limit`); the scale is reloaded each
  iteration; tribe = GetTribe +0x10, for the WONDER (10) the scaffold's if not −1 and scale WonderScale(pos)
  (fn_73DC40); info GAbodeInfo::Find; skipped if ScaffoldsRequired is 0 or greater than n (unsigned) or, without force,
  if the position does not suit its mesh (IsSuitableForFixed 0x603DC0, 0x73DA34); the strictly greater
  GetDesireToBeBuilt wins. It does not write the scaffold's +0x14 MapCoords.
- fn_73D8D0 uses GameFloatRand(2π) (Town.cpp 0xB31) and π/4 (0x8C6C9C).

##### The wonder's power

- Town::GetWonderPower (0x73DAF0): raw = GetRawDesire(14); r = 15 × raw if > 15, otherwise 15 (also NaN); sums
  GetTownArtifactValue (vt +0xC8) + GetImpressiveValue (vt +0x66C) × 0.1 of the objects (fixed and then mobile) at
  distance < r; result ≥ 0.25 (0x8AB3D4); constants 15.0 (0x8C2C40) and 0.1 (0x8AC404).
- Literal in GetWonderPower: GetFirstIterator (0x6034D0) is always called on `pos` (0x73DB67) and not on the copy the
  spiral moves, so the same cell is walked n times (0x73DBF7..0x73DC04).

<a id="wonder-details"></a>
##### The wonder object

- The Wonder class of runblack.exe (Wonder.cpp, 0xC8 bytes, vtable 0x8DF96C; an Abode, 0xC4 bytes, plus +0xC4; spec
  dev\docs\collected\documentacion\edificios\workshop_scaffold_spec.md §3).
- Wonder::Create (0x778E80) (`new 0xC8`): ctor 0x778E00 (SetToZero 0x779060), SetPower(fifth argument, inferred: the
  scale) (0x778ECB..0x778ED0), CallVirtualFunctionsForCreation (0x779160), built → AddToPlayer (0x778EE1..0x778EF3): a
  script wonder counts at once, a plan one from Built.
- +0xC4 power: SetToZero 0x779060 (the ctor 0x778E00) = 0; Wonder::SetPower (0x779070, vt +0x8C8) = +0xC4 (Create
  0x778ED0 passes the script's scale, Scaffold::TryToBuildPlannedBuilding passes Town::GetWonderPower); GetPower inline
  fn_779030. Saved after the Abode's (Save 0x779080 / Load 0x7790F0).
- AddToPlayer (0x778FC0): with a player, tribe (GetTribe 0x779040 = &GTribeInfo[info +0x158]) and built, GPlayer
  +0xB8[tribe] += power; RemoveFromPlayer (0x778F50) subtracts it.
- Wonder::Built (0x778F30) = Abode::Built and then AddToPlayer; DeleteDependancys (0x778E60) (ToBeDeleted 0x778E40) =
  RemoveFromPlayer and Abode::DeleteDependancys; MakeFunctional 0x778F20 = `jmp Abode::MakeFunctional`.

##### The wonder's model and its draw

- The wonder's mesh is its info row's, as for every abode: Wonder has no GetMesh of its own (vtable +0x608 is
  Abode::GetMesh 0x404AA0 = info vt +0x2C), and GAbodeInfo::GetMesh 0x401240 returns the row's +0x15C (the mesh,
  right after the tribe +0x158), an index into the meshes of `Data\AllMeshes.g3d`. The rows in `info.dat` (each tribe's
  ABODE_WONDER, abode number 10): Celtic 91, African 122, Aztec 72, Japanese 146, Indian 59, Egyptian 122, Greek 136,
  Norse 217, Tibetan 248 (the African row uses the same mesh as the Egyptian one). Wonder::Get3DType 0x779190 morphs
  with the land for Celtic, Japanese, Indian, Norse and Tibetan, by the jump table at 0x7791B0.
- Every wonder model (like every house) carries a wooden scaffold as a status-1 sub-mesh beside its status-0 model
  and a physics sub-mesh: mesh 91 has sub-mesh 1 (status 1, a TexturedChroma ring of 126 triangles up to 10.8 units
  high, wider than the model), 2 (physics) and 3 (status 0, the stone ring, 420 triangles, 7.5 high); the other
  wonders split the same way (packtool + l3dtool on the frozen data). The whole draw shows only status 0 (the scaffold
  never shows on a built wonder); the partly built draw (`DrawPartialyBuilt`, [Physics](../physics/physics.md)) shows status 0
  cut at the build plane and the highest status as the scaffold, risen from the ground below 20 %, whole up to 80 %,
  then cut from the top. Nothing is drawn at 0 % (`DrawBuilding` 0x517FE0) and a planned abode draws nothing
  (PlannedMultiMapFixed::Draw is empty). So a wonder site early in its build shows mostly the wooden scaffold: that is
  the original's draw, not a wrong mesh.
- The heights the Celtic wonder's draw is cut at (mesh 91, scale 1): the cut's "half" is the mesh's bounding box
  size.y (`LH3DMesh` +0x28), which `LH3DMesh::ComputeBoundingBox` (0x8081B0) makes half the y extent of every vertex of
  every sub-mesh, scaffold and physics included (`LH3DSubMesh::ComputeBoundingBox` 0x87FB20 has no filter), so
  (10.797 + 0.002)/2 = 5.3999. The stone is cut at 10.8·pct and is 7.45 high, so it stands whole from pct ≈ 0.69;
  above 0.8 the scaffold is cut at 10.8·(1 − 5·(pct − 0.8)), 5.40 at 0.9. The scaffold (read from the frozen data) is
  an outer cage at most 4.73 high (radius 14–17), spars over the inner stones up to 6.3 and a middle mast to 10.8, so
  at 0.9 only the mast and the inner spars go and the outer cage stays whole: the testbed's
  `examples.wonder_built_90` shot is the original's draw.

##### Script commands on a scaffold

- `SET_SCAFFOLD_PROPERTIES` (`GScript::SetScaffoldProperties` 0x6FEAB0): POP destroy (ebx), size (a float), type
  (stored raw), the object; none → "Object no longer valid!" (0xC0DA90); not IsScaffold (vt +0x49C) → "Thing must be
  scaffold" (0xC0DA78) and nothing (the next test 0x6FEB4A fails). Then (0x6FEB54..0x6FEB82): +0x90 = type,
  `SetScaffoldValue(ftol(size))`, flags bit 0x800 = destroy & 1 (0x6FEB74..0x6FEB82).
- `SET_ACTIVE` on a scaffold (vt +0x49C, 0x6FD7A1..0x6FD7B7): active → `ForceBuildBuilding(0)` 0x6E8860; inactive:
  nothing. ForceBuildBuilding(player) 0x6E8860: BuildBuilding(player, 1) -> SetPhantomMeshAndPosition and +0x94 = turn
  (callers Abode::DestroyedByEffect 0x404031, GScript::SetActive 0x6FD7B7).

#### Citadel plan

- CREATE_PLANNED_CITADEL (handler 0x715E91) → PlannedTownCitadelHeart ctor 0x467DD0(pos, heartInfo, town, angle,
  scale): PlannedMultiMapFixed ctor 0x648780 (+0x14 pos, +0x28 angle, +0x2C scale, +0x30 = 0, +0x38 = 0, +0x3C the
  creation turn, +0x40 info, +0x44 = 0), vtable 0x8C9D4C, +0x48 = town; with a town Town::AddPlanned 0x73D080, at the
  tail of the town's list. It is only a plan: not drawn, not in the map cells, no land flattening, no creation index.
  `heartInfo` is the script's N3: 0xC5E270 + N3 x 0x158.
- PlannedTownCitadelHeart::CreatePlanned 0x467EA0 (vt +0x500): fn_4695E0(info, &+0x14, +0x28 angle, GetScale vt
  +0x120) 0x467EC8 = MapCoords::IsSuitableForFixed(info vt +0x2C GetMesh 0x464370 (= info +0x124), angle, scale)
  0x6038B0 != 0 (`neg; sbb; neg`), the heart info's IsOkToCreateAtPos; null if not, otherwise vt +0x504 (0x467EDA).
- PlannedTownCitadelHeart::CreatePlannedNoFixedCheck 0x467EF0 (vt +0x504), returning the heart or null:
  1. player = +0x48->GetPlayer (town vt +0x1C, 0x467EFB; +0x48 used without a NULL check, 0x467EF5); none -> 0
     (0x467F02), and the plan survives.
  2. citadel = player +0xA48 (0x467F08); none -> `new 0x80` (CitadelHeart.cpp line 0x714) Citadel ctor
     0x462B10(&+0x14, GCitadelInfo 0xC5E1E8, player): made with the heart below.
  3. CitadelHeart::Create(&+0x14, +0x40, citadel, +0x28 angle, GetScale, life, 1) 0x464E20 (0x467F72), under
     construction at 0 %; none -> 0.
  4. heart +0x94 = +0x48, the plan's town (0x467F83).
  5. +0x30 -> heart +0x58 |= 4 (0x467F90: a rebuild plan's site is a repair site).
  6. PostCreatePlanned 0x648C50 (0x467F99): heart +0x64 = plan +0x38 (footpaths); plan->GetTown() (GameThing
     0x56FF10 = 0): no Town::CheckWhenNewBuildingCreated.
  7. plan->ToBeDeleted(0) (vt +0xC 0x467E80 -> Town::RemovePlanned, 0x467FA5).
- CitadelHeart::Create 0x464E20(pos, info, citadel, angle, scale, life, underConstruction): the heart (ctor 0x4649B0
  over CitadelPart 0x4693F0 over MultiMapFixed 0x52E1E0), its CallVirtualFunctionsForCreation 0x4675A0 and, at
  life >= 1 (fcomp 1.0; test ah, 1; jne), fn_464F50(citadel, 0): CREATE_CITADEL; never in the conversion of a plan.
  With `citadel` null the player's citadel is made with this heart.
- CitadelPart ctor 0x4693F0: +0x80 = citadel (0x469443), SetLife(info +0x128 StartLife) (0x469452), +0x7C = info
  +0x11C, the part at the head of citadel +0x4C. Citadel ctor 0x462B10 when the player has none (from 0x467F3D); the
  heart ctor 0x4649F9..0x464A4D makes it the citadel's heart (+0x30) when it has none, and +0x6C its influence
  (InfluenceSources' CitadelInfluence). Heart +0x94 = the plan's town; InsertMapObject (0x46776C) puts it in the map
  cells.

#### Placement and town area

- Town::SetTownArea (0x73AAF0): min = 0x7FFFFFFF, max = 0 (0x73AB00..0x73AB1B); fn_73AC90 for each abode (+0x754) and
  each field (+0x780); only called by AddStructureToTown (0x739A0A) and RemoveStructureFromTown (0x739BB6).
- fn_73AC90 (0x73AC90): the object's GetRadius (vt +0x60: Object 0x638110 → Get2DRadius) around its +0x14 in metres (x
  × 10 × 2⁻¹⁶, no height); below the minimum → min = ftol(v × 65536 / 10); z with +0x72C / +0x738.
- The Abode ctor calls AddStructureToTown (0x4013BB → 0x7399A0) before its 3D object exists (+0x40 = 0, 0x63657B), so
  Get2DRadius (Object 0x638180) gives 0 (0x6381E9): the newcomer counts as a point.
- The Field ctor puts the field at the head of +0x780 and calls fn_0073AC90 with its own radius 5.0 (Get2DRadius
  0x528E80) (0x527E64..0x527E82); it stores the GFieldTypeInfo in +0x120 (0x527E23) and GameRand(10) in +0x11C
  (0x527E90).
- SetTownArea also computes a camera point at +0x598 from +0x740 (or the storage pit, or the centre of the rectangle)
  with GCamera::SetPointFromPointDistanceHeadingAndPitch (0x442810); only Town::Save / Load read it (0x73ED70 /
  0x73F490) (0x73AB5C..0x73AC7F).
- fn_0073AE10 (0x73AE10): centre of the rectangle x = ftol((maxX × 10 / 65536 + minX × 10 / 65536) × 0.5 × 65536 /
  10), z with +0x738 / +0x72C, altitude 0 (constants 0x999AA0, 0x8AC41C, 0x8AA3B4, 0x8AC408).
- Town::AsssignTownFeature (0x73EAC0) (at the end of GSetup::LoadMapFeatures 0x71813D): MakeScenicForest (0x741B40)
  for each town (newest first) at the centre of the rectangle (0x741B8C), and in a second pass AssignForestsToTown
  (0x73EB00).
- MapCoords::IsSuitableForFixed (0x6038B0) → fn_604020: uses the static Game3DObject g_tmp (0xD38330) with mesh, angle
  and scale; with no mesh 0 (0x603B10); walks the cells of the NewCollideDescriptor (GetNext 0x46AD80 stops at a cell
  outside the map and ends with 1).
- In each cell: the POSITION's cell in 0..0x1FF, with a block and not water (lc +6 & 0x10); no MultiMapFixed of the
  fixed list (+0x24 & 2 and IsSolidToNewAbode vt +0x7D8) with d < R_obj + scale × max(+0x24, +0x2C) (strict)
  (0x6040A9..0x604170).
- The variant (Game3DObject*, angle, scale, under) (0x603DC0) also rejects the coastline (lc +6 & 0x20,
  MapCoords::IsCoastal 0x6036A0) (0x603F16..0x603F75) and does not count `under` as an obstacle (0x603F96); it is used
  by the scaffolds (0x6E8E37, 0x6E873C, 0x6E9118) and GetNewPlannedBuilding (0x73DA34).
- IsSuitableForFixedAbodeInTown only acts with a town (0x603865); GetNearestTown(&near, &dist, excluded = town, tribe =
  −1) by octagonal cell distance (0x60387B); code 1 → 0xE, which IsOkToCreateAtPos (0x404B10) turns into "accepted"
  (`neg; sbb; neg` 0x404B2E..0x404B32).

#### Script buildings

Complements "Converting" above.

- Abode::Init (0x403130) does the MakeFunctional of a whole abode (the script one); a plan one does it in Built.
- StoragePit::MakeFunctional (0x732F30) → SetStoragePit: the last one wins; Creche::MakeFunctional (0x50AB50): +0x744 =
  this only if null (the first one wins, 0x50AB72).
- Abode::Create (0x402E20) calls CreateAbodeSurroundingObjects (0x403E00) last (0x40308E, after Init 0x403087): the
  "did you know?" sign and the lantern.
- Abode::GetNewEp 0x403590 takes the first entrance point of a type from the NewEP block (flag ContainsNewEP; ABODE_EPP
  type and position, in block order).
- TownCentre::MakeFunctional (0x743E80): the town's +0x9A4 = this if still null (like CREATE_TOWN_CENTRE 0x71577C)
  (0x743EAB), and CreateTotemIfNecessary (0x743DA0) → TotemStatue::Create (0x737CC0).
- Totem: the tribe's pedestal (GTotemStatueInfo, table 0xDA1D18 by Abode::GetTribeType) at the centre's special point 6
  (TownCentre::GetTotemPos 0x743F20, through its full matrix), with its Y angle and scale (0x737B20) and the icon on
  top; it rises with the morphed centre: (H(p) − H(pos)) + p.y (GetExtraPos 0x80FF20, IsStaticMorphable 0x81002E,
  0x8100B7..0x8100D0); it faces the worship site (AddToPlayer 0x738130) and carries the creature's icon
  (SetPlayersCreature 0x7381C0).
- The totem's life is its town centre's: `ReduceLife` 0x737C90 and `IncreaseLife` 0x737C60 pass the call on to the
  centre (+0x7C, vt +0x5B8 / +0x5BC, every argument), and only without a centre act on the totem itself
  (MultiMapFixed's 0x52F5E0, Object's 0x637870). It takes no effect: `IsEffectReceiver` 0x561130 = 0.

#### Graveyard

- Graveyard.cpp 0x595CB0..0x595FB0; the dead in +0xC4 are saved by Graveyard::Save (0x595EE0).
- Town::SetGraveyard fn_0073D690: +0x748 = g only if +0x748 is null or g is null: the first functional one stays
  (0x73D69C..0x73D6A2).
- AddDead fn_00595E50: requires a town and IsFunctional and (float) +0xC4 < 50 (0x8C6CA4); the 0.18 factor is at
  0x9003E0; the stage goes to LH3DObject +4 bits 21..23 through vt +0x1D8 (fn_007F9C40), so stage 8 shows as 0
  (0x595E58..0x595EC5).
- Graveyard::MakeFunctional (0x595E00): after Abode::MakeFunctional, with a town without a graveyard SetGraveyard(this)
  and AddDead (literal: it counts one dead) (0x595E0C..0x595E3A).
- Graveyard::DeleteDependancys (0x595CE0) (from ToBeDeleted 0x595CB0): if the town has it as graveyard, it searches
  +0x754 for the first other functional abode whose ABODE_TYPE has bit 2 or bit 9 (`test 0x204`, so any civic one,
  e.g. the storage pit, passes) and does SetGraveyard(0), SetGraveyard(found or 0); then Abode::DeleteDependancys
  (0x403F00) (0x595D05..0x595D59).

### Scaffold details

#### The scaffold object

Scaffold (MobileObject, 0x9C bytes, vtable 0x8E552C, Scaffold.cpp of runblack.exe), its own part +0x58..+0x98 (spec
dev\docs\collected\documentacion\edificios\workshop_scaffold_spec.md §2.1, §4.1). Its fields:

| Offset | What it holds |
|---|---|
| +0x14 | MapCoords: the scaffold's position at CreateNoInit and at TryToBuildPlannedBuilding's SetPos 0x6E9137 |
| +0x50 | the object's scale, which is the scaffold's value (how many scaffolds it stands for) |
| +0x58 | MobileObject::object: the owner Workshop (RemoveFromWorkshop 0x6E9200 dynamic_casts it), or null |
| +0x68 | the BuildingSite it belongs to (BuildingSite::AddScaffold 0x43BF18; cleared 0x6E83F1, 0x6E9636), or null |
| +0x6C | the current plan (what the phantom shows; SetPlannedAbode 0x6EA9F0), or a null pointer |
| +0x70 | the newest candidate plan |
| +0x74 | Game3DObject (type 1, CallVirtualFunctionsForCreation 0x6E855D): the phantom building; its mesh is set by vt +0xF4 SetMesh in SetPlannedAbode / SetPhantomMeshAndPosition and read by vt +0xF8 in ProcessInHand 0x6E871B; 0 = no mesh. Drawn by Scaffold::Draw 0x6EA5C0 |
| +0x78 | the cross-fade timer |
| +0x7C | the GPlayer who holds / dropped it (InterfaceSetInMagicHand 0x6E9704; 0 at EndPhysics 0x6E8827) |
| +0x80 | the phantom's rotation |
| +0x84 | the plan's scale shown |
| +0x88 | the flags (uint16, below) |
| +0x8C | Town* (GetTown 0x55E120): Create's argument 3, GetCombinedTown fn_6E8F50 |
| +0x90 | the type (SET_SCAFFOLD_PROPERTIES; −1 for a new one, fn_6E8500) |
| +0x94 | the game turn when it was placed (EndPhysics 0x6E883A, ForceBuildBuilding 0x6E8886); 0 while held (InterfaceSetInMagicHand 0x6E9707) and for a new one (fn_6E8500) |
| +0x98 | 0, written only by the ctor 0x6E83B3; Workshop::Process reads it as "next" |

- The value: fn_6E9D10 = ftol((GetScale() - 0.5f) * 5.0f + 0.5f) ([0x8AA3B4] = 0.5, [0x8DAD58] = 5.0), the scaffolds
  it stands for; SetScaffoldValue(n) 0x6E9D40 = SetScale(float(n) * 0.2f + 0.5f) ([0x8AB244] = 0.2).
- The +0x88 flags:
  - bit 0: the cross-fade phase (UpdatePhantomBuildingPointers 0x6E8B20, ProcessInHand step 3);
  - bit 1: the hover test was done this hand turn (ProcessInHand 0x6E86CF / 0x6E86DE);
  - bits 2..5: the "no building here" feedback counter (ProcessInHand 0x6E8581..0x6E8595 and 0x6E86C0);
  - bits 6-7: the workshop slot (see [Slots and snapping](#slots-and-snapping));
  - bit 8: the plan founds a new town (ChoosePlan fn_6E8D20 town-less branch; BuildBuilding 0x6E92A2);
  - bit 9: no phantom rotation (ProcessInHand 0x6E85A3, 0x6E8639). (pending) its setter: no write found in
    Scaffold.cpp;
  - bit 10: dropped gently (InitialisePhysicsFromHand 0x6EA8D2..0x6EA903: PhysicsObject::Landed && !dontReplant);
    cleared by EndPhysics 0x6E8840;
  - bit 11: SET_SCAFFOLD_PROPERTIES' destroy (0x6FEB74..0x6FEB82): DestroyThingsInWay when it leaves the hand, and the
    `force` of BuildBuilding / ChoosePlanForScaffold ((flags >> 11) & 1).
- The cross-fade timer starts at 0x190 (ProcessInHand 0x6E8654, UpdatePhantomBuildingPointers) and steps by 0x64 per
  hand turn (0x6E85E5).
- Constants: [0x8AB210] = 0x40C90FDB (2 pi): ProcessInHand's phantom rotation wrap (0x6E85BF); [0x8C78D8] = 0x3FC90FDB
  (pi / 2): the tap split's quarter turns (0x6E9EFA), GameRand(4) (Scaffold.cpp line 0x38C); [0x8C6C9C] = 0x3F490FDB
  (pi / 4): ChoosePlan's town-centre angles (0x6E8E48).

#### Creation

- Scaffold::Create(pos, info, town, owner, yAngle, scale) fn_6E8460: `new 0x9C` (Scaffold.cpp line 0x42), the ctor
  fn_6E8390 (MobileObject(pos, info, owner, yAngle, scale) 0x606E40 with the owner workshop as its object +0x58,
  fn_6E8500: +0x90 = -1), +0x8C = town, CallVirtualFunctionsForCreation 0x6E8540 (the map cells; the phantom +0x74).
  The info is always the global GScaffoldInfo 0xD959D0. Registers the class's tap handler once (RegisterTapHandler).
- CallVirtualFunctionsForCreation 0x6E8540, after MobileObject's (InsertMapObject vt +0x544): +0x74 =
  Game3DObject::Create(1) unless being deleted (the phantom: Scaffold::phantomMesh 0);
  SetPlannedAbode(PlannedAbode::CreateNoInit(pos, 0, 0, 0, 1.0)) 0x4055C0; +0x70 = CreateNoInit(pos, 0, 0, 0, 1.0).
  The two plans are in no town list.
- SetPlannedAbode(p) 0x6EA9F0: +0x6C = p; HasPlan(0) -> the phantom's mesh = the plan info's (vt +0xF4; an out-of-range
  mesh -> 0).
- SwapPlannedAbodes 0x6E8C60: t = +0x6C; SetPlannedAbode(+0x70); +0x70 = t.
- SetPlannedInvalid(k) 0x6EA9B0: k == 0 -> +0x6C info = 0; always +0x70 info = 0. The original writes through a null
  pointer.
- GetTown 0x55E120: +0x8C.

#### In the hand

- InterfaceSetInMagicHand(status) 0x6E96E0, after Object::InterfaceSetInMagicHand 0x637660 succeeded: +0x7C =
  status->GetPlayer(), the holder; +0x94 = 0; the old site cancelled without a rebuild plan (RemoveOldBuildingSite
  between Set/GetShouldNotBeAddedToPlanned); the owner workshop's ScaffoldMoved (slot = 1).
- RemoveOldBuildingSite 0x6E9610: site -> site's RemoveScaffold 0x43BF20, site->GetBuilding()->ToBeDeleted(0), +0x68
  = 0; a missing or unavailable +0x6C / +0x70 -> a new CreateNoInit(pos, 0, 0, 0, 1.0).
- ProcessInHand 0x6E8570, every turn while held: the feedback counter, the phantom's rotation, the cross-fade and the
  plan choice. Returns 1 (the scaffold stays in the hand), always.
  1. 0x6E8573..0x6E8595: flags & 0x3C -> the bits 2..5 of ((flags & 0xFFFC) + 4) (the counter +1, 15 wraps to 0).
  2. 0x6E859C..0x6E85E0: +0x80 += (flags & 0x200) ? 0 : info +0x114; above 2 pi (`test ah, 0x41; jne` keeps <=) -= 2
     pi.
  3. 0x6E85E2..0x6E8654: +0x78 -= min(+0x78, 100) (signed: a negative one becomes 0); <= 0 and !(flags & 1): Swap;
     SetPlannedInvalid(1); HasPlan(0) -> +0x84 = its GetScale, !(flags & 0x200) -> +0x80 = its +0x28; flags |= 1;
     +0x78 = 400.
  4. GetInterfaceStatusHoldingThis 0x63A1B0: what ProcessInHand reads of the interface status holding it.
     GInterfaceStatus +0xA0 != 0 (0x6E8662, `test; jbe`). (pending) its meaning (inferred: the turns the hand has been
     still / over the land).
  5. 0x6E86DE: flags &= ~2; town = GetTownForBuilding().
  The SPOT_VISUAL 4 is the "no building here" puff (ProcessInHand 0x6E86AC, EndPhysics).
- UpdatePhantomBuildingPointers 0x6E8B20: the cross-fade between +0x6C (shown) and +0x70 (the newest candidate)
  (approximate semantics, literal steps): a = HasPlan(0), b = HasPlan(1).
  - 0x6E8BC6 (the fade): flags & 1 -> clear it, +0x78 = 400 - +0x78; else +0x78 < 1 (`cmp 1; jge` returns) -> flags |=
    1, +0x78 = 400.
  - a && b, the same mesh (info vt +0x2C, 0x6E8B57..0x6E8B6B): Swap; a WONDER (vt +0x44) -> +0x84 = its scale;
    SetPlannedInvalid(1).
  - a && b, a different mesh (0x6E8BA9): the +0x70 number != TOWN_CENTRE -> flags &= ~0x100 (0x6E8BBD); then the fade.
  - a && !b (0x6E8B40 je 0x6E8BBD): flags &= ~0x100; then the fade.
  - !a && b (0x6E8C09): +0x78 = 400; Swap; +0x84 = the plan's scale; SetPlannedInvalid(1); flags |= 1.
  - !a && !b (0x6E8C3F): Swap; SetPlannedInvalid(1).
- ChoosePlan fn_6E8D20: town && !town->IsScaffoldAwayFromWorkshops(this) 0x740300 -> 0; 0x6E8DDA: a town-less
  scaffold (CHL CREATE) never founds a town. The mesh = fn_73DAA0(info) (a static Game3DObject 0xDA2914 with the
  info's mesh); 8 angles k x pi / 4, the float accumulated from 0 (0x6E8E40..0x6E8E56). 0x6E8E68: +0x70 info, pos (Set
  0x603320), +0x28 = a, +0x2C = 1.0; flags |= 0x100.
- GetTownForBuilding 0x6E8F70: a = pos.FindNearType(ABODE 0, 50.0) 0x6045F0 (the nearest, not cut at r); a &&
  CastAbode -> a->GetTown() (even 0); else the nearest town or town plan below 50 m of every player (GetNextPlayer
  0x5508A0). The 50.0 is FindNearType's radius and the town / plan start distance.
- GetOverwriteDropToolTip 0x6E9860: HasPlan(0) == 1 ? the plan info's ToolTipsForBuild (+0x1BC) : 0xEF0 (without a
  plan). GetOverwritePickUpToolTip 0x6EA9D0.
- ValidForPlaceInHand 0x6E96A0: no site -> 1; a site, CanStillBeAdjusted (fn_6EAA40) and the site's percent built == 0
  (`test ah, 0x40`) -> 1; else 0.

#### Combining and splitting

- FindTouchingScaffold fn_6E8C80: only when held (+0x7C): o = fn_604AF0(pos, IsScaffold 0x6EB0A0, 15.0, this) (radius
  0x41700000 = 15.0); o && IsTouching(o, 0.001) (vt +0x6B8, margin 0x3A83126F = 0.001) && both +0x90 == -1 && (this has
  no site || (CanStillBeAdjusted && this site's percent built == 0)) -> o.
- ValidToApplyThisToObject(status, o) 0x6E9900: another scaffold that may combine with it (o IsScaffold (vt +0x49C) &&
  !(o +0x24 & 0x40, IN_PHYSICS: PhysicsObjects::IsFlying)), or a storage pit that stores wood.
- ApplyThisToObject(status, o, packet) 0x6E99E0: a scaffold not flagged (+0x24 & 0xC) -> Combine, the spot visual 0x1B
  (the combine, 0x6E9A1D) and G_ScaffoldCombine at `handPos` (GInterfaceStatus +0xC8; with an interface status), return
  1: the HAND then calls status->GetInterface()->StartImmersion(0x2C, 0x80000000) 0x5D8A10 (0x6E9AEB..0x6E9AFB); a
  storage pit that takes it (StoragePit::DeleteObjectAndTakeResource) -> 3; else 0.
- GParticleContainer::CreateSpotVisual(pos, type, 1.0, 0) 0x63E540: the float is not a duration (0 = the entry's own
  life, see MagicTeleport.cpp).
- Combine(o) 0x6E9B60: this (in the hand) goes into o (on the ground); o survives with both values and this one's
  plan. fn_6E8F50: both +0x8C = GetCombinedTown(o).
  - GetCombinedTown(o) 0x6E8ED0: one town null -> the other; equal values -> this town when this +0x7C is that town's
    player (0x6E8F11..0x6E8F1D: this +0x7C == this town's GetPlayer, vt +0x1C), else o's; else the town of the larger
    value (unsigned, `ja`).
  - 0x6E9B83..0x6E9C1E, o in physics (+0x24 & 0x40) only: v = po +0x104 (PhysOb +0xDC, the linear velocity) and L = po
    +0x90 (PhysOb +0x68, the angular momentum), both whole; PhysicsObject::RemoveObject(o, 0, 0).
  - 0x6E9CA4..0x6E9CDB: o was in physics -> InitialisePhysics(v, 0, thrower 0, add 1, status 0) (vt +0x784); a body ->
    po +0x90 = L.
- Scaffold::InterfaceValidToTap 0x6E9D80 / Scaffold::InterfaceTap 0x6E9DD0 (the tap split):
  - InterfaceValidToTap: value > 1 && not flying (!(+0x24 & 0x40)) && (no site || CanStillBeAdjusted) && +0x90 == -1.
  - The owner workshop (RTDynamicCast, Dynamic_cast<Workshop*>(+0x58) at 0x6E9FFE) with GetSpaceInStore() != 0: slot =
    first free (fn_779610); w->AddScaffold(x) fn_779C10; the slot byte = 1 (taken away, 0x6EA03D); x slot bits = slot
    & 3; x +0x58 = w. The byte is written through Workshop::ScaffoldMoved 0x77A610 (slot[x bits 6-7] = 1) once x has
    its bits: the same byte, the same 1.
  - 0x6EA062..0x6EA0DB, in physics (+0x24 & 0x40) only: L = 0.5 x po +0x90 (PhysOb +0x68, the angular momentum), v =
    po +0x104 (PhysOb +0xDC, the linear velocity, WHOLE); PhysicsObject::RemoveObject(this, 0, 0). The same pattern as
    Rock::SplitInTwo (PhysicsObjects.cpp).
  - This: IsObjectInMap -> RemoveMapObject; pos = pos - off (operator- 0x6055C0); InsertMapObject; then the 3D
    object's identity matrix with the new translation, +0x44 = 1.0, +0x48 = 0 (0x6EA180..0x6EA1DD).

#### Leaving the hand and the physics

- InterfaceSetOutMagicHand(status) 0x6E9770 (the scaffold leaves the hand): status && its player && player +0x8E0 != 2
  ((pending) +0x8E0: taken as != 2) && HasPlan(0) && flags & 0x400 && e = value - plan ScaffoldsRequired > 0 (signed,
  `jle`): a new scaffold of the surplus is made, and the hand must put it in the hand
  (GInterfaceStatus::PlaceObjectInMagicHand 0x5DC870). Status->GetInterface()->StartImmersion(0x2C, 0x80000000)
  0x5D8A10: the hand's, on this return 1.
- InitialisePhysicsFromHand 0x6EA8A0: before Object's 0x636F00 (0x6EA8A3..0x6EA8AD): +0x89 & 8 (flags 0x800) ->
  DestroyThingsInWay 0x6EAB90; after Object's returned a body (0x6EA8D2..0x6EA903): flag 0x400 = (po.flags &
  PhysicsObject::Landed) && !dontReplant.
- InitialisePhysics 0x6EA840: Object::InitialisePhysics 0x637480; a body and an owner -> owner->ScaffoldMoved(this) (vt
  +0x5E0). The hand's path does not come here: Object's InitialisePhysicsFromHand 0x636F00 calls AddObject 0x6443A0
  directly (0x636FB1).
- CanBecomeAPhysicsObject 0x6EA910: snapped in a workshop slot (owner and slot byte == 2) -> 0; a site (+0x68) -> 0;
  else MobileObject's 0x607AA0 (`mov eax, 1`).
- EndPhysics(po, insert) 0x6E8780: the snap to a workshop slot, or BuildBuilding for a gentle drop from the hand, or
  the "no building" puff. GetPlayer && po -> player->ConsiderMakingCreatureMimicPlayer(po +0x24, 6 BUILD_HOUSE, this, 0)
  0x4EA900 (the creature). The class calls r = Object::EndPhysics 0x6375A0 itself first (PhysicsObjects::BackInMap,
  0x6E87B6): back in the map, or ToBeDeleted outside it. The original goes on with the rest either way (0x6E87BB: only
  r is kept).
- Insert (bl, the 2nd argument): town = GetTownForBuilding; snapped = town ? town->CheckScaffoldSnapToPoint(this)
  0x740340 : 0.
- DestroyThingsInWay 0x6EAB90: the objects inside the plan's radius are pushed away (bodies) or deleted.
  - own = Get2DRadius (vt +0x64); R = HasPlan(0) ? the plan mesh's max(+0x24, +0x2C) (ties +0x2C) x the plan's
    GetScale : own. The temporary Game3DObject(0) 0x6EABC3 is never freed (a leak, literal).
  - 36 (0x24) spiral cells (0x6EAC74).
  - 0x6EACE7..0x6EAD09: o not (+0x24 & 2, the MultiMapFixed class bit), CanBecomeAPhysicsObject (vt +0x7B0) and !(+0x25
    & 0x10) -> pushed; else deleted. (pending) the Object +0x25 bit 0x10, taken as clear.
  - 0x6EAD0F..0x6EAE25: e = GetNearestEdgeOfObject(o) 0x636CD0 (GetLHPoint of GetNearestPosOfObject); p =
    Get3DPoint(pos); dx = (e.x - p.x) x 2, dz = (e.z - p.z) x 2 (dy replaced by 0.5); m = dx > dz ? dx : dz (signed,
    `test ah, 0x41; jne`); v = (dx / m x 2, 0.2, dz / m x 2); the push 0.5 / 0.2 (0x6EAD47, 0x6EAD85).

#### Building

- BuildBuilding(player, force) 0x6E9250: the scaffold becomes a site (a new town first for a founding town centre).
  True when a site was made.
  - 0x6E925C: town = GetTownForBuilding(); !HasPlan(0) || plan ScaffoldsRequired > value (`jbe` skips) -> fn_6E8EB0.
  - The new town: Town(pos, GTownInfo 0xDA2780, player, GetTown() +0x5B8, 0, 0xABA52, 1) 0x739350 (0xF28 bytes, line
    0x20D); 0x6E930B push 0xABA52 -> +0x5B4, the 6th argument of Scaffold::BuildBuilding's Town ctor 0x739350, the same for every founded town. (pending) the ctor's
    5th argument 0. The ctor's last argument (push 1 0x6E9309 -> +0x5F8, 0x739583): no influence until
    TownCentre::Process 0x743E53 clears it.
  - fn_740D90(t, GetTown()): t holds every magic type the old town holds (0..0x29). With a player -> t +0x798
    GBelief::SetBelief(player number, t info +0xF0) 0x4387D0 (**🔍 inferred** the founder's belief).
  - G_PlantScaffold 174: GetInterfaceStatusWhoLastDroppedMe 0x63A710 == GGame::MyInterfaceStatus 0x555880 (BuildBuilding 0x6E9413) -> 2D, owner 0; else 3D at the own GetLHPoint with the owner this; track 0 (0x6E93DE..0x6E9470).
  - The villagers' own reaction to the scaffold comes from CreateReaction's spread (BuildBuilding 0x6E93C7).
- TryToBuildPlannedBuilding(town, force) 0x6E9080: 0x6E9080: plan = fn_6E9060(); none -> 0.
- DeletePlannedBuildingsUnderMe 0x6E94A0: the town plans under the new site go, within 100.0; the radius is the site
  root's Get2DRadius, without a site 0.
- SetPhantomMeshAndPosition 0x6E8890: with a site and a building, the phantom takes the site's building mesh (the
  building's info mesh, vt +0xF4). The phantom's matrix (the building's scale / Y angle at its pos, 0x6E8952..0x6E8AF0),
  +0x44 / +0x48 and vt +0x1E8 0x6E8B03 are also set.
- fn_6EAA80(reaction): every town of g_game +0x205C84 (the first node, then town +0x75C, the per-player next:
  literal) with GetDistanceInMetres(pos, town) < info +0x11C (300 m): the first k villagers of the abodes' lists are
  impressed by the reaction (vt +0x98C Villager::UpdateHowImpressed(r, 0): belief only, no state change).

#### Deletion

- GameThing::ToBeDeleted (vt +0xC) of a scaffold, Scaffold::ToBeDeleted 0x6E83E0: the class part, then
  MobileObject::ToBeDeleted 0x606F00. The class part (DeleteDependancys): site -> site->RemoveScaffold(this) 0x43BF20,
  +0x68 = 0; RemoveFromWorkshop; the phantom; both plans' ToBeDeleted.
- 0x6E8421: +0x68 still set and its percent built == 0 -> site->ToBeDeleted(0): dead (+0x68 was cleared above,
  literal).
- RemoveFromWorkshop 0x6E9200: w = dynamic_cast<Workshop*>(+0x58); in w's list (fn_779DD0) -> w->RemoveScaffold(this)
  fn_779C60; +0x58 = 0 in both cases when w (0x6E9236: edx = 0).

### The citadel's parts

- fn_882AC0: LH3DMesh::CreateFromHD("Data\Citadel\OutsideMeshes\B_FIRST_TEMPL...", 0) 0x882AC7, the heart's mesh (Game.cpp
  loads Data\Citadel\OutsideMeshes as "temple/<file>").
- CitadelHeart::CallVirtualFunctionsForCreation 0x4675A0 skips the creation index of the heart, its CitadelEntrance
  (Object ctor 0x636520 from 0x4676CD) and its TempleLeash (CreateLeashes 0x464950). Its steps:
  1. SetPosition 0x467648: the point of the MapCoords, the Y angle (vt +0x508) and scale 1.0 (0x46761F; the plan's
     scale only feeds the influence).
  2. The land flattening of fn_882730 (LH3DCitadel vt +0x234, from 0x46767F); 2.3 InitialiseIsFixedForMapList vt
     +0x850; 2.4 the entrance +0xDC (0x46768F..0x467765); 2.5 InsertMapObject vt +0x544 (0x46776C), with its collide
     shape from CreateCollideData 0x468FB0; 2.6 (pending) +0xE0 fn_454960(2, ..., 15.0, ...), the global heart list
     [0xC5E3D4], +0x90 fn_8499C0, CreateLeashes 0x464950, the +0xA8 object; fn_468DB0 (0x467856).
  3. heart +0x98 = fn_4651D0: the alignment bird flock (Flock 0x52F780, GFlockInfo 0xC5E624).
  4. life >= 1 (fcomp 1.0; test ah, 1; jne) -> fn_464F50(citadel, 0): CREATE_CITADEL; never a plan's conversion.
- The land flattening, fn_882730 0x8827C7.. 0x882962: cx = ftol(x x 0.1) (+0xB8), cz = ftol(z x 0.1) (+0xBC); A = the
  altitude byte (+4) of the land cell (cx, cz), 0 outside 0..0x1FF or without a block; for x = cx - 8..cx + 8
  (outer), z = cz - 8..cz + 8: t = fn_8826C0(x - cx, z - cz), a = the cell's altitude (0 outside), fn_800DA0(x, z,
  ftol(a x t + (1 - t) x A) & 0xFF). It runs when the heart is created: at load for CREATE_CITADEL, at the plan's
  conversion for a planned citadel.
  - fn_8826C0(dx, dz) 0x8826C0: d = sqrt((10 dx)^2 + (10 dz)^2) stored as a float; d > 70 ([0x92B2C8]; fcomp; test ah,
    0x41) -> 1; d < 35 ([0x9A3D7C]) -> 0; else (d - 35) x 0.0285714 ([0x9000D4] = 0x3CEA0EA1). The game thread's x87
    is at 24 bits (fn_007DEE00, `and 0xFCFF` 0x7DEE0D): every step rounds to a float, so d == 70 gives exactly 1.
  - The blend for one cell (dx, dz) from the heart's, of altitude `altitude`, the heart's cell `centreAltitude`
    (0x882919..0x88292D: fild a; fmul t; fld 1; fsub t; fmul A (a float, fstp [esp + 0x10]); faddp; __ftol; & 0xFF):
    ftol(a x t + (1 - t) x A) & 0xFF, in float.
- CitadelEntrance ctor 0x468EB0 (`new 0x68`, CitadelHeart.cpp line 0x5E0; from CallVirtualFunctionsForCreation
  0x46768F..0x467765): Object ctor 0x636520(pos, GObjectInfo 0xD41F68), +0x54..+0x64 = 0, vtable 0x8CA294; its 3D
  object LH3DObject::Create(type 1) with the Entrance mesh, SetPosition at the heart's point (heart angle, scale 1), vt
  +0x1E8; entrance +0x54 = heart. Its Draw (vt +0x610, 0x4648B0) is a bare `ret` and nothing puts it in the map cells.
  (pending) its mesh (vt +0xF4 with the LH3DCitadel's vt +0x204 0x80BBC0, **🔍 inferred** Entrance.l3d, [0xFAA7E4]
  0x88297B) and how the hand reaches it.
- CitadelEntrance (vtable 0x8CA294): vt +0x740 InterfaceValidToTap 0x468F50, vt +0x744 InterfaceTap 0x468EF0.
- `CitadelHeart::ToBeDeleted` 0x464C50: the CitadelEntrance +0xDC ToBeDeleted(now) (0x464CCF), the doves or bats'
  flock and its birds (0x465230 at 0x464D5A, [The temple's doves or bats](../magic/temple-and-citadel.md#the-temples-doves-or-bats)),
  then the building site (MultiMapFixed::ToBeDeleted, through `CitadelPart::ToBeDeleted` 0x464D62).

## openblack

Status: **partial**. Progress rows: [construction](../project/progress/building/construction.md) (89%),
[workshop and scaffolds](../project/progress/building/workshop_and_scaffolds.md) (57%),
[building by the creature](../project/progress/building/building_by_creature.md) (12%).

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#world).

### Where the original's objects live

- The draw of a partly built building: the partial model is `components::DrawMesh` with the mesh of
  `physics::PartialBuild::BuildMesh`; nothing at 0 % is `components::NotDrawn`; the static shadow is
  `abodes::CastsShadowOnTexture`.
- Plans and sites: `ecs::plans` and `ecs::building_sites` (`src/ECS/Town/BuildingSites.h`, `src/ECS/Components/BuildingSite.h`),
  the building's own state in `ecs::abodes` (`src/ECS/Abodes.h`). The graveyard is `src/ECS/Town/Graveyard.h`.
- The building site: one per entity; ecs::building_sites is its only writer and building_sites::Create sets its class.
  The town's list is Town::buildingSites and the building's +0x74 is Abode::buildingSite. Of the GameThing part only
  `beingDeleted` is kept; the global list g_game +0x205CAC and the scaffolds "on the way" are not. On a
  WorkshopBuildingSite `woodPile` stays null. The citadel heart's site also carries components::CitadelBuildingSite.
- The workshop: one per workshop abode entity, assigned by ecs::workshops::Create (from AbodeArchetype);
  ecs::workshops is its only writer. +0xD8 (next in the town's list) is Town::workshops. The "ghost scaffold" +0xCC
  and its draw (not ported) render.
- The wonder: one per wonder abode entity, assigned by ecs::wonders::Create (from AbodeArchetype); ecs::wonders is its
  only writer. Wonder::Get3DType is the archetype's `morphsWithTerrain`. Its power is (not ported) in saves.
- The scaffold: ecs::scaffolds is its only writer. The value is the Transform's scale (scaffolds::GetValue /
  SetValue); the workshop slot helper is the one of both sides (ecs::workshops and ecs::scaffolds). +0x98 is not kept
  (workshops' business). Of the phantom +0x74 only its mesh is kept, and the phantom's draw (Scaffold::Draw 0x6EA5C0,
  RenderingSystem.cpp:180) is (not ported). +0x6C nullopt = a null pointer.
- A scaffold's plans: only what the scaffold reads of them is kept; Town::plannedAbodes never holds them.
- The citadel: `src/ECS/Archetypes/CitadelArchetype.{h,cpp}` (the citadel's parts and its plan); the Citadel is kept
  on its first heart's entity, components::CitadelWorship.
- Files: `src/ECS/Scaffolds.{h,cpp}`, `src/ECS/Components/BuildingSite.h`, `Scaffold.h`, `Workshop.h` and `Wonder.h`.

### Differences and approximations

<a id="building-sites-buildingsite-1"></a>
<a id="citadel-building-site-six-piles-1"></a>
<a id="scaffolds-choosing-the-building-and-the-wonders-power-1"></a>
<a id="citadel-plan-1"></a>
<a id="scaffold-details-1"></a>
<a id="the-citadels-parts-1"></a>

- Plans and sites: **⚠️ approximate** the town areas recomputed at the query; a deleted site is freed one turn later.
  openblack walks the components where the original walks its lists.
- The citadel site: openblack keeps the Citadel on this entity (CitadelWorship).
- Football: openblack keeps no football state nor feature table: 0, its .data value, as BuildingSites.cpp's
  k_FootballEnabled (pending).
- The wonder's mesh: openblack loads the pack by index and gives the abode `info.meshId` (AbodeArchetype), so the mesh
  is the same. Its draw's cut uses the same bounding box the original computes (`L3DMesh`'s, over all sub-meshes).
- The citadel plan: openblack's towns always have an owner (the neutral player too). The heart info: **⚠️ approximate**
  openblack keeps the one record (N3 = 0, every shipped land); another N3 reads past it into the pens' records in the
  original. The heart's footpath link (+0x64) is not ported.
- The heart's creation: **⚠️ approximate** its collide shape (CreateCollideData 0x468FB0) is not ported: map_cells takes
  the mesh's. Its map list setup (2.3) is map_cells' own. The alignment bird flock (step 3) is (not ported). The
  entrance: openblack makes the entrance without one, and no leash; it keeps no model for it (a Mesh would also print a
  footprint). CitadelEntrance::InterfaceValidToTap 0x468F50 / InterfaceTap 0x468EF0 are in ecs::hand_tap
  (Register<CitadelEntrance>).
- The totem (2026-10-16): `life::ReduceLife` / `IncreaseLife` (`life::LifeTarget`) and `effects::IsEffectReceiver`
  (the plinth and its icon).
- Scaffold creation: CallVirtualFunctionsForCreation's MobileObject part is the central MobileObjectArchetype (the
  creation index, the Transform: Y angle only, uniform scale, the components::MobileObject the map cells and the
  physics tell the class by **🔍 inferred**, the map cells). **⚠️ approximate** the cells' insert runs before the
  Scaffold part is assigned: nothing in it reads that part. SetPlannedInvalid skips a null pointer. The info is
  info.scaffold.
- Town founding: TownArchetype::Create with a new unique key (openblack's Town::id: the highest + 1) and +0x5B4 =
  0xABA52 (Town::scriptId). (to do, Milagros) the town's magic types (IsMagicTypeHeld / AddMagicTypesHeld) are not
  ported here. (to do, milagros2) SetBelief and GTownInfo +0xF0 are (pending).
- **⚠️ approximate** map_cells::ForEachTown's order in fn_6EAA80.
- **⚠️ approximate** SetPhantomMeshAndPosition: only the mesh is kept; the phantom's matrix, +0x44 / +0x48 and vt +0x1E8
  are not.
- **⚠️ approximate** the plant sound: Object::GetInterfaceStatusWhoLastDroppedMe 0x63A710 == GGame::MyInterfaceStatus
  0x555880 (the plant sound is 2D then): openblack has one local interface, so "the local hand let it go last"
  (scaffolds::OnLeftHand with PLAYER_ONE).
- Insert is always 1 in openblack.
- The tap split: (not ported) the 3D object's identity matrix with the new translation, +0x44 = 1.0, +0x48 = 0:
  openblack's Transform is both the object's scale (the value) and the drawn one, so only the position moves.
- DestroyThingsInWay: the temporary Game3DObject leak is not ported.
- The creature's mimicry in EndPhysics is (not ported). Without the deferred deletion the entity is already destroyed
  after Object::EndPhysics, so only then the rest is skipped (its Scaffold::ToBeDeleted part runs through the dead-list
  hook).
- Scaffold deletion: GameThing::ToBeDeleted (vt +0xC) of any object the scaffold deletes is an abode's own clean-up,
  else ecs::ToBeDeleted. The dead-list hook of ecs::ToBeDeleted (abodes::OnToBeDeleted -> DeleteDependancys) calls the
  scaffold's class part for an entity with components::Scaffold, so every generic deletion (the storage pit's
  DoDeleteObjectAndTakeResource 0x63AAB1, DestroyThingsInWay, the physics' BackInMap outside the map) runs it. The class
  part is idempotent, safe to run twice; never call it from inside ecs::ToBeDeleted (it would recurse). The dead branch
  0x6E8421 (+0x68 still set) is not ported.
- Guards: openblack adds a null guard in three places of the scaffold code where the original has none, besides
  SetPlannedInvalid skipping a null pointer.

<a id="where-it-is-in-the-code"></a>
### Where it is in the code

`src/ECS/Scaffolds.cpp` and `Scaffolds.h`:
- `NewPlan`: a new plan at a point, with no info, angle 0 and scale 1.
- `WorkshopOwner`: the scaffold's owner when it is a workshop (used when it leaves the workshop and by the tap split).
- `SpotVisual`: a spot visual at a point; its time argument is not a duration (0 keeps the entry's own life).
- `scaffolds::GetTownForBuilding` looks for the nearest abode with `ObjectType::Abode` within 50 m and takes its town.
- `OnPickedUp`: the hand took the scaffold; the old site is cancelled with no rebuild plan and the owner workshop told.
- The physics hooks of the Scaffold class: `CanBecomePhysicsObject` (no while snapped in an occupied workshop slot,
  `Workshop::k_SlotOccupied`, or as a site), `OnBeforeInitialisePhysicsFromHand` (the destroy flag clears the things
  in the way), `OnInitialisePhysicsFromHand` (sets "dropped gently" when it landed and may replant),
  `OnInitialisePhysics` (tells the owner workshop the scaffold moved) and `OnEndPhysics` (the snap to a slot, the
  building for a gentle drop, or the "no building" puff). `OnEndPhysics` returns the entity that stays, or
  entt::null when it was deleted out of the map; the surplus scaffold of InterfaceSetOutMagicHand is returned, or
  entt::null, and is called after the physics' InitialisePhysicsFromHand (which sets the flag 0x400).

`src/ECS/Components/Scaffold.h`, the flags and timers:
- `Scaffold::k_FadePhase` (bit 0, the phantom's cross-fade), `Scaffold::k_HoverDone` (bit 1, the hover test done this
  hand turn), `Scaffold::k_FeedbackMask` (bits 2..5, the "no building here" counter), `Scaffold::k_FoundsTown` (bit 8,
  the plan founds a town; cleared when the phantom changes to a building that is not `AbodeNumber::TownCentre`),
  `Scaffold::k_NoPhantomRotation` (bit 9, no setter found in the original), `Scaffold::k_DroppedGently` (bit 10) and
  `Scaffold::k_Destroy` (bit 11, SET_SCAFFOLD_PROPERTIES' destroy).
- `Scaffold::k_FadeStart` (400) and `Scaffold::k_FadeStep` (100 per hand turn): the cross-fade timer.

`src/ECS/Archetypes/CitadelArchetype.cpp` and `CitadelArchetype.h`:
- `CitadelArchetype::CreatePlanned`: null unless the heart info's mesh suits a fixed object at the plan's point,
  angle and scale; else it goes on with `CreatePlannedNoFixedCheck`.
- `CitadelArchetype::FlattenedAltitude` (`FlattenedAltitude`): the land flattening's blend for one cell,
  `truncate(a t + (1 - t) A) & 0xFF` in float.
- `HeartInfo`: the heart info record of the script's N3 (one record kept, N3 = 0 on every shipped land).
- `CreateEntrance`: the entrance object at the heart's point, pointing back at its heart, with no Mesh and not in the
  map cells.
- `CitadelArchetype::RegisterTapHandlers`: registers the entrance's tap handlers once (later calls do nothing);
  `CreateHeart` calls it.
- The citadel plan tests check that a citadel plan's abode type is `AbodeType::Citadel` (GetAbodeType 0x467E30 gives
  0x804) and that its site has no town.

### Test hooks

- `OPENBLACK_TEST_BUILD_AT="<x>,<z>[,<desire>]"`: forces the building of the planned building at that point, as
  the script command BUILD_BUILDING does (desire 1 by default).

## Pending

- **Verify with xref/oracle:** the **🔍 inferred** and **⚠️ approximate** marks on this page. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- The citadel as a plan (see [Citadel plan](#citadel-plan)) (queued fix F-95).
- A wonder drawn with wooden fences: the mesh choice and the sub-meshes drawn were read and match (see
  [Scaffolds](#scaffolds-choosing-the-building-and-the-wonders-power)); the fences are the model's own scaffold
  sub-mesh, drawn while the wonder is below 80 % built. Still to measure in a run: that a wonder site's build percent
  rises as in the original (a site that stays low keeps showing mostly the scaffold), with a testbed scenario that
  builds one.
