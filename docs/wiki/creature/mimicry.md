# Mimicry, towns and miracle reactions

## Summary

The creature learns by watching its player. Every deed the player does (feeding a worship site, throwing a villager
into the sea, watering a field, casting a miracle on something) is reported to the player's own creature, which may
start copying it if it is old enough, sees it, and is not busy with something more important. The towns and villagers
also feed the creature's picture of what its player wants (the perceived desires), and a creature reacts to miracles
cast near it like any living thing: it runs from a nasty one, looks at a nice one, and may learn the miracle.

openblack has the 46 deeds and the per-player entry point; four deed sites report (planting a tree, a building hit by
a thrown thing, a villager thrown into the sea, water on crops), the perceived desires and their scroll reader are
ported, and the creature takes the four miracle reaction types 3, 13, 21 and 37. The other deed sites, the gate's
full order and the town's attitude step are still to port. The town desires themselves belong to
[The towns](../beings/towns.md).

## The original

### Mimicry and the town hooks

The original reports a player's deed to the creature through `GPlayer::ConsiderMakingCreatureMimicPlayer(status,
action, thing, magic)` at 0x4EA900 (bw1-decomp `src/Black/Player.h:163`; `symbols.txt` line 4883). It is a method
of the **player**: it only concerns that player's creature (player +0xA4C). There are **25 direct call sites** and no
indirect ones (the address never appears in the image as a pointer). Of the 46 deed values (`DETECTED_PLAYER_ACTION`,
bw1-decomp `include/chlasm/CreatureEnum.h:745-805`, the 46 rows of `info.creatureMimic`), 40 can be reported and 6
never are. Unless a row says otherwise, `magic` is 0.

**Evidence.** Every caller was disassembled from the game's `runblack.exe` with capstone. Its layout matches the
1.20 `symbols.txt`, but its SHA-1 is not the one in `build.sha1`. The Mac build was not available for a cross-check.
The four sites ported first (deeds 11, 16, 21, 33) were read again from the same exe before they were ported.

#### Corrections to earlier notes

- **Wrong labels in the 1.20 `symbols.txt`.** 0x4EA670 is labelled `DecideOnNewPlan` (line 4882) but is the real
  `Creature::MimicPlayer`: 3 arguments (`ret 0xC`), calls `GetMimickingAction`, called at the end of
  `ConsiderMakingCreatureMimicPlayer`. The Mac symbols put `DecideOnNewPlan` in another TU. 0x4E0DE0 is labelled
  `MimicPlayer` (line 4663) but takes 1 argument (`ret 4`); it is called by `ForceActOnObject` and `fn_005E6BD0` and is
  not a deed site.
- **`Town::UpdateAggressor` is not a deed site.** The code at 0x73CAAA..0x73CB29 calls guidance functions
  (`fn_0071C960`, `fn_0071C9F0`), not the mimic (see [The emergency's creature part](#the-emergencys-creature-part)).
  The claims in openblack's `MapShield.cpp` comment and in the old miracle notes were wrong; the wiki is corrected,
  the code comment is Pending.
- **`Spell::InitWithPos` is not a mimic site.** It has no call to the mimic function. It does make the creature
  empathise (`fn_00721730`, below), which is what openblack's `Spell.cpp` comment says; a `SeeMiracle` call there would
  be a guess.
- **The abode hit tests FROM_HAND on the abode's own body**, not on the hitter's (0x406273 tests the argument's +0x1D8,
  the abode's entry; the hitter is its +0x20). A first reading took it for the hitter's flags. The test also comes
  before the `PhysicallyDestroysAbodes` test.
- **Fidelity risk at `StoragePit` 0x733844.** When food is put into another player's pit, the original draws
  `GameRand(2)` (bw1-decomp `StoragePit.cpp` line 688) **before any creature check**, so the draw happens even when the
  player has no creature.
- **Disciple off-by-one in `Villager::Landed`.** The deed is disciple type + 21. `VILLAGER_DISCIPLE` has TRADER = 9,
  which has no deed of its own, so the last values shift by one: TRADER gives 30 (CHANGE_HOUSE), CHANGE_HOUSE gives 31
  (WORSHIP), WORSHIP gives 32 (TAKE_OBJECT_HOME), FROM_VORTEX gives 33, NONE gives 21 (THROW_IN_THE_SEA).
  [The villagers](../beings/villager-states.md) already states this correctly.
- **The animals' sinking tests availability first.** `Living::HasSunk` 0x5ED370 (the animals') returns 0 when the
  animal is not available (vt +0x2C, 0x5ED375), then reports deed 21 for a last dropper (0x5ED3A9, with no null test of
  the player), then sets it dying and deletes it.

#### Gate in `ConsiderMakingCreatureMimicPlayer`

`ConsiderMakingCreatureMimicPlayer` 0x4EA900, in order 📼 1.20 only:

1. The player has a creature (player +0xA4C) without Flags +0x24 & 0x10, and, when it has a leash status
   (`fn_004CF060`), that leash working (+0x12C → +0x18 not 0).
2. The creature has a player, the player's +0x8E0 is not 2, and creature +0x1268 (the phase) is at least 3.
3. Leash and table check: per-action table at 0xCAB220, 0xC0 bytes per action; the action row's flag +4 (0xCAB224)
   must be 0 unless the status is ROPE (+0x1C == 2), untied (+0x24 == 0) and the first argument.
4. The current reaction's +0x10 is below 0x96.
5. `CanSeePos(thing)`, and the distance is below k², k = 2·(trunc(√(60·size + 160)) div 2) + 1 with the 3D +0x90
   (225 m at size 1), `fn_004EFC70(+0x160)`.
6. The action's priority (table +0) above 0 with no mimic running, else stage +0x1C50 == 2 or at least the running
   one's for another deed.
7. Then it calls `Creature::MimicPlayer` 0x4EA670. With a magic m: n = trunc(needed(m) − seen); n > 0 →
   `ConsiderLearningAction(1, m)`, then n > 1 stops, and n == 1 stops unless `GameRand(2)` (CreatureMimic.cpp:0x16C)
   is non-zero.

Mimicry's stage 2 (`fn_004EA540`) forces the table's desire for 120 s through `SetCheatDominantDesire`
([The timed dominant desire](leash.md#the-timed-dominant-desire)). The copied action itself is forced by
`MimicForceCopyingAction` (0x4EA1EA, 0x4EA32B, 0x4EA46C), which asks the action's cast predicate first, and the mimic
copy of CastHealSpell's object filter is at 0x4EA208: both are on
[May it cast: the action's predicate](casting.md#may-it-cast-the-actions-predicate).

#### Spell hits

`fn_004E9DF0(thing, magic, player)` is called from `Spell::ApplyDefaultSpellEffect` at 0x720E8A, only when something
is hit, the spell has a player and it is not a creature cast; the report itself is at 0x720EB7. Returns 46 for "no
deed".

- The thing is what the effect reached last. For a miracle cast on an object (type 5 with a target) it is the target,
  but only when `IsEffectReceiver` (vt +0x774) passes; when it fails there is no deed. Otherwise it is
  `ApplyEffectToMapPos`' return. The deed is worked out and reported before the spell's reaction.
- First, for every magic type: if the spell's desire is not ANGER and the nearest town within 100 m belongs to someone
  else, the deed is 20. The desire is `GMagicEffectInfo[magic]` +0x98 (`perceivedPlayerDesire[0]`; table at 0xCC6630,
  0x11C bytes per entry). The town is `MapCoords::GetNearestTown(thing +0x14, 100.0f)` 0x6020E0: it walks the players
  with `GetNextPlayerAndNeutral` and each player's town list (+0xA50, next +0x75C), measures with `fn_00605CD0`
  (`gutils::GetDistanceInMetres`) and keeps a town only when strictly nearer than the running best, which starts at
  100. Of two towns equally far, the first in that order wins. Its player is `GetPlayer` of the town.
- Fireball 1-3: 17.
- Lightning and explosion 4-8: 18. 9 (EXPLOSION_ONE_PU_TWO), 12, 13 and 16-20: nothing.
- Heal 10-11: 42.
- Food 14-15: 1 on a worship site, else 3 on a built storage pit, otherwise nothing.
- Wood 21: 5 on a built storage pit, else 8 on a building being built, otherwise nothing.
- Water 22-23: 33 on a field, 34 on something on fire, and **33 on anything else**.
- Flock 24-25: always 20.
- Any other magic type: nothing.

The jump table is at 0x4E9F20 (bytes) and 0x4E9F00 (targets). The predicates, by class:

- `IsStoragePit` (vt +0x300): a storage pit when built (row 3); 0 for a workshop and every other class.
- `IsWorshipSite` (vt +0x308): `WorshipSite` 0x55DC90 returns 1; everything else 0.
- `IsBuildingWhichIsBeingBuilt` (vt +0x318): the fixed buildings' rule in row 8.
- `IsField` (vt +0x20C): `Field` 0x527F30 returns 1; `PileResource` and the base class 0.
- `IsOnFire` (vt +0x298): `Object::IsOnFire` 0x637CC0 for an object; 0 for a `GameThingWithPos` that is not one.

**Water is reported twice on a field.** The water's drop goes through the spell event, so the generic deed comes first,
with magic 22 or 23, on the last thing reached; then `Field::ApplyWaterSpell` 0x528F64 reports 33 again, with magic 22,
on the field. Each report that passes the gate draws the mimicry's random number.

#### Landing

`ConsiderCreatureMimickingWhenObjectLands` 0x4EAAB0 is called from `Object::InitialisePhysicsFromHand` (0x637306, a
put-down that lands at once), `Object::EndPhysics` (0x63764A) and the hand's drop fn_005DC1E0 (0x5DC294, every
interface's: the player's hand and a computer player's alike, after the drop's record of the last dropped object, see
[the last dropped object](../hand/grab-hold-and-throw.md#the-release)). It takes no argument; ecx is the object (an
oracle run of fn_005DC1E0 with the callee stubbed records ecx = the dropped object). A gentle put-down that lands at
once is so considered twice: in `InitialisePhysicsFromHand`, then in the drop. Status = the thing's last dropper
(`GetInterfaceStatusWhoLastDroppedMe` 0x4EAAB9, none: nothing); player = that status's player (vt +0x1C).

- Within 80 m of the player's citadel (player +0xA48): 36 if the thing's town (`GetTown`) belongs to another player,
  otherwise 32.
- Otherwise: 35 if the thing's town belongs to another player and the nearest town is the dropper's.

#### Deed table

Complete for the exe. The "openblack" column says what is **ported**; otherwise it gives the site where the report
would go, which waits for the follow-up hooks (Pending).

| Deed | Name | Reporting function, call address | Trigger (status / player / thing) | openblack | Conf. |
|---|---|---|---|---|---|
| 0 | PUT_FOOD_IN_WORSHIP_SITE | `WorshipSite::DoCreatureMimicAfterAddingResource` 0x77DF19 | Food added to a worship site; the `MultiMapFixed` test runs first. Dropper's status and player; thing = the site | `ECS/ObjectDelivery.cpp`, `ECS/PotResource.cpp` | High |
| 1 | CAST_MAGIC_FOOD_IN_WORSHIP_SITE | `Spell::ApplyDefaultSpellEffect` 0x720EB7 via `fn_004E9DF0` | Food miracle hits a worship site. Spell's status and player; thing = what was hit; magic = the spell's | `Magic/Core/SpellEvent.cpp` | High |
| 2 | PUT_FOOD_IN_STORAGE_PIT | `StoragePit::DoCreatureMimicAfterAddingResource` 0x733870 | Food put in a pit, status +0x128 is the dropper's own player | ObjectDelivery / PotResource (the deed is computed, not reported) | High |
| 3 | CAST_MAGIC_FOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Food miracle hits a **built** pit (`StoragePit::IsStoragePit` 0x4E4990 is `Abode::IsBuilt`). A workshop is not a pit: `Workshop::IsStoragePit` 0x7792F0 returns 0, as do the town centre, creche, graveyard, wonder and the base class | SpellEvent.cpp | High |
| 4 | PUT_WOOD_IN_STORAGE_PIT | StoragePit 0x7338B6 | Non-food resource in a pit, own player (after the building-site test) | ObjectDelivery / PotResource (computed) | High |
| 5 | CAST_MAGIC_WOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a built pit (not a workshop, see row 3) | SpellEvent.cpp | High |
| 6 | BUILD_HOUSE | `Scaffold::EndPhysics` 0x6E87A9 | A thrown scaffold stops. Status = physics object +0x24; player = the **scaffold's** owner; thing = the scaffold | `ECS/Scaffolds.cpp` | High |
| 7 | PUT_WOOD_IN_BUILDING_SITE | `MultiMapFixed::DoCreatureMimicAfterAddingResource` 0x52F239 | Wood added to a building being built; returns true and stops the subclass tests | ObjectDelivery / PotResource (computed for a pit) | High |
| 8 | CAST_MAGIC_WOOD_BY_BUILDING_SITE | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a building being built (not a built pit). `MultiMapFixed::IsBuildingWhichIsBeingBuilt` 0x4E41C0: `!(GetPercentBuilt >= 1) \|\| ((+0x58 & 4) && !(GetLife() >= 1))`, +0x58 bit 4 being the not-repaired flag ([Buildings](../world/buildings.md)); `fcomp` and C0 make a NaN count as below 1. Only fixed buildings have it (abodes, pits, workshops, town centres, features, the citadel heart, worship sites, fields); trees, villagers, creatures, piles and plain objects return 0 (0x4054C0) | SpellEvent.cpp | High |
| 9 | PUT_WOOD_IN_WORKSHOP | `Workshop::DoCreatureMimicAfterAddingResource` 0x77A6AB | Wood added to a workshop (after the building-site test) | ObjectDelivery / PotResource | High |
| 10 | CAST_MAGIC_WOOD_BY_WORKSHOP | **never reported** | Wood cast on a workshop gives 8 while it is being built, otherwise nothing (`Workshop::IsStoragePit` returns 0) | none | High |
| 11 | PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | Replanted tree, after `Forest::AddTree`. Player = `PhysicsObject::GetPlayer`; status = +0x24; thing = the tree | **ported**: `HandTrees.cpp` `Replant` | High |
| 12 | GIVE_TOWN_PROTECTION_WITH_SHIELD | **never reported** | Shield 19-20 gives nothing in `fn_004E9DF0` | none | High |
| 13 | BRING_PEOPLE_TO_WORSHIP | **never reported** | — | none | High |
| 14 | MAKE_ARTEFACT | `Town::AddArtifact` (Mac name; 1.20 `fn_0073FDA0`) 0x73FE49; `WorshipSite::AddArtifact` 0x77DBA1 | A thrown object suitable as an artifact lands within 50 m and joins a town or worship site (`Fixed::EndPhysics` 0x52E047 / 0x52E0BE). Player = the thrower; status = last dropper; not for trees | `ArtefactSystem.cpp` | High (Mac name: Med) |
| 15 | DAMAGE_BY_THROWING | `PhysicsObject::GameTurnUpdate` 0x6463BE | After the impact sound and `ReactToPhysicsImpact`: a body from the hand (+0x1D8 & 4) with a status, a living thing or rock, not a tree. Status = +0x24; thing = the body | `ECS/Physics/PhysicsObjects.cpp`, after `ReactToPhysicsImpact` | High |
| 16 | DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286; `GameTurnUpdate` 0x64643A | Abode hit, with the abode's own body from the hand; thing = the abode. Physics: the same body test when it hit something (+0x20); thing = the thrown body | **ported** for the abode (`BuildingDamageSystem.cpp`); the physics one: PhysicsObjects.cpp | High |
| 17 | DAMAGE_WITH_FIRE | 0x720EB7 via `fn_004E9DF0` | Fireball 1-3 hits something | SpellEvent.cpp | High |
| 18 | DAMAGE_WITH_MAGIC | same | Lightning or explosion 4-8 hits something | SpellEvent.cpp | High |
| 19 | IMPRESS_BY_THROWING | **never reported** | — | none | High |
| 20 | IMPRESS_WITH_MAGIC | same | The desire/town rule, or any flock 24-25 | SpellEvent.cpp | High |
| 21 | THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED; `Living::HasSunk` 0x5ED3A9 | Sinks while available (`IsAvailable` first). Status = last dropper; thing = itself. `Living::HasSunk` does not null-check the player. Also NONE disciple + 21 | **ported** for the villager (`VillagerDrowning.cpp`); the animal's: no last dropper is kept for animals | High |
| 22-29 | MAKE_DISCIPLE_FARMER … CRAFTSMAN | `Villager::Landed` 0x760744 | First turn landed (+0x90 == 1, flags +0xE0 & 0x20 and & 0x200). Status = last dropper, needs a player. Deed = disciple (+0xF2) + 21 if below 46 | `VillagerDisciple.cpp` (`villager::ShowDiscipleToCreature`) | High (code) |
| 30 | MAKE_DISCIPLE_CHANGE_HOUSE | same | Reached by TRADER (9) + 21 | same | Med |
| 31 | MAKE_DISCIPLE_WORSHIP | same | Reached by CHANGE_HOUSE (10) + 21 | same | Med |
| 32 | TAKE_OBJECT_HOME | `ConsiderCreatureMimickingWhenObjectLands` 0x4EAB49 (+ disciple WORSHIP) | Dropped within 80 m of own citadel; thing's town is not another player's | **ported** for a put-down that lands at once and every hand's drop (`creature_mimic::ConsiderObjectLands`); the end of physics: `ECS/Systems/Implementations/VillagerPhysics.cpp` | High |
| 33 | CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64; also 0x720EB7 via `fn_004E9DF0` | Field: spell has a player; magic always 0x16; thing = the field. `fn_004E9DF0`: water hits a field **or anything not on fire** | **ported** for the field (`SpellWater.cpp`); the spell hit: SpellEvent.cpp | High |
| 34 | CAST_WATER_TO_PUT_OUT_FIRE | 0x720EB7 via `fn_004E9DF0` | Water hits something on fire (not a field) | SpellEvent.cpp | High |
| 35 | STEAL_OBJECT_AND_PUT_IN_TOWN | ObjectLands 0x4EAB8E | Not near own citadel; thing's town is another player's; nearest town is the dropper's | FromHand.cpp / VillagerPhysics.cpp | High |
| 36 | STEAL_OBJECT_AND_PUT_BY_CITADEL | ObjectLands 0x4EAB34 | Within 80 m of own citadel; thing's town is another player's | same | High |
| 37 | BREAK_ROCKS | `GInterface` tap handler 0x5DA650 (unnamed), call 0x5DA6C8 | Tapping a rock that is valid to tap. Status = interface +0x39C; thing = the rock | `ECS/Systems/Implementations/HandTurn.cpp` | High |
| 38 | THROW_FOOTBALL_IN_GOAL | **never reported** | — | none | High |
| 39 | CATCH_FOOTBALL | **never reported** | — | none | High |
| 40 | SACRIFICE | `Animal::ApplyThisToObject` 0x41B3DF; `Villager::ApplyThisToObject` 0x752D10 | Animal or villager applied to a `WorshipTotem`. Status = the applying hand's; thing = the victim; followed by the alignment update | `ECS/AltarOffering.cpp` (`ecs::altar_offering`) | High |
| 41 | PLAY_WITH_TOY | `Object::InitialisePhysicsFromHand` 0x637457 | Status, `IsToy`, and no thrower or the thrower is not a creature (bw1-decomp `Object.cpp:603-606`) | `ECS/Physics/FromHand.cpp` | High |
| 42 | HEAL | 0x720EB7 via `fn_004E9DF0` | Heal 10-11 hits something | SpellEvent.cpp | High |
| 43 / 44 | STEAL_FOOD_FROM_FARM / _FROM_STORAGE_PIT | StoragePit 0x7338B6 (shared tail; deed pushed at 0x733861) | Food put in a pit and status +0x128 is not the dropper. Deed = 43 + (`GameRand(2)` != 0) | ObjectDelivery / PotResource (computed, the draw made) | High |
| 45 | STEAL_WOOD_FROM_STORAGE_PIT | StoragePit 0x73389A | Non-food in a pit, status +0x128 is not the dropper | same (computed) | High |

#### Town and villager hooks

All of these are empty stubs in bw1-decomp (`TownAttitudeToCreature.cpp`, `Town.cpp`, `Player.cpp`,
`CreatureDirectControl.cpp`, `TownDesire.cpp`, `Spell.cpp` are 0-35 lines; `TownCreatureInfo.h` lists only the
destructor and `GetBaseInfo`). Everything below was read with capstone from the shipped `runblack.exe` using the 1.20
symbols. The exe matches them: 0x4C80F0 has the expected bytes, and the Creature vtable at 0x8CC810 resolves to the
right symbols. How a town's own desires are worked out is on [The towns](../beings/towns.md); this page has only what
reaches the creature.

##### Town step 16

`Town::UpdateAttitudeToCreature` 0x7437F0 (size 0x210).

- **Call site:** `Town::Process` 0x747380 calls it at 0x74744B, right after `ProcessTownEmergency` at 0x747444
  ([The villagers](../beings/villager-states.md)). Declared at `Town.h:353`; TU `Black/TownAttitudeToCreature.cpp`
  0x743690-0x743A30 (`splits.txt:2255`).
- **Data:** Town +0x970 is a singly linked list of `{next, TownCreatureInfo*}` nodes; Town +0x974 is its count. Each
  record: +0x10 the Creature, +0x14 the attitude, +0x18 the turn the attitude was set, +0x1C the turn it was reset.
- **Creation:** `Town::CreateCreatureInfo` 0x743720 on first contact (attitude 0, both stamps = turn), called from
  `Villager::SetupReactToCreature` at 0x767673.
- **Reader:** `Town::GetTownAttitudeToCreature(Creature*)` 0x7436F0 returns +0x14, or 0 if there is no record. Read by
  the villager reactions (0x76767B, 0x767779, 0x76849E), `WatchFightAnimation`, `Living::NumGameTurns…ToCreature`
  (0x5F1BBA, 0x5F1CCE) and `Creature::GetImpressiveValue` (0x47B244).
- **Per record, each turn:**
  1. If the creature's GameThing flags byte +0xA has bit 0 set, every node for it is unlinked and freed and the info is
     deleted (vtable +4, argument 1).
  2. Gate: `GetNearestTown(Town+0x14, 10.0)` 0x6020E0 must return this town (literal, and looks always true since it is
     the town's own position) and `CreaturePlan::IsValid` (mind +0xF48) 0x4F12E0 must be true.
  3. The new attitude comes from a member function in the creature-action info table: index = Creature +0x1120,
     entries 0x50 bytes, function at 0x9D16B8 + idx·0x50, called on `[[mind+0xF58]+0x30]`. Creature +0x1120 is the
     action of the **last plan carried out to its end** (written only by `Creature::Initialise` 0x47465F and by the
     success path fn_00475450 at 0x475469 from mind +0xF60; not saved), not the current plan's. The table is filled at
     run time by `crt_xc_fn_CreatureAction_00491B90` (`_$E90` 0x491BA0); its 328 rows equal bw1-decomp's
     `CreatureAction.cpp` table row for row. 0x768540 None → 1 (240 rows); 0x768550 Fear → 3 (22 rows: 15, 18, 45,
     46, 47, 95, 96, 97, 99, 100, 103, 105, 185, 186, 197, 228, 232, 236, 252, 263, 283, 296); 0x768560 Respect → 4
     (58 rows); the vcall thunk 0x4C38C0 for the object's vt +0x418 (8 rows: 11, 12, 13, 14, 275, 276, 277, 278):
     `GameThingWithPos::…Eating` → 1, `Living::AttitudeToCreatureEating` 0x768580 → `vt+0x2C8(0) ? 3 : 1` (3 only for
     a Villager or SpecialVillager). The rows of each family are listed in openblack's code.
  4. Fear (3) is forced when mind +0xF50 == 4, `[mind+0xF58]` vt+0x2C returns 6, and
     `[Creature+0x160]+0x28 == [mind+0xF58]+0x30`.
  5. A result of 1 does nothing. If the attitude changed, or it is 3, it is stored with +0x18 = turn. Only when it
     changed is `fn_004C9FE0(creature, 0x21 = CREATURE_HELP_TYPE_SHOW_TOWN_ATTITUDE_TO_CREATURE, att, 0, 0, 0)` called.
  6. **Timeout:** while attitude != 1, `secs = (turn − info+0x18) / (1000 / [0xD01A38])` (unsigned integer). Durations
     {0: 15.0, 2: 10.0, 3: 30.0, 4: 10.0}, index ≥ 5 → 0. When `secs > dur`, attitude becomes 1 and +0x1C = turn.
- **Attitude values:** 1 = none, 3 = fear, 4 = respect; 0 and 2 unnamed. Five help texts
  (`HELP_TEXT_TOWN_ATTITUDE_TO_CREATURE_01..05`, 1083-1087) fit values 0-4. No writer of 2 was found.

##### The emergency's creature part

- `Town::ProcessTownEmergency` 0x7477A0, `CallAllVillagersToTownEmergency` 0x747890 and `SetInStateOfEmergency`
  0x7479A0 have **no creature code** (no +0xA4C read, no call that reaches a creature).
- The "creature part" openblack's `TownEmergency.cpp` refers to is in **`Town::UpdateAggressor` 0x73C9B0, at
  0x73CAA3-0x73CB2D** ([Buildings](../world/buildings.md); `Town.h:247`), after the slot update `fn_0073E0F0`:
  - `obj = EffectValues+0x28`. If it exists and `IsCreature()` (vt+0x34): `c = CastCreature()` (vt+0xA4),
    `p = c->GetPlayer()`. If `p` exists, `c` is not the town player's creature (+0xA4C) and
    `p->IsMemberOfThisPlayer(MyInterfaceStatus)`, it calls `fn_0071C960(guidance, c, effect)`: if `[0xC221CC]` is set
    and `HelpSpritesPlayNow(0xB)`, then `HelpSpiritSay(GetRandomSample(0xD99D08), 0xB)`.
  - Otherwise it calls `fn_0071C9F0(guidance, town, effect)` (sample group 0xD) when the caused player is local.
- **Help-spirit audio only, no game state.** It changes nothing in the verification checks.

##### The satisfy activity and `GetVillagerActivityDesire`

- `Town::SetVillagerActivity` 0x73FF10 (`Town.h:169`): best = the football's vt+0x4C (Town +0xEA4); then tries the
  town player's creature (+0xA4C) and the artifacts (+0x994, next +0x20), each with a strict `>`. If best == 0 it
  returns 0, otherwise it calls the winner's vt+0x50 (matches [The villagers](../beings/villager-states.md)).
- Creature vtable `??_7Creature@@6B@` 0x8CC810: +0x4C = **0x401870 `GameThing::GetVillagerActivityDesire`**
  (`fld 0.0; ret 4`), +0x50 = `GameThing::SetVillagerActivity` 0x401880, +0x54 = the GameThing default.
- None of the four 0xB44-sized vtables (Creature, Living, SpecialVillager, Villager) overrides +0x4C, and only
  Creature overrides `CastCreature`, so there is no subclass. The only overrides are Football 0x532220, TownArtifact
  0x4262D0 and Town 0x73FF00. `Creature.h` declares none.
- **The creature's part is always 0 and never wins.**

##### `GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire`

`GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire` 0x4C80F0: 0x3A bytes; `Player.h:183-187`; TU
`CreatureDirectControl.cpp` 0x4C5F80-0x4C8440.

- `c = player+0xA4C`. If `c` exists and `Creature::CanSeePos(pos)` 0x477440, it calls
  `fn_004C8050(&mind[c+0x164]+0x18C80, desire, weight)`, which checks `-1 < d < 17` and sets
  `a[0xA0/4 + d] = clamp(a[...] + weight, 0, 1)`. The sister `MakeCreatureEmpathiseWithPlayer` 0x4C80B0 does the same
  through `fn_004C8000` on `a[0..39]` (CREATURE_DESIRES). Neither tests whether the creature is available or alive.
- **The add** (0x4C8000 / 0x4C8050): `v = w + v` stored, then `v < 0` **or unordered** (C0 of `fcom` with 0) → 0, else
  `v > 1` → 1. The player guard is `cmp d, 40; jge` only (signed): a negative `d` writes before the array. The town
  guard is `d > -1` and `d < 17`.
- **`Creature::CanSeePos(pos)` 0x477440**: `look = ConvertScawenAngleToGameAngle(GetLookDirection())` (the float
  stored first, 0x477454); `to = GetAngleFromXZ(creature+0x14, pos)`; both `& 0xFFFF`, `d = |look − to|`
  (cdq/xor/sub), `d > 0x400` (unsigned) → `0x800 − d`; seen when `d <= 0x2AA`, or else when the high words of x and z
  of `pos` equal the creature's (+0x16, +0x1A: the same map cell). `LH3DCreature::GetLookDirection` 0x480C90: when the
  look target +0x48C0..+0x48C8 is all 0 (`fn_00482D40`), or equal to the 3D position +0x78..+0x80 in all three, the
  body's yaw +0x84; otherwise `fn_007FAA50` (GetYAngle, see [Coordinates and angles](../engine/coordinates-and-angles.md)) of target −
  position, each a float subtraction.
- **`GetDominantDesire` 0x4C7ED0**: `result = 40`; for d = 0..39, `CreatureDesires::IsActivated(d)` and `v > 0` (not
  unordered) → `v = 0`, `result = d`. The last one wins and every one found is zeroed. `IsActivated` 0x4DCDA0 is true
  when the desire's own flag is set (mind desires +0x164, `[this + d*4 + 8]`) **or** when bit 0x2000 of the game flags
  (g_game+0x14) is set (0x4DCDAC..0x4DCDB8). The only write of that bit is 0x6433B1, at start-up in PCMain: it is set
  when the registry value `GatheringFlag` (under `Software\Lionhead Studios Ltd\Black & White...`, 0x643380) is read
  and non-zero.
- **The personality scroll** (`CreatureRoom::MakePersonalityScrollText` 0x788D20), after the attention line (text
  0x4AA): `GetDominantDesire` (0x788F23); when it returns less than 40, `TempleRoom::AddText(0x4AB)`. That text's case
  in `AddText` (jump-table entry 0x2A, 0x79B61B) calls `GetDominantDesire` **again** (0x79B651) and writes its line (DB
  texts 0x4AB with 0x248) only when that one is also below 40; otherwise it jumps to the common tail 0x79C3DC, which is
  one `AddNewLine`. The first call has just zeroed every value it found, so the second always returns 40: the line's
  text is never written, and a perceived desire only adds one line end. Then, whatever was found, one `AddNewLine`
  (0x788F41), the loop over the creatures it knows (texts 0x4AC..0x4AE), `AddNewLine` (0x788FB3) and the four beliefs
  (0x4B0..0x4B3). The function is called with 0 or 1 (on the texture, or the text in front) from `CreatureRoom::Draw`
  each frame while it is the focused scroll (0x787052, and 0x78706E once the zoom is back at 0.5 or under), when the
  focus leaves it (0x786FF6), from `InitEngine` (0x7885C4) and from `fn_00789730` (0x78979A, next to
  `CreatureScrollLikesSet`; not read). Each call is a destructive read.
- **The ring push** (0x4C7F60): the turn test first; then per value, player 0..39 then town 0..16: `ring[index] = v`,
  `index + 1`, back to 0 when it reaches 30 (equality only), `count + 1` while `count < 30` (unsigned). The fade
  (0x4C7F20) runs before the push in the same call.
- **The spell's site** (`fn_00721730`, 0x71FE5D, first thing in `Spell::InitWithPos`): for +0x98 and +0x9C of the
  effect info, each when `< 40` (signed), `GetPlayer` (vt +0x1C) → `MakeCreatureEmpathiseWithPlayer(d, 1.0, pos)`; then
  +0xA0 when `< 40` → `...TownDesire(t, 1.0, pos)`; `pos` is the cast point. No null test of the player.
- **The feedback's site** (0x4E10AE..0x4E10D5): `f > 0` → 1 COMPASSION, else (0 or unordered too) 2 ANGER, weight
  `fabs(f)`.
- **The object** at mind +0x18C80 is a `CreaturePerceivedPlayerDesires`, at +0x40 inside `CreatureAttitudeToPlayer`
  (mind +0x18C40; ctor `fn_004C8130`, called from the `CreatureMental` ctor at 0x4D23BE;
  `CreatureAttitudeToPlayer.h:21-28`). Layout: +0x00 float[40]; +0xA0 float[17]; +0xE4 57 histories of 0x80 bytes
  (30-float ring, index at +0x78, count at +0x7C). The header's 0x20-byte history struct is a placeholder: the 0x80
  stride totals 0x1D64 = 0x1DA4 − 0x40.
- **Each turn:** `Creature::ProcessState` → `fn_00477A00` (0x477A88) → `fn_004C7EB0`: `fn_004C7F20` multiplies all 57
  values by **0.9995** (float at 0x8CF3D4); `fn_004C7F60` pushes them into the rings only on even turns (skips when
  `g_game+0x205A40 & 1`). `fn_004C7EB0`, `fn_00477A00`, `fn_004C7F20` and `fn_004C7F60` have no other caller.
- **The gate**: the call at 0x47338C sits in the block 0x473306..0x47338C, run only when the dword at 0xD00DE8 is not 0
  (0x4732FA `cmp [0xD00DE8], ebx` with ebx = 0 since 0x472E23; `je 0x473391`). The same dword gates
  `Creature::AddToCheckSum` (0x473B43). 0xD00DE8 is `GGlobal::Global` +0x2D2C8 [V]. It lies past the end of `.data`'s raw
  bytes (0xC3D000), and the image holds its address only at the two reads (and as bytes of a call's rel32 at
  0x78C1AE), because its one write is base-relative: the GGlobal constructor sets it to 1 (0x590F2A,
  `[esi + 0x2D2C8]`, run by the static initializer 0x54A6D0) and nothing writes it again (no absolute write, and no
  indexed access reaches 0xD00C00..0xD00DE8). Its neighbours 0xD00DE0 / 0xD00DE4 / 0xD00DD8 / 0xD00DD4 are set to 1 the
  same way (oracle on 0x54A6E0; 0xD00DE4 gates `Update3DCreatureFromAttributes` at 0x4737B5). The same block also holds
  `fn_00477130`, the creature's map position following its 3D body (`MoveMapObject` of the body's +0x78 position, its
  only caller), `fn_004EF510` (strength, `ModifyStrength`, its only caller), and the calls into mind +0x1AA80
  (`fn_004DF620`) and `fn_004D82A0`. The flag lies in `GGlobal::Global`, whose constructor sets 0xD00DD4..0xD00DF0 to
  1 at start-up ([A creature with no plan](plans-and-needs.md#a-creature-with-no-plan)), so the fade runs every
  creature turn.
- **Readers:** the town-desire array (+0xA0) is only saved and loaded (`fn_004E79D0` at 0x4E7DA9 from `SaveMind`;
  `fn_004E84A0` at 0x4E8914, version ≥ 0x10). No gameplay reader of it or its histories was found. The creature-desire
  array is read by `CreaturePerceivedPlayerDesires::GetDominantDesire` 0x4C7ED0 (from
  `CreatureRoom::MakePersonalityScrollText` 0x788F23 and `TempleRoom::AddText` 0x79B651), which returns the last
  activated desire with value > 0 and zeroes those values. Feedback also adds to it
  (`UpdateAttitudeToPlayerFromFeedback` 0x4E10D5, index 1 or 2).
- **The other writers**: `Villager::EndPhysics` 0x5F0B53 (1 COMPASSION 0.1 or 2 ANGER 0.5), `Rock::InterfaceTap`
  0x6E7541 (3, 0.5), `GPacket::ProcessPacket` 0x63D24D and `fn_00653150`+0x1770 (0, 0.5).
- **The callers of 0x4C80F0**:

  | Caller | Call | Desire, weight | openblack |
  |---|---|---|---|
  | `CheckInteractWithWorshipSite` | 0x7572FD, 0x75738B | 9, 0.5; 7, 0.5 | state 171 not ported; not in bw1-decomp's source |
  | `CheckInteractWithAbode` | 0x7574DF | 5 FOR_ABODES, 0.5 (no player test) | state 172 not ported (disciples) |
  | `CheckInteractWithField` | 0x7575F6 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithFishFarm` | 0x757674 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithTree` | 0x7576E8 | 1 FOR_WOOD, 0.5, with a player | state 175 not ported |
  | `TakeWoodFromTree` | 0x75FBF1 | 1 FOR_WOOD, 0.5, with a player | **published** (`VillagerForester.cpp`) |
  | `fn_00721730` from `Spell::InitWithPos` | 0x71FE5D | `GMagicEffectInfo` +0xA0, 1.0, plus +0x98 / +0x9C into 0x4C80B0 | **ported** (`Magic/Core/Spell.cpp` `InitWithPos`) |

  Indexes 0..16 are `TOWN_DESIRE_INFO_*` (`include/chlasm/Enum.h:1481-1500`, openblack's `TownDesireInfo`).
  bw1-decomp's source shows only five calls (`VillagerCheck.cpp:103,136,152,166`, `VillagerForester.cpp:164`); the two
  worship-site calls and the spell call are from the exe.

#### The computer player's readiness

What the computer player's creatures wait on (Land 2's arrival; the creatures themselves are on
[Land 2's creatures](players-creature.md#land-2s-creatures)):

- `GScript::ComputerPlayerReady` 0x6EEC40 (COMPUTER_PLAYER_READY): false without a player (no message); with no
  computer player (+0x944) it logs "Jonty - No computer player" and pushes false; otherwise true when the computer
  player's action state (+0x34) has no sub-actions left (its count +0x2C is 0). Confirmed with the oracle (stubbed
  POP / PUSH / GetPlayer).
- `PlayerActionState::IsBusy` 0x650840: its sub-action count (+0x2C) is not 0 (bw1-decomp `PlayerComputer.h` puts the
  sub-actions at +0x28).
- The arrival scene waits twice on COMPUTER_PLAYER_READY (`GiveScaffold` ip 76680, `ShowDisciple` ip 76760), which
  FORCE_COMPUTER_PLAYER_ACTION ("FindPlaceToUseScaffoldForBuilding") fills from the action table built at 0x66127A and
  the computer player's turn (`GComputerPlayer::NewProcess`) empties.
- `GScript::LoadComputerCreature` 0x6FD2F0 makes Khazar's and Lethys's creatures, reached only after `EndEnterLand2`.
- The computer player's own forced desire goes through `SetCheatDominantDesire` at 0x654F5D in `fn_00653150`
  ([The timed dominant desire](leash.md#the-timed-dominant-desire)).

#### Reactions to miracles

A creature is a Living: `Reaction::ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 gives it every reaction spread over
its cell, as it does the villagers and the animals (see [Reactions](../beings/animal-life.md#reactions)). The table entries are from
the CRT initialiser 0x6E0E80, the vcall thunks resolved through the `Creature` vtable.

- **The spread's body for a creature** (0x6E3FF3..0x6E42E7): `IsAvailableForReaction` (vt +0x984), the shield test
  (`fn_0072B990`), the half-Manhattan distance, the type's turns before reacting again (table +0x40), then
  `IsAvailableForBeliefButNotReaction` (vt +0x988, `Living`'s 0x417090: always 0 for a creature). With no reaction held
  (+0x94): the score (`fn_006E4620`: `isReacting[type]` of the info at +0x28, the species' `CreatureInfo` row, then the
  type's priority within `maxReactionDistance`) > 0 and the records (`fn_006E4340`) → `turnCreated` stamped,
  `StartReacting` (vt +0x994). With one held: the switch rule (the same type only with the table's +0x04 flag and
  another reaction; the new score above the held one's at the distance to the held reaction's cell; the held one lasted
  `max(10, cur / new × 20 − 10)` s, or 1 s for 16), then `SetReactionDoneWhen(new type)`, the creature off the old
  reaction's followers (no record refreshed, +0x94 not cleared), and `StartReacting`.
- **The type table** (0xC09CC0 + 0xC0 × t; +0x10 setup, +0x20 priority, +0x30 turns to react, +0x40 before again, each a
  vcall thunk):

  | Type | Setup | Priority | +0x30 / +0x40 |
  |---|---|---|---|
  | 3 FLEE_FROM_SPELL | `Creature::SetupFleeFromObject` 0x4F2B00 (vt +0x9A4) | `Creature::FleeFromSpellPriority` 0x4F2AB0 (vt +0xA38) | the creature's standard (vt +0xAC0 / +0xAC4) |
  | 13 REACT_TO_MAGIC_SHIELD | `Creature::SetupReactToMagicShield` 0x4F3090 (vt +0x9D4) | `Living::ReactToMagicShieldPriority` 0x5F1860: info[13].priority | `Living` 0x5F1AD0 / 0x5F1AF0 (vt +0xAE4 / +0xAE8), which call the creature's standard |
  | 21 LOOK_AT_NICE_SPELL | `Creature::SetupLookAtNiceSpell` 0x4F2B40 (vt +0x9B0) | `Living::LookAtNiceSpellPriority` 0x5F16A0 (vt +0xA40) | the creature's standard; field +0x00 of this row is 1 |
  | 37 REACT_TO_IMPRESSIVE_SPELL | the same as 21 | the same as 21 | the creature's standard |

  An earlier study read type 21 as having no +0x30 / +0x40 entry: it has them, the same as 3 and 37.
- **Priorities.** `Creature::FleeFromSpellPriority`: 0 if the initiator is a `Spell` whose creator (+0xA0) is this
  creature, else `Living::FleeFromSpellPriority` 0x5F1630: `d = FastDistance(creature, initiator)` (map units);
  `d >= 600000` → info[3].priority; else `priority + (600000 − d) × 100 / 600000` (signed integer division), returned
  as a byte. `Living::LookAtNiceSpellPriority`: 0 for a spell this creature cast, else **info[21].priority**
  (0xD4FEF4), for 37 too.
- **Turns.** `Creature::StandardNumGameTurnsToReactFunction` 0x4F29C0: `ftol(((max − d) × imp / max + 1 − imp) ×
  numGameTurnsForCreatureToReact)`; `...BeforeReactingAgain` 0x4F2A20: `ftol((imp × d / max + 1 − imp) ×
  numGameTurnsForCreatureBeforeReactingAgain)` (max = maxReactionDistance, imp = howImportantIsDistance).
- **Availability** `Creature::IsAvailableForReaction` 0x4F2820, all of: `IsFunctional` (`Living`'s, from
  `IsAvailable`); +0x1060; not in the dance editor; Flags +0x24 without 0x80 and 0x400; mind +0x1C14 clear or +0x380
  set; not `IsFighting`; mind +0xF60 ≠ 21 and its friend's (mind +0x1BE8) ≠ 21; +0x384 (fainted), +0x10AC, +0x10B0 all
  0; Flags without 0x10; while mimicking (mind +0x1C38) only a type whose priority is above 150; mind +0x1C10 and
  +0x10F4 clear. What is known of the fields:
  - +0x1060 is "reactions on": `Creature::Initialise` sets it to 1 (0x4743FA); `GScript::CreatureReaction` 0x6F5600
    (CREATURE_REACTION: the thing popped first, then the value) writes it; the creatures made by the script's create
    path (the CREATE native's creature case, 0x6F14BE, with ebp, cleared at 0x6F11B0, both as the player and as the
    value) and by `CREATURE_CREATE_RELATIVE_TO_CREATURE` (`fn_004F6610`, 0x4F666B) start with 0; `SwapMinds` sets it
    to 1 on one of the two creatures (0x47C3BC) and 0 on the other (0x47C3CA). The challenge script never calls
    CREATURE_REACTION. A scan of the exe for the displacement finds no other access: written at 0x4743FA, 0x47C3BC,
    0x47C3CA, 0x4F666B, 0x6F14BE and 0x6F5673, read only at 0x4F2833. The Land 1 creature is made by
    `LOAD_MY_CREATURE` (`Creature::Load` 0x4E7FF0 → `LoadFullyQualified` → `CreateCreature` 0x474B50 → `Create`
    0x474A20 → the constructor 0x473B90, which calls `Initialise` at 0x473D9A), so its reactions are on; a saved
    game's `Creature::Load` (0x4E631A) calls `Initialise` too.
  - The test list (0x4F2820..0x4F2924) does not read the mind's "active" flag +0x20CE4 that the freeze spell clears: a
    stilled creature takes a reaction up, and only its learning (`ConsiderLearningAction` returns at once with the
    mind not active) is stopped. Nor do `ChangeSource` 0x4DE730 and `ForceActivityAndForceAction` 0x4C44B0 read it
    📼 1.20 only: a frozen mind is frightened and replanned.
  - +0x10B0 is set when its life runs out, before it faints (`ReduceLife` 0x47DDE2, `ApplyEffect` 0x478EDB..0x478F00):
    knocked out. Mind +0x1C14 is set with it and by `Faint`.
  - +0x10AC (also the hand's locked-select test), +0x10F4 and mind +0xF60 = 21: not named; mind +0x1C10 is being
    teleported, Flags 0x400 a script's control and 0x10 the hand's locked select. Their writers: `StopWhatIAmDoing`
    clears +0x10AC, +0x10B0 and +0x10F4; `fn_0047CFB0` writes +0x10F4.
- **`Creature::StartReacting`** 0x4F26D0 copies mind +0xF4C..+0xF74 to +0xF7C..+0xFA4 (the plan, kept), then
  `Living::StartReacting` 0x6E4590 calls the type's setup (table +0x10). The kept plan is written by 0x4F26D0,
  0x47A1D0 and 0x4F36C2 and read only by `RemoveBeliefsAboutDeletedObjects` (0x4D8737..0x4D877B [V scan]), so
  nothing restores it.
- **Actions 156 and 157** (rows 0x9D1678 + 0x50·a filled at 0x495D80) 📼 1.20 only: 156 "RunAwayFromPos"
  (`ConstructSubActionsForRunAwayFromPos` 0x4AD2F0): `SetRequiredSpeed(info +0x258)`; `GameRand(2)` == 0 → sub-action
  10 with animation 61; the flee point 0x47A500 from mind +0x1C84 (GameRand(40) + info +0x2E8 along the creature −
  point direction, (1, 0) when both parts are under 0.001, then a valid-point search 400 / 10); sub-action 8 to it
  within 5 × `GetHeight`. 157 "ExaminePos" 0x4AD490: `GameRand(2)` == 0 → 7 turn, 80 point (float 1.0) and a flag;
  then 8 go within 5 heights, 7 turn; with the flag 24 wait ftol(turns a second × 2.1), else 80 point (float 2.0).
  What sub-action 80's float means is not read.
- **`SetupReactToNastyMagic`** 0x4F3210 (from type 3, with the spell's magic +0xB4, read without a test that the
  initiator is a spell): `ChangeSource(19, 0.5)`; on a Rope leash (leash +0x12C → +0x1C == 2), or with mind +0x164 ≤ 0
  (or unordered), desire 6 / action 157 `CREATURE_EXAMINE_POS`, else 5 / 156 `CREATURE_RUN_AWAY_FROM_POS`; mind
  +0x1C84 = the initiator's position; `ForceActivityAndForceAction(desire, GetBeliefAboutObject(self), action, 0, 0, 1)`.
  Then: a spell whose creator has a spell icon (vt +0xB8) and whose seed (+0xAC) is gone → **return** (no
  AddReaction). Learning (`fn_004F2930(1, magic)`: `ConsiderLearningAction(1, magic)` unless magic is 6) unless the
  creator is set, not a creature, and the spell's player is of type (+0x8E0) 2 or 3, or the seed's byte +0x71 is set
  (`fn_00721910`); after learning from a spell, the seed's +0x71 = 1 (`fn_007218F0`). A thing that is not a spell
  always teaches. Then, on any leash and with the miracle known (`KnowsAction(1, magic)`), a 64-cell spiral from the
  leash's position for the first object other than itself → `fn_004F8850` (a pending cast of the miracle at it, mind
  +0x21D0..+0x21E0) and +0x380 = 1. Last: +0xBC = the initiator, `AddReaction(reaction, 0)`.
- **`SetupReactToNiceMagic`** 0x4F30B0 (types 21 / 37 with the spell's magic; 13 with 19 SHIELD; also
  `SetupReactToMagicTree` 0x4F2E50 with 21 WOOD): the reaction's player set, not the creature's and not `IsAllied`
  (0x64D5D0: +0x950[other] > 0) → return. Not a spell: magic 21 → return; else examine and learn. A spell: icon without
  seed → return; creator set, not a creature, player of type 2 (not 3) → neither examine nor learn; seed +0x71 set →
  neither. Otherwise `ForceActivityAndForceAction(6, belief, 157, 0, 0, 1)` with +0x1C84 the position, the learning,
  and the seed's +0x71 = 1. Then +0xBC and `AddReaction` (also when it did not look).
- **The seed's +0x71** is cleared when a seed is made (`fn_00728140`, from the three `SpellSeed` constructors) and
  written only by the two setups.
- **`Creature::AddReaction`** 0x4F2680: +0x94 = the reaction, the creature at the head of its followers (+0x18, count
  +0x1C), `UpdateHowImpressed(reaction, 1)` 0x4F2780: nothing for its own reaction; else the initiator's
  `GetImpressiveValue(this, reaction)` (vt +0x1B0); the reaction's player is the creature's → mind +0x18C64 += 4 × it;
  else, the initiator being a creature, `UpdateHowImpressiveIsCreature(it, 12 × value)`.
- **The end.** `Living::ProcessLiving` 0x5EC8C6 runs `Living::ProcessReaction` 0x5F1270 for every Living, creatures
  too, before its `ProcessState`: the reaction not available → `StopReacting`; the object (+0xBC) none or not
  available → `StopReactingAndSetState`; `turn − record turn` (signed) above the type's turns to react (table +0x30, at
  `GetDistanceInMetres` to the object) → `StopReactingAndSetState`. A reaction's `ShutDown` 0x6E4720 calls
  `StopReactingAndSetState` (vt +0x99C) on each follower. For a creature `ResetStateAfterReacting` 0x477F30 is
  `StopReacting`, so all of these are `Living::StopReacting` 0x5F1140: off the followers, the type's record refreshed
  (`fn_005F0FE0`), +0x94 = 0, +0xBC = 0.
- The fire reaction is `Creature::ReactToFirePriority` 0x4F2EE0 and `SetupReactToFire` 0x4F2F70.

### Sources

- bw1-decomp: `src/Black/Player.h`, `include/chlasm/CreatureEnum.h` (the deeds), `include/chlasm/Enum.h` (the town
  desires), `CreatureAttitudeToPlayer.h`, `Town.h`, `StoragePit.cpp`, `Object.cpp`, `VillagerCheck.cpp`,
  `VillagerForester.cpp`, `PlayerComputer.h`; every body read in the exe with capstone and the xref index; the
  computer player's readiness pinned with the oracle.

## openblack

**Partial.** Every deed the original reports has a site that reports it (10, 12, 13, 19, 38 and 39 are never reported
there): the resources (0, 2, 4, 7, 9 and the thefts 43 to 45), the spell hits through `magic::MiracleDeed` (1, 3, 5,
8, 17, 18, 20, 33, 34, 42), the scaffold's house (6), the tree (11), the artefact (14), the throws (15, 16), the sea (21), the
disciples (22 to 31), the landings (32, 35, 36), the water on crops (33), the rock tap (37), the sacrifice (40) and the
toy (41). The perceived desires, their reader, the creature's miracle reactions (types 3, 13, 21, 37) and the town's
attitude step are ported; the gate's magic step and the animal's sea deed are not. The port
decisions taken from raffclar's code are in [Port decisions: the creature](../project/port-decisions-creature.md).

#### What openblack does with a deed

- `creature_watching::Deed` (`src/Creature/CreatureDeeds.h`): the 46 deeds in the table's order, `k_DeedCount` = 46,
  pinned to the size of `InfoConstants::creatureMimic`.
- `ecs::creature_mimic` (`src/ECS/CreatureMimic.{h,cpp}`): `Consider(player, deed, object, magic)` publishes
  `events::PlayerDeedForMimic` (`src/ECS/Events/CreatureMimicEvents.h`) with the object's position, read through the
  const registry (the origin for something with no `Transform`). Its handler, registered in `Locator.cpp` with the
  other game handlers, passes the deed to `CreatureMindSystemInterface::PlayerDid(player, deed, point, object)` when
  the mind system is there.
- **`PlayerDid` takes the player, and only creatures whose `Creature::owner` is that player watch** (a change to
  raffclar's, whose `PlayerDid` has no player and walks every creature). This is the original's per-player entry
  point; what the original does with `magic` is not ported (Pending).
- **No draw decides whether the creature starts to copy** (step 6 of [the gate](#gate-in-considermakingcreaturemimicplayer)):
  the table's +0 is a rank, not a chance. `creature_watching::MayStartMimicry` checks the phase (at least 3), the
  learning leash, the reaction priority (below 150; the port passes 0) and the sight (within 150 m), then the rank: above
  0 when nothing is copied, else the running mimicry has reached copying the desire (stage 2), or the new deed is another
  one ranked at least as high. `BeginMimicry` then starts it with no test; `StartMimicry` is the two together. Until
  2026-10-17 openblack rolled `GameFloatRand(1.0f)` against the rank, a draw the original never makes. It drops the magic
  and has no hand, leash-works or player-type gate (Pending).
- **The deeds' sites and the gate** (2026-10-17, from raffclar's #518, #546 and #551, unverified against v1.42): the
  resources given by an interface (`object_resources::ReportResourceDeed`: a pit's own deed, then any building going up,
  a worship site's food, a workshop's wood), a put-down that lands at once (`creature_mimic::ConsiderLanding`, the
  hand's player), a toy let go, a disciple's first landed turn (`villager::ShowDiscipleToCreature`), a rock tap before it
  splits, a scaffold's end of physics with a player (deed 6, ours) and the altar's sacrifice (`ecs::altar_offering`).
  `NoticeDeed` checks the leashable creature, the sight by `CanSeePos`, the reach in metres against the cells swept
  (`WithinCopyReach`, vb105 kept), the reaction's priority, then the miracle's step 7 (`CastDeedGate`, the times needed as the float product, `WatchMiracle`,
  `GameRand(2)`), and stirs the deed's first source by 0.1 and source 54 by 0.3 (Pending: the end of a flight and the
  bodiless landing; the animals' sea deed). The landing deeds (32, 35, 36) since 2026-10-21: `creature_mimic::ConsiderObjectLands`
  for the player whose interface let go of the thing last (`ecs::last_dropped::WhoLastDropped`), from a put-down that
  lands at once (`physics::from_hand::InitialisePhysicsFromHand`; until then always for PLAYER_ONE) and from every hand's
  drop (`HandSystem::RemoveFirstFromHand`, the 0x18 result in `HandleApplyResult`, a computer player's
  `GameHandRunner::LetGo`), after the drop's record. Test
  `CreatureMimicSiteTest.AThingAHandLetGoOfIsReportedForThePlayerWhoseHandItWas`.
- **On Land 1** the player's own creature is there, so the deed sites below that run on that land (villagers dropped in
  the sea, water on a player's fields, trees replanted from the hand) reach its mind. The empathy sites publish an
  event the minds take (below); nothing is made, moved or drawn by them.

| Deed | Site (original) | openblack | Notes |
|---|---|---|---|
| 11 PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | `HandSystem::Replant` (`HandTrees.cpp`) | after the spot visual, before the tree goes back in its cell; player PLAYER_ONE, as the alignment line beside it ([Dropping and replanting](../hand/grab-hold-and-throw.md#dropping-and-replanting)). openblack updates the alignment before the deed, the original after it (Pending there) |
| 16 DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286 | `BuildingDamageSystem::ReactToImpact` (abodes and storage pits; `StoragePit` 0x733730 and `TownCentre` 0x744380 only forward to the Abode's) | before the test of what hit it; with a player and the **building's own body** marked FROM_HAND, which never happens in practice ([Physics](../physics/physics.md)); `creature_mimic::ShouldMimicBuildingHit`. The port follows the exe, not the first spec, which had `hit->flags` and would fire on every hand-thrown rock hitting a house (coordinator decision, 2026-10-08); the first guard is split: the valid test, then the mimic, then the destroy test (pure code between them) |
| 21 THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED | `ecs::HasSunk`, villager branch, through `creature_mimic::ConsiderThrownInTheSea` | after `IsAvailable` (0x750AB5), before the dead test; the player is the last dropper's (`VillagerLastInteraction`, the hand's). openblack's animal branch of `HasSunk` has no availability test (Pending) |
| 33 CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64 | `water::ApplyWaterSpell`, field branch | after the any-object part (0x528F3A), before the fire test and the sowing; only when the spell has a player; magic always 0x16 (`MagicType::Water`), also for the power-up |

- The pit's `GameRand(2)` is already drawn (`StoragePitStore::DoCreatureMimicAfterAddingResource`).
- The spell hits' deed choice is `magic::MiracleDeed` (`Magic/MiracleDeeds`), checked against the jump table for every
  magic type, and called by the magic system for the thing a miracle affects (`MagicSystem.cpp`, with the field, the
  fire, the built storage pit, the worship site and the building going up as its targets). The field's own report is
  ported (`SpellWater.cpp`).
- The creature's activity desire for the town: openblack's `CheckSatisfyRelaxation` already gives 0; only the comment
  in `VillagerSatisfy.cpp` needs changing (Pending).
- The town's attitude step (step 16) is ported: `town_creature_attitude::UpdateAttitudes` runs as step 16 of
  `ProcessTown` (`TownProcess.cpp`), with the numbers above ([Pending](#pending) keeps its two differences).
- COMPUTER_PLAYER_READY answers from the computer player's hand (2026-10-18): `ComputerPlayerReady` (CHLApi.cpp)
  pushes false for a player the land does not have, else `ComputerPlayerSystem::IsHandReady`, true when the hand has
  no step left (`computer_hand::IsReady`, `ECS/ComputerHand.cpp`); FORCE / QUEUE_COMPUTER_PLAYER_ACTION give the hand
  its steps.

#### The perceived desires in openblack

Each published villager site calls `creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), desire, 0.5,
villager)` on its success path only. It publishes `events::CreatureEmpathyWithTownDesire` (player, desire, weight, the
villager's position) when there is a player. The mind side:

- `Creature/PerceivedDesires.{h,cpp}` (pure): the 57 values, `Increase` / `IncreaseTown` (the add above), `Fade`,
  `TakeDominant` (`GetDominantDesire`, none for 40), `CanSeePos` and `LookYaw` (`GetLookDirection`). The field is
  `CreatureMindState::perceivedDesires`.
- The mind turn calls `Fade` right after `++mind.turn`. The 57 rings are not ported: nothing but the mind save reads
  them, and they come with the save.
- `CreatureMindSystem::EmpathiseWithPlayer` / `EmpathiseWithTownDesire` (the two `GPlayer` functions): the player's
  creature through `LeashSystemInterface::PlayersCreature`, then `CanSeePos` and the add. The look: the head's
  `CreatureAnimation::lookAt` as the 3D look target; the body's yaw is minus openblack's locomotion heading (a heading
  `h` faces `(−sin h, −cos h)`, a body yaw `y` faces `(sin y, −cos y)`), read from the body's rotation before the
  creature first moves. The creature's map position is `FromMetres` of its Transform.
- The town-desire event is passed to the minds by `creature_mimic::AddMimicEventHandlers`; the spell site in
  `InitWithPos` (only with a player); `ReceiveFeedback` adds 1 or 2 after the slight-feedback test.
- `IsActivated` reads only the desire's flag, not the game flag 0x2000 (Pending).
- The reader: `TempleScrolls::Write` of the mind scroll calls `CreatureFacts::takeGodsDesire` each time it writes it,
  as the game does: openblack's builds are the room's set-up, each turn of the held scroll, the focus changes and each
  frame of the text in front (`TempleScrolls::Create`, `Hold`, `SetFocus`, `AppendFocusedText`), and openblack's
  ImGui cave screen without a temple each frame its page is shown. `CreatureCaveSystem::TakeGodsDesire` is
  `TakeDominant` over the activated desires of the mind. A desire found writes one line end, and the line end at
  0x788F41 is written always.
- The values are in no state-hash part and draw nothing; the scroll is only in the temple, which no check enters.

#### The miracle reactions in openblack

- `ecs::effects::reactions` gives a `components::Creature` the `LivingClass::Creature` handler,
  `ecs::creature_reactions` (`ECS/Systems/Implementations/CreatureReactions.{h,cpp}`), registered with the others in
  `Locator.cpp`. The rules are pure in `Creature/CreatureReactionRules.{h,cpp}`. Only types 3, 13, 21 and 37 are
  taken; every other type scores 0 (each has its own `Creature::*Priority` / `Setup*`, separate items).
- The reaction held is `components::CreatureReaction` (+0x94, its type, +0xBC, and whether it is a follower).
  `ProcessReaction` runs at the start of each creature's mind turn (`CreatureMindSystem::ProcessTurn`), not in the
  living list, which holds no creatures in openblack.
- The availability models IsAvailable, fighting, openblack's faint activity for +0x384, `CreatureKnockedOut` for +0x10B0
  and openblack's mimicry for +0x1C38; +0x1060 is on while the creature has no `components::CreatureReactionsOff`: on
  for every creature but those the CHL `CREATE` makes (`player_creature::MakeForScript`), until `CREATURE_REACTION`
  turns it on; the rest is in Pending. As the original, a stilled mind (`CreatureMindState::paused`) is not tested: it
  takes the reaction up, grows afraid and replans, and `WatchMiracle` alone skips the learning.
- The setups call `CreatureMindSystem::ReactToNastyMagic` / `ReactToNiceMagic` (raffclar's agendas,
  `Creature/CreatureMiracleReactions`) for the forced activity, and `WatchMiracle` (one creature's sighting, the body
  of `SeeMiracle`) for `ConsiderLearningAction`. The fear read is `Desire::Fear` after the source grew (inferred for
  mind +0x164). The seed's +0x71 is `SpellSeed::learnedFrom`. The player's type is `PlayerMagic::playerType`. A
  reaction has no player when its miracle has none (`Spell::hasPlayer`); a nice miracle of a player neither the
  creature's nor allied to it is ignored (`creature_reaction_rules::IsOthersPlayer` over the reaction's player's
  alliances).
- raffclar's agendas make actions 156 and 157's draws in the same order with the same 2.1 s (`FleeFrom` ports
  0x47A500); openblack's `PointAt` holds 5 s. The examine agenda points with the `PointAt` object order
  (`ObjectOrder::Kind::PointAt`, the hands' `PointAt`). openblack does not keep the reacting creature's plan, and
  behaves the same as the original, which never restores it.

### Test hooks

None of its own; `Consider`'s own tests pin the deed's fields.

## Pending

- **What the creature thinks its player wants**
  ([`MakeCreatureEmpathiseWithPlayerTownDesire`](#gplayermakecreatureempathisewithplayertowndesire)):
  - the other writers, `Villager::EndPhysics` 0x5F0B53 (1 COMPASSION 0.1 or 2 ANGER 0.5), `Rock::InterfaceTap`
    0x6E7541 (3, 0.5), `GPacket::ProcessPacket` 0x63D24D and `fn_00653150`+0x1770 (0, 0.5): queued as fixes; the
    villager sites not ported (worship site, abode, tree) come with those sites;
  - the reader: the original's `InitEngine` call and `fn_00789730` are not read, so openblack's scroll builds are not
    matched call for call; only how often the values are zeroed could differ;
  - `IsActivated`'s second case (game flag 0x2000 from the registry value `GatheringFlag`): without the value, as on
    the check machines, the two agree; what the option is for and the registry helper's success test
    (0x64338A..0x643393) are not read;
  - the 57 rings (30 past values each, written on even game turns) come with the mind save, and the turn parity with
    them;
  - the look: that openblack's `CreatureAnimation::lookAt` stands for `LH3DCreature` +0x48C0 and minus openblack's
    heading for the body yaw +0x84 is inferred (the heading convention is open).
- **Reactions to miracles** ([Reactions to miracles](#reactions-to-miracles)):
  - the fire reaction (`Creature::ReactToFirePriority` 0x4F2EE0, `SetupReactToFire` 0x4F2F70) is queued as a fix; the
    other types a creature takes (0, 4, 6 to 9, 16 to 20, 22, 23 and the rest) are each their own item and score 0;
  - the availability fields not named: Flags +0x24 bit 0x80, +0x10AC, +0x10F4, mind +0x1C14 against +0x380, mind
    +0xF60 = 21 (its own and its friend's); +0x1060 (reactions on) is cleared by the CHL CREATE creature case and set
    by `CREATURE_REACTION`; `CREATURE_CREATE_RELATIVE_TO_CREATURE` and `SwapMinds` are still to clear or set it;
  - the forced desire and belief (5 FEAR / 156, 6 CURIOSITY / 157 through `ForceActivityAndForceAction`) wait on the
    forced activity;
  - sub-action 80's float (1.0 or 2.0 in `ExaminePos` 0x4AD490) is not read; openblack's `PointAt` holds 5 s;
  - the fear read at mind +0x164 is taken as `Desire::Fear` 🔍 inferred;
  - `UpdateHowImpressed`: the initiators' impressive values (vt +0x1B0) and what they feed (mind +0x18C64,
    `UpdateHowImpressiveIsCreature`) are not ported;
  - the pending cast at the leash holder's first object after learning on a leash (`fn_004F8850`, +0x380 = 1): queued
    as a fix;
  - `ProcessReaction` runs in the creature's mind turn, not in the living list's order: queued with the creature's turn
    inside `Living::ProcessLiving`.
- **Mimicry** ([Mimicry and the town hooks](#mimicry-and-the-town-hooks)): every deed the original reports has its
  site ([What openblack does with a deed](#what-openblack-does-with-a-deed)); the sites imported from raffclar's lanes
  are not yet compared with v1.42 one by one.
  - the animal's THROW_IN_THE_SEA waits on the animals' `HasSunk` (below);
  - the gate's steps and `MimicPlayer` with magic (0x4EA900, 0x4EA670; read in
    [Gate in ConsiderMakingCreatureMimicPlayer](#gate-in-considermakingcreaturemimicplayer)): queued as a fix;
  - mimicry's stage 2 forces its desire with `SetCheatDominantDesire` for 120 s; openblack's copy uses
    `MakeFullyDominant` ([The timed dominant desire](leash.md#the-timed-dominant-desire)); queued as a fix;
  - the name of `GInterfaceStatus` +0x128 (the header's `LeashStatus`, compared with a `GPlayer*`) and physics flag
    +0x1D8 & 4 (FROM_HAND); whether the disciple off-by-one is the original's bug or the `VILLAGER_DISCIPLE` enum is
    wrong; no cross-check against the Mac build;
  - code comments to correct: `MapShield.cpp` (it is help-spirit guidance) and `VillagerSatisfy.cpp` (the creature's
    activity desire is always 0);
  - the animals' `HasSunk` availability test (`Living::HasSunk` 0x5ED375): queued as a fix;
  - no test pins the PLANT_TREE site in `HandSystem::Replant` (reached only through a real hand's end of physics for a
    tree); `Consider`'s own tests pin the deed's fields;
  - the town's attitude to the creature (step 16, `Town::UpdateAttitudeToCreature` 0x7437F0): the records are
    kept (`components::Town::creatureRecords`, `ecs::town_creature_attitude`: one per creature, the newest first,
    made with attitude 0; `AttitudeOf` reads 0 without one), the creature's last action carried out to its end
    (`CreatureMindState::lastCompletedAction`, written by `Complete`) and a held plan's validity
    (`creature_planner::IsValid`), 2026-10-20; the update is step 16 of `ProcessTown`
    (`town_creature_attitude::UpdateAttitudes`: the drop of an unavailable creature's record, the gate, the families
    of `creature_mind_tables::TownAttitudeFamilyOf`, the hungry-and-holding fear, `Store`'s stamping, the help
    message `Kind::TownAttitudeToCreature` and `TimeOut`), 2026-10-20. It replaced `CreatureTownAttitude`, one view per
    creature from its object action with its own timings, which only the debug spawner read; the spawner now lists
    each town's record. A villager taking up a creature's reaction makes the record (`VillagerCreature.cpp`,
    `town_creature_attitude::CreateRecord`), and every creature makes that reaction as it is made
    (`CreatureArchetype::Create`), 2026-10-20. (openblack) a turn
    longer than a second, which the original divides by zero on, never times out; a plan done to a thing that is gone
    is not judged as done to a villager (the original keeps the belief's type);
  - the multiplayer function `FUN_0064DA80`; the creature's mimicry of water on fields is in
    [What openblack does with a deed](#what-openblack-does-with-a-deed).
- **Land 2's rival creatures**: COMPUTER_PLAYER_READY now answers from the computer hand
  ([The computer player's readiness](#the-computer-players-readiness)), so Land 2's arrival scene no longer waits for
  ever at `GiveScaffold`; whether the two rival creatures are then made on Land 2 (`LOAD_CREATURE` in `LandControl2`)
  is not measured yet.
- **The creature and water**: no creature walks in water yet: footsteps in the water and fish scare (`fn_00483290`),
  going into the water and its limits, the animal's report of being thrown into the sea (`Living::HasSunk` 0x5ED3A9;
  the villager's is published, [Mimicry and the town hooks](#mimicry-and-the-town-hooks)),
  `CheckAllCreaturesForCatching`, drinking from the sea, and being the consumer of the `LandAvoid` mask.
