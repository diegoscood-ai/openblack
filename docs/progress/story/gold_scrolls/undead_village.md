# Undead Village

The fourth land's last story scroll before the Creed: once all three of Nemesis's Guardian Stones are broken, a man of
the Japanese village tells how one village refused Nemesis, had its two totems driven into the ground and was turned
into walking skeletons. Raising both totems back to full height breaks the curse; the freed villagers tell the player
that the Creed lies in the body of the Guide, and the creature fetches it from the mountain top.

**Land:** 4 · **Giver:** a man of the Japanese village, under a gold scroll at the village's edge · **Script:** Land4Meteorites (its undead-village part) · **Reward:** the second element of the Creed (the creature picks it up) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table (`InfoScript2.txt`) for every spoken line and its speaker, and the executable.
openblack is judged on this tree: the land never runs there (openblack starts the story's top script, which always runs
Land 1's control script first, and the map-loading command changes land only after the earlier lands' exits, which no check has reached), and 6 of the 68 commands the quest uses
(counted before the snapshots were made real; among the rest: skeletons, creature orders and the Creed) only logged "not implemented" in `src/CHLApi.cpp`; the skeletons (`SetSkeleton`) and the creature orders are real now, and making the Creed object (`CREATE`) is not done. Every
row is pending unless the notes say otherwise. The land as a whole is in [../land_4.md](../land_4.md); its script program
in [../../scripts/land4_script.md](../../scripts/land4_script.md).

**Progress: 0/49 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The undead village is set up as soon as the land starts: the town by the two totems is made skeletons, two African totems (scale 2.233, turned 352.1° and 36°) are made beside it fully sunk into the ground (height 0), both totems and the whole town are made indestructible | pending | `SetSkeleton` is real; `CREATE` of a totem makes a puzzle totem, sunk at height 0 (`PuzzleTotemArchetype`, [the puzzle totem](../../../../magic/puzzle-totem.md)), and `SetIndestructable` and its `HEIGHT` are real |
| The quest itself only starts after the land's control script sees all three Guardian Stones broken: the lightning stone (beating Sleg, [the_defending_ogres.md](the_defending_ogres.md)), the darkness stone ([the_heartbroken_man.md](the_heartbroken_man.md)) and the fire stone ([the_totem_puzzle.md](the_totem_puzzle.md)), in any order | pending | `LandControl4` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The control script waits for the whole quest (it is run in the foreground), then runs the Creed scene and only then the exit vortex quest ([leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md)) | pending | plain script order; see [leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Until the Creed is in the creature's hand, the undead town's health is set back to full every five minutes, "so they don't hobble about" | pending | health (property 1) is set by `SetProperty`, a town's on each of its members; not checked in play |
| Until the town is saved, any belief of the player's in it that reaches 0.1 is wiped to 0 (checked every second): the town cannot be impressed while cursed | pending | `BeliefForPlayer` and `SetPlayerBelief` are real (towns); not checked in play |
| After the story scene has been seen, the second time the belief is wiped the good advisor steps out: "Leader, we aren't getting anywhere trying to impress the people here." / "We've just GOT to break the curse Nemesis has put on this place.", then the evil advisor "But we'll need influence to get near those Totems." | pending | the belief wipes it counts use `BeliefForPlayer` and `SetPlayerBelief`, which are real (towns); not checked in play |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears at the edge of the Japanese village, on the man's spot | pending | `CreateHighlight` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| While the camera is within 100 of the scroll and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | pending | the notify script calls real natives (`SpiritEject`, `SpiritPointGameThing`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest waits until the scroll (or its spot) is clicked; nothing else moves on | pending | `GameThingClicked` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| As the scene starts, both totems are pushed up to their full height so the story can show them | pending | `HEIGHT` = `MAX_HEIGHT` of the puzzle totems is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The story scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays through the whole scene | pending | `StartMusic` is real (`src/Audio/GameMusic`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves in 5 seconds to look at the river bank; a Japanese farmer appears from behind a hut, walks to a mark, turns 90° left, walks to his spot and faces the camera, gossiping | pending | `CREATE` makes villagers; `MoveGameThing` and the animations are real for villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The man: "Your success is great, Mighty One." / "You have almost restored our land to blessed normality." / (the camera creeps closer over 20 seconds) "But one more area remains tainted by Nemesis." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fade to black; the town is shown as it was, alive: the skeletons are switched off, the time of day set to 16:50 and stopped, and the camera glides past the houses for 30 seconds | pending | the fade and clock natives are real, and so is `SetSkeleton`; not checked in play |
| The man: "One Village refused to bow to Nemesis." / "They saw his might and were afraid." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fade; a happy scene in the village: two children (ages 10 and 9) jumping and facing each other, a man walking slowly by, two women gossiping, and a man the camera follows walking to the middle of the village; all are drawn in high detail | pending | `CREATE` makes villagers and `SetHighGraphicsDetail` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The totems shake: the gate-stone grinding sound and a camera shake (radius 150, strength 0.25, 5 seconds) around each totem, then the camera looks down from high behind them | pending | `ShakeCamera` and `PlaySoundEffect` are real, but the totems are never made |
| The scroll is entered in the challenge log here, titled "Undead Village", at 0%, with the reminder "Break Nemesis' curse to get the Creed." (said by the good advisor when the log entry is clicked) | pending | `Snapshot` is real |
| From here the totems start to sink back (see "Raising the totems") | pending | the totems' `HEIGHT` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The man: "So Nemesis buried their Totems, their source of power." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The curse: "And cursed their Village." — when the followed man reaches his spot a puff of smoke (scale 6) bursts and he is yanked into the ground; one child points and talks, then one after another, a quarter to half a second apart, each villager vanishes in smoke (scale 3) and is yanked underground; each comes back a skeleton, drowns, mourns and lies poisoned | pending | `SpecialEffectPosition` and `SetSkeleton` are real; not checked in play |
| The whole town turns to skeletons; after the last villager six seconds pass, then a fade and the six actors are removed | pending | `SetSkeleton` is real |
| Back with the man (the time of day is put back and the clock restarted): "If you help them. I'm sure they'll be forever in your debt." / (the camera creeps closer over 20 seconds) "They know where the Creed you seek can be found." | pending | the clock and text natives are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fade back to where the player's camera was before the scene; the music stops; the man is removed | pending | fade, camera, `StopMusic` and `ObjectDelete` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Raising the totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player raises each totem with the hand (the town's totem pole is gripped and pulled up), which needs the player's influence there, as the advisors hint | pending | the hand takes hold of a puzzle totem and drags it in whole steps, in the influence ([the puzzle totem](../../../../magic/puzzle-totem.md#the-hand-and-the-tap)); not yet reached in play: `LOAD_MAP` changes the land, but the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../../creature/town_actions.md](../../creature/town_actions.md) |
| A totem that is partly raised and not being held by the hand sinks back, a tiny step (0.00005 of its height) every pass of the watcher, and puffs dust (scale 7) while it sinks | pending | the 0.00005 taken off the height is one whole step by the truncation (`puzzle_totem::SetHeightProperty`, tested); `IS_LOCKED_INTERACTION` of a held puzzle totem is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Quirk: the whole watch (sinking and the creature's help) only runs while the player has a creature; with none, the totems never sink | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the creature is within 50 of a totem that has once been raised above 90 and has since fallen right back to the ground, the creature is ordered to raise that totem itself; when the totem reaches full height the creature is released | pending | `CREATURE_DO_ACTION` 56 (RaiseTotemPole) raises a puzzle totem by 0.02 of its highest a turn (`mind_town_help::TotemShareOf` / `Apply`, tested by `PuzzleTotemPlaceTest.TheCreatureRaisesItByItsOwnShare`); not yet reached in play: `LOAD_MAP` changes the land, but the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../../creature/decision_making.md](../../creature/decision_making.md) |
| The curse breaks the moment both totems stand at exactly full height together | pending | `HEIGHT` and `MAX_HEIGHT` of the puzzle totems are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Nothing fails this quest and nothing times it out: the land waits for it for ever | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The curse lifted

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A skeleton villager appears in the village, and a scene starts with the second epic theme: the camera moves in over 6 seconds and the skeleton walks to the village's middle | pending | `StartMusic` is real, but the skeleton villager needs `SetSkeleton` (real) |
| He is yanked into the ground in a large puff of smoke (scale 16); the whole town stops being skeletons, and so does he; he drowns, mourns and then stands | pending | `SetSkeleton` is real |
| The freed man: "We owe you everything." / "We can tell you where the Creed you need can be found." / "The Creed lies in the body of the one you knew as the Guide." | pending | `RunText` is real; see [../creature_guide.md](../creature_guide.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The Creed is made on a mountain top (at height 138.7, the height of the mountain top where the Guide died on the first land, though at a different spot of the map), with an anti-influence of radius 30 around it so the hand cannot take it | pending | `CREATE` of the Creed object is not done |
| Fade; the camera is set to the start of the land's seventh camera path and flies along it to show where the Creed lies | pending | `RunCameraPath` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll goes to 100% in the challenge log, the gold scroll is removed, and the village can be harmed again | pending | `Snapshot` is real |
| The music stops; the creature is released; the town counts as saved (the belief wiping stops, so it can now be impressed) | pending | `StopMusic` and `ReleaseFromScript` are real; the saved flag is plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the freed man tells where it is, the Creed piece is made on a hill east of the home village, with a 30-radius ring of anti-influence round it so the player's hand cannot take it | pending | `CREATE` of the Creed object is not done; `InfluencePosition` is real |
| Only the creature can collect it: the scene waits until the Creed is in the creature's hand | pending | `InCreatureHand` is real since 2026-10-21 and the Creed object is not made |
| The moment it is, the creature counts as having the Creed; the screen fades to white over a second and, 2 seconds later, the Creed object is removed | pending | reached only once the creature holds the Creed: `InCreatureHand` is real but the Creed object is not made; `SetFade` and `ObjectDelete` are real |
| The creature is put on the hilltop facing east, with a faint glow above its hand (scale 5, power 0.1) | pending | `SetPosition`, turning a creature (`SetFocus`) and `SetCreatureCreedProperties` are real; not checked in play |
| A cut scene with the epic script music (number 4): the camera looks up at the creature's head from below and fades in over a second | pending | `StartMusic`, camera cuts and the fade are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera climbs slowly (20 seconds) high above the hill; after 4 seconds the glow above the hand grows to full power | pending | `MoveCameraPosition` and `SetCreatureCreedProperties` are real; not checked in play |
| Both advisors step out. Good: "You found the second Creed!" Evil: "Yeah. Oh this is a great day!" | pending | `SpiritEject` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A second later a screen rumble sounds and the camera shakes hard (amplitude 1, radius 100, 5 seconds) | pending | `PlaySoundEffect` and `ShakeCamera` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 6 seconds later the good advisor: "What was that?" Both go home, the music stops, the creature is let go | pending | advisors, `StopMusic` and `ReleaseFromScript` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| While the creature has not got the Creed, the Undead Village's people are kept at full health | pending | health (property 1) is set by `SetProperty`, a town's on each of its members; not checked in play |
| The rumble leads straight into the exit vortex quest | pending | plain script order; [leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| What the Creed does to the creature in play (beyond the glow) | pending | not determined; see [../land_4.md](../land_4.md) |

