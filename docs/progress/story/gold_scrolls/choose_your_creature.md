# Choose Your Creature

The first land's first creature chapter: Sable, the creature trainer, challenges the player to open the gates of the
creatures' glade with three gate stones; the first is the stone the villagers danced round, the second is earned in
[The Lost Brother](the_lost_brother.md), the third is carved by [The Sculptor](the_sculptor.md). With all three on the
platform the gates open, the cow, the ape and the tiger show off, and the player picks one by clicking it twice.

**Land:** 1 · **Giver:** Sable, the creature trainer, at the creature gates north of the Norse village · **Script:** ChooseYourCreature (with CreaturesInGlade and the gate-stone guards in ProtectGateKeys) · **Reward:** the player's creature (cow, ape or tiger); no alignment change · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table, the land file `Land1.txt` and the executable (how a gate stone fits into the
platform). openblack's state is judged on this tree: the land's control script reaches this quest after the opening and
the temple scene (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and no command on its paths, nor
on those of the scripts it starts, only logs "not implemented" in `src/CHLApi.cpp`. A run on 2026-10-22 (Land 1,
SkipBox answer 0, deterministic fixed-step runs) played the tutorial, the temple scene and this quest up to the first
stone in one launch: the quest starts once the temple scene is over, and laying the tiger gate stone in its platform
sets `GateKey2Present` to 1 and starts The Lost Brother. Nothing after that has been run yet; the glade makes its
creatures (`CREATE` of a creature) but has not been reached without skipping. Every row is pending unless the notes say
otherwise. How a creature is chosen and what the species differ in:
[../../creature/species_choice.md](../../creature/species_choice.md).

**Progress: 1/90 done, 0 chosen, 51 partial, 0 inferred — 23%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold scroll (a story highlight), logged as "Choose Your Creature" | partial 70% | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); the log pictures `Snapshot` and `UpdateSnapshot` are real and the challenge log is not drawn; re-measured 2026-10-21: the highlight and the snapshots are real; the challenge log is not drawn |
| It starts once the opening is over: after the family has led the player home and the temple scene is done, and after the Throwing Stones and Lost Flock silver scrolls have been set going | partial 60% | `LandControl1` runs it after `FollowUs` and `CitadelGuide`; in the run of 2026-10-22 it starts once the temple scene is over. See [../land_1.md](../land_1.md), [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md); the two silver starts were not looked at in that run |
| The land's control script waits for the whole quest (all three stones and the choice) before going on | pending | not checked in play: the glade's creatures are made now (`CREATE` of a creature), and no check has yet played the choice loop to its end |
| What it unlocks next: the creature breeder, the Explorers and the guide's wandering are started, then the trainer's first lesson (seeing the creature's home pen) begins | pending | not checked in play: the glade's creatures are made now (`CREATE` of a creature), and no check has yet played the choice to its end. See [the_creatures_learning.md](the_creatures_learning.md); [../creature_guide.md](../creature_guide.md); [../silver_scrolls/the_creature_breeder.md](../silver_scrolls/the_creature_breeder.md); [../silver_scrolls/the_explorers.md](../silver_scrolls/the_explorers.md) |
| The Lost Brother is started by this quest when the first stone is placed, and The Sculptor when the second is | partial 50% | the gate-stone loop reads the platform's count (`ObjectInfoBits`, real); in the run of 2026-10-22 laying the tiger stone starts [the_lost_brother.md](the_lost_brother.md); the second stone, and so [the_sculptor.md](the_sculptor.md), is not reached in a run yet |
| The scroll's progress mark and reminder change at every step: 0 at the start (reminder "Didn't we see the Villagers dancing around a Stone a while ago?"), 0.25 with one stone ("We need to find another Gate Stone. Perhaps the Villagers can help."), 0.5 with two ("We should speak to the sculptor about the Gate Stones."), 0.75 in the glade and 1 when a creature is chosen ("We need to choose a Creature!") | pending | the progress marks are set through `UpdateSnapshot`, which is real; the challenge log is not drawn |
| Each reminder is said by whichever advisor owns the line (all of these are the good advisor's), stepping out to say it | partial 60% | the shared reminder calls only real natives (`SpiritEject`, `RunText`); the reminder texts it would read come from the log, real now, not yet checked in this quest; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## The scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze did-you-know scroll is placed beside the gate stone platform: "Every time you activate a Gold Story Scroll you start the next chapter of the story of Black and White. How quickly you progress depends on how often you choose to activate them." | partial 60% | `CreateHighlight` and `HighlightProperties` are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| A gold scroll appears in front of the creature gates, raised 14 above the ground | partial 60% | `CreateHighlight` and the height through `SetProperty` (YPos) are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Whenever the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points at it and says "There's a Gold Story Scroll down here, Boss.", at most once every 30 seconds and only when no cut-scene is running | partial 60% | the shared notify script `ChallengeHighlightNotify` calls only real natives (`SpiritPointPos`, `RunText`); the quest runs in the run of 2026-10-22, but whether the advisor nagged was not looked at |
| The nagging stops when the scroll (or the place it stands) is clicked | partial 60% | `GameThingClicked` is real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Nothing ever removes the gold scroll in this script once it is clicked (undetermined: whether the game hides a story scroll by itself once its chapter is logged as finished) | partial 60% | the same script runs, so nothing removes it either; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |

## The trainer's challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cut-scene with the creature guide's theme; the good advisor is sent home | partial 60% | `StartMusic` (`src/Audio/GameMusic`), `StartCameraControl` and `SpiritHome` are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Sable, the trainer, is made inside the hut by the gates, walks out of its door to a spot before the camera and turns to face it, the camera gliding down to her and following her | partial 60% | `CREATE` makes villagers, `MoveGameThing` and `SetFocus` work on villagers, `MoveCameraPosition` and `FocusFollow` are real (`src/Camera/ScriptCamera`); played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| The quest is logged in the story's log at this point, with a picture, at 0 progress | pending | `Snapshot` is real; the challenge log is not drawn |
| Sable bows and says "Greetings, Holy One. You've activated a Gold Story Scroll." then "My name is Sable. I am a trainer of Creatures." | partial 60% | `RunText`, `TextRead` and the villager's animation (`OverrideStateAnimation`) are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Gossiping: "I challenge you to open these gates.", "Behind them are three wondrous Creatures.", "Every god has a Creature.", "They can grow as tall as a mountain and can perform Miracles." | partial 60% | real text natives; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| "But you can't open the Gate unless you find the three Gate Stones." then "Each Stone you find must be placed on this platform.": the camera swings round to show the empty platform for a few seconds and comes back to her | partial 60% | real camera and text natives; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| "The Villagers were dancing around a Gate Stone when you arrived.": the camera rises over the land and flies down to the tiger gate stone at the end of the ravine the family led the player through, which is lit up | partial 60% | real camera natives; the stone's guard lights it (below); played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Over the stone: "Once you place this Gate Stone on the platform, I will tell you of the other two." | partial 60% | `RunText` is real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| A quick fade to black and back to Sable: "Now retrieve the first Gate Stone, Holy One." | partial 60% | `SetFade`, `SetFadeIn`, `FadeFinished` and the camera and text natives are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| The camera rises high over the hut, looking south over the village, the music stops and the cut-scene ends | partial 60% | real camera natives and `StopMusic`; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| The tiger stone can now be picked up and moved (it was fixed in place since the family's welcome dance) | partial 60% | `SetIdPickupable` and `SetIdMoveable` are real; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| Sable walks back to her door and is removed 3 seconds later | partial 60% | `MoveGameThing` and `ObjectDelete` work on villagers; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |

## Finding the first stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three stones are part of the land: the tiger stone where the villagers danced, the ape stone by the lost brother's house and a blank rock in the quarry, with the gate stone platform and the gates by the trainer's hut | done | `Land1.txt` places them (`MobileStaticArchetype`, `AnimatedStaticArchetype`); see [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| A gate stone is dropped from the hand onto the platform to place it: the stone disappears and the platform shows it in one of its three sockets | pending | laying a held gate stone in the platform works (`src/ECS/HeldApply.cpp:249`, `AnimatedStaticSystem::LayGateStone`: the stone leaves the hand, fades and goes, and the platform counts it; test `AnimatedStaticSystemTest.ThePlinthTakesTheCarvedStonesAndCountsThem`); whether the platform shows the stone in a socket is not checked in play |
| The quest knows which stones are in by what the platform holds: the ape stone counts 1, the tiger 2 and the cow 4, so 3 means ape and tiger and 7 all three; the blank rock counts nothing | partial | `ObjectInfoBits` gives each stone's worth (ape 1, tiger 2, cow 4); not checked in play |
| A stone once in the platform stays in it; the player can't take it out again | pending | a laid stone is removed from the land (`src/ECS/HeldApply.cpp:249`), so it can't be taken out; not checked in play |
| When the camera is within 100 of the tiger stone and it is on screen, the good advisor points at it: "Here's the stone the people were dancing around! Pick it up by clicking the Action Button on it.", at most every 30 seconds, until the player first picks it up | partial | `AdvisorSpotsGateKey2` calls only real natives (field of view, `SpiritPointPos`, `RunText`); the held read (property 9) is real (`src/CHLApi.cpp:1679`); the stone is picked up and laid in the run of 2026-10-22, but whether the advisor pointed at it first was not looked at |
| The first time the player hits the gates (for instance throwing something at them), the good advisor says "I sense that battering the Gates won't work, esteemed one."; only once | pending | `GameThingHit` and `ClearHitObject` are real |
| Clues while the platform is empty: 2 minutes after the cut-scene the good advisor says "Didn't we see the Villagers dancing around a Stone a while ago?" | partial 60% | `SetTimerTime` and the timer reads are real (`src/ECS/ScriptTimer`); the clue timers were not looked at in the run of 2026-10-22 |
| Every 2 minutes after that, still empty, the good advisor points at the stone and says "The Stone's at the end of the ravine we followed the Villagers through!" if it is within 50 of where they danced, or the first clue again if it has been moved | partial 60% | real timer, distance and advisor natives; played in the run of 2026-10-22 (the quest runs up to the first stone); not compared with the original |
| From the second clue on the scroll's reminder becomes the ravine line, even when the advisor actually said the first clue | partial 60% | the same script runs; the reminder is written through the log, real now, not yet checked in this quest; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The clues stop for good once any stone is in the platform | pending | the platform reads its stones (`ObjectInfoBits`, real) and the tiger stone is read in the run of 2026-10-22; whether the clues stop was not looked at |

## The first stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placing the tiger stone plays a scaffold-planting sound and moves the scroll to 0.25 | pending | the placement is reached in the run of 2026-10-22 (`GateKey2Present` 1); the sound and the scroll's mark were not checked; `PlaySoundEffect` itself is real |
| A cut-scene brings Sable out of her hut as before: she cheers ("Excellent. You've found a Gate Stone. You'll need to search for another, now.") then looks about ("Try looking for a Gold Story Scroll in the Village.") | partial 60% | real villager, camera and text natives; the scene follows the first placement in the run of 2026-10-22, its steps not looked at one by one |
| The Lost Brother starts here, its scroll appearing in the village | partial 60% | in the run of 2026-10-22 The Lost Brother starts after the tiger stone is laid; where its scroll appears was not checked; see [the_lost_brother.md](the_lost_brother.md) |
| The camera rises looking south over the village, Sable walks back in and is removed | partial 60% | real camera and villager natives; reached after the first placement in the run of 2026-10-22, not looked at one by one |

## The second stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ape stone, earned in The Lost Brother, is placed the same way; with ape and tiger in, the scaffold sound plays and the scroll moves to 0.5 | pending | not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real); see [the_lost_brother.md](the_lost_brother.md) |
| Sable comes out cheering: "Good. You have two of the Gate Stones. But the third could be a problem.", "The Villagers say the final one was destroyed aeons ago. All is not lost, though.", "The Village sculptor can carve a new Gate Stone for you. You should see him." | partial 60% | real villager and text natives; not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The Sculptor starts here; the camera rises over the village and Sable goes back in | pending | not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real); [the_sculptor.md](the_sculptor.md) is not reached in a run yet |

## The uncarved rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the blank quarry rock is put down within 20 of the platform (not held, not flying), Sable comes out puzzled: "The stone isn't carved. Perhaps the sculptor can help?", and the camera looks south towards the sculptor | partial | the distance and flying reads are real, the held read (property 9) is real too (`src/CHLApi.cpp:1679`); not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real) |
| If the rock is still there afterwards, a reminder starts: every 3 minutes that the rock stays put and isn't picked up, the next time the platform is in view within 80, Sable comes out and says the same line again | partial | real timer and field-of-view natives; not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real) |
| Moving or picking up the rock ends the reminder, and putting it back by the platform starts it all over | partial | the same script; the held read (property 9) is real (`src/CHLApi.cpp:1679`); not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real) |
| The cow stone carved from the rock counts as the third stone when placed: the scaffold sound plays | pending | the carving is not reached in a run yet; see [the_sculptor.md](the_sculptor.md) |

## The stones are guarded

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guards start in the opening, as the family reaches the unfinished temple; the ape stone is fixed in place from then until The Lost Brother frees it | partial | `ProtectGateKeys` is started by `FollowUs` and calls only real natives (`SetIdPickupable`, `SetIdMoveable`); in the run of 2026-10-22 the guards start during the opening walk (`ProtectGateKeysFlag` 1); the stones' locks were not looked at one by one |
| A gate stone left lying (not held or flying) where the player has no influence is removed and made again at once at its home: the tiger stone's home is the dance site (only when it is more than 20 from it), the ape stone's is the soapbox by the brother's house, the blank rock's is the quarry | partial | the guards call `GetInfluence`, `CREATE` and `ObjectDelete` (real), and the held read (property 9) is real (`src/CHLApi.cpp:1679`), so a stone in the hand counts as held; not checked in game |
| A stone picked up and dropped more than 50 from its home and more than 20 from the platform goes back home a minute later (for the rock, more than 20 from the sculptor) | partial | real timers (`CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining`); the held read (property 9) is real (`src/CHLApi.cpp:1679`); not checked in game |
| A stone that no longer exists (destroyed, thrown away) is made again at its home | partial | `CREATE` of a mobile static is real; not checked in game |
| Each stone made again is lit up by a marker on it; the tiger stone is first lit up when Sable shows it, the ape stone when it is more than 100 from its home, the rock once the sculptor has asked for it | pending | the guards light the stones with `SPECIAL_EFFECT_OBJECT` (script ips 30481, 30534, 30560), which is real (`src/CHLApi.cpp:3121`); `ADD_SPOT_VISUAL_TARGET_OBJECT` is not used by these scripts; not checked in play |
| A guard stops when its stone is in the platform; the rock's guard stops when the sculptor has carved it, and a guard for the cow stone starts then | partial | plain script flags; the platform reads its stones (`ObjectInfoBits`, real); whether the guards stop is not checked in play |
| A rock left by the platform goes back to the quarry a minute after it was dropped, as the platform is more than 20 from the sculptor, so the 3-minute reminder above can hardly ever play | partial | follows from the same script; not checked in game |

## Opening the gates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With all three stones in, the game's clock is set to 15:40 and runs for the scene, the scroll moves to 0.75, and the camera is kept inside a new zone around the glade | partial | `SetGameTime`, `GameTimeOnOff` and `SetCameraZone` are real; the zone file is read (`src/Camera/PlayerCameraScript`) but the player camera does not keep to it yet ([player-camera.md](../../../../camera/player-camera.md#zones-and-fixed-rotation)); reached in the glade with the second and third SkipBox answers |
| The three young creatures are made in the glade beyond the gates: the cow, the ape and the tiger | partial | `CREATE` of a creature makes them, of no player and out of reactions (`player_creature::MakeForScript`, from the exe); not checked in game (our runs keep the old creature and skip the glade) |
| Sable comes out with a lasting "well done" and says "The Gate Stones are together. Pass through and claim your Creature!" | partial | real villager and text natives; not checked in play: the run of 2026-10-22 goes only as far as the first stone (the platform's count, `ObjectInfoBits`, is real) |
| The camera moves onto the platform; with a grinding stone sound and a 7-second camera shake (within 20, amplitude 0.1) the stones sink into the platform | pending | `ShakeCamera` and `PlaySoundEffect` are real, and `SET_OPEN_CLOSE` sets the platform's open state (`src/CHLApi.cpp:6941`, `AnimatedStaticSystem::SetOpenState`); whether the stones are drawn sinking is not checked in play |
| The camera cuts to the gate's chain, the gates open with a bolt sound, Sable beckons, and the camera watches the gates swing open with their opening sound | pending | `SET_OPEN_CLOSE` sets the gates' open state (`src/CHLApi.cpp:6941`), which the gates' and platform's collision models follow (`src/ECS/PhysicsClasses.cpp`); whether the gates are drawn swinging open is not checked in play |
| The camera flies through to the glade along a recorded path, with the third epic theme | partial | `RunCameraPath` and `StartMusic` are real; reached with the second and third SkipBox answers, not checked in game |

## The creatures show off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tiger sits, the cow walks to its place, the ape points at the camera; the cow looks confused, then cow and ape turn to face the camera | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; `CREATURE_DO_ACTION` (`src/CHLApi.cpp:3617`) forces the row as a plan by its name; the glade's rows 81 SitDown, 82 LookAtCamera, 168 PointAtCamera, 188 LookButDontApproach and 189 LookForever have executors (`src/Creature/CreaturePlanActions.cpp`), 163 PointAtObject has none, so it logs "not implemented", shows nothing and its `PLAYED` wait is true at once; whether every `PLAYED` wait of the glade passes is unverified |
| The quest is logged again with a picture of the glade, at 0.75 | pending | `UpdateSnapshot` is real |
| Both advisors come out. Good: "Look at them. Just look at them. These are Miraculous Creatures indeed." (the cow asks to be picked, the ape waves) | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far, so their part is missing; the advisor lines are real |
| Evil: "They certainly are. But not quite as big as I expected." Good: "Not yet, maybe. But they can become the most powerful Creatures in the world." (the cow prays, the tiger growls) | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| Evil: "Now that I'd like to see. We must have one. Which should we choose?" Good: "Any. They're all special." Evil: "If rather small at the moment." (each creature plays happy or "pick me" in turn) | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| The advisors go home, cow and ape wave, the camera settles on the choosing view, the music stops and the clock is stopped again | partial 60% | `StopMusic`, `GameTimeOnOff` and the camera are real; the creatures are made (`CREATE` of a creature) but the glade is not reached in a run yet |
| Sable is removed and the camera is kept to a small choosing zone | partial 80% | `ObjectDelete` and `SetCameraZone` are real; re-measured 2026-10-21: the deletion and the zone are real, and the camera keeps to it |

## Choosing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Left alone, each creature cycles through show-off moves (pointing at the camera, waving, "pick me", happy, praying, tired, impressing, "look at me", the ape laughing), one every 5 to 20 passes of the choosing loop | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| Every 5 seconds each idle creature turns to face the camera | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| With the hand right over a creature (within 1), it asks to be picked; the other two take turns looking at the camera, "look at me" and a sad face (the cow and ape cry, the tiger gets angry) | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; `GetHandPosition` is real |
| Hovering over the cow for ten passes, with nothing being said, the good advisor points at it: "We could have the cow. A strong and noble beast." and the evil advisor answers "What? Not the fierce, lethal tiger? Click the Action Button on him!" | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| Over the ape: the good advisor, pointing: "Hmm. How about the ape. Intelligent and quick to learn?" | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| Over the tiger the evil advisor should say "I'm up for the tiger. Look at those claws.", but the line waits on the ape's hover count, which is zero whenever the hand is over the tiger, so it never plays | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| A comment isn't repeated for the same creature twice running; the creatures keep showing off while a comment is read | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| Clicking a creature once has the good advisor ask "Are you sure you want this Creature? Click on him again if you are."; clicking another creature starts its count afresh | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; `GameThingClicked` and `ClearClickedObject` are real |
| Clicking the same creature a second time chooses it | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; see [../../creature/species_choice.md](../../creature/species_choice.md) |

## The choice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The chosen creature is happy; the others cry (the tiger is angry instead when the ape is chosen); one points at the chosen creature and the other eyes it without coming near | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far |
| The camera closes in on the chosen creature over 10 seconds; evil advisor: "We've chosen a Creature."; a second later the creature-chosen theme plays | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; the camera and `StartMusic` are real |
| The quest is logged as finished (1, no alignment change) and the screen fades to black over 2 seconds | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`); `SetFade` is real |
| In the dark the creature becomes the player's creature and is moved to its home by the Norse village, which becomes its home position; the other two are deleted (they don't wander off) | pending | `CreatureSetPlayer` (it makes the creature the local player's) and `SetCreatureHome` are real, and the glade's creatures are made now (`CREATE` of a creature), though in play the player's creature has so far come only from `LoadMyCreature` (the fourth SkipBox answer, `src/ECS/PlayerCreature.cpp`) |
| The camera zone widens to the fifth stage of the land and the creature is released to the game | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; `ReleaseFromScript` and `SetCameraZone` are real |
| The creature's development is started: it begins growing up from its first stage, kept at home | partial 70% | `DevFunction` 1 (the first stage, kept within 12 of its home) and `SetCreatureDevStage` are real on our player's creature (see [players-creature.md](../../../../creature/players-creature.md#the-players-creature)); the chosen creature is not yet made in play; re-measured 2026-10-21: the natives are real on our creature; the chosen creature's making is not reached in a run |
| The screen stays black until the trainer's first lesson, which fades in on the creature waking at its pen | pending | the three creatures are not yet made in play: `CREATE` of a creature makes them now (`CreateScriptObject`), but a no-skip game has not yet been run that far; see [the_creatures_learning.md](the_creatures_learning.md) |

## Skipping and keeping an old creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game that skips the opening (the "skip tutorial" choice) deletes the three stones and goes straight to the glade: a 2-second fade to black instead of Sable and the platform, the gates opening as the screen fades in, and no advisor talk | partial | `CanSkipTutorial` is real (the SkipBox, `src/Gui/SkipBox`); the stones are deleted (`ObjectDelete`), the fade and the camera path run, the gates' open state is set (`SET_OPEN_CLOSE`, `src/CHLApi.cpp:6941`; not checked in play whether they are drawn open) and the creatures are made (`CREATE` of a creature); this path is not checked in play to the choice. See [land-change-and-saving.md](../../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| A player keeping the creature of an earlier game (patch 1.1) skips the glade too: clock to 15:40, the fifth camera zone, the gates open, and the old creature is loaded at a fixed spot | partial | `IsKeepingOldCreature`, `CurrentProfileHasCreature` and `LoadMyCreature` are real (the fourth SkipBox answer loads `--creature-file` at the fixed spot), as are the clock and the zone; the gates' open state is set (`SET_OPEN_CLOSE`, `src/CHLApi.cpp:6941`); not checked in play whether they are drawn open |
| Both paths count the quest as done for the scripts waiting on it (the Explorers) | partial | the fourth answer sets the done flag; the glade path (second and third answers) has not been run to the choice |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't fail: lost, thrown away or destroyed stones are made again at home, and the platform keeps every stone placed | partial 50% | the same script runs; the platform takes and counts the stones (the first one seen in the run of 2026-10-22); the remaking of lost stones is not checked in play |
| The scenes wait for Sable to reach her spots with no time limit; if something blocked her, the cut-scene would never end (not known to happen) | partial 60% | the same script runs; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The story waits for the choice with no time limit; there is no way to leave the glade without choosing, as the camera zone holds the camera there | pending | the creatures are made now (`CREATE` of a creature); the glade path is not checked in play |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature guide's theme under Sable's first talk, the third epic theme for the gates and the glade, the creature-chosen theme for the choice | partial 60% | `StartMusic` and `StopMusic` are real; only the glade's theme is reached (second and third SkipBox answers); re-measured 2026-10-21: the music natives are real; only the glade's theme is reached |
| The scaffold-planting sound when a stone goes in; the stone-grinding, bolt and gate-opening sounds as the gates open | pending | `PlaySoundEffect` is real; the first placement is reached in the run of 2026-10-22, but these sounds were not checked |

## Saves, other versions and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps which stones are in, the stones' guards and the chosen creature | pending | openblack has no saved games |
