# The Ogre

A first-land silver scroll: an ogre named Sleg sleeps in a narrow pass, guarding a reward. The player either feeds
him twice until he falls asleep, or sends the creature in to beat him in a fight; either way the way to the reward
opens, the reward turns out to be a beach ball, and a healing miracle dispenser appears by the temple.

**Land:** 1 · **Giver:** the advisors, at the ogre's pass (the ogre himself speaks once) · **Script:** CreatureGuardian · **Reward:** a beach ball (toy ball) reward behind the ogre, then a healing miracle dispenser near the temple (the powered-up heal if the healing miracle was already given) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on this tree: of the 74 script functions
the quest and the scripts it starts need, 16 only log "not implemented" in `src/CHLApi.cpp`, among them every creature
action, desire and fight command, the snapshot and reward commands, and making a creature from a script
(`CreatureCreateRelativeToCreature`). The land's control script starts this quest only in a game that skips the creature
training (the start-up box's fourth answer), and there the ogre never appears, since the fight lesson that lets it out
is skipped (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), so none of it happens; rows are todo
unless the notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md), the script's function coverage
in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), chests and dispensers in
[../rewards.md](../rewards.md).

**Progress: 1/53 done, 6 partial — 8%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script is started by the land's control script after the guide's fight lesson, alongside the exit-vortex script, and then waits for the flag the fight lesson raises at its end | partial | with the fourth tutorial answer `LandControl1` starts it with the exit-vortex script and it waits on the flag; with the first answer the land's control script never gets that far (the intro does not finish) |
| A new game that skips the creature training never runs the fight lesson, so the flag is never raised and the ogre never appears in that game | done | same in our tree: `CanSkipCreatureTraining` reads the requester's answer, `LandControl1` skips the lesson and the script waits for good (`Gui/SkipBox`, `Game::Run`) |
| When the guide is asleep after the fight lesson and is clicked, it swings the camera to the ogre's place ("Why don't you try out what you've learnt while I sleep?") only if the ogre's scroll hasn't been clicked yet | todo | the creature guide's lessons never run in our tree |
| The ogre is made from the player's creature (same mind data, scale 1) as an ogre, at his lair in a narrow pass | todo | the quest never starts in our tree (row 2 above); `CreatureCreateRelativeToCreature` is a stub and CHL `CREATE` makes no creatures |
| He keeps itself 20% bigger than the player's creature for the rest of the quest | todo | never reached; `CreatureAutoscale` is a stub |
| His name over him is "Sleg" | todo | never reached; `SetCreatureName` is a stub |
| His alignment is set fully evil; all his desires are switched off except hunger (held at 0.1) and anger (held at 0.05); his own agenda gets almost no priority | todo | never reached; `CreatureSetDesireActivated`, `CreatureSetDesireValue`, `CreatureSetDesireMaximum`, `CreatureSetAgendaPriority` are stubs and `SetProperty` has no alignment |
| He sleeps beside himself (the "sleep by object" state) at the lair until the scroll is clicked | todo | never reached; `CreatureDoAction` is a stub |
| From the moment he exists until the introduction ends, whenever the player's creature comes within 70 of him it is unleashed and walked back to a spot down the valley (checked at most every 5 seconds), and freed again once out of range | partial | never reached; `DetachObjectLeash`, `ReleaseFromScript` and the timers work, but `MoveGameThing` does not move creatures |
| A silver challenge scroll stands at the pass, a little beyond the lair | todo | script scrolls work (`CreateHighlight`, `src/ECS/ScriptHighlight.cpp`), but the quest never starts: the fight-lesson flag is never raised |
| While the camera is within 100 of the scroll and it is on screen, at most every 30 seconds the good advisor steps out, points at it and says "Your attention is required here." | todo | the notice's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`, `PosFieldOfView`, `DllGettime`), but the quest never starts |
| Clicking the scroll or the spot it marks starts the introduction | todo | `GameThingClicked` on a scroll works, but the quest never starts |

## Preparations made on the click

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A beach ball reward is placed in a hollow behind the ogre, at the far end of the pass | todo | never reached; `CreateReward` is a stub; the ball itself: [../../nature/toys.md](../../nature/toys.md) |
| An anti-influence circle of radius 50 is put over the reward so the hand can't reach it, and an influence circle of radius 50 round the lair so the hand can act at the ogre | todo | `InfluencePosition` makes influence and anti-influence rings (`src/Magic/Script/CHLInfluence.cpp`), but the quest never starts |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sleg's own music plays for the scene | partial | the music command plays the track (`StartMusic`, `src/Audio`); the scene never runs |
| The film starts: the creature's leash is taken off and it stands idle; the camera glides (3 s) to view the pass's entrance from below, looking at the ogre's upper body | todo | the scene never runs; the leash and camera natives work, but the ogre is never made |
| The ogre scratches himself | todo | never reached; `OverrideStateAnimation` does not handle creatures |
| The good advisor steps out, points at the ogre: "Ugh, how horrid. I think he's trying to speak." then clings to the screen's side | todo | the scene never runs (the advisor natives, `ClingSpirit` included, work) |
| The camera moves to the other side of the entrance; the scroll is entered in the challenge list at 0% with the reminder "The Ogre's guarding something. We should investigate." | todo | never reached; `Snapshot` is a stub |
| The ogre, in his own voice: "Me guard here. Me big hungry. Me name Sleg." while the camera moves lower | todo | the scene never runs |
| The good advisor: "This ogre seems to be guarding something. A reward, perhaps?" and goes home | todo | the scene never runs |
| The evil advisor steps out and looks at the camera: "He's guarding the pass. He's outside our influence so let's send our Creature to fight him." and goes home | todo | the scene never runs |
| The ogre turns to the player's creature and acts hungry | todo | never reached; creature actions are stubs |
| The good advisor points at and looks at the ogre: "We shouldn't resort to combat with the poor thing. Hmm. He looks hungry." | todo | the scene never runs |
| The creature points at the ogre; the ogre looks it over, plays his angry animation and goes back to sleep; the camera rises over the pass and the music stops | todo | never reached; `CreatureDoAction` is a stub |
| The creature is given back to the player, and from now on it may walk up to the ogre | todo | never reached |

## Feeding him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 seconds the script looks for a store of food within 50 of the ogre | todo | never reached |
| When food is there and the player's creature is not within 50 of it, the ogre is fed for the first time (a film): the camera swings to the food, the ogre walks to it and eats it as he would a miracle food pile | todo | never reached; creature actions are stubs |
| The rest of the food is removed; the scroll goes to 25%; the good advisor: "Well he's eaten that. Is he getting drowsy? Or is it me?"; the ogre walks back to his lair and the camera returns to where it was | todo | never reached; `UpdateSnapshot` is a stub |
| Food put by him a second time (again with the creature not within 50) is eaten the same way; the ogre then falls asleep beside himself and the food is removed | todo | never reached |
| The evil advisor: "That big oaf's as thick as a rock! He's gone right to sleep. Duh!"; the scroll goes to 50% | todo | never reached; `UpdateSnapshot` is a stub |
| The camera flies through the pass (from the tunnel's start to its end) and comes to rest looking at the reward; the creature is freed | todo | never reached (the camera path natives work) |

## Fighting him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A fight starts when the creature comes within 30 of the ogre while he hasn't eaten twice and 30 seconds have passed since the last fight; or at once, whatever else, if it gets within 30 of a spot just past him (trying to slip by) | todo | never reached (the timer natives work) |
| The creature is held idle, its leash is switched off and taken off | partial | never reached; the leash commands work (`SetLeashWorks`, `DetachObjectLeash`) |
| The evil advisor: "Uh oh. You've got him mighty riled. Prepare for battle, Boss!" | todo | never reached |
| The ogre is told to fight the creature; once the creature is fighting the ogre's strength is lowered by 0.2 | todo | never reached; the creature fight natives are stubs and `SetProperty` has no strength |
| When both have stopped fighting, the leash works again and the creature is freed; whichever has less fight health lost | todo | never reached; `IsFighting` is a stub and `GetProperty` has no health |
| Won: the camera closes on the ogre, who is killed for good; the evil advisor: "You beat him! Now let's get the reward and scram!"; the camera then flies through the pass to the reward as for feeding | todo | never reached; the creature action natives are stubs |
| Lost: both creatures stand idle; the good advisor: "Hold on, Leader. Pull back from the fight and try again when we've recovered some strength."; the scroll is set back to 0%; the ogre walks back to his lair and faces the creature | todo | never reached; `UpdateSnapshot` is a stub |
| After a lost fight the ogre heals by 1% of his health every 6 seconds (about 10 minutes from nothing to full), stopping for good if he starts fighting again | todo | never reached; the shared fight-healing script needs creature health (`GetProperty`, `SetProperty`) |
| The player can try again after 30 seconds; every new fight lowers the ogre's strength by another 0.2, so each retry is easier (how low strength can go was not confirmed) | todo | never reached |

## Success and the reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once he sleeps or dies, the anti-influence over the reward is replaced with an influence circle (radius 50) so the hand can reach it, and the lair's influence is removed | todo | `InfluencePosition` and deleting a ring work, but the quest never starts |
| The scroll goes to 75% with the reminder "Let's look for whatever that Ogre was hiding." | todo | never reached; `UpdateSnapshot` is a stub |
| The ogre (asleep or dead) is removed as soon as the camera is more than 100 away from him and he isn't on screen | todo | never reached (`ObjectDelete` and the field-of-view checks work) |
| When the beach ball reward is opened, the scroll is completed (100%) | todo | never reached; `CreateReward` and `IsActive` are stubs; opening chests: [../rewards.md](../rewards.md) |
| A healing miracle dispenser is built near the temple; if the healing miracle was already handed out (the Pied Piper also gives one), it is the powered-up healing miracle instead; there is no dispenser film or help for it | partial | never reached; CHL `CREATE` makes spell dispensers and `SetMagicProperties` works (`src/Magic/Script/CHLWorship.cpp`) |
| The dispenser's recharge time is set to 0 seconds (what a zero time does was not confirmed) | todo | never reached |
| The evil advisor: "A beach ball? We went through all that for a lousy beach ball?!"; a second later the good advisor: "Hold on! A spell Dispenser just appeared near our Temple!" | todo | never reached |
| The quest is marked finished for the rest of the land | todo | never reached |

## Advisors, music and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sleg's music plays only for the introduction | partial | `StartMusic` / `StopMusic` work; the scene never runs |
| The ogre is a creature: he eats, sleeps, fights and is hurt like one, and the player's creature is the only one who can fight him | todo | no ogre is ever made; creature fights: see ../../creature/ |
| The player's creature is forced to point at the ogre in the introduction and is held for each film | todo | never reached; `CreatureDoAction` is a stub |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A day-to-night script is started with the quest but its whole body is commented out, so it does nothing; it once darkened the time of day and raised a lightning drizzle while the camera was near the pass | n/a | cut |
| An unused evil line: "Look! Here it is. You know I reckon we could fight our way up to it." | n/a | in the text table, used by no script |
| Cut ways to finish: the ogre running after the creature when hit by a rock, and the quest ending when the player's influence covered the reward | n/a | commented out of the script |
| A cut reward of a heal-miracle dispenser at the reward's spot | n/a | commented out |
| The fourth land has a different ogre quest with its own script | n/a | not this quest |
| A test script starts this quest alone with the fight-lesson flag raised | n/a | not started by the game |
| A saved game keeps the ogre, his state and the scroll | todo | openblack has no saved games |
