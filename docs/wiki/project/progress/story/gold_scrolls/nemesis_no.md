# Nemesis. No!

Khazar's death on Land 2: as soon as the player starts winning towns from Lethys (or has six towns), Nemesis opens a
vortex beside Khazar's town, burns it with fireballs, blows up Khazar's temple and kills Khazar's creature with a
storm; Lethys's creature takes the piece of the Creed from the body and walks into the vortex, and the advisors work out
what the Creed is. It has no scroll: it is a story film that records a gold (quest) entry in the challenge log, filed
under Khazar's last words, "Nemesis. No!". The land as a whole is in [../land_2.md](../land_2.md), Khazar in
[../../rival_gods/khazar.md](../../rival_gods/khazar.md#his-death), Nemesis in
[../../rival_gods/nemesis.md](../../rival_gods/nemesis.md).

**Land:** 2 · **Giver:** none (a story event at Khazar's town; Nemesis speaks) · **Script:** KillKhazarMain · **Reward:** none · **Repeatable:** no

Sources: the quest's script source (`KillKhazar.txt`) and the land's control script (`LandControl2.txt`), checked
against the compiled form in the shipped `challenge.chl`; the land's map script (`Land2.txt`) for the towns' owners; the
game's English text table; and the executable (read-only) for stopping scripts by name and for the challenge log.
openblack's state is judged on this tree: of the 61 script functions this film needs, 9 only logged "not implemented"
in `src/CHLApi.cpp` when they were counted (among them creatures' actions and desires, the computer players, the Creed
glow and the challenge log; most of these are real natives now, `CreatureDoAction` logging it only for an action row with
no executor), and openblack never runs Land 2's control script at all (see [../land_2.md](../land_2.md) row 1), so Khazar never
dies; every row below is pending unless the notes say otherwise.

**Progress: 0/63 done, 0 chosen, 0 partial, 0 inferred — 0%**

## When it happens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds whether Khazar should die, until it has happened | pending | `LandControl2` has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each check counts reasons: one for each of Lethys's three starting towns (the two Celtic towns nearest the player and his home town) that he no longer holds; one if the player has more than five towns; one if Khazar has no towns left or Lethys has two or fewer | pending | `GetTownWithId` and a town's player are real; `GetPlayerTownTotal` is real; not checked in play |
| Khazar dies if there is any reason at all, so in practice the first town the player takes from Lethys (which also drops him to two towns), a sixth town of the player's own, or Khazar losing both his towns sets it off | pending | the reasons come from those reads, which are real; not checked in play |
| It waits while Khazar is busy in one of his own lessons or challenges (Khazar's "in a script" flag); the check repeats until he is free | pending | plain script flags; see [worship_site.md](worship_site.md), [impress_village.md](impress_village.md), [khazars_fireball_challenge.md](khazars_fireball_challenge.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The control script waits for the whole film to finish before going on | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Straight after it, the land's last gold scroll is started if it wasn't already | pending | plain script; see [destroy_it.md](destroy_it.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Setting the stage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's own behaviour script (his visits and comments) is stopped, and the land is told Khazar is gone | pending | `StopScript` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Two cows are put in a small flock (inner 5, outer 15) just east of where the vortex will open | pending | `FlockCreate` and filling it with cows (`PopulateContainer`) are real; not checked in play |
| Khazar's hand flies fast (speed 400) to above his temple | pending | moving a god's hand (`MoveComputerPlayerPosition`) is real; not checked in play |
| The player's creature is moved to a spot north of Khazar's town (by the Indian town there) and made to stand idle | pending | `SetPosition` is real; the idle animation (`CreatureDoAction`) is forced as a plan through the creature's mind; not checked in play |
| Lethys's and Khazar's creatures have all their own wants switched off; Khazar's creature is put just west of its town's buildings and made to cower on the spot; Lethys's creature is made to stand idle | pending | the rival creatures are loaded after the arrival scene (`LoadCreature` is real), which has not yet been run to its end; `CreatureSetDesireActivated` is real and `CreatureDoAction` goes through the mind (an action row with no executor only logs "not implemented") |
| While the film runs, a background script keeps both rival creatures alive and not burning, every 2 seconds: Khazar's until the storm strikes it, Lethys's until it has gone into the vortex | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |

## The film: the vortex and the fireballs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The screen fades to black over 2 seconds; the camera's position and aim are remembered | pending | `SetFade` and the camera reads are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The time of day is set to 16:50 and stopped | pending | `SetGameTime` and `GameTimeOnOff` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera is put high to the north-east, aimed at the vortex spot east of Khazar's town, with a wide lens slowly narrowing; Nemesis's music starts; the screen fades in over 3 seconds | pending | `SetCameraPosition`, `SetCameraFocus`, `StartMusic`, `SetFadeIn` and the lens (`MoveCameraLens`) are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The ground shakes (radius 700, strength 0.5, 3 seconds), then a vortex opens there and the ground shakes harder (radius 200, strength 0.7, 6 seconds) | pending | `ShakeCamera` and `CREATE` of a vortex are real; see [../portals.md](../portals.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's creature turns to the vortex and cowers; the camera drifts down towards the town over 18 seconds | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| A huge black storm cloud forms over the vortex (rain, heavy overcast, lightning sheets and a few forks, wide reach) | pending | `CREATE` of a weather thing and the `Change*Properties` natives are real (`src/Magic/Script/CHLWeather.cpp`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nemesis: "Lethys, my ally. You have failed me. I, Nemesis, have arrived to take control." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Fireballs fly from the vortex in curling volleys: five at the crèche, four at the tents, four at another tent, four at the town centre; Khazar's creature turns to watch each and cowers | pending | `SpellAtPos` is real (`src/Magic/Script/CHLSpells.cpp`); the curl from a point in the air is not checked; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The crèche is set on fire; between volleys, Nemesis: "But I will not let them threaten my destiny." and "I will be the only god." | pending | `SetOnFire` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The building at the town centre is set on fire | pending | `SetOnFire` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The film: Khazar's temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second black cloud (no forks, 30-second life) forms over Khazar's temple; the camera looks at it from close, the lens widening | pending | weather and lens natives are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nemesis: "Khazar. You have defied me for the last time." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's temple is blown up | pending | `ObjectDelete` of a citadel heart is real (`DeleteByScript`); never reached in play; see [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-is-destroyed) |
| Khazar: "Nemesis. No!" | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records a quest (gold) entry titled "Nemesis. No!", success 1, alignment 0, its picture the view of the exploding temple, and no reminder line | pending | `Snapshot` is real |
| Nemesis: "Khazar, I know your Creature hides a part of the Creed inside him.", "I do not need it. I already have the power that three combined Creeds brings.", "And I will use it to destroy you." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Khazar's computer player is switched off, and the player's alliance with Khazar is set to none | pending | `EnableDisableComputerPlayer` is real; `SetPlayerAlly` ends the alliance (`ecs::alliances::SetAlliance` (`src/ECS/PlayerAlliances.cpp`)); see ../../rival_gods/khazar.md |

## The film: Khazar's creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 10 seconds, five of the town's buildings burn (from 0.4 to 0.7) | pending | `SetOnFire` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera looks across the burning town at Khazar's creature, closing in on it over 14 seconds | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| The storm cloud moves (speed 20) over the storage pit, then the big building, then onto Khazar's creature, its lightning forks dying away when it arrives | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run); the cloud's speed (`SetProperty` SPEED of a weather thing) and its journey (MOVE_GAME_THING sends a weather thing's storm to the point, `weather_thing::SetTarget`) are real |
| A level-2 explosion miracle (radius 50, 7 seconds) strikes Khazar's creature from the cloud, and it dies for good | pending | the rival creatures are made only after the arrival scene, and `CreatureDoAction` works only in part |
| The Creed's glow shows over the body (scale 5, full power) | pending | `SetCreatureCreedProperties` is real |
| The cloud drifts back to the vortex | pending | weather natives are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The film: Lethys takes the Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's creature is put on the hill west of Khazar's town, facing the body; the camera follows it from near the vortex | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run); `SetFocusFollow` is real |
| It is given the strong creature spell and walks (speed 10) to the body | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| The buildings are set burning again | pending | `SetOnFire` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nemesis: "Lethys. Retrieve the Creed from Khazar's dead Creature. It must not fall into our enemy's hands." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lethys: "As always, I will do as you command. I'll send my Creature for it now." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Once it is within 32 of the body (or after 55 seconds), the glow fades on both, then moves to Lethys's creature at full power, and the body's glow goes out | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run); the Creed glow (`SetCreatureCreedProperties`) is real |
| Lethys's creature turns and walks into the vortex | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| The evil advisor: "He's got an element of the Creed!"; the good advisor: "Does this mean all is lost? I fear it does!" | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera pulls up; once out of view, Khazar's creature's body is deleted | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| When Lethys's creature reaches the vortex (or no longer exists, or after 60 seconds) its spell is removed and the creature itself is deleted: Lethys has no creature for the rest of the land | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| The vortex fades out; the camera rises and widens over 14 seconds, aimed above the vortex | pending | `VortexFadeOut` is real (`ecs::vortex`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After 4 seconds the screen fades to black over 2 seconds; the music stops; the storm cloud is deleted; the camera is put back where it was and the screen fades in over 3 seconds | pending | fades, the camera and `StopMusic` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The advisors explain the Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor steps out: "I can't believe it. Khazar wiped out like a bug!" | pending | `SpiritEject` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The good advisor steps out, and they talk: good "I understand what the old Guide meant, now.", good "The power of three Creeds must be combined.", evil "But Nemesis said he has three Creeds.", good "Yes. But he's after the others to stop us getting them.", evil "We've got to get them, then!", good "Absolutely!" | pending | advisors and texts are real; see [../creature_guide.md](../creature_guide.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Game time starts again; the film ends; the player's creature is released back to its own will | pending | `GameTimeOnOff` and `ReleaseFromScript` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar is gone for the rest of the land: no more visits or comments, no more reactions when towns change hands, and his lessons and challenges that check whether he is gone stop offering him | pending | Khazar's computer player does not exist in our tree; see ../../rival_gods/khazar.md |
| His temple is gone and his town is left burning; his towns stay where they are, unowned by a god with a temple | pending | the temple's destruction is not done |
| Neither rival god has a creature on the land afterwards | pending | the rival creatures are not yet made in play (`LoadCreature` is real; they are loaded after the arrival scene, not yet run) |
| The land's last gold scroll starts if it hadn't ([destroy_it.md](destroy_it.md)); from the next check on, Lethys steals the creature once he is down to one town or the player holds two of his three ([lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md)) | pending | the theft's computer-player and creature natives are real; not checked in play; see [destroy_it.md](destroy_it.md) |
| It can't fail and can't soft-lock by itself; but if Khazar were stuck "in a script", he would never die, the creature would never be stolen, and the land could only end through Lethys's temple vortex | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays through the film and stops before the advisors' talk | pending | `StartMusic` and `StopMusic` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The voices are Nemesis's, Khazar's, Lethys's and the two advisors' | pending | `RunText` speaks each line in its speaker's voice; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature is moved to the north of Khazar's town and held idle for the whole film, then released | pending | moving the creature (`SetPosition`) and holding it idle (`CreatureDoAction`, through the creature's mind) are real natives; not checked in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The trigger test counts several reasons but fires on any one, so Khazar dies at the very first loss of a town by Lethys | pending | the reasons come from the town reads, which are real; not checked in play |
| If Khazar is in one of his scripts when the player wins the land outright (Lethys has no towns and his temple is nearly destroyed), the land's exit can open on the same check before Khazar dies, skipping Khazar's death and the creature's theft | pending | the exit check reads the town totals (`GetPlayerTownTotal`), which are real; not checked in play |
| A position for Khazar's hand is remembered but never used | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The player's creature is released and then looked up again, which changes nothing | pending | `ReleaseFromScript` and `CallPlayerCreature` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nemesis's first line differs from the source comment ("Lethys, my ally. You have failed. Combined, our enemies are too strong.") | pending | the same script, so the same text; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

