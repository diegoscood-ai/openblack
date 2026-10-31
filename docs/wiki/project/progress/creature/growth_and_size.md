# Growth and size

A creature starts small and grows over the game, fast while young and then ever more slowly, up to twice its grown
size. Miracles can make it bigger or smaller for a while. Its size changes how far it sees, how fast it moves, how
much it eats and how hard it hits.

**Progress: 29/35 done, 0 chosen, 1 partial, 4 inferred — 84%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/body.md).

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' starting size | done | `CreatureArchetype::StartScale` (`src/ECS/Archetypes/CreatureArchetype.cpp`) from the creature table; test `CreatureArchetypeTest.StartValuesComeFromTheSpeciesRow` |
| It grows fast while young, at full speed until a share of its growing-up time, then falls to its slowest | done | `creature_physiology::Growth`, run by `CreaturePhysiologySystem`; test `YoungCreaturesGrowFastThenSlowly` |
| It grows more with energy to spare | done | `creature_physiology::Growth`; test `CreaturePhysiology.YoungCreaturesGrowFastThenSlowly` |
| It grows three times as fast asleep | done | `creature_physiology::k_SleepGrowthFactor`; tests `CreaturePhysiology.YoungCreaturesGrowFastThenSlowly`, `CreaturePhysiology.ItGrowsThreeTimesAsFastSleepingAtHomeOrBySomething` |
| It grows standing still or moving (the original's moving test is patched out) | done | `creature_physiology::TickTurn`; test `ItGrowsStandingStillOrMovingAndUpToFullSize` |
| It grows by itself up to size 2 and no further | done | `creature_physiology::k_MaxGrownSize`; same test |
| It only grows from the third stage of growing up | done | `creature_physiology::k_GrowingPhase`; test `CreaturePhysiology.ItGrowsStandingStillOrMovingAndUpToFullSize` |
| A creature made bigger than 2 by other means is put back to 2 by its next turn's growth | done | `creature_physiology::Grow`; tests `ItGrowsStandingStillOrMovingAndUpToFullSize`, `AGrowthStepKeepsTheSizeWithinNoneAndFullSize` |
| The game's growth runs on game turns, so a faster game makes it grow faster | done | `CreaturePhysiologySystem` runs once a game turn (`ecs::creature_loop::ProcessTurn`); tests `CreatureLoopTest.TurnCallsTheServicesInOrder`, `CreaturePhysiologySystemTest.TheBodysTurnsFollowTheTimeScale` |

## Size limits and how it is shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Size 1 stands about 15 units tall whatever the species' mesh | done | `creature_morph` `k_HeightAtSizeOne`; test `ACreatureOfSizeOneIsFifteenUnitsTall` |
| The drawn size is kept between a smallest and a largest | done | `creature_morph::ClampScale`; test `SizeIsKeptBetweenItsLimits`; its height and scale read the drawn size kept within 0.05 and 4, as the body's setter keeps it (`object::GetScale` in `src/ECS/ObjectMetrics.cpp`; test `test_object_metrics`) |
| The mesh is scaled by the creature's size and the species' own scale | done | `creature_morph` scale, `CreatureArchetype::DrawnScale`; tests `CreatureMorph.AGivenSizeIsKeptBetweenItsLimitsAndDrawnAtItsScale`, `CreatureMorph.ACreatureOfSizeOneIsFifteenUnitsTall`, `CreatureArchetypeTest.TheCreatureTakesItsBodyAndSize` |
| Near its player's citadel, in its pen, the creature is shrunk down so that it fits | done | `ecs::player_creature::ShrinkInPens` (`src/ECS/PlayerCreature.cpp`, each creature turn): 0.22 within 14 m of its pen home between the pen's walls, back to its size at 16 m, as `ShrinkDownIfNearCitadel` in the exe; drawn only (`components::CreatureDrawPose::scale`); tests `PlayerCreature.InThePenACreatureIsDrawnDownToANewbornsSize`, `OutsideThePenACreatureIsDrawnAtItsOwnSize`, `CreatureDrawnBody.InItsPenTheEyesSitAsDeepOnTheSmallerBodyAtThePenSizesSize`. Our wiki has the numbers ([page](../../../creature/players-creature.md#the-players-creature)) |
| Inside the temple's Creature Cave its copy is drawn at its real size, not the pen's | done | the room's copy takes the creature's own size, held within the limits a body is drawn at (`CreatureCopyArchetype::Create`, made by the temple interior as the room starts; the pen's shrink is only the creature's drawn pose); test `CreatureCopyArchetypeTest.TheCopyIsGivenTheCreaturesSizeWithinItsLimits`; see [creature_cave.md](creature_cave.md). The exe's room (`CreatureRoom::InitEngine`) gives the copy the real size ([page](../../../creature/players-creature.md#the-players-creature)) |
| The creature's size is a value in the Creature Cave's attributes scroll | inferred | the attributes scroll's contents are not read in the exe (only its help script, [temple-interior](../../../interface/temple-interior.md#the-temples-help-scripts)); the cave's snapshot has no size (`creature_cave::Snapshot`, `src/Creature/CreatureCave.cpp`) |

## What size changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bigger creatures walk and run faster | done | `creature_locomotion`; test `SpeedsGrowWithSize` |
| Bigger creatures use energy more slowly and fill up on less | done | see [physiology.md](physiology.md); tests `CreaturePhysiology.EnergyRunsDownSlowerForBigOrSleepingCreatures`, `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Bigger creatures tire less from actions | done | `creature_physiology::ApplyActionCost` in `CreaturePhysiologySystem`; test `CreaturePhysiology.ActionsCostEnergyAndBuildStrength` |
| Bigger creatures hit harder in fights, and make bigger arenas | done | see [fighting.md](fighting.md); tests `CreatureFight.DamageFollowsTheFormula`, `CreatureFight.ArenaIsSizedByTheBiggerCreatureAndCapped` |
| Bigger creatures lie out cold longer when knocked out | done | the knock-out time grows with size (`src/Creature/CreatureFight.cpp`); test `CreatureFight.HealthStaminaAndLife` |
| The leash held in the hand is longer for bigger creatures | done | see [leash.md](leash.md); test `LeashRules.HandLengthsGrowWithTheCreature` |
| Hair grows with size but heads shrink | done | test `ScaleGrowsWithSizeButHeadsShrink` |
| Footprints are bigger for bigger creatures | done | `creature_footprints` print size by the creature's size; see [locomotion.md](locomotion.md); test `CreatureFootprints.Side` |
| Sounds are picked by size: heavier footsteps for bigger creatures | done | `creature_audio` size key; test `SizeKey`; see [animation.md](animation.md) |
| Bigger creatures walk through small trees | inferred | written from expectation: no rule by size for walking through trees is found in the wiki or the exe's route plan (the creature branch of the route plan radius shrinks an obstacle by the creature's nav radius, [body](../../../creature/body.md#the-nav-radius-and-the-route-plan-in-openblack), which is done); no such rule in our tree |
| Bigger creatures can carry heavier things | inferred | no weight limit by size is found in the exe; what is read is that carrying builds strength by the held thing's weight over the creature's mass, which grows with the drawn size (`physics::living::CarriedShare`, test `CreaturePhysiology.CarryingMakesItStronger`; [body](../../../creature/body.md#growth-size-and-age-as-time-passes)) |
| Bigger creatures are more impressive to villagers | done | the creature's impressive value (`Creature::GetImpressiveValue` 0x47B200) starts from its drawn size plus its species' addition, held between 1 and 4, times its species' multiplier ([villager-reactions.md](../../../beings/villager-reactions.md#a-creature-reaction-6)); ours `magic::CreatureImpressiveValue` from `CreatureImpressiveValue` in `VillagerReactions.cpp`, as a villager takes up the creature's reaction; tests `Impressiveness.ACreaturesValueIsItsHeldSizeByItsWearinessDistanceDeedAndBalance`, `MagicFoundation.CreatureImpressiveSizeIsHeldBetweenOneAndFour`; see [town_actions.md](town_actions.md) |

## Miracles and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The big and small creature spells ease the creature to a bigger or smaller size for a while, then back | done | `creature_spells::SizeTarget`, to the creature's largest or smallest size (`ecs::components::CreatureSizeLimits`, else 2.4 and 0.2; with a joint-activity partner, `creature_spells::SizeAims`: big to 1.8 times the partner's drawn size but at least its own, small to 0.5555556 of it but at most its own, each held to that limit, as the original's 0x47DB50 / 0x47DBD0), run by `src/Magic/Spells/SpellCreature.cpp`; see [../miracles](../miracles/); tests `CreatureSpells.TheBodySpellsTargets`, `CreatureSpells.WithAPartnerTheSizeSpellsAimByThePartnersSize`, `CreaturePropertyTest.WithAPartnerItsSmallestAndLargestSizeGoByThePartnersSize`, `CreatureSpells.ASpellEasesInHoldsForItsTimeAndEasesOut` |
| Those spells take it to its own smallest or largest size (0.2 and 2.4 unless a script sets them), leaving a creature already past it as it is | done | `creature_spells::SizeTarget` and `SizeLimits` (`src/Creature/CreatureSpells.cpp`), the creature's own in `ecs::components::CreatureSizeLimits`; `test/test_creature_spells.cpp` `TheBodySpellsTargets` |
| Scripts can scale a creature automatically to the local player's creature's size, or stop doing so | done | `CREATURE_AUTOSCALE` (`src/CHLApi.cpp`) sets `components::CreatureAutoscale`; each turn `ecs::player_creature::Autoscale` steps halfway towards that size times the factor, at most 2 (`creature_size::AutoscaleStep`, `src/Creature/CreatureSize.cpp`), skipped for a creature in the hand's locked select, as the exe's autoscale step ([players-creature](../../../creature/players-creature.md)); tests `PlayerCreature.AnAutoscaleStepGoesHalfwayTowardsTheOtherCreaturesSizeTimesTheFactor`, `AnAutoscaleStepIsCappedAtTwo`, `PlayerCreatureNativesTest.NoAutoscaleWhenOffInALockedSelectOrWithoutTheLocalCreature` |
| Scripts can set a creature's size (SCALE and HEIGHT), and a creature a script loads keeps its file's size | done | `SET SCALE` / `SET HEIGHT` on a creature (`ecs::player_creature::SetCreatureScale`, `SetCreatureHeight`, `src/ECS/PlayerCreature.cpp`), as the exe's `SetSize` / `SetHeight` ([players-creature](../../../creature/players-creature.md)); creatures loaded by `LOAD_MY_CREATURE` keep their file's size (`CreatureMindFileBody`); tests in `test/creature/test_player_creature.cpp` (`SetCreatureScale`, `SetCreatureHeight`), `CreatureMindFileBody.TakesTheBodyAFileKeeps` |
| Size is kept in a saved creature file and in saved games | partial 50% | creature files keep it (test `CreatureMindFileBody.TakesTheBodyAFileKeeps`); saved games are pending |
