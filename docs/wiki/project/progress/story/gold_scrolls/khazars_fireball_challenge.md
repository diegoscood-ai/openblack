# Khazar's Fireball Challenge

A land 2 gold scroll and Khazar's lesson in attack: three empty huts on the shore are the targets, three fireball
seeds are laid out a long throw away, and however well (or badly) the player throws them, Khazar ends by giving the
Fireball miracle to the player's village. This file also covers the "Miracle Challenge" scroll that Khazar leaves by
the worship site after the [Worship Site](worship_site.md) lesson, which starts this challenge and
[Khazar's Shield Challenge](khazars_shield_challenge.md) together. The land as a whole is in
[../land_2.md](../land_2.md), the land's control script in [../../scripts/land2_script.md](../../scripts/land2_script.md),
the script program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), the miracle itself in
[../../miracles/fireball.md](../../miracles/fireball.md).

**Land:** 2 · **Giver:** Khazar (the friendly god), through a scroll by the player's worship site and then a scroll on the target huts · **Script:** MiracleChallenges, FireBallChallenge · **Reward:** the Fireball miracle (first level) at the player's home village's worship site, given whatever the result · **Repeatable:** no

Sources: the quest's script source (`Land2FireballChallenge.txt`), the land's scroll-notify helper (`SetupLand2.txt`),
the hand demo (`HandDemos.txt`), the lesson that places the first scroll (`LearnWorshipping.txt`) and the land's control
script, checked against the compiled `challenge.chl`; the game's English text table; and the land's map script
(`Scripts/Land2.txt`) for the huts. openblack's state is judged on this tree: of the 61 script functions the two scrolls
(and the shield challenge they start) need, 10 only logged "not implemented" in `src/CHLApi.cpp` when they were counted (among them Khazar's
hand, the held object, hits on an object and the challenge log; Khazar's hand, the held object, `IS_OF_TYPE` and the
challenge log are real now), and the land's control script never runs in openblack
(the story always begins with Land 1's script and reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked), so the quest never appears;
every row below is pending unless the notes say otherwise. A quest run
(quest=SetupLand2/MiracleChallenges(marker@3334.112;76.487;3165.987) on Land2.txt, timeline
quest.khazars_fireball_challenge, run qrp4_q2, 2026-10-29, `refs/story/evidence/qrp4_q2.log`) plays the scroll, the tour and the
challenge up to its first wait; how to run it and where it stops are under [openblack](#openblack).

**Progress: 43/70 done, 0 chosen, 9 partial, 0 inferred — 69%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the Worship Site lesson ends, Khazar's hand flies to a spot just south of the player's worship site (about 75 m from the altar) and the "Miracle Challenge" scroll is started there; the lesson does not wait for it | done | `MoveComputerPlayerPosition` is real; see [worship_site.md](worship_site.md); quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 280318); its effects' own rows are done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed…", [hand_in_scripts.md](../../hand/hand_in_scripts.md) "Scripts put a highlight" |
| Nothing else in the land waits for either miracle challenge to be finished: no flag is set on completion, and the land's later quests (The Workshop, Impress Village, the last scroll) are started by other conditions | pending | the same script |
| What it unlocks: only the Fireball miracle at the home village's worship site; the same miracle also comes with Town10 (an Indian town that starts with a fire miracle) when it is won | done | `Land2.txt` gives town 10 a fire miracle and our map loading reads it; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533603); its effects' own rows are done: [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Puts a miracle inside an object (a dispenser, a chest) or takes it out", [challenge_natives_towns_and_players.md](../../scripts/challenge_natives_towns_and_players.md) "Gives the town with a number from the land script" |
| An unfinished challenge holds Khazar "busy" for as long as it runs; the land's control script will not start Khazar's death while he is busy, so leaving this challenge half done holds back the end of the land (see Script quirks) | pending | Khazar's death needs computer-player natives (real now; not checked in play) |

## The Miracle Challenge scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears 6 m up in the air at the spot Khazar's hand went to, just south of the worship site | done | `CreateHighlight` and the height through `SetProperty` (YPos) are real; quest run 2026-10-28: passes, every effect's own row done. |
| A two-minute timer starts as soon as the scroll is placed; it decides Khazar's first line later | done | `CreateTimer` is real (`src/ECS/ScriptTimer`); quest run 2026-10-28: passes, every effect's own row done. |
| The loop ends when the scroll or the spot under it is clicked, or when Khazar has died; either way the scroll is then switched to its active look | done | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real Quest run 2026-10-28: passes; finished by [clicking_and_activating.md](../../hand/clicking_and_activating.md) "Tapping a challenge's marker".; every row that finishes it is now done |
| If Khazar has already died when the loop ends, nothing more happens: neither challenge is started and the two miracles are never taught this way | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The scroll is never deleted by the script; it goes when the scroll's script ends (scripts' own objects are cleared with them; inferred, as for the other scrolls) | pending | the same script |

## Khazar's tour (introduction)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking it starts both challenges at once, each with its own scroll (this one on the target huts, the other on the island hut), and then plays a film | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's music starts | done | `StartMusic` is real; quest run 2026-10-28: passes, every effect's own row done. |
| Khazar's hand flies fast back to the worship site; the camera rises above it over 5 seconds looking west, then pans slowly east over 12 seconds | done | the camera moves, Khazar's hand flight (`MoveComputerPlayerPosition`) and his wait (`ComputerPlayerReady`) are real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done), [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Asking whether a god's hand is ready" (done) (his wait). |
| Khazar speaks: if the scroll was clicked within two minutes of being placed, "You are keen to learn the secret of casting Miracles. I appreciate your enthusiasm."; otherwise "You are ready to learn the secret of using Miracles." | done | `GetTimerTimeRemaining` and `RunText` are real; quest run 2026-10-28: passes, every effect's own row done. |
| Khazar: "Come with me. I have a set up an area where you can learn more." (the text table's own wording); two seconds later the camera lifts over the temple and his hand drifts off east | done | real camera and text natives; quest run 2026-10-28: passes, every effect's own row done. |
| Scroll drawing is switched on, so the two new challenge scrolls show (inferred from where it is switched) | done | `SetDrawHighlight` is real; quest run 2026-10-28: passes, every effect's own row done. |
| His hand flies to the island hut of the shield challenge; the camera moves in over 7 seconds and he says "Here you can learn about defence." | done | Khazar's hand flight is real (`MoveComputerPlayerPosition`); the camera and text are real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |
| His hand flies on to the middle target hut of this challenge; the camera follows over 7 seconds and he says "And here you can learn to attack." | done | Khazar's hand flight is real (`MoveComputerPlayerPosition`); the camera and text are real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |
| The camera pulls back over 4 seconds to look over the huts from the west and Khazar says "Activate the Scroll of your choice." | done | real camera and text natives; quest run 2026-10-28: passes, every effect's own row done. |
| Khazar's hand goes home to his own temple, the music stops, the film ends and Khazar is released | partial 60% | `ReleaseComputerPlayer` is real; `StopMusic` is real. Quest run 2026-10-28: passes; his hand going home by [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done); finished by [khazar.md](../../rival_gods/khazar.md) "His lines are shown as dialogue text and he is released back to his mind after" (his release back to his mind), both pending. |

## How the fireball scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The targets are the three houses of a small empty Norse village on the south-east shore, a neutral village the map makes uninhabitable for the challenge | done | the houses are made by our map loading (`AbodeArchetype`); the land script leaves the village uninhabitable (`SET_TOWN_UNINHABITABLE`, read by `src/ECS/Town/AbodeVillagers.cpp`; see [../../scripts/land2_script.md](../../scripts/land2_script.md)). Quest run 2026-10-28: passes, every effect's own row done. |
| As soon as the challenge is started (at the tour, before its scroll is clicked) an anti-influence ring of radius 50 is laid round the first hut, so the player's hand can't reach in and must throw | done | `InfluencePosition` is real (`src/Magic/Script/CHLInfluence.cpp`, which passes the anti flag on to the ring); quest run 2026-10-28: passes, every effect's own row done. |
| A gold scroll appears on the middle hut | done | `CreateHighlight` is real; quest run 2026-10-28: passes, every effect's own row done. |
| The quest waits for the scroll or the middle hut to be clicked; if Khazar has died first the scroll is made active and nothing more happens | done | `GameThingClicked` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real Quest run 2026-10-28: passes; finished by [clicking_and_activating.md](../../hand/clicking_and_activating.md) "Tapping a challenge's marker".; every row that finishes it is now done |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click, a ring of influence of radius 40 is made where the seeds will lie, so the player can pick them up | done | `InfluencePosition` is real; quest run 2026-10-28: passes, every effect's own row done. |
| All three huts are mended to full health | done | a building's health (property 1) is set (`SetProperty` HEALTH); quest run 2026-10-28: passes, every effect's own row done. |
| The player's control is cut down to the hand only for the whole challenge | done | `SetInterfaceInteraction` is real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533598); its effects' own rows are done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sets how much of the interface the player may use" |
| A film starts; Khazar's music plays and Khazar counts as busy | done | `StartMusic` is real; quest run 2026-10-28: passes, every effect's own row done. |
| The camera swings over 4 seconds to look along the shore at the huts while Khazar's hand flies fast to them | done | the camera and Khazar's hand flight (`MoveComputerPlayerPosition`) are real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |
| Khazar: "Behold three huts. We shall use these as our targets." | done | `RunText` is real; quest run 2026-10-28: passes, every effect's own row done. |
| The log entry "Khazar's Fireball Challenge" is recorded at 0 with the reminder "Try and burn one of these huts with the Fireball Miracle seeds." | done | `Snapshot` is real; quest run 2026-10-28: passes; quest_check --natural PASS, run qrp4_q2 (2026-10-29 22:31:54, refs/story/evidence/qrp4_q2.log line 45479); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture" |
| Khazar's hand flies slowly back inland and three Fireball one-shot seeds appear on a hillside about 270 m from the huts, 2 m apart in a diagonal line | partial 60% | `CREATE` of one-shot seeds is real (`magic::script::CreateOneShotSpell`), and so is Khazar's hand flight (`MoveComputerPlayerPosition`); not checked in play. Quest run 2026-10-28: passes; his hand's flight by [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done); finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up". |
| The camera moves over 4 seconds to just behind the seeds, looking out to the huts | done | real camera natives; quest run 2026-10-28: passes, every effect's own row done. |
| Khazar: "Here are some fireballs to use." while his hand drifts down to hover by the seeds | done | Khazar's hand is real (the computer player's hand natives); `RunText` is real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |
| Khazar's fireball hand demo plays (a fire seed is put in the hand): "You hold the Action Button down. Like this.", pausing for the hand; "You then move the Hand to put momentum on the fireball.", pausing again; "You release the Action Button to cast the fireball."; then "Devastating! Fire spreads and causes serious damage." | partial 60% | `PlayHandDemo` and `HandDemoTrigger` are real (`src/Input/HandDemo.h`); in a quest run (run qrp4_q2, 2026-10-29, refs/story/evidence/qrp4_q2.log) the demo's seed was never thrown ([openblack](#openblack)); in the hand-played run the demo's seed is armed and cast during the demo (lines 501744 and 505450, `Hand seed: cast FIRE`); quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 507029); finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Plays a recorded demonstration of the hand" |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First the quest waits for the player to hold a fire miracle: until then, every 30 seconds (the first at once) the good advisor says "Tap the Fireball Miracle." | done | `GetObjectHeld199` (the player's hand) and `IsOfType` are real; in a quest run (run qrp4_q2, 2026-10-28) the wait reads the hand from turn 818, which still holds the hand demo's unthrown seed ([openblack](#openblack)); quest run 2026-10-28: passes; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 508626); its effects' own rows are done: [advisors.md](../advisors.md) "Every advisor line has its own recording", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Gives the object a player's hand or a creature is holding: *get held by ‹value›*", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether an object is of a kind" |
| The check looks for a fire spell seed in the hand (script type 24): neither the one-shot globe nor the seed its tap puts in the hand counts | done | the in-hand check (`GetObjectHeld199`, `IsOfType`) is real. The original settles which seeds count: neither the globe nor its tapped seed. The globe is a mobile object (script type 20), and the seed its tap puts in the hand is the one-shot type 30, not 24 (`SpellSeed::GetScriptObjectType` 0x729C90; [openblack](#openblack)); quest run 2026-10-28: passes; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 508628); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether an object is of a kind", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Gives the object a player's hand or a creature is holding: *get held by ‹value›*" |
| The first wait also ends if all three seeds are gone without one being seen in the hand (for instance thrown at once); then the advisor's throwing tip is skipped | pending | in the original this is how the wait ends in normal play, since a tapped seed never answers the in-hand test ([openblack](#openblack)); the seed counts use real natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 15 m from the first seed's spot fades out and is made again at its own spot | partial 60% | real field-of-view, distance, delete and create natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand Quest run 2026-10-28: passes; done in their own rows: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe", "Delete with no mode: a villager, an animal, a building…"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with fade: any other object (a scroll, a scaffold, a villager, a rock)", [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up". |
| Once a fire miracle is held, the evil advisor steps out: "Now throw it at a hut like you'd throw a rock." It plays only for a fire seed not made by a globe: a rival's fireball caught in the hand, a globe's seed still held after a save and reload, or a seed from the player's own Fireball icon. A seed tapped from the challenge's globes never brings it | pending | follows from the in-hand check (`IsOfType` type 24 sub-type 2), which is real; which seeds answer 24 in the original and in openblack: [openblack](#openblack); not checked in play |
| Any hut that drops below full health counts as hit, once; hits add up over the whole challenge | done | a building's health (property 1) is read (`GetProperty` HEALTH, real); quest_check --natural PASS, run l2_worship_deaths3 (2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 498045); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of an object (a villager, a…" |
| A round ends at once if all three huts are hit; otherwise when all the seeds are used up: the quest waits 1 second, then until the hand is empty, then 6 seconds more, and counts the huts one last time | done | the hit test's health read (`GetProperty` HEALTH) and the empty-hand wait (`GetObjectHeld199`) are real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 515723); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of an object…", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Gives the object a player's hand or a creature is holding: *get held by ‹value›*" |
| The throw itself (fireball flight, bouncing, setting the huts alight and fire spreading) is the miracle's normal behaviour | partial 80% | the fireball miracle works in our tree ([../../miracles/fireball.md](../../miracles/fireball.md)); the challenge never runs; finished by [fireball.md](../../miracles/fireball.md) "The ball flies under gravity", "Villagers set alight run burning and die", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |

## Results and retries

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After each round a short film shows the huts from the shore, the camera drifting closer over 10 seconds (8 after a miss), then snaps back to where the player was | done | real camera natives; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 516113); its effects' own rows are done: [script_camera.md](../../camera/script_camera.md) "Set the camera's position at once", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Glide the camera's focus to a point over a time" |
| All three huts hit: the log entry is set to 1 and the evil advisor says "Nice aiming, Boss. You damaged all three huts." and "Fire spreads, you see. A Village can be torched in minutes." | pending | the hit count reads health (`GetProperty` HEALTH, real) and the challenge record is real (`Snapshot`, `UpdateSnapshot`); not checked in play |
| Two huts hit: "Pretty nifty. Two of the three huts are damaged." then the same fire-spreads line; log 1 | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| One hut hit: "Oh yeah! You hit one of the huts." then the fire-spreads line; log 1 | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| Any hit ends the challenge; there is no reward difference between one hut and three | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| No hut hit: the log entry's success goes up by a third, and the evil advisor says "Oh man. None of the huts got toasted." after the first miss, "You still haven't managed to hit anything." after the second and third | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); the hit test reads health, which is real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 515732); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "Its record is updated with how well it went", [advisors.md](../advisors.md) "Scripts make them appear and disappear", [advisors.md](../advisors.md) "Every advisor line has its own recording", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…" |
| After a miss three new seeds appear at the same spots; Khazar says "Have some more practice. Here are more Fireballs." the first time and "Try again." the second | partial 60% | `CREATE` of the seeds and `RunText` are real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 516188); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe", [advisors.md](../advisors.md) "Khazar, the friendly god of the second land…", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…"; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up" |
| The player gets three rounds at most; after the third miss the challenge ends as if passed | done | plain script; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533597) |
| There is no failure: the challenge always ends with the reward | done | the same script; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533603) |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The music stops, the log entry is set to 1 and the player's full control comes back | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); `StopMusic` and `SetInterfaceInteraction` are real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533598); its effects' own rows are done: [music.md](../../audio/music.md) "Scripts start and stop a piece of music", [challenges_and_rewards.md](../challenges_and_rewards.md) "Its record is updated with how well it went", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sets how much of the interface the player may use" |
| In a dialogue (not a film) the Fireball miracle (first level) is given to the player's home village, so its worship site can pray for it | done | `SetMagicInObject` is real, and so is the home town's look-up (`GetTownWithId`); see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533603); its effects' own rows are done: [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Puts a miracle inside an object (a dispenser, a chest) or takes it out", [challenge_natives_towns_and_players.md](../../scripts/challenge_natives_towns_and_players.md) "Gives the town with a number from the land script" |
| Khazar: "My Fireball Challenge is complete.", "I have given the knowledge of the Fireball Miracle to your Village.", "From now on your people can worship for this Miracle at your Worship Site." | done | `RunText` is real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 537385); its effects' own rows are done: [advisors.md](../advisors.md) "Khazar, the friendly god of the second land…", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…" |
| The evil advisor: "Wow. We can create these now. Bet they'll impress non-believers!" | done | `RunText` is real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 539891); its effects' own rows are done: [advisors.md](../advisors.md) "Every advisor line has its own recording", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…" |
| Khazar is released and no longer busy; any seeds left fade away | done | `ReleaseComputerPlayer` is real; `ObjectDelete` is real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 541984); its effects' own rows are done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Releasing a god back to its mind"; now done too: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with fade: any other object (a scroll, a scaffold, a villager, a rock)" |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The huts stay as damaged as the player left them (burning huts burn on); nothing rebuilds them | pending | the same script; fire burns on in our tree |
| The fireball miracle stays at the home village's worship site for the rest of the land | pending | follows from the gift, which needs the home town (`GetTownWithId` is real; not checked in play) |
| The influence ring by the seeds and the anti-influence ring round the huts are never removed by the script (undetermined: whether they go when the script ends) | pending | the same script |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor owns the notifications and the results; the good advisor owns the log reminder and the tap tip | done | `SpiritEject` and `RunText` are real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 515728); its effects' own rows are done: [advisors.md](../advisors.md) "Scripts make them appear and disappear", [advisors.md](../advisors.md) "Every advisor line has its own recording" |
| Clicking the log entry speaks its reminder "Try and burn one of these huts with the Fireball Miracle seeds." through the good advisor, who steps out to say it | partial | the shared reminder calls real natives; the challenge record is real (`Snapshot`, `UpdateSnapshot`); no quest check or natural run has reached it yet; finished by [scrolls_and_signs.md](../../interface/scrolls_and_signs.md) "A started scroll stays as a reminder", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Takes a picture of the camera's view for the challenge log in the temple", "Updates the challenge log picture with what is going on", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays through the tour, and again from the challenge's click until the reward | done | `StartMusic` and `StopMusic` are real; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533596); its effects' own rows are done: [music.md](../../audio/music.md) "Scripts start and stop a piece of music" |
| The tour stops the music at its end; the challenge stops it only after the last round | done | the same script; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533596); its effects' own rows are done: [music.md](../../audio/music.md) "Scripts start and stop a piece of music" |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts stops it from fetching or throwing the seeds (undetermined what happens if it does) | partial 60% | the same script; absent quest_check --natural PASS, nothing matched from line 280318 to the run's end, natural run l2_worship_deaths3 (2026-10-29 16:54:14, refs/story/played/l2_worship_deaths3.log line 533597); undetermined: what the creature does with the seeds |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar counts as busy from the click until the reward; Khazar's death only starts while he is not busy, so a challenge clicked and then ignored (seeds never used) holds back Khazar's death, and with it Lethys's vortex and the end of the land, until it is finished | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The busy flag is shared: the shield challenge (or any other Khazar lesson) finishing clears it even while this one is still running, so Khazar's death can then start in the middle of this challenge | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The first hut is looked up at the village's position, about 5 m from that house's own position (undetermined: whether the lookup finds it; if not, that hut can't count as hit) | partial 60% | `CallNear` is real, so the same lookup would be made. Quest run 2026-10-28: passes; the undetermined point (whether the lookup finds the first hut) is still open, and `CALL_NEAR` is partial in the script coverage. |
| A round with seeds left never ends while the player holds something: the wait for an empty hand has no time limit | pending | the empty-hand wait reads `GetObjectHeld199`, which is real; not checked in play |
| The three hit results all set the log to 1 before the last line; a miss raises it by a third, so after three misses it reaches 0.99 before the final 1 | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The two "no hut hit" lines after the second and third misses are the same line | done | the same script; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 533313) |


## openblack

- **How to run it:** `OB_SESSION=<session> sh tools/checks/story/quest_run.sh <label> <exe folder>
  "SetupLand2/MiracleChallenges(marker@3334.112;76.487;3165.987)" quest.khazars_fireball_challenge 30000
  tools/checks/story/quests/khazars_fireball_challenge.txt Land2.txt`. The marker is the spot by the worship site where
  the Worship Site lesson leaves the Miracle Challenge scroll. MiracleChallenges starts both challenges, so [Khazar's
  Shield Challenge](khazars_shield_challenge.md#openblack) runs the same way with its own timeline and spec. Add
  `OPENBLACK_SPELL_TRACE=1` for the hand's seed lines (`Hand seed: ...`,
  `src/ECS/Systems/Implementations/HandSpellSeed.cpp`).
- **The timeline** (`src/Debug/TestbedQuestKhazarsFireball.cpp`): it taps the Miracle Challenge scroll, waits for the
  tour's script to stop, taps the huts' scroll, then taps each globe and throws at the huts. Every wait for a line is
  the testbed's voice wait (`WaitText`, which holds until the line's voice ends, like the script's `wait until read`). A
  wait that holds once the text is shown lets the next input land inside the line's dialogue.
- **The last run** (run qrp4_q2, 2026-10-29, `refs/story/evidence/qrp4_q2.log`): the quest check passes 30 of the 70 rows, fails
  26, and has 1 gate row and 13 rows with no spec. Everything up to the first wait passes: the Miracle Challenge scroll,
  Khazar's tour, the huts' scroll, the introduction, the seeds and the hand demo's lines.
- **The hand-played run** (`refs/story/played/l2_worship_deaths3.log`, 2026-10-29 16:37:28, joint290's exe, md5
  4358b4ea): Land 2 played by hand from its start with the whole land control script, read with `--natural`. The
  quest check passes 50 of the 70 rows, fails 7, and has 13 rows with no spec. The Miracle Challenge scroll is placed as
  the Worship Site lesson ends (line 280318) and tapped at turn 13521 (line 482995); the tour, the huts' scroll, the
  hand demo, three rounds of seeds and the reward follow. The demo's seed is armed and thrown during the demo (lines
  501744 and 505450, `Hand seed: cast FIRE`), so the stop below does not happen in this run. The player hits no hut in
  three rounds. The seven that fail: the three hit results, "any hit ends the challenge" and the hit results' log
  quirk (no hut was hit), the "Now throw it" line (it cannot play for a tapped seed, below), and "Clicking it starts
  both challenges", whose patterns put the two challenges' first look-ups before the tour's film starts. In the run the
  film starts first and the two challenges (the shield's first) make their markers and find their huts later in the
  same turn (lines 483081 to 483107), as the script machine runs a task started during its pass once the starting task
  yields ([the loop](../../../../engine/loop.md)); the spec's order is not yet checked against that.
- **Where it stops: the hand demo's seed is never thrown.** The demo (`PLAY_HAND_DEMO` "fireball" with its trigger
  pause, turn 570) puts fire seed obj:3520 in the hand (turn 571). Its recorded press (record 26) and release (record
  100) are a swing and a throw. The original throws here: its playback copies each record's throw block into the hand
  before a button message (0x5DB0A5..0x5DB0B6, [the hand demos](../../../debugging.md#hand-demos)), and the demo's own
  lines describe the throw and its hit. In openblack the seed trace has no `Hand seed:` line between the demo's start
  and its end (turns 570 to 707): no arming, no failed apply and no cast. The seed stays in the hand (`GET_OBJECT_HELD`
  gives obj:3520 from turn 818 on). The timeline's first globe tap then presses with that seed and casts it at the
  globe's spot (turn 840, `Hand seed: cast FIRE at (3246.6, 3165.6)`). A real tap would not have made a seed anyway,
  because a busy hand takes none (`src/Magic/Core/OneOffSpellSeed.cpp:109-112`).
  - Which exit drops the demo's press is not traced yet. A held seed's press goes to `HandSystem::SeedActionPressed`.
    Its only exit that writes no trace line is the hand being out of the influence
    (`src/ECS/Systems/Implementations/HandSpellSeed.cpp:498-503`). Otherwise, the press never reached the seed branch of
    the press chain (`src/ECS/Systems/Implementations/HandSystem.cpp`).
  - By rough geometry the demo's press lands inside the seeds' ring, `INFLUENCE_POSITION(3246.58, 25.46, 3165.65, 40, 0,
    0)`. That is an estimate, not a measurement.
  - Ruled out: the hand's apply-to-object state, because no thrown fireball existed before turn 840. Also ruled out: the
    tug hold on a resting globe, because a hand that holds something takes the seed branch.
- **The script's in-hand test `type 24 2`** (the first wait, `Land2FireballChallenge.txt`; it is the evil advisor's "Now
  throw it at a hut", text 4534):
  - In the original only `SpellSeed::GetScriptObjectType` (0x729C90) answers 24. It answers 30 when the seed's byte
    +0x72 is set (xref notes #3345, #3347-#3349).
  - The only code that sets +0x72 is `OneOffSpellSeed::CreateSpellIntoHand` (0x72A730, the write at 0x72A803, note
    #3355). That is a globe's tap and a script's `CREATE` of a one-shot spell into the hand. So a tapped globe seed
    answers 30, and 4534 does not play in normal play.
  - A fire seed that answers 24 comes from three places:
    - a rival's fireball caught in the hand, a loose seed whose +0x72 stays 0 (0x682EA0, note #3351);
    - a globe's seed held across a save and reload. `SpellSeed::Save` (0x729CA0) does not write +0x72, `SpellSeed::Load`
      (0x729FB0) does not read it, and the default constructor (0x727F20, through 0x728140) zeroes it (notes #3350,
      #3352-#3354);
    - a seed from the player's own Fireball icon.
  - Not checked: whether anyone casts a fireball near the player during this wait on Land 2, and whether the player can
    own a Fireball icon then.
  - raffclar's #486 (16196afb6) marks the same seeds (his `fromGlobe`, set by the globe's tap and the script's
    seed into the hand) and adds a fallback to the miracles' held seed in GET_OBJECT_HELD; openblack has both already:
    `fromOneShot` below, and a seed in the hand is the hand's held thing (`HandSystem::GetHeldObject`). Not taken.
  - In openblack the flag is `SpellSeed::fromOneShot`. It is set only by `one_off::CreateSpellIntoHand`
    (`src/Magic/Core/OneOffSpellSeed.cpp:128`), and `src/ECS/ScriptTypes.cpp:196` answers 30 for it, as the original
    does. Not yet checked: whether openblack's save and load keep `fromOneShot`. The original drops the flag on a
    reload.
- The detail is in [the script object types](../../../../scripts/land1-intro.md#the-script-object-types).

## Pending

- Find which exit drops the fireball demo's press. This needs a trace line for the seed press's silent exit, or the
  press branch, as a debug readout. Then fix it as its own measured commit. The hand demo checks play this demo with an
  empty hand, so they do not reach this path.
- After that, the rows from the throw on: the hits, the results, the retries and the reward. The spec's row for 4534
  cannot pass in a normal run of the original, because no tapped seed answers 24. Its pattern needs a caught fireball or
  a reload.
- Check whether openblack's save and load keep `SpellSeed::fromOneShot`. The original's reload makes a globe seed answer
  24.
