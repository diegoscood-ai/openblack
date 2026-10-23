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
every row below is pending unless the notes say otherwise.

**Progress: 0/72 done, 0 chosen, 3 partial, 0 inferred — 3%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the Worship Site lesson ends, Khazar's hand flies to a spot just south of the player's worship site (about 75 m from the altar) and the "Miracle Challenge" scroll is started there; the lesson does not wait for it | pending | `MoveComputerPlayerPosition` is real; see [worship_site.md](worship_site.md) |
| Nothing else in the land waits for either miracle challenge to be finished: no flag is set on completion, and the land's later quests (The Workshop, Impress Village, the last scroll) are started by other conditions | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| What it unlocks: only the Fireball miracle at the home village's worship site; the same miracle also comes with Town10 (an Indian town that starts with a fire miracle) when it is won | pending | `Land2.txt` gives town 10 a fire miracle and our map loading reads it; the challenge's own gift is never reached |
| An unfinished challenge holds Khazar "busy" for as long as it runs; the land's control script will not start Khazar's death while he is busy, so leaving this challenge half done holds back the end of the land (see Script quirks) | pending | Khazar's death needs computer-player natives (real now; not checked in play) |

## The Miracle Challenge scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears 6 m up in the air at the spot Khazar's hand went to, just south of the worship site | pending | `CreateHighlight` and the height through `SetProperty` (YPos) are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A two-minute timer starts as soon as the scroll is placed; it decides Khazar's first line later | pending | `CreateTimer` is real (`src/ECS/ScriptTimer`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the camera is within 100 of the scroll and the scroll is on screen, the evil advisor steps out, points at it and says "Khazar's left us a Miracle Challenge, to expand our knowledge!", at most once every 30 seconds and only when no film is playing | pending | the notify loop calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The loop ends when the scroll or the spot under it is clicked, or when Khazar has died; either way the scroll is then switched to its active look | pending | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If Khazar has already died when the loop ends, nothing more happens: neither challenge is started and the two miracles are never taught this way | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The scroll is never deleted by the script; it goes when the scroll's script ends (scripts' own objects are cleared with them; inferred, as for the other scrolls) | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Khazar's tour (introduction)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking it starts both challenges at once, each with its own scroll (this one on the target huts, the other on the island hut), and then plays a film | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's music starts | pending | `StartMusic` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's hand flies fast back to the worship site; the camera rises above it over 5 seconds looking west, then pans slowly east over 12 seconds | pending | the camera moves are real, but Khazar's hand flight (`MoveComputerPlayerPosition`) and his wait (`ComputerPlayerReady`) |
| Khazar speaks: if the scroll was clicked within two minutes of being placed, "You are keen to learn the secret of casting Miracles. I appreciate your enthusiasm."; otherwise "You are ready to learn the secret of using Miracles." | pending | `GetTimerTimeRemaining` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "Come with me. I have a set up an area where you can learn more." (the text table's own wording); two seconds later the camera lifts over the temple and his hand drifts off east | pending | real camera and text natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Scroll drawing is switched on, so the two new challenge scrolls show (inferred from where it is switched) | pending | `SetDrawHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| His hand flies to the island hut of the shield challenge; the camera moves in over 7 seconds and he says "Here you can learn about defence." | pending | Khazar's hand flight is real (`MoveComputerPlayerPosition`); not checked in play; the camera and text are real |
| His hand flies on to the middle target hut of this challenge; the camera follows over 7 seconds and he says "And here you can learn to attack." | pending | Khazar's hand flight is real (`MoveComputerPlayerPosition`); not checked in play; the camera and text are real |
| The camera pulls back over 4 seconds to look over the huts from the west and Khazar says "Activate the Scroll of your choice." | pending | real camera and text natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar's hand goes home to his own temple, the music stops, the film ends and Khazar is released | pending | `ReleaseComputerPlayer` is real; `StopMusic` is real |

## How the fireball scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The targets are the three houses of a small empty Norse village on the south-east shore, a neutral village the map makes uninhabitable for the challenge | partial 70% | the houses are made by our map loading (`AbodeArchetype`); the uninhabitable flag is not (see [../../scripts/land2_script.md](../../scripts/land2_script.md)); re-measured 2026-10-21: the houses are made; the uninhabitable flag is not |
| As soon as the challenge is started (at the tour, before its scroll is clicked) an anti-influence ring of radius 50 is laid round the first hut, so the player's hand can't reach in and must throw | pending | `InfluencePosition` is real (`src/Magic/Script/CHLInfluence.cpp`); whether it makes an anti-influence ring was not checked; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A gold scroll appears on the middle hut | pending | `CreateHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Notify: while the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points and says "We gotta have a go at this Fireballing. Boss. We gotta!", at most once every 30 seconds and only outside films | pending | the notify loop calls real natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits for the scroll or the middle hut to be clicked; if Khazar has died first the scroll is made active and nothing more happens | pending | `GameThingClicked` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click, a ring of influence of radius 40 is made where the seeds will lie, so the player can pick them up | pending | `InfluencePosition` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| All three huts are mended to full health | pending | a building's health (property 1) is set (`SetProperty` HEALTH) |
| The player's control is cut down to the hand only for the whole challenge | pending | `SetInterfaceInteraction` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A film starts; Khazar's music plays and Khazar counts as busy | pending | `StartMusic` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera swings over 4 seconds to look along the shore at the huts while Khazar's hand flies fast to them | pending | the camera is real, but Khazar's hand flight is real (`MoveComputerPlayerPosition`); not checked in play |
| Khazar: "Behold three huts. We shall use these as our targets." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The log entry "Khazar's Fireball Challenge" is recorded at 0 with the reminder "Try and burn one of these huts with the Fireball Miracle seeds." | pending | `Snapshot` is real |
| Khazar's hand flies slowly back inland and three Fireball one-shot seeds appear on a hillside about 270 m from the huts, 2 m apart in a diagonal line | pending | `CREATE` of one-shot seeds is real (`magic::script::CreateOneShotSpell`), but Khazar's hand flight is real (`MoveComputerPlayerPosition`); not checked in play |
| The camera moves over 4 seconds to just behind the seeds, looking out to the huts | pending | real camera natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "Here are some fireballs to use." while his hand drifts down to hover by the seeds | pending | Khazar's hand is real (the computer player's hand natives); not checked in play; `RunText` is real |
| Khazar's fireball hand demo plays (a fire seed is put in the hand): "You hold the Action Button down. Like this.", pausing for the hand; "You then move the Hand to put momentum on the fireball.", pausing again; "You release the Action Button to cast the fireball."; then "Devastating! Fire spreads and causes serious damage." | pending | `PlayHandDemo` and `HandDemoTrigger` are real (`src/Input/HandDemo.h`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First the quest waits for the player to hold a fire miracle: until then, every 30 seconds (the first at once) the good advisor says "Tap the Fireball Miracle." | pending | `GetObjectHeld` (the player's hand) and `IsOfType` are real; not checked in play |
| The check looks for a fire spell seed in the hand, not the one-shot globe itself (undetermined: whether picking up the globe alone counts, or only once tapped) | pending | the in-hand check (`GetObjectHeld`, `IsOfType`) is real; not checked in play |
| The first wait also ends if all three seeds are gone without one being seen in the hand (for instance thrown at once); then the advisor's throwing tip is skipped | pending | the seed counts use real natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 15 m from the first seed's spot fades out and is made again at its own spot | pending | real field-of-view, distance, delete and create natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Once a fire miracle is held, the evil advisor steps out: "Now throw it at a hut like you'd throw a rock." | pending | follows from the in-hand check, which is real; not checked in play |
| Any hut that drops below full health counts as hit, once; hits add up over the whole challenge | pending | a building's health (property 1) is read (`GetProperty` HEALTH, real); not yet reached in play (`LOAD_MAP` changes land now, not yet checked) |
| A round ends at once if all three huts are hit; otherwise when all the seeds are used up: the quest waits 1 second, then until the hand is empty, then 6 seconds more, and counts the huts one last time | pending | the hit test's health read (`GetProperty` HEALTH) and the empty-hand wait (`GetObjectHeld`) are real; not checked in play |
| The throw itself (fireball flight, bouncing, setting the huts alight and fire spreading) is the miracle's normal behaviour | partial 80% | the fireball miracle works in our tree ([../../miracles/fireball.md](../../miracles/fireball.md)); the challenge never runs; re-measured 2026-10-21: the miracle works; the challenge never runs |

## Results and retries

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After each round a short film shows the huts from the shore, the camera drifting closer over 10 seconds (8 after a miss), then snaps back to where the player was | pending | real camera natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| All three huts hit: the log entry is set to 1 and the evil advisor says "Nice aiming, Boss. You damaged all three huts." and "Fire spreads, you see. A Village can be torched in minutes." | pending | the hit count reads health (`GetProperty` HEALTH, real) and the challenge record is real (`Snapshot`, `UpdateSnapshot`); not checked in play |
| Two huts hit: "Pretty nifty. Two of the three huts are damaged." then the same fire-spreads line; log 1 | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| One hut hit: "Oh yeah! You hit one of the huts." then the fire-spreads line; log 1 | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| Any hit ends the challenge; there is no reward difference between one hut and three | pending | the hit count reads health (`GetProperty` HEALTH), which is real; not checked in play |
| No hut hit: the log entry's success goes up by a third, and the evil advisor says "Oh man. None of the huts got toasted." after the first miss, "You still haven't managed to hit anything." after the second and third | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`); the hit test reads health, which is real; not checked in play |
| After a miss three new seeds appear at the same spots; Khazar says "Have some more practice. Here are more Fireballs." the first time and "Try again." the second | pending | `CREATE` of the seeds and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The player gets three rounds at most; after the third miss the challenge ends as if passed | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| There is no failure: the challenge always ends with the reward | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The music stops, the log entry is set to 1 and the player's full control comes back | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`); `StopMusic` and `SetInterfaceInteraction` are real |
| In a dialogue (not a film) the Fireball miracle (first level) is given to the player's home village, so its worship site can pray for it | pending | `SetMagicInObject` is real, but the home town comes from `GetTownWithId`, real too; not checked in play; see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Khazar: "My Fireball Challenge is complete.", "I have given the knowledge of the Fireball Miracle to your Village.", "From now on your people can worship for this Miracle at your Worship Site." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The evil advisor: "Wow. We can create these now. Bet they'll impress non-believers!" | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar is released and no longer busy; any seeds left fade away | pending | `ReleaseComputerPlayer` is real; `ObjectDelete` is real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The huts stay as damaged as the player left them (burning huts burn on); nothing rebuilds them | pending | the same script; fire burns on in our tree; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The fireball miracle stays at the home village's worship site for the rest of the land | pending | follows from the gift, which needs the home town (`GetTownWithId` is real; not checked in play) |
| The influence ring by the seeds and the anti-influence ring round the huts are never removed by the script (undetermined: whether they go when the script ends) | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor owns the notifications and the results; the good advisor owns the log reminder and the tap tip | pending | `SpiritEject` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Clicking the log entry speaks its reminder "Try and burn one of these huts with the Fireball Miracle seeds." through the good advisor, who steps out to say it | partial 60% | the shared reminder calls real natives; the challenge record is real (`Snapshot`, `UpdateSnapshot`); re-measured 2026-10-21: the script calls only natives that work; not checked in a run yet |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays through the tour, and again from the challenge's click until the reward | pending | `StartMusic` and `StopMusic` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The tour stops the music at its end; the challenge stops it only after the last round | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts stops it from fetching or throwing the seeds (undetermined what happens if it does) | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar counts as busy from the click until the reward; Khazar's death only starts while he is not busy, so a challenge clicked and then ignored (seeds never used) holds back Khazar's death, and with it Lethys's vortex and the end of the land, until it is finished | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The busy flag is shared: the shield challenge (or any other Khazar lesson) finishing clears it even while this one is still running, so Khazar's death can then start in the middle of this challenge | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| The first hut is looked up at the village's position, about 5 m from that house's own position (undetermined: whether the lookup finds it; if not, that hut can't count as hit) | pending | `CallNear` is real, so the same lookup would be made; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A round with seeds left never ends while the player holds something: the wait for an empty hand has no time limit | pending | the empty-hand wait reads `GetObjectHeld`, which is real; not checked in play |
| The three hit results all set the log to 1 before the last line; a miss raises it by a third, so after three misses it reaches 0.99 before the final 1 | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The two "no hut hit" lines after the second and third misses are the same line | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

