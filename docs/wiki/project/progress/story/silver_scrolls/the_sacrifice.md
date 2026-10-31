# The Sacrifice

A second-land silver scroll: an Indian priest, his wife and their small son come to the player's new worship site, where
the priest offers his firstborn to the altar. Anything sacrificed there counts towards a total of 4000 of prayer power;
who (or what) the player gives up, or spares, sets the outcome's alignment. Reaching 4000 with the boy alive earns a
heal miracle reward.

**Land:** 2 · **Giver:** an Indian priest and priestess from the Indian village the control script calls Town6 (town id 11), at the player's worship site · **Script:** Sacrifice · **Reward:** a heal miracle chest from the sky (only for reaching 4000 without killing the family) · **Repeatable:** no

Sources: the quest's script source (`Sacrifice.txt`, with the land control script `LandControl2.txt` and the shared
helpers `ChallengeNotify.txt`, `StandardReminder.txt`, `Reward.txt`), checked line by line against the PC game's
compiled `challenge.chl`; the game's text table; the land file `Land2.txt` for which town sits where; and the executable
for what reading a worship site's sacrifice total returns. openblack is judged on this tree: of the 60 script functions
the quest and the scripts it runs need, none is a stub in `src/CHLApi.cpp` at this tree (the sacrifice-total,
held-object, snapshot and reward commands are real); creating villagers works (`Create`). The land's control
script never runs (see [../land_2.md](../land_2.md) and [../../scripts/land2_script.md](../../scripts/land2_script.md)),
so none of it happens; rows are pending unless the notes say otherwise. The script's function coverage is in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md); how sacrifice gives prayer power in
[../../worship/prayer_power.md](../../worship/prayer_power.md).

**Progress: 0/66 done, 0 chosen, 5 partial, 0 inferred — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher for the quest as soon as the land is set up | pending | `LandControl2` has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)) |
| Every 11 seconds the watcher looks for a worship site within 15 of a fixed altar spot by the player's home village (the site the player is taught to build in this land; the plague's watcher uses the same spot) | pending | the finder and the built test work (`Call`, `GetProperty` built percentage in `src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest starts only once that worship site is fully built (built exactly 1) and the Indian village with town id 11 belongs to the player; the village is checked only after the site is built, and the watcher then stops | pending | reading the village's owner (`GetProperty` player) is real (`src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scroll is a silver scroll (a challenge highlight) beside a house about 100 north of that Indian village's centre, far west of the altar | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Clicking it makes the scroll active (opened) and the quest begins at once; there is no time limit on clicking it | pending | `GameThingClicked` and `SetActive` on a highlight work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Only on the click are the family made at the house: a 6-year-old Indian boy (an Indian farmer villager made a child by age), a priest and a priestess, all three joining the Indian village | pending | `Create` makes villagers and children, `FlockAttach` works; setting the age works for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After the introduction the scroll is moved to beside the altar (about 20 from it) and stays there; the script never removes it | pending | `SetPosition` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene under the Gregorian chant script theme | partial 60% | `SetWidescreen`, `StartMusic` work (`src/Audio`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Brings the black cinema bars in or out", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| The three are drawn in high detail; the priest and priestess walk a few paces from the house while the boy shuffles along very slowly (speed 0.1) | pending | `SetHighGraphicsDetail`, `MoveGameThing` and the speed property work for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera glides in on them over 5 seconds, then closer over 6 | pending | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After 5 seconds the priest, gossiping, says "Greetings, almighty presence." while the boy stands scared stiff and the priestess idles | pending | `RunText` and the villagers' script animations (`SetScriptState`, `SetScriptUlong`) work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera eases round; the priest: "As a gesture of gratitude from our people, we have a gift for you." and the dialogue closes | pending | `RunText`, `TextRead`, `GameCloseDialogue` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The boy (now at speed 0.2) and his parents walk off to a spot about 50 south; the camera pulls back and up over about 8 seconds, rising to 80 high | pending | `MoveGameThing` and the camera moves work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black over 2 seconds; the camera is cut to the player's altar and the family is put down beside it, each then taking one step | partial 60% | `SetFade`, `SetCameraPosition`, `SetCameraFocus`, `SetPosition` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", "Fades the screen to a colour over a time", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Moves an object to a position at once", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| The screen fades back in over 2 seconds; the parents face the camera, and the boy mourns, looking away towards one spot | pending | `SetFadeIn` and `SetFocus` on villagers work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera drifts over 8 seconds; the priest: "We are a spiritual tribe, with ancient customs." | pending | the camera moves and `RunText` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest is recorded: title "The Sacrifice", 0 done, alignment 0, reminder "These people await a sacrifice from you." (evil advisor), with the current view as its picture. The snapshot records the challenge in the player's challenge log with its title and reminder | pending | `Snapshot` is real; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| The parents turn to the boy; the priest: "If you place anything on this altar, its life-force is added to our stored prayer power." | pending | `SetFocus` on villagers and `RunText` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The priest: "As a gesture of my belief, I offer my firstborn son to the altar." (waits for the player to click on) | pending | `RunText` with interaction works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The text clears and the camera pulls back over 3 seconds to take in the altar | pending | `GameClearDialogue` and the camera moves work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Both advisors pop out. Evil: "Now that's what I call a gesture of belief!" The evil advisor then points at the altar while the good advisor answers: "But human sacrifice is wrong! Why not sacrifice something else? Like a plant." (waits for a click) | pending | `SpiritEject`, `SpiritPointPos`, `RunText` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The family go back to normal detail, the music stops and the scene ends | partial 60% | `SetHighGraphicsDetail`, `StopMusic`, `EndDialogue` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Draws an object in full detail however far away it is", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Stops the script's music", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Lets go of the dialogue box", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must sacrifice 4000 worth of prayer power at the worship site, by whatever means, or choose to sacrifice or kill one of the family | pending | the altar takes what the hand gives it and adds its prayer power to the site's sacrifice total (`ecs::altar_offering`, `altar_offering::Offer` in `src/ECS/AltarOffering.cpp`, offered from `HandSystem::ApplyHeldToObject` in `src/ECS/Systems/Implementations/HandApplyToObject.cpp`); the quest is not reached in play |
| What counts is the site's own running sacrifice total, read once after the introduction and then continually; only what is added after the introduction counts | pending | `GetSacrificeTotal` is real and reads the site's own total (`WorshipSite::sacrificed`, raised by the hand's offerings, `altar_offering::Offer` in `src/ECS/AltarOffering.cpp`; test `WorshipTest.AScriptReadsTheSacrificeTotalOfASiteAndNothingOfAnythingElse`); the quest is not reached in play |
| The script only reads the total, so anything that adds to it counts (trees, animals, villagers, the boy himself); it does not look at what was sacrificed | pending | the site's sacrifice total is kept (`WorshipSite::sacrificed`); the quest is not reached in play |
| There is no time limit and nothing ends the quest except one of the outcomes below | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Rules while it runs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the boy is alive he cries every 4 seconds, a random one of eleven child-crying samples played at him, until the quest ends | partial 60% | timers (`CreateTimer`, `GetTimerTimeRemaining`) and `PlaySoundEffect` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Creates a timer running for a number of seconds", "Gives the time a timer has left", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| Whenever the total rises, after 2 seconds the priest (if alive) judges the offering by how much it added: under 700 "Hmm. That didn't produce much prayer power."; 700 to 2000 "Yes. Feel the power."; over 2000 "That's a powerful sacrifice. A whole heap of prayer power has been stored from it." | pending | the judged amount comes from `GetSacrificeTotal`, real (`src/Magic/Script/CHLWorship.cpp`); the quest is not reached in play |
| The first time an offering adds under 700, the evil advisor first grumbles "Bah, humbug!" (once per quest) | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| If the priest is dead when the total rises, only the evil advisor's "Bah, humbug!" is said, and only if not said before | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| Offerings made during the 2-second pause and the priest's line are added together and judged as one next time round | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| The scroll's progress is the amount sacrificed since the start divided by 4000, updated continually with alignment 0 and the same title and reminder; it is capped at 1 once 4000 is reached | pending | `UpdateSnapshot` is real; undetermined how an update with alignment 0 interacts with the 0.5 recorded when the boy is let go |
| Each time round, if no ending has happened, the script notes which of the three (if any) the player is holding in the hand; this is how it later tells "sacrificed" from "killed" | pending | `GetObjectHeld199` is real (`src/CHLApi.cpp`); the quest is not reached in play |
| An ending counts as a sacrifice when that person no longer exists and was the one in the hand the last time the script looked; a person who dies any other way (leaving a body) counts as killed | pending | `GetObjectHeld199` and the altar sacrifice are real; not reached in play; undetermined: whether the altar removes a villager straight from the hand, which this test relies on |
| The endings are checked in a fixed order: the priest first, then the priestess, then the boy; only if all three live are the boy's release and the 4000 total looked at | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each death ending is a short scene: the camera is cut to the altar view, the line is said (waiting for a click), the record is closed fully done, and the camera is put back where it was | pending | `SetCameraPosition`, `SetCameraFocus` and `Snapshot` are real; the quest is not reached in play |
| The priest sacrificed: the boy blows a raspberry at his mother, who looks at the camera unimpressed: "It's what he would have wanted." Alignment +1 | pending | the altar sacrifice and `Snapshot` are real; the quest is not reached in play |
| The priest killed any other way: the same gestures, the priestess: "You killed my husband! But he was a good priest!" Alignment +0.5 | pending | `GetObjectHeld199` and `Snapshot` are real; the quest is not reached in play |
| The priestess sacrificed: the boy blows a raspberry at his father, who looks at the camera unimpressed: "Ah, so you thought my wife'd make a better sacrifice. Interesting," Alignment 0 | pending | the altar sacrifice is real; the quest is not reached in play |
| The priestess killed any other way: the priest, unimpressed: "My wife! You do work in mysterious ways, don't you?" Alignment -0.2 | pending | `GetObjectHeld199` and `Snapshot` are real; the quest is not reached in play |
| The boy sacrificed: the priest looks at the camera impressed, the priestess unimpressed: "Excellent. I knew sacrificing my lad would provide a huge power-boost." Alignment -1 | pending | the altar sacrifice is real; the quest is not reached in play |
| The boy killed any other way: the priest, defeated: "Er, you were supposed to sacrifice him on the altar. Such a waste." Alignment -1 | pending | `GetObjectHeld199` and `Snapshot` are real; the quest is not reached in play |
| None of the death endings gives a reward, and the quest is over after any of them | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Reaching 4000 with all three alive: the camera glides to the altar over 4 seconds; after 2 the boy blows a raspberry at his father | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| The priest faces the camera: "The power from that sacrifice will remain stored until you need it." while he prays and the priestess cheers; the boy turns to the camera and does a mocking dance | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| The priest: "You allowed my son to live. I bow to your will." (waits for a click). The record is closed fully done with alignment +1 | pending | the sacrifice total and `Snapshot` are real; the quest is not reached in play |
| The reward then follows in its own scene (below) | pending | `CreateRewardInTown` is real |

## Letting the boy go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the boy is more than 100 from the altar and is neither in the hand nor flying (the player has carried him off and put him down), a scene cuts to the altar view and the priest, waiting impatiently: "We are awaiting a sacrifice." (waits for a click) | pending | the in-hand and flying properties are real (`GetProperty`, `src/CHLApi.cpp`); the quest is not reached in play |
| The record is updated: 0 done, alignment +0.5; the boy is let go from the script to live as an ordinary villager; this happens only once | pending | `ReleaseFromScript` works; `UpdateSnapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Unlike the endings, this scene does not put the camera back where it was; it is left on the altar view | pending | the camera natives work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Letting him go does not end the quest: the 4000 total is still asked for, and the boy's later death still ends it with his "killed" (or "sacrificed") line and alignment -1 | pending | the sacrifice total is real (`GetSacrificeTotal`); the quest is not reached in play |
| The parents are never let go by the script in any outcome; only the released boy goes back to normal village life | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the 4000 ending gives one: a chest holding one heal miracle (a plain one-shot, not a power-up), dropped from the sky at a spot about 20 in front of the altar | pending | `CreateRewardInTown` is real; see [../rewards.md](../rewards.md) |
| The chest is given "in" the Indian village whose centre is at the given spot (town id 12, the one the control script calls Town1, the nearest village), not the sacrificing family's village | pending | `CreateRewardInTown` is real |
| What the chest holds follows the reward rules: a town that can't yet cast heal learns it; if the player already has heal, the next heal power-up the player lacks is given (to that village) | pending | the reward chest is real, not yet checked in this quest; [../rewards.md](../rewards.md) "Miracle rewards" |
| The reward scene picks one of two camera shots at random (high, 80 up and 60 back, or low, 15 up and 15 back), glides in over 4 seconds, looks up for the falling chest, then follows it down from 25 up and 30 back | pending | the shared reward-from-sky script: the camera moves work and the chest is made (`CreateRewardInTown`), not yet checked in this quest |
| Clicking the chest says its help line, if the dialogue is free (the first reward of the game would get the special first-reward lines instead, which can't happen by this land) | pending | `GetHelp` is real (`src/CHLApi.cpp`), but there is no chest |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The prayer power given stays in the site's store like any other sacrifice | pending | the site stores the offering's prayer power (`altar_offering::Offer` in `src/ECS/AltarOffering.cpp`); the quest is not reached in play; see [../../worship/prayer_power.md](../../worship/prayer_power.md) |
| On finishing, the script sets a land flag marking the sacrifice as completed; nothing in the shipped scripts reads it | pending | the flag is declared by the land's set-up script; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nothing else in the land's story depends on the outcome | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The priest's lines are the "man" narrator voice, the priestess's the "woman" voice; the advisors use their own voices | pending | the voices are read past; see ../../scripts/info_scripts.md |
| Clicking the active scroll again speaks its reminder, "These people await a sacrifice from you.", through the evil advisor | pending | the shared reminder script's natives (`SpiritEject`, `RunText`) work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Music: the Gregorian chant script theme for the introduction only; the endings have none | partial 60% | `StartMusic` and `StopMusic` play and stop it; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", "Stops the script's music", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script never mentions the creature; whatever the creature sacrifices at the site adds to the same total, and a family member it kills counts as "killed" (it was not in the player's hand) | pending | the site's total is kept (`WorshipSite::sacrificed`); the creature's own sacrifice action only puts the thing down by the altar, the original's bug kept (`src/Creature/CreatureSacrifice.cpp`, [plans and needs](../../../../creature/plans-and-needs.md)); the quest is not reached in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the record line in the introduction the script waits for a line to be read though none is showing | pending | `TextRead` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the family, the total read at the start and the record | pending | openblack's saved games are not reachable in play: only the debug bar's load, which makes the forests and trees (`Game::LoadSavedGame`, [saved games](../../../../engine/saved-games.md#openblack)) |
