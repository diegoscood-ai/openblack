# Worship Site

The first of Khazar's lessons on the second land, logged as the gold scroll "Worship Site": once the player's temple
and village centre are finished, Khazar gives a builder disciple to raise the Norse worship site; when it stands he
comes back with a second scroll and teaches worship (raising the village totem) and charging miracles, then places the
workshop's scaffold and a third scroll that leads to his two miracle challenges. The land as a whole is in
[../land_2.md](../land_2.md), the land's script in [../../scripts/land2_script.md](../../scripts/land2_script.md), the
script program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Khazar (the friendly god's hand), at scrolls he lays by the player's temple · **Script:** LearnWorshipping · **Reward:** a builder disciple and the Norse worship site's miracles (grain, wood, water) to charge; the lesson ends with the workshop scaffold and the scroll to Khazar's two miracle challenges · **Repeatable:** no

Sources: the quest's script source (`LearnWorshipping.txt`), the shared Land 2 scroll notifier (`SetupLand2.txt`), the
hand demo scripts it runs (`HandDemos.txt`), the god-confronts-player helper (`Land2ComputerAI.txt`), the land's arrival
(`BeginLand2.txt`) and control script (`LandControl2.txt`), checked against the compiled scripts in the shipped
`challenge.chl`; the game's English text table (`InfoScript2.txt`) and its hand demo recordings (`Data/HandDemo`).
openblack's state is judged on this tree: of the 95 script functions this quest and the scripts it runs need, 19 only
logged "not implemented" in `src/CHLApi.cpp` when they were counted (among them moving and forcing Khazar's hand, the
challenge log, the totem read, disciples, hits on an object and the held object; Khazar's hand, the challenge log and
the held object are real now). More basically, openblack never runs Land 2's control
script: the story reaches it only through the earlier lands' exits (`LOAD_MAP` changes land now, not yet checked) and the story's top script always begins with Land 1's control script
(see [../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never appears; every row is pending
unless the notes say otherwise.

**Progress: 1/74 done, 0 chosen, 1 partial, 0 inferred — 2%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background straight after the arrival film (Khazar's introduction, the scaffolds, the disciple and the one-shot miracles), along with the land's other watchers | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The arrival leaves the player's citadel at 30% built and the village centre at 30% built, a builder disciple at the village centre, and Khazar's three gifts of one-shot food, wood and water (plus two heals) by the temple; the quest needs both buildings finished | pending | `BuildBuilding` is real; the arrival is not a gold scroll and has no file of its own (see [../land_2.md](../land_2.md), Arriving); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the quest finishes, the control script (which checks every 9 seconds) opens The Workshop's watcher, the 20-minute timer for Impress Village, the influence watcher and a "spruce up your worship site" info sign | pending | plain script flags in `LandControl2`; see [the_workshop.md](the_workshop.md) and [impress_village.md](impress_village.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The lesson's own end lays the scroll that leads to [Khazar's Fireball Challenge](khazars_fireball_challenge.md) and [Khazar's Shield Challenge](khazars_shield_challenge.md) | pending | `CreateHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Waiting for the temple and village centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At once, the village centre can no longer be picked up and the citadel can't build a worship site | pending | `SetIdPickupable` and `SetCanBuildWorshipsite` (`src/Magic/Script/CHLWorship.cpp`) are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest waits, checking without pause, until both the citadel and the village centre are fully built, or Khazar is gone | pending | the built percentage (property 22) is handled by `GetProperty`; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the citadel is finished, Khazar's hand flies to 10 m in front of the camera (snapped there if it hasn't arrived within 10 seconds) and he says "Your Temple is built. Excellent." | pending | moving Khazar's hand (`MoveComputerPlayerPosition`) is real; not checked in play; `RunText` is real |
| When the village centre is finished, Khazar comes the same way and says "Good. You have built much. But there is more for you to construct." | pending | Khazar's hand moves (`MoveComputerPlayerPosition`) and the line are real; not checked in play |
| Each line is said once, in whichever order the buildings finish | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The first scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's hand flies to a spot in front of the citadel (speed 250, at a fixed height), then is let go | pending | `MoveComputerPlayerPosition` and `ReleaseComputerPlayer` are real |
| A gold scroll appears there, raised 5 above the ground | pending | `CreateHighlight` and the height through `SetProperty` (YPos) are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| While the camera is within 100 of the scroll and the scroll is on screen, the good advisor steps out, points at it and says "Khazar has something important to tell us.", at most once every 30 seconds and only when no film is playing | pending | the notifier calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The quest waits until the scroll (or its spot) is clicked, or until Khazar is gone; the scroll is then switched on | pending | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| A new man is made at the citadel and joins the player's home town | pending | `CREATE` makes villagers (`CreateScriptObject`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## The builder film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film starts; Khazar's hand (speed 350) picks the man up and drops him by the worship site's plot while the camera swings over to look at it | pending | forcing Khazar to pick up and drop (`ForceComputerPlayerAction`), widescreen and the camera are real; not checked in play |
| Khazar: "I have given you an expert builder of Worship Sites" | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The man becomes a builder disciple (with its sound), carries a heavy mallet, walks to the plot and hammers in a loop | pending | `SetDisciple` and `SetObjectCarrying` are real |
| The camera pulls back over the plot; the home town and the citadel are now allowed to build a worship site | pending | `SetCanBuildWorshipsite` and the camera are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "This building is of great importance." then "I shall return once you have finished building the Worship Site." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar's hand is let go, the dialogue closes and the camera pulls further back | pending | `ReleaseComputerPlayer` is real; the dialogue and camera are real |
| The log entry "Worship Site" is recorded at 0 with the good advisor's reminder "You need to build the worship site. Create some Disciples." | pending | `Snapshot` is real; the challenge log is not drawn |
| Both advisors step out; as the film ends the evil advisor says "Oh boy, oh boy, oh boy, are we going to cause some havoc now!" and the good advisor "Havoc? No, but it will be good to cast lots of lovely Miracles when we need them." | pending | `SpiritEject` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The builder is let go to work on his own | pending | `ReleaseFromScript` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Building the worship site

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has the Norse worship site built by the temple (any builders, the given disciple or the player's own) | pending | our tree has worship sites (`src/Worship`); see [../../worship/worship_sites.md](../../worship/worship_sites.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| At half built, the log entry is set to 0.5: recorded afresh if the worship site is on screen, otherwise only updated quietly | pending | `Snapshot` and `UpdateSnapshot` are real; `GameThingFieldOfView` is real |
| When it is fully built the first scroll fades away | pending | `ObjectDelete` of a highlight is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The quest then waits until the worship site is on screen, or 30 seconds, and logs "Worship Site" at 1 (complete) | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Khazar comes up to the camera and says "The power you wield grows!" | pending | Khazar's hand moves are real; not checked in play |
| There is no timer and no way to fail the building part: the quest waits for ever | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## The second scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's hand flies (speed 150) to a spot by the worship site and is let go; a second gold scroll appears there on the ground | pending | `MoveComputerPlayerPosition` is real; `CreateHighlight` is real |
| The good advisor nags the same way: "Look. One of Khazar's Scrolls. Let's click on it." | pending | the notifier calls real natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Two bronze info signs appear once it is clicked: one at the village centre on raising and lowering the totem to send people to worship, one by the temple on worship sites, worshippers' needs, charging and the chant store | pending | the info sign script and `CreateHighlight` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The village centre can be picked up again from now on | pending | `SetIdPickupable` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Khazar's lesson on worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film with Khazar's music; Khazar's hand flies to the village centre (speed 400) and the camera follows | pending | `StartMusic`, the camera and moving Khazar's hand are real; not checked in play |
| Khazar: "Your Worship site is built. Now let me teach you how to create Miracles." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "Your people will worship you at the Temple." then "To send them there you must raise the Totem at the Village Centre." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The totem hand demo plays, paused at each step: "You move the Hand over the Totem." · "Then you hold down the Action Button to grab the Totem." · "You move the Hand up and down to raise and lower the Totem." · "I've raised the Totem so your followers will go to the Worship Site." | pending | `PlayHandDemo`, `HandDemoTrigger` are real (`src/Input/HandDemo.h`, `Data/HandDemo/totem.hnd`); see [../../hand/hand_in_scripts.md](../../hand/hand_in_scripts.md) and [../../hand/totem.md](../../hand/totem.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| So that worship has begun by the film's end, up to eight of the home town's villagers (not ones held by scripts) are put straight at the worship site, within 3 of its centre, and gathered into a small group (inner 3, outer 10) that is then broken up | pending | `FlockCreate`, `FlockAttach`, `FlockDisband` are real (`ecs::script_containers`); see [../../worship/worshippers.md](../../worship/worshippers.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "The higher you raise the Totem the more people from your Village will go to worship you" then "Now come and look at your followers." while his hand drifts slowly to the worship site | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The camera flies a recorded track around the worshippers; the worship site's stored power is emptied before and after it | pending | `RunCameraPath` and `GameSetMana` are real; see [../../camera/camera_paths.md](../../camera/camera_paths.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "They are dancing, worshipping you. This gives you power." · "These are the Miracles that your people offer you." · "The Miracles of Grain, Wood and Water." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## The miracle hand demo

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The worship site's power is set to nothing and the "miracle" hand demo plays | pending | `GameSetMana` and `PlayHandDemo` are real (`Data/HandDemo/Miracle.hnd`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "To activate one, grab the Miracle with the Action Button and it will be charged by your worshippers."; while this is being read the site's power is held at nothing | pending | real text and mana natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "You'll see the energy rings flowing into your hand."; the site's power is raised to three quarters of a grain miracle's cost, in 50 steps with no pause between them (so in effect at once) | pending | `GetManaForSpell` and `GameSetMana` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "When the Miracle is charged it appears in your hand and you are ready to cast the Miracle."; if nothing is in the hand by then, the site's power is set to that three quarters again | pending | the in-hand test `GetObjectHeld` is real; not checked in play |
| The film waits for the demo to finish | pending | `IsPlayingHandDemo` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "Remember worshipping comes at a cost, the needs of your worshippers can be seen on these flags." as the camera moves to the worship site's needs flags | pending | real text and camera natives; see [../../worship/worship_sites.md](../../worship/worship_sites.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The "miraclecast" hand demo plays without changing the hand's look; meanwhile Khazar: "So it's good to satisfy these needs and keep your Villagers in good health." | pending | `PlayHandDemo` is real (`Data/HandDemo/MiracleCast.hnd`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "I'll leave you now to experiment with Miracles, and expand your influence by growing your Village." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The home town's totem is set to half height, so half the village goes on worshipping after the film | pending | the totem height is not a property `SetProperty` handles |
| The camera pulls back over the temple, the music stops and the film ends; the second scroll fades away | pending | `StopMusic`, the camera and `ObjectDelete` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## After the lesson

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar makes a scaffold at his own workshop, set to become a workshop (size 3) and to vanish once used, and his hand carries it (speed 150) to a plot in the player's village to plan the workshop; it then can't be picked up or moved | pending | the scaffold (`CREATE`, `SetScaffoldProperties`), the flags and Khazar's hand carrying it (`ForceComputerPlayerAction`) are real; not checked in play |
| Khazar's hand then flies to a third spot by the temple and is let go; the scroll for his miracle challenges appears there (see [khazars_fireball_challenge.md](khazars_fireball_challenge.md) and [khazars_shield_challenge.md](khazars_shield_challenge.md)) | pending | `MoveComputerPlayerPosition` and `ReleaseComputerPlayer` are real; `CreateHighlight` is real |
| The quest marks the lesson done, which lets the control script start The Workshop ([the_workshop.md](the_workshop.md)), the timer for Impress Village ([impress_village.md](impress_village.md)) and the "spruce up your worship site" info sign | pending | plain script flags; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Dying worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the lesson's end, a watcher checks the home town's count of worshippers who died from worship, at most once a minute (first check a minute after it starts) | pending | `GetTownWorshipDeaths` and `CreateTimer` are real; whether our worshippers die from worship: [../../worship/worshippers.md](../../worship/worshippers.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If more have died than when the watcher began, both advisors step out: evil "Ha! Your worshippers are working and dying for you!", good "Save them! You only have to lower your Village Totem." | pending | `SpiritEject` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The count it compares with is never moved on, so after the first death the warning comes back every minute, new deaths or not | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The watcher ends for good as soon as the totem is below 0.4 of its height | pending | `GetTotemStatue` is real |

## Khazar coming to the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| For each remark, Khazar's hand flies (speed 350) to 10 m in front of the camera, rechecking every 0.3 seconds; after 10 seconds it is put there at once | pending | `MoveComputerPlayerPosition` and `SetComputerPlayerPosition` are real; `GetFacingCameraPosition` is real |
| The remark is shown as a one-line dialogue in his voice, then his hand is let go | pending | `ReleaseComputerPlayer` is real; the dialogue is real |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Both scrolls are nagged by the good advisor (see above); the log entry's reminder is the good advisor's "You need to build the worship site. Create some Disciples." and is the same at every stage, even after the site is built | partial 60% | the reminder runs real natives; the log entry itself is real (`UpdateSnapshot`); re-measured 2026-10-21: the reminder and the log entry are real; not checked in a run |
| The advisors' banter after the builder film and the dying worshippers warning are the only other advisor lines | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's theme plays through the worship lesson and stops at its end; the builder film has no music of its own (the arrival's Khazar theme has already stopped) | pending | `StartMusic` and `StopMusic` are real (`src/Audio/GameMusic`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The builder disciple is made by the script, with no disciple sound (the original plays it only when the hand puts a villager down whose disciple changed) | done | `SetDisciple` (local 2b91a9f27); the sound is the hand's: `villager::DisciplePutDownByHand`, checked against the exe 2026-10-19 |
| Every spoken line is the voice of its speaker in the text table (Khazar, good spirit, evil spirit) | pending | `RunText` speaks each line in its speaker's voice (`src/Help`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Failing, Khazar's death and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't be failed and has no time limit | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If Khazar is gone (killed by Nemesis, see [../land_2.md](../land_2.md), the end of the land) before the temple and village centre are done, the scroll, builder and film are skipped and the worship site is simply allowed | pending | Khazar's death needs Nemesis's computer player (real natives now; not checked in play); see [../land_2.md](../land_2.md) |
| If Khazar is gone when the second scroll would appear, the lesson, the workshop scaffold and the challenges' scroll are all skipped; the lesson is still marked done | pending | follows from Khazar's death, whose computer-player natives are real; not checked in play |
| If Khazar dies while the second scroll waits, the scroll notifier stops but the scroll is never removed: it stays on the ground | pending | follows from Khazar's death, whose computer-player natives are real; not checked in play |
| Nothing here can stop the land being finished: the land ends on Lethys's towns and temple, not on the lessons, so there is no soft-lock | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wait for the citadel and village centre and the dying worshippers watcher loop without any pause | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The first 0.5 log entry is recorded only if the worship site is on screen at that moment; otherwise the entry is updated without being announced | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The builder's two closing lines are said in the reverse order of their numbers in the text table ("of great importance" before "I shall return") | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The miracle demo's slow fill of the worship site's power has no pause in it, so the power jumps straight to three quarters of a grain miracle | pending | the same script and `GameSetMana`; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

