# Khazar

The good rival god of the second land: the player's ally, who brought them through the vortex, gives them builders and
scaffolds, comments on their deeds and is killed by Nemesis part-way through the land. He is the land's second player
and is run by the computer player's mind, steered by the land's challenge scripts.

**Progress: 5/46 done, 6 partial — 17%**

See [../story/land_2.md](../story/land_2.md) for the land's challenges, and [ai.md](ai.md), [magic.md](magic.md),
[towns_and_influence.md](towns_and_influence.md) and [creatures.md](creatures.md) for how every computer god works.

## On the map

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the second player a computer player | partial | `TOGGLE_COMPUTER_PLAYER` adds the player (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`), with no mind behind it |
| His temple stands on his own side of the land, owned by him | done | `CREATE_CITADEL` with its owner (`CitadelArchetype`) |
| He has a Norse worship site beside his temple | done | `CREATE_WORSHIP_SITE` (`magic::script::CreateWorshipSite`) |
| He owns a Norse town with fire, nature, food and wood miracles | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| He owns a Greek town with heal, teleport, physical shield and the itchy creature miracle | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| Each of his towns starts believing 0.65 in him | done | `SET_TOWN_BELIEF` (`ecs::town_belief::SetBeliefInPlayer`), read by the belief rules; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| Belief in him and in Lethys is capped at 0.75 in every town of the land | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`) |
| His influence rings his temple and towns in his player colour | done | the influence of his temple and towns (`src/ECS/Influence`), drawn in his colour (`influence::k_CircleColours`); see [../worship/influence.md](../worship/influence.md) |
| He is allied to the player at the full 100%, so the player can use his influence (unconfirmed what the alliance shares beyond that) | todo | `SET_PLAYER_ALLY` is a stub; the story never reaches Land 2 (`LOAD_MAP` is empty) |
| His mind is told to expand his influence only a little (one fifth of full) | todo | `SET_COMPUTER_PLAYER_PERSONALITY` is a stub; see [ai.md](ai.md) |
| His attitude to Lethys is set to a quarter (kept doubled inside, as a half) | todo | `SET_COMPUTER_PLAYER_ATTITUDE` is a stub |

## His creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tortoise loaded from his own mind file, set down near his temple | todo | `LOAD_CREATURE` is a stub; see [creatures.md](creatures.md) |
| It is named from the game's text (the script's note calls it Khalen) | todo | `SET_CREATURE_NAME` is a stub |
| It is made fully grown and drawn 1.2 times its auto-scaled size | todo | `CREATURE_AUTOSCALE` is a stub and the creature is never loaded |
| It knows every everyday skill and almost every miracle, all body values set to 0.2 | todo | see [creatures.md](creatures.md) |
| Once the player has a creature, it takes on the same alignment and is made friends with it | todo | `CREATURE_FORCE_FRIENDS` is a stub |
| While Nemesis kills Khazar, the script keeps his creature at full health until its own end | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |

## Arrival on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His hand is put by his temple as the land opens, then flies to greet the player | todo | `SET_COMPUTER_PLAYER_POSITION` and `MOVE_COMPUTER_PLAYER_POSITION` are stubs |
| He introduces himself to his own music, then explains the threat from Lethys | todo | the music exists (`MUSIC_TYPE_SCRIPT_KHAZAR`, `src/Audio/Game/BankTables.h`), but the story never reaches Land 2 (`LOAD_MAP` is empty) and the scene needs his hand: the computer player natives are stubs in `src/CHLApi.cpp` |
| He places a town centre scaffold (size 5) from his workshop for the player, moving his hand at 40 then 150 | todo | needs his hand: the computer player natives are stubs in `src/CHLApi.cpp` |
| Later he gives a storage pit (size 3) and more scaffolds | todo | needs his hand: the computer player natives are stubs in `src/CHLApi.cpp`; scaffolds: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| He picks up a new villager by his hand and drops it at the player's town, where it becomes a builder disciple | todo | a forced "pick up and drop" action: `FORCE_COMPUTER_PLAYER_ACTION` is a stub |
| He leaves food and wood one-shot miracles for the player | partial | dormant: scripts can make the one-shot seeds (CHL `CREATE`, see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)), but the story never reaches Land 2 (`LOAD_MAP` is empty) and the scene waits on his hand |
| He pours wood on the player's storage pit four times, his wood icon topped up with enough prayer power for six | todo | `GAME_SET_MANA` works, but the queued actions are stubs (`QUEUE_COMPUTER_PLAYER_ACTION`) |

## What he says

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To speak, his hand flies to 10 metres in front of the camera at speed 350, rechecking every 0.3 seconds | todo | needs his hand: the computer player natives are stubs in `src/CHLApi.cpp`; `GET_FACING_CAMERA_POSITION` itself works |
| If it isn't there after 10 seconds it is put there at once | todo | needs his hand: the computer player natives are stubs in `src/CHLApi.cpp` |
| His lines are shown as dialogue text and he is released back to his mind after | todo | `RELEASE_COMPUTER_PLAYER` is a stub |
| He stays quiet while another script is using him | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| Every 20 minutes (the first after 10) he urges the player to build, or once gestures are learnt to take over the land | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| If the player attacked one of his two towns in the last 3 seconds he complains (one of eight lines), then waits 20 seconds; otherwise he checks again in 3 | todo | `GET_TIME_SINCE_OBJECT_ATTACKED` is a stub in our tree, and the story never reaches Land 2 (`LOAD_MAP` is empty) |
| When his raw influence at the camera beats the player's he comes over, at most once a minute | todo | `GET_INFLUENCE` works (`magic::script::GetInfluence`), but coming over needs his hand: the computer player natives are stubs in `src/CHLApi.cpp` |
| Then half the time he says one of six "this is my land" lines, otherwise building or conquest advice | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| When the player wins one of Lethys's towns he praises them (one of four lines) | todo | the story never reaches Land 2 (`LOAD_MAP` is empty); see [towns_and_influence.md](towns_and_influence.md) |
| When the player wins one of his towns he complains (one of eight lines) | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| When someone else wins one of his towns he laments it (one of four lines) | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| When the player wins a neutral town he congratulates them (one of six lines) | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| He takes part in the land's shield and fireball challenges | todo | the story never reaches Land 2 (`LOAD_MAP` is empty); see [../story/land_2.md](../story/land_2.md) |

## His death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land checks every 9 seconds whether his death is due | todo | the story never reaches Land 2 (`LOAD_MAP` is empty); the whole death scene: [../story/gold_scrolls/nemesis_no.md](../story/gold_scrolls/nemesis_no.md) |
| It is due once the player holds any of Lethys's three towns, or has more than five towns, or Khazar has none left, or Lethys has two or fewer | todo | `GET_PLAYER_TOWN_TOTAL` is a stub |
| It waits until Khazar isn't busy in another script | todo | the story never reaches Land 2 (`LOAD_MAP` is empty) |
| Nemesis's music starts and black storm clouds gather over the vortex and his temple | partial | dormant: the music (`MUSIC_TYPE_SCRIPT_NEMESIS`) and the weather objects (CHL `CREATE` of a weather thing, `src/Magic/Script/CHLWeather.cpp`) exist, but the story never reaches Land 2 (`LOAD_MAP` is empty); see [nemesis.md](nemesis.md) |
| Volleys of fireballs fall on four places in his land, curling left, straight and right | partial | dormant: `SPELL_AT_POS` casts them, as the neutral player, but the story never reaches Land 2 (`LOAD_MAP` is empty) |
| His temple explodes | todo | the temple's destruction is not in our tree (`ObjectDelete` of a citadel heart logs "not implemented") |
| His creature is struck by an explosion from the cloud; Lethys's creature, made strong, comes to it, then both are taken away (Lethys's through the vortex) | todo | the story never reaches Land 2 (`LOAD_MAP` is empty); the vortex Nemesis opens at his death: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-khazars-death-and-lethyss-vortex) |
| He is switched off and his alliance with the player ends | todo | `ENABLE_DISABLE_COMPUTER_PLAYER` and `SET_PLAYER_ALLY` are stubs |
| The land's final scroll is offered | todo | the story never reaches Land 2 (`LOAD_MAP` is empty); see [../story/land_2.md](../story/land_2.md) |
