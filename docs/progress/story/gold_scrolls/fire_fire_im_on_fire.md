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
game's text table. openblack never runs Land 3's control script (the land-loading command does nothing). Fire works in
openblack (setting things alight, the hurt-by-fire switch, reading whether something burns), and so do the bonfire's
creation, villagers, dialogue, camera cuts, fades and music, but snapshots and the rival god's hand are stubs in
`src/CHLApi.cpp`. Rows are todo unless the notes say otherwise.

**Progress: 0/45 done, 0 partial — 0%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly one pillar is down and the Japanese village is the player's; win another village first and it never happens | todo | the prison's pillars follow town ownership, read through stubs (`GetTownWithId`), so it is never started |
| A bonfire is made on the beach below the Japanese village as soon as it starts | todo | `CREATE` makes the bonfire (`CreateScriptObject`); never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry 20 seconds after it ends | todo | `Snapshot` and `UpdateSnapshot` are stubs |
| It waits 60 seconds; if all three pillars are down by then, nothing happens | todo | plain script wait; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## Lethys's wrath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music fades to black; Lethys's hand is held and flown fast to a spot above the beach | todo | Lethys's hand (`MoveComputerPlayerPosition`, `ReleaseComputerPlayer`) is a computer-player stub; `StartMusic` and the fade are real |
| The camera is set looking at Lethys's hand and fades back in | todo | `SetCameraPosition`, `SetCameraFocus`, `SetFade`, `SetFadeIn` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Sixteen Japanese fishermen are made round the bonfire, all facing it and unhurt by fire: they prod the fire, sit, sleep, yawn or look puzzled, each animation looping | todo | `CREATE` makes villagers, `SetFocus`, the animations and `SetHurtByFire` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Game sound effects are switched on | todo | `SetGameSound` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Lethys: "Your ally is dead. I have your Creature and you still dare to oppose me?" | todo | `RunText` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera turns to the campfire. Lethys: "I see some of your minions have strayed." while it closes in low by the fire | todo | real camera and text natives; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| A second later the camera cuts back to Lethys's hand. Lethys: "Behold my wrath!" | todo | real camera and text natives; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The weather is switched off and cleared within 150 of a spot between the beach and the village | todo | `PauseUnpauseClimateSystem` and `KillStormsInArea` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| One and a half seconds later Lethys throws three level-one fireballs: at the sand by the fire, at the bonfire and at the third fisherman | todo | `SpellAtPos` and `SpellAtThing` are real (`src/Magic/Script/CHLSpells.cpp`); see [../../miracles/fireball.md](../../miracles/fireball.md); never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The fishermen are set alight and start running | todo | `SetOnFire` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera pulls back over the beach. The story-log entry is started at nothing done, its reminder the good advisor's "Oh no! They're heading towards the Village!" | todo | the log entry is a stub |
| Both advisors step out. Good: "Oh no! They're heading towards the Village!" Evil: "Stop them! Put them out! Do something!" | todo | `SpiritEject` and `RunText` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| A fade to black and the advisors go home | todo | fade and `SpiritHome` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the monk appears on the path up to the village, playing his ambient animation three times, with two water one-shot miracles beside him | todo | `CREATE` makes villagers and one-shot miracles (`magic::script::CreateOneShotSpell`); never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Monk: "Your people are mad with pain and fear." Monk: "Let me help. It's the least I can do." | todo | `RunText` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera turns to the miracles. Monk: "These Water Miracles will be of use. But hurry." Monk: "That is all." A fade to black and he is gone | todo | real camera, text, fade and delete natives; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| While the monk talks, fishermen that reach the first point on their route wait there until he has finished | todo | plain script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Without the monk there are no water miracles from the script | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera is set over the path to the village, fades in, and Lethys's hand is let go | todo | the camera and fade are real; letting Lethys's hand go is a stub |

## The burning fishermen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each runs at a random 0.3 to 0.5 of normal speed with the on-fire running animation | todo | the speed and the animation overrides are handled for villagers; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Each first runs about wildly: seven short dashes to random spots within 10 of itself, set alight again at each | todo | `MoveGameThing` and `SetOnFire` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| It then runs to a point on the path, kept alight all the way, then to a second point near it (shifted at random by up to 5), then on to its target in the village | todo | villager moves and `SetOnFire` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Six make for the town centre, five for the storage pit, three for the crèche and two for the temple | todo | plain script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Within 2 of its target it waits 5 seconds and from then on is hurt by its flames, so it burns to death unless put out | todo | `SetHurtByFire` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| If it is picked up it waits to be dropped; if thrown it waits to land; then it runs on to its target, or to the second point if it had not yet passed it | todo | the held read (property 9) is not handled; flying is |
| A fisherman's run ends when it is no longer on fire or no longer exists | todo | `IsOnFire` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| A fisherman put out while still alive joins the Japanese village | todo | joining a town (`AttachToGame`) is a stub |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Put the fishermen out before their flames kill them: with water (the monk's miracles, or the player's own), or by dropping them in the sea | todo | fire and water are real in our tree; see [../../miracles/water.md](../../miracles/water.md); never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| There is no time limit other than how long the fishermen burn | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the sixteenth fisherman's run ends a cut scene starts; if that last one is alive the camera closes in on him | todo | real camera natives; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| All 16 saved: the last fisherman says "Thank you, Mighty One. You saved us all." and the entry is marked complete with alignment +1 (good) | todo | the log is a stub, so no alignment change; the line is real |
| 6 to 15 saved and the last one alive: he says "Thank you. At least we all didn't die."; complete, no alignment change | todo | the log is a stub |
| 6 to 15 saved and the last one dead: the good advisor steps out: "Well, you tried to save them. Pity you didn't help them all."; complete, no alignment change | todo | the log is a stub |
| 5 or fewer saved: the evil advisor steps out: "You let 'em fry. That was a treat to see, Boss."; complete with alignment −0.8 (evil) | todo | the log is a stub, so no alignment change |
| After the last fisherman, the music stops and the weather is switched back on | todo | `StopMusic` and `PauseUnpauseClimateSystem` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fires the fishermen started in the village stay to burn or be put out by the normal fire rules | todo | fire spreads by our fire rules; see ../nature/ and ../building/; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Saved fishermen stay in the Japanese village as ordinary villagers | todo | joining a town (`AttachToGame`) is a stub |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It cannot block the story: freeing the creature does not wait for it | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fishermen seven and eight are made twice: the first pair are left standing at the campfire with no animation, no fire protection and no script, and are not counted (undetermined: whether Lethys's fireballs burn them) | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The wait for all sixteen to finish has no pause in its loop | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| A test switch skips the 60-second wait | n/a | only the test launcher uses it |
| The story-log title is a line of the thrown man's script on the same land ("Fire! Fire! I'm on fire!") | todo | the log is a stub |
