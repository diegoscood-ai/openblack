# The Sea

A land 2 silver scroll at a hut in the Indian town on the shore: a mother's five sons have swum out into the bay and
won't come home. The player (in practice the creature, since the bay is shut to the player's hand) fetches them out of
the water, carries the mother down to the beach to scare them home, or kills them; each ending sets its own
alignment. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** a mother (Indian housewife) at her hut in Town2 (the Indian town by the bay) · **Script:** Baywatch · **Reward:** a "Big" creature miracle dispenser, only if all five sons walk home alive · **Repeatable:** no

Sources: the quest's script source (`Baywatch.txt`) and the land's control script, checked line by line against the
compiled form in the shipped `challenge.chl`; the shared helpers it runs (the scroll notify loop, the standard reminder,
the dispenser reward); the game's English text table; and the executable (read-only) for what a challenge log entry does
with its success and alignment, and how the script's random numbers are drawn. openblack's state is judged on this tree:
of the 64 script functions this quest and the scripts it starts need, none is a stub in `src/CHLApi.cpp` at this tree
(a villager's wandering place, the held checks, the health read and the challenge log are real), and the
land's control script never runs (see [../land_2.md](../land_2.md) row 1), so the quest never appears; every row below
is pending unless the notes say otherwise.

**Progress: 0/77 done, 0 chosen, 3 partial, 0 inferred — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher for this quest along with the land's other challenge watchers | pending | `LandControl2` has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)) |
| The watcher waits 4 minutes, then checks whether both Town6 (the Indian town near Lethys's snowy edge, which has no challenge of its own) and Town2 (the Indian town where the scroll appears) belong to the player; if not, it waits another 4 minutes and checks again, for ever | pending | the town owner read (`GetProperty` player) is real (`src/CHLApi.cpp:1818`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Once both towns are the player's, the quest starts once and the watcher ends; nothing can start it a second time | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Before the scroll appears, the bay around the swimming spot is ringed with an anti-influence area of radius 30, so the player's hand has no influence there (and so can't reach the children directly) | pending | `InfluencePosition` with the anti flag works (`src/Magic/Script/CHLInfluence.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A silver scroll appears on the ground at the mother's hut, at the hut's position on the cliff top above the bay (about 186 m inland of the swimming spot); its height isn't changed by the script | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the camera is within 100 of the scroll and the scroll is on screen, the evil advisor steps out, points at it and says "Hey. There's a job for you. Wanna do it?", at most once every 30 seconds and only when no film is playing | pending | the shared notify loop's natives work (`SpiritEject`, `SpiritPointPos`, `GameThingFieldOfView`, `RunText`, `DllGettime`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits until the scroll (or the hut spot) is clicked; the scroll is then switched on and the rest begins | pending | `GameThingClicked` and `SetActive` on a highlight work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| No mother and no children exist until the scroll is clicked: the mother (an Indian housewife) is made at the hut, and five Indian farmer boys are made at five spots close together in the bay | pending | `Create` makes villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A log entry is recorded during the introduction (see below), not when the scroll appears | pending | `Snapshot` is real |

## The children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each son is made as a grown farmer and then set to age 11, so he looks like a child | pending | the age property works for villagers (`SetProperty`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each son waits a random 1 to 4 seconds, then starts a looping swimming animation and turns to face a spot in the middle of the group | pending | `SetScriptState`, `SetScriptUlong` and `SetFocus` work for villagers, `Random` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A count of children in the water starts at 5 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A son lifted out of the water by the player's hand or the creature's hand leaves the in-water count (once, however long he is held) | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| Each son's script waits until he is no longer held by either hand and no longer flying through the air before deciding what happened | pending | the in-hand and flying properties are real (`src/CHLApi.cpp:1679`); not reached in play |
| Put down (or landing) alive on dry land (ground height above 0): he plays a lost-in-the-crowd animation once and says "All right I'm going home.", then walks back to the hut at half speed | pending | `GetLandHeight`, the script animations, `RunText` and `MoveGameThing` work for villagers; the held checks are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Put down (or landing) alive in the water (ground height 0 or below): he stays where he is, starts swimming again and rejoins the in-water count | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| A son who reaches within 5 of the hut alive counts as home and his script ends; dropping him by the hut counts at once | pending | `MoveGameThing` and `GetDistance` work for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A son's death is noticed whenever his health falls to 0 or below, wherever he is (swimming, walking home, after a throw); he counts as killed, leaves the in-water count if he was swimming, and his script ends | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| The first son to die (only the first) brings out the evil advisor: "That'll teach the kids a lesson." and then the good advisor: "How savage! Killing such innocents!" | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Nothing in the script drowns the children or makes them tire: they swim until fetched or killed, and the quest never ends by time | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Whether a son eaten by the creature counts as killed (health read as 0) or is lost track of is undetermined | pending | depends on what the health read gives for an eaten (removed) villager; not checked |
| Once a son is home, nothing more is watched for him: killing him later does not change the count | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen) starts with the script music "generic 3"; the mother is drawn in high detail | pending | `SetWidescreen`, `StartMusic`, `SetHighGraphicsDetail` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera glides over 3 seconds to the hut while the mother walks at half speed to a spot about 12 m from it | pending | `MoveCameraPosition`, `MoveCameraFocus`, `MoveGameThing` and the speed property work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| She plays a looking-for-something animation as the camera creeps in over 6 seconds | pending | the villager's script animation and the camera moves work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The mother: "My children have gone swimming, and they have not returned." | pending | `RunText` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The dialogue closes and the camera cuts to a low view over the water looking at the children | pending | `GameCloseDialogue`, `SetCameraPosition`, `SetCameraFocus` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The challenge log entry is recorded here, titled "The Sea", success 0, alignment 0, its picture taken from this view of the swimming boys; its reminder is the good advisor: "This woman's children have not returned from swimming." | pending | `Snapshot` is real |
| The mother (over the view of the boys): "I worry for their safety, and I'm too tired to find them." while the camera glides lower over the water for 6 seconds | pending | `RunText` and the camera moves work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera cuts back to the hut and pulls up and back over 4 seconds (its aim over 3); the mother's speed drops to a fifth | pending | the camera natives and the speed property work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The high detail and the music stop, the film ends, and the mother wanders around her spot (within 6 of it) | pending | `SetScriptStatePos` and `SetScriptFloat` and the wander round a place (state 199) are real (checked against v1.42 on 2026-10-20): the two whole-number settings (4 and 20) are the fewest and most turns it stands at each stop |
| From the end of the film, the children laugh: at the swimming spot, each time the script runs, a random one of seven child-laugh sounds plays with a chance of 1 in (250 ÷ swimmers + 1), so 1 in 51 with five swimming and rarer as fewer swim; the laughing stops for good once no child is in the water, even if one is later put back | pending | `PlaySoundEffect` and `RandomUlong` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The main watch starts 5 seconds after the film and runs until one of the endings below | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Bring each son out of the water and set him down on dry land so he walks home, or drop him straight by the hut; with the bay shut to the player's influence, the creature is the usual way to fetch them | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play; the unused line "Let's get the Creature over there to scoop them out." shows that intent |
| Or take the mother to the beach: while all five sons are still swimming, the mother within 40 of the swimming spot and held by neither the player nor the creature starts the beach ending | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| The beach ending is only open while every son is in the water: once any son has been taken out (and not put back) or killed, it can no longer happen | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Bringing the camera within 20 of the swimming spot with it on screen makes a son call out, once only: "We're having too much fun, we're not going anywhere." | pending | `GameThingFieldOfView` and `RunText` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Endings

Every ending records the challenge log's final state (success and alignment) for "The Sea". Updating an existing log
entry applies the given alignment to the player's alignment (scaled by a per-player factor that is not determined
here); success is kept between 0 and 1 and alignment between -1 and 1.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All five sons home and the mother alive: a film at the hut; the mother: "Thank you for bringing back my loved ones." | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| …the reward dispenser is put down quietly (no reward sting, no advisor talk, no help) and the camera flies over 5 seconds (its aim over 2) to look at it; the log entry is recorded again with a new picture of the dispenser: success 1, alignment +1, same reminder | pending | the dispenser can be made (`Create`, `src/Worship/SpellDispenser.cpp`) but `Snapshot` is real and the ending is not reached |
| Some sons home and the rest killed (one to four killed), no child left in the water, the mother alive: a film at the hut; log update success 1, alignment -0.2, no new picture; the mother: "I'll find where the others got to, I'm sure they'll be somewhere." No reward | pending | `UpdateSnapshot` is real; the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| All five sons killed and the mother alive: a film at the hut; log update success 1, alignment -0.6, no new picture; the mother: "So you couldn't find my loved ones. I'll have to search for myself."; then the evil advisor: "She'll be searching for a long time. Ha!" No reward | pending | `UpdateSnapshot` is real; the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| All five sons killed and the mother dead: no film, just dialogue; the evil advisor: "A whole family slaughtered. Nice going." The log is updated with the "details" form: success 1, alignment -1, title "The Sea", the current camera view passed but no new picture taken, and no reminder given | pending | `UpdateSnapshotPicture` is real; the source comment quotes the line as "A whole family slaughtered, nice going!"; whether the old reminder is cleared by passing none is not determined |
| Every son accounted for (home or killed, at least one home) but the mother dead: a film at the hut; log update success 1, alignment -0.8, no new picture; the good advisor: "Oh how terrible. Orphans." No reward | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play; `UpdateSnapshot` is real |
| Beach ending, mother alive: a film at the beach; a son: "Quick, run! It's mom!"; the screen fades to black over 2 seconds | pending | `SetFade` and `FadeFinished` work; the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| …in the dark the mother is put back at her spot and every son (all are alive, since all five were swimming) is put around her, each in an idle animation (two plain idles, a lost look, an unimpressed look); she turns to face the camera at the hut; fade in over 2 seconds | pending | `SetPosition` and the villagers' script animations work; never reached |
| …the mother: "Thank you for bringing back my loved ones."; the log entry is recorded again with a new picture of the family: success 1, alignment +0.8; the camera pulls up and back over 4 seconds | pending | `Snapshot` is real; no reward is given on this ending |
| Beach ending, mother dead (her body within 40 of the spot while all five swim): a film at the beach; the woman's voice: "Aaahhh!"; fade to black over 2 seconds | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play; the source comment writes it "AAaahhh !!!!" |
| …her body is put back at her spot lying dead, and the five sons are put around her, each facing her: two inspecting her, one mourning, one overworked, one scared stiff; fade in; the log entry is recorded again with a new picture of this scene: success 1, alignment -0.5; the camera pulls up and back over 4 seconds | pending | `Snapshot` is real; never reached |
| The watch ends with the first ending reached; the children's scripts and the laughing are then stopped | pending | `StopScript` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A quest with sons still in the water simply waits: there is no time limit, no failure ending and no way to abandon it | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reward is a miracle dispenser for the creature miracle "Big", built (as a Norse dispenser building) on the cliff top between the hut and the bay, about 74 m from the hut, turned 180°, and switched on | partial 60% | `CreateWithAngleAndScale` makes spell dispensers (`src/Worship/SpellDispenser.cpp`), `SetActive` works on them; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../rewards.md](../rewards.md) and [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| The dispenser's refill time is set to 0 seconds (the helper always sets it, because it tests the game time rather than the given refill time) | pending | `SetTimerTime` works; what a 0 refill time means for the dispenser is not determined; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| It is given quietly: no reward sting, no camera fly-to from the helper, no first-dispenser tip or signpost, no help lines, and it does not count towards the land's dispenser-reward tally | pending | the quest's own camera flies to it instead; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Only the all-home, mother-alive ending gives it; the equally good beach ending (+0.8) gives nothing | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mother and the surviving sons stay in the world | pending | they are never released from the script; whether they go back to normal village life is not determined |
| The anti-influence ring over the bay and the scroll are never removed by the script | pending | whether the game removes them when the challenge ends is not determined |
| The reward dispenser stays where it was put | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor brings the job to the player's notice (the scroll-notify line) | pending | the notify line's natives work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log's reminder is the good advisor's "This woman's children have not returned from swimming." | pending | the shared reminder script's natives work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The first killing: evil "That'll teach the kids a lesson.", good "How savage! Killing such innocents!" | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| All sons killed, mother alive: evil "She'll be searching for a long time. Ha!"; whole family dead: evil "A whole family slaughtered. Nice going."; orphans: good "Oh how terrible. Orphans." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the script music "generic 3" and stops it at its end | partial 60% | `StartMusic`/`StopMusic` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| The children's laughter (seven samples, from the script sound bank) is heard at the swimming spot while any child swims | partial 60% | `PlaySoundEffect` works (`src/Audio`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| The endings have no music or sound of their own | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A son held in the creature's hand counts the same as one in the player's hand; where the creature puts him down decides whether he walks home or swims again | pending | `InCreatureHand` is real since 2026-10-21 |
| The mother in the creature's hand does not start the beach ending until the creature lets go | pending | `InCreatureHand` is real since 2026-10-21 |
| A son the creature kills counts as killed (whether eating counts is not determined, see above) | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll-notify helper is called with four values (no notify distance), so it uses the shared 100 | pending | the cut Big Fish quest calls it with five |
| The all-home, mother-alive ending and the beach ending record the log with "snapshot challenge" a second time instead of "update snapshot"; the game treats it as an update with a new picture | pending | `Snapshot` is real |
| The beach ending checks each son's health before gathering him, but it can only start with all five swimming, so all five are always alive there | pending | dead checks |
| A son killed with others still swimming is not followed by any ending until every son is home or dead, so a single son left swimming holds the quest open for ever | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the mother dies with all five sons home, the orphans ending is used (alignment -0.8), not the thanks ending | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The "Aaahhh!" of the dead-mother beach ending is in the woman's voice although the mother is dead | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The main watch and the laughing loop have no wait, so they run every time scripts run | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A son dropped on dry land somewhere he can't walk home from never counts as home, which also holds the quest open | pending | the walk home is ordered once and never repeated |
| The dispenser helper's refill check tests the game time instead of its refill argument, so the refill time is always set | pending | shared helper |
| A testing script that sets up land 2 and runs this quest at once exists in the sources but is not in the shipped program | pending | not in the shipped program |

