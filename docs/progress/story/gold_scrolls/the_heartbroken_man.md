# The Heartbroken Man

The gold scroll on the fourth land that frees the third of Nemesis's Guardian Stones, the one that keeps the land in
darkness. A woman asks the player to bring Keiko, kidnapped by Nemesis to the Aztec village, back to her husband Adam,
the lonely old man who keeps the stone on a mountain top. Reuniting them, killing him or killing her all free the stone;
the three endings differ in what is said and in the alignment the challenge log records.

**Land:** 4 · **Giver:** a woman at a hut below Adam's mountain (the scroll is beside her) · **Script:** Land4Nomad (in
Land4Meteorites) · **Reward:** the darkness Guardian Stone is destroyed and the sky turns back to day; influence around the
hut (killing Adam) or Adam's hut (reuniting them) · **Repeatable:** no

The land as a whole is in [../land_4.md](../land_4.md); the other two stones are [The Totem Puzzle](the_totem_puzzle.md)
and [The Defending Ogres](the_defending_ogres.md); what happens once all three are broken is in
[Undead Village](undead_village.md) and [leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`), the game's
text table (`Scripts/InfoScript2.txt`) for every spoken line and its voice. openblack's state is judged on this tree:
openblack starts the story's top script, which always runs the first land's control script first; the map-loading
command (`LoadMap` in `src/CHLApi.cpp`) changes land now, but only through the earlier lands' exits, which no check has reached and the first land's script stalls long before its end, so the
fourth land's control script, and this quest, never run. Rows are partial only where every command they need works in
openblack; they still never happen in play.

**Progress: 0/76 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background as soon as the land begins, together with the other two stone quests | pending | `LandControl4` never runs (see [../../scripts/land4_script.md](../../scripts/land4_script.md)); the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| From the start of the land a small Guardian Stone (a meteor at a third of its size, raised 2 off the ground) sits near Adam's hut on the mountain, smoking: a bonfire effect 25 times normal size that lasts the whole land | pending | `CREATE` makes the meteor and its flags are real (`SetIndestructable`, `SetIdMoveable`, `SetIdPickupable`); the height (YPos) is handled, the scale through `SetProperty` is not for a mobile static; `SpecialEffectPosition` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest itself waits until the elder of the Japanese village has finished his introduction to [The Totem Puzzle](the_totem_puzzle.md), which itself waits until the player owns, destroys or empties the Japanese village | pending | plain script flag; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The man who explains Nemesis's curse earlier describes this stone: "The third Stone was given to a lonely old man." "Who worships only Nemesis." | pending | his scene is covered with [The Defending Ogres](the_defending_ogres.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Breaking this stone is one of the three conditions the land waits for before the Undead Village and the way out open | pending | plain script flags; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears by the woman's hut, on the lowland west of Adam's mountain, and its challenge is made the current one | pending | `CreateHighlight` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| While the scroll is not clicked, whenever the camera is within 100 of it and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | pending | the notify script calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Clicking the scroll (or its spot) starts the introduction and turns the scroll active | pending | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll is never removed by the script: it stays where it is after the quest is over | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A woman (an Aztec housewife) is made at her hut and Adam (a nomad) at his house on the mountain, facing a spot by the door and mourning on an endless loop | pending | `CREATE` makes villagers; `SetFocus` and the animations on villagers are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cut scene begins with the generic script music (number 3) | pending | `StartMusic` and `StartCameraControl` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The woman (drawn in high detail) walks out of her hut while the camera glides over 6 seconds to look at her from beside the scroll | pending | `MoveGameThing`, `MoveCameraPosition`, `MoveCameraFocus` and `SetHighGraphicsDetail` are real for villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| When she has arrived and the camera has stopped, she turns to the camera and gossips (three loops of her talking gesture) | pending | villager animations are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The woman: "At last! Someone to hear my prayers!" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black over 2 seconds, the camera cuts to Adam's door on the mountain and fades back in over 2 | pending | `SetFade`, `SetFadeIn`, `SetCameraPosition`, `SetCameraFocus` and `EndDialogue` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records the quest: title "The Heartbroken Man", progress 0, alignment 0, with the reminder: the good advisor saying "Find Keiko in the Aztec Village and reunite her with Adam." | pending | `Snapshot` is real |
| The woman (heard over Adam mourning): "My neighbour, Adam is a soul in torment." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera drifts round the mountain top for 10 seconds while she goes on: "Keiko, his wife has been kidnapped and taken to the Aztec Village by Nemesis." | pending | real camera and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| "Before he left he made sure she stays alive only while Adam's faith in him remains." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera turns to look down at the woman for 4 seconds, then flies back to her in 6 to 7 seconds; she gossips again: "Please help them by rescuing Keiko and reuniting her with Adam." | pending | real camera, villager and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera closes on her over 5 seconds as she walks back into her hut | pending | real camera and villager natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor steps out: "Hey, Boss. Wasn't he the lonely guy with the Guardian Stone?" then "Let's kill him!" | pending | `SpiritEject` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The music stops and the cut scene ends | pending | `StopMusic` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Keiko (an Aztec housewife) is made at the edge of the Aztec village and the woman who told the story is removed | pending | `CREATE` and `ObjectDelete` of villagers are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Keiko in the Aztec village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko walks at a tenth of normal speed round three spots by the village: at the first she sits down, sits for four loops and stands; at the second she inspects something; at the third she mourns (into, three loops, out of); then back to the first | pending | `MoveGameThing`, the speed and the animations are real for villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| She walks between the spots with despairing walks | pending | `OverrideStateAnimation` on villagers is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Picked up by the hand, thrown, or held by the creature, she is watched: once she is put down again more than 100 from her second spot, she is put straight back there, but only when nobody is holding her, she is not flying and the camera is not looking at her | pending | `SetPosition`, the held read (property 9, `GET_PROPERTY` InHand) and `InCreatureHand` (since 2026-10-21) are real; not checked in play |
| So the player can carry her anywhere, but she must be kept in view, or she is snapped back to her village | pending | follows from the held reads, which are real; not checked in play |
| The script sets no condition on reaching her; whether the hand can pick her up depends on the game's usual influence rule, which the script does not touch | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bring Keiko to within 50 of Adam: the first time she gets that close, Adam walks out of his house to a spot by his door and waves for attention on an endless loop, and the player's influence is made in a circle of 50 around him | pending | `MoveGameThing` and `InfluenceObject` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Once Adam is out, the reunion starts when Keiko is within 50 of him and neither of them is held by the hand or the creature or flying through the air | pending | the held read (property 9, `GET_PROPERTY` InHand) and `InCreatureHand` (since 2026-10-21) are real; not checked in play |
| The quest has no timer and no distance limit other than these: it waits as long as the player likes | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Killing Keiko (or anything that removes her) ends the quest the evil way | pending | the health read (property 1, `GetProperty` HEALTH) is real; not checked in play |
| Killing Adam (or anything that removes him) ends the quest the other way | pending | the health read (property 1, `GetProperty` HEALTH) is real; not checked in play |
| Whichever comes first wins: the script checks Keiko's death, then Adam's death, then the reunion, in that order, every time round | pending | the death checks read health, which is real; not checked in play |

## Reunited (the good ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk and its watcher stop; neither of them can be picked up any more | pending | `StopScript` and `SetIdPickupable` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black over a second; the epic script music (number 4) starts; the two are set face to face by Adam's door, drawn in high detail, with the camera close on them; it fades in after 2 seconds | pending | fade, `SetPosition`, the camera, music, `SetFocus` and `SetHighGraphicsDetail` are real for villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After a second they both dance | pending | villager animations are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records it complete: progress 1, alignment +0.8 | pending | `Snapshot` is real; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| When the dance ends they hug, then both turn to the camera | pending | villager animations and `SetFocus` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Adam, gossiping: "Oh how can I ever, ever thank you!"; half a second later Keiko joins in with her own gestures | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Keiko: "We're together again! I can't believe it." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Adam: "You are a magnificent god! I have a reason to live!" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| They walk slowly (Adam at a fifth of normal speed, Keiko at 0.15) to the door and into the house while the camera pulls back over 6 seconds; the screen fades to black and the music stops | pending | villager moves and the camera are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Keiko killed (the evil ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black over a second | pending | `StopScript` and the fade are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cut scene: Adam is set by his door facing the camera, in high detail, mourning; it fades in after 2 seconds | pending | real villager, detail and fade natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records it complete: progress 1, alignment -0.8 | pending | `Snapshot` is real |
| Adam: "You've killed my soulmate!" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| "Nemesis promised to protect her. He has failed!" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| "Take the Stone. I've got no faith left in anything." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black | pending | `SetFade` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Adam killed (the other ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black | pending | `StopScript` and the fade are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cut scene: the neighbour woman is made again at her spot by her hut, facing the camera, mourning for five loops; it fades in after 2 seconds | pending | `CREATE` makes villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records it complete: progress 1, alignment 0 (neither good nor evil) | pending | `Snapshot` is real |
| The woman: "My neighbour lies dead." then "You are as cruel as Nemesis. Are all gods like this?" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor steps out: "Boss, that guy's out of the way." then "And the Stone isn't under Nemesis' influence any longer." and goes home | pending | `SpiritEject`, `SpiritHome` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black; the player is given influence in a circle of 200 around the woman, which is never taken away; the woman is let go to live as a normal villager | pending | `InfluenceObject` and `ReleaseFromScript` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The Guardian Stone breaks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever the ending, Adam and Keiko are both removed from the world, even after a happy reunion | pending | `ObjectDelete` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cut scene: the camera is set looking at the stone by the hut and fades in after 2 seconds | pending | camera cut and fade are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After a reunion the good advisor points at the stone: "Look! The newfound faith of this man has freed the Guardian Stone." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After either killing the evil advisor points at it: "Ha! Killing. It's the answer to everything. Eh, beardy?" then "Never underestimate death. And fear. And agony." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A second later the Guardian Stones music starts; the camera shakes within 300 of the stone (amplitude 0.1, 5 seconds) and follows the stone as it rises, a tenth at a time, to 20 off the ground | pending | `ShakeCamera`, `FocusFollow`, `StartMusic` and the height (YPos) are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A bang and a flash (6 seconds each) go off at the stone, it is destroyed with an explosion and the stone-explosion sound plays | pending | `SpecialEffectPosition` and `PlaySoundEffect` are real, but which `ObjectDelete` mode the stone uses was not checked |
| After 2 seconds the screen fades, the camera cuts to look at the sky over the home village and fades in | pending | fade and camera cut are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The day comes back: game time is switched on again and moved to noon over 200 | pending | `GameTimeOnOff` and `MoveGameTime` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After 10 seconds both advisors step out. Good: "The sky! It's turning back to normal!" Evil: "Personally I liked it better before." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 3 seconds later they go home; the screen fades, the camera goes back to where it was before the scene and fades in | pending | `GetCameraPosition` and `CREATE` of a marker are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The darkness stone counts as broken; the music stops and the smoke over the stone is removed | pending | plain script flag, `StopMusic` and `ObjectDelete` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## What it unlocks next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The darkness the land has been in since the start lifts: the land's darkness effect, which held the clock at dusk, lets time run again as soon as this stone breaks | pending | the clock natives are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the Ogre has not been beaten yet, the gold scroll of [The Defending Ogres](the_defending_ogres.md) is put up over the Ogre's valley (active, 10 off the ground) | pending | `CreateHighlight` is real; see [The Defending Ogres](the_defending_ogres.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| With the fire and lightning stones also broken, the land moves on to the [Undead Village](undead_village.md) | pending | plain script flags; see [undead_village.md](undead_village.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: every way it ends breaks the stone | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It cannot soft-lock while either villager can die: the player can always kill Adam or Keiko (by hand, miracle or creature) | pending | the death checks read health (property 1), which is real; not checked in play |
| It cannot start until the Japanese village has been taken, destroyed or emptied, because it waits for the bell-tower elder's introduction | pending | plain script flag; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can carry Keiko: while she is in its hand she is not snapped back, and the reunion waits until it has put her down | pending | `InCreatureHand` is real since 2026-10-21 |
| The creature killing either of them (eating or attacking) ends the quest like the player killing them | pending | the health read is real; not checked in play |

