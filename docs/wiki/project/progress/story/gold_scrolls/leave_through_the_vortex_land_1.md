# Leave Through the Vortex (Land 1)

The first land's last gold scroll: after Nemesis's storm kills the guide, a golden light marks a vortex that opens near
the coast, swallows a shepherd and throws him back out, and the advisors tell the player to send people and supplies
through and then follow. Clicking the scroll over the vortex dives the camera into it and ends the land. The quest's
flow is here; how vortices work (what they suck in, what crosses, the arrival) is in [../portals.md](../portals.md).

**Land:** 1 · **Giver:** the two advisors, at the vortex near the coast · **Script:** LeaveThroughVortexL1 ·
**Reward:** the way to the second land · **Repeatable:** no

Sources: the land's control script and the quest's script (the original source text, which matches the shipped
`challenge.chl`), the shared reminder script, the game's text table and the executable. openblack is judged on this
tree: a game that does not skip the creature guide has not yet been run as far as the storm (a run of 2026-10-22 reached
Choose Your Creature's first gate stone), and none of the commands the quest needs only logs "not implemented" in
`src/CHLApi.cpp`.

**Progress: 0/41 done, 0 chosen, 13 partial, 0 inferred — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background straight after the guide's fight lesson, together with The Ogre, and then runs the storm | pending | the call is in `LandControl1`, but a no-skip game has not yet been run that far: past Choose Your Creature's glade and the guide's lessons, whose script-made guide creature `CREATE` makes now. See [../creature_guide.md](../creature_guide.md) |
| It waits until the storm opens the way: 30 seconds after the storm turns on the player's village (about two minutes after the guide dies) | pending | the storm's script needs the guide creature, which `CREATE` of a creature makes now; the creature actions are forced by their row (`CREATURE_DO_ACTION`, `src/CHLApi.cpp:3617`), but the guide's death (row 261 DeadForever) has no executor, so the guide does not die, though the storm's script goes on (see [../creature_guide.md](../creature_guide.md)); never reached in a run |
| A yellow "see this" beam is made where the vortex will open, lasting until the vortex appears | partial 60% | `SpecialEffectPosition` is real in `src/CHLApi.cpp`; this branch is only reached in a no-skip game, which stops earlier; re-measured 2026-10-21: the native is real; only a no-skip game reaches it |
| The good advisor steps out: "This is hopeless. Wait. What's this golden light?" and, half a second in, the evil advisor points at the spot (without having been called out first; how that shows is undetermined) | pending | `SpiritEject`, `SpiritPointPos`, `RunText` are real (advisors drawn and moving, `src/Help`); never reached in a no-skip game |
| The quest then waits until the player brings the camera within 200 of the spot; there is no reminder meanwhile | pending | the camera-near check uses real natives (`GetCameraPosition`, `GetDistance`); never reached in a no-skip game |
| A new game that skips the creature guide (or keeps the old creature) opens the vortex and its scroll at once, with no scenes | partial 60% | the guide-skip branch calls only real natives: `CREATE` of an In vortex at the marker (`ecs::vortex::Create`) and `CreateHighlight` for the scroll. Reached with the third and fourth SkipBox answers, but not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## The vortex opens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature is forbidden to go through the vortex for now, and its kind is recorded "for setting up the next land", though no shipped script ever reads it back (quirk) | pending | property 36 (may go through a vortex) is set by `SET_PROPERTY` (`src/CHLApi.cpp:2224`, `Creature::mayGoThroughVortex`) and `GameSubType` is real; not checked in play |
| A cinema: the camera moves in over 7 seconds to look at the vortex's place | pending | `MoveCameraPosition`, `MoveCameraFocus` are real (`src/Camera/ScriptCamera`); never reached in a no-skip game |
| 4 seconds in, a Norse shepherd is made beside it, faces it and looks about (the looking-for-something animation, twice) | pending | `CREATE` makes villagers (`CreateScriptObject`), `SetFocus` and `SetScriptState` work on villagers; never reached in a no-skip game |
| The vortex opens (an "in" vortex, the kind that takes things to the next land) and the yellow beam is removed; the shepherd is pulled in | pending | `CREATE` of an In vortex and its pull are ported (`ecs::vortex`, test/test_vortex.cpp); never reached in a no-skip game (the skip path opens it at once instead) |
| A second later both advisors step out; the good advisor looks back over the land; evil advisor: "What's this? Some trick? Some new Miracle?" | pending | advisors and texts are real (`SpiritEject`, `LookAtPosition`, `RunText`); never reached in a no-skip game |
| The camera moves onto the vortex (8 seconds); good advisor: "It looks like a portal of some kind." | pending | real natives (`MoveCameraPosition`, `HasCameraArrived`, `RunText`); never reached in a no-skip game |
| The camera goes to the vortex's edge; evil advisor: "It looks like a trap to me." then "Look. It's sucking stuff in." | pending | real natives; never reached in a no-skip game |
| Good advisor: "Try throwing something into it." as the camera swings to a wide view of the vortex; then "I bet you anything it'll come out wherever this Vortex leads to." | pending | real natives; never reached in a no-skip game |
| Both advisors go home and the cinema ends | pending | `SpiritHome`, `EndCameraControl`, `EndDialogue` are real; never reached in a no-skip game |

## Testing the vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A 60-second timer starts; the shepherd comes back when it runs out, or as soon as the player picks something up and lets go of it (or has held it for 60 seconds) | pending | `CreateTimer` works and the held-object question (`GET_OBJECT_HELD` of the hand, script ip 48437) is real (`src/CHLApi.cpp:5377`); not checked in play |
| 4 seconds later, the quest waits until the vortex is in view and the camera is within 150 of it | pending | `PosFieldOfView` and the camera distance are real; never reached in a no-skip game |
| What the player throws in really goes through to the next land | pending | the In vortex takes objects (`ecs::vortex`), and `src/ECS/VortexSave.cpp` writes them for the next land; `LOAD_MAP` changes the land (`Game::ChangeLand`), but the next land's vortex does not throw them out yet (the crossing lanes, group58, are written, not landed). See [../portals.md](../portals.md#what-crosses-and-what-is-left) |

## The shepherd comes back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema: the camera turns to the vortex (4 seconds); 3 seconds in, a new shepherd is made at its middle and flung out towards the camera at speed 16, the camera following him | pending | the new shepherd is made and flung (`SET_HEADING_AND_SPEED`, script ip 48541, `src/CHLApi.cpp:4313`: a thing at rest is thrown into the physics with a random tumble); not checked in play; the camera follow `SetFocusFollow` is real |
| The shepherd: "Cor. There's a whole new world through there." | pending | `RunText` is real; never reached in a no-skip game |
| The camera pulls back to the wide view; both advisors step out and the camera looks up | pending | real natives; never reached in a no-skip game |
| Good advisor: "That's it. I'm going through for a look.", pointing high and then down into the vortex as he "goes through" | pending | `SpiritPointPos` is real; never reached in a no-skip game |
| Evil advisor: "He's braver than I thought. He's gone through."; 2 seconds later the good advisor points high again, as if back | pending | real natives (`SpiritPointPos`, `StopPointing`); never reached in a no-skip game |
| Good advisor: "The Villager was right. There's a new land through there." then "And we should send plenty of food, wood and followers into the Vortex as well." (a line shared with the fourth land's vortex) | pending | `RunText` is real; never reached in a no-skip game |
| Evil advisor: "You really aren't afraid of this thing, are you?" | pending | `RunText` is real; never reached in a no-skip game |
| Good advisor: "I say we throw things through and then go through it ourselves. Our Creature will follow. Try clicking the Action Button on the Scroll." — a second in, the gold scroll appears 20 above the vortex and he points at it | pending | `CreateHighlight` and the height through `SetProperty` (YPos) are real; never reached in a no-skip game |
| Evil advisor: "What the hell. Do we want to live forever?"; good advisor: "Well actually…" and the cinema ends | pending | real natives; never reached in a no-skip game |

## The scroll and leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is not recorded in the scroll log: no title, progress mark or reminder text is set for it (unlike the guide's and trainer's scrolls) | partial | nothing to set; the scroll is a real highlight (`CreateHighlight`, `src/ECS/ScriptHighlight`) |
| Straight away and then every 30 minutes until the scroll (or its spot) is clicked, the reminder "There's a Gold Story Scroll ya gotta click on, Boss." is said | partial | the shared reminder `QuestHighlightNotify` calls only real natives (`StartDialogue`, `RunText`, `GameThingClicked`); reached on the guide-skip path, not checked in game |
| The reminder's advisor never steps out or points: the quest passes the evil advisor's voice where the reminder expects an advisor, so neither advisor matches (quirk) | partial | our tree runs the same script, so the same quirk follows; not checked in game |
| Once the scroll is clicked it is switched on and the creature is allowed through the vortex | pending | the scroll's click is real (`GameThingClicked`), and the creature's permission (property 36) is set by `SET_PROPERTY` (`src/CHLApi.cpp:2224`); not checked in play |
| A cinema: the camera climbs to 100 above the vortex over 6 seconds looking down, then dives into it over 8 | partial | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` are real; reached once the scroll is clicked on the guide-skip path, not checked in game |
| 5 seconds into the dive the screen fades to black over 2 seconds | partial | `SetFade` works (`src/CHLApi.cpp`, `ScreenFade`); reached on the guide-skip path, not checked in game |
| When the camera arrives, every script of the guide's story (the storm with it) is stopped and the land is told to end | partial | `StopScriptsInFiles` works by source file and the script sets the leave-now flag; reached on the guide-skip path, not checked in game |
| With the storm's script stopped the land's control script finishes; the story stops every other script and loads the second land | partial | `LandControl1` finishes and `LOAD_MAP` changes the land (`Game::ChangeLand`) to the second land; only Land 1's skip path reaches it in play (a full Land 1 has not yet been run past the creature glade since `CREATE` of a creature became real), and no check measures it yet. See [../portals.md](../portals.md#arrival-scenes) and [../land_2.md](../land_2.md) |
| The creature follows the player through: in the second land it is made again at the arrival vortex | pending | no land loading and no creature carried between lands; see [../portals.md](../portals.md#arriving) |

## Failure, soft locks and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot fail: nothing in it reacts to the shepherd dying, the vortex being ignored or the scroll waiting; the player can stay in the first land as long as they like | partial 50% | our tree runs the same script; nothing in it can fail; re-measured 2026-10-21: as the row above: the same script |
| If the storm stops existing before the scroll is clicked, the storm's script ends, the land's control script finishes and the story loads the second land at once, skipping the rest of this quest (undetermined whether the storm can end by itself) | pending | the storm's script is never reached (it needs the guide creature) |
| In a skipped-guide game the creature's permission to go through is given too: the creature is looked up before the vortex quest starts, by the land's control script and by the first lesson, which runs on every answer | partial | script ips: the creature global is set at 68051 (the land's control script) and 6012 (the first lesson), before the permission is set at 48746-48753; our tree runs the same script (`CallPlayerCreature` gives the profile creature on the fourth answer) and sets the permission (`src/CHLApi.cpp:2224`); not checked in play |
| Next in the story: the second land, Khazar's ([../land_2.md](../land_2.md)) | partial | `LOAD_MAP` changes the land (`Game::ChangeLand`); only Land 1's skip path reaches it in play (a full Land 1 has not yet been run past the creature glade since `CREATE` of a creature became real), and no check measures it yet |
| No music is started by the quest; whatever the storm left playing carries on | partial | our tree runs the same script; no music call |
