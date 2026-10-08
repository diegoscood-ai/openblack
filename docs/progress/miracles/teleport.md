# Teleport

Each teleport cast leaves a stone with a swirling pool on the ground. A player's stones form a network: villagers and
the creature walking past one jump to the stone that leaves them nearest their goal when it saves enough walking, and a
villager dropped on a stone jumps at once. There is no camera or hand travel through stones.

**Progress: 22/29 done, 5 partial — 84%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## The stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each cast makes one stone at the cast point, owned by the caster; there is no power-up | done | `src/Magic/Spells/SpellTeleport.cpp` (`InitWithPos`: the stone onto the spell's object list), `src/Magic/Objects/MagicTeleport.cpp` (`Create`, the player's stone list) |
| A stone can't be cast within 6 m of buildings, fields, features, rocks, dead trees, forests, the temple, miracle dispensers or other stones | partial | `src/Magic/CastRules.cpp` with `teleport::AnyMultiCellStaticNear` (`MagicTeleport.cpp`, `map_cells::IsMultiCellStaticClass`): worship sites, totems, spell icons, fish farms and the temple count; the football pitch and the other citadel parts have no class yet |
| The pool is a land-textured disc in the owner's colour at the middle, fading at the rim and slowly turning | partial | `src/Particles/Rules/SurfRevol.cpp`, `src/Graphics/RendererRevolvedSurface.cpp`; `test/test_teleport.cpp` (SurfRevol suites); drawn without the original's lighting, so dimmer |
| A humming loop plays at each stone while it stands | done | data-driven particle sound of SF_TeleportVortex (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`) |
| The hum fades out softly when the stone goes | done | `src/Audio/Services/SpellSounds.cpp` (the soft release flag of the pool's sound) |
| In the hand the seed is a spinning sparkle in the player's colour | done | the seed's in-hand effect (`src/Magic/Hand/HandMagicFX.cpp`, `src/Particles/Rules/HandFollow.cpp`); not checked in game |
| The stone lives while its prayer power lasts (one a turn), and costs to create as the game's tables say | done | `src/Magic/Core/Chants.cpp` (one a turn from the initial chants, no player timer), effect tables in `src/Magic/MagicTables.cpp` |
| When the miracle ends the stone, its pool and its reaction go | done | `SpellTeleport.cpp` (`TeleportCloseDown` sets the stone dying), `teleport::ToBeDeleted` (`MagicTeleport.cpp`: reactions, effect, list) |
| New buildings can't be placed on a stone | done | stones are multi-cell statics in their map cells, which the placement test refuses (`src/ECS/Town/TownPlacement.cpp`) |
| A stone can't be burned, picked up, thrown or used by the creature | partial | not thrown and not used by the creature; the hand picks the pool up when its spell has a seed (`HandSystem::ValidForPlaceInHand` and `SeedToPlaceInHand`, `src/ECS/Systems/Implementations/HandSystem.cpp`), the seed going back to the hand; no explicit rule keeps fire or miracle effects off a stone. Our wiki differs: the original lets the hand pick up a pool whose spell has a seed ([miracles](../../bw1-notes/miracles.md#the-hand-and-the-stones-faithful)) |

## Who travels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers and the creature of the stone's player walking within reach of a stone may react; animals never | partial | villagers: `src/ECS/Systems/Implementations/VillagerTeleport.cpp` (`ReactToTeleportPriority`); the creature's teleport reaction is not ported; animals take none. Our wiki differs: the reaction is spread only once, when the stone is made, so only those already moving then react ([miracles](../../bw1-notes/miracles.md#who-uses-a-stone)) |
| They turn aside only when going via the stones beats the walk by enough (walk more than 1.2 times the stone route) | done | `teleport::ShouldLivingThingReact` (`MagicTeleport.cpp`, the 1.2 rule); `test/test_teleport.cpp` (WorthTheDetour) |
| Taking up the stone competes with any other reaction they hold, by the reaction table's priority | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp` (priority and switching), `VillagerTeleport.cpp` |
| A villager heading off to decide what to do never turns aside | done | follows from the reaction contest (`VillagerReactions.cpp`) |
| Slow villagers walk to the stone, fast ones run, by the game's speed threshold | done | `villager_teleport::SetupReactToTeleport` (state 201 or, strictly faster than the speed threshold, 251) |
| A villager re-checks each turn whether it has reached the stone | done | `villager_teleport::GoToTeleportReaction` (`VillagerTeleport.cpp`) |
| Walkers stop being travellers once they leave, give up or jump | done | `teleport::ProcessPlayers` (`MagicTeleport.cpp`, the player turn's traveller pruning) |
| Leaving the stone's reaction also takes a villager off its town's list of those on the way to worship | done | `villager_teleport::ExitReactToTeleport` (`VillagerTeleport.cpp`) clears the on-the-way flags |

## The jump

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The traveller comes out of the player's stone that leaves it nearest its goal; newer stones win ties | done | `teleport::ChooseTarget` (`MagicTeleport.cpp`); `test/test_teleport.cpp` (ChooseTargetTakesTheBiggestSaving) |
| Distances are measured with the game's own approximate distance | done | `teleport::FastDistance`; `test/test_teleport.cpp` (FastDistanceIsMaxPlusHalfMin) |
| Yellow sparkles appear where the traveller leaves and at the stone it arrives at | done | `MagicTeleport.cpp` (spot visual 14 at both ends, `psys::manager::CreateSpotVisual`) |
| A villager is moved at once, with a "go" sound where it leaves and an "arrive" sound where it lands | done | `MagicTeleport.cpp` (the go and arrive samples through `audio::tags::CreateAtMapCoords`), `villager_teleport::LandAt` |
| Each jump gives back prayer power for the walking saved (less with tribal power) and tops the miracle up from the store | done | `MagicTeleport.cpp` (`PayFor` of minus the saving x costPerKilometer x 0.001, forced); `test/test_teleport.cpp` (JumpCostSign, NegativePayForAddsChants) |
| The creature walks to within 5 m of the stone, fades out over 20 turns, moves, plays the arrive sound and fades back in, then walks on to its goal | todo | the creature's teleport reaction and move by teleport are not ported |
| Each jump is a miracle event with no damage or alignment effect | done | `MagicTeleport.cpp` (a point spell event at the stone) |

## Hand and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager dropped on a stone of the dropping player, who owns more than one stone, decides where it is going and jumps at once | done | `src/ECS/Systems/Implementations/HandApplyToObject.cpp` (a held villager applied to a stone), `villager_teleport::LandAt` |
| Worshippers going to a far worship site are routed through the stones | done | `src/ECS/Systems/Implementations/VillagerWorship.cpp` (`CanIGetToTheWorshipSite`, `teleport::FindRouteStone`); `test/test_teleport.cpp` (RouteStone, RouteStoneBothEndsAreLookedForApart) |
| A creature casting teleport gets a shorter-lived stone than the player's | partial | the creature timer is read from the tables (`src/Magic/MagicTables.cpp`), but creatures do not cast player miracles in our tree |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stones and their travellers are kept in a saved game | todo | openblack has no game saving |
