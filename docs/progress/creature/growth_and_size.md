# Growth and size

A creature starts small and grows over the game, fast while young and then ever more slowly, up to twice its grown
size. Miracles can make it bigger or smaller for a while. Its size changes how far it sees, how fast it moves, how
much it eats and how hard it hits.

**Progress: 27/36 done, 2 partial — 78%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' starting size | done | `CreatureArchetype::StartScale` (`src/ECS/Archetypes/CreatureArchetype.cpp`) from the creature table |
| It grows fast while young, at full speed until a share of its growing-up time, then falls to its slowest | done | `creature_physiology::Growth`, run by `CreaturePhysiologySystem`; test `YoungCreaturesGrowFastThenSlowly` |
| It grows more with energy to spare | done | `creature_physiology::Growth` |
| It grows three times as fast asleep | done | `creature_physiology::k_SleepGrowthFactor` |
| It grows standing still or moving (the original's moving test is patched out) | done | `creature_physiology::TickTurn`; test `ItGrowsStandingStillOrMovingAndUpToFullSize` |
| It grows by itself up to size 2 and no further | done | `creature_physiology::k_MaxGrownSize`; same test |
| It only grows from the third stage of growing up | done | `creature_physiology::k_GrowingPhase` |
| A creature made bigger than 2 by other means is put back to 2 by its next turn's growth | done | `creature_physiology::Grow`; tests `ItGrowsStandingStillOrMovingAndUpToFullSize`, `AGrowthStepKeepsTheSizeWithinNoneAndFullSize` |
| The game's growth runs on game turns, so a faster game makes it grow faster | done | `CreaturePhysiologySystem` runs once a game turn (`ecs::creature_loop::ProcessTurn`) |

## Size limits and how it is shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Size 1 stands about 15 units tall whatever the species' mesh | done | `creature_morph` `k_HeightAtSizeOne`; test `ACreatureOfSizeOneIsFifteenUnitsTall` |
| The drawn size is kept between a smallest and a largest | done | `creature_morph::ClampScale`; test `SizeIsKeptBetweenItsLimits` |
| The mesh is scaled by the creature's size and the species' own scale | done | `creature_morph` scale, `CreatureArchetype::DrawnScale` |
| Near its player's citadel the creature is shrunk down so that it fits (unconfirmed by how much) | done | `ecs::player_creature::ShrinkInPens` (`src/ECS/PlayerCreature.cpp`, each creature turn): 0.22 within 14 m of its pen home between the pen's walls, back to its size at 16 m; drawn only (`components::CreatureDrawPose::scale`); test `CreatureDrawnBody.InItsPenTheEyesShrinkWithTheBody`. Our wiki has the numbers ([page](../../bw1-notes/creature.md#the-players-creature)) |
| Inside the temple's creature room it is shown at the room's size | todo | the Creature Cave draws no creature yet; see [creature_cave.md](creature_cave.md). Our wiki differs: the Cave's copy of the creature takes its real size, not one of the room's ([page](../../bw1-notes/creature.md#the-players-creature)) |
| The creature's size is a value in the Creature Cave's attributes scroll | todo | the cave's snapshot and the attributes scroll have no size (`creature_cave::Snapshot`, `src/Creature/CreatureCave.cpp`) |

## What size changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bigger creatures walk and run faster | done | `creature_locomotion`; test `SpeedsGrowWithSize` |
| Bigger creatures play their animations more slowly | done | test `BiggerCreaturesPlayMoreSlowly` |
| Bigger creatures see further | done | `creature_look::LookRange` |
| Bigger creatures use energy more slowly and fill up on less | done | see [physiology.md](physiology.md) |
| Bigger creatures tire less from actions | done | `creature_physiology::ApplyActionCost` in `CreaturePhysiologySystem` |
| Bigger creatures hit harder in fights, and make bigger arenas | done | see [fighting.md](fighting.md) |
| Bigger creatures lie out cold longer when knocked out | done | the knock-out time grows with size (`src/Creature/CreatureFight.cpp`) |
| The leash held in the hand is longer for bigger creatures | done | see [leash.md](leash.md) |
| Small creatures have bigger eyes for their size | done | test `SmallCreaturesHaveBiggerEyesForTheirSize` |
| Hair grows with size but heads shrink | done | test `ScaleGrowsWithSizeButHeadsShrink` |
| Footprints are bigger for bigger creatures | done | `creature_footprints` print size by the creature's size; see [locomotion.md](locomotion.md) |
| Sounds are picked by size: heavier footsteps for bigger creatures | done | `creature_audio` size key; test `SizeKey`; see [animation.md](animation.md) |
| Bigger creatures walk through small trees | todo | no rule lets a creature walk through small trees in our tree |
| Bigger creatures can carry heavier things (unconfirmed) | todo | |
| Bigger creatures are more impressive to villagers | todo | see [town_actions.md](town_actions.md) |

## Miracles and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The big and small creature spells ease the creature to a bigger or smaller size for a while, then back | done | `creature_spells::SizeTarget`, to the creature's largest or smallest size (`ecs::components::CreatureSizeLimits`, else 2.4 and 0.2), run by `src/Magic/Spells/SpellCreature.cpp`; see [../miracles](../miracles/) |
| Those spells take it to its own smallest or largest size (0.2 and 2.4 unless a script sets them), leaving a creature already past it as it is | done | `creature_spells::SizeTarget` and `SizeLimits` (`src/Creature/CreatureSpells.cpp`), the creature's own in `ecs::components::CreatureSizeLimits`; `test/creature/test_creature_spells.cpp` `TheBodySpellsTargets` |
| Scripts can scale a creature automatically to a size, or stop doing so | todo | `CREATURE_AUTOSCALE` is a stub |
| Scripts can read a creature's grown-up size | todo | `ID_ADULT_SIZE` is a stub |
| Scripts can set the size when a creature is made | partial | creatures loaded by `LOAD_MY_CREATURE` keep their file's size (`src/ECS/PlayerCreature.cpp`, `CreatureMindFileBody`); script creation of creatures is in [saves_and_files.md](saves_and_files.md) |
| Size is kept in a saved creature file and in saved games | partial | creature files keep it (test `CreatureMindFileBody.TakesTheBodyAFileKeeps`); saved games are todo |
