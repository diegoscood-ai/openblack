# The Immersion Mushrooms

A first-land silver scroll that exists only for players with an Immersion force-feedback mouse: a hippy beside his
hut asks the player to find the most powerful of his magic mushrooms, the one that shakes most in the hand, and drop
it in his cauldron. The right one gets a healing over his hut and a compassion creature miracle dispenser; the wrong
one blows his hut up.

**Land:** 1 · **Giver:** a hippy at his hut · **Script:** MagicMushroom · **Reward:** a "Compassion" creature miracle dispenser beside the hut, for the right mushroom only · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable (the puzzle's mushrooms). openblack is judged on this tree:
of the 62 script functions the quest and the scripts it starts need, 6 only logged "not implemented" in `src/CHLApi.cpp`
when they were counted on an earlier tree, among them the force-feedback mouse check, the puzzle's state and the
snapshot; at this tree the mouse check answers no (`IMMERSION_EXISTS`, `ImmersionExists` in `src/CHLApi.cpp`), `GET_OBJECT_STATE` and
the snapshot commands are real, and only the mushroom puzzle itself is missing; creating the puzzle, the hippy and the
dispenser works (`CreateScriptObject`), and so does the game-time read (`DllGettime`). openblack has no force-feedback
support at all (see [../../pc_integration/online_services.md](../../pc_integration/online_services.md)). The land's
control script starts this quest only in a game that skips the creature training (the start-up box's fourth answer; see
[../../scripts/land1_script.md](../../scripts/land1_script.md)). The puzzle itself is summarised in
[../minigames.md](../minigames.md), the mushrooms as things in the world in
[../../resources/poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md), the land in
[../land_1.md](../land_1.md).

Rows reachable only with a force-feedback device have the status `device`, counted apart and in no score (user, 2026-10-27; [progress/README.md](../../README.md#status)).

**Progress: 0/2 done, 0 chosen, 2 partial, 0 inferred, 35 device-only (not scored) — 50%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest after the trainer's slap-and-stroke lesson (with the singing stones' start), or straight after setup in a game that skips the creature training | partial 40% | with the fourth tutorial answer `LandControl1` starts it straight after setup (run logs show it calling `ImmersionExists`); with the first answer the land's control script never gets that far; still open: the puzzle is the force-feedback mouse's only (device, haptics not ported), so no system row finishes it, and no quest run's PASS is cited |
| Without a force-feedback mouse the script ends at once after making its markers, leaving the land as it is: no puzzle is made and nothing waits on one | partial 60% | no quest_check PASS of a run yet (the run logs only show the call), so the native is the measured share; `ImmersionExists` answers no, as the original does on a machine without the mouse (checked in the exe, 2026-10-19: [the force-feedback mouse](../../../../scripts/script-commands.md#the-interface)); still open: the puzzle is the force-feedback mouse's only (device, haptics not ported), so no system row finishes it, and no quest run's PASS is cited |
| The quest happens only if a force-feedback mouse is plugged in | device | openblack has no force-feedback support, so it always takes the branch without the mouse; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| With the mouse, every magic mushroom and every small mushroom already standing within 100 of the puzzle's spot is removed | device | never reached (row above): the mushrooms stay, as for a player without the mouse; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| The mushroom puzzle is made beside the hippy's hut at 1.2 scale | device | never reached; CHL `CREATE` makes a puzzle game thing (`src/ECS/PuzzleGames.cpp`), but only the fish puzzle has its parts ported; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| A silver challenge scroll stands at the hippy's hut | device | script scrolls work (`CreateHighlight`), but the quest never gets this far (no force-feedback mouse); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Clicking the scroll or the hut starts the introduction | device | `GameThingClicked` on a scroll works, but the quest never gets this far; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A hippy villager comes out of the hut; he can't be killed, hurt by fire or picked up | device | never reached; CHL `CREATE` makes villagers and the three flags work (`SetIndestructable`, `SetHurtByFire`, `SetIdPickupable`); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Generic script music 4 plays | device | `StartMusic` plays the track; the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| In high detail he walks to his spot by the cauldron while the camera glides in (4 s); he faces the camera, a second later turns to the mushrooms and talks, pointing | device | the scene never runs (`MoveGameThing`, `SetFocus` and `OverrideStateAnimation` work for villagers); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| The camera turns to the mushrooms: hippy: "Hello. Have you noticed my mushrooms?" | device | the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| The camera moves; he looks puzzled: "Only they have special properties." | device | the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| He faces the camera, gossiping: "The more they shake, the more powerful they are." | device | the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| The camera moves again: "And I'm after the best one to do a little experiment." / "Could you please find it and drop it in my cauldron?" | device | the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| He walks back into his hut, back to normal detail, as the camera pulls out | device | the scene never runs; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| The scroll is entered in the challenge list at 0% with the reminder "We need to find the strongest mushroom and put it in the hippy's cauldron."; the music stops | device | never reached; `Snapshot` is real; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |

## The mushrooms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The puzzle makes 18 mushrooms, scattered round it in a square, each of a slightly different size and turn; the scatter is worked out from each mushroom's number, so the layout is the same every game | device | no mushroom puzzle in openblack (`src/ECS/PuzzleGames.cpp` ports the fish puzzle only); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Held in the hand, each mushroom plays the mushroom-challenge effect on the mouse with its own pair of settings; the right one, the ninth, has the strongest setting (10000) with the smallest other value (13); one other has the same strength but a value of 50, the rest are weaker | device | no force feedback and no mushroom puzzle in openblack; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| A mushroom that is destroyed is made again at its spot | device | no mushroom puzzle; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| A mushroom taken from its spot and let go anywhere but the cauldron is put back at its spot | device | no mushroom puzzle; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| A mushroom dropped into the cauldron (the middle of the puzzle) is used up and decides the puzzle: the ninth wins, any other loses | device | no mushroom puzzle; `Played` answers only for the fish puzzle; `GetObjectState` is real but has no mushroom result to give; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |

## The result

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the scroll is removed; the hippy comes out to the cauldron in high detail as the camera closes in; facing it: "Many thanks, Holy One. I hope you got the right one or my experiment could be fatal." | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| He walks back into the hut as the camera pulls out to a wide view; two seconds later a suspense sound starts; once he is inside he is removed | device | never reached (`PlaySoundEffect` works); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Right mushroom: the camera shakes round the hut (radius 40, amplitude 0.1, 5 s); three seconds later the suspense sound stops and a level 2 healing miracle comes down onto the hut from 30 above it, lasting 5 s | device | never reached; `SpellAtPos`, `ShakeCamera` and the sound natives work; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Right mushroom: the scroll is completed (100%, alignment 0) | device | never reached; `UpdateSnapshot` is real; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Wrong mushroom: the same shake; three seconds later the hippy is made again at the hut (unkillable), the hut is set on fire (burning speed 0.5) and a level 1 explosion miracle is cast on it from 30 above, 50 across, lasting 30; the suspense sound stops | device | never reached; `SetOnFire`, the miracle cast and making a villager work; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Wrong mushroom: a second later the blast flings the hippy through the air so that he lands where the camera is two seconds later (thrown, with a random spin), and the scroll is completed (100%, alignment 0) | device | never reached; `SetTarget` is real (not seen in play yet); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Wrong mushroom: after the film the hippy can be killed again and is handed back to the game; with the right one he is never seen again | device | never reached (`ReleaseFromScript` works); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Right mushroom: the good advisor steps out, points at the reward's spot: "Aha. We got the right mushroom. And there's a reward for us." | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Wrong mushroom: two seconds later the evil advisor steps out: "Okay, okay. So we got the wrong mushroom. So sue us." | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A "Compassion" creature miracle dispenser is built beside the hut | device | never reached; CHL `CREATE` makes spell dispensers and `SetMagicProperties` works (`src/Magic/Script/CHLWorship.cpp`); dispensers: [../rewards.md](../rewards.md); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Its recharge time is set to 0 seconds (what a zero time does was not confirmed) | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| A film with the reward sting: the camera glides to the dispenser; the evil advisor steps out | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| If it is the first dispenser given by a film in the game: a did-you-know scroll ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") is placed by it, a signpost next to it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."); the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | device | never reached; the shared dispenser script also reads the help (`GetFirstHelp` and `GetLastHelp` are real for a spell dispenser); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Otherwise the evil advisor points at it: "Nice. Another of those cool Miracle Dispensers." | device | never reached; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
| Then the dispenser's own help lines for its miracle are spoken | device | never reached; which lines the game hands out for the compassion miracle was not traced; only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the puzzle, the hippy and the scroll | device | openblack's saved games (loaded only from the debug bar so far) put the scripts' machine back (`Game::LoadSavedGame`), but of the world's objects only the land's buildings, piles, forests and trees and a few others yet, not the quest's people, animals, creatures or scrolls ([saved games](../../../../engine/saved-games.md#pending)); only with a force-feedback device: the script's `if immersion exists` (refs/chl/MagicMushroom.txt:17) answers no without one, in the original as in openblack (`IMMERSION_EXISTS`) |
