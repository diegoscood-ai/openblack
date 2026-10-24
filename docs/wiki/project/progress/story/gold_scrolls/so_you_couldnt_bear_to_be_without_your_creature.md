# So You Couldn't Bear to Be Without Your Creature?

The third land's main gold quest, "free the creature": the player follows Lethys through the vortex and finds their
creature frozen in a prison of three stone pillars, each one fed by the prayers of a village. Winning a village sinks
its pillar; losing it raises the pillar again. When all three are down the creature is freed; later, once Lethys's last
town is in trouble, Lethys begs for his life, hands over the creed from Khazar's creature and opens the vortex to the
next land. The quest has no title of its own: its challenge-log entry is headed by Lethys's line "So you couldn't bear
to be without your Creature?", which is never actually spoken.

**Land:** 3 · **Giver:** the advisors, on arrival (no scroll; logged straight away) · **Script:** GetThroughVortexL2, FreeTheCreature · **Reward:** the creature back (leash and temple restored), the creed from Khazar's creature, and the exit vortex · **Repeatable:** no

The land as a whole is in [../land_3.md](../land_3.md), the land's map script in
[../../scripts/land3_script.md](../../scripts/land3_script.md), the vortex mechanics in [../portals.md](../portals.md).
The two attacks started part-way through have their own files: [the_wolves_are_possessed.md](the_wolves_are_possessed.md)
and [fire_fire_im_on_fire.md](fire_fire_im_on_fire.md); the exit is
[leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md). How the creature was taken at the end of Land 2:
[lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`), the game's
text table and the executable. openblack's state is judged on this tree: the story reaches it only through the earlier lands' exits (`LOAD_MAP` changes land now, not yet checked) and the
story's top script always begins with Land 1's control script, so Land 3's control script and everything below never
runs. Of the 97 script commands this quest and the scripts it starts need, 82 do something in `src/CHLApi.cpp` (among
them camera set, fades, music, fire, the climate switch, creating villagers, animals and highlights, and dialogue);
when they were counted, belief changes, creature actions and the computer-player moves were stubs; these and the
snapshots are real now (`SetPlayerBelief`, `CreatureDoAction` through the creature's mind, the computer player's hand
natives). Rows are pending unless the notes say otherwise.

**Progress: 1/97 done, 0 chosen, 0 partial, 0 inferred — 1%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script first sets the land up, then starts the man who wants to be thrown and this quest's prison in the background, then runs the arrival and waits for it to end before starting the monk, the rejuvenator and the tree puzzle | pending | Land 3's control script never runs (see [../../scripts/land3_script.md](../../scripts/land3_script.md)); the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The land ends when the exit vortex's scroll is clicked; the control script then returns and every other script of the land is stopped, so anything still waiting (such as Lethys's own end) is cut off | pending | `StopAllScriptsExcluding` works, but the land never loads |
| The quest is logged as a gold (story) entry, not a silver challenge | pending | `Snapshot` and `UpdateSnapshot` are real |
| The log's title is Lethys's line "So you couldn't bear to be without your Creature?"; that line is not spoken anywhere in the shipped scripts | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Setting up the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's influence is virtual for this land and the player is set to have no alliance with Lethys | pending | `SetVirtualInfluence` is real; `SetPlayerAlly` sets no alliance (`ecs::alliances::SetAlliance` (`src/ECS/PlayerAlliances.cpp`)) |
| The player's creature is loaded at the prison on the hill (it was left with Lethys at the end of Land 2) | pending | `LoadMyCreature` is real (`src/ECS/PlayerCreature.cpp`), loading the profile's creature; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature is barred from the temple | done | `SetCreatureInTemple` keeps the creature room from showing it (raffclar #349, unverified against v1.42) |
| The land's four towns are found: the player's home, the Japanese village, the Indian village and the Egyptian village | pending | the town look-ups (`GetTownWithId`) are real; not checked in play |
| All weather is switched off and cleared within 220 of the home town for 10 minutes, to give the player a chance to build up, then switched back on | pending | `PauseUnpauseClimateSystem` and `KillStormsInArea` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Lethys's creature, a wolf named "Laetes", fully grown and 1.5 times normal size, is made near the prison | pending | Lethys's creature is not yet made in play (`LoadCreature` is real) |
| Lethys's computer player is told not to react to an aggressive creature and not to expand his influence | pending | `SetComputerPlayerPersonality` is real |

## The prison

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three prison pillars on the hill are made indestructible | pending | `SetIndestructable` and `CallNear` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature is put on the prison spot facing the player's temple, its leash is taken away, it is made indestructible and its health is set to a tenth | pending | `SetPosition` is real; the leash detach and the health read (`GET_PROPERTY` Health) and turning a creature (`SetFocus`) are real; not checked in play |
| Each pillar is tied to one village: the second pillar to the Japanese village, the third to the Indian village, the first to the Egyptian village | pending | plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Every 3 seconds each pillar still standing, if the creature is within 15 of the prison spot, fires a 3-second magic beam from the creature to a point 8 above the pillar, with a small plasma sound, and casts a creature freeze on it | pending | `SpecialEffectObject` is real, but the freeze beam and creature spells by script are not done |
| Every 3 seconds each pillar's script also keeps the creature from tiring (its exhaustion reset to nothing) | pending | exhaustion is not a property `SetProperty` handles |
| Lethys's creature is set at a spot by the prison facing the player's creature, and loops until all three pillars are down | pending | Lethys's creature is not yet made in play (`LoadCreature` is real) |
| If a cow is within 50 of Lethys's creature, it stomps on it and eats it | pending | Lethys's creature is not yet made in play (`LoadCreature` is real); `CreatureDoAction` forces the action through the creature's mind; not checked in play |
| Otherwise, if it is within 75 of the player's creature it tortures it in turn with a fireball, a lightning bolt and an itchy spell, its miracle energy refilled each time, then waits 3 seconds | pending | Lethys's creature is not yet made in play (`LoadCreature` is real) |
| Otherwise it walks back to its spot | pending | Lethys's creature is not yet made in play (`LoadCreature` is real) |
| Every tenth time round this loop the creature's frozen moan is heard at the player's creature | pending | `PlaySoundEffect` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| When the last pillar falls Lethys's creature looks confused, frightened and angry, four times over, and is then let go | pending | Lethys's creature is not yet made in play (`LoadCreature` is real) |

## The arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene starts with Nemesis's music; the arrival vortex opens and throws out the player's people into the home town while the camera starts inside the vortex and pulls back over it | pending | `StartMusic` and the arrival vortex (`CREATE`, the Out's emission in `ecs::vortex`) are real; see [../portals.md](../portals.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The shared arrival script is handed 30 people, but it never uses that number: how many people come out is decided by the vortex itself | pending | the same script; see [../portals.md](../portals.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The arrival script also tries to load the player's creature at the vortex, but the game only loads it when the player has none, and it was already loaded in the prison, so the creature does not come through | pending | `LoadMyCreature` does nothing when the player has a creature, as the original; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The vortex fades out 15 seconds after opening and is gone 8 seconds later | pending | `VortexFadeOut` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Both advisors step out. Evil: "Well, that wasn't a trap. But we'd better find our Creature here." Good: "He's got to be around here somewhere." | pending | `SpiritEject` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature's frozen moan is heard; the camera swings over to the prison and both advisors look at the creature. Good: "What was that?" | pending | `PlaySoundEffect`, the camera and the advisors are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves closer. Evil: "It sounded like our Creature." | pending | real camera and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor points at it: "Look!" Good: "He's trapped! He must be in agony." | pending | `SpiritPointGameThing` and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Evil: "Lethys' Creature is torturing him." Evil: "Boy, is he gonna pay for this!" as the camera flies round the prison | pending | real camera and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Good: "It looks like these statues are holding him in place." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Out of view, the player's temple and worship site are built, a storage pit and a workshop scaffold are set down near the home town, and the town centre is built | pending | `BuildBuilding`, `CREATE` of a scaffold and `SetScaffoldProperties` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest is logged at nothing done, its reminder the good advisor's line about the statues | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The camera turns to the villages. Evil: "But where are they drawing their energy from?" Both advisors look at the camera. Good: "Only the prayers of many Villages could be that strong." | pending | real camera, advisor and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera returns over the home town, the advisors go home and the music stops | pending | real camera, advisor and music natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 30 seconds later the advisors speak again. Good: "I think we need to get this Village built." Evil: "Yeah, we gotta be strong for when we get our Creature back." Good: "This place will give us a foundation to fight from." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The arrival waits for the vortex to have closed before the rest of the land's scripts start | pending | plain script wait; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Win over the Japanese, Indian and Egyptian villages (by impressive miracles and belief, or by destroying them: a village that no longer exists counts as won) | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play; see ../town/ for village ownership |
| A pillar is checked every 3 seconds: when its village becomes the player's, it falls; when the village is lost again, it rises | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The quest's progress mark is a third per pillar down, and goes back down when one rises | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) and the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| There is no time limit and nothing ends the quest in failure | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## A pillar falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with epic music: the camera flies to look at the won village, waits 2 seconds, fades to black and back onto the pillar (aimed a quarter of the creature's height higher for the Japanese pillar) | pending | the camera and fades are real, but the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The evil advisor points at the pillar. First time: "Look! When we took control of that Village, one of the Pillars fell." Later times: "We've taken over the Village!" | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The camera shakes for 5 seconds within 150 of the pillar, a big cloud of dust (7 times normal size) rises and the gate-stone grinding sound plays | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The pillar sinks into the ground in steps of a quarter until it is 20 below | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The log's mark is updated, its reminder becoming the evil advisor's line about the pillar falling | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The first time only, the good advisor: "If we manage to keep control of it, I reckon it will stay down!"; and if this is the first pillar: "There are two more pillars and two more Villages. It's clear to me we should take those Villages, Leader." | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| For the first two pillars the scene fades back to the village it was won from (not to where the player was looking) | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The creature-help prompts are switched off at the end of the scene | pending | developer function 11 is not handled by `DevFunction` |

## A pillar rises again

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If a won village is lost (to Lethys or anyone else), its pillar rises: a cut scene with the failure music fades to the pillar | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| Both advisors step out and the evil advisor points. First time: good, "Disaster! We lost the Village!"; later: "Oh no. A Village has been lost!" | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The same shake, dust and grinding sound; the pillar rises by quarters back to ground level | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The first time only, evil: "The pillar's back, keeping our Creature imprisoned." | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The log's mark drops a third, its reminder becoming the evil advisor's line about the pillar being back | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The scene fades back to where the player was looking | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |

## The creature is freed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the third pillar falls the evil advisor says "We've taken over the Village!" (the long line is skipped) and nothing else of the usual pillar talk | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| Lethys: "Quickly, my Creature. Take that last Village back for me!" | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The camera flies over the prison. Good: "Our Creature's free but he's really hurt! Leash him and get him home. He needs rest." Evil: "It's good to have the big guy back." Good: "Yes. We're complete again." | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The freeze is lifted; the camera turns to the creature, which looks at it | pending | the freeze is creature script state, not done |
| The log is marked complete | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The creature waves for attention ("look at me"), walks down towards the lowland while the camera follows, looks at the camera again and gives a friendly wave | pending | creature animations and walks by script are not done |
| The creature is released from the script: its miracle energy is filled, the leash is given back and it may go into the temple again | pending | `ReleaseFromScript`, the leash and `SetCreatureInTemple` are real (raffclar #349, unverified against v1.42), but the energy property is not |
| Lethys's computer player is told to react to an aggressive creature again | pending | `SetComputerPlayerPersonality` is real |
| The pillars turn their faces away from the prison and the beams and plasma sound stop | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| The creature stays at a tenth of its health, as the good advisor warns; nothing in the script heals it | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature is no longer indestructible once all three pillars are down | pending | the indestructible flag on a creature follows the pillars, which never fall |

## Lethys gives up the creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 5 minutes after the creature is freed, the script starts watching Lethys's own town (the fourth, high in the west) | pending | the timer is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It waits until Lethys's lead in belief over the player in that town has fallen below half of what Lethys's belief was when the creature was freed, or the player's belief there passes 400 | pending | `BeliefForPlayer` is real (towns); not checked in play |
| The three pillars are forced fully down, in case they were cheated past | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| A fade to black; both creatures are moved to a spot in the lowland, both leashes taken away, the player's creature made indestructible, and Lethys's hand held still and flown above the prison | pending | `MoveComputerPlayerPosition` is real and Lethys's creature is not yet made in play (`LoadCreature` is real) |
| A cut scene with epic music. Lethys: "Your power is mighty and I have little strength." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera flies to the prison; it shakes for 5 seconds within 300, dust rises at all three pillars and the pillars rise back to ground level | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| Lethys: "Spare me and I will give you everything." The camera closes in. Lethys: "Here. I give you the Creed from Khazar's Creature." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Lethys's hand drifts back over the prison; a fade to white, and the player's creature is back on the prison spot | pending | Lethys's hand is real now (a computer player's hand natives); not checked in play |
| A creed glow appears above the creature's head (five times normal size), faint at first, then at full strength as the camera rises to it | pending | `SetCreatureCreedProperties` is real |
| Lethys: "Allow me to survive and I will guide you to yet another Creed." Lethys: "It is in a land you once knew. Use this Vortex." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The exit vortex is opened with its own gold scroll ([leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md)) and the camera turns to it. Lethys: "Find the Creed there. It is yours." | pending | `CREATE` of a vortex and its highlight are real; see [leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera turns back. Lethys: "But I beg. Please leave me with my last Village." Lethys: "Without it I will be banished to the void." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The glow is switched off, the music stops, and the creature, its leash, Lethys's creature's leash and Lethys's hand are all let go | pending | the glow (`SetCreatureCreedProperties`) and Lethys's hand (the computer player's hand natives) are real; not checked in play |

## Sparing or finishing Lethys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player may leave through the vortex straight away, sparing Lethys | pending | the vortex quest calls only real natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the player instead wins Lethys's last town (or it is destroyed), Lethys's hand is held and flown away from his temple during a cut scene with Nemesis's music | pending | the town ownership read is real and Lethys's hand is real now (a computer player's hand natives); not checked in play |
| A fade to his temple. Lethys: "You have taken everything from me." Lethys: "I am nothing now. A denizen of the great void." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Lethys's temple explodes, watched from three camera angles over 26 seconds | pending | `ObjectDelete` of a citadel heart is real (`DeleteByScript`); never reached in play; see [../losing_and_game_over.md](../losing_and_game_over.md) |
| Lethys's computer player is switched off; good: "Lethys has gone." Evil: "Serves him right. Now let's go through the Vortex to look for the other Creed." Good: "Yes. If we're ready." | pending | `EnableDisableComputerPlayer` is real; not checked in play |
| The choice is remembered: arriving on Land 4, the good advisor praises sparing Lethys ("You know, you did the right thing by sparing Lethys, Leader.") or the evil advisor cheers his death | pending | Land 4 is never loaded; see [../land_4.md](../land_4.md) |
| Leaving through the vortex before Lethys's town is won stops this script, so Lethys counts as spared | pending | plain script flags; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story cannot move on until all three villages are held at the same time: the exit vortex only opens after the creature is freed and Lethys's town is in trouble | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| If Lethys's town no longer exists before the creed scene, his belief there may never be readable and the creed scene may never come, leaving no way out of the land (undetermined: what the belief reading gives for a destroyed town) | pending | not traced; the belief read (`BeliefForPlayer` of a town) is real |
| Its own steps start the Lethys fanatics' fire attack (when the Japanese village is the first won) and the possessed wolves (when the Indian village is the second) | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play; see [fire_fire_im_on_fire.md](fire_fire_im_on_fire.md), [the_wolves_are_possessed.md](the_wolves_are_possessed.md) |
| When the wolves attack and the monk's own quest is done, the monk's Wonder gift is started too | pending | follows from the wolves' attack; see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| Next: the exit vortex to Land 4 | pending | [leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music over the arrival and over Lethys's end; epic music when a pillar falls and over the creed scene; the failure music when a pillar rises | pending | `StartMusic` and `StopMusic` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature's frozen moan on arrival and while it is held; the gate-stone grinding as pillars move; a small plasma sound with each freeze beam | pending | `PlaySoundEffect` and `AttachSoundTag` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The main loop that starts the two attacks, the loop waiting for Lethys's town and the loop waiting for a town to be taken over have no wait in them | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The fire attack only starts if exactly one pillar is down and the Japanese village is the player's; the wolves only if exactly two are down and the Indian village is the player's. Win the villages in another order and one or both attacks never happen | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
| When a pillar falls the cut scene ends looking at the won village, not where the player was looking (a rising pillar does return the camera) | pending | the pillars follow town ownership, read through `GetTownWithId` and a town's player, both real; not checked in play |
