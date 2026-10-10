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
openblack never runs Land 5's control script (the land-loading native does nothing and the story always begins with Land
1's control script), so this challenge never starts. Of the 51 script functions it and the scripts it runs need, 42 work
in `src/CHLApi.cpp`, among them making a rock, setting it on fire and its temperature, casting a spell at a place, the
dialogue, the advisors and the camera moves; the other 9, among them throwing an object at a target, the town's health
total, the challenge record and the reward chest, only log "not implemented". Every row is todo unless the notes say
otherwise.

**Progress: 0/57 done, 4 partial — 4%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge is started by the land's control script, only the first time the player wins the Greek village (Nemesis's village round the Greek Wonder, the land's town 4) | todo | `LandControl5` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| That first capture plays the land's own story scene first (a Greek farmer, the Wonder blown up by an explosion, the creature curse recorded, a tornado that carries the farmer off, the Nemesis theme); the meteors are started as that scene fades back in | todo | the scene belongs to the land's story, see [../land_5.md](../land_5.md) |
| Losing the village to Nemesis and winning it back never starts it again (it is started once, on the first capture only) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| No scroll is put up: the scroll over a villager's hut and the advisor's "Your attention is required here." call are written in the script but switched off, so the player is never asked to accept it | todo | the hut is still looked up but never used; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| It waits 300 seconds (five minutes) after the capture before anything happens (the source notes "Should be 300") | todo | the wait is a script sleep (the VM's own); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Then it waits until the camera is within 150 of the village centre with the centre on screen, the hand holds nothing, and the opening meteor's spot (about 120 from the centre) is not on screen | todo | `PosFieldOfView` works, `GetObjectHeld` is a stub |
| The village it watches is whatever town is within 150 of the village centre at that moment; its villager count is taken now | todo | `CallNear` and `IdSize` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The challenge runs to its end once begun: nothing in it reacts to Nemesis taking the village back, and it has no time limit or failure | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Everything stops when Nemesis loses his last village (the land then stops every script but its own) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The meteors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A meteor is a rock set on fire at a temperature of 2000 and not blown by the wind | partial | our tree makes the rock (`CreateScriptObject`), sets it on fire (`SetOnFire`) and sets its temperature (`SetTemperature`); `SetAffectedByWind` is a stub; the challenge never runs; see [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md) |
| Volley meteors are made at half size, at a launch point 250 high about 400 from the village, and thrown to land on their target spot in 4 seconds | todo | `SetTarget` (throwing at a place) is a stub |
| While a meteor flies (and is not in the hand) it trails a bonfire flame effect, remade every frame at eight times normal size, so it leaves a long burning plume | todo | `SpecialEffectPosition` works and the flying property is read; the in-hand property is not ported; never thrown (`SetTarget` stub) |
| Each launch plays the lava-bomb trail sound at the village's tower, not at the meteor | todo | `PlaySoundEffect` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| At least 4 seconds after it stops flying, and once it is out of the hand and lying still, the meteor blows up: a small level-1 fireball (radius 1, 2 seconds) at a random point up to 2 sideways and 2 above it, the temple-explosion sound, and the rock is deleted with an explosion | partial | `SpellAtPos`, `PlaySoundEffect`, `ObjectDelete` and rock creation work; the wait reads the in-hand property, not ported; the challenge never runs |
| A meteor caught in the hand doesn't burst while held; it bursts wherever it comes to rest after being thrown or dropped | todo | the in-hand property is not ported |
| If a volley meteor is in the hand 6 seconds after its launch, the good advisor comes out once: "Oh I say. Well caught." (only once in the whole game) | todo | `GetObjectHeld` is a stub |
| The damage done comes from the burning rocks themselves (fire, impact) and their fireballs; the script hurts nothing directly | partial | burning objects and fireballs exist outside the script (`src/ECS/Fire`, `SpellAtPos`); not checked that a burning rock sets a building alight; the challenge never runs |

## The opening

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning meteor appears on the ground about 120 from the village centre (made at the spot itself, not thrown), with the lava-bomb trail sound | todo | rock creation, `SetOnFire` and `PlaySoundEffect` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A film starts: the camera shakes round the meteor (radius 500, amplitude 1, 1.5 seconds) with the low temple-explosion rumble | todo | `ShakeCamera` and `PlaySoundEffect` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The evil advisor comes out pointing at it while the camera turns to it over 1 second: "Hey. Fire! Over there!" and goes home | todo | `SpiritEject`, `SpiritPointPos`, `MoveCameraFocus`, `RunText` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera moves to look at it from close by (15 up, 15 and 35 off) over 3 seconds; the challenge is then recorded as started, titled "The Heavenly Fire", with the reminder "We need to protect the Village! Nemesis is attacking!" from the good advisor | todo | `Snapshot` is a stub |
| Both advisors come out; good: "It's a meteor. Thank goodness it missed the Village."; then 2 seconds pass | todo | `SpiritEject` and `RunText` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A warning shot (a full-size meteor) is thrown from the launch point at the near edge of the village (about 70 from the centre) | todo | `SetTarget` is a stub |
| The evil advisor sits low on the right of the screen and the good advisor half-way down on the left; evil: "Heads up! There's more incoming!" while the camera turns to the launch point over 2 seconds and then follows the meteor down | todo | `ClingSpirit`, `SetFocusFollow` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| After 2 seconds, good: "They're heading towards the Village! " and "We've got to protect the people!"; both go home | todo | `RunText` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| After 2 more seconds the camera looks down below the launch point (2.5 seconds) and moves to a view over the village from about 95 away (3 seconds), and the film ends | todo | the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The village's total building and villager health is taken as the starting figure, then 1 second passes | todo | `GetTownAndVillagerHealthTotal` is a stub |

## The five volleys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First volley: the evil advisor points at the launch point: "Yay, more! These are cool. See if you can catch some, Boss." Five meteors follow at five spots across the village, the last at the tower in the centre | todo | `SetTarget` stub |
| In the first volley each meteor after the first waits for the one before to land (6 seconds and until it stops flying or is caught) and then 8 more seconds, so they come about 14 seconds or more apart | todo | `SetTarget` stub |
| After it, good: "They've stopped. Let's clean the place up."; then 20 seconds pass | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Before each later volley the script waits until the camera is within 200 of the tower; looking away holds the next volley back (the source itself says "not sure about this") | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Second volley: evil, pointing: "A second volley is on the way. Right over the Village Centre."; five meteors 8 seconds apart (round the centre to the storage pit); then 20 seconds | todo | `SetTarget` stub |
| Third volley: evil: "Another salvo! Heads down!"; five meteors 6 seconds apart round the large building beside the centre (the fourth and fifth each wait for the one before to land); then 15 seconds | todo | `SetTarget` stub |
| Fourth volley: evil: "Incoming! This is madness. Although it looks totally superb."; five meteors 5 seconds apart round the crèche; then 10 seconds | todo | `SetTarget` stub |
| Fifth volley: evil: "Hey, there's some left. The attack seems to be ending, though."; five meteors 1 second apart round the village centre and the Wonder; then 8 seconds | todo | `SetTarget` stub |
| Good advisor: "Phew.  It's really over this time." In all 27 meteors fall: the opening one, the warning shot and 25 in the volleys | todo | `SetTarget` stub |

## The verdict

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 4 seconds the villagers are counted again; the loss is the start count minus the new count (births during the attack make up for deaths) | todo | `IdSize` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| No villagers lost (and more than one still living): good: "You kept the Village safe! Wonderful! " | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Fewer than 8 lost: good: "Well, most people survived, but there were casualties." | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| 8 or more lost: the evil advisor: "Hey. It's been carnage. Way to go, Boss!" (the source comment gives it to the good advisor) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| If the village has one villager or none left, nothing is said about the people | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The damage share is (starting health minus final health) divided by the starting health; the source notes about 0.39 for a village left completely unhelped | todo | `GetTownAndVillagerHealthTotal` is a stub |
| 22% or less: good: "You protected the buildings, Leader. Super." in a short film that turns the camera to a spot about 25 from the centre (2 seconds) and moves it 20 up and 20 off (4 seconds); alignment +1 and the reward | todo | needs the health total (stub) |
| More than 22% up to 30%: good: "A few buildings got damaged, I'm afraid."; alignment 0, no reward | todo | needs the health total (stub) |
| More than 30%: evil: "Whew. The Village took a serious pounding there, Boss."; alignment -1, no reward | todo | needs the health total (stub) |
| The challenge is then recorded as finished (success 1) with that alignment, whatever happened | todo | `UpdateSnapshot` is a stub |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest falls from the sky at that spot by the village centre, holding the Itchy creature miracle seed (cast on a creature to make it itchy) | todo | `CreateReward` is a stub; see [../rewards.md](../rewards.md) |
| No reward film: the chest is dropped without the camera following it (the good verdict's own short film is the only camera move) | todo | `CreateReward` stub |
| If it is the first chest of the game the advisors explain it ("click the Action Button on it to open it"); otherwise, when opened, the reward's own help line is spoken if the dialogue is free | todo | no reward chests; by Land 5 it is never the first chest in a normal game |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever burnt or was smashed stays so; the script repairs nothing and the challenge never comes back | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A meteor caught and kept in the hand only bursts once let go and lying still, even after the challenge is over | todo | the in-hand property is not ported |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge plays no music of its own (the capture scene before it stops the Nemesis theme) | n/a | nothing to do |
| Sounds: the low temple-explosion rumble with the opening shake, the lava-bomb trail at the tower for every launch, the temple-explosion bang at every burst | partial | `PlaySoundEffect` works and the sounds are in our audio tables; the scripts never reach them |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature takes no part in the script: it is never called, moved or checked, though it can catch or put out fires on its own if it has learnt to | todo | see ../../creature/ |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The opening meteor's fire and temperature are set again a few seconds into the film, by when its burst script may already have deleted it; nothing visible comes of it | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The warning shot isn't watched for a catch, so catching it is never praised | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The villager verdict is measured on the town's net head count, so babies born during the attack hide deaths | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The volley lines in the source are marked "TEST", and the fifth volley borrows the line written as the new first one ("there's some left"), which is why it says the attack is ending | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Cut and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scroll over a villager's hut, pointed out by the good advisor with "Your attention is required here.", was planned and switched off | n/a | never in the game |
| The challenge's lines 6 and 8 to 11 are missing from the text table: the old volley announcements were cut and replaced by the five new Land 5 lines | n/a | never in the game |
| The tower and storage pit are looked up but never used, nor is the villager's hut | n/a | never in the game |
