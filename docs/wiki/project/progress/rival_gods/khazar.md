# Khazar

The good rival god of the second land: the player's ally, who brought them through the vortex, gives them builders and
scaffolds, comments on their deeds and is killed by Nemesis part-way through the land. He is the land's second player
and is run by the computer player's mind, steered by the land's challenge scripts.

**Progress: 6/45 done, 0 chosen, 9 partial, 0 inferred — 22%**

See [../story/land_2.md](../story/land_2.md) for the land's challenges, and [ai.md](ai.md), [magic.md](magic.md),
[towns_and_influence.md](towns_and_influence.md) and [creatures.md](creatures.md) for how every computer god works.

## On the map

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the second player a computer player | partial 40% | `TOGGLE_COMPUTER_PLAYER` adds the player (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`), marks it a computer player and switches its mind on (`ComputerPlayerSystem::SwitchOn`), whose start then runs once on the land (`computer_player::Start`, tests `ComputerPlayerStart.*`); the mind's own turn weighs and acts each turn (`computer_player::TakeTurn`), but only 6 of its 59 actions are ported |
| His temple stands on his own side of the land, owned by him | done | `CREATE_CITADEL` with its owner (`CitadelArchetype`); test `CitadelPlanTest.CreateCitadelIsBuilt` |
| He has a Norse worship site beside his temple | done | `CREATE_WORSHIP_SITE` (`magic::script::CreateWorshipSite`); tests `LandScriptMeshCommandsTest.AWorshipSiteIsTheTribesSiteOfThePlayersTempleNotTheLinesPlace` and `LandScriptMeshCommandsTest.ATownsBuildingSiteOfTheWorshipSiteIsFinishedAndRemoved` |
| He owns a Norse town with fire, nature, food and wood miracles | partial 40% | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| He owns a Greek town with heal, teleport, physical shield and the itchy creature miracle | partial 40% | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| Each of his towns starts believing 0.65 in him | done | `SET_TOWN_BELIEF` (`ecs::town_belief::SetBeliefInPlayer`), read by the belief rules; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md); test `TownBeliefTest.Scripts` |
| Belief in him and in Lethys is capped at 0.75 in every town of the land | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`); tests `TownBeliefTest.Scripts`, `TownBeliefTest.SetBeliefClampsToCapOnly` |
| His influence rings his temple and towns in his player colour | done | the influence of his temple and towns (`src/ECS/Influence`), drawn in his colour (`influence::k_CircleColours`); see [../worship/influence.md](../worship/influence.md); test `InfluenceTest.circlesHideTheirContact` |
| He is allied to the player at the full 100%, so the player can use his influence (unconfirmed what the alliance shares beyond that) | pending | the alliance native makes it, as the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)) (`ecs::alliances::SetAlliance` (`src/ECS/PlayerAlliances.cpp`)); Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| His mind is told to expand his influence only a little (one fifth of full) | partial 30% | the weight is set (`computer_player::SetPersonality`; the native matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))); the mind's turn reads it (`computer_player::TakeTurn`), but few of the actions it steers are ported; see [ai.md](ai.md) |
| His attitude to Lethys is set to a quarter (kept doubled inside, as a half) | partial 40% | set (`SET_COMPUTER_PLAYER_ATTITUDE`, `computer_player::SetAttitude`); the score rule that reads it (DefeatPlayer37, `computer_player::Score`, the attitude times the desire to attack) is written and runs in the mind's turn, but the actions under it are not ported; see [ai.md](ai.md) |

## His creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tortoise loaded from his own mind file, set down near his temple | done | `LOAD_CREATURE` (`ecs::player_creature::ScriptLoadCreature`), at the script's point; see [creatures.md](creatures.md); test `ScriptLoadCreatureTest.TheScriptsSpeciesIsMadeAtThePointOnTheGroundThenLedAndGrownUp` |
| It is named from the game's text (the script's note calls it Khalen) | done | the naming is the original's (`player_creature::SetName`, kept over the mind file taken up after it); quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127070); its effects' own rows are done: [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Gives a creature a name" |
| It is made fully grown and drawn 1.2 times its auto-scaled size | done | the autoscale (`player_creature::Autoscale`) is real; `LOAD_CREATURE` makes the creature now; quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127072); its effects' own rows are done: [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Sets a creature's stage of development (how much…", [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Lets a creature's size follow the land's scale, or not" |
| Once the player has a creature, it takes on the same alignment and is made friends with it | done | `CREATURE_FORCE_FRIENDS` is real; quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127141); its effects' own rows are done: [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Makes two creatures friends or not", [challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) "Reads a creature's strength or alignment (51 and 47 calls)", [challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) "Sets a creature's strength or alignment (33 and 21 calls)" |
| While Nemesis kills Khazar, the script keeps his creature at full health until its own end | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Arrival on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His hand is put by his temple as the land opens, then flies to greet the player | pending | the natives that put and move his hand are real and match the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)) |
| He introduces himself to his own music, then explains the threat from Lethys | pending | the music exists (`MUSIC_TYPE_SCRIPT_KHAZAR`, `src/Audio/Game/BankTables.h`), but Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real and the scene needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| He places a town centre scaffold (size 5) from his workshop for the player, moving his hand at 40 then 150 | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| Later he gives a storage pit (size 3) and more scaffolds | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play; scaffolds: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| He picks up a new villager by his hand and drops it at the player's town, where it becomes a builder disciple | pending | a forced "pick up and drop" action: `FORCE_COMPUTER_PLAYER_ACTION` is real |
| He leaves food and wood one-shot miracles for the player | partial 50% | dormant: scripts can make the one-shot seeds (CHL `CREATE`, see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)), but Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real and the scene waits on his hand |
| He pours wood on the player's storage pit four times, his wood icon topped up with enough prayer power for six | pending | `GAME_SET_MANA` and `QUEUE_COMPUTER_PLAYER_ACTION` work; a queued action runs only when openblack has its node's action (so far six leaves, `computer_hand::HandActions`; not "ShallCastWoodSpell", the one this queues); not reached in play |

## What he says

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To speak, his hand flies to 10 metres in front of the camera at speed 350, rechecking every 0.3 seconds | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play; the point in front of the camera itself is right (`GetFacingCameraPosition` in `src/CHLApi.cpp`, pinned bit for bit to the original's results by `ScriptCameraDetails.AFacingPositionGoesAlongTheViewDirectionAsTheOriginalRoundsIt`; the scripts pass the distances 10 and 20: [script-camera.md](../../../camera/script-camera.md#opcodes)) |
| If it isn't there after 10 seconds it is put there at once | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| His lines are shown as dialogue text and he is released back to his mind after | pending | the release native is real and matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)) |
| He stays quiet while another script is using him | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Every 20 minutes (the first after 10) he urges the player to build, or once gestures are learnt to take over the land | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the player attacked one of his two towns in the last 3 seconds he complains (one of eight lines), then waits 20 seconds; otherwise he checks again in 3 | pending | `GET_TIME_SINCE_OBJECT_ATTACKED` is real, and Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When his raw influence at the camera beats the player's he comes over, at most once a minute | pending | the influence native works (`magic::script::GetInfluence`; it matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))), but coming over needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| Then half the time he says one of six "this is my land" lines, otherwise building or conquest advice | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the player wins one of Lethys's towns he praises them (one of four lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [towns_and_influence.md](towns_and_influence.md) |
| When the player wins one of his towns he complains (one of eight lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When someone else wins one of his towns he laments it (one of four lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the player wins a neutral town he congratulates them (one of six lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He takes part in the land's shield and fireball challenges | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../story/land_2.md](../story/land_2.md) |

## His death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land checks every 9 seconds whether his death is due | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the whole death scene: [../story/gold_scrolls/nemesis_no.md](../story/gold_scrolls/nemesis_no.md) |
| It is due once the player holds any of Lethys's three towns, or has more than five towns, or Khazar has none left, or Lethys has two or fewer | partial 60% | `GET_PLAYER_TOWN_TOTAL` counts the player's towns; the land's story must still reach it |
| It waits until Khazar isn't busy in another script | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nemesis's music starts and black storm clouds gather over the vortex and his temple | partial 50% | dormant: the music (`MUSIC_TYPE_SCRIPT_NEMESIS`) and the weather objects (CHL `CREATE` of a weather thing, `src/Magic/Script/CHLWeather.cpp`) exist, but Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [nemesis.md](nemesis.md) |
| Volleys of fireballs fall on four places in his land, curling left, straight and right | partial 50% | dormant: `magic::script::SpellAtPos` casts them, as the neutral player, but Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| His temple explodes | pending | `OBJECT_DELETE` of a citadel heart now starts its destruction (`TempleDestructionSystem::DeleteByScript`, from `ObjectDelete` in `src/CHLApi.cpp`); Land 2's story has not been run this far |
| His creature is struck by an explosion from the cloud; Lethys's creature, made strong, comes to it, then both are taken away (Lethys's through the vortex) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the vortex Nemesis opens at his death: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-khazars-death-and-lethyss-vortex) |
| He is switched off and his alliance with the player ends | pending | the alliance native ends it (`ecs::alliances::SetAlliance` (`src/ECS/PlayerAlliances.cpp`)); the switch-off as in [ai.md](ai.md); both natives match the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)) |
| The land's final scroll is offered | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../story/land_2.md](../story/land_2.md) |
