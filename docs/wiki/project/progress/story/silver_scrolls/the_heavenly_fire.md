# The Heavenly Fire

A Land 5 challenge: some minutes after the player first wins Nemesis's Greek village, burning meteors are fired at it
from a point high in the sky about 400 away, in an opening shot, a warning shot and five volleys of five. The player
protects the village (catching meteors in the hand is allowed and praised); at the end the advisors judge how many
people died and how badly the village was hurt, and a well-protected village earns a miracle seed from the sky.

**Land:** 5 · **Giver:** none: it starts on its own, announced by the evil advisor (no scroll is put up) · **Script:** FireOnHigh · **Reward:** a chest from the sky holding an Itchy creature miracle seed, only if the village lost at most 22% of its building and villager health; alignment +1, 0 or -1 · **Repeatable:** no

Sources: the challenge script (the original source text, which matches the PC game's compiled `challenge.chl`), the
land's control script that starts it, the shared reward script, the game's text table and the executable. The land as a
whole is in [../land_5.md](../land_5.md), the land's script program in
[../../scripts/land5_script.md](../../scripts/land5_script.md) and the script list in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md). openblack's state is judged on this tree:
openblack never runs Land 5's control script (the story always begins with Land 1's control script and reaches it only through the earlier lands' exits; `LOAD_MAP`
changes land now, not yet checked), so this challenge never starts. Of the 51 script functions it and the scripts it runs need, none
is a stub in `src/CHLApi.cpp` at this tree: making a rock, setting it on fire and its temperature, throwing it at a
target (`SET_TARGET`), casting a spell at a place, the town's health total, the held-object read, the challenge record,
the dialogue, the advisors and the camera moves all work. Every row is pending unless the notes say otherwise.

**Progress: 0/57 done, 0 chosen, 4 partial, 0 inferred — 3%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge is started by the land's control script, only the first time the player wins the Greek village (Nemesis's village round the Greek Wonder, the land's town 4) | pending | `LandControl5` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| That first capture plays the land's own story scene first (a Greek farmer, the Wonder blown up by an explosion, the creature curse recorded, a tornado that carries the farmer off, the Nemesis theme); the meteors are started as that scene fades back in | pending | the scene belongs to the land's story, see [../land_5.md](../land_5.md) |
| Losing the village to Nemesis and winning it back never starts it again (it is started once, on the first capture only) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| No scroll is put up: the scroll over a villager's hut and the advisor's "Your attention is required here." call are written in the script but switched off, so the player is never asked to accept it | pending | the hut is still looked up but never used; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It waits 300 seconds (five minutes) after the capture before anything happens (the source notes "Should be 300") | pending | the wait is a script sleep (the VM's own); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Then it waits until the camera is within 150 of the village centre with the centre on screen, the hand holds nothing, and the opening meteor's spot (about 120 from the centre) is not on screen | pending | `PosFieldOfView` works, `GetObjectHeld199` (what the hand holds) is real (`src/CHLApi.cpp`); the quest is not reached in play |
| The village it watches is whatever town is within 150 of the village centre at that moment; its villager count is taken now | pending | `CallNear` and `IdSize` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The challenge runs to its end once begun: nothing in it reacts to Nemesis taking the village back, and it has no time limit or failure | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Everything stops when Nemesis loses his last village (the land then stops every script but its own) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The meteors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A meteor is a rock set on fire at a temperature of 2000 and not blown by the wind | partial 40% | our tree makes the rock (`CreateScriptObject`), sets it on fire (`SetOnFire`) and sets its temperature (`SetTemperature`); `SetAffectedByWind` is real (on a rock it does nothing, as in the game); the challenge never runs; see [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md); finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a reward chest", "Whether an object is blown about by the wind", "Sets an object's temperature", "Sets an object on fire at a burning speed", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| Volley meteors are made at half size, at a launch point 250 high about 400 from the village, and thrown to land on their target spot in 4 seconds | pending | `SetTarget` is real (not seen in play yet) |
| While a meteor flies (and is not in the hand) it trails a bonfire flame effect, remade every frame at eight times normal size, so it leaves a long burning plume | pending | `SpecialEffectPosition` works and the flying and in-hand properties are read; the throw: `SetTarget` is real (not seen in play yet) |
| Each launch plays the lava-bomb trail sound at the village's tower, not at the meteor | pending | `PlaySoundEffect` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| At least 4 seconds after it stops flying, and once it is out of the hand and lying still, the meteor blows up: a small level-1 fireball (radius 1, 2 seconds) at a random point up to 2 sideways and 2 above it, the temple-explosion sound, and the rock is deleted with an explosion | partial 40% | `SpellAtPos`, `PlaySoundEffect`, `ObjectDelete` and rock creation work; the wait reads the in-hand property, real (`GetProperty` in `src/CHLApi.cpp`); the challenge never runs; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Casts a miracle at a position", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with explode", "Reads whether a thing is in the player's hand", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| A meteor caught in the hand doesn't burst while held; it bursts wherever it comes to rest after being thrown or dropped | pending | the in-hand property is read (`GetProperty` IN_HAND); not seen in game |
| If a volley meteor is in the hand 6 seconds after its launch, the good advisor comes out once: "Oh I say. Well caught." (only once in the whole game) | pending | `GetObjectHeld199` (what the hand holds) is real (`src/CHLApi.cpp`); the quest is not reached in play |
| The damage done comes from the burning rocks themselves (fire, impact) and their fireballs; the script hurts nothing directly | partial 60% | burning objects and fireballs exist outside the script (`src/ECS/Fire`, `SpellAtPos`); not checked that a burning rock sets a building alight; the challenge never runs; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Casts a miracle at a position", [fireball.md](../../miracles/fireball.md) "Villagers set alight run burning and die", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |

## The opening

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning meteor appears on the ground about 120 from the village centre (made at the spot itself, not thrown), with the lava-bomb trail sound | pending | rock creation, `SetOnFire` and `PlaySoundEffect` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A film starts: the camera shakes round the meteor (radius 500, amplitude 1, 1.5 seconds) with the low temple-explosion rumble | pending | `ShakeCamera` and `PlaySoundEffect` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor comes out pointing at it while the camera turns to it over 1 second: "Hey. Fire! Over there!" and goes home | pending | `SpiritEject`, `SpiritPointPos`, `MoveCameraFocus`, `RunText` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves to look at it from close by (15 up, 15 and 35 off) over 3 seconds; the challenge is then recorded as started, titled "The Heavenly Fire", with the reminder "We need to protect the Village! Nemesis is attacking!" from the good advisor | pending | `Snapshot` is real |
| Both advisors come out; good: "It's a meteor. Thank goodness it missed the Village."; then 2 seconds pass | pending | `SpiritEject` and `RunText` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A warning shot (a full-size meteor) is thrown from the launch point at the near edge of the village (about 70 from the centre) | pending | `SetTarget` is real (not seen in play yet) |
| The evil advisor sits low on the right of the screen and the good advisor half-way down on the left; evil: "Heads up! There's more incoming!" while the camera turns to the launch point over 2 seconds and then follows the meteor down | pending | `ClingSpirit`, `SetFocusFollow` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After 2 seconds, good: "They're heading towards the Village! " and "We've got to protect the people!"; both go home | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After 2 more seconds the camera looks down below the launch point (2.5 seconds) and moves to a view over the village from about 95 away (3 seconds), and the film ends | pending | the camera moves work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The village's total building and villager health is taken as the starting figure, then 1 second passes | pending | `GetTownAndVillagerHealthTotal` is real (`test/test_script_flocks.cpp`); the quest is not reached in play |

## The five volleys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First volley: the evil advisor points at the launch point: "Yay, more! These are cool. See if you can catch some, Boss." Five meteors follow at five spots across the village, the last at the tower in the centre | pending | `SetTarget` is real (not seen in play yet) |
| In the first volley each meteor after the first waits for the one before to land (6 seconds and until it stops flying or is caught) and then 8 more seconds, so they come about 14 seconds or more apart | pending | `SetTarget` is real (not seen in play yet) |
| After it, good: "They've stopped. Let's clean the place up."; then 20 seconds pass | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Before each later volley the script waits until the camera is within 200 of the tower; looking away holds the next volley back (the source itself says "not sure about this") | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Second volley: evil, pointing: "A second volley is on the way. Right over the Village Centre."; five meteors 8 seconds apart (round the centre to the storage pit); then 20 seconds | pending | `SetTarget` is real (not seen in play yet) |
| Third volley: evil: "Another salvo! Heads down!"; five meteors 6 seconds apart round the large building beside the centre (the fourth and fifth each wait for the one before to land); then 15 seconds | pending | `SetTarget` is real (not seen in play yet) |
| Fourth volley: evil: "Incoming! This is madness. Although it looks totally superb."; five meteors 5 seconds apart round the crèche; then 10 seconds | pending | `SetTarget` is real (not seen in play yet) |
| Fifth volley: evil: "Hey, there's some left. The attack seems to be ending, though."; five meteors 1 second apart round the village centre and the Wonder; then 8 seconds | pending | `SetTarget` is real (not seen in play yet) |
| Good advisor: "Phew.  It's really over this time." In all 27 meteors fall: the opening one, the warning shot and 25 in the volleys | pending | `SetTarget` is real (not seen in play yet) |

## The verdict

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 4 seconds the villagers are counted again; the loss is the start count minus the new count (births during the attack make up for deaths) | pending | `IdSize` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| No villagers lost (and more than one still living): good: "You kept the Village safe! Wonderful! " | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fewer than 8 lost: good: "Well, most people survived, but there were casualties." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 8 or more lost: the evil advisor: "Hey. It's been carnage. Way to go, Boss!" (the source comment gives it to the good advisor) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the village has one villager or none left, nothing is said about the people | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The damage share is (starting health minus final health) divided by the starting health; the source notes about 0.39 for a village left completely unhelped | pending | `GetTownAndVillagerHealthTotal` is real; the quest is not reached in play |
| 22% or less: good: "You protected the buildings, Leader. Super." in a short film that turns the camera to a spot about 25 from the centre (2 seconds) and moves it 20 up and 20 off (4 seconds); alignment +1 and the reward | pending | the health total is real; the quest is not reached in play |
| More than 22% up to 30%: good: "A few buildings got damaged, I'm afraid."; alignment 0, no reward | pending | the health total is real; the quest is not reached in play |
| More than 30%: evil: "Whew. The Village took a serious pounding there, Boss."; alignment -1, no reward | pending | the health total is real; the quest is not reached in play |
| The challenge is then recorded as finished (success 1) with that alignment, whatever happened | pending | `UpdateSnapshot` is real |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest falls from the sky at that spot by the village centre, holding the Itchy creature miracle seed (cast on a creature to make it itchy) | pending | `CreateReward` is real; see [../rewards.md](../rewards.md) |
| No reward film: the chest is dropped without the camera following it (the good verdict's own short film is the only camera move) | pending | `CreateReward` gives the chest now; not yet checked in this quest |
| If it is the first chest of the game the advisors explain it ("click the Action Button on it to open it"); otherwise, when opened, the reward's own help line is spoken if the dialogue is free | pending | the reward chest is real, not yet checked in this quest; by Land 5 it is never the first chest in a normal game |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever burnt or was smashed stays so; the script repairs nothing and the challenge never comes back | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A meteor caught and kept in the hand only bursts once let go and lying still, even after the challenge is over | pending | the in-hand property is read (`GetProperty` IN_HAND); not seen in game |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds: the low temple-explosion rumble with the opening shake, the lava-bomb trail at the tower for every launch, the temple-explosion bang at every burst | partial 40% | `PlaySoundEffect` works and the sounds are in our audio tables; the scripts never reach them; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature takes no part in the script: it is never called, moved or checked, though it can catch or put out fires on its own if it has learnt to | pending | see ../../creature/ |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The opening meteor's fire and temperature are set again a few seconds into the film, by when its burst script may already have deleted it; nothing visible comes of it | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The warning shot isn't watched for a catch, so catching it is never praised | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The villager verdict is measured on the town's net head count, so babies born during the attack hide deaths | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The volley lines in the source are marked "TEST", and the fifth volley borrows the line written as the new first one ("there's some left"), which is why it says the attack is ending | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

