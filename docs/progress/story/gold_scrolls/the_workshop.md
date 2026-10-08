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
and the scripts it runs need, 10 still only log "not implemented" in `src/CHLApi.cpp` (among them resources in a
building, the town's adult counts, the held object, a special effect and the challenge log). openblack never runs Land
2's control script at all (see [../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never
appears; every row is todo unless the notes say otherwise.

**Progress: 0/62 done, 1 partial — 1%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the end of Khazar's worship lesson ([worship_site.md](worship_site.md)) his hand carries a workshop scaffold (size 3) to a plot in the player's home village, planning the workshop | todo | the scaffold (`CREATE`, `SetScaffoldProperties`) is real, but Khazar's hand carrying it (`ForceComputerPlayerAction`) is a stub; see [../../building/workshop_and_scaffolds.md](../../building/workshop_and_scaffolds.md) |
| The control script (checking every 9 seconds) starts the quest's watcher once the worship lesson is done, and only once | todo | never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The watcher looks for a building within 10 of the workshop's spot; while there is none it looks again at once (radius 5) with no pause; once there is one it checks every 5 seconds whether it is fully built and the player's | todo | `CallNear` and the built percentage (property 22) are real; the owner read (property 21) is not handled by `GetProperty`; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| When the workshop is finished and the player's, the quest proper starts and the watcher ends; it also ends, without the quest, if Khazar is gone | todo | plain script; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| A gold scroll appears in front of the workshop | todo | `CreateHighlight` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| While the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points at it and says " Hey! What is this guy doing?", at most once every 30 seconds and only when no film is playing | todo | the notifier calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The engineer is made at the workshop and walks out to stand in front of it, before the scroll is even clicked | todo | `CREATE` makes villagers and `MoveGameThing` moves them; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The quest waits until the scroll (or its spot) is clicked; the scroll is then switched on | todo | `GameThingClicked` is real and `SetActive` handles highlights; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film with generic quest music; the engineer is drawn in high detail; the camera moves in close and waits for him to reach his spot, then 2 seconds | todo | `SetWidescreen`, `StartMusic`, `SetHighGraphicsDetail` and the camera are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| He turns to the camera and beckons as the camera eases in to a close-up over 12 seconds | todo | `SetFocus` and `OverrideStateAnimation` on villagers are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Engineer: "Khazar sent me. I can help you build your settlement up." while he gossips, then idles | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The dialogue closes and the log entry "The Workshop" is recorded at 0, with the good advisor's reminder "The man in the workshop wants some wood." | todo | `Snapshot` is a stub |
| Engineer: "People are sleeping outdoors as they don't have enough buildings to live in." while he looks around for something, then gossips twice | todo | `RunText` and villager animations are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Engineer: "This is the Workshop. I make Scaffolding here." then "It allows you to decide where new buildings should be constructed." | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Engineer: "If you can supply me with enough wood, I can build a Scaffold so that you can plan more houses." (the player may act while it is read), with an overworked gesture | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| He goes back into the workshop, the camera returns to where it was before the film, his detail drops and the music stops | todo | `StopMusic`, `SetHighGraphicsDetail` and the camera are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## Supplying the wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the film, all wood already in the workshop is taken away and any scaffold already beside it (within 10) fades away, so the player starts from nothing | todo | `GetResource` and `RemoveResource` are stubs; `ObjectDelete` is real |
| The engineer comes out to a spot in front of the workshop, can't be picked up or moved, and idles there | todo | the flags and the villager moves are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Each second the workshop's wood is checked; whenever it has gone up he cheers | todo | the workshop's wood read `GetResource` is a stub (our workshops: `src/ECS/Town/Workshops`) |
| The step ends when the workshop's wood has fallen by 800 or more within one second, which is the workshop using its wood up to make a scaffold | todo | the wood read is a stub, so the step never ends |
| There is no time limit and no reminder other than the log entry's | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## The scaffold is made

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film: the engineer, in high detail, saws wood as the camera closes in on him | todo | real villager, camera and detail natives; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Engineer: "Give me a little time and I'll turn the wood into Scaffolds." (one line), facing the camera with a gossip | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The log entry is recorded at 0.1 with the good advisor's reminder "We're just waiting for the man to create a Scaffold." | todo | `UpdateSnapshot` is a stub |
| The camera returns to where it was and the film ends | todo | real camera natives; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The quest checks every 0.3 seconds for a scaffold within 10 of the workshop; when one appears the engineer goes back inside | todo | `CallNear` is real; our tree has workshops and scaffolds (`src/ECS/Town/Workshops`, `src/ECS/Scaffolds.cpp`); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| It then waits until the camera is within 50 of the workshop and the workshop is on screen; the engineer is removed | todo | `ObjectDelete` and the camera reads are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Good advisor: "It looks like your Scaffolding is ready." then "We just need to move it to an area where we want it built.", stepping out, then points at the scaffold: "We need to place a Scaffold down on the ground." (the player may act meanwhile) | todo | `SpiritPointGameThing` and `RunText` are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The log entry is updated to 0.2 with that last line as its reminder | todo | `UpdateSnapshot` is a stub |

## Placing the scaffold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits, checking without pause, until the player is holding a scaffold (any scaffold) or the new scaffold is more than 5 from where it was made | todo | `GetObjectHeld` and `IsOfType` are stubs |
| Good advisor (one line): "When you're happy with its location and angle, click the Action Button." | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The quest waits until the thing that was in the hand is no longer held | todo | the held object read is a stub |
| Both advisors step out: good "Oh, this is such a worthwhile cause, planning homes to be built for the homeless." · evil "There is another way to solve this homeless problem." · good "Oh yes?" · evil "Yeah. Kill 'em." | todo | `SpiritEject` and `RunText` are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| A bronze info sign about the workshop appears beside it (dropping wood, the flag showing the wood needed, the smoking chimney, the horn, room for three scaffolds, combining up to seven) | todo | the info sign script and `CreateHighlight` are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The log entry is updated to 0.3 with the good advisor's reminder "There are still homeless people in the Village. You need to build more houses." | todo | `UpdateSnapshot` is a stub |

## Housing the homeless

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The home town's room for adults is noted at this moment | todo | `ObjectAdultCapacity` is a stub |
| Every 4 seconds the town's adults (counted as at most 40) are compared with its room for adults, giving a homeless share | todo | `IdAdultSize` is a stub; see [../../town/growth_and_housing.md](../../town/growth_and_housing.md) |
| The step ends when fewer than 40% are homeless, or the town has no adults, or its room for adults has grown by 10 or more since it was noted | todo | the counts are stubs |
| Wiping out the homeless (as the evil advisor suggests) also ends it, because the homeless share falls | todo | the counts are stubs |

## The engineer's reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second gold scroll appears at the workshop; the evil advisor nags it: "Hey the man, he wanna talk to us more!" | todo | `CreateHighlight` and the notifier are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The quest waits for it to be clicked | todo | `GameThingClicked` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| A widescreen film: the engineer is made again at the workshop, in high detail, and walks out in front; the camera closes in | todo | real villager, camera and detail natives; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Engineer: "Your Village now truly has more room for people. Building more will encourage growth." with an impressed gesture · "This is a Village I want to be part of." · "In my own Village I held the knowledge of the forest Miracle. Follow me!" pointing at the worship site's miracle | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Epic music starts; he runs to the worship site at 0.6 speed; after 6 seconds he is put part of the way up the hill and walks on | todo | `StartMusic`, `SetPosition` and villager moves are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| At the worship site he faces the miracle icon and plays a summoning animation; a magic beam joins the miracle icon to him | todo | `SpecialEffectObject` is real, but the beam `AddSpotVisualTargetObject` is a stub |
| The Forest miracle is given to the home town, so it can be charged at its worship site and village centre (the command is given twice, six seconds apart) | todo | `SetMagicInObject` is real (`src/Magic/Script/CHLWorship.cpp`); see [../../miracles/forest.md](../../miracles/forest.md); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The log entry "The Workshop" is recorded at 1 (complete), still with the reminder about homeless people | todo | the log entry is a stub |
| Engineer: "Constant worship is needed to maintain it, but the wood it provides is a blessing to the people." | todo | `RunText` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The beam goes, the engineer joins the home town, the camera pulls back over the temple and the music stops | todo | `StopMusic` and the camera are real, but joining the town needs the town (`GetTownWithId` is a stub) |
| He walks back to the workshop, the scroll goes, and after 30 seconds he is let go to live as an ordinary villager | todo | villager moves, `ObjectDelete` and `ReleaseFromScript` are real; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The workshop, the planned houses and the Forest miracle stay; the engineer stays in the village | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Nothing else in the story waits on this quest: Impress Village is timed from the worship lesson, not from here | todo | the same script; see [impress_village.md](impress_village.md); never reached: Land 2 needs `LOAD_MAP`, an empty native |

## Failing and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no failure and no time limit; each step waits for ever | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| If Khazar is killed before the workshop is built, the quest never starts; once started it carries on without him | todo | Khazar's death needs computer-player natives (stubs) |
| If the workshop is never built (or is destroyed before being finished), the quest never starts; nothing else depends on it, so there is no soft-lock | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## Advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scrolls are nagged by the evil advisor; every log reminder is the good advisor's | partial | the notifier and reminder call real natives; the log entries are stubs |
| Music: generic quest music for the introduction, the epic theme for the reward | todo | `StartMusic` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part; it may carry wood to the workshop like the player, since the quest only watches the workshop's wood | todo | follows from the wood read, a stub |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The watcher's search for a missing workshop and the wait for the scaffold to be picked up loop without any pause | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The wood step watches for wood being used up (a fall of 800 in a second), not for wood being given; feeding the workshop slowly still works because the drop comes when the scaffold is made | todo | the wood read is a stub |
| The Forest miracle is enabled twice | todo | `SetMagicInObject` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The town's adults are capped at 40 when working out the homeless share, so a town of 40 or more adults only needs room for more than 24 of them, however big it is | todo | the counts are stubs |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second info sign on combining scaffolds into buildings (one to seven scaffolds) is commented out; its text is still in the text table | n/a | never in the shipped game |
| Earlier places for the 0 and 0.1 log entries, after the films, are commented out | n/a | never in the shipped game |
| A comment "need a highlight to say about the homeless" marks where the second scroll was added | n/a | a source comment only |
