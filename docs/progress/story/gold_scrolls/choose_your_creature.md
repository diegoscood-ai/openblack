# Choose Your Creature

The first land's first creature chapter: Sable, the creature trainer, challenges the player to open the gates of the
creatures' glade with three gate stones; the first is the stone the villagers danced round, the second is earned in
[The Lost Brother](the_lost_brother.md), the third is carved by [The Sculptor](the_sculptor.md). With all three on the
platform the gates open, the cow, the ape and the tiger show off, and the player picks one by clicking it twice.

**Land:** 1 · **Giver:** Sable, the creature trainer, at the creature gates north of the Norse village · **Script:** ChooseYourCreature (with CreaturesInGlade and the gate-stone guards in ProtectGateKeys) · **Reward:** the player's creature (cow, ape or tiger); no alignment change · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table, the land file `Land1.txt` and the executable (how a gate stone fits into the
platform). openblack's state is judged on this tree: the land's control script reaches this quest after the opening and
the temple scene, which are not checked in game (see [../../scripts/land1_script.md](../../scripts/land1_script.md)); 13
of the 110 commands it and the scripts it starts need only log "not implemented" in `src/CHLApi.cpp`, and its gate-stone
loop never ends (`ObjectInfoBits` is a stub), so no creature is chosen; every row is todo unless the notes say
otherwise. How a creature is chosen and what the species differ in:
[../../creature/species_choice.md](../../creature/species_choice.md).

**Progress: 1/90 done, 47 partial — 27%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold scroll (a story highlight), logged as "Choose Your Creature" | partial | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); the log pictures `Snapshot` and `UpdateSnapshot` are stubs and the challenge log is not drawn |
| It starts once the opening is over: after the family has led the player home and the temple scene is done, and after the Throwing Stones and Lost Flock silver scrolls have been set going | partial | `LandControl1` runs it after `FollowUs` and `CitadelGuide`; reached only after the rest of the intro and the temple scene, which are not checked in game. See [../land_1.md](../land_1.md), [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md) |
| The land's control script waits for the whole quest (all three stones and the choice) before going on | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub), so `LandControl1` never goes on |
| What it unlocks next: the creature breeder, the Explorers and the guide's wandering are started, then the trainer's first lesson (seeing the creature's home pen) begins | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub). See [the_creatures_learning.md](the_creatures_learning.md); [../creature_guide.md](../creature_guide.md); [../silver_scrolls/the_creature_breeder.md](../silver_scrolls/the_creature_breeder.md); [../silver_scrolls/the_explorers.md](../silver_scrolls/the_explorers.md) |
| The Lost Brother is started by this quest when the first stone is placed, and The Sculptor when the second is | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub); [the_lost_brother.md](the_lost_brother.md) and [the_sculptor.md](the_sculptor.md) are never started |
| The scroll's progress mark and reminder change at every step: 0 at the start (reminder "Didn't we see the Villagers dancing around a Stone a while ago?"), 0.25 with one stone ("We need to find another Gate Stone. Perhaps the Villagers can help."), 0.5 with two ("We should speak to the sculptor about the Gate Stones."), 0.75 in the glade and 1 when a creature is chosen ("We need to choose a Creature!") | todo | the progress marks are set through `UpdateSnapshot`, a stub; the challenge log is not drawn |
| Each reminder is said by whichever advisor owns the line (all of these are the good advisor's), stepping out to say it | partial | the shared reminder calls only real natives (`SpiritEject`, `RunText`); the reminder texts it would read come from the stubbed log |

## The scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze did-you-know scroll is placed beside the gate stone platform: "Every time you activate a Gold Story Scroll you start the next chapter of the story of Black and White. How quickly you progress depends on how often you choose to activate them." | partial | `CreateHighlight` and `HighlightProperties` are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| A gold scroll appears in front of the creature gates, raised 14 above the ground | partial | `CreateHighlight` and the height through `SetProperty` (YPos) are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Whenever the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points at it and says "There's a Gold Story Scroll down here, Boss.", at most once every 30 seconds and only when no cut-scene is running | partial | the shared notify script `ChallengeHighlightNotify` calls only real natives (`SpiritPointPos`, `RunText`); reached only after the rest of the intro and the temple scene, which are not checked in game |
| The nagging stops when the scroll (or the place it stands) is clicked | partial | `GameThingClicked` is real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Nothing ever removes the gold scroll in this script once it is clicked (undetermined: whether the game hides a story scroll by itself once its chapter is logged as finished) | partial | the same script runs, so nothing removes it either; reached only after the rest of the intro and the temple scene, which are not checked in game |

## The trainer's challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cut-scene with the creature guide's theme; the good advisor is sent home | partial | `StartMusic` (`src/Audio/Services/GameMusic`), `StartCameraControl` and `SpiritHome` are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Sable, the trainer, is made inside the hut by the gates, walks out of its door to a spot before the camera and turns to face it, the camera gliding down to her and following her | partial | `CREATE` makes villagers, `MoveGameThing` and `SetFocus` work on villagers, `MoveCameraPosition` and `FocusFollow` are real (`src/Camera/ScriptCamera`); reached only after the rest of the intro and the temple scene, which are not checked in game |
| The quest is logged in the story's log at this point, with a picture, at 0 progress | todo | `Snapshot` is a stub; the challenge log is not drawn |
| Sable bows and says "Greetings, Holy One. You've activated a Gold Story Scroll." then "My name is Sable. I am a trainer of Creatures." | partial | `RunText`, `TextRead` and the villager's animation (`OverrideStateAnimation`) are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Gossiping: "I challenge you to open these gates.", "Behind them are three wondrous Creatures.", "Every god has a Creature.", "They can grow as tall as a mountain and can perform Miracles." | partial | real text natives; reached only after the rest of the intro and the temple scene, which are not checked in game |
| "But you can't open the Gate unless you find the three Gate Stones." then "Each Stone you find must be placed on this platform.": the camera swings round to show the empty platform for a few seconds and comes back to her | partial | real camera and text natives; reached only after the rest of the intro and the temple scene, which are not checked in game |
| "The Villagers were dancing around a Gate Stone when you arrived.": the camera rises over the land and flies down to the tiger gate stone at the end of the ravine the family led the player through, which is lit up | partial | real camera natives; the stone's guard lights it (below); reached only after the rest of the intro and the temple scene, which are not checked in game |
| Over the stone: "Once you place this Gate Stone on the platform, I will tell you of the other two." | partial | `RunText` is real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| A quick fade to black and back to Sable: "Now retrieve the first Gate Stone, Holy One." | partial | `SetFade`, `SetFadeIn`, `FadeFinished` and the camera and text natives are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| The camera rises high over the hut, looking south over the village, the music stops and the cut-scene ends | partial | real camera natives and `StopMusic`; reached only after the rest of the intro and the temple scene, which are not checked in game |
| The tiger stone can now be picked up and moved (it was fixed in place since the family's welcome dance) | partial | `SetIdPickupable` and `SetIdMoveable` are real; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Sable walks back to her door and is removed 3 seconds later | partial | `MoveGameThing` and `ObjectDelete` work on villagers; reached only after the rest of the intro and the temple scene, which are not checked in game |

## Finding the first stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three stones are part of the land: the tiger stone where the villagers danced, the ape stone by the lost brother's house and a blank rock in the quarry, with the gate stone platform and the gates by the trainer's hut | done | `Land1.txt` places them (`MobileStaticArchetype`, `AnimatedStaticArchetype`); see [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| A gate stone is dropped from the hand onto the platform to place it: the stone disappears and the platform shows it in one of its three sockets | todo | nothing fills the platform in our tree (its socket state stays 0) |
| The quest knows which stones are in by what the platform holds: the ape stone counts 1, the tiger 2 and the cow 4, so 3 means ape and tiger and 7 all three; the blank rock counts nothing | todo | `ObjectInfoBits` is a stub that always answers 0, so the loop never ends |
| A stone once in the platform stays in it; the player can't take it out again | todo | nothing fills the platform |
| When the camera is within 100 of the tiger stone and it is on screen, the good advisor points at it: "Here's the stone the people were dancing around! Pick it up by clicking the Action Button on it.", at most every 30 seconds, until the player first picks it up | partial | `AdvisorSpotsGateKey2` calls only real natives (field of view, `SpiritPointPos`, `RunText`); the held read (property 9) is not handled by `GetProperty`; reached only after the rest of the intro and the temple scene, which are not checked in game |
| The first time the player hits the gates (for instance throwing something at them), the good advisor says "I sense that battering the Gates won't work, esteemed one."; only once | todo | `GameThingHit` and `ClearHitObject` are stubs |
| Clues while the platform is empty: 2 minutes after the cut-scene the good advisor says "Didn't we see the Villagers dancing around a Stone a while ago?" | partial | `SetTimerTime` and the timer reads are real (`src/ECS/ScriptTimer`); the platform always reads empty, so the clues would keep coming; reached only after the rest of the intro and the temple scene, which are not checked in game |
| Every 2 minutes after that, still empty, the good advisor points at the stone and says "The Stone's at the end of the ravine we followed the Villagers through!" if it is within 50 of where they danced, or the first clue again if it has been moved | partial | real timer, distance and advisor natives; reached only after the rest of the intro and the temple scene, which are not checked in game |
| From the second clue on the scroll's reminder becomes the ravine line, even when the advisor actually said the first clue | partial | the same script runs; the reminder is written through the stubbed log |
| The clues stop for good once any stone is in the platform | todo | the platform never reads a stone, so the clues never stop |

## The first stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placing the tiger stone plays a scaffold-planting sound and moves the scroll to 0.25 | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub); `PlaySoundEffect` itself is real |
| A cut-scene brings Sable out of her hut as before: she cheers ("Excellent. You've found a Gate Stone. You'll need to search for another, now.") then looks about ("Try looking for a Gold Story Scroll in the Village.") | partial | real villager, camera and text natives; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| The Lost Brother starts here, its scroll appearing in the village | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub); [the_lost_brother.md](the_lost_brother.md) is never started |
| The camera rises looking south over the village, Sable walks back in and is removed | partial | real camera and villager natives; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |

## The second stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ape stone, earned in The Lost Brother, is placed the same way; with ape and tiger in, the scaffold sound plays and the scroll moves to 0.5 | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub); see [the_lost_brother.md](the_lost_brother.md) |
| Sable comes out cheering: "Good. You have two of the Gate Stones. But the third could be a problem.", "The Villagers say the final one was destroyed aeons ago. All is not lost, though.", "The Village sculptor can carve a new Gate Stone for you. You should see him." | partial | real villager and text natives; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| The Sculptor starts here; the camera rises over the village and Sable goes back in | todo | blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub); [the_sculptor.md](the_sculptor.md) is never started |

## The uncarved rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the blank quarry rock is put down within 20 of the platform (not held, not flying), Sable comes out puzzled: "The stone isn't carved. Perhaps the sculptor can help?", and the camera looks south towards the sculptor | partial | the distance and flying reads are real, the held read (property 9) is not; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| If the rock is still there afterwards, a reminder starts: every 3 minutes that the rock stays put and isn't picked up, the next time the platform is in view within 80, Sable comes out and says the same line again | partial | real timer and field-of-view natives; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| Moving or picking up the rock ends the reminder, and putting it back by the platform starts it all over | partial | the same script; the held read (property 9) is not handled; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| The cow stone carved from the rock counts as the third stone when placed: the scaffold sound plays | todo | the carving is never reached; see [the_sculptor.md](the_sculptor.md) |

## The stones are guarded

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guards start in the opening, as the family reaches the unfinished temple; the ape stone is fixed in place from then until The Lost Brother frees it | partial | `ProtectGateKeys` is started by `FollowUs` and calls only real natives (`SetIdPickupable`, `SetIdMoveable`); not checked in game past the intro's hand-over |
| A gate stone left lying (not held or flying) where the player has no influence is removed and made again at once at its home: the tiger stone's home is the dance site (only when it is more than 20 from it), the ape stone's is the soapbox by the brother's house, the blank rock's is the quarry | partial | the guards call `GetInfluence`, `CREATE` and `ObjectDelete` (real), but the held read (property 9) is not handled by `GetProperty`, so a stone always counts as not held; not checked in game |
| A stone picked up and dropped more than 50 from its home and more than 20 from the platform goes back home a minute later (for the rock, more than 20 from the sculptor) | partial | real timers (`CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining`); the held read (property 9) is not handled; not checked in game |
| A stone that no longer exists (destroyed, thrown away) is made again at its home | partial | `CREATE` of a mobile static is real; not checked in game |
| Each stone made again is lit up by a marker on it; the tiger stone is first lit up when Sable shows it, the ape stone when it is more than 100 from its home, the rock once the sculptor has asked for it | todo | `AddSpotVisualTargetObject` is a stub |
| A guard stops when its stone is in the platform; the rock's guard stops when the sculptor has carved it, and a guard for the cow stone starts then | partial | plain script flags; the platform never reads a stone, so the first two guards never stop |
| A rock left by the platform goes back to the quarry a minute after it was dropped, as the platform is more than 20 from the sculptor, so the 3-minute reminder above can hardly ever play | partial | follows from the same script; not checked in game |

## Opening the gates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With all three stones in, the game's clock is set to 15:40 and runs for the scene, the scroll moves to 0.75, and the camera is kept inside a new zone around the glade | partial | `SetGameTime`, `GameTimeOnOff` and `SetCameraZone` are real; the zone file is read (`src/Camera/PlayerCameraScript`) but the player camera does not keep to it yet ([../../../bw1-notes/script-camera.md](../../../bw1-notes/script-camera.md#zones-and-fixed-rotation)); reached in the glade with the second and third SkipBox answers |
| The three young creatures are made in the glade beyond the gates: the cow, the ape and the tiger | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Sable comes out with a lasting "well done" and says "The Gate Stones are together. Pass through and claim your Creature!" | partial | real villager and text natives; blocked: the quest waits for ever in its gate-stone loop (`ObjectInfoBits` is a stub) |
| The camera moves onto the platform; with a grinding stone sound and a 7-second camera shake (within 20, amplitude 0.1) the stones sink into the platform | todo | `ShakeCamera` and `PlaySoundEffect` are real, but sinking the stones (`SetOpenClose`) is a stub |
| The camera cuts to the gate's chain, the gates open with a bolt sound, Sable beckons, and the camera watches the gates swing open with their opening sound | todo | `SetOpenClose` is a stub; the gates' and platform's collision models follow an open state (`src/ECS/PhysicsClasses.cpp`) but nothing opens them |
| The camera flies through to the glade along a recorded path, with the third epic theme | partial | `RunCameraPath` and `StartMusic` are real; reached with the second and third SkipBox answers, not checked in game |

## The creatures show off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tiger sits, the cow walks to its place, the ape points at the camera; the cow looks confused, then cow and ape turn to face the camera | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); `CreatureDoAction` is a stub |
| The quest is logged again with a picture of the glade, at 0.75 | todo | `UpdateSnapshot` is a stub |
| Both advisors come out. Good: "Look at them. Just look at them. These are Miraculous Creatures indeed." (the cow asks to be picked, the ape waves) | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`), so their part is missing; the advisor lines are real |
| Evil: "They certainly are. But not quite as big as I expected." Good: "Not yet, maybe. But they can become the most powerful Creatures in the world." (the cow prays, the tiger growls) | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Evil: "Now that I'd like to see. We must have one. Which should we choose?" Good: "Any. They're all special." Evil: "If rather small at the moment." (each creature plays happy or "pick me" in turn) | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The advisors go home, cow and ape wave, the camera settles on the choosing view, the music stops and the clock is stopped again | partial | `StopMusic`, `GameTimeOnOff` and the camera are real; the creatures are not made |
| Sable is removed and the camera is kept to a small choosing zone | partial | `ObjectDelete` and `SetCameraZone` are real; the player camera does not keep to the zone yet |

## Choosing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Left alone, each creature cycles through show-off moves (pointing at the camera, waving, "pick me", happy, praying, tired, impressing, "look at me", the ape laughing), one every 5 to 20 passes of the choosing loop | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Every 5 seconds each idle creature turns to face the camera | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| With the hand right over a creature (within 1), it asks to be picked; the other two take turns looking at the camera, "look at me" and a sad face (the cow and ape cry, the tiger gets angry) | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); `GetHandPosition` is real |
| Hovering over the cow for ten passes, with nothing being said, the good advisor points at it: "We could have the cow. A strong and noble beast." and the evil advisor answers "What? Not the fierce, lethal tiger? Click the Action Button on him!" | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Over the ape: the good advisor, pointing: "Hmm. How about the ape. Intelligent and quick to learn?" | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Over the tiger the evil advisor should say "I'm up for the tiger. Look at those claws.", but the line waits on the ape's hover count, which is zero whenever the hand is over the tiger, so it never plays | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| A comment isn't repeated for the same creature twice running; the creatures keep showing off while a comment is read | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Clicking a creature once has the good advisor ask "Are you sure you want this Creature? Click on him again if you are."; clicking another creature starts its count afresh | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); `GameThingClicked` and `ClearClickedObject` are real. With the second and third SkipBox answers the glade waits for a click on a creature that does not exist |
| Clicking the same creature a second time chooses it | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); see [../../creature/species_choice.md](../../creature/species_choice.md) |

## The choice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The chosen creature is happy; the others cry (the tiger is angry instead when the ape is chosen); one points at the chosen creature and the other eyes it without coming near | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The camera closes in on the chosen creature over 10 seconds; evil advisor: "We've chosen a Creature."; a second later the creature-chosen theme plays | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); the camera and `StartMusic` are real |
| The quest is logged as finished (1, no alignment change) and the screen fades to black over 2 seconds | todo | the log is a stub; `SetFade` is real |
| In the dark the creature becomes the player's creature and is moved to its home by the Norse village, which becomes its home position; the other two are deleted (they don't wander off) | todo | `CreatureSetPlayer` is a stub; `SetCreatureHome` is real. Our player's creature comes only from `LoadMyCreature` (the fourth SkipBox answer, `src/ECS/PlayerCreature.cpp`) |
| The camera zone widens to the fifth stage of the land and the creature is released to the game | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); `ReleaseFromScript` and `SetCameraZone` are real |
| The creature's development is started: it begins growing up from its first stage, kept at home | partial | `DevFunction` and `SetCreatureDevStage` are real on our player's creature (see [../../../bw1-notes/creature.md](../../../bw1-notes/creature.md#the-players-creature)); the chosen creature is never made |
| The screen stays black until the trainer's first lesson, which fades in on the creature waking at its pen | todo | the three creatures are never made: `CREATE` of a creature is not done (`CreateScriptObject`); see [the_creatures_learning.md](the_creatures_learning.md) |

## Skipping and keeping an old creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game that skips the opening (the "skip tutorial" choice) deletes the three stones and goes straight to the glade: a 2-second fade to black instead of Sable and the platform, the gates opening as the screen fades in, and no advisor talk | partial | `CanSkipTutorial` is real (the SkipBox, `src/Gui/SkipBox`); the stones are deleted (`ObjectDelete`), the fade and the camera path run, but the gates (`SetOpenClose`) stay shut and the creatures are not made. See [../../../bw1-notes/map-loading.md](../../../bw1-notes/map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| A player keeping the creature of an earlier game (patch 1.1) skips the glade too: clock to 15:40, the fifth camera zone, the gates open, and the old creature is loaded at a fixed spot | partial | `IsKeepingOldCreature`, `CurrentProfileHasCreature` and `LoadMyCreature` are real (the fourth SkipBox answer loads `--creature-file` at the fixed spot), as are the clock and the zone; the gates (`SetOpenClose`) stay shut |
| Both paths count the quest as done for the scripts waiting on it (the Explorers) | partial | the fourth answer sets the done flag; the glade path (second and third answers) never finishes because the creatures are not made |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't fail: lost, thrown away or destroyed stones are made again at home, and the platform keeps every stone placed | partial | the same script runs; the platform never takes a stone, so the quest cannot progress either |
| The scenes wait for Sable to reach her spots with no time limit; if something blocked her, the cut-scene would never end (not known to happen) | partial | the same script runs; not checked in game |
| The story waits for the choice with no time limit; there is no way to leave the glade without choosing, as the camera zone holds the camera there | todo | the creatures are never made, so the glade path waits for ever |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature guide's theme under Sable's first talk, the third epic theme for the gates and the glade, the creature-chosen theme for the choice | partial | `StartMusic` and `StopMusic` are real; only the glade's theme is reached (second and third SkipBox answers) |
| The scaffold-planting sound when a stone goes in; the stone-grinding, bolt and gate-opening sounds as the gates open | todo | `PlaySoundEffect` is real, but these sounds come after the stones go in, which never happens |

## Saves, other versions and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps which stones are in, the stones' guards and the chosen creature | todo | openblack has no saved games |
| The cow's look-at point in the opening of the glade has a typing slip (a coordinate ten times too far north), so it stares far off past the glade | n/a | a data quirk in the script itself; our tree runs the script as shipped |
| A cut placement of the ape stone first (and its line "Right. We need another Gate Stone…", missing from the text table) is commented out of the script | n/a | not in the program |
| An older, unshipped version of the quest (a good-advisor "Shh." line and a swap-style "are you sure" line) and a bare test script ("Choose one of these mothers") | n/a | not in the program |
| A cut silver scroll version of the sculptor's part | n/a | see [../silver_scrolls/the_sculptor_cut_silver.md](../silver_scrolls/the_sculptor_cut_silver.md) |
