# The Workshop

A gold scroll on the second land: once the workshop Khazar planned in the player's village is built, an engineer
"sent by Khazar" appears there, explains scaffolds and asks for wood; the player feeds the workshop until it makes a
scaffold, places it to plan a house, and builds homes until the village's homeless are housed. The engineer then joins
the village and gives the player the Forest miracle. The land as a whole is in [../land_2.md](../land_2.md), the
land's script in [../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** an engineer sent by Khazar, at the workshop in the player's home village · **Script:** TheWorkshop · **Reward:** the Forest miracle for the home village (and the engineer joins the village) · **Repeatable:** no

Sources: the quest's script source (`TheWorkshop.txt`), the shared scroll notifier (`ChallengeNotify.txt`), the standard
reminder and the info sign scripts, the land's control script (`LandControl2.txt`) and the lesson that plans the
workshop (`LearnWorshipping.txt`), checked against the compiled scripts in the shipped `challenge.chl`; the game's
English text table (`InfoScript2.txt`). openblack's state is judged on this tree: of the 58 script functions the quest
and the scripts it runs need, 8 only logged "not implemented" in `src/CHLApi.cpp` when they were counted (among them
resources in a building, the held object, a special effect and the challenge log; the resources, the held object and
the challenge log are real now, the beam `AddSpotVisualTargetObject` still logs it). openblack never runs Land
2's control script at all (see [../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never
appears; every row is pending unless the notes say otherwise.

**Progress: 0/62 done, 0 chosen, 1 partial, 0 inferred — 1%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the end of Khazar's worship lesson ([worship_site.md](worship_site.md)) his hand carries a workshop scaffold (size 3) to a plot in the player's home village, planning the workshop | pending | the scaffold (`CREATE`, `SetScaffoldProperties`) and its plan turned to 160 (`SET_PROPERTY` ANGLE: `chlapi::SetAngleByScript`) and Khazar's hand carrying it (`ForceComputerPlayerAction`) are real; not checked in play; see [../../building/workshop_and_scaffolds.md](../../building/workshop_and_scaffolds.md) |
| The control script (checking every 9 seconds) starts the quest's watcher once the worship lesson is done, and only once | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The watcher looks for a building within 10 of the workshop's spot; while there is none it looks again at once (radius 5) with no pause; once there is one it checks every 5 seconds whether it is fully built and the player's | pending | `CallNear` and the built percentage (property 22) are real; the owner read (property 21) is real for an abode (its town's owner); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the workshop is finished and the player's, the quest proper starts and the watcher ends; it also ends, without the quest, if Khazar is gone | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A gold scroll appears in front of the workshop | pending | `CreateHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points at it and says " Hey! What is this guy doing?", at most once every 30 seconds and only when no film is playing | pending | the notifier calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The engineer is made at the workshop and walks out to stand in front of it, before the scroll is even clicked | pending | `CREATE` makes villagers and `MoveGameThing` moves them; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits until the scroll (or its spot) is clicked; the scroll is then switched on | pending | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film with generic quest music; the engineer is drawn in high detail; the camera moves in close and waits for him to reach his spot, then 2 seconds | pending | `SetWidescreen`, `StartMusic`, `SetHighGraphicsDetail` and the camera are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He turns to the camera and beckons as the camera eases in to a close-up over 12 seconds | pending | `SetFocus` and `OverrideStateAnimation` on villagers are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Engineer: "Khazar sent me. I can help you build your settlement up." while he gossips, then idles | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The dialogue closes and the log entry "The Workshop" is recorded at 0, with the good advisor's reminder "The man in the workshop wants some wood." | pending | `Snapshot` is real |
| Engineer: "People are sleeping outdoors as they don't have enough buildings to live in." while he looks around for something, then gossips twice | pending | `RunText` and villager animations are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Engineer: "This is the Workshop. I make Scaffolding here." then "It allows you to decide where new buildings should be constructed." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Engineer: "If you can supply me with enough wood, I can build a Scaffold so that you can plan more houses." (the player may act while it is read), with an overworked gesture | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He goes back into the workshop, the camera returns to where it was before the film, his detail drops and the music stops | pending | `StopMusic`, `SetHighGraphicsDetail` and the camera are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Supplying the wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the film, all wood already in the workshop is taken away and any scaffold already beside it (within 10) fades away, so the player starts from nothing | pending | `GetResource`, `RemoveResource` and `ObjectDelete` are real (`src/CHLApi.cpp`); not yet reached in play (Land 2's story is not run that far) |
| The engineer comes out to a spot in front of the workshop, can't be picked up or moved, and idles there | pending | the flags and the villager moves are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each second the workshop's wood is checked; whenever it has gone up he cheers | pending | the workshop's wood is read with `GetResource` (real, `src/CHLApi.cpp`; our workshops: `src/ECS/Town/Workshops`); not yet reached in play (Land 2's story is not run that far) |
| The step ends when the workshop's wood has fallen by 800 or more within one second, which is the workshop using its wood up to make a scaffold | pending | the wood read (`GET_RESOURCE`, `ecs::object_resources`) is real; not checked in play |
| There is no time limit and no reminder other than the log entry's | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The scaffold is made

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film: the engineer, in high detail, saws wood as the camera closes in on him | pending | real villager, camera and detail natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Engineer: "Give me a little time and I'll turn the wood into Scaffolds." (one line), facing the camera with a gossip | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry is recorded at 0.1 with the good advisor's reminder "We're just waiting for the man to create a Scaffold." | pending | `UpdateSnapshot` is real |
| The camera returns to where it was and the film ends | pending | real camera natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest checks every 0.3 seconds for a scaffold within 10 of the workshop; when one appears the engineer goes back inside | pending | `CallNear` is real; our tree has workshops and scaffolds (`src/ECS/Town/Workshops`, `src/ECS/Scaffolds.cpp`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| It then waits until the camera is within 50 of the workshop and the workshop is on screen; the engineer is removed | pending | `ObjectDelete` and the camera reads are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor: "It looks like your Scaffolding is ready." then "We just need to move it to an area where we want it built.", stepping out, then points at the scaffold: "We need to place a Scaffold down on the ground." (the player may act meanwhile) | pending | `SpiritPointGameThing` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry is updated to 0.2 with that last line as its reminder | pending | `UpdateSnapshot` is real |

## Placing the scaffold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits, checking without pause, until the player is holding a scaffold (any scaffold) or the new scaffold is more than 5 from where it was made | pending | `GetObjectHeld` (the player's hand) and `IsOfType` are real; not checked in play |
| Good advisor (one line): "When you're happy with its location and angle, click the Action Button." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits until the thing that was in the hand is no longer held | pending | the held object read (`GetObjectHeld`) is real; not checked in play |
| Both advisors step out: good "Oh, this is such a worthwhile cause, planning homes to be built for the homeless." · evil "There is another way to solve this homeless problem." · good "Oh yes?" · evil "Yeah. Kill 'em." | pending | `SpiritEject` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A bronze info sign about the workshop appears beside it (dropping wood, the flag showing the wood needed, the smoking chimney, the horn, room for three scaffolds, combining up to seven) | pending | the info sign script and `CreateHighlight` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry is updated to 0.3 with the good advisor's reminder "There are still homeless people in the Village. You need to build more houses." | pending | `UpdateSnapshot` is real |

## Housing the homeless

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The home town's room for adults is noted at this moment | pending | `ObjectAdultCapacity` is real (the town's adult places in its homes, `ecs::script_containers::AdultCapacity`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Every 4 seconds the town's adults (counted as at most 40) are compared with its room for adults, giving a homeless share | pending | `IdAdultSize` and `ObjectAdultCapacity` are real (`ecs::script_containers::AdultSize`, `AdultCapacity`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../town/growth_and_housing.md](../../town/growth_and_housing.md) |
| The step ends when fewer than 40% are homeless, or the town has no adults, or its room for adults has grown by 10 or more since it was noted | pending | the counts are real, so the test now takes the play's branch (before, both counts were 0 and it ended at once on "no adults"); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Wiping out the homeless (as the evil advisor suggests) also ends it, because the homeless share falls | pending | the counts are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The engineer's reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second gold scroll appears at the workshop; the evil advisor nags it: "Hey the man, he wanna talk to us more!" | pending | `CreateHighlight` and the notifier are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits for it to be clicked | pending | `GameThingClicked` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A widescreen film: the engineer is made again at the workshop, in high detail, and walks out in front; the camera closes in | pending | real villager, camera and detail natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Engineer: "Your Village now truly has more room for people. Building more will encourage growth." with an impressed gesture · "This is a Village I want to be part of." · "In my own Village I held the knowledge of the forest Miracle. Follow me!" pointing at the worship site's miracle | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Epic music starts; he runs to the worship site at 0.6 speed; after 6 seconds he is put part of the way up the hill and walks on | pending | `StartMusic`, `SetPosition` and villager moves are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| At the worship site he faces the miracle icon and plays a summoning animation; a magic beam joins the miracle icon to him | pending | `SpecialEffectObject` is real, but the beam `AddSpotVisualTargetObject` is a stub |
| The Forest miracle is given to the home town, so it can be charged at its worship site and village centre (the command is given twice, six seconds apart) | pending | `SetMagicInObject` is real (`src/Magic/Script/CHLWorship.cpp`); see [../../miracles/forest.md](../../miracles/forest.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry "The Workshop" is recorded at 1 (complete), still with the reminder about homeless people | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Engineer: "Constant worship is needed to maintain it, but the wood it provides is a blessing to the people." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The beam goes, the engineer joins the home town, the camera pulls back over the temple and the music stops | pending | `StopMusic` and the camera are real, but joining the town needs the town (`GetTownWithId` is real; not checked in play) |
| He walks back to the workshop, the scroll goes, and after 30 seconds he is let go to live as an ordinary villager | pending | villager moves, `ObjectDelete` and `ReleaseFromScript` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The workshop, the planned houses and the Forest miracle stay; the engineer stays in the village | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nothing else in the story waits on this quest: Impress Village is timed from the worship lesson, not from here | pending | the same script; see [impress_village.md](impress_village.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Failing and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no failure and no time limit; each step waits for ever | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If Khazar is killed before the workshop is built, the quest never starts; once started it carries on without him | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| If the workshop is never built (or is destroyed before being finished), the quest never starts; nothing else depends on it, so there is no soft-lock | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scrolls are nagged by the evil advisor; every log reminder is the good advisor's | partial 60% | the notifier and reminder call real natives; the challenge record is real (`Snapshot`, `UpdateSnapshot`); re-measured 2026-10-21: the script calls only natives that work; not checked in a run yet |
| Music: generic quest music for the introduction, the epic theme for the reward | pending | `StartMusic` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part; it may carry wood to the workshop like the player, since the quest only watches the workshop's wood | pending | follows from the wood read, which is real; not checked in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The watcher's search for a missing workshop and the wait for the scaffold to be picked up loop without any pause | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The wood step watches for wood being used up (a fall of 800 in a second), not for wood being given; feeding the workshop slowly still works because the drop comes when the scaffold is made | pending | the wood read is real; not checked in play |
| The Forest miracle is enabled twice | pending | `SetMagicInObject` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The town's adults are capped at 40 when working out the homeless share, so a town of 40 or more adults only needs room for more than 24 of them, however big it is | pending | the counts are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

