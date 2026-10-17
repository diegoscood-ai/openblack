# Buildings

## Summary

A town's buildings as the original runs them: the resources an object holds and the floating numbers that show them,
a building's life, damage and repair, and what a knock does to it. Building sites, plans, the workshop and the
scaffolds are on [Plans and building sites](building-sites.md); the villagers who use the buildings on
[Villagers: data and states](../beings/villager-states.md); the breaking itself on
[Physics impacts](../physics/physics-impacts.md#buildings-that-break).

openblack ports the abodes, their resources, life and damage, the floating numbers and the knock; what a broken
building does to its town and a few town book-keeping fields wait (see [Pending](#pending)).

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
  (`components::TownBelief`). DoCreatureMimicAfterAddingResource (vt +0x68C) is (not ported): no creature.
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
- Town::RemoveStructureFromTown 0x739A60 (Abode::ToBeDeleted 0x402C93), the part openblack keeps: +0x98 = 0 (0x739A8A: Abode::GetTown 0x401730 answers no town) and out of the town's list +0x754 (0x739B17..0x739B57). (not ported) an unbuilt abode's sites in +0x790 (0x739A94..0x739AEE; MultiMapFixed::ToBeDeleted deletes its own site), fn_739A40 for a built one, an emptied town's ToBeDeleted (0x739B61..0x739B8F:), vt +0x8F0(0) and +0x990 fn_66C610. SetTownArea 0x739BB6 unless the original deletes the town there
- MultiMapFixed::ToBeDeleted 0x52E2B0 (before Object::ToBeDeleted 0x636670, the generic part of ecs::ToBeDeleted): the reactions it started (Reaction::RemoveAllReactionsInitiatedByObject 0x6E4750, 0x52E2B7); its footpath link (vt +0x74, +0x64: (not ported) footpaths); out of g_game +0x205CB4 ; in the map -> g_game +0x59C0 |= 1 ((pending) its reader); the building site (+0x74) ToBeDeleted and fn_52E3F0(0) (0x52E33A.. 0x52E34F: building_sites::ToBeDeleted clears +0x74 itself). Nothing of the physics at the mark (Abode::ToBeDeleted 0x402C60 and this one touch neither the DestructionMesh +0x90 nor the body): the body goes at the next BeginTurn (PhysicsObject::GameTurnUpdate 0x645018) or with the immediate deletion, the FragMesh's model when the entity is really destroyed (OnBuildingDamageDestroyed / OnDrawMeshDestroyed)
- Abode::ToBeDeleted 0x402C60
- 0x402C6F: DeleteDependancys (vt +0x910). Literal: StoragePit, Graveyard, Creche, Workshop and Wonder have run it already, so it runs twice for them; the second pass of a graveyard / a creche finds the town's +0x748 / +0x744 no longer this one and writes 0 there (SetGraveyard(0) 0x595D4A, 0x50AAB3); a wonder is taken off its player twice (Wonder::RemoveFromPlayer 0x778F50)
- 0x402C9F..0x402CA6: MultiMapFixed::ToBeDeleted 0x52E2B0 with the argument
- 0x464CD3..0x464CE6: the CameraExclusion +0xE0 deleted (0x454A00): (not ported) no camera exclusion in openblack 0x464CEC..0x464D18: the citadel's +0x30 (its heart) = 0 when this: (pending) they are not all checked for an unavailable heart 0x464D5A fn_00465230: +0x98 (its +0x38 = 0, fn_00530140, ToBeDeleted(0)). (pending) +0x98 not identified 0x464D62: CitadelPart::ToBeDeleted 0x469540 (out of the citadel's part list +0x4C: none in openblack) -> MultiMapFixed::ToBeDeleted 0x52E2B0
- WorshipSite::ToBeDeleted 0x77AA60 -> CitadelPart::ToBeDeleted 0x469540 (0x77ABFC: out of the citadel's part list +0x4C, next +0x84: none in openblack) -> MultiMapFixed::ToBeDeleted 0x52E2B0 (0x4695B1)
- 0x406722..0x406738: EffectValues(3, hitter, player) 0x524FE0: the preset's numbers and radius (fn_005250A0), the applier (fn_005256C0) and the player (+0x3C)
- 0x40674B..0x406781: r = +0x18 (FragMesh::GetRemaining); r < 0.4 [0x980188] && player && IsMemberOfThisPlayer(MyInterfaceStatus) -> GGuidance::HelpSpritesDestroyBuilding 0x71D070. (not ported, §6)
- 0x4067B7: EffectNumbers::operator*= 0x525720 (f); 0x4067C3..0x4067CD: EffectValues::operator/= 0x525950 by GetDefenseMultiplier 0x637930, number by number (fdiv; a 0 multiplier divides by 0, literal)
- Abode 0x4016F0 = 0; StoragePit 0x55CCE0 = 1; TownCentre 0x55DB30 = 1
- P = GetPercentBuilt - amount; p > 0 ? p : 0 (test ah, 0x41); SetPercentBuilt (fn_52EDD0); p == 0 -> Object::ReduceLife(GetLife(), player) (0x52F659: life 0)
- Abode::DeleteAbodeSurroundingObjects 0x403D20 (Abode::ToBeDeleted 0x402C98): the script highlights in the cell of point 1 (when the info has a didYouKnow) and the street lanterns in the cell of point 0, ToBeDeleted(0)
- StopBeingFunctional vt +0x918: Abode 0x4073C0 (a player and +0xB9 >= 200: the player's statistics, not ported; nothing else), StoragePit 0x733960 (then Pot::SetupReaction on its piles that hold something), TownCentre 0x744A00 (then the town's worship percentage 0). The villagers stay and no site is made here (ReduceLife makes it)
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

- 0x40683A..0x406844: GetTown -> [0xC4CC6C], HowManyPeople::KnockKnock 0x829690 (not ported). 0x406849..0x406862: for any abode, before the type test, each inhabitant of +0xA0 (next +0xE4) SetStateWhenTappedOnAbode 0x752B80 (VillagerEmergency.h, V11_spec §5) a copy of the list; the original walks +0xA0 / +0xE4 live, and SetStateWhenTappedOnAbode does not change it (only LivingAction::SetState + SetupMoveToPos, 0x761410 / 0x761421)
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
- 0x464C59..0x464CD0: the CitadelEntrance +0xDC: its torches +0x58 / +0x5C (deleted, vt +4) and their night lights +0x60 / +0x64 (fn_00823420), its 3D object +0x40: (not ported) openblack's entrance has none of them; then its ToBeDeleted(now) (push ebp 0x464CCF)
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
- The Abode class of the original (Abode.cpp 0x401350..0x409000): every village building, the fields, the totem, the storage pit, the town centre and the spell dispenser included. Only the interface tap lives here for now (milestone B8 of dev\tmp_dis\audio\PLAN.md): knocking on a roof.
- Abode::GetNewEp 0x403590 (ABODE_EPP type): the first entrance point of that type of the abode's mesh (the NewEP block, graphics::L3DMesh::GetNewEntrancePoints) through the abode's world matrix (Object::GetWorldMatrix 0x638200); none without a mesh, without the block or without a point of the type
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
- +0xB9: counts up to 200 in each Abode::Process (0x404503..0x40450F); read by Abode::StopBeingFunctional 0x4073C8 (>= 200 with a player: the player's statistic, not ported)
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

### Sources

- Disassembly of `runblack.exe` the exe.
- bw1-decomp `src/Black/Abode.h`, `GameThingWithPos.h` (Flags +0x24), `Town.h`.

## openblack

Status: **partial**. Progress rows: [abodes](../project/progress/building/abodes.md) (64%),
[civic buildings](../project/progress/building/civic_buildings.md) (73%),
[damage and repair](../project/progress/building/damage_and_repair.md) (90%), and [the town](../project/progress/town/).

- `src/ECS/Abodes.{h,cpp}`, `src/ECS/Components/Abode.h`, `src/ECS/Archetypes/AbodeArchetype.{h,cpp}`,
  `src/ECS/ShowNeeds.{h,cpp}` (the floating numbers) and `src/3D/ChimneySmoke.cpp`;
- a building's and a town's state lives in components (`components::Town`, `components::TownBelief`, the abode
  components) or Locator services; the desire, the belief fold and the builder ring are pure functions tested with
  fakes.

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#world).

#### Resources held by objects

- openblack's float division and cube (`store_rules::LastTakenModifier`, fed by `town_stores` with 0 as never) give the same value for turn differences below 2^24.

#### The floating numbers (ValueSpinner)

- openblack: the component `FloatingNumber` (ECS/Components/FloatingNumber.h) and its rules (ECS/FloatingNumber.h: `Step`, `Alpha`, `TextPlace`, `ScreenPixel`), stepped every frame by `Game` with `FrameGameSeconds()`; `Game`'s `FillFloatingNumbers` puts those on the screen, furthest first, into `OverlayFrame::floatingNumbers` (none with the wide screen on, nor in the temple), and `Renderer::DrawFloatingNumbers` draws them in font j0 in the `FinishFrameIcons` view after the input prompt icons' first draw, under the bars and the fade, as the Z-sorter's flush at the start of the end of the frame does. Only the believers gained make one so far.

#### Life and damage

- openblack `abodes::OnPhysicalDamage`
- openblack's town lists are the abodes with the town's id (town_stats::AbodesOf, town_queries::TownAbodes, TownCentreOf, TownSystem::FindAbodeWithSpace, the influence), so the id goes and a zombie abode is in none of them.
- openblack never deletes a town
- openblack keeps no such list
- openblack's citadel is the heart itself (worship::citadel::HeartOf) 0x464D1B..0x464D52: out of the hearts' list [0xC5E3D4] (next +0xD8): the Each<CitadelHeart> readers.
- openblack's plans are not entities

#### The knock on a building

- openblack: `AbodeKnockSystem` (`Locator::abodeKnockSystem`, raffclar's slot) with `abode_knock::IsTappable` (the village centre refused, f82881887), the read-out on game time with float products (bfbd591ed) and the held villager's home (`components::Abode::heldVillagersHome`, fe2ddeff3). Holding a villager near a house (`DiscipleInHandNear` 0x407420) does not bring the read-out up yet (queued fix F-16, after the disciples' port).
- openblack: 
- openblack: 

#### Abode details

- openblack's towns always have one: NEUTRAL
- openblack: 
- openblack: 

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
- `DrawVisuals`: one frame of a workshop's shown-needs sign: the height eased toward the desire, placed at the
  owner's show-needs point and turned toward the camera. Returns whether it is drawn.

#### Notes on the port

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
  openblack has the TODO at that point (Scaffolds.cpp, "the fire spreads to x"). Waits for a testbed scenario that can
  light and split a scaffold, then one measured commit (N-C-17, scaffold row) (queued fix F-97).
- CREATE_TOWN_TEMPORARY_POTS (0x716B65..0x716BF0) (queued fix F-97).
- A missing pit pile is created by StoragePit::AddResource (Pot::Create 0x66CF10); openblack makes all six with the
  pit and never deletes them **⚠️ approximate**. How CREATE_ABODE fills a new pit is not read: today it goes through
  StoragePit::AddResource (pile sounds and the pulse at load) **⚠️ approximate** (queued fix F-97).
- Picking a 0 % (invisible) house; Town +0x740 totem / +0x750 workshops / +0xEA4 football; the graves stage's reader;
  feature_build should hide with NotDrawn (queued fix F-95).
- **(not verified)** freeAdultPlaces = MaxVillagers - adults housed (queued fix F-97).
- What a broken building does to its town: the alignment and the anger of the town, and the inhabitants leaving when
  the building stops being functional (StopBeingFunctional 0x4073C0, see [Life and damage](#life-and-damage)). The
  villager side of a hit house is in [Villagers: data and states](../beings/villager-states.md) (the tap from Abode::ReduceLife); whether the building
  side is ported is **(not verified)** (queued fix F-97).
- `workshops::SpecialPointOf`: «(not verified) that SpecialPoint::yAngle is fn_779910's Y angle» (queued fix F-97).
- `workshops::FinishScaffold`: «the angle when the point is missing: fn_779910's local is not initialised
  (0x779922..0x779939)» (queued fix F-97).
- `wonders::Create`: «edi = Create's fifth argument [esp + 0x20] (0x778EA3). 🔍 inferred the scale: Create(pos, info,
  town, yAngle, scale, ...) as Workshop::Create's argument order» (queued fix F-97).
- `wonders::Create`: «(+0x40)->vt +0x1F4() ? ... : (+0x40)->vt +0x1E8() ((pending) both names)» (queued fix F-97).
- `workshops::Create`: «(+0x40)->vt +0x1E8() 0x779445 ((pending) its name)» (queued fix F-97).
- `k_WorkingLoopSample` 0x4A: «(pending) the sample's name in the .sad» (queued fix F-97).
- `PlayerWonderPower` (GPlayer +0xB8[tribe]): «(pending) who reads it (inferred: a wonder bonus per tribe) and who
  clears it» (queued fix F-97).
- `gravesStage`: «(pending) what the draw shows for it» (queued fix F-97).
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

