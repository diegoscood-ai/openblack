# The Immersion Mushrooms

A first-land silver scroll that exists only for players with an Immersion force-feedback mouse: a hippy beside his
hut asks the player to find the most powerful of his magic mushrooms, the one that shakes most in the hand, and drop
it in his cauldron. The right one gets a healing over his hut and a compassion creature miracle dispenser; the wrong
one blows his hut up.

**Land:** 1 · **Giver:** a hippy at his hut · **Script:** MagicMushroom · **Reward:** a "Compassion" creature miracle dispenser beside the hut, for the right mushroom only · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable (the puzzle's mushrooms). openblack is judged on this tree:
of the 62 script functions the quest and the scripts it starts need, 6 only log "not implemented" in `src/CHLApi.cpp`,
among them the force-feedback mouse check, the puzzle's state and the snapshot; creating the puzzle, the hippy and the
dispenser works (`CreateScriptObject`), and so does the game-time read (`DllGettime`). openblack has no force-feedback
support at all (see [../../pc_integration/online_services.md](../../pc_integration/online_services.md)). The land's
control script starts this quest only in a game that skips the creature training (the start-up box's fourth answer; see
[../../scripts/land1_script.md](../../scripts/land1_script.md)). The puzzle itself is summarised in
[../minigames.md](../minigames.md), the mushrooms as things in the world in
[../../resources/poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md), the land in
[../land_1.md](../land_1.md).

**Progress: 0/37 done, 7 partial — 9%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest after the trainer's slap-and-stroke lesson (with the singing stones' start), or straight after setup in a game that skips the creature training | partial | with the fourth tutorial answer `LandControl1` starts it straight after setup (run logs show it calling `ImmersionExists`); with the first answer the land's control script never gets that far |
| The quest happens only if a force-feedback mouse is plugged in; otherwise the script ends at once, leaving the land as it is | partial | `ImmersionExists` is a stub that always answers no, so the script ends at once, which is right for a player without such a mouse; openblack has no force-feedback support |
| With the mouse, every magic mushroom and every small mushroom already standing within 100 of the puzzle's spot is removed | todo | never reached (row above): the mushrooms stay, as for a player without the mouse |
| The mushroom puzzle is made beside the hippy's hut at 1.2 scale | todo | never reached; CHL `CREATE` makes a puzzle game thing (`src/ECS/PuzzleGames.cpp`), but only the fish puzzle has its parts ported |
| A silver challenge scroll stands at the hippy's hut | todo | script scrolls work (`CreateHighlight`), but the quest never gets this far (no force-feedback mouse) |
| While the camera is within 100 of the scroll and it is on screen, at most every 30 seconds the evil advisor steps out, points at it: "We got ourselves a task down here!" | todo | the notice's natives work, but the quest never gets this far |
| Clicking the scroll or the hut starts the introduction | todo | `GameThingClicked` on a scroll works, but the quest never gets this far |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A hippy villager comes out of the hut; he can't be killed, hurt by fire or picked up | partial | never reached; CHL `CREATE` makes villagers and the three flags work (`SetIndestructable`, `SetHurtByFire`, `SetIdPickupable`) |
| Generic script music 4 plays | partial | `StartMusic` plays the track; the scene never runs |
| In high detail he walks to his spot by the cauldron while the camera glides in (4 s); he faces the camera, a second later turns to the mushrooms and talks, pointing | todo | the scene never runs (`MoveGameThing`, `SetFocus` and `OverrideStateAnimation` work for villagers) |
| The camera turns to the mushrooms: hippy: "Hello. Have you noticed my mushrooms?" | todo | the scene never runs |
| The camera moves; he looks puzzled: "Only they have special properties." | todo | the scene never runs |
| He faces the camera, gossiping: "The more they shake, the more powerful they are." | todo | the scene never runs |
| The camera moves again: "And I'm after the best one to do a little experiment." / "Could you please find it and drop it in my cauldron?" | todo | the scene never runs |
| He walks back into his hut, back to normal detail, as the camera pulls out | todo | the scene never runs |
| The scroll is entered in the challenge list at 0% with the reminder "We need to find the strongest mushroom and put it in the hippy's cauldron."; the music stops | todo | never reached; `Snapshot` is a stub |

## The mushrooms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The puzzle makes 18 mushrooms, scattered round it in a square, each of a slightly different size and turn; the scatter is worked out from each mushroom's number, so the layout is the same every game | todo | no mushroom puzzle in openblack (`src/ECS/PuzzleGames.cpp` ports the fish puzzle only) |
| Held in the hand, each mushroom plays the mushroom-challenge effect on the mouse with its own pair of settings; the right one, the ninth, has the strongest setting (10000) with the smallest other value (13); one other has the same strength but a value of 50, the rest are weaker | todo | no force feedback and no mushroom puzzle in openblack |
| A mushroom that is destroyed is made again at its spot | todo | no mushroom puzzle |
| A mushroom taken from its spot and let go anywhere but the cauldron is put back at its spot | todo | no mushroom puzzle |
| A mushroom dropped into the cauldron (the middle of the puzzle) is used up and decides the puzzle: the ninth wins, any other loses | todo | no mushroom puzzle; `Played` answers only for the fish puzzle and `GetObjectState` is a stub |

## The result

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the scroll is removed; the hippy comes out to the cauldron in high detail as the camera closes in; facing it: "Many thanks, Holy One. I hope you got the right one or my experiment could be fatal." | todo | never reached |
| He walks back into the hut as the camera pulls out to a wide view; two seconds later a suspense sound starts; once he is inside he is removed | todo | never reached (`PlaySoundEffect` works) |
| Right mushroom: the camera shakes round the hut (radius 40, amplitude 0.1, 5 s); three seconds later the suspense sound stops and a level 2 healing miracle comes down onto the hut from 30 above it, lasting 5 s | partial | never reached; `SpellAtPos`, `ShakeCamera` and the sound natives work |
| Right mushroom: the scroll is completed (100%, alignment 0) | todo | never reached; `UpdateSnapshot` is a stub |
| Wrong mushroom: the same shake; three seconds later the hippy is made again at the hut (unkillable), the hut is set on fire (burning speed 0.5) and a level 1 explosion miracle is cast on it from 30 above, 50 across, lasting 30; the suspense sound stops | partial | never reached; `SetOnFire`, the miracle cast and making a villager work |
| Wrong mushroom: a second later the blast flings the hippy through the air so that he lands where the camera is two seconds later (thrown, with a random spin), and the scroll is completed (100%, alignment 0) | todo | never reached; `SetTarget` is a stub |
| Wrong mushroom: after the film the hippy can be killed again and is handed back to the game; with the right one he is never seen again | todo | never reached (`ReleaseFromScript` works) |
| Right mushroom: the good advisor steps out, points at the reward's spot: "Aha. We got the right mushroom. And there's a reward for us." | todo | never reached |
| Wrong mushroom: two seconds later the evil advisor steps out: "Okay, okay. So we got the wrong mushroom. So sue us." | todo | never reached |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A "Compassion" creature miracle dispenser is built beside the hut | partial | never reached; CHL `CREATE` makes spell dispensers and `SetMagicProperties` works (`src/Magic/Script/CHLWorship.cpp`); dispensers: [../rewards.md](../rewards.md) |
| Its recharge time is set to 0 seconds (what a zero time does was not confirmed) | todo | never reached |
| A film with the reward sting: the camera glides to the dispenser; the evil advisor steps out | todo | never reached |
| If it is the first dispenser given by a film in the game: a did-you-know scroll ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") is placed by it, a signpost next to it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."); the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | todo | never reached; the shared dispenser script also reads the help (`GetFirstHelp`, `GetLastHelp` are stubs) |
| Otherwise the evil advisor points at it: "Nice. Another of those cool Miracle Dispensers." | todo | never reached |
| Then the dispenser's own help lines for its miracle are spoken | todo | never reached; which lines the game hands out for the compassion miracle was not traced |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The right-mushroom branch also stops a muffled child's crying sound that is never started | n/a | a leftover in the script; nothing to hear |
| The evil advisor's wrong-mushroom line ends with a "stop pointing" though he never points | n/a | harmless script slip |
| An earlier placing of the scroll's challenge-list entry during the film, and a final completion after the reward, are commented out | n/a | cut |
| The engine has a second mushroom puzzle kind that no land uses | n/a | see [../minigames.md](../minigames.md) |
| The end credits show a hippy beside a giant magic mushroom (among the credited firms is Immersion Corp.), and the fifth land's opening reuses the mushroom suspense sound | n/a | not this quest |
| A saved game keeps the puzzle, the hippy and the scroll | todo | openblack has no saved games |
