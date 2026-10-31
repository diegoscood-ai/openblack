# Swap To Brown Bear

A fifth-land silver scroll that appears once the player wins the neutral Japanese village: a villager complains of a
stench from the forest, and the player follows a trail of eight piles of dung, picking each one up, to find the brown
bear that made them. The bear stays as a creature the player can swap their own creature for, and a heal miracle chest
falls beside it.

**Land:** 5 · **Giver:** a Japanese farmer from a house on the edge of the neutral Japanese village · **Script:** SwapToBrownBear · **Reward:** a brown bear creature to swap to, and a heal miracle chest given to the player's home village · **Repeatable:** no (the swap offer itself never ends)

Sources: the quest's script (the original source text, checked line by line against the decompile of the PC game's
compiled `challenge.chl`, where it is compiled), Land 5's control script and map script, the game's text table and its
reward table (`info.dat`). openblack is judged on this tree: the quest's challenge-record and reward commands
are real in `src/CHLApi.cpp` and its creature commands are not checked here, `Create` makes the villager, the dung
and the creature (`CreateScriptObject`), and `SwapCreature` is real. The story reaches the land only through the earlier lands' exits (`LOAD_MAP` changes land now, not yet checked), so Land 5's
control script, which starts this quest, has not yet run in play. Every row is pending unless the notes say otherwise. The land
is in [../land_5.md](../land_5.md); the swap that ends the quest is described in full in
[creature_swaps.md](./creature_swaps.md).

**Progress: 0/51 done, 0 chosen, 4 partial, 0 inferred — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started by the land's control script the first time the neutral Japanese village becomes the player's, however it was won | pending | `LandControl5` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); reading a town's owner (`GET_PROPERTY` player) is real now (`GetProperty` in `src/CHLApi.cpp`) |
| It is started only once: losing the village to Nemesis and winning it back does not start it again | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The check sits in the land's main loop that runs while the creature is cursed, until the three wonder villages (Greek, Tibetan and Aztec) are all the player's; if the player takes all three before the Japanese village, the loop ends and this quest is never offered | pending | read from the control script: no later code starts it; the curse: [../gold_scrolls/i_have_a_surprise_for_you.md](../gold_scrolls/i_have_a_surprise_for_you.md); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The control script's town checks run one after another with the cut scenes for taking the wonder villages, so the scroll can appear a little after the village is won if one of those scenes is playing | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| As soon as the quest starts, a silver scroll appears over a house on the village's edge (about 60 from its centre) and the challenge is registered | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`), `Snapshot` is real; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Clicking the scroll or the house makes the scroll active and the quest goes on; nothing happens before that click (no time limit) | pending | `GameThingClicked` and `SetActive` on a highlight work (see [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md)); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first pile of dung appears at once, about 100 west of the house, with a swarm of flies over it that lasts for ever | pending | `Create` makes the dung (a lump of poo, `CreateScriptObject`), which goes into the physics and settles on the land as the original's does (`script_create::MobileObjectStartsPhysics`), and `SpecialEffectObject` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A Japanese farmer comes out of the house (the source notes he was a woman until the voice used turned out to be a man's) | pending | `Create` makes villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cut scene with the "happy" script music starts; the farmer is drawn in high detail | partial 40% | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Draws an object in full detail however far away it is", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The camera glides to a low view by the house (3 seconds for the position, 4 for the focus) | pending | `MoveCameraPosition`, `MoveCameraFocus` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After 2 seconds the farmer walks slowly (speed 0.4) to a spot a few paces in front of the house, then turns to face the camera | pending | `MoveGameThing`, `SetFocus` and the speed property work for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He plays his "poisoned" (retching) animation once while the camera creeps closer over 12 seconds | pending | the villagers' script animations (`SetScriptState`, `SetScriptUlong`, `Played`) work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A quarter of a second after it ends, the challenge record opens: title "Swap To Brown Bear", progress 0, reminder "The people here are complaining about the stench." (good advisor) | pending | `Snapshot` is real |
| He looks around as if searching and says "Ugh. There's a frightful stench coming from the forest." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He turns to the first pile of dung, talks and points, as the camera swings up and round to look west over the forest (3 seconds): "Could you check it out, big one. It's sickening." | pending | `RunText`, `SetFocus` and the camera moves work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera goes back to where the player had it, but now looking at the first pile of dung (3 seconds) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The farmer walks back to his house, the music stops and the scene ends; once he is within 2 of the house he is removed | pending | `MoveGameThing`, `StopMusic`, `ObjectDelete` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Straight after, the good advisor steps out: "Sickening, is it? I don't believe you." then "Hmm. Yes it really is rancid. I wonder what's causing it?" (the evil advisor is popped out for the second line, but both lines are recorded in the good advisor's voice) | pending | `SpiritEject` and `RunText` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Following the trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player clears the dung by picking each pile up with the hand; the moment a pile is in the hand it vanishes (the hand is left empty) and the next pile appears with its own flies | pending | the in-hand property (`GetProperty` IN_HAND) is real; `ObjectDelete` works; not seen in game |
| Only one pile exists at a time; they lead from near the village west, then north through the forest, each 25 to 45 apart: eight piles in all, the last about 220 north-west of the house | pending | positions from the script; needs the in-hand test |
| After the first pile, the good advisor: "Ugh. I can still smell something. There must be several more around. Let's clear them up." | pending | needs the in-hand test |
| After the seventh pile, the evil advisor: "The smell is almost gone. There's one more out there, though." | pending | needs the in-hand test |
| Picking up the eighth pile ends the trail | pending | needs the in-hand test |
| The flies of a picked-up pile are never removed by the script (undetermined whether they go with the pile) | pending | needs the in-hand test |
| There is no time limit, no counter on screen and no way to fail; the creature can't do it for the player (only the hand holding a pile counts) | pending | read from the script; whether a creature picking a pile up counts as "in hand" is undetermined |
| If the farmer's house is damaged at any time during the trail (its health drops below what it was after the introduction), the good advisor steps out, points at it and says "The owner of that house will be a little distraught." (once only) | pending | the health read (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`) is real now; not checked in play |
| A further hint was meant for the first pile: once it was on screen with the camera within 100, the good advisor would point at it and say "We really should clean up this mess. Someone could step in it." It never plays (see bugs) | pending | script behaviour to keep; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Finding the bear

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell-success sparkle appears for 2 seconds at a spot about 30 past the last pile, almost at sea level, and a brown bear creature appears there | pending | `Create` makes creatures now (`player_creature::MakeForScript`); not checked in play; `SpecialEffectPosition` works |
| A cut scene with the short epic sting starts; the camera moves to a low view of the spot over 2 seconds | partial 40% | `StartMusic` plays the sting; the scene never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The good advisor: "Aha! So that's what's been causing this smell!" | pending | needs the trail (in-hand test) and the bear |
| The bear walks about 25 further north, the camera following it down over 2 seconds; when it is there the camera drops lower | pending | needs the bear |
| The heal miracle chest falls from the sky onto the spot the bear appeared on (see below) | pending | `CreateRewardInTown` is real |
| The evil advisor: "The brown bear. So they do do it in the woods!" while the bear looks back at the spot, for at least 3 seconds | pending | needs the bear |
| The bear turns to the camera, both advisors go home, the dialogue closes and a second later the challenge record closes as a success (progress 1, alignment 0) | pending | `Snapshot` is real |
| The music stops and the scene ends; the bear is then handed to the shared creature-swap offer | pending | `CREATE` makes the bear now and `SWAP_CREATURE` is real; not checked in play |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest drops from the sky at the bear's spot, given in the player's home Norse village (the first village on the land), without its own camera film | pending | `CreateRewardInTown` is real; see [../rewards.md](../rewards.md) |
| The chest is the reward table's first heal kind: it holds a heal miracle seed (undetermined whether it is the powered-up heal its name suggests; the table's power-up field is empty for both heal kinds) | pending | read from the reward table in `info.dat` with our `GRewardInfo` layout (`src/InfoConstants.h`); the chests are real now, not yet checked in this quest |
| Once the chest is opened, the good advisor says "Great! We can heal people with this." if no dialogue is in the way; if this were the game's first reward, the first-reward explanation would play instead | partial 60% | the chest opens (`RewardSystem::Tap`, ported from raffclar's reward-chest work, unverified against v1.42) and `GetHelp` gives its help text (`src/CHLApi.cpp`); not checked in play against the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Gives a help text by number", [rewards.md](../rewards.md) "The chest is a closed wooden chest model", "Every later chest", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The real reward is the bear itself: a new species for the player's creature, if they want it | pending | see the next section |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears over the bear and it keeps turning to look at the camera; when the camera is near, the evil advisor says "Swap your Creature with this one if you want." at most once a minute | pending | no bear; the shared swap script; full detail in [creature_swaps.md](./creature_swaps.md) |
| Clicking it with the player's creature further than 50 away: "You'll need to bring our Creature, Boss." | pending | no bear |
| Otherwise the two creatures meet, the good advisor asks to confirm ("Are you sure you want a new Creature? ...") and clicking the bear confirms, clicking the player's own creature cancels | pending | `CREATE` makes the bear now and `SWAP_CREATURE` is real; not checked in play |
| On confirmation the creature's mind moves into the brown bear in a short scene, and the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | pending | `SwapCreature` is real: it moves the mind into the other body (`CreatureMindSystem::SwapMinds`, see [creature_swaps.md](./creature_swaps.md)); not checked in play |
| The old body stays under a scroll and the offer never ends, so the player can swap back and forth | pending | `SWAP_CREATURE` is real; not checked in play |

## Aftermath, advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The farmer is gone for good (removed once home); the house stays | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll over the house is never removed by the script (undetermined whether the game removes it when the quest's script ends) | pending | undetermined whether the game removes it when the quest's script ends |
| The reminder line, replayed by clicking the scroll, is spoken by whichever advisor owns it (the good advisor) | pending | the shared reminder script's natives work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Music: the "happy" script track for the introduction, the short epic sting for the bear | partial 40% | music itself works (`StartMusic`); counted in the scene rows above; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The player's creature is not involved until the swap; nothing checks the creature during the trail | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Bugs, quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is the only one on the land tied to winning the Japanese village, and can be missed for good by taking the wonder villages first | pending | see "How it appears" |
| A saved game keeps where the trail is, the bear and the offer | pending | openblack's saved games (loaded only from the debug bar so far) put the scripts' machine back (`Game::LoadSavedGame`), but of the world's objects only the land's buildings, piles, forests and trees and a few others yet, not the quest's people, animals, creatures or scrolls ([saved games](../../../../engine/saved-games.md#pending)) |
