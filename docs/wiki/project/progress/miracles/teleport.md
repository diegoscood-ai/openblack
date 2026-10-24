# Teleport

Each teleport cast leaves a stone with a swirling pool on the ground. A player's stones form a network: villagers and
the creature walking past one jump to the stone that leaves them nearest their goal when it saves enough walking, and a
villager dropped on a stone jumps at once. There is no camera or hand travel through stones.

**Progress: 10/29 done, 0 chosen, 17 partial, 0 inferred — 78%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/teleport.md).

## The stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each cast makes one stone at the cast point, owned by the caster; there is no power-up | done | `src/Magic/Spells/SpellTeleport.cpp` (`InitWithPos`: the stone onto the spell's object list), `src/Magic/Objects/MagicTeleport.cpp` (`Create`, the player's stone list) Test `MagicObjectPlayerTest.ATeleportStoneHasItsSpellsPlayerOrNone`; scenario `miracles.teleport_two_points` |
| A stone can't be cast within 6 m of buildings, fields, features, rocks, dead trees, forests, the temple, miracle dispensers or other stones | partial 80% | `src/Magic/CastRules.cpp` with `teleport::AnyMultiCellStaticNear` (`MagicTeleport.cpp`, `map_cells::IsMultiCellStaticClass`): worship sites, totems, spell icons, fish farms and the temple count; the football pitch and the other citadel parts have no class yet |
| The pool is a land-textured disc in the owner's colour at the middle, fading at the rim and slowly turning | partial 70% | `src/Particles/Rules/SurfRevol.cpp`, `src/Graphics/RendererRevolvedSurface.cpp`; `test/test_teleport.cpp` (SurfRevol suites); drawn without the original's lighting, so dimmer. (the pool's lighting, issue #59) |
| A humming loop plays at each stone while it stands | done | data-driven particle sound of SF_TeleportVortex (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`); test `test/test_teleport_miracle.cpp` `TeleportMiracleSounds.AHummingLoopPlaysAtEachStoneWhileItStands` |
| The hum fades out softly when the stone goes | done | `src/Audio/Services/SpellSounds.cpp` (the soft release flag of the pool's sound); test `test/test_teleport_miracle.cpp` `TeleportMiracleSounds.TheHumFadesOutSoftlyWhenTheStoneGoes` |
| In the hand the seed is a spinning sparkle in the player's colour | partial 80% | The seed's in-hand effect (`src/Magic/Hand/HandMagicFX.cpp`, `src/Particles/Rules/HandFollow.cpp`); scenario `miracles.hand_hold_poses`; not compared with the original |
| The stone lives while its prayer power lasts (one a turn), and costs to create as the game's tables say | done | `src/Magic/Core/Chants.cpp` (one a turn from the initial chants, no player timer), effect tables in `src/Magic/MagicTables.cpp`. Test `MagicTables.realInfoDat` (with the game's data) |
| When the miracle ends the stone, its pool and its reaction go | done | `SpellTeleport.cpp` (`TeleportCloseDown` sets the stone dying), `teleport::ToBeDeleted` (`MagicTeleport.cpp`: reactions, effect, list); test `test/test_teleport_miracle.cpp` `TeleportMiracleStones.AStoneThatGoesTakesItsReactionsItsPoolAndItsTravellers` |
| New buildings can't be placed on a stone | partial 80% | stones are multi-cell statics in their map cells, which the placement test refuses (`src/ECS/Town/TownPlacement.cpp`) No test or check covers it |
| A stone can't be burned, thrown or used by the creature; the hand picks up a pool whose miracle has a seed, giving the seed back | partial 70% | Not thrown and not used by the creature; the hand picks the pool up when its spell has a seed (`HandSystem::ValidForPlaceInHand` and `SeedToPlaceInHand`, `src/ECS/Systems/Implementations/HandSystem.cpp`), as the original ([page](../../../magic/teleport.md#the-hand-and-the-stones)), only the drawn point being stored (approximate); no explicit rule keeps fire or miracle effects off a stone |

## Who travels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers and the creature of the stone's player walking within reach of a stone may react; animals never | partial 60% | villagers: `src/ECS/Systems/Implementations/VillagerTeleport.cpp` (`ReactToTeleportPriority`); the creature's teleport reaction is not ported; animals take none. The stone's reaction is offered when the stone is made and again whenever the turn's walk over the reactions comes to it (`reactions::ProcessTurn`), so villagers who walk into reach later may react too ([miracles](../../../magic/teleport.md#who-uses-a-stone)) Scenarios `miracles.teleport_two_points`, `miracles.teleport_worship` |
| They turn aside only when going via the stones beats the walk by enough (walk more than 1.2 times the stone route) | done | `teleport::ShouldLivingThingReact` (`MagicTeleport.cpp`, the 1.2 rule); test `test/test_teleport.cpp` (WorthTheDetour) |
| Taking up the stone competes with any other reaction they hold, by the reaction table's priority | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp` (priority and switching), `VillagerTeleport.cpp`. Tests in `test/test_villager_reactions.cpp` |
| A villager heading off to decide what to do never turns aside | partial 80% | follows from the reaction contest (`VillagerReactions.cpp`) No test covers this case |
| Slow villagers walk to the stone, fast ones run, by the game's speed threshold | partial 80% | `villager_teleport::SetupReactToTeleport` (state 201 or, strictly faster than the speed threshold, 251) No test covers it; scenario `miracles.teleport_two_points` |
| A villager re-checks each turn whether it has reached the stone | partial 80% | `villager_teleport::GoToTeleportReaction` (`VillagerTeleport.cpp`) No test covers it |
| Walkers stop being travellers once they leave, give up or jump | done | `teleport::ProcessPlayers` (`MagicTeleport.cpp`, the player turn's traveller pruning); test `test/test_teleport_miracle.cpp` `TeleportMiracleStones.WalkersStopBeingTravellersOnceTheyLeaveGiveUpOrJump` |
| Leaving the stone's reaction also takes a villager off its town's list of those on the way to worship | partial 80% | `villager_teleport::ExitReactToTeleport` (`VillagerTeleport.cpp`) clears the on-the-way flags. No test covers it |

## The jump

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The traveller comes out of the player's stone that leaves it nearest its goal; newer stones win ties | done | `teleport::ChooseTarget` (`MagicTeleport.cpp`); `test/test_teleport.cpp` (ChooseTargetTakesTheBiggestSaving) |
| Distances are measured with the game's own approximate distance | done | `teleport::FastDistance`; `test/test_teleport.cpp` (FastDistanceIsMaxPlusHalfMin) |
| Yellow sparkles appear where the traveller leaves and at the stone it arrives at | done | `MagicTeleport.cpp` (spot visual 14 at both ends, through `StartSpotVisual`); scenario `miracles.teleport_two_points` |
| A villager is moved at once, with a "go" sound where it leaves and an "arrive" sound where it lands | done | `MagicTeleport.cpp` (the go and arrive samples through `audio::tags::CreateAtMapCoords`), `villager_teleport::LandAt`. Scenario `miracles.teleport_two_points` |
| Each jump gives back prayer power for the walking saved (less with tribal power) and tops the miracle up from the store | done | `MagicTeleport.cpp` (`PayFor` of minus the saving x costPerKilometer x 0.001, forced); `test/test_teleport.cpp` (JumpCostSign, NegativePayForAddsChants) |
| The creature walks to within 5 m of the stone, fades out over 20 turns, moves, plays the arrive sound and fades back in, then walks on to its goal | pending | The creature's teleport reaction and move by teleport are not ported; scenario `miracles.teleport_creature` describes the original's |
| Each jump is a miracle event with no damage or alignment effect | done | `MagicTeleport.cpp` (a point spell event at the stone, [page](../../../magic/teleport.md)); its effect row has no burn, crush, hit, heal, throw or belief and the plain alignment modifier 1, so it changes no life and moves no alignment (an effect moves it by the life it changed: `MiracleCoreRows.AnEffectMovesTheAlignmentByTheKindOfThingAndTheLifeItChanged`); integration test `test/test_teleport_miracle.cpp` `TeleportMiracleRows.realInfoDatAJumpsEventHasNoEffect` |

## Hand and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager dropped on a stone of the dropping player, who owns more than one stone, decides where it is going and jumps at once | partial 80% | `src/ECS/Systems/Implementations/HandApplyToObject.cpp` (a held villager applied to a stone), `villager_teleport::LandAt`. No test or check covers the drop |
| Worshippers going to a far worship site are routed through the stones | done | `src/ECS/Systems/Implementations/VillagerWorship.cpp` (`CanIGetToTheWorshipSite`, `teleport::FindRouteStone`); `test/test_teleport.cpp` (RouteStone, RouteStoneBothEndsAreLookedForApart) Scenarios `miracles.teleport_worship`, `miracles.teleport_straight_two_points` |
| A creature casting teleport gets a shorter-lived stone than the player's | partial 30% | The creature timer is read from the tables (`src/Magic/MagicTables.cpp`), but the creature's plan has no teleport cast in our tree |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stones and their travellers are kept in a saved game | pending | openblack has no game saving |
