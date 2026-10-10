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
command (`LoadMap` in `src/CHLApi.cpp`) has an empty body and the first land's script stalls long before its end, so the
fourth land's control script, and this quest, never run. Rows are partial only where every command they need works in
openblack; they still never happen in play.

**Progress: 0/76 done, 0 partial — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background as soon as the land begins, together with the other two stone quests | todo | `LandControl4` never runs (see [../../scripts/land4_script.md](../../scripts/land4_script.md)); `LOAD_MAP` is empty |
| From the start of the land a small Guardian Stone (a meteor at a third of its size, raised 2 off the ground) sits near Adam's hut on the mountain, smoking: a bonfire effect 25 times normal size that lasts the whole land | todo | `CREATE` makes the meteor and its flags are real (`SetIndestructable`, `SetIdMoveable`, `SetIdPickupable`); the height (YPos) is handled, the scale through `SetProperty` is not for a mobile static; `SpecialEffectPosition` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The quest itself waits until the elder of the Japanese village has finished his introduction to [The Totem Puzzle](the_totem_puzzle.md), which itself waits until the player owns, destroys or empties the Japanese village | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The man who explains Nemesis's curse earlier describes this stone: "The third Stone was given to a lonely old man." "Who worships only Nemesis." | todo | his scene is covered with [The Defending Ogres](the_defending_ogres.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Breaking this stone is one of the three conditions the land waits for before the Undead Village and the way out open | todo | plain script flags; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears by the woman's hut, on the lowland west of Adam's mountain, and its challenge is made the current one | todo | `CreateHighlight` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| While the scroll is not clicked, whenever the camera is within 100 of it and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | todo | the notify script calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Clicking the scroll (or its spot) starts the introduction and turns the scroll active | todo | `GameThingClicked` is real and `SetActive` handles highlights; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The scroll is never removed by the script: it stays where it is after the quest is over | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A woman (an Aztec housewife) is made at her hut and Adam (a nomad) at his house on the mountain, facing a spot by the door and mourning on an endless loop | todo | `CREATE` makes villagers; `SetFocus` and the animations on villagers are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A cut scene begins with the generic script music (number 3) | todo | `StartMusic` and `StartCameraControl` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The woman (drawn in high detail) walks out of her hut while the camera glides over 6 seconds to look at her from beside the scroll | todo | `MoveGameThing`, `MoveCameraPosition`, `MoveCameraFocus` and `SetHighGraphicsDetail` are real for villagers; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| When she has arrived and the camera has stopped, she turns to the camera and gossips (three loops of her talking gesture) | todo | villager animations are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The woman: "At last! Someone to hear my prayers!" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades to black over 2 seconds, the camera cuts to Adam's door on the mountain and fades back in over 2 | todo | `SetFade`, `SetFadeIn`, `SetCameraPosition`, `SetCameraFocus` and `EndDialogue` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The challenge log records the quest: title "The Heartbroken Man", progress 0, alignment 0, with the reminder: the good advisor saying "Find Keiko in the Aztec Village and reunite her with Adam." | todo | `Snapshot` is a stub |
| The woman (heard over Adam mourning): "My neighbour, Adam is a soul in torment." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The camera drifts round the mountain top for 10 seconds while she goes on: "Keiko, his wife has been kidnapped and taken to the Aztec Village by Nemesis." | todo | real camera and text natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Before he left he made sure she stays alive only while Adam's faith in him remains." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The camera turns to look down at the woman for 4 seconds, then flies back to her in 6 to 7 seconds; she gossips again: "Please help them by rescuing Keiko and reuniting her with Adam." | todo | real camera, villager and text natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The camera closes on her over 5 seconds as she walks back into her hut | todo | real camera and villager natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The evil advisor steps out: "Hey, Boss. Wasn't he the lonely guy with the Guardian Stone?" then "Let's kill him!" | todo | `SpiritEject` and `RunText` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The music stops and the cut scene ends | todo | `StopMusic` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Keiko (an Aztec housewife) is made at the edge of the Aztec village and the woman who told the story is removed | todo | `CREATE` and `ObjectDelete` of villagers are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Keiko in the Aztec village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko walks at a tenth of normal speed round three spots by the village: at the first she sits down, sits for four loops and stands; at the second she inspects something; at the third she mourns (into, three loops, out of); then back to the first | todo | `MoveGameThing`, the speed and the animations are real for villagers; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| She walks between the spots with despairing walks | todo | `OverrideStateAnimation` on villagers is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Picked up by the hand, thrown, or held by the creature, she is watched: once she is put down again more than 100 from her second spot, she is put straight back there, but only when nobody is holding her, she is not flying and the camera is not looking at her | todo | `SetPosition` is real, but the held read (property 9) is not handled and `InCreatureHand` is a stub |
| So the player can carry her anywhere, but she must be kept in view, or she is snapped back to her village | todo | follows from the held reads, which are not handled |
| The script sets no condition on reaching her; whether the hand can pick her up depends on the game's usual influence rule, which the script does not touch | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bring Keiko to within 50 of Adam: the first time she gets that close, Adam walks out of his house to a spot by his door and waves for attention on an endless loop, and the player's influence is made in a circle of 50 around him | todo | `MoveGameThing` and `InfluenceObject` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Once Adam is out, the reunion starts when Keiko is within 50 of him and neither of them is held by the hand or the creature or flying through the air | todo | the held read (property 9) is not handled and `InCreatureHand` is a stub |
| The quest has no timer and no distance limit other than these: it waits as long as the player likes | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Killing Keiko (or anything that removes her) ends the quest the evil way | todo | the health read (property 1) is not handled by `GetProperty` |
| Killing Adam (or anything that removes him) ends the quest the other way | todo | the health read (property 1) is not handled by `GetProperty` |
| Whichever comes first wins: the script checks Keiko's death, then Adam's death, then the reunion, in that order, every time round | todo | the death checks read health, not handled |

## Reunited (the good ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk and its watcher stop; neither of them can be picked up any more | todo | `StopScript` and `SetIdPickupable` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades to black over a second; the epic script music (number 4) starts; the two are set face to face by Adam's door, drawn in high detail, with the camera close on them; it fades in after 2 seconds | todo | fade, `SetPosition`, the camera, music, `SetFocus` and `SetHighGraphicsDetail` are real for villagers; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After a second they both dance | todo | villager animations are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The challenge log records it complete: progress 1, alignment +0.8 | todo | `Snapshot` is a stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| When the dance ends they hug, then both turn to the camera | todo | villager animations and `SetFocus` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Adam, gossiping: "Oh how can I ever, ever thank you!"; half a second later Keiko joins in with her own gestures | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Keiko: "We're together again! I can't believe it." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Adam: "You are a magnificent god! I have a reason to live!" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| They walk slowly (Adam at a fifth of normal speed, Keiko at 0.15) to the door and into the house while the camera pulls back over 6 seconds; the screen fades to black and the music stops | todo | villager moves and the camera are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Keiko killed (the evil ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black over a second | todo | `StopScript` and the fade are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A cut scene: Adam is set by his door facing the camera, in high detail, mourning; it fades in after 2 seconds | todo | real villager, detail and fade natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The challenge log records it complete: progress 1, alignment -0.8 | todo | `Snapshot` is a stub |
| Adam: "You've killed my soulmate!" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Nemesis promised to protect her. He has failed!" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Take the Stone. I've got no faith left in anything." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades to black | todo | `SetFade` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Adam killed (the other ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black | todo | `StopScript` and the fade are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A cut scene: the neighbour woman is made again at her spot by her hut, facing the camera, mourning for five loops; it fades in after 2 seconds | todo | `CREATE` makes villagers; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The challenge log records it complete: progress 1, alignment 0 (neither good nor evil) | todo | `Snapshot` is a stub |
| The woman: "My neighbour lies dead." then "You are as cruel as Nemesis. Are all gods like this?" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The evil advisor steps out: "Boss, that guy's out of the way." then "And the Stone isn't under Nemesis' influence any longer." and goes home | todo | `SpiritEject`, `SpiritHome` and `RunText` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades to black; the player is given influence in a circle of 200 around the woman, which is never taken away; the woman is let go to live as a normal villager | todo | `InfluenceObject` and `ReleaseFromScript` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The Guardian Stone breaks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever the ending, Adam and Keiko are both removed from the world, even after a happy reunion | todo | `ObjectDelete` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A cut scene: the camera is set looking at the stone by the hut and fades in after 2 seconds | todo | camera cut and fade are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After a reunion the good advisor points at the stone: "Look! The newfound faith of this man has freed the Guardian Stone." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After either killing the evil advisor points at it: "Ha! Killing. It's the answer to everything. Eh, beardy?" then "Never underestimate death. And fear. And agony." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A second later the Guardian Stones music starts; the camera shakes within 300 of the stone (amplitude 0.1, 5 seconds) and follows the stone as it rises, a tenth at a time, to 20 off the ground | todo | `ShakeCamera`, `FocusFollow`, `StartMusic` and the height (YPos) are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A bang and a flash (6 seconds each) go off at the stone, it is destroyed with an explosion and the stone-explosion sound plays | todo | `SpecialEffectPosition` and `PlaySoundEffect` are real, but which `ObjectDelete` mode the stone uses was not checked |
| After 2 seconds the screen fades, the camera cuts to look at the sky over the home village and fades in | todo | fade and camera cut are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The day comes back: game time is switched on again and moved to noon over 200 | todo | `GameTimeOnOff` and `MoveGameTime` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After 10 seconds both advisors step out. Good: "The sky! It's turning back to normal!" Evil: "Personally I liked it better before." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| 3 seconds later they go home; the screen fades, the camera goes back to where it was before the scene and fades in | todo | `GetCameraPosition` and `CREATE` of a marker are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The darkness stone counts as broken; the music stops and the smoke over the stone is removed | todo | plain script flag, `StopMusic` and `ObjectDelete` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## What it unlocks next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The darkness the land has been in since the start lifts: the land's darkness effect, which held the clock at dusk, lets time run again as soon as this stone breaks | todo | the clock natives are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| If the Ogre has not been beaten yet, the gold scroll of [The Defending Ogres](the_defending_ogres.md) is put up over the Ogre's valley (active, 10 off the ground) | todo | `CreateHighlight` is real; see [The Defending Ogres](the_defending_ogres.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| With the fire and lightning stones also broken, the land moves on to the [Undead Village](undead_village.md) | todo | plain script flags; see [undead_village.md](undead_village.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: every way it ends breaks the stone | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| It cannot soft-lock while either villager can die: the player can always kill Adam or Keiko (by hand, miracle or creature) | todo | the death checks read health (property 1), not handled |
| It cannot start until the Japanese village has been taken, destroyed or emptied, because it waits for the bell-tower elder's introduction | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can carry Keiko: while she is in its hand she is not snapped back, and the reunion waits until it has put her down | todo | `InCreatureHand` is a stub |
| The creature killing either of them (eating or attacking) ends the quest like the player killing them | todo | the health read is not handled |

## Unused or cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An early draft of the lines survives in the source comments: Adam was "Knobbo", Keiko his "girlfriend", and the woman was simply his friend and neighbour | n/a | comments only |
| A test launcher (not compiled into the game) runs this quest on its own by setting the start flag | n/a | never in the shipped `challenge.chl` |
