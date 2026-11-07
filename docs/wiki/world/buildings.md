# Buildings

## Summary

A town's buildings as the original runs them: the resources an object holds and the floating numbers that show them,
a building's life, damage and repair, and what a knock does to it. Building sites, plans, the workshop and the
scaffolds are on [Plans and building sites](building-sites.md); the villagers who use the buildings on
[Villagers: data and states](../beings/villager-states.md); the breaking itself on
[Physics impacts](../physics/physics-impacts.md#buildings-that-break).

openblack ports the abodes, their resources, life and damage (what a broken building does to its town included), the
floating numbers and the knock; a few town book-keeping fields and details wait (see [Pending](#pending)).

## The original

Read in `Abode.cpp`, `MultiMapFixed.cpp`, `StoragePit.cpp`, `BuildingSite.cpp`, `Workshop.cpp` and `Town.cpp` of the
exe. The town's belief is on [Towns](../beings/towns.md#the-towns-belief).

### Resources held by objects

`ecs::object_resources` (`src/ECS/ObjectResources.h`) is the original's Object::GetResource (vt +0x98),
RemoveResource (vt +0xA0), AddResource (vt +0x9C) and IsPoisoned (vt +0x4A4) for every object that holds food or wood:

- **Abode**: +0xBC[type] (`Abode::foodAmount` / `woodAmount`). GetResource 0x404D30; RemoveResource 0x404F10 ->
  DoResourceRemoving 0x404F60 (the town's CallDesireFunction before the removal, then JustRemoveResource 0x404D60 =
  min(amount, held)); AddResource 0x404D90 -> DoResourceAdding 0x404DF0 -> JustAddResource 0x404D40 (no cap).
- **Storage pit**: the total of its piles (PotStructure::GetResource 0x66EF00, StoragePit::RemoveResource 0x7332A0,
  `StoragePitStore`).
- **IsPoisoned**: StoragePit 0x7336B0 (any available pile poisoned), Pot 0x55D4E0, an abode 0 (GameThingWithPos
  0x402400), a villager its own flag.

- **Pots and piles**: a pile of a storage pit answers the pit's total (PotStructure::GetResource 0x66EF00); any other
  pot its own amount when the resource is its own (Pot::JustGetResource 0x66D390). RemoveResource of a pit's pile
  (PotStructure 0x66EE10): the touched pile gives n - min(over, n) (over = CalulateAmountOverMaximum 0x733260: wood
  above 25000, food above 15000), then the pit pile 5 -> 1 the rest. A pot that empties (Pot::JustRemoveResource
  0x66D410) loses its reaction, its poison and its fire; a loose pile, a temporary pot, MagicFood or MagicWood is then
  deleted (PotStructure::JustRemoveResource 0x66D9B0), a pit's pile stays. AddResource of a pile: PotStructure 0x66ED70
  (`pot_resource`).
- **Storage pit** (`StoragePitStore`, StoragePit::AddResource 0x732F60 / RemoveResource 0x7332A0): only FOOD and WOOD;
  each pile's JustAddResource (the pile sound with the amount still asked for, the cap, the poison), the town pulse
  +0x5E8 = 1 / +0x5EC = 0 when the pit held none of it and something went in, then DoResourceAdding; removing goes
  pile 5 -> 1 and then DoResourceRemoving (the town's desire before the change, also for villagers). The full
  original sequence is in [Store (StoragePit)](objects.md#store-storagepit).

**The hand's branch.** DoResourceAdding 0x404DF0 and DoResourceRemoving 0x404F60 do more only with a GInterfaceStatus
(`pot_resource::Dropper`); villagers and scripts pass none (0x404E01 `je 0x404EE3`).
- Removing: Town::SetGameTurnResourceLastRemoved 0x7400D0 (Town +0xEC8[player][type] = the turn), the desire after,
  and GAlignment::Update(this, type, -n, before - after) 0x414520 of the **town's owner** (`alignment::UpdateForResource`:
  k = n > 0 ? giveResourceAligmnetChangeMultiplier : take..., 0.5 each; pending += ScaleChange(delta x k)).
- Adding: delta = before - after x Town::GetGameTurnResourceLastRemovedModifier 0x740030 (never taken 1, else
  min((turn - last) / 1000, 1)^3), GAlignment::Update of the dropper's player, and GBelief::AddToBelief 0x437EB0 with
  delta x (not the owner ? multiplierForNonOwnerAddingResource 1.0 : 1) x multiplierForAddingResourceToTown 0.25:
  Town +0x798 GBelief +0xC8 (pending, folded each turn) and +0x28 (recent, decays) += f, +0x48 = the turn
  (`components::TownBelief`). DoCreatureMimicAfterAddingResource (vt +0x68C): openblack's `object_resources::ReportResourceDeed`
  (`src/ECS/ObjectResources.cpp`, the pit's `StoragePitStore::DoCreatureMimicAfterAddingResource`; test
  `test/creature/test_creature_mimic.cpp`).
- GetGameTurnResourceLastRemovedModifier (0x740030) and SetGameTurnResourceLastRemoved (0x7400D0): player ≥ 8
  (unsigned) or type outside FOOD / WOOD → 0 / nothing (0x740037..0x740049, 0x7400D6..0x7400E4); the 1000 is GTownInfo
  +0x100 maxGameturnsForBeliefAfterRemovingFromStoragePit; only Abode::DoResourceRemoving (0x404FD7) calls Set.
  A stored turn of 0 is "never taken" (1). Otherwise the turn difference (fild, as int64) is divided by the count
  (fidiv), capped at 1 and cubed, each step rounded at the FPU's 24-bit precision:
- GBelief::AddToBelief indexes with GPlayer::GetPlayerNumber (0x64A790) without checking (0x437EBB); with a thing:
  DrawBelief if draw (0x437F19) and GGuidance::BeliefSFX (0x437F40), which only sounds for a player below the strongest
  belief (+0x8) (0x437F0A..0x437F2A).
- CallDesireFunction is called from Abode::DoResourceRemoving (0x404FA0) and DoResourceAdding (0x404E22) before the
  change; Villager::ArrivesAtFoodReaction (0x764A45) also calls CalculateDesireForFood.

- 0x4067D2..0x4067DD: ApplyEffect(e, 0) (vt +0x5CC, Object 0x637980): the heal -> IncreaseLife, the damage -> ReduceLife (the site, StopBeingFunctional, the town's emergency), the alignment (GAlignment::Update 0x414410)

#### The floating numbers (ValueSpinner)

Read in the frozen runblack.exe.

- `ValueSpinner` (0xA0 bytes, vtable 0x8C589C, a list from `ValueSpinner::first` [0xED92E8] through +4): +8 the point
  (LHPoint), +0x14 the font (`GatheringText::gamefont`[0] [0xECCD08], the font of `data\j0`, SetupGameFonts 0x830FF1),
  +0x18 the life, +0x1C the colour (ARGB dword), +0x20 the text (UTF-16). fn_00833A00 sets the point, the font, the
  life 5.0 (0x833A1C), the colour and copies the text. `Init` 0x833AE0 (from `Object::DrawValue` 0x6392C0 only) and
  fn_00833A50 (from `GBelief::DrawBelief`'s debug flag only) write a number themselves, red / green / black by its sign.
- `Update` 0x833CB0: vt +0 `UpdatePosition` 0x4382D0 (y += dt × 10, [0x8AB414]), life −= dt, deleted once life < 0
  (0x833CC8). `GGame::Process3dEngine` runs it on every spinner each frame with dt = `g_game_time_inc` [0xEA9EC0] ×
  0.001 (the frame's game ms, 0x54E037..0x54E062), then, unless the help system's wide screen +0x45E8 is on
  (0x54E064..0x54E076) and while the Z-sorter is on ([0xECA644]), queues each spinner's `Draw` 0x833B90 in the Z-sorter
  at its squared distance from the camera (0x54E082..0x54E0CD). Only in the land's frame (case 0).
- `Draw`: `LH3DTech::ProjectPoint` 0x819390 must give 2 (on the screen) (0x833BAD); w = `GetStringWidth`(text, 15)
  ([0x41700000]); alpha = ftol(life × 255) ([0x8AB270]), capped at 255, nothing below 1 (0x833BD5..0x833BF3); then
  `DrawTextRaw` twice in the font at 15 px, clip 0 to 4096: black (0, 0, 0, alpha) at (x − w − 1, y + 1, z + 0.1)
  ([0x8AA390] 1.0, [0x8AB22C] 0.1), then the colour with its alpha byte set to alpha at (x − w, y, z)
  (0x833BF9..0x833CA0). `DrawTextRaw` tests no depth, so the text ends at the point's pixel and is never hidden.
- Who makes one: `GBelief::ProcessOncePerTurn` (the believers gained, above); the village totem's packet 0x29 (grey
  "%3.0f", [The village totem: the grip, the drag and the let go](../magic/village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go)); `Creature::FinishActionUnsuccessfully`
  0x4757A0 (a `CreatureValueSpinner`); the sacrifice in `Living` 0x5ED7AB, `Tree` 0x74BF67 and `Villager` 0x752F90
  `ApplyThisToObject`; sub_744610 (a `VarSpeedValueSpinner`, "%d", no caller found).
- fn_00438340: the "losing belief" help sprite (list 10, fn_0071CCC0, with adults + children) when another player has
  more belief; nothing on land 1 (g_game +0x205A08) (0x438345..0x4383C4).
- GBelief::DrawBelief (0x438800): v = ftol(f × 10000) (truncated); v ≤ 0 → nothing; point = GetLHPoint + GetHeight
  (vt +0x42C) (0x438827..0x438864); the player's colour or 0xFFFFFFFF; with g_game +0x14 & 0x4000 a debug ValueSpinner
  "B n" (Belief.cpp 0x15B).
- Belief sprite queue at 0xD4ED00 (24 bytes per entry; grows 10 at a time, fn_0069B6B0): fn_0069B550 does not add if
  [0xC02A08] != 1 or there are already 400 (0x69B550..0x69B56B); UR_BeliefSprite::ModifyAtomCollection (0x69B750)
  takes the LAST one (LIFO) (0x69B781), every entry each turn; the sprites themselves are in
  [The belief sprites](../physics/particle-rules.md#the-belief-sprites).
- Belief help sprites: LosingBelief fn_0071CCC0 (list 10), GeneralBad fn_0071CD70 (list 13), GeneralGood fn_0071CDF0
  (list 14).

### Life and damage

The building's side of physical damage (Abode::ApplyEffectsDueToPhysicalDestruction 0x406640, StopBeingFunctional
0x4073C0, DestroyedByEffect 0x403F80;; the hit itself is in
[Physics](../physics/physics.md)). The original lowers the life through the effect system (EffectValues(3) 0x524FE0, damage =
max(0, life - remaining) / GetDefenseMultiplier 0x637930, Object::ApplyEffect vt +0x5CC -> Abode::ReduceLife
0x405D90), not by setting it, and shows HelpSpritesDestroyBuilding 0x71D070 when less than 0.4 is left (0x406753) and
the player is the local one.

- A storage pit under construction whose percent reaches 0 drops its life from 1 to 0 (0x52F659), crossing the 0.75
  threshold: StopBeingFunctional and `SetInStateOfEmergency` (0x405E07..0x405E3E).
- A house does not cause a town emergency: `CausesTownEmergencyIfDamaged` of Abode (0x4016F0) returns 0.
- `TownCentre::StopBeingFunctional` 0x744A00 sets the town's worship percentage to 0.
- `TownCentre::ToBeDeleted` 0x743B40 runs DeleteDependancys 0x743BE0 twice (0x743B4E, then `Abode::ToBeDeleted`
  0x402C6F): on the second pass the town's +0x9A4 is not this centre, so 0 is written (0x743BFD) and +0x9A4 always ends
  at 0. The totem +0xCC is deleted with it (0x743B54..0x743B65).
- `Creche::DeleteDependancys` 0x50AA50 writes the town's creche +0x744 = the creche found, unconditionally (0x50AAB3);
  found is 0 when +0x744 is another creche.
- `RemoveStructureFromTown` (0x739A8A) sets +0x98 = 0; `MultiMapFixed::ToBeDeleted` calls
  RemoveAllReactionsInitiatedByObject (0x52E2B7).
- `Town::ProcessTownRepairs` skips an abode that already has a site (0x747E39).
- Emergency: started by Abode::ReduceLife (0x405E3E) when a storage pit or centre drops below its functional
  threshold, or by ProcessTownEmergency; it lasts GTownInfo +0x110 (1200 turns).
- Town::SetInStateOfEmergency (0x7479A0): if +0xF1C == 0 it calls CallAllVillagersToTownEmergency; in both cases +0xF1C
  = turn (on turn 0 it stays "no emergency", literal) (0x7479A3..0x7479D5).
- CallAllVillagersToTownEmergency (0x747890): abodes of +0x754 and inhabitants of +0xA0 in list order; row = 0xD091E8 +
  0x90 × (GetFinalState & 0xFF), if its function gives != 0 → SetTopState(0xF2); the next villager (+0xE4) is read after
  SetTopState (0x747898..0x747949); a debug hook via [0xC22E8C] if [0xC22E84] == 0 (0x747951).
- Town::ProcessTownEmergency (0x7477A0) (step 15): in an emergency, if +0xEB0 == turn it is refreshed; a worship
  percentage +0x5C0 != 0 is saved in +0xEC4 and set to 0 (SetWorshipPercentage 0x73C060) (0x7477AE..0x7477F0).
- Outside an emergency: storage pit or centre (+0x9A4, no IsAvailable test) on fire (Object::IsOnFire 0x637CC0) →
  starts it (0x7477F9..0x747826); otherwise, with +0xEC4 != 0 and +0x5C0 == 0 the saved percentage comes back; then
  +0xEC4 = +0xF1C = 0 every turn (0x74782F..0x747873).
- Town::UpdateAggressor (0x73C9B0) (from Object::ApplyEffect 0x637B7D, ReduceLifeDueToBurning 0x637C91 and the
  physical shield 0x72D783): +0xEAC = the effect's GetCausedPlayer (0x525910) or the neutral (0x73C9C7), +0xEB0 = turn
  (0x73CA82 / 0x73CA98), and the per-player aggression slot; all of it in
  [villagers.md, "Town aggression"](../beings/towns.md#town-aggression).

- The first other functional abode of the town's list (+0x754, next +0x9C: town_stats::AbodesOf) whose ABODE_TYPE (info +0x120) has one of `bits`, as StoragePit::DeleteDependancys 0x732D05..0x732D2B (`test byte, 0x24`) and Creche::DeleteDependancys 0x50AA84..0x50AAAA (`test byte, 0x44`) search it: the bits, then IsFunctional (vt +0xD4), then not this one. Literal: the Civic bit 2 is in both masks, so any functional civic building passes. The order: Abode::Abode 0x4013BB calls Town::AddStructureToTown 0x7399A0, which links the abode at the head of +0x754 (0x7399C3..0x7399CF): the newest abode first, AbodesOf's order (the creation index, descending)
- Creche::DeleteDependancys 0x50AA50, its own part (then Abode::DeleteDependancys, 0x50AABC): with a town (vt +0x48), found = its +0x744 is this creche ? the first other functional abode with ABODE_TYPE bit 2 or 6 : 0 (esi = 0 from 0x50AA61); then +0x744 = found, unconditionally (0x50AAB3)
- TownCentre::DeleteDependancys 0x743BE0, its own part (Abode::DeleteDependancys 0x403F00 follows, 0x743C84)
- DeleteDependancys (vt +0x910) of the abode's class: Field 0x528100 (fields::DeleteDependancys; no Abode part), StoragePit 0x732CD0, Graveyard 0x595CE0, Workshop 0x7794F0, Wonder 0x778E60, Creche 0x50AA50 (their part, then Abode::DeleteDependancys), Abode 0x403F00 (= jmp RemoveAllVillagersFromAbode 0x404560: Villager::HomeDeleted 0x7611F0 of each: out of the abode, the town's homeless list, 129 HOMELESS_START), TownCentre 0x743BE0
- StoragePit::ToBeDeleted 0x732C30, its part before Abode::ToBeDeleted (0x732CB8): DeleteDependancys (vt +0x910, 0x732C38), then the food pile +0xC4 and the five wood piles +0xC8.. (0x732C3E..0x732CB3), each one available (vt +0x2C): SetMultiMapFixed(0) (vt +0x868), its ToBeDeleted(now) (vt +0xC, push ebx 0x732C6D / 0x732CA5), the slot = 0. An unavailable one keeps its slot (literal)
- Town::RemoveStructureFromTown 0x739A60 (Abode::ToBeDeleted 0x402C93), the part openblack keeps: +0x98 = 0 (0x739A8A: Abode::GetTown 0x401730 answers no town) and out of the town's list +0x754 (0x739B17..0x739B57). (not ported) an unbuilt abode's sites in +0x790 (0x739A94..0x739AEE; MultiMapFixed::ToBeDeleted deletes its own site), fn_739A40 for a built one, vt +0x8F0(0) and +0x990 fn_66C610. The emptied town's ToBeDeleted and SetTownArea 0x739BB6: see the next item
<a id="a-towns-deletion"></a>
- **A town's deletion** (read in the v1.42 exe 2026-10-27, bw1-decomp `Town.cpp:157/219/254/756`):
  - The trigger, `Town::RemoveStructureFromTown` 0x739A60 (0x739B61..0x739B8F): once the abode is out of the list,
    with the abode list and the planned list both empty and `stats.NumAdults + NumChildren` 0, a town not yet
    unavailable gets `ToBeDeleted(0)` (vt +0xC) and the call returns: no `SetTown(0)`, no playtime part and no
    `SetTownArea`. Otherwise `SetTownArea` 0x739BB6, also for a town with people but no abode or plan left. Its callers:
    `Abode::ToBeDeleted`, `Abode::Delete`, `SpellDispenser::SetTown`, the football's destructor (fn_006439C0) and
    `Town::DeleteDependancys`.
  - `Town::ToBeDeleted` 0x739970: for an available town, `GameThingWithPos::ToBeDeleted` (the mark: the town is
    unavailable from here, so a building that leaves it later deletes it no more), `DeleteDependancys` (vt +0x500)
    and `ForceNeedUpdateInfluence` (0x739993).
  - `Town::DeleteDependancys` 0x739690, in order: the playtime's ToBeDeleted; `RemoveTownFromPlayer` 0x73A7D0; the
    field list emptied; each homeless villager, the head first, out of the list and, when available,
    `Villager::TownDeleted` (into the vagrants unless there, `SetTopState(130 VAGRANT_START)`, `SetTown(0)`); every
    abode left `RemoveStructureFromTown`; every animal `SetTown(0)` and out of the list; the meetings and the planned
    abodes ToBeDeleted each; the building sites ToBeDeleted all; the fish farms, spell icons (deleted), flocks and
    creature records lists emptied; the name; each need flag (`town_desire_flags[i]`) ToBeDeleted, its slot 0; each
    reward of the game's list whose town is this one ToBeDeleted; out of the game's town list; the forests,
    artefacts and missionaries lists emptied.
  - `RemoveTownFromPlayer` 0x73A7D0: `UpdateInfluence(0)` with the influence put back after it; the empty countdown 0;
    with a player: `GPlayer::RemoveTown` 0x64C0E0 (out of its town list) and every magic type the town holds
    disabled for the player; with a worship site: `WorshipSite::RemoveTown`, the site's building site out of the
    town's list when it is in it (`RemoveBuildingSite(MultiMapFixed*)` 0x73BA20, which deletes it), and the site's
    builders from the town sent on; the citadel heart's building site the same (its builders sent on, the site out of
    the list, `RemoveBuildingSite(BuildingSite*)` 0x73B990, which does not delete it); the worship site link 0 and the
    travel time 0; each abode `RemoveFromPlayer`; the owner 0; `UpdateNearestTowns`.
  - `THING_VALID` of a town reads `Town::IsCompletelyDestroyed`, not whether it was deleted
    ([script-commands.md](../scripts/script-commands.md)).
- MultiMapFixed::ToBeDeleted 0x52E2B0 (before Object::ToBeDeleted 0x636670, the generic part of ecs::ToBeDeleted): the reactions it started (Reaction::RemoveAllReactionsInitiatedByObject 0x6E4750, 0x52E2B7); its footpath link (vt +0x74, +0x64: `footpaths::DeleteLink` in openblack's `MultiMapFixedToBeDeleted`); out of g_game +0x205CB4 ; in the map -> g_game +0x59C0 |= 1 ((pending) its reader); the building site (+0x74) ToBeDeleted and fn_52E3F0(0) (0x52E33A.. 0x52E34F: building_sites::ToBeDeleted clears +0x74 itself). Nothing of the physics at the mark (Abode::ToBeDeleted 0x402C60 and this one touch neither the DestructionMesh +0x90 nor the body): the body goes at the next BeginTurn (PhysicsObject::GameTurnUpdate 0x645018) or with the immediate deletion, the FragMesh's model when the entity is really destroyed (OnBuildingDamageDestroyed / OnDrawMeshDestroyed)
- Abode::ToBeDeleted 0x402C60
- 0x402C6F: DeleteDependancys (vt +0x910). Literal: StoragePit, Graveyard, Creche, Workshop and Wonder have run it already, so it runs twice for them; the second pass of a graveyard / a creche finds the town's +0x748 / +0x744 no longer this one and writes 0 there (SetGraveyard(0) 0x595D4A, 0x50AAB3); a wonder is taken off its player twice (Wonder::RemoveFromPlayer 0x778F50)
- 0x402C9F..0x402CA6: MultiMapFixed::ToBeDeleted 0x52E2B0 with the argument
- 0x464CD3..0x464CE6: the CameraExclusion +0xE0 deleted (0x454A00): (not ported) no camera exclusion in openblack 0x464CEC..0x464D18: the citadel's +0x30 (its heart) = 0 when this: (pending) they are not all checked for an unavailable heart 0x464D5A `CitadelHeart::RemoveAlignmentFlock` 0x465230: +0x98 is the heart's alignment flock (its +0x38, the heart, = 0; its birds deleted, fn_00530140; ToBeDeleted(0)); openblack deletes the birds from the head of the flock, then the flock (`CitadelHeartToBeDeleted`, `src/ECS/Abodes.cpp`) 0x464D62: CitadelPart::ToBeDeleted 0x469540 (out of the citadel's part list +0x4C: none in openblack) -> MultiMapFixed::ToBeDeleted 0x52E2B0
- WorshipSite::ToBeDeleted 0x77AA60 -> CitadelPart::ToBeDeleted 0x469540 (0x77ABFC: out of the citadel's part list +0x4C, next +0x84: none in openblack) -> MultiMapFixed::ToBeDeleted 0x52E2B0 (0x4695B1)
- 0x406722..0x406738: EffectValues(3, hitter, player) 0x524FE0: the preset's numbers and radius (fn_005250A0), the applier (fn_005256C0) and the player (+0x3C)
- 0x40674B..0x406781: r = +0x18 (FragMesh::GetRemaining); r < 0.4 [0x980188] && player && IsMemberOfThisPlayer(MyInterfaceStatus) -> GGuidance::HelpSpritesDestroyBuilding 0x71D070 (openblack: `guidance::RemarkBuildingDestroyed`, from `abodes::OnPhysicalDamage`)
- 0x4067B7: EffectNumbers::operator*= 0x525720 (f); 0x4067C3..0x4067CD: EffectValues::operator/= 0x525950 by GetDefenseMultiplier 0x637930, number by number (fdiv; a 0 multiplier divides by 0, literal)
- Abode 0x4016F0 = 0; StoragePit 0x55CCE0 = 1; TownCentre 0x55DB30 = 1
- P = GetPercentBuilt - amount; p > 0 ? p : 0 (test ah, 0x41); SetPercentBuilt (fn_52EDD0); p == 0 -> Object::ReduceLife(GetLife(), player) (0x52F659: life 0)
- Abode::DeleteAbodeSurroundingObjects 0x403D20 (Abode::ToBeDeleted 0x402C98): the script highlights in the cell of point 1 (when the info has a didYouKnow) and the street lanterns in the cell of point 0, ToBeDeleted(0)
- StopBeingFunctional vt +0x918: Abode 0x4073C0 (a player and +0xB9 >= 200: the player's statistics, `game_stats::BuildingStoppedFunctional`; nothing else), StoragePit 0x733960 (then Pot::SetupReaction on its piles that hold something), TownCentre 0x744A00 (then the town's worship percentage 0). The villagers stay and no site is made here (ReduceLife makes it)
- Abode::DestroyedByEffect 0x403F80: a building whose site has a scaffold is started again from zero where the site's head scaffold is (Scaffold::RemoveOldBuildingSite + ForceBuildBuilding, 0x404001..0x404047); else ToBeDeleted. (not ported) GoolooGooloo 0x5E6540 and the script abode's keep path (IsInScript vt +0x448)
- GameThing::ToBeDeleted (vt +0xC) of a building: ecs::ToBeDeleted, whose building part is OnToBeDeleted below (Abode::ToBeDeleted 0x402C60 and the classes'), then the map cells, the physics and the registry (or the dead list)
- The buildings part of GameThing::ToBeDeleted (vt +0xC), the class's own unlinking. ecs::ToBeDeleted calls it for every entity at the mark, before the map cells and before components::Unavailable (GameThing::ToBeDeleted 0x56FB70 sets the flag last: the thing is still available while it unlinks). It never calls ecs::ToBeDeleted on the entity itself (only on the piles, pots, totem and sites it owns). Nothing for a PlannedAbode (0x4056B0: Town::RemovePlanned). Nothing for anything that is not one of these: - a scaffold: Scaffold::ToBeDeleted 0x6E83E0, its part before MobileObject::ToBeDeleted 0x606F00 (scaffolds::DeleteDependancys); - a worship site: WorshipSite::ToBeDeleted 0x77AA60 (worship::site::ToBeDeleted), then CitadelPart::ToBeDeleted 0x469540 (the citadel's part list +0x4C: none in openblack) and MultiMapFixed::ToBeDeleted 0x52E2B0; - an abode (the fields and the civic buildings too): the class's own part (StoragePit 0x732C30: DeleteDependancys and its six piles; Graveyard 0x595CB0, Creche 0x50AA30, Wonder 0x778E40: DeleteDependancys; Workshop 0x779480: workshops::ToBeDeleted; Field 0x5280F0: nothing), then Abode::ToBeDeleted 0x402C60: DeleteDependancys (vt +0x910) once more, MoveAbodeToPlannedAbodes, Town::RemoveStructureFromTown (Abode::townId = Abode::k_NoTown: out of every town list), MultiMapFixed::ToBeDeleted (its reactions, its building site). Nothing of the physics: Abode::ToBeDeleted touches neither the FragMesh (+0x90) nor the body (the body leaves at the next BeginTurn, 0x645018; the FragMesh's model at the real destruction, ConnectDrawMeshListener's listeners). `now` is GameThing::ToBeDeleted's argument, handed on as the original does to what goes with the thing: the pit's piles (push ebx 0x732C6D / 0x732CA5), the building site (push esi 0x52E347), the worship site's totem and food pot (0x77AACF / 0x77ABCC). ecs::ToBeDeleted should pass its own (the engine area's abodehook calls it with the default)
- CausesTownEmergencyIfDamaged vt +0x920: Abode 0x4016F0 = 0, StoragePit 0x55CCE0 = 1, TownCentre 0x55DB30 = 1
- Abode::ReduceLife(amount, player) 0x405D90 around MultiMapFixed::ReduceLife 0x52F5E0 (repair_spec.md §2.1, §2.2): built -> Object::ReduceLife; not built -> +0x5C - amount (>= 0) through SetPercentBuilt, at 0 the life too; then the stop-being-functional part with the town's emergency (town_emergency::SetInStateOfEmergency for a storage pit or a town centre, also an unbuilt one at 0 %) and the building site (+0x638 = 0 here: an ordinary site, repaired by the builders of GetBestBuildingSite, repair_spec §5.4). A field's vt +0x5B8 (Field::ReduceLife 0x52A0A0) changes nothing. Below 1, each inhabitant's SetStateWhenTappedOnAbode (V11). Returns the new life. The physics', the effects', the fire's and the beam's damage comes here

### The knock on a building

A tap on a building. The hand's side, the packet and the turn it lands on are in
[Tapping objects](../hand/grab-hold-and-throw.md#tapping-objects).

- Every Abode class (Abode, Windmill, Creche, Field, Football, Wonder, Graveyard, Totem, StoragePit, SpellDispenser,
  Workshop) takes a tap through Abode's `InterfaceValidToTap` 0x406820 (1) and `InterfaceTap` 0x406830, once it is
  begun (`Abode::IsInteractable` 0x407200: GetPercentBuilt != 0). The village centre does not: `TownCentre`'s
  ValidToTap 0x55DB10 is 0. PuzzleTotem has both its own (0x6DA5F0, 0x6DA610).
- `Abode::InterfaceTap` 0x406830, in order: the building's town (`Abode::GetTown` 0x401730, may be null) kept in
  [0xC4CC6C] (0x40683F); `HowManyPeople::KnockKnock` 0x829690 (0x406844), the population read-out; each inhabitant
  (list +0xA0, next +0xE4, newest first: `AddVillagerToAbode` 0x404060) through
  `Villager::SetStateWhenTappedOnAbode` 0x752B80 (0x406855, its result ignored); then, for living quarters only
  (`GetAbodeType()` 0x4061F0 & 2: the houses and the windmill), the hand's `Ctap_house` clip 0x39 for the local
  status and the knock sound 0x6E + [0xC4CC7C] (the counter wraps at 9), owned by the building
  ([The audio engine](../audio/engine.md)). No ownership test: any player's town takes it.
- The read-out (8 s, a gold person per adult living there and a green one per free place, over each drawn building
  of the knocked town) and the held villager's home (one teal person, the abode's +0x7C bit 0x10):
  [The population read-out](../interface/screen-and-readouts.md#the-population-read-out).
- The villagers that come out (available and inside, two `GameFloatRand` draws each, the walk out of the door, the
  yawn): [Repairs, the tap on a home and the town emergency](../beings/villager-jobs.md#repairs-the-tap-on-a-home-and-the-town-emergency). `Abode::ReduceLife`
  0x405DF3 sends them out the same way when the new life is below 1.

- 0x40683A..0x406844: GetTown -> [0xC4CC6C], HowManyPeople::KnockKnock 0x829690 (openblack: `AbodeKnockSystem`'s read-out). 0x406849..0x406862: for any abode, before the type test, each inhabitant of +0xA0 (next +0xE4) SetStateWhenTappedOnAbode 0x752B80 (VillagerEmergency.h, V11_spec §5) a copy of the list; the original walks +0xA0 / +0xE4 live, and SetStateWhenTappedOnAbode does not change it (only LivingAction::SetState + SetupMoveToPos, 0x761410 / 0x761421)
- 0x405DE6..0x405E02: every inhabitant (+0xA0, next +0xE4 read after the call) SetStateWhenTappedOnAbode 0x752B80 (VillagerEmergency.h). a copy of the list; the call does not change it (0x761410 / 0x761421)
- +0x98 the town (Abode::GetTown 0x401730), by its id (Town::id, a key of RegistryContext::towns)

### Abode details

- fn_007401D0 (at 0x739B12): a workshop goes to Town::RemoveWorkshop (its DeleteDependancys has done it already).
- CitadelHeart::GetAbodeType 0x464B60 and WorshipSite::GetAbodeType 0x55DC70 (vt +0x8C4) = 0x804
- EffectValues(3, ...) 0x406732: the preset g_EffectInfo[3] (0xCC94C8 + 3 x 0x34, info.dat effect[3]: crush 1, alignment modification 1, radius 1)
- Abode::StopBeingFunctional 0x4073C8: the age byte +0xB9 from which the player's statistic counts (cmp 0xC8; jb)
- GetPlayer (vt +0x1C) of the hitter: what EffectValues::GetPlayer 0x5254C0 answers when AppliedBy (+0x28) is set (it is taken before +0x3C). The hitters of ReactToPhysicsImpact's path (PhysicsConstantsType 3 / 20) are mobile statics: Rock::GetPlayer 0x6E77A0 = +0x90 (Rock::SetPlayer vt +0x20 0x439720; (pending) its writers: none on the hand's path, null here), MobileStatic::GetPlayer 0x6088B0 = the +0x7C object's player ((not ported) null) else the neutral player (g_game +0x205A5B). Also a villager (Villager::GetPlayer 0x7502F0) and a tree (0x55D8C0 = 0)
- StoragePit::DeleteDependancys 0x732CD0, its own part (then Abode::DeleteDependancys, 0x732D56): with a town (vt +0x48) whose GetStoragePit 0x73B5B0 is this pit, the first other functional abode with ABODE_TYPE bit 2 or 5 goes through dynamic_cast<StoragePit*> (0x732D3C: null for any other class) into Town::SetStoragePit 0x73EA60
- 0x743BE5..0x743BEC: GetTown (vt +0x48); none -> only the icons
- 0x743BF2..0x743C48: esi = 0; +0x9A4 == this -> the town's list +0x754 (the newest first, next +0x9C): the first whose info +0x120 is 0x404 (TownCentre: `cmp ..., ebp`, an equality, not FirstOtherFunctional's bit test), IsFunctional (vt +0xD4) and != this; +0x9A4 = esi (0 too when +0x9A4 was another centre: literal)
- 0x743C4E..0x743C80: the six spell icons +0xD0
- The pile's ToBeDeleted: PileFood 0x66E100 closes its speed-up visual (nothing for a wood pile, PotStructure 0x66D960); both end in Pot::ToBeDeleted 0x66D110: Pot::RemoveReaction 0x66D6A0 (0x66D113), then the common deletion
- 0x739B61..0x739B8F: no abode left (+0x758 == 0) and no plan (+0x9AC == 0) -> the town's ToBeDeleted, no SetTownArea ((pending) +0x618 + +0x61C == 0 and !(+0xA & 1), taken as true); else SetTownArea 0x739BB6 (this abode is out of town_stats::AbodesOf already)
- 0x52E33A..0x52E348: the site's ToBeDeleted with this one's argument (push esi)
- 0x402C66: town = GetTown() (vt +0x48), before anything
- 0x402C75..0x402C93: with a town and !(g_game +0x14 & 0x8000) ((pending) the bit taken as clear, as in BuildingSite::ToBeDeleted): MoveAbodeToPlannedAbodes (vt +0x90C, 0x402C8A), then Town::RemoveStructureFromTown
- 0x402C98: DeleteAbodeSurroundingObjects 0x403D20
- Citadel::ToBeDeleted 0x462B90, called by the heart's with 0 (0x464D91): on openblack's citadel entity (the heart's)
- 0x462B96..0x462BB4: the six worship sites +0x34..+0x48, each ToBeDeleted(arg); WorshipSite::ToBeDeleted 0x77AA60 clears its slot (a copy: the sites change the array)
- 0x462BB6..0x462C54: the list +0x7C freed (fn_00462CC0) and the lists +0x4C (next +0x84), +0x54 (+0x8C), +0x5C (+0x44) and +0x64 (+0x84) deleted: (not ported) openblack's citadel keeps none of them 0x462C57..0x462C66: the player's +0xA48 = 0 when this: worship::citadel::Of skips an unavailable citadel 0x462C6C..0x462C90: SetVirtualInfluence(0) 0x64C3B0 only in a multiplayer game: (not ported) single player 0x462C95..0x462C9B: GGame::ForceNeedUpdateInfluence 0x555270
- 0x462CA7: GameThingWithPos::ToBeDeleted 0x56FE00: ecs::ToBeDeleted's own part
- CitadelHeart::ToBeDeleted 0x464C50(now)
- 0x464C59..0x464CD0: the CitadelEntrance +0xDC: its torches +0x58 / +0x5C (deleted, vt +4) and their night lights +0x60 / +0x64 (fn_00823420), its 3D object +0x40: openblack deletes the two lanterns and their lights (`worship::citadel_lanterns::Delete`, from `CitadelHeartToBeDeleted`, 868cdd684); (not ported) the 3D object, which openblack's entrance has none of; then its ToBeDeleted(now) (push ebp 0x464CCF)
- 0x464D67..0x464D94: its citadel (vt +0x114), when it is not being deleted already (+0xA & 1): ToBeDeleted(0)
- Scaffold::ToBeDeleted 0x6E83E0, its part before MobileObject::ToBeDeleted 0x606F00: out of its site and its workshop, the phantom and both plans
- Graveyard::ToBeDeleted 0x595CB5, Creche::ToBeDeleted 0x50AA35, Wonder::ToBeDeleted 0x778E45: DeleteDependancys (vt +0x910) first
- 0x4035C0..0x40367D: flag 0x800000, then the first entry whose +0x34 is the type
- 0x403E07..0x403E24: info +0x1C0 (didYouKnow) and GetNewEp(1)
- 0x403E26..0x403E43: ScriptHighlight::Create(MapCoords(point) 0x603160, row 1 0xD964A0, 0, 0.0, 1.0)
- 0x403E79..0x403ED6: GetNewEp(0), no lantern within 40 m (0x42200000) -> GStreetLantern::Create(coords, GMobileStaticInfo[7] 0xD3AF0C) 0x7346E0
- 0x403ECD: the altitude set to 0 before GStreetLantern::Create (it stands on the ground)
- 0x403D27..0x403D8E: info +0x1C0 and GetNewEp(1): the cell's type 0x23 things whose IsScriptHighlight (vt +0x48C)
- 0x403D90..0x403DEA: GetNewEp(0): the cell's type 0x1C (MobileStatic) things whose IsStreetLantern (vt +0x204)
- 0x401650: (+0x7C >> 2) & 1
- 0x401660: +0x7C = (+0x7C & 0xFB) | (b & 1) << 2
- MultiMapFixed::Built 0x52EBB0 of a CitadelPart, then the class's part: CitadelHeart::Built 0x465000 (worship::citadel::HeartBuilt) or WorshipSite::Built 0x77AC10 (WorshipSiteBuilt)
- MultiMapFixed::GetPercentRepairedForNonFunctional 0x52EFC0
- fn_404960 0x404960 (the tail of Abode::MakeFunctional; V6_pending §5): the town's first "a centre, a storage pit and a house" (Town +0x5FC, set once)
- 0x404990..0x404A0E: the other abodes (+0x754, next +0x9C) whose GetPercentBuilt (vt +0x880) is not below 1
- 0x404A10: already complete before this one
- 0x404A24..0x404A59: this one, with no %built test
- 0x404A71..0x404A88: +0x5FC = 1; GetPlayer()->FUN_0064DA80(12, 1) returns at once outside a multiplayer game (0x64DA90): nothing to call. (not ported) the readers of +0x5FC are not searched
- CitadelPart::IsBuilt 0x464AD0 (the citadel heart) and WorshipSite::IsBuilt 0x77BDD0 (the same test; its walk of +0xE0 is dead): !(+0x58 & 2) && GetPercentBuilt >= 1
- MultiMapFixed 0x438D80
- 0x4016A0 / CitadelPart 0x464AB0: GetPercentRepaired (vt +0x884 = GetLife) >= 1
- MultiMapFixed 0x438D70
- 0x401500: GetLife (vt +0x11C)
- 0x407290 / 0x52EFC0
- GetDestructionMesh (vt +0x8B4, Abode 0x401700 = +0x90) && +0x74
- Only the creation (0x52EA40) and Built (0x52EC2C) change the bit, never the draw: a NotDrawn built abode keeps it. +0x58 bit 8: off from the ctor (0x52E24A) until Built (0x52EC3A), the same lifetime as the 0x1000 bit the citadel heart: the LH3DMeshedObject ctor leaves +4 = 0x10009 (0x816537, bit 0x1000 clear) and only LH3DCitadel::SetPercent turns it on, vt +0x80(1) = fn_7F9880 when the percent reaches 1 (0x883186): an unbuilt temple casts no texture shadow. (pending) a worship site's bit (its creation is not read): on
- An abode's +0x5C or a CitadelPart's (the citadel heart's and the worship sites' vt +0x900 is MultiMapFixed's too; WorshipSite::BuildBy 0x77DC50 then builds its totem +0xDC: (not ported) the WorshipTotem keeps no building state)
- 3. +0x40 && !IsField (vt +0x210): SetShadowOnTexture(1) 0x52EC2C (the built bit below: CastsShadowOnTexture) and RequestChangeTexture 0x5E2FF0 (the per-block re-bake: not needed, openblack redraws the static shadows each frame) 4. +0x58 = (+0x58 & ~2) | 8; +0x5C = 1.0
- Abode::Built 0x404720: with a town and a player (0x40472C..0x404743), GameStats(player +0xA44)::IncrementAllBuildingsBuilt 0x56A3F0 (this) by GetAbodeType (vt +0x8C4), and FUN_0064da80(IsWonder ? 4 : 3, 1) (multiplayer only); then with a town MakeFunctional (vt +0x914)
- 2. 0x4047FA..0x404812: +0x58 bit 2 = !IsRepaired()
- 3. 0x404818..0x40483C: +0x7C bit 1 once, Town::AddAbodeToTownStats 0x739A20 = TownStats::Add(Abode) 0x7498C0 (town_stats::Compute counts the abodes with the flag from the next town turn)
- 4. GetRoomLeftForAdults 0x404660 != 0 -> Town::AllVillagersCheckNeedNewAbode 0x73D150, a bare `ret` in the exe 5. IsRepaired && IsBuilt -> Town::RemoveBuildingSite(this) 0x73BA20 (a repair site, if any)
- 6. 0x404860..0x40488C: a storage pit that is not this one and turn (g_game +0x205A40) > 0: the footpath to it (footpaths::MakeAbodeFootpath: GFootpath 0x534EB0, AttemptRerenderFootpathWithCreatureRP 0x5387D0, AddFootpathLink on both)
- The class's own MakeFunctional (vt +0x914): StoragePit 0x732F30, Creche 0x50AB50, Graveyard 0x595E00 and TownCentre 0x743E80 call Abode::MakeFunctional first, then their part
- TownCentre::MakeFunctional 0x743E80: CreateTotemIfNecessary 0x743E9C, +0x9A4 when null 0x743EAB, the spell icons (AbodeArchetype::MakeTownCentreFunctional, in that order)
- RemoveDamage (vt +0x8B8, Abode 0x403F40): the DestructionMesh goes and the whole model is drawn again (physics::Buildings::RemoveDamage; it redraws the construction itself when it had one)
- Abode 0x401680: `ret`. StoragePit::RestartBeingFunctional 0x7339D0: the food pile +0xC4, then the five wood piles +0xC8.., each when available (vt +0x2C) -> Pot::RemoveReaction 0x66D6A0
- Vt +0x5B8 of a field (it carries an Abode too): Field::ReduceLife 0x52A0A0 = GetLife; ret 8: nothing changes
- MultiMapFixed::ReduceLife 0x52F5E0 (0x405DCC)
- Else if (old >= 1.0) FUN_004073F0(player) 0x4073F0: a multiplayer statistic only (V6_pending §5) 0x405E5E..0x405E7A: no site and a town -> Town::AddBuildingSite(this) 0x73B8E0
- 0x405EBE: l == 0 -> FUN_00405D80, `mov eax, 1; ret` (no effect)
- MultiMapFixed::GetDesireToBeRepaired 0x52ECE0 (the citadel heart's and the worship sites' vt +0x8D8): repaired -> 0; else v = ((1 - GetPercentRepaired) x 0.5 + 0.5) x info +0x118, v below 1 (fcom; test ah, 1), else 1
- Abode::GetDesireToBeRepaired 0x406970 -> MultiMapFixed 0x52ECE0, through TownDesire's port of both
- 0x40452F..0x404551: !GetShouldNotBeAddedToPlanned (vt +0x8F8, 0x401650: +0x7C bit 2) && PlannedAbode::Create(this) 0x405660 != 0 -> 1. The bit's only writer, SetShouldNotBeAddedToPlanned (vt +0x8FC), is called by the scaffolds (Scaffold::TryToBuildPlannedBuilding 0x6E9171, InterfaceSetInMagicHand 0x6E96E0, DestroyThingsInWay 0x6EAE2F)
- 0x517FE0: pct != 0 -> Game3DObject vt +0x110(pct) (fn_816AD0); at 0 nothing of the building is drawn (the temple's fn_816AD0(0) draws only the scaffold sunk by its whole height; PartialBuild draws nothing at 0, audit (g)). without resources (tests) nothing. TODO(the physics area Hito 2): the heart passes PartialBuildOptions {.innerOffset = PartialBuild::k_TempleInnerOffset.skipZero = false} (the 1.0 m walls)
- The Abode class of the original (Abode.cpp 0x401350..0x409000): every village building, the fields, the totem, the storage pit, the town centre and the spell dispenser included (openblack: `src/ECS/Abodes.cpp` and the abode archetype).
- Abode::GetNewEp 0x403590 (ABODE_EPP type): the first entrance point of that type of the abode's mesh (the NewEP block, graphics::L3DMesh::GetNewEntrancePoints) through the abode's world matrix (Object::GetWorldMatrix 0x638200); none without a mesh, without the block or without a point of the type
- The Abode class of the original (Abode.cpp 0x401350..0x409000): every village building, the fields, the totem, the storage pit, the town centre and the spell dispenser included. Only the interface tap lives here for now (milestone B8 of dev\tmp_dis\audio\PLAN.md): knocking on a roof.
- Abode::GetNewEp 0x403590 (ABODE_EPP type): the first entrance point of that type of the abode's mesh (the NewEP block, graphics::L3DMesh::GetNewEntrancePoints) through the abode's world matrix (Object::GetWorldMatrix 0x638200); none without a mesh, without the block or without a point of the type. The world matrix is made fresh from the MapCoords (x, GetAltitude + altitude, z), upright, with the angle and the scale (the cells of `LH3DObject::SetPosition` 0x423140, [the object's own placement](../engine/matrices-and-zoomer.md#the-objects-own-placement)); not the 3D object's matrix. openblack (`abodes::GetEntrancePoint`, 2026-10-28): `affine::PlacementMatrix` at the abode's map position (x and z on the map's grid) and its Transform's height, with its placed angle, then the point in the original's order; until then the Transform's model, at the unquantised position, with the cosine and the sine rounded before the scale
- Abode::CreateAbodeSurroundingObjects 0x403E00, the last step of Abode::Create 0x402E20: the did-you-know sign (ScriptHighlight row 1 at entrance point 1, with the info's didYouKnow / dykCategory) and the street lantern at point 0 unless one is within 40 m (no shipped mesh has a point 0)
- GetShouldNotBeAddedToPlanned vt +0x8F8 (Abode 0x401650) = +0x7C bit 2; false for anything that is not an abode
- SetShouldNotBeAddedToPlanned vt +0x8FC (Abode 0x401660): +0x7C bit 2 = b & 1
- Abode::MoveAbodeToPlannedAbodes 0x404520 (vt +0x90C, from ToBeDeleted 0x402C8A): no town -> false; not GetShouldNotBeAddedToPlanned (+0x7C bit 2, set by the scaffolds) and PlannedAbode::Create (Abode*) 0x405660 (plans::CreateFromBuilding: a rebuild plan, +0x30 = 1, when it was built) -> true; else the town's RemoveBuildingSite(this) 0x73BA20 and false
- IsBuilt vt +0x890: Abode 0x4016C0 = !(+0x58 & 2) && GetPercentBuilt (+0x5C) >= 1; a Feature 0x422110 (the same on its percentBuilt; openblack's Feature keeps no +0x58); CitadelPart 0x464AD0 (the citadel heart) and WorshipSite 0x77BDD0 the same as Abode's on components::CitadelPartBuild; any other MultiMapFixed 0x438D80 = 1
- IsRepaired vt +0x88C: Abode 0x4016A0 and CitadelPart 0x464AB0 = GetPercentRepaired (GetLife) >= 1; any other MultiMapFixed 0x438D70 = 1
- GetPercentRepaired vt +0x884 0x401500 = GetLife (vt +0x11C, ecs::life)
- GetPercentRepairedForNonFunctional vt +0x894: Abode 0x407290 = info +0x1B8 thresholdForStopBeingFunctional; MultiMapFixed 0x52EFC0 = 0.75 (also an abode without an info record)
- GetDestructionMesh vt +0x8B4 (Abode 0x401700 = +0x90 FragMesh): the physics' BuildingDamage with its FragMesh
- GetPercentRepairedFromWhenDamaged vt +0x888 0x52F010: not built -> 1; a DestructionMesh (+0x90, the physics' BuildingDamage) and a site: a = 1 - site +0x640, b = GetPercentRepaired - site +0x640, (a == 0 || b == 0) ? 0 : b / a; else GetPercentRepaired x 0.98 ([0x8CF3FC])
- SetShadowOnTexture (LH3DObject +4 bit 0x1000, vt +0x80 fn_7F9880) of a building: false for an abode without the built bit (+0x58 bit 8): CallVirtualFunctionsForCreation 0x52EA1E..0x52EA40 turns it off for an unbuilt one, MultiMapFixed::Built 0x52EC2C on again; true otherwise, drawn or not (RenderingSystem's CastsStaticShadow calls it; the physics area' CastsPhysicsShadow may)
- Abode::MakeFunctional 0x4047E0 (vt +0x914) and the class parts (StoragePit 0x732F30 Town::SetStoragePit 0x73EA60, Creche 0x50AB50 town +0x744, TownCentre 0x743E80 the totem and the spell icons; the graveyard's +0x748 (Town::SetGraveyard 0x73D6A2; Town +0x748 is written only by the Town constructors 0x73904D /
0x7393B5 and SetGraveyard, and read by Graveyard::MakeFunctional 0x595E1A, Graveyard::DeleteDependancys 0x595CF6,
Town::GetDesireToBeBuilt 0x73A4C8 / 0x73A533, Town::VillagerDead 0x73E47F, Save and Load)). The order in the .cpp
- MultiMapFixed::Repaired 0x52EC70 + Abode::Repaired 0x4047B0 (vt +0x8AC): the site's ToBeDeleted, RemoveDamage (vt +0x8B8, Abode 0x403F40: physics::Buildings::RemoveDamage), +0x58 &= ~4, MakeFunctional with a town. True
- Abode::IncreaseLife(x) 0x405ED0 (vt +0x5BC): wasAbove = vt +0x894 < life; Object::IncreaseLife 0x637870 (cap 1); !wasAbove && vt +0x894 < the new life -> RestartBeingFunctional (vt +0x91C 0x401680). Returns the new life
- RestartBeingFunctional vt +0x91C: Abode 0x401680 = `ret`; StoragePit 0x7339D0: Pot::RemoveReaction on its available piles (the food pile +0xC4, the five wood piles +0xC8)
- Abode::GetDesireToBeRepaired 0x406970 (vt +0x8D8, with MultiMapFixed's 0x52ECE0): town_desire::AbodeDesireToBeRepaired on this abode; 0 for anything else
- The partly built model of an abode with a building site and no DestructionMesh: MultiMapFixed::Draw 0x518090 -> DrawBuilding 0x517F90 draws fn_816AD0 at GetPercentForDrawBuilding (physics::PartialBuild::BuildMesh) into components::DrawMesh, and nothing at 0 (0x517FE0: components::NotDrawn; the footprint stays, SetFootPrintOnTexture 0x52EA33). The Mesh component stays the whole model (the 3D object's mesh: sizes, map cells, type). Rebuilt only when the percent changed; both go when IsDrawBuilding no longer holds. With a FragMesh the physics' RedrawBuilding draws it
- +0xA0 / +0xA4: the villagers of the abode, the head first (Abode::AddVillagerToAbode 0x40415A inserts at the head, next = villager +0xE4). Its order decides who moves in the shuffle (SwapMaleForFemaleFrom 0x407620, TakeVillagerFrom 0x4075B0) and who is the pair at bed time (CheckWhenGoingToBed 0x760BE4). Changed only by ecs::abode_villagers
- +0xA8 / +0xAC MaleFemaleVillagers[sex]: the first adult of each sex that moved in (AddVillagerToAbode 0x4041E4); RemoveDeletedVillagerFromAbode 0x404246 clears both, RemoveAliveVillagerFromAbode none (literal)
- PresentAtHome (+0xB6): villagers inside now; only Abode::ArriveHome 0x405FA0 (++) and Abode::LeaveHome 0x405FB0 (--) change it (from Villager::ArriveHome 0x751FBC, ExitAtHome 0x761B5F / LeaveHome 0x751FFE). Lights the chimney smoke (Abode::Draw 0x516288) and the night windows (0x515F78)
- +0xB9: counts up to 200 in each Abode::Process (0x404503..0x40450F); read by Abode::StopBeingFunctional 0x4073C8 (>= 200 with a player: the player's statistic, `game_stats::BuildingStoppedFunctional`)
- +0x58 bit 2: not repaired / a repair asked for (PlannedAbode::CreatePlannedNoFixedCheck 0x4057CC, ProcessTownRepairs 0x747E6E, MakeFunctional 0x404812 = !IsRepaired; cleared by MultiMapFixed::Repaired 0x52EC8D)
- +0x7C bit 1: TownStats::Add(Abode) 0x7498C0 has counted it (Abode::MakeFunctional 0x404818..0x40483C, once). true by default for the whole abodes (Abode::Init 0x403130 -> MakeFunctional when IsBuilt); false for a plan's until its MakeFunctional. town_stats::Compute counts only the abodes that have it
- +0x7C bit 2: Get/SetShouldNotBeAddedToPlanned (vt +0x8F8 0x401650 / vt +0x8FC 0x401660): the building is not planned again when it goes. Set by the scaffolds (TryToBuildPlannedBuilding 0x6E9171, InterfaceSetInMagicHand 0x6E96E0, DestroyThingsInWay 0x6EAE2F); read by MoveAbodeToPlannedAbodes 0x404520 (no rebuild plan when set)

### Chimney smoke details

- fn_007F8E00's globals, set by GLandscape::Draw (crt init 0x7F8DA0 / 0x7F8DD0: handWind (1, 0, 0), handPos (-10000, 0, 0); handSpeed in the bss, 0)
- [0xE9A110]
- [0xE9A124]
- 0x9A22E0
- 0x9A22DC, along +0xF8 = (0, 1, 0)
- 0x9A22E4: the hand within 15 units of the chimney
- 0x5DBD4E..0x5DBD5F: fild [0xD01A38]; fdivr [0x8AB228] = 1000, read every turn
- GLandscape::Draw 0x5E4310..0x5E448E (once per frame, before the objects are drawn): the hand position, wind and speed that fn_007F8E00 reads, from the hand's 3D object and GInterfaceStatus::HandVelocity (fn_005DBC60, advanced here once per game turn)
- Abode::Draw 0x516288..0x5162E0, for an abode on screen this frame: lit = PresentAtHome != 0, or a workshop making a scaffold (Workshop+0xC4 != 0). Returns whether the smoke goes to the Z-sorter (LH3DSmoke::AddDrawing: state != 3).

### Shown needs

- Workshop::GetShowNeedsPos 0x77A630 (vt +0xE4, any index): Game3DObject::GetSpecialPos(9) 0x63B060
- 0x719E98..0x719EAD: the 3D object's +0x60 and +0x5C = 0x2D. (pending) the two fields of the Game3DObject 0x719EB0..0x719EC1: +0x5C > ShowNeedGreater (+0x10C; test ah, 0x41 jne: else the reset below)
- 0x71A16C..0x71A19F: Zoomer::SetPosition(+0x5C) inline, nothing drawn
- 0x719ED0..0x719EEB: SetDestinationWithSpeedAndTime(fn_0071A200(), 0, 1.0) 0x407D60, every frame
- 0x719EF0..0x719F77: the advance by g_game_time_inc x 0.001
- 0x719F7E..0x719F90: the height = value x MaxHieght (+0x110)
- 0x719F99..0x719FAE: the owner's GetShowNeedsPos(index, &coords) (vt +0xE4); the coords' altitude = the height (0x719FB2); GetAltitude 0x803090 of them is computed and dropped (fstp st(0) 0x719FBF)
- 0x719FC1..0x71A02A: x, z of the coords (x 10 / 65536), the yaw toward g_camera: atan2(camera z - z, camera x - x) (fpatan of st1 = dz, st0 = dx) + pi/2 [0x8C78D8]; ConvertMapCoordToLandscapePoint 0x613750 (the land plus the altitude)
- 0x71A051: LH3DObject::SetPosition(point, yaw, 1.0) 0x423140
- 0x71A056..0x71A076: L = (MaxNeedValue - ShowNeedGreater) x 0.2; value < L (test ah, 1): the 3x3 scaled for the draw only by (value - ShowNeedGreater) / (L - ShowNeedGreater) (0x71A07C..0x71A0E4, restored at 0x71A116)
- fn_00719E80: +0x5C = desire (returns 1)

### Making an abode

- TownCentre::CreateTotemIfNecessary 0x743DA0 (a built town centre of a town, as MakeFunctional 0x743E80 calls it) -> TotemStatue::Create 0x737CC0: the tribe's plinth (GTotemStatueInfo, table 0xDA1D18 by Abode::GetTribeType) at the town centre's special point 6 (TownCentre::GetTotemPos 0x743F20, through its full matrix; raised like the morphing town centre, by the land under the point minus the land under the origin), with its Y angle and scale (0x737B20), and the icon on the plinth's top. Both static, no foundation sink (not an Abode). Facing the worship site (AddToPlayer 0x738130) and the creature's icon (SetPlayersCreature 0x7381C0) wait for those systems: the icon is the hand (BuildingSpellHand) of a player without a creature.
- GetExtraPos 0x80FF20 of the morphed town centre (IsStaticMorphable 0x81002E): (H(p) - H(pos)) + p.y (0x8100B7..0x8100D0)
- 0x743EAB: the town's +0x9A4 = this when still null (as CREATE_TOWN_CENTRE 0x71577C)
- fn_00552FF0 (the land script's CREATE_ABODE / CREATE_TOWN_CENTRE without their town, cases 8 and 9: 0x7156A6, 0x7157FF): the global town list, the first always taken, then GetDistanceInMetres < best. MapCoords x, z only (FromMetres: the altitude is not read)
- Abode::Abode 0x4013BB: Town::AddStructureToTown 0x7399A0 (at the head of +0x754, then SetTownArea 0x739A0A)
- Abode::Init 0x403130 -> MakeFunctional of a whole one (the script's): StoragePit::MakeFunctional 0x732F30 -> Town::SetStoragePit 0x73EA60 (town +0x30 = this, the last one wins; the temporary pots). A plan's at Built
- The Graveyard class (+0xC4 its dead, 0x595CB0..0x595FB0); a whole one's MakeFunctional 0x595E00 (+0x748, one dead); a plan's at Built
- Workshop::Create 0x779590: the Workshop part, its ShowNeedsVisuals' creation index ((not ported) the object) then its wood pile, in that order (CallVirtualFunctionsForCreation 0x7793E0, CreatePileWood 0x7795EA). A whole one's MakeFunctional 0x7797B0 (Abode::Init 0x403130: the town's list); a plan's at Built
- Abode::Create 0x402E20: CreateAbodeSurroundingObjects 0x403E00 last (0x40308E, after Init 0x403087): the did-you-know sign (its creation index after everything above) and the street lantern
- UnderConstruction: the MultiMapFixed ctor 0x52E1E0's flag (PlannedAbode::CreatePlannedNoFixedCheck 0x4057AC passes 1): +0x58 bit 1, +0x5C = 0, not counted by TownStats, and none of the MakeFunctional parts (storage pit, creche, graveyard, the town centre's totem and icons) until abodes::Built. The script's abodes are whole (0)

### The football pitch

A pitch is an abode of its own class (`Football`, bw1-decomp `Football.h`, `FootBall.cpp`), one record for every tribe
(6000 wood, 8 scaffolds, desire to be built 1.0, read from info.dat: [football progress](../project/progress/town/football.md#the-pitch)).

- **The switch.** `GGame::FootballEnabled` 0xD0196C is written only by `GGame::Init` 0x54F54F, from
  `BWCheckFeatureIsEnabled("football")` 0x5259D0 -> fn_00525A40: 0 in a network game, else 1 only when the Soccer
  Add-On's registry code is there ([not in the original](../project/not-in-the-original.md#townfootball)). Read once,
  as the game starts. Its three readers: `Scaffold::ValidToApplyThisToObject` 0x6E9924 (two scaffolds join while worth
  at most maxNumberForCombining 8, less one without football), `Town::GetDesireToBeBuilt` 0x73A4DE (the pitch's case
  returns 0 at once without football) and `Town::Process` 0x747414 (step 10 of [the town's turn](../beings/towns.md)).
- **A town building its own pitch.** Only a scaffold of eight offers it: `Town::GetNewPlannedBuilding` 0x73D980 weighs
  every record whose ScaffoldsRequired is at most the scaffold's worth, by `GetDesireToBeBuilt`. With eight, the
  pitch's case gives its desire 1.0 while the town's pitch (Town +0xEA4) is null and no site of the town builds one;
  the wonder's (seven scaffolds) gives 1.0 less the scaffold penalty (8 - 7) x 0.3, 0.7, and a building of fewer
  scaffolds less again, so the pitch is built where its model fits. `Town::CreateFootballPitch` 0x73A6A0 (a pitch made
  anywhere round the town) has no caller.
- **Town +0xEA4**, the town's pitch, is written only by `Town::SetToZero` 0x73C710 (null) and
  `Football::MakeFunctional` 0x531380 (this): nothing clears it when the pitch goes, so a town whose pitch has gone
  wants no other.
- **Becoming functional.** `Football::MakeFunctional` 0x531380: with a town, the town's pitch = this, Functional
  (+0x1DC) = 1, PlayState (+0x200) = 0; then `CreateBall` 0x5313E0 unless `GetBall` 0x531410 gives one that
  `IsAvailable` (`GetBall` itself makes one when +0x258 is null); then `Abode::MakeFunctional`. Football or not. A
  whole one (a script's) is made functional by `Abode::Init` 0x403177, a plan's at `Built`. `CreateBall`:
  `Ball::Create(&Pos, GBallInfo, 0, 0, 0)` 0x435B90: a football on the pitch's middle, given to no town.
- **Its turn.** The pitch is one of its town's structures (`Abode::Abode` 0x4013BB, `AddStructureToTown`), so the
  town's abode pass fn_00747600 calls its `Process` (vt +0x5FC, `Football::Process` 0x533E40) every processAbodeEvery
  (1) turns whatever the switch, and step 10 (0x74740A..0x74741E) calls it once more when the switch is on.
  `Football::Process` does not call `Abode::Process`, so a pitch being built never runs its site's turn. The match
  itself still steps once a game turn: it runs only while the game turn differs from a static, [0xCD0350], which it
  then takes (the first pitch to ask in a turn plays it, so with two pitches the second never plays); the ball's
  checks and the clean-up below run on every call. Below, the match.
- **No damage.** `Football::ReduceLife` 0x531E40 returns 1.0 and changes nothing, built or not.
- **The match's turn**, `Football::Process` 0x533E40, in order:
  - `GetBall`; more than 100 m from the pitch (`GetDistanceInMetres` [0x8AB41C]) or on water (`MapCoords::IsWater`),
    or the first thing in a player's hand (`GetLeaderInterfaceStatus`, `GetFirstObjectInCurrentHand`) and more than
    80 m off [0x8D060C]: the ball's `ToBeDeleted(0)` (vt +0xC), +0x258 = 0, `GetBall` (a new one), return 1;
  - once a game turn ([0xCD0350]): two debug messages ("Football State: %s", "Home (%d) v Away (%d)"); the Mexican
    wave: while +0x210 is 0, the place +0x21C (of +0x25C's 40) holding a watcher that is not moving (`IsMoving`
    vt +0x174) gets `SetTopState(84)`, ++ +0x21C, past 39 → +0x210 = 256, +0x21C = 0; else -- +0x210;
  - PlayState 0 (waiting): fewer than 3 a side → ++ +0x208, past ftol(T × 180) [0x8CF38C] (T = 1000 / the ms a turn,
    an integer) each player `SetTopState(36 GO_HOME)` and the ball's reactions removed (+0x208 is not reset, so this
    goes on every turn); three or more → ++ +0x208, past ftol(T × 30) [0x8BF51C]: the ball, unless a physics object
    (+0x24 & 0x40), `MoveMapObject(Pos)` (vt +0x55C), +0x204 = 1, +0x208 = 0; then with +0x204 and fn_00532900
    (fewer than 5 of both sides moving) → PlayState 1, +0x308 = ftol(T × 15) [0x8DDF88], fn_00534AA0 and fn_005346F0 on
    both ranked lists;
  - PlayState 1 (playing): no home or no away player, or one each → PlayState 0. Else, with +0x2FC (a town match):
    ++ +0x208, and the town's first sorted desire (`GetSortedDesire(0)`) neither 15 RELAXATION nor 2 PLAYTIME with
    +0x208 over 0x708 → +0x1EC = the turn, `StopFootballGame` 0x531FD0 (and the turn goes on). Then `GetBall`;
    `ProcessBall` 1 (along the pitch): +0x254 = 0, `SortList` (0x534910) of both sides, fn_00534AA0, +0x308 under
    ftol(T × 15) → ++, else fn_005346F0 on both and +0x308 = 0; fn_005347A0 (each ranked player's
    `AssignFootballSubState`) on both. Off the pitch: of the two sides' first players the one further from the ball
    (`FastDistance`; on a tie the away one) fetches it; every other player who is not the fetcher nor +0x254, while PlayState is not 3 and its top state not 82, gets
    `SetTopState(82 FOOTBALL_MATCH_PAUSED)`; with no +0x254 yet: the fetcher's `StartMoveToObject(ball, 79
    ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL)`, +0x254 = it, +0x20C = 0 and on to the clean-up; else +0x20C = 0;
  - PlayState 3 (after a goal): ++ +0x300 past 70 → PlayState 1, +0x308 = ftol(T × 15);
  - every call, the clean-up: the ball unavailable → +0x258 = 0 and fn_00531C20 (outside a script's cup final: each
    player `SetTopState(36)`); the referee +0x220, both sides' players, both ranked lists, +0x244/+0x248 (the nearest
    attackers), +0x24C/+0x250 (the nearest defenders), +0x254 and the 40 places each let go when unavailable (the
    places' count +0x218 stays); also the cup final's slots +0xC8 (10) and +0xF0 (20) and +0x140, which only a
    script's match fills.
- fn_00534AA0, the ranking: each side's ranked list (+0x234 home, +0x23C away) emptied, then each player in the
  side's order goes before the first ranked one strictly further from the goal it attacks (`FastDistance` to
  `GetGoalPosition(side == home)`: the home side attacks the second goal mouth), else last.
- fn_005346F0, the positions: in a ranked list, the last keeps goal (74 'J'), the first half (count >> 1) attack (73
  'I', villager +0x11A = its place) and the rest defend (75 'K', +0x11A = its place less the half); a player whose
  final state is not its position's gets `SetTopState`, its counter (+0x58) 20 and +0x118 = 1 (choose again); then
  `AssignFootballSubState`. With 5: 2 attack, 2 defend, 1 keeps goal; with 3: one each.
- `SortList` 0x534910: a ball unavailable or none → `CreateBall`; the side bubble-sorted by `FastDistance` to the ball
  (a stable sort, nearest first); then its attacker nearest the ball into +0x244 + side × 4 (only one strictly nearer
  than 0xFFFFFFFF), and its defender nearest the ball into +0x24C + side × 4, the search starting from the nearest
  attacker's distance (only a defender nearer than every attacker is taken); a side with none keeps its old one.
- `StopFootballGame` 0x531FD0: each player `LivingAction::SetState(TOP, 163)`, `GoHome`, flags &= ~0x80; each of the
  40 places' watchers `GoHome`; the town's belief (`GBelief::AddToBelief` of Town +0x798, the pitch's player, the
  pitch, 1, guidance 1) of min(+0x1E8 / ftol(ftol(T × footballMatchDuration) × T), 2) × beliefForFooballMatch (GTownInfo
  +0x104, +0x108); +0x1E8 is written only by `SetToZero`, so the share is 0 (0 / 0, not a number, with a zero
  duration); PlayState 0; `SetToZero`.
- `Football::SetToZero` 0x531CE0 empties the match (both sides, the ranked lists, the places and their count, the
  wave's clocks, the tallies, +0x204, +0x208, +0x20C, the referee, the nearest attackers and defenders, the cup
  final's slots), forgets the ball (+0x258 = 0: the ball stays where it is and the next `GetBall` makes another),
  +0x1EC = 0 (so the last match's turn `Process` has just set is lost: the pitch is wanted at 1 again at once),
  +0x2FC = 1, and the pitch's flags |= 8. It keeps PlayState, +0x300, +0x304, +0x308 and +0x254.
- **A kick**, `Ball::KickBallAtDestination(destination, speed, flag)` 0x435C40: from the ball's LHPoint to the
  destination's, d = to - from, |d| = sqrt((dz² + dy²) + dx²), t = |d| / speed, the fall h × -9.81 [0x8C57D0] with
  h = (t × t) × 0.5, the velocity ((dx - 0), (dy - h × -9.81), (dz - 0)) × (1 / t); a ball already a physics object
  (+0x24 & 0x40) `PhysicsObject::RemoveObject(ball, 1, 1)` first; off the map (vt +0x178) → 0; `InitialisePhysics`
  (vt +0x784) with that velocity and no spin; its physics object's flags |= 2, `AdjustToGroundLevel(0, 1)`; Ball +0xB4
  = the flag (1 from the players' kicks, 0 from the dead ball's restart: only a ball kicked with 1 can score) and
  +0x84 = the destination. `sub_435DF0`, a second kick, has no caller.
- **The creature and a match.** The creature action table has eight match actions, 220 to 227, each done to a
  thing whose `IsPlayingFootball` (vt +0x364, the Villager's 0x532DB0: the final state's exit is `ExitFootball`)
  holds, or for 226 / 227 `IsPlayingFootballAndMySideHasJustScored` 0x532E00 / `...OtherSideHasJustScored` 0x532E50
  (PlayState 3 and +0x304 equal to, or not, `Creature::IsOnHomeTeam` 0x47C690 = ~player number & 1: an even number
  plays for the home side). Only two are in a desire's row of the desire action table (info.dat, read 2026-11-03):
  Play (desire 3) lists 224 GoalieCatchBall and 225 GoalieFoul; no row of it, nor of the town table, lists 220 to
  223, 226 or 227, so nothing but a script's forced action could start them, and no shipped script forces one.
  - `ConstructSubActionsForAttackerThrowBallAtGoal` 0x4ACA70 (also 221, 223 and 224's builder: 0x4ACCB0, 0x4ACDB0,
    0x4ACDC0 call it): the plan's villager's football and ball (none -> fails); the goal `GetGoalPosition(
    IsOnHomeTeam)`; with nothing carried (physical +ObjectCarried 0), `GameRand(3)` != 0 -> INDIVIDUAL_ACTION 0x43
    (FEEL_PLAYFUL), then PICKUP of the ball looking while going towards it; then the main THROW_AT_POS 0x38 at the
    goal's LHPoint.
  - `ConstructSubActionsForGoalieFoulAttacker` 0x4ACDD0: football and ball (none -> fails); the other side's first
    player (`IsOnHomeTeam` 0 -> the home list +0x224, else the away list +0x22C; an empty side fails); the main STOMP
    0x11 on it, looking while going towards it, with the anger face.
  - `ConstructSubActionsForDefenderStompOnBall` 0x4ACCC0: STOMP on the ball; `...CelebrateGoal` 0x4ACF70 and
    `...CommiserateGoal` 0x4AD000: INDIVIDUAL_ACTION 0x37 (happy) and 0x38 (sad). Not reached (above).
- **Taking villagers up.** `Football::SetVillagerActivity` 0x532190 (from `Town::SetVillagerActivity` 0x73FF10):
  `GetBall`; +0x2FC (1 from `SetToZero` 0x531CE0) == 0 or no ball -> 0. Then fn_00532730: no referee (+0x220) -> the
  villager is the referee, and walks nowhere; else fn_00532500(villager, 1): the home side (+0x224, count +0x228) while
  under 5, else the away side (+0x22C, count +0x230) while at most 4, each at the list's head, and
  `Villager::SetupMoveToPos(spot, 72 FOOTBALL_WAIT_FOR_KICK_OFF)`; both full -> fn_00532500(villager, 0) refuses too
  and fn_00532760 makes a watcher: fn_005322E0 takes, while fewer than 40 places (+0x218) are taken, the first free of
  the 40 (+0x25C, fn_005326A0, counted) and the angle (the count after) x 0.15707964 [0x8D8720], else
  `GameFloatRand(2 pi)`; `Object::GetNearestEdge(angle, 2.0)` 0x636DF0; then x and z each + `GameFloatRand(4)` - 2;
  `SetupMoveToPos(place, 83 FOOTBALL_WATCH_MATCH)`. Last: home + away == 1 -> +0x208 (the turns waited for players)
  = 0; the villager's flags |= 0x80 (0x5321F9) and its +0x11C = the pitch; returns 1.
- **The player's spot**, fn_00532400: x, then z, = ftol((p x 10 x 1/65536 + (`GameFloatRand(8)` - 4)) x 65536 / 10)
  (0x53242C..0x532494), every step a float; the altitude kept. The oracle: the middle (1677721, 2621440) and the
  campaign seed 0x0088F89F give (1687509, 2633972).
- **Waiting for the kick-off**, state 72's function `Villager::WaitForKickOff` 0x763140: PlayState == 1 -> top state
  FOOTBALL_ATTACKER; returns 1.
- **The players' substates.** `Villager::AssignFootballSubState` 0x75D130 by the final state: the attackers' table
  [0xC237B8] (8 rows), the defenders' [0xC23BB8] (8), the goalie's [0xC23FB8] (7), rows of 0x80 bytes: +0 the first
  precondition, +0x10 an optional second, +0x20 the priority, +0x30 the process (and the names). The assignment
  (0x75D170, 0x75E0B0, 0x75EAB0) runs only with the choose-again byte (villager +0x118): best = the row count,
  best priority -0.0001; each row's first precondition, then its second (false skips the row), then, if the first
  held, its priority, strictly greater wins; the substate (action +3) = best, +0x118 = 0, the substate's turns
  (+0x119) = 0. The rows:
  - attacker: Shoot, Pass, LobNearGoal, Dribble (IsNearestBall); MoveToBePassed (IsNotNearestBall); GoToBall
    (IsNotTouchingBall); GoHome (IsNotTouchingBall, IsNotAtHome); Idle (IsNotQuiteNearBall);
  - defender: Save, Clear, Dribble (IsQuiteNearBall); Pass (IsNearestBall); Mark, GoToBall (IsNotTouchingBall);
    GoHome (IsNotNearestBall, IsNotAtHome); Idle (IsNotQuiteNearBall);
  - goalie: Save, Clear (IsQuiteNearBall, IsAtHome); Look (IsAtHome); Pass (IsQuiteNearBall); GoToBall
    (IsNotTouchingBall); GoHome (IsNotAtHome); Idle (IsNotQuiteNearBall, IsAtHome).

  The preconditions (0x75F070..0x75F230): touching = `IsTouching(ball, GetHeight())` (vt +0x6B8, +0x42C; the
  touching one is in no table); nearest = `GetHowCloseToBallIndex` 0x5347C0 (the place in the side, as last sorted)
  == 0, not nearest = it > 0 unsigned (-1, on neither side, counts); at home under 2 m and not at home over 2 m from
  fn_00532F60; quite near within 5 m and not quite near over 5 m from the ball.
- **The positions' states**, 73 `FootballAttacker` 0x75D230, 74 `FootballGoalie` 0x75EA10, 75 `FootballDefender`
  0x75E170: no football or `GetBall` 0 -> `DecideWhatToDo`; (short) the counter (+0x58) under 1 -> ++, 1; a substate
  past the table (7, the goalie 6) -> 1; else the row's process, then its first precondition false -> +0x118 = 1; 1.
- **The processes** (bw1-decomp `VillagerFootball.cpp`; at the ball = touching and `GetHeight` over the ball's
  altitude; each kick `KickBallAtDestination(.., 1)`, then the counter 0 and +0x118 = 1; not at the ball: GoToBall):
  - Shoot: the goal attacked (`GetGoalPosition(IsPlayerOnHomeTeam)`), `PerturbTarget` 0x532EB0, speed
    `GameFloatRand(4)` + 12. LobNearGoal: the goal attacked + (`GameFloatRand(4)` - 2) on x then z, `PerturbTarget`,
    `GameFloatRand(1)` + 3. Dribble (attacker and defender): a fifth of the way to the goal attacked, `PerturbTarget`,
    `GameFloatRand(1)` + 6. Pass (0x75D3A0, 0x75E730, 0x75EE10): the team-mate at `GetHowCloseToGoalIndex` 0x534820
    less 1 in its own ranked list (+0x234 / +0x23C; none below 0), at its `GetDestPos` (vt +0x860) when `IsMoving`
    (vt +0x174) else its position; kicked only when that is more than 1 m from the ball, `GameFloatRand(1)` + 6; +0x118
    = 1 either way. The goalie passes only within 10 m of its home spot (else +0x118 = 1).
  - MoveToBePassed 0x75D850: the counter 0; from its own goal (`GetGoalPosition(!home)`) towards the goal attacked by
    `GameFloatRand(0.2)` + 0.6 (in double), across along the way's normal (-dz, ?, dx) by `GameFloatRand(18)` - 9 (the
    normal's middle word is never written: inferred 0); `SetupMoveToPos(.., 73)`; +0x118 = 1.
  - GoToBall (0x75DA90, 0x75E5A0, 0x75ECD0): at the ball -> +0x118 = 1; a ball in flight (+0x24 & 0x40) and
    `GameRand(3)` != 0 (the goalie `GameRand(10)` >= 8) -> `SetupMoveToPos(Ball +0x84, the position's state)` and the
    raw `SetState(TOP, 222)`; else `StartMoveToObject(ball, the position's state)`.
  - GoHome: `SetupMoveToPos(fn_00532F60, 73 / 75)`, the goalie `SetupMoveToPos(GetGoalPosition(!home), 74)`; +0x118
    = 1. Idle (attacker, defender): `LookAtObject(ball, NORMAL)`; the counter at least 600 or the ball place under 2
    (unsigned) -> +0x118 = 1. The goalie's Look 0x75EC60 (and Idle): `LookAtObject(ball, NORMAL)`, ++ +0x119 past 20
    or the ball under 6 m -> +0x118 = 1.
  - Save (defender; Clear the same): the goal attacked + (`GameFloatRand(10)` - 5) on x then z, no perturbing,
    `GameFloatRand(3)` + 8. The goalie's Save (Clear the same): the pitch's middle, `PerturbTarget` three times,
    `GameFloatRand(1)` + 9. Mark 0x75E560: `StartMoveToObject(+0x244 [IsPlayerOnHomeTeam], 75)` (the other side's
    attacker nearest the ball) when there is one; +0x118 = 1.
  - The priorities: Shoot 0x75DC20 clamp(1 - clamp(d goal, 0, 40) x 0.025, 0.1, 1) x the angle's share (|a - b|
    folded to 0..0x400, >> 11: always 0, so the share is 1); Dribble clamp(clamp(d goal, 0, 40) x 0.025, 0.1, 1);
    MoveToBePassed clamp(clamp(d ball, 0, 20) x 0.05, 0, 0.2); GoHome clamp(clamp(d home, 0, 20) x 0.05, 0.1, 1);
    Idle clamp((1 - clamp(d home, 0, 10) x 0.1) x (clamp(d ball, 10, 40) - 10) x 0.025, 0, 1); Pass
    `GameFloatRand(0.2)` + 0.8 with a team-mate ahead, else 0; Lob and GoToBall 0; the defender's Save
    `GameFloatRand(0.3)` + 0.7, Clear `GameFloatRand(0.4)` + 0.6, Mark `GameFloatRand(1)`, Idle 1 - GoHome; the
    goalie's Save, Clear and Look `GameFloatRand(1)`, Idle 1 - GoHome.
- **The home spot**, fn_00532F60(villager, side = !home): by the final state, 73: fn_00533000 (rows of 5 by +0x11A,
  across fn_00533120(place, ranked count >> 1, 0.8), along 0.33 - rows x 0.1), 74: `GetGoalPosition(side)` (its own
  goal), 75: fn_00533090 (rows of 3, fn_00533120(place, (count - 1) >> 1, 0.5), along -0.76 - rows x 0.1); others the
  pitch's middle. fn_005331A0 lays the shares from the middle towards `GetGoalPosition(side == 0)`: along the way, and
  across it on (-dz, 0, dx). fn_00533120(i, k, w): k 1 -> 0, k 2 -> (i - 0.5) x w, else w / (k - 1) x i - w x 0.5.
- **The other football states**: 76 `FootballWonGoal` 0x5319C0 and 77 `FootballLostGoal` 0x5319D0 return 1 (the
  positions come back with the next fn_005346F0, the 15 s counter full after the goal's pause); 78
  `StartMoveToPickUpBallForDeadBall` 0x75F270 (football and ball -> `StartMoveToObject(ball, 79)`, 1, else 0); 79
  0x75F2C0 `LookAtObject(football, FAST)`, `SetTopState(80)`; 80 `PlayAnimThenSetState(81)`; 81 0x75F300 at the ball
  -> `KickBallAtDestination(football Pos, 10, 0)`, `StartMoveToObject(ball, 79)`, `SetPlayState(1)` 0x534860 (the
  counter +0x308 = ftol(T x 15)); 82 `FootballMatchPaused` 0x763170 `LookAtObject(ball, NORMAL)`; 83
  `FootballWatchMatch` 0x7631F0: no town or no ball -> `DecideWhatToDo`; PlayState 0 -> `LookAtObject(football,
  NORMAL)`, 1 -> `LookAtObject(ball, NORMAL)` (each returned), 4 (never set) -> flags &= ~0x80 and `DecideWhatToDo`
  not 1 -> `GoHome`; 84 `FootballMexicanWave` 0x7631B0: no town -> `DecideWhatToDo`; `LookAtObject(football, FAST)`,
  `PlayAnimThenSetState(83)`; 222 `FootballMoveToBall` 0x75F380: football, ball and `IsAvailable` -> the ball's
  destination more than 1 m from `GetDestPos` -> goal = it, `InitStepsXZ`; `MoveTo`, 10 -> `SetTopStateToFinal`; the
  result; else `DecideWhatToDo`.
- **The football clips** (the state table's animation functions, called as a state's clip is chosen):
  `FootballWatchMatchAnimation` 0x423C80 (state 83): `GetTown` (vt+0x48) null or `GetFootball` 0x751EA0 null -> 0x110
  P_GOSSIP_MAN, no draw; the pitch's PlayState (+0x200) 2 -> 0xE8 P_CROWD_UNIMPRESSED_1, no draw; 3 (a goal) ->
  `GameRand(3)` (Animations.cpp line 0x21B) 0 0x182 P_STAND_DESPAIR_1, 1 0xEA P_CROWD_WON, 2 0xEB P_CROWD_WON_2; else
  `GameRand(3)` (line 0x228) 0 0x18B P_TALKING_AND_POINTING, 1 0x18C P_TALK_TO_NEIGHBOUR, else 0x110. PlayState 2
  ("Paused" in the debug names at 0x40 apart from "WaitingForPlayers") is never written in this exe: its writers are
  0 (the constructor, `MakeFunctional`, `StopFootballGame`, `Process`), 1 (`Process`, `SetPlayState` 0x534860, called
  only from 0x75F36E with 1) and 3 (`GoalHasBeenScored` 0x531940), so the 0xE8 branch is never reached.
  `FootballWonGoalAnimation` 0x423DA0 (`GameRand(100)` < 25 0xE3, < 50 0xEA, < 75 0xEB, else 0xE8) is in the exe but
  never used: state 76's animation slot (StateTable +0x2B20 in `_$E64` 0x59D020) is 0, no call or pointer to 0x423DA0
  is in the exe, and bw1-decomp's GStates.cpp puts it in the row only for the Creature Isle build
  (`VERSION_BW1WCI`). A celebrating player takes the table's clip of state 76, with no draw.
- **The exit**, `Villager::ExitFootball` 0x763280 (exit 21 of rows 71 to 84): the circle hug reset; the next state's
  exit the same (`IsStateExitFunctionSameAs`) or 1 / 2 -> 1; else +0x254 == it -> 0; `RemoveVillagerFromTeam` 0x5325D0
  (the home side's if it is there, else the away side's; it always answers 1, so `RemoveVillagerFromMexicanWave`
  0x5326E0 is never called and a watcher's place stays until it is gone); flags &= ~0x80; 1. The ranked lists keep
  the player until the next ranking.
- A script's ID_SIZE of a pitch is its two sides' players.
- **The goals.** The goal model `Data\football.l3d` (loaded once by the first pitch's
  `CallVirtualFunctionsForCreation` 0x530F80 into [0xCD01D0]) keeps a box list at its header's +0x34 (the file's
  offset 76): a count, 2, then boxes of 0x20 bytes (a dword, the centre, the half-extents, the half-diagonal): the
  first goal mouth at x 13.555, the second at x -13.802, each 0.14 deep, 1.35 high (half) and 2.6 wide (half).
  `GetGoalPosition(i)` 0x532C80: box i's centre through the pitch's drawn matrix (G3D +0x14:
  x = ((m6 z + m3 y) + m0 x) + m9, y = ((m7 z + m1 x) + m4 y) + m10, z = ((m8 z + m2 x) + m5 y) + m11), as a MapCoords;
  the pitch's Pos when the list has no box i (with no list, box 0 is the model's own box).
- **The pitch's own space**, fn_00531A00: from Pos (x, z × 10/65536, y = GetAltitude + the altitude), GetYAngle and
  GetScale: no turn, the scale on the diagonal; else c s and s s each rounded once (`fcos`/`fsin` unrounded), m0 =
  0 + c s, m2 = 0 + s s, m6 = 0 - s s, m8 = c s - 0, m4 = s. Its inverse is `LHMatrix::SetInverse` 0x7FB3F0 (in place:
  the translation sums -((r6 t11 + r3 t10) + r0 t9), -((r7 t11 + r4 t10) + r1 t9), -((r8 t11 + r5 t10) + r2 t9),
  not the order of 0x7FB290; the oracle pins it).
- **The ball's turn on the pitch**, `Football::ProcessBall` 0x531E50: the ball's LHPoint into Ball +0x6C; that point
  through the inverse (x = ((m6 z + m3 y) + m0 x) + m9, y = ((m7 z + m4 y) + m1 x) + m10, z = ((m8 z + m5 y) + m2 x)
  + m11); while PlayState is 1, `IsInsideGoal(ball, 0)` → `GoalHasBeenScored(1)` and ++ the byte +0x214, else
  `IsInsideGoal(ball, 1)` → `GoalHasBeenScored(0)` and ++ +0x215; the point again into Ball +0x78; then fn_005324D0:
  1 when -h <= x <= h, h the pitch model's half-length (mesh +0x24, fn_00533BB0's box): only the length is tested.
- **A goal**, `Football::IsInsideGoal` 0x532990: 0 unless Ball +0xB4 (set by a kick) is non-zero; both of the ball's
  points (+0x78 the turn before, +0x6C now) through the inverse (here x = ((m3 y + m6 z) + m0 x) + m9,
  y = ((m1 x + m7 z) + m4 y) + m10, z = ((m5 y + m2 x) + m8 z) + m11); the heading (+0x30C..+0x314) = d / |d|,
  |d| = sqrt((dz² + dx²) + dy²), kept only at 0.0001 or more; then box i: the way crosses the plane x = the box's
  centre x (either way; starting on it and ending short of it is a goal at once), and there y ≤ the half-height (no
  lower bound) and -half-width ≤ z ≤ half-width; a goal also puts "GOAL!!!!!" in the players' text messages
  (`GGlobal::AddPlayerTextMessage` 0x591010, with a beep).
- `Football::GoalHasBeenScored(side)` 0x531940: PlayState 3, +0x300 = 0, +0x304 = side; each home player
  `SetTopState(76 + (side != 0))`, each away player `SetTopState(76 + (side == 0))`: a goal in the first mouth
  (side 1) has the home side mourn (77) and the away side celebrate (76), yet adds to the home tally.

### Sources

- Disassembly of `runblack.exe` the exe.
- bw1-decomp `src/Black/Abode.h`, `GameThingWithPos.h` (Flags +0x24), `Town.h`.

## openblack

Status: **partial**. Progress rows: [abodes](../project/progress/building/abodes.md) (85%),
[civic buildings](../project/progress/building/civic_buildings.md) (80%),
[damage and repair](../project/progress/building/damage_and_repair.md) (92%), and [the town](../project/progress/town/).

- `src/ECS/Abodes.{h,cpp}`, `src/ECS/Components/Abode.h`, `src/ECS/Archetypes/AbodeArchetype.{h,cpp}`,
  `src/ECS/ShowNeeds.{h,cpp}` (the floating numbers) and `src/3D/ChimneySmoke.cpp`;
- a building's and a town's state lives in components (`components::Town`, `components::TownBelief`, the abode
  components) or Locator services; the desire, the belief fold and the builder ring are pure functions tested with
  fakes.

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#world).

#### Resources held by objects

- openblack's float division and cube (`store_rules::LastTakenModifier`, fed by `town_stores` with 0 as never) give the same value for turn differences below 2^24.

#### The floating numbers (ValueSpinner)

- openblack: the component `FloatingNumber` (ECS/Components/FloatingNumber.h) and its rules (ECS/FloatingNumber.h: `Step`, `Alpha`, `TextPlace`, `ScreenPixel`), stepped every frame by `Game` with `FrameGameSeconds()`; `Game`'s `FillFloatingNumbers` puts those on the screen, furthest first, into `OverlayFrame::floatingNumbers` (none with the wide screen on, nor in the temple), and `Renderer::DrawFloatingNumbers` draws them in font j0 in the `FinishFrameIcons` view after the input prompt icons' first draw, under the bars and the fade, as the Z-sorter's flush at the start of the end of the frame does. The believers gained (`town_belief`) and the prayer power of an altar's sacrifice (`AltarOffering.cpp`, in red, 6c6a19c4e) make one so far.

#### Life and damage

- openblack `abodes::OnPhysicalDamage`
- What a broken building does to its town: the alignment and the town's anger go through the effect
  (`effects::ApplyEffect`, `src/ECS/Effects/EffectValues.cpp`: `alignment::Update`, `town_emergency::AttackTown`; test
  `RepairsTest.AnAttackedObjectsTownIsAttackedByItsAggressorValue`); a creature-applied effect moves no alignment yet
  (the TODO there). StopBeingFunctional 0x4073C0 is `abodes::StopBeingFunctional` (`src/ECS/Abodes.cpp`, from
  `ReduceLife` below the info's threshold): the player's statistics for a building 200 turns old or more
  (`game_stats::BuildingStoppedFunctional`), a storage pit's piles that hold something set up their pot reactions
  again, a town centre sets its town's worship share to 0, and the villagers stay. Tests
  `RepairsTest.TownCentreStopBeingFunctionalStopsTheWorship` and
  `RepairsTest.ASettledBuildingAPlayerStopsCountsInThatPlayersStatistics` (`test/test_repairs.cpp`). The villager side
  of a hit house is in [Villagers: data and states](../beings/villager-states.md) (the tap from Abode::ReduceLife).
- openblack's town lists are the abodes with the town's id (town_stats::AbodesOf, town_queries::TownAbodes, TownCentreOf, TownSystem::FindAbodeWithSpace, the influence), so the id goes and a zombie abode is in none of them.
- openblack deletes a town left with no building, plan or villager (`ecs::town_deletion`, 87c47d897; [Notes on the port](#notes-on-the-port))
- openblack keeps no such list
- openblack's citadel is the heart itself (worship::citadel::HeartOf) 0x464D1B..0x464D52: out of the hearts' list [0xC5E3D4] (next +0xD8): the Each<CitadelHeart> readers.
- openblack's plans are not entities

#### The knock on a building

- openblack: `AbodeKnockSystem` (`Locator::abodeKnockSystem`, raffclar's slot) with `abode_knock::IsTappable` (the village centre refused, f82881887), the read-out on game time with float products (bfbd591ed) and the held villager's home (`components::Abode::heldVillagersHome`, fe2ddeff3). Holding a villager near a house (`DiscipleInHandNear` 0x407420) does not bring the read-out up yet: the disciple's port (`ecs::villager::DiscipleInHand`,
  `src/ECS/Villager/VillagerDisciple.cpp`) reads the house only for the disciple's choice (queued fix F-16).

#### Abode details

- openblack's towns always have one: NEUTRAL

#### The football pitch

- 🎯 Chosen difference (2026-11-02): football is on, as if the Soccer Add-On were installed, outside network games
  only, as the original's switch (openblack plays no network game). `EngineConfig::footballAddOn` (on) is read once as
  the game starts into `EngineConfig::footballEnabled` (`ecs::football_rules::EnabledAtGameStart`, in `Game`'s
  constructor); the debug bar's World menu switch "Football off (no Soccer Add-On)" turns it off, as the original
  without the add-on. Tests `FootballRules.FootballIsOnWithTheAddOnOutsideNetworkGamesOnly`,
  `BuildingSitesTest.FootballOffIsZero`.
- Its three readers: `scaffolds::ValidToApplyThisToObject` (`BuildingSitesTest.WithFootballScaffoldsJoinUpToEight`),
  `plans::GetDesireToBeBuilt` (`BuildingSitesTest.WithFootballATownWantsAPitchWhileItHasNoneAndNoneIsBeingBuilt`,
  `BuildingSitesTest.WithFootballATownThatHasHadAPitchWantsNoOtherEvenOnceItHasGone`) and step 10 of
  `town_process::ProcessTown`.
- The pitch: `ecs::football` (`src/ECS/Town/Football.{h,cpp}`) on `components::FootballPitch`, which `AbodeArchetype`
  gives every pitch; `components::Town::footballPitch` is Town +0xEA4 (`football::PitchOf` reads a gone pitch as none,
  where the original keeps a freed pointer; the desire reads the field itself). `football::MakeFunctional` runs first
  in `abodes::MakeFunctional` (`BuildingSitesTest.APitchMadeFunctionalIsItsTownsPitchAndKeepsABallStillAbout`);
  `abodes::ReduceLife` answers 1 for a pitch (`BuildingSitesTest.APitchTakesNoDamage`). The town's abode pass runs
  `football::Process` for a pitch instead of the abode's turn, and step 10 once more with football.
- The kick: `ecs::ball_kick::KickBallAtDestination` (`src/ECS/BallKick.{h,cpp}`) on
  `football_rules::KickVelocity` (test `FootballRules.AKickLandsOnItsTargetAfterItsDistanceOverItsSpeed`): the body
  through `DynamicsSystem::AddObject` with no spin, `k_PushedByLiving`, `Body::SettleOnLand(ground, false, true)`.
  🔍 inferred the removal of a ball already in flight is `RemoveObjectWithEndPhysics`, which puts it back on the map.
- The match's turn: `football::Process` (the ball lost, `MatchTurn` once a game turn through
  `RegistryContext::footballTurn`, which `Game` carries from land to land, and `LetGoOfWhatHasGone`), with
  `WaitForPlayers`, `Play`, `RankSides`, `AssignPositions`, `SortSide`, `StopGame` and `SetToZero`
  (`src/ECS/Town/Football.cpp`) on the pure `football_rules::TurnsFor`, `MatchBelief`, `RankOrder` and
  `PositionForRank` (tests `FootballRules.TheMatchsClocksAreWholeTurns`,
  `AMatchsBeliefIsItsCountOverTheDurationTwiceInTurnsAtMostTwice`, `ASideIsRankedNearestTheGoalFirstAnEqualOneAfter`,
  `FivePlayTwoTwoAndOneThreePlayOneEach`). ⚠️ approximate only the local player's hand is asked about the ball; the
  debug messages, the pitch's flag 8 and the cup final's slots are not kept.
- The players: `football::AssignSubstate` calls `villager::AssignFootballSubState`
  (`src/ECS/Systems/Implementations/VillagerFootball.{h,cpp}`: the three tables, states 73 to 84 and 222, `ExitFootball` on rows 71
  to 84 and MOVE_TO_POS's exit on 222, wired in `LivingActionSystem`), on `football::IsPlayerOnHomeTeam`, `BallRank`,
  `GoalRank`, `RankedPlayer`, `NearestAttacker`, `PerturbTarget`, `HomePosition`, `SetPlayState` and `PlayerLeaves`,
  and the pure `football_rules::ChooseSubstate`, `ShootPriority`, `DribblePriority`, `MoveToBePassedPriority`,
  `GoHomePriority`, `IdlePriority`, `AcrossShare`, `AttackerShares`, `DefenderShares`, `FormationSpot`,
  `MoveToBePassedSpot` and `DribbleSpot` (tests
  `FootballRules.APlayersWishesGrowAndShrinkWithItsDistancesWithinTheirLimits`,
  `APlayersHomeSpotIsItsRowsShareAcrossAndAlongTheWayToTheGoal`, `AnAttackerWaitsForAPassUpTheWayAndToItsSide`,
  `ADribbleKicksTheBallAFifthOfTheWayToTheGoal`, `APlayerPlaysTheFirstHeaviestSubstateItsConditionsAllow`). The
  substate is `Villager::footballSubstate`, the choose-again byte `footballChooseAgain`, the counter
  `LivingAction::turnsUntilStateChange`. (openblack, guard) a pass with no team-mate at the place, and the dead ball's
  last step with no pitch or ball, do nothing where the original reads through null. The watchers' clip is
  `villager_animation::FootballWatchMatchClip(play state, random)`, given the pitch's play state by
  `VillagerAnimate` (none with no town or no pitch; test `VillagerAnimation.AWatcherLooksByWhatTheMatchIsDoing`); the
  never-set paused state has no value in `FootballPitch::PlayState`, so its branch is not ported. The won goal takes its
  table clip, as the original's state table (no clip function).
- The villagers: `football::SetVillagerActivity`, from `ArtefactSystem::OfferRelaxation` (the town's pitch weighed
  first, by `football::GetVillagerActivityDesire`), with the pure rules `ecs::football_rules` (`ChooseRole`,
  `PlayerSpot`, `WatcherAngle`, `WatcherPlace`; `test/test_football_rules.cpp`); state 72 is
  `football::WaitForKickOff`; the watchers' and the wave's states are `villager::FootballWatchMatch` and
  `FootballMexicanWave`.
- The goals: `football::GoalPosition`, `ProcessBall`, `IsInsideGoal` and `GoalHasBeenScored` on the pure
  `ecs::football_geometry` (`src/ECS/FootballGeometry.{h,cpp}`: `PitchMatrix`, `InverseInPlace`, the two point
  orders, `CrossesGoalMouth`, `AlongThePitch`, `BallHeading`, `GoalPointInWorld`; `test/test_football_geometry.cpp`,
  the inverse pinned to the oracle). The goal model loads through the mesh cache, its box list read by `L3DFile`
  (`GetBoxes`) and kept on `L3DMesh::GetBoxes`. ⚠️ approximate the turn's sine and cosine are taken in double and
  rounded once, as `ObjectMatrix` does. The players' on-screen text message of a goal is not drawn (logged).
- The creature: executors `GoalieCatchBall` (`Build::ThrowBallAtGoal`, `creature_mind::ThrowBallAtGoal`) and
  `GoalieFoul` (`Build::FoulPlayer`, `creature_mind::FoulPlayer`) on `Target::Footballer` (a villager whose final
  state ends in the football exit, `PlaysFootball` in `CreatureMindLearning.cpp`, shared with the act-on facts), the
  match read by `SituateFootball` (`Situation::football`: the ball, the goal of the creature's side by
  `creature_mind::IsOnHomeSide` of its owner's number, the other side's first player); test
  `CreatureKindActions.ACreatureJoinsAMatchByThrowingItsBallAtTheGoalOrFoulingTheOtherSide`. The six unlisted
  actions have no executor (not-in-the-original.md).
- Testbed: `building.football_pitch_built` (a whole pitch and its ball), `building.football_pitch_from_scaffolds`
  (a scaffold of eight laying out the town's pitch) and `building.football_match` (twelve adults at the hour the town
  wants relaxation most take the pitch up and play; not run yet) and `building.football_match_creature` (the same
  match, with a tiger a script tells to do the goalie's catch and the goalie's foul to two of the adults; not run yet),
  `src/Debug/TestbedScenarioFootball.cpp`.

#### Where it is in the code

`src/ECS/Abodes.cpp` and `Abodes.h`:
- `AbodeToBeDeleted`: the abode part of a building's deletion (its town, its dependants, the objects around it).
- `MultiMapFixedToBeDeleted`: the fixed object's part of the deletion; the building site goes, the body and the
  damage mesh stay until the entity is really destroyed.
- `StoragePitToBeDeleted`: the storage pit's part, before the abode part: the food pile and the five wood piles are
  unlinked and deleted.
- `StoragePitHandOn` and `CrecheHandOn`: when the town's storage pit or creche goes, the next functional abode of that
  kind takes the role (or none).
- `CitadelToBeDeleted`: the citadel's deletion, on openblack's citadel entity (the heart's), which also forces the
  influence to update.
- `BuiltCitadelPart`: a finished citadel part, then the heart's or the worship site's own "built".
- `CheckTownHasCentrePitAndHouse`: the tail of MakeFunctional; sets the town's flag once it has a centre, a storage
  pit and a house.
- `HasDestructionMesh`: whether the building has a damage mesh in the physics' BuildingDamage.
- `abodes::RedrawConstruction` builds the partly built model only when the resources exist
  (`Locator::resources::has_value`), so tests without resources draw nothing.
- `components::Abode` build flags: `Abode::k_UnderConstruction` (0x2), `Abode::k_NotRepaired` (0x4, a repair asked
  for, cleared when repaired) and `Abode::k_Built` (0x8).

`src/ECS/ShowNeeds.cpp`:
- `DrawVisuals`: one frame of a shown-needs sign (a workshop's wood sign, a worship site altar's three): the height
  eased toward the desire, placed at the owner's show-needs point and turned toward the camera. Returns whether it is
  drawn.
- `show_needs::DrawFor`: the object draw list runs a sign's frame only when it draws the sign's building,
  as the original does inside the building's draw; `show_needs::EndDrawnFrame` then shows the signs drawn this frame
  and hides the others, which keep their ease where it was. Test:
  `WorshipTest.AnAltarsNeedSignOnlyMovesOnTheFramesItsSiteIsDrawn` ([the altar's need signs](../magic/worship.md#the-altars-need-signs)).

#### Notes on the port

- A town's deletion (`ecs::town_deletion`, `src/ECS/Town/TownDeletion.{h,cpp}`, from `abodes::RemoveStructureFromTown`
  and the town branch of `ecs::ToBeDeleted`, so a script's `OBJECT_DELETE` of a town takes the same steps): the
  trigger (`town_deletion::EmptiedForGood`, with the town's live adult and child counts); its leaving its player is
  `town_belief::RemoveTownFromPlayer`, which the change of hands shares, as the original's two share 0x73A7D0; `Town::deleting` stands for
  the original's unavailable flag that its generic part sets first; a marked town is left out of `map_cells::TownsOf`
  (its player's list) and `town_queries::TownsNewestFirst` (the game's list), and it leaves `Context().towns`. The log
  says `Town <id> deleted: no building, plan or villager left in it`. Tests `TownDeletion.*` and `TownDeletionTest.*`
  (`test/test_town_deletion.cpp`).
  - Where openblack has no state for a step, it is left out: the playtime (openblack's town keeps only its
    `playthings`), the field list, the meetings, the fish farms, the name and the forests and artefacts lists (a town
    keeps none of them); the influence's `UpdateInfluence(0)` and put-back (only the forced update at the end is
    kept, which the put-back makes the same); the travel time to the worship site and `UpdateNearestTowns` (openblack
    keeps neither, as in the change of hands); the owner set to none (the marked town keeps its owner but is in no
    player's list).
  - (pending) a deleted town's planned abodes are left in its list (the trigger needs none; only a script's
    `OBJECT_DELETE` of a town with plans reaches it), and its town centre's spell icons are left to the centre (the
    original deletes them with the list); the rewards are deleted by their `serial`, the order the game's reward
    list keeps them unverified.
- (openblack) +0x98 = 0: Town::RemoveStructureFromTown 0x739A8A (from Abode::ToBeDeleted 0x402C93, abodes::OnToBeDeleted) leaves the abode without a town; no town has this id

### Test hooks

- `OPENBLACK_TEST_KNOCK="[count[,index[,delay[,interval]]]]"`: taps the index-th living-quarters abode `count`
  times through the hand's tap (with `OPENBLACK_SFX_TRACE=1` the log shows the knock samples).
- `OPENBLACK_TEST_ABODE_LIFE="<index>,<life>[,<pit>]"`: brings an abode (or with pit = 1 a storage pit) down to that
  life as a hit in the game would.
- `OPENBLACK_TEST_HIT_ABODE` and `OPENBLACK_TEST_FRAGMESH`: on [Physics impacts](../physics/physics-impacts.md#test-hooks).

## Pending


- The scaffold tap split's fire: the original passes the scaffold's fire to the new half right after that half starts
  to fly (CopyFire at 0x6EA123, in Scaffold's split, after its physics start and before SetScaffoldValue(value - 1)).
  openblack has the TODO at that point (Scaffolds.cpp, "the fire spreads to x"); `FireSystemInterface::CopyFire`
  now exists for it. Waits for a testbed scenario that can
  light and split a scaffold, then one measured commit (N-C-17, scaffold row) (queued fix F-97).
- Picking a 0 % (invisible) house; Town +0x740 totem / +0x750 workshops;
  feature_build should hide with NotDrawn (queued fix F-95).
- `workshops::SpecialPointOf`: «(not verified) that SpecialPoint::yAngle is fn_779910's Y angle» (queued fix F-97).
- `workshops::FinishScaffold`: «the angle when the point is missing: fn_779910's local is not initialised
  (0x779922..0x779939)» (queued fix F-97).
- `wonders::Create`: «edi = Create's fifth argument [esp + 0x20] (0x778EA3). 🔍 inferred the scale: Create(pos, info,
  town, yAngle, scale, ...) as Workshop::Create's argument order» (queued fix F-97).
- `wonders::Create`: «(+0x40)->vt +0x1F4() ? ... : (+0x40)->vt +0x1E8() ((pending) both names)» (queued fix F-97).
- `workshops::Create`: «(+0x40)->vt +0x1E8() 0x779445 ((pending) its name)» (queued fix F-97).
- `k_WorkingLoopSample` 0x4A: «(pending) the sample's name in the .sad» (queued fix F-97).
- `PlayerWonderPower` (GPlayer +0xB8[tribe]): ✅ settled 2026-10-21: it is MaxTribalPower, copied (at least 1) into
  TribalPower every player's turn and reset to 1 with the map; now `PlayerMagic::maxTribalPower`
  ([building-sites.md](building-sites.md#where-the-originals-objects-live)).
- `gravesStage`: ✅ drawn since 2dcfc5741, see [the graves on the model](building-sites.md#the-graves-on-the-model).
- `ProcessAbode` +0xB9: «+0xB9 < 200 -> ++ (no reader found)» (queued fix F-97).
- `QueueBeliefSprite`: «[0xC02A08] != 1 (a constant 1)» (queued fix F-97).
- `GetAddedThisPeriodRatio`: «0x438070 (no direct caller)» (queued fix F-97).
- `PowerSlot`: «🔍 inferred an abode's GetPlayer is its town's (Town +0x2C, Town::owner)» (queued fix F-97).
- `building_sites::Process` (citadel): «🔍 inferred not reached in the original: CitadelHeart::Process 0x4665A0 does not
  call MultiMapFixed::Process 0x52F700, the only caller of a site's Process found» (queued fix F-97).
- `TownStats AddAbode` +0x4C: «(not verified) that it equals that book-keeping» (queued fix F-97).
- ShowNeeds: «0x719E98..0x719EAD: the 3D object's +0x60 and +0x5C = 0x2D. (pending) the two fields of the
  Game3DObject» (queued fix F-97).
- Buildings: creature hits; a scenario for the rebuild on a blow (a town's hut repaired by its villagers between two
  blows, or a hut with no town); what happens to
  the building itself (alignment, aggressor, inhabitants, the town's emergency, −0.2 of a building site) is in
  [Life and damage](#life-and-damage).

