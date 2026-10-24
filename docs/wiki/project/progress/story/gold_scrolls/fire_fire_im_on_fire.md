# Fire! Fire! I'm on Fire!

A gold story event on the third land: once the player has won the Japanese village (as the first of the three villages
that hold the creature's prison), Lethys sets sixteen of the village's fishermen alight at their beach campfire. Mad
with pain, they run for the village's buildings. The player must put them out before they reach the village and burn to
death; how many are saved sets the alignment. If the monk's quest is done, he hands over two water miracles. It is
logged in the story log under the line "Fire! Fire! I'm on fire!" (borrowed from the man who wants to be thrown; it has
no title of its own) and has no scroll to click.

**Land:** 3 · **Giver:** Lethys (a cut scene; no scroll) · **Script:** FreeTheCreature (its fanatic attack) · **Reward:** saved fishermen join the Japanese village; alignment +1 for all 16, none for 6 to 15, −0.8 for 5 or fewer · **Repeatable:** no

Part of freeing the creature: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md).
The monk: [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md). Water: [../../miracles/water.md](../../miracles/water.md),
fireball: [../../miracles/fireball.md](../../miracles/fireball.md), one-shot miracles:
[../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md). The land: [../land_3.md](../land_3.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table. openblack never runs Land 3's control script (the story reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked). Fire works in
openblack (setting things alight, the hurt-by-fire switch, reading whether something burns), and so do the bonfire's
creation, villagers, dialogue, camera cuts, fades, music, the challenge records (snapshots) and the rival god's hand
(the computer player's hand natives in `src/CHLApi.cpp`); joining a fisherman to a town (`AttachToGame`) still only
logs "not implemented". Rows are pending unless the notes say otherwise.

**Progress: 0/45 done, 0 chosen, 0 partial, 0 inferred — 0%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly one pillar is down and the Japanese village is the player's; win another village first and it never happens | pending | the prison's pillars follow town ownership, read through `GetTownWithId` (real); not checked in play |
| A bonfire is made on the beach below the Japanese village as soon as it starts | pending | `CREATE` makes the bonfire (`CreateScriptObject`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry 20 seconds after it ends | pending | `Snapshot` and `UpdateSnapshot` are real |
| It waits 60 seconds; if all three pillars are down by then, nothing happens | pending | plain script wait; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## Lethys's wrath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music fades to black; Lethys's hand is held and flown fast to a spot above the beach | pending | Lethys's hand (`MoveComputerPlayerPosition`, `ReleaseComputerPlayer`, through `Locator::computerPlayerSystem`), `StartMusic` and the fade are real; not checked in play |
| The camera is set looking at Lethys's hand and fades back in | pending | `SetCameraPosition`, `SetCameraFocus`, `SetFade`, `SetFadeIn` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Sixteen Japanese fishermen are made round the bonfire, all facing it and unhurt by fire: they prod the fire, sit, sleep, yawn or look puzzled, each animation looping | pending | `CREATE` makes villagers, `SetFocus`, the animations and `SetHurtByFire` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Game sound effects are switched on | pending | `SetGameSound` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Lethys: "Your ally is dead. I have your Creature and you still dare to oppose me?" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera turns to the campfire. Lethys: "I see some of your minions have strayed." while it closes in low by the fire | pending | real camera and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| A second later the camera cuts back to Lethys's hand. Lethys: "Behold my wrath!" | pending | real camera and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The weather is switched off and cleared within 150 of a spot between the beach and the village | pending | `PauseUnpauseClimateSystem` and `KillStormsInArea` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| One and a half seconds later Lethys throws three level-one fireballs: at the sand by the fire, at the bonfire and at the third fisherman | pending | `SpellAtPos` and `SpellAtThing` are real (`src/Magic/Script/CHLSpells.cpp`); see [../../miracles/fireball.md](../../miracles/fireball.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The fishermen are set alight and start running | pending | `SetOnFire` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera pulls back over the beach. The story-log entry is started at nothing done, its reminder the good advisor's "Oh no! They're heading towards the Village!" | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Both advisors step out. Good: "Oh no! They're heading towards the Village!" Evil: "Stop them! Put them out! Do something!" | pending | `SpiritEject` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| A fade to black and the advisors go home | pending | fade and `SpiritHome` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the monk appears on the path up to the village, playing his ambient animation three times, with two water one-shot miracles beside him | pending | `CREATE` makes villagers and one-shot miracles (`magic::script::CreateOneShotSpell`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Monk: "Your people are mad with pain and fear." Monk: "Let me help. It's the least I can do." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera turns to the miracles. Monk: "These Water Miracles will be of use. But hurry." Monk: "That is all." A fade to black and he is gone | pending | real camera, text, fade and delete natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| While the monk talks, fishermen that reach the first point on their route wait there until he has finished | pending | plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Without the monk there are no water miracles from the script | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera is set over the path to the village, fades in, and Lethys's hand is let go | pending | the camera, the fade and letting Lethys's hand go (`ReleaseComputerPlayer`) are real; not checked in play |

## The burning fishermen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each runs at a random 0.3 to 0.5 of normal speed with the on-fire running animation | pending | the speed and the animation overrides are handled for villagers; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Each first runs about wildly: seven short dashes to random spots within 10 of itself, set alight again at each | pending | `MoveGameThing` and `SetOnFire` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| It then runs to a point on the path, kept alight all the way, then to a second point near it (shifted at random by up to 5), then on to its target in the village | pending | villager moves and `SetOnFire` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Six make for the town centre, five for the storage pit, three for the crèche and two for the temple | pending | plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Within 2 of its target it waits 5 seconds and from then on is hurt by its flames, so it burns to death unless put out | pending | `SetHurtByFire` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| If it is picked up it waits to be dropped; if thrown it waits to land; then it runs on to its target, or to the second point if it had not yet passed it | pending | the held read (property 9, `GET_PROPERTY` InHand) and the flying read are real; not checked in play |
| A fisherman's run ends when it is no longer on fire or no longer exists | pending | `IsOnFire` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| A fisherman put out while still alive joins the Japanese village | pending | joining a town (`AttachToGame`) is a stub |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Put the fishermen out before their flames kill them: with water (the monk's miracles, or the player's own), or by dropping them in the sea | pending | fire and water are real in our tree; see [../../miracles/water.md](../../miracles/water.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| There is no time limit other than how long the fishermen burn | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the sixteenth fisherman's run ends a cut scene starts; if that last one is alive the camera closes in on him | pending | real camera natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| All 16 saved: the last fisherman says "Thank you, Mighty One. You saved us all." and the entry is marked complete with alignment +1 (good) | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`), so no alignment change; the line is real |
| 6 to 15 saved and the last one alive: he says "Thank you. At least we all didn't die."; complete, no alignment change | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| 6 to 15 saved and the last one dead: the good advisor steps out: "Well, you tried to save them. Pity you didn't help them all."; complete, no alignment change | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| 5 or fewer saved: the evil advisor steps out: "You let 'em fry. That was a treat to see, Boss."; complete with alignment −0.8 (evil) | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`), so no alignment change |
| After the last fisherman, the music stops and the weather is switched back on | pending | `StopMusic` and `PauseUnpauseClimateSystem` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fires the fishermen started in the village stay to burn or be put out by the normal fire rules | pending | fire spreads by our fire rules; see ../nature/ and ../building/; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Saved fishermen stay in the Japanese village as ordinary villagers | pending | joining a town (`AttachToGame`) is a stub |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It cannot block the story: freeing the creature does not wait for it | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fishermen seven and eight are made twice: the first pair are left standing at the campfire with no animation, no fire protection and no script, and are not counted (undetermined: whether Lethys's fireballs burn them) | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The wait for all sixteen to finish has no pause in its loop | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The story-log title is a line of the thrown man's script on the same land ("Fire! Fire! I'm on fire!") | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
