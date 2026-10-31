# Khazar's Shield Challenge

A land 2 gold scroll and Khazar's lesson in defence: at a lone hut on a small island off the east shore Khazar casts a
Physical Shield and lets the player throw rocks at it, then hands over three shield seeds; the player shields the hut,
Khazar drops three boulders on it, and whatever happens the Physical Shield miracle is given to the player's village.
It is started, together with [Khazar's Fireball Challenge](khazars_fireball_challenge.md), by the "Miracle Challenge"
scroll Khazar leaves after the [Worship Site](worship_site.md) lesson (that scroll and Khazar's tour are described in
the fireball file). The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), the miracle itself in
[../../miracles/physical_shield.md](../../miracles/physical_shield.md).

**Land:** 2 · **Giver:** Khazar (the friendly god), through a scroll on the island hut · **Script:** PhysicalShieldChallenge · **Reward:** the Physical Shield miracle at the player's home village's worship site, given whatever the result · **Repeatable:** no

Sources: the quest's script source (`Land2ShieldChallenge.txt`), the scroll that starts it
(`Land2FireballChallenge.txt`), the land's scroll-notify helper (`SetupLand2.txt`), the hand demo (`HandDemos.txt`) and
the land's control script, checked against the compiled `challenge.chl`; the game's English text table; and the land's
map script (`Scripts/Land2.txt`) for the hut. The attack on the hut is Khazar's own boulders, not Lethys's (the land
file's summary row says Lethys). openblack's state is judged on this tree: of the 55 script functions this challenge
needs, 9 only logged "not implemented" in `src/CHLApi.cpp` when they were counted (among them Khazar's hand, the held object,
hits on an object, aiming a rock and the challenge log; Khazar's hand, the held object and the challenge log are real
now), and the land's control script never runs in openblack (the story reaches it only through the earlier lands' exits
(`LOAD_MAP` changes land now, not yet checked) and the story always begins with Land 1's script), so the quest never appears; every row below is
pending unless the notes say otherwise. A quest run (quest=SetupLand2/MiracleChallenges(marker@3334.112;76.487;3165.987)
on Land2.txt, timeline quest.khazars_shield_challenge, run qrp4_q3, 2026-10-29, `refs/story/evidence/qrp4_q3.log`) plays the
challenge up to the player's go, and a hand-played run of the same entry (2026-10-28,
`refs/story/played/play_shield2.log`) plays it through to the reward; how to run them and what they show are under
[openblack](#openblack).

**Progress: 33/60 done, 0 chosen, 15 partial, 0 inferred — 69%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Started only by clicking the "Miracle Challenge" scroll by the worship site, at the same moment as the fireball challenge; the two can be done in either order | partial 60% | plain script; see [khazars_fireball_challenge.md](khazars_fireball_challenge.md#the-miracle-challenge-scroll); quest_check --natural PASS, run l2_worship_deaths3 (2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 483172); finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object" |
| If Khazar has died before the Miracle Challenge scroll is clicked, this challenge is never started | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| Nothing in the land waits for it: no flag is set on completion | pending | the same script |
| What it unlocks: only the Physical Shield miracle at the home village's worship site; the same miracle also comes with Town2 (Khazar's Greek town at the start) and Lethys's home town when they are won | partial 60% | `Land2.txt` gives towns 2 and 5 a physical shield miracle and our map loading reads it; the challenge's own gift is `SetMagicInObject`, real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The player's worship site shows one icon per miracle they have", "Converting a town adds its village-centre miracle" (towns 2 and 5). |
| While it runs Khazar counts as busy, which holds back his death and so the end of the land (see Script quirks) | pending | Khazar's death needs computer-player natives (real now; not checked in play) |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut is the single house of a tiny empty Norse village on an island, a neutral village the map makes uninhabitable for the challenge | done | the house is made by our map loading (`AbodeArchetype`); the land script leaves the village uninhabitable (`SET_TOWN_UNINHABITABLE`, read by `src/ECS/Town/AbodeVillagers.cpp`; see [../../scripts/land2_script.md](../../scripts/land2_script.md)). Quest run 2026-10-28: passes, every effect's own row done. |
| A gold scroll appears on the hut | done | `CreateHighlight` is real; quest run 2026-10-28: passes, every effect's own row done. |
| The quest waits for the scroll or the hut to be clicked; the scroll is then switched to its active look | done | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real Quest run 2026-10-28: passes; finished by [clicking_and_activating.md](../../hand/clicking_and_activating.md) "Tapping a challenge's marker".; every row that finishes it is now done |
| If Khazar dies before the click, the scroll is made active and nothing more happens | pending | Khazar's death needs computer-player natives (real now; not checked in play) |

## The introduction and Khazar's demonstration

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click Khazar counts as busy and his hand flies fast to a spot on the shore south-west of the hut | done | `MoveComputerPlayerPosition` and the wait for his hand (`ComputerPlayerReady`) are real. Quest run 2026-10-28: passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done), [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Asking whether a god's hand is ready" (done) (the wait for his hand). |
| A ring of influence of radius 40 is made round the hut, and the hut is mended to full health | done | `InfluencePosition` and a building's health (property 1, `SetProperty` HEALTH) are real; quest run 2026-10-28: passes, every effect's own row done. |
| A film starts with Khazar's music; the camera moves in on the hut over 3 seconds | done | `StartMusic` and the camera moves are real; quest run 2026-10-28: passes, every effect's own row done. |
| The log entry "Khazar's Shield Challenge" is recorded at 0 with the reminder "We need to throw stones at Khazar's Shield" | done | `Snapshot` is real;; quest run 2026-10-28: passes; quest_check --natural PASS, run qrp4_q3 (2026-10-29 22:37:25, refs/story/evidence/qrp4_q3.log line 43757); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture" |
| Khazar: "Soon you will need to protect your Villagers and their buildings."; the camera slides round over 6 seconds | done | `RunText` and the camera are real; quest run 2026-10-28: passes, every effect's own row done. |
| Three rocks (0.7 scale) appear on the mainland about 80 m west of the hut, 2 m apart | done | `CreateWithAngleAndScale` makes rocks; quest_check PASS, the hand-played run (`refs/story/played/play_shield2.log`); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Creates an object of a kind at a position with an angle and a scale". |
| Khazar: "The Physical Shield protects against objects which are not of a spiritual nature." and "When you have the Miracle, this is how you cast a Shield." | done | `RunText` is real; quest run 2026-10-28: passes, every effect's own row done. |
| Whatever the player is holding is deleted, so the hand is empty for the demo | done | the held object comes from `GetObjectHeld199`, and `ObjectDelete`, both real; the quest run of 2026-10-29 (`refs/story/evidence/qrp4_q3.log` line 57271) passes; its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with no mode: a villager, an animal, a building…". |
| Khazar's shield hand demo: the camera frames the hut over 2 seconds and a shield miracle is put in the hand with seven times a normal shield's strength, "to deflect the 3 rocks"; Khazar: "You move the Hand to where you wish to start the Shield.", "Then you hold down the Action Button and trace the perimeter of the Shield. Like this.", "And the Shield appears." as the demo hand draws the circle and the shield goes up over the hut | partial 60% | `PlayHandDemo` is real, and the shield miracle put in the hand takes its strength (`SetProperty` 17 of a miracle in a hand sets `SpellSeed::castMultiplier`); since the hand demos feed their recorded moves to the gestures, the demo's circle is recognised and its shield cast: a hand-played run (2026-10-28, `refs/story/played/play_shield2.log`) passes this row (line 9417, `RUN_TEXT(false, 5122, 0)`, task PShield). Finished by the hand demos' gesture row, [gesture_effects.md](../../gesture/gesture_effects.md) "The tutorial plays recorded hand demos" |
| The camera moves over 2 seconds to a spot by the rocks, aimed at the hut, and the player's control is cut down to the hand | done | `SetInterfaceInteraction` and the camera are real; quest run 2026-10-28: passes, every effect's own row done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sets how much of the interface the player may use". |
| As the film ends Khazar says "Now throw these rocks at the hut and watch the Shield." | done | `RunText` is real; quest run 2026-10-28: passes, every effect's own row done. |

## Throwing the rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest finds the shield within 5 of the hut and waits until the shield is gone or hit, or 20 seconds pass | partial 60% | `SpellAtPoint` and `GameThingHit` are real; the wait ends by the shield being gone. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Finds a miracle of a kind already at a position, within a radius", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether an object has been hit". |
| The shield stopping thrown rocks (and wearing down) is the miracle's normal behaviour | partial 80% | the physical shield works in our tree ([../../miracles/physical_shield.md](../../miracles/physical_shield.md)). `CPShield hit` is never true, in the original as in openblack: the script holds the shield spell, and the hit record only ever holds a physics body ([wiki](../../../../magic/shields.md#the-hit-record-and-the-shield-challenge)); the wait ends by the shield being gone or by the 20 seconds. A hand-played run (2026-10-28) ended it by the shield being gone; finished by [physical_shield.md](../../miracles/physical_shield.md) "Thrown objects bounce off with the game's spring response", "A blow by a rock costs the dome by the rock's speed and mass"; still open: no quest run's PASS is cited for it |
| Full control comes back after the wait | done | `SetInterfaceInteraction` is real; quest run 2026-10-28: passes, every effect's own row done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sets how much of the interface the player may use". |
| If the shield was hit or broke: a film, the camera rising over 6 seconds to look down at the hut; Khazar: "See? The hut is protected. But a strong enough attack will destroy the Shield." then "Now you can try. Shield the hut and I will attack it." | done | real camera and text natives. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| If 20 seconds pass with no hit: Khazar says "The Shield deflects the rocks." (whether or not any rock was thrown) as the camera rises, then "Now you can try. Shield the hut and I will attack it." | done | real camera and text natives; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 560546); its effects' own rows are done: [advisors.md](../advisors.md) "Khazar, the friendly god of the second land…", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Glide the camera's focus to a point over a time" |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three Physical Shield one-shot seeds appear on the shore about 40 m south-west of the hut, 2 m apart, each with three times a normal shield's strength | partial 60% | `CREATE` of one-shot seeds is real (`magic::script::CreateOneShotSpell`), and their strength through `SetProperty` 17 of a one-shot orb sets `OneOffSpellSeed::scale`, the multiplier of the miracle each puts in the hand (2026-10-28, not yet run); quest run 2026-10-28: passes; done in their own rows: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe"; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Sets the strength or alignment of anything else" (seed strength). |
| Khazar's demonstration shield is removed and the hut mended to full health again | done | `ObjectDelete` with no mode on the shield, and mending the hut (health, property 1), are real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| The player's control is cut down to the hand | done | `SetInterfaceInteraction` is real; quest run 2026-10-28: passes, every effect's own row done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sets how much of the interface the player may use". |
| The log entry moves to 0.3 with the new reminder "We need to cast a Shield over the hut on this island." | done | `UpdateSnapshot` is real;; quest run 2026-10-28: passes; quest_check --natural PASS, run qrp4_q3 (2026-10-29 22:37:25, refs/story/evidence/qrp4_q3.log line 99478); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture", "Its record is updated with how well it went" |
| The quest waits until the player holds a shield seed or a shield stands within 5 of the hut; every 30 seconds (the first at once) the good advisor says "Tap the Shield Miracle and wait." | partial 60% | `GetObjectHeld199` (the player's hand), `IsOfType` and `SpellAtPoint` are real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [advisors.md](../advisors.md) "Every advisor line has its own recording", [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Finds a miracle of a kind already at a position, within a radius". |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 12 m from the first seed's spot fades out and is made again at its own spot | partial 60% | real field-of-view, distance, delete and create natives; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 562207); its effects' own rows are done: [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Whether a position is in the camera's view", [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Gives the distance between two positions", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with fade: any other object (a scroll, a scaffold, a villager, a rock)", [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up" |
| Once a seed is held, the evil advisor says "Hold the Action Button and draw a circle on the ground to place the Shield." | done | follows from the in-hand check, which is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 563150); its effects' own rows are done: [advisors.md](../advisors.md) "Every advisor line has its own recording", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether an object is of a kind", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Gives the object a player's hand or a creature is holding: *get held by ‹value›*" |
| The player then gets one go: the quest waits 1 second, looks for a shield within 5 of the hut, and if there isn't one waits until the hand is empty and 3 seconds more, then moves on whether or not a shield was cast | partial 60% | the empty-hand wait reads `GetObjectHeld199` and the search `SpellAtPoint`, both real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Finds a miracle of a kind already at a position, within a radius". |
| A shield drawn so that its centre is within 5 of the hut counts; a shield beside the hut does not | partial 60% | `SpellAtPoint` is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Finds a miracle of a kind already at a position, within a radius". |
| If a shield is found, the good advisor steps out: "Hey! You've cast the Miracle! Marvellous!" | done | `SpiritEject` and `RunText` are real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 570433); its effects' own rows are done: [advisors.md](../advisors.md) "Every advisor line has its own recording", [advisors.md](../advisors.md) "Scripts make them appear and disappear" |
| Full control comes back | pending | `SetInterfaceInteraction` is real |

## Khazar's attack (the test)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the hut is mended to full health again and Khazar's hand rises to 20 m above the hut; the camera moves to the shore over 4 seconds, looking up at it | done | mending the hut (health) and the camera are real; Khazar's hand is real (the computer player's hand natives). The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |
| Khazar: "The Shield is ready." (said whether or not the player cast one) | done | `RunText` is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| Three boulders, each a random 0.6 to 0.8 scale, appear 50 m above the hut at random spots up to 25 m away and are each aimed to land on the hut within 2 seconds, half a second apart | done | `CreateWithAngleAndScale` makes the rocks and `SetTarget` aims them, both real; quest_check PASS, the hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Creates an object of a kind at a position with an angle and a scale". |
| Three seconds later Khazar's hand goes back to the shore | done | Khazar's hand is real (the computer player's hand natives). The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done: [challenge_scripts.md](../../rival_gods/challenge_scripts.md) "Moving a god's hand to a place at a speed" (done). |

## Success and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera closes on the hut over 3 seconds and the log entry is set to 1 either way | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); the camera is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture", "Its record is updated with how well it went" |
| If the hut is still at full health (any damage at all counts as a failure): Khazar says "The hut survived my onslaught. Well done.", "Different sized objects do different levels of damage to a Shield." and "It'll disappear after taking excessive damage as well." | done | a building's health (property 1) is read by `GetProperty` (its life). The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| Otherwise: "You failed to Shield the hut. It is destroyed." and "It is no disaster this time. However you will need practice further." | pending | the result reads health (`GetProperty` HEALTH), which is real; not checked in play |
| There is no retry and no penalty: both endings go on to the reward | done | the same script. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera pulls back over 4 seconds and the Physical Shield miracle is given to the player's home village, so its worship site can pray for it | partial 60% | `SetMagicInObject` is real, and so is the home town's look-up (`GetTownWithId`); see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md). The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The player's worship site shows one icon per miracle they have". |
| Khazar: "I have given the knowledge of the Physical Shield Miracle to your Village." and "From now on your people can worship for this Miracle at your Worship Site." | done | `RunText` is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| The music stops, the film ends, Khazar is released and no longer busy, and any shield seeds left fade away | partial 60% | `StopMusic` and `ObjectDelete` are real; `ReleaseComputerPlayer` is real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with fade: any other object (a scroll, a scaffold, a villager, a rock)" (the seeds fade), [khazar.md](../../rival_gods/khazar.md) "His lines are shown as dialogue text and he is released back to his mind after" (pending). |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut stays as damaged as the boulders left it; the boulders and the three practice rocks stay on the land | pending | the same script |
| The influence ring round the hut is never removed by the script (undetermined: whether it goes when the script ends) | pending | the same script |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor owns the notification, the log reminders, the tap tip and the praise; the evil advisor gives the drawing tip | done | `SpiritEject` and `RunText` are real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes; quest_check --natural PASS, hand-played run l2_worship_deaths3 (joint290, exe md5 4358b4ea, 2026-10-29 16:37:28, refs/story/played/l2_worship_deaths3.log line 568587); its effects' own rows are done: [advisors.md](../advisors.md) "Every advisor line has its own recording", [advisors.md](../advisors.md) "Scripts make them appear and disappear" |
| Clicking the log entry speaks its current reminder through the good advisor, who steps out to say it | partial | the shared reminder calls real natives; the challenge record is real (`Snapshot`, `UpdateSnapshot`); no quest check or natural run has reached it yet; finished by [scrolls_and_signs.md](../../interface/scrolls_and_signs.md) "A started scroll stays as a reminder", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Takes a picture of the camera's view for the challenge log in the temple", "Updates the challenge log picture with what is going on", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays from the click until the reward | done | `StartMusic` and `StopMusic` are real. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts reacts to it (undetermined what happens if it picks up the seeds or rocks) | partial 60% | the same script; absent quest_check --natural PASS, nothing matched from line 622 to the run's end, hand-played run play_shield2 (2026-10-28 17:07:38, refs/story/played/play_shield2.log line 16337); undetermined: what the creature does with the seeds or rocks |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The loop meant to give the player time and fresh seeds counts the remaining seeds into a total it never adds to, so it always believes the seeds are used up: the player gets exactly one attempt, ending 3 seconds after the hand is next empty | done | the same script. The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |
| The third seed's respawn check is guarded by whether the first seed exists instead of the third (both waits), so with the first seed gone the third is never replaced, and with the first present a used third seed may be remade (undetermined which happens in the engine) | pending | the same script |
| The seeds' spot is about 40.6 m from the hut, just outside the radius-40 influence ring, with one seed inside it and one outside (undetermined whether the player can reach all three without the home village's influence) | partial 60% | the same script; quest run 2026-10-28: passes; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The lands' scripts place one-shot globes, plain or powered up". |
| Holding any object (a villager, a rock) when the demonstration starts deletes it | pending | the held object comes from `GetObjectHeld199`, which is real; not checked in play |
| Khazar counts as busy from the click to the reward; a challenge clicked and left half done (seeds never taken) keeps him busy, which holds back Khazar's death and so the rest of the land | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The busy flag is shared with the fireball challenge and Khazar's other lessons, so whichever finishes first clears it while this one may still be running | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| Khazar's "The Shield is ready." is spoken even when no shield was cast | done | the same script: the line has no branch round it (in this run a shield was cast). The hand-played run of 2026-10-28 (`refs/story/played/play_shield2.log`, quest_check 43 of 60 rows) passes, every effect's own row done. |


## openblack

- **How to run it:** `OB_SESSION=<session> sh tools/checks/story/quest_run.sh <label> <exe folder>
  "SetupLand2/MiracleChallenges(marker@3334.112;76.487;3165.987)" quest.khazars_shield_challenge 30000
  tools/checks/story/quests/khazars_shield_challenge.txt Land2.txt`. It runs the same as [Khazar's Fireball
  Challenge](khazars_fireball_challenge.md#openblack), because MiracleChallenges starts both challenges. Add
  `OPENBLACK_SPELL_TRACE=1` for the hand's seed lines (`Hand seed: ...`).
- **The timeline** (`src/Debug/TestbedQuestKhazarsShield.cpp`):
  - It picks the practice rocks as script type Rock (33). A challenge rock (`CREATE_WITH_ANGLE_AND_SCALE` type 8,
    sub-type 2) is a rock to the scripts (`src/ECS/ScriptTypes.cpp:150-157`), not a mobile static.
  - Every wait for a line is the testbed's voice wait (`WaitText` until its voice ends).
  - It throws the first rock only after line 4568's dialogue. The challenge clears its hit memory only then, so a hit
    before that is forgotten.
- **The timeline's run** (run qrp4_q3, 2026-10-29, `refs/story/evidence/qrp4_q3.log`): the quest check passes 24 of the 60 rows, fails
  23, and has 1 gate row and 12 rows with no spec. Everything up to the wait for a held seed passes: the scroll, the
  introduction, the rocks, the hand demo's lines, the wait after the rocks, which sees no hit (line 74629), the
  20-second line (turn 1101, line 94202) and the seeds. The drawing tip and everything after it fail: no shield seed
  is ever held (`IS_OF_TYPE(..., 30, 5)` is never true).
- **The hand-played run** (`refs/story/played/play_shield2.log`, 2026-10-28, recorded with `hand_play.sh`; its exe
  has the two fixes below, which landed in joint278): the quest check passes 43 of the 60 rows, fails 4, and has 1 gate
  row and 12 rows with no spec. The demo's circle casts Khazar's shield over the hut (`SPELL_AT_POINT(20, ...)` finds
  it at turn 969), the rocks are thrown until it is gone, the player taps a seed and casts a shield on the hut,
  Khazar's boulders fall, the hut survives and the reward is given. The four that fail are the branches this play did
  not take (20 seconds with no hit, lost seeds remade, the hut destroyed) and the busy flag, which the quest check reads
  from a global the run does not log.
- **The second hand-played run** (`refs/story/played/l2_worship_deaths3.log`, 2026-10-29 16:37:28, joint290's exe,
  md5 4358b4ea): Land 2 played by hand from its start with the whole land control script, read with `--natural`. The
  quest check passes 44 of the 60 rows, fails 4, and has 12 rows with no spec. The challenge starts from the player's
  tap on the Miracle Challenge scroll (line 482995), the hut's scroll is tapped at turn 14826 (line 537765), Khazar's
  demo shield takes no hit in its 20 seconds, the player casts a shield on the hut, the boulders fall, the hut keeps
  its full health and the reward is given (line 576465). The four that fail are the branches this play did not take
  (the demo shield hit, the hut destroyed) and the busy flag, which the run does not log.
- **Where the timeline's run stopped, before those fixes: the hand demo's circle was not recognised, a difference in
  the demo's playback**, fixed by "Hand demo: each recorded mouse move the playback plays is a gesture sample at its
  pixel" (4c340650b), with "Magic: a script's search for a shield at a point finds the first available one" (2ee8bcd6a).
  - The demo ("shield", turn 678) puts shield seed obj:3537 in the hand. The recorded press arms it (`Hand seed:
    armed`). The release at the hut fails (`Hand seed: FailApply at (3620.1, 8.7, 3459.5)`), and no `Gestures: circle`
    line is written between the two.
  - The release's test that fails is `GestureAllowsCast`. A shield needs its circle
    (`src/ECS/Systems/Implementations/HandSpellSeed.cpp:561-564`). The original has the same test (0x729AF0 in
    `SendApplyToMapCoord`, [hand casting](../../../../magic/hand-casting.md)), so the refusal itself is right.
  - The original feeds the demo's moves to the gestures (xref notes #3359-#3364):
    - Its playback (0x5DAEE0) sends every record, mouse moves included, to the interface's message handler (at
      0x5DB2EE), with the record's pixel placed between the cinema bars.
    - The handler of a mouse move (0x5D99E0) adds one gesture sample at that pixel (0x5CEAD0). So each recorded move is
      exactly one sample: the circle is records 61 to 251, 191 moves.
    - The live mouse adds none while a demo plays (`GGame::MouseHandler` 0x54FFE0 returns early at 0x55003B).
  - openblack's playback then only moved the cursor, and its gestures were sampled by the live path at the cursor, so
    the samples differed in count, place and time. The fix feeds each recorded move to the gestures as one sample.
  - What followed in that run, in order:
    - Khazar's demonstration shield did not exist, so the wait for a hit ended at once.
    - The seeds' wait ended at once on the demo's seed (`IS_OF_TYPE(held, 30, 5)`).
    - The player's go waited for an empty hand for ever (`Land2ShieldChallenge.txt:201-203`).
    - The timeline's tap on a seed did nothing, because a busy hand takes no seed
      (`src/Magic/Core/OneOffSpellSeed.cpp:109-112`).
- In that run the demo's strength settings on its seed (property 17) logged "not implemented" (`SET_PROPERTY` and
  `GET_PROPERTY` for 17, `src/CHLApi.cpp`), and so did the seeds' strength; both are ported since (2026-10-28,
  [script commands](../../../../scripts/script-commands.md#properties-and-flags)), not yet run.

## Pending

- Run the timeline's quest run again on the fixes, so the rows after the player's go have a scripted run as well as
  the hand-played one.
- The branches the hand-played run did not take: 20 seconds with no hit, lost seeds remade, the hut destroyed.
- A quest run with the seeds' strength (property 17) ported, to see the demo's shield and the three orbs' shields at
  their strength.
