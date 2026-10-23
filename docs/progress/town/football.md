# Football

Lionhead's free Soccer Add-On for Black & White (summer 2001) turns on football: towns can build a pitch from eight
scaffolds, and relaxing villagers play five-a-side matches on it while others watch and do the Mexican wave. The code
for all of it ships in the game. The add-on only switches it on.

**Progress: 4/51 done, 0 chosen, 15 partial, 2 inferred — 25%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## The add-on and its switch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Football is off unless the add-on has switched it on; the game checks once when a game starts | partial 50% | football is fixed off, as in the original without the add-on (`GGame::FootballEnabled` 0xD0196C, written only by `GGame::Init` 0x54F54F): `k_FootballEnabled` is false in `src/ECS/Scaffolds.cpp` and `src/ECS/Town/BuildingSites.cpp`; there is no switch to turn it on |
| The switch only works outside multiplayer: in a multiplayer game football stays off even when installed. Skirmish counts as single-player | pending | no switch, so no multiplayer rule either (the feature check fn_525A40 asks `GGame::IsMultiplayerGame`) |

## The pitch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every tribe's pitch is the same: one model (not in the tribe's style), 6000 wood, 8 scaffolds, up to 6 builders, influence 20, impressiveness 0.0015 | done | read from the abode info table like every building (`src/InfoConstants.h`, `src/ECS/Archetypes/AbodeArchetype.cpp`); info.dat is read in every fidelity run. A bigger pitch model and separate centre-circle, corner-flag and goal models exist, but no table uses them |
| Scaffolds join up to 8 only with football on (7 otherwise), so only then can a pitch be offered | partial 50% | the combine maximum takes the flag (`src/ECS/Scaffolds.cpp`, as `Scaffold::ValidToApplyThisToObject` 0x6E9924), fixed off, so 7: test `BuildingSitesTest.AHeldScaffoldMayJoinAScaffoldUnderTheHandOnlyWithinTheCombinedLimit`; the football-on half needs the switch; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A town wants a pitch only with football on, only when it has no pitch and none being built | partial 40% | the pitch's desire to be built returns 0 with the flag off (`src/ECS/Town/BuildingSites.cpp`, as 0x73A4DE; `test/test_building_sites.cpp`); the town's pitch lookup is not ported (`TownFootball` returns null) and the football-on half needs the switch; see [town_desires.md](town_desires.md) |
| The building site's ring is a circle for a pitch | partial 90% | `PosBuilderProcess` makes the circle for a pitch (`src/ECS/Town/BuildingSites.cpp`; the original passes IsFootball, vt +0x464); no test (no check builds a pitch's ring); see [../building/construction.md](../building/construction.md) |
| Finishing a pitch does not set off the "new building" reaction other civic buildings do | partial 90% | `abodes::Built` leaves out the citadel and the pitch where the new building reaction would start (`src/ECS/Abodes.cpp`); no test, and that reaction is not made for any building yet |
| Villagers build it like any building. Its own "going to build the pitch" state does nothing | done | its building site is any building's (`src/ECS/Town/BuildingSites.cpp`, `test/test_building_sites.cpp`); `MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION` has no state functions in the original's table (`VillagerOriginalFns.h` row 70, test `VillagerOriginalFns.EachColumnKeepsTheOriginalsEqualities`; bw1-decomp `Villager::MoveToFootballPitchConstruction` returns 1); see [../building/construction.md](../building/construction.md) |
| Scripts can place a pitch like any other building | done | `CREATE_ABODE` through `src/ECS/Archetypes/AbodeArchetype.cpp` (`test/test_load_scene.cpp`). One multiplayer map's Tibetan town has one |
| The pitch drapes over the land | partial 90% | `MorphWithTerrain` for the pitch (`src/ECS/Archetypes/AbodeArchetype.cpp`), as `Football::Get3DType` gives MORPHABLE; no test (no check places a pitch) |
| Nothing collides with it, and the creature need not walk round it | partial 30% | the original: `Football::InteractsWithPhysicsObjects` and `CreatureMustAvoid` both false. `route_plan_world::CreatureMustAvoid` gives 0 for the type Football, but whether the pitch's info row carries that type is not checked, and `DynamicsSystem::InteractsWithPhysicsObjects` has no pitch exception: it is an obstacle like any standing building; see [../physics/collisions.md](../physics/collisions.md) |
| The pitch takes no damage: anything that would hurt it leaves its life full | pending | `Football::ReduceLife` 0x531E40 returns 1.0; ours takes damage like any abode (`abodes::ReduceLife`, `src/ECS/Abodes.cpp`) |
| Once built, it becomes the town's pitch (one per town) and makes its ball on the centre spot | pending | `Football::MakeFunctional` 0x531380 sets the town's pitch (+0xEA4) and calls `CreateBall` 0x5313E0 when it has no ball, flag or no flag; no pitch object or ball of the pitch in our tree (`src/ECS/Town/TownProcess.cpp`) |
| Whenever the pitch has no ball, it makes a new one on the centre spot | pending | no ball |
| Its question-mark text says a game starts once it is built and nobody is worshipping; the tooltip says "Football Pitch" | pending | see [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |
| With football off, a pitch placed by a script never runs a match, but relaxing villagers can still go to it and wait there | partial 30% | the original: the town runs the pitch's turn only with the flag (`Town::Process` 0x747414), but `MakeFunctional` makes its ball anyway and `Football::SetVillagerActivity` 0x532190 needs only the ball and +0x2FC (1 from `SetToZero`), so villagers become referee, players or watchers and walk there to wait. Ours: no match is run, as with football off, but a villager choosing the pitch does nothing (`ArtefactSystem::OfferRelaxation`) |

## Who plays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager taking up relaxation picks the most wanted of the town's pitch, its god's worship place and the town's artefacts | partial 60% | the pitch is weighed first, then the player's creature, then the artefacts newest first, a later one only when wanted strictly more, as `Town::SetVillagerActivity` 0x73FF10 (`relaxation_rules::Choose`, `ArtefactSystem::OfferRelaxation`; test `RelaxationRules.ThePitchIsWeighedFirstAndAnArtefactMustBeWantedStrictlyMore`); a villager that chooses the pitch does nothing, as the pitch's own `SetVillagerActivity` is not ported; see [../villager/play_and_gossip.md](../villager/play_and_gossip.md) |
| The pitch is wanted at 1 until the first match has ended. After a match it is not wanted for 3 minutes, then at a random 0 to 1 | partial 80% | `relaxation_rules::FootballDesire`, matching `Football::GetVillagerActivityDesire` 0x532220 (tests `RelaxationRules.APitchNeverPlayedOnDrawsEveryAdultFully`, `AfterAGameThePitchDrawsByChanceOnlyOnceItsRestIsOver`); no match is played, so the last-match turn is never set and the pitch is always wanted at 1 by an adult ([villager decisions](../../../beings/villager-decisions.md)) |
| Villagers under 15 never join. Nobody joins once both teams have five and there is a referee | partial 50% | the rule is in `relaxation_rules::FootballDesire` as at 0x532220 (tests `RelaxationRules.APitchNeverPlayedOnDrawsEveryAdultFully`, `NoOneJoinsAGameOnWithFullTeams`); no teams or referee are ever formed: every football state is a TODO row of the villager state table (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`) |
| The first to arrive becomes the referee. Then the home team fills to five, then the away team, and the rest come to watch | pending | `Football::SetVillagerActivity` 0x532190: referee (fn_532730, +0x220), home then away team (fn_532500, at most 5 each), else a watcher (fn_532760); each walks there by `SetupMoveToPos` |
| Players walk to a random spot within 4 m of the centre and wait for kick-off | pending | `FOOTBALL_WALK_TO_POSITION` and `FOOTBALL_WAIT_FOR_KICK_OFF` are TODO rows |
| Players and spectators stay in the town and its desires: every football state counts toward the town's playtime | pending |  |
| A villager taken up by the pitch (referee, player or watcher) takes no reactions | partial 50% | the original sets the football flag 0x80 in `SetVillagerActivity` (0x5321F9) and `IsAvailableForReaction` refuses it (bw1-decomp `VillagerReaction.cpp`); ours has the check (`VillagerCore.cpp`, test `VillagerReactionsTest.IsAvailableForReaction`) but nothing sets the flag |

## The match

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A match waits, plays, pauses after a goal, and can be stopped (turns of a tenth of a second) | pending | `Football::SetPlayState` 0x534860, `StopFootballGame` 0x531FD0, `Process` 0x533E40 |
| With fewer than three a side, waiting players are sent away after 180 s | pending |  |
| With three or more a side, after 30 s the ball is put on the centre spot (unless it is in flight). Kick-off comes once fewer than five players are still walking | pending |  |
| At kick-off and every 15 s of play each side is re-ranked by distance to a goal. The first half attack, the rest defend and the last keeps goal, so five play 2-2-1 and three play 1-1-1 | pending |  |
| If a side has no players, or both have only one, the match goes back to waiting | pending |  |
| A town match stops once it has run 3 minutes and playtime and relaxation are no longer what the town wants most. Players and the first 40 spectators go home | pending | matches started by a script never stop this way |
| A goal counts when the ball crosses a goal mouth taken from the goal model, and only after a kick has set it moving | pending |  |
| After a goal the scorers celebrate and the others mourn for 7 s (70 turns), then play resumes. The pitch does not put the ball back itself; it is restarted as a dead ball | pending |  |
| The score is kept per side, but the tally and the creature's "my side scored" test credit the side the villagers mourn for. Only the debug text shows the tally | pending | reproduce as-is: celebrations follow the villagers' reading |
| When the ball leaves the pitch, one side's first-listed player fetches it for a restart while the rest stand paused. The game compares the two sides' first players and sends the one further from the ball | pending | see ../villager/tools_and_carried_items.md for the carried ball |
| The ball is removed if it goes more than 100 m from the pitch, into water, or more than 80 m away in a god's hand, and a new one appears on the centre spot | pending |  |
| Each kick launches the ball on an arc that lands on its target, with flight time = distance ÷ kick speed, under gravity 9.81 | pending | kick speeds per role: [../story/minigames.md](../story/minigames.md). Ball bounce: [../physics/object_dynamics.md](../physics/object_dynamics.md) |
| When a match ends the town gains belief: 0.01 times up to twice a measure of how long the match ran | pending |  |
| Leaving a match by any other state (picked up, scared, called away) takes a player off the team. A spectator who leaves keeps their wave place until they are gone | pending |  |

## Spectators

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first 40 spectators take places 9° apart all round the pitch, just outside it, scattered by up to 2 m. Later ones stand at random points round it | pending | unsettled: `GetVillagerActivityDesire` 0x532220 gives 0 exactly when `SetVillagerActivity` would make a watcher (both teams full and a referee), so how a watcher is reached is not found |
| They watch the pitch before kick-off and the ball in play, picking one of three watching animations at random, and others after a goal | pending |  |
| Mexican wave: each turn the next of the 40 places, if filled and standing still, starts the wave. After all 40, the wave rests 256 turns (25.6 s), then goes round again | pending | `FOOTBALL_MEXICAN_WAVE` is a TODO row |
| A match impresses those watching (football has its own kind of impressiveness) | inferred | `IMPRESSIVE_TYPE_FOOTBALL` (8) is in bw1-decomp's enum, but where a match uses it was not found; see ../worship/ |

## The player and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can pick up and throw the ball (it shows the ball's in-hand help) and pick up players | partial 40% | a script's football is a mobile object (`BallArchetype`), which the hand grabs and throws as any (`HandPlacement`), and players are villagers; no test of it, and the pitch's own ball and its in-hand help are missing; see ../hand/ |
| A ball in flight is a physics body of its own: the football's info for its weight and sounds, and its own material (row 2 of the physics constants) | done | `DynamicsSystem` (`CanBecomeAPhysicsObject`, its info, `ConstantsType`); test `ParticleCarriedObjectsTest.AFootballIsAPhysicsObjectWithItsOwnInfoAndRow`; see [../../../physics/physics.md](../../../physics/physics.md) |
| The football is a toy ball for the creature: it may kick it about and it never hurts it | partial 30% | the creature's beliefs count it a toy (`IsToy`, `CreatureMindBeliefs.cpp`, as `Ball::IsToy` / `IsToyBall`) and its object actions know it (`Thing::Ball`); kicking it about and never being hurt are not checked; see [../nature/toys.md](../nature/toys.md) |
| The creature can act on a villager who is playing: pick up the ball (first, two times in three, a short gesture when its hand is empty) and throw it at the goal its side attacks. The kick-at-goal, clear and catch actions all do this | pending | see [../creature/town_actions.md](../creature/town_actions.md) |
| It can stomp on the ball, and as "goalkeeper foul" stomp on the first-listed player of the other side | pending |  |
| It celebrates when its side has just scored and commiserates when the other side has. Creatures of even-numbered players are on the home side | pending |  |

## The cup final and the footballers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two special villagers: "Footballer Team 1" in a Norse kit and "Footballer Team 2" in a Celtic kit, belonging to no tribe | partial 70% | the two villager numbers and their meshes are in the tables (`src/Enums.h`, `src/3D/AllMeshes.h`), so a script can make one like any villager; their tribe rule is not checked; see [../villager/special_villagers.md](../villager/special_villagers.md) |
| Both appear in the shipped Land 5 end credits: a Celtic footballer among the ten waving villagers, and a Norse footballer in the special shepherd's place | pending | the credits script is compiled into the game. See [../story/ending.md](../story/ending.md) |
| Script commands to find a town's pitch, stop its games, keep it for scripts only, start a match with a chosen referee, and add or remove players (at most five a side) | pending | GET_FOOTBALL_PITCH, STOP_ALL_GAMES, START_MATCH_WITH_REFEREE, GAME_TEAM_SIZE and the rest are stubs (`src/CHLApi.cpp`); see [../scripts/challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) |

## Sounds, statistics and leftovers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball has its own collision sound, and kicks play the kick sound | inferred | the kick functions (`Ball::KickBallAtDestination` 0x435C40, 0x435DF0) play no sound, and where the kick sound plays was not found; the collision sound is the ball info's (physics row above); see ../villager/tools_and_carried_items.md and ../audio/ |
