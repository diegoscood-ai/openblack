# The Plague

A land 2 silver scroll: soon after the player owns the Indian village beside Lethys's side of the land, Lethys's hand
hovers over its village store, and when the scroll is opened the whole village and its store turn out to be poisoned.
The player must find the source (the store's rotten food), get rid of it and heal the sick before half the village
dies. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md). How poison works in general (spreading,
symptoms, a toadstool poisoning a store, healing, emptying a pile) is in
[../../resources/poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md); this file only covers what the
quest does with it.

**Land:** 2 · **Giver:** a sick Indian trader at a hut of the plagued Indian village (Town2), the evil advisor points the scroll out · **Script:** Plague · **Reward:** the level 1 lightning bolt miracle enabled at the village · **Repeatable:** no

Sources: the quest's script source (`Plague.txt`) and its trigger in the land's control script (`LandControl2.txt`),
both checked against the PC game's compiled `challenge.chl` (argument orders, the snapshot values and the shape of the
Lethys wait); the game's English text table for every line and its speaker; the executable (read only) for what
poisoning a town, a store and a villager does. openblack's state is judged on this tree: of the 63 script functions the
quest and the scripts it starts need, none is a stub in `src/CHLApi.cpp` at this tree (the computer player's hand,
the challenge log, the held-object read, the type test and the poison commands are real), and the land's control script never runs (see the
first row of [../land_2.md](../land_2.md)), so the quest never appears; every row is pending unless the notes say
otherwise.

**Progress: 0/53 done, 0 chosen, 5 partial, 0 inferred — 6%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the plague check for Town2 (the Indian village, id 10) at the start of the land, alongside the other towns' quest checks | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real ([../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| The check waits until Town2 belongs to the player and the worship site at the player's own home altar spot (the spot the worship lesson and the "spruce up your worship site" signpost use, also the spot the Sacrifice check watches) is completely built; then it waits 5 more minutes and starts the quest | pending | `CallNear` is real; `GetProperty` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the worship site doesn't exist yet the check looks again every 33 seconds; while Town2 isn't the player's, or the site exists but is unfinished, it loops without any wait | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; quirk of the source; the compiled program is the same |
| The quest first waits (every 5 seconds) until the trader's hut and the village store both exist at their spots | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If Lethys is still on the land, the quest waits for his computer player to be free (not in another script), pauses his AI and flies his hand at speed 200, keeping its height, to just above the plagued village's store | pending | `ComputerPlayerReady`, `EnableDisableComputerPlayer311`/`345` and `MoveComputerPlayerPosition` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lethys's hand stays there while a 30-second timer runs; the timer was started when the quest began, before waiting for Lethys and flying his hand, so the hover can be much shorter than 30 seconds | pending | `CreateTimer`, `GetTimerTimeRemaining` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the player's camera comes within 200 of the store with it on screen during the hover, the evil advisor steps out, points at the store and says "Hey. Lethys is doing something over that Village, Boss." and the hover ends at once; it also ends if Lethys leaves the land | pending | `SpiritEject`, `SpiritPointPos`, `RunText` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lethys's AI is then let go. Nothing visible is cast: his hand only hovers, and the poisoning itself is done by the quest script later | pending | `ReleaseComputerPlayer` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A silver scroll appears 6 above the trader's hut | pending | `CreateHighlight` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The poisoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll adds 5,000 food to the village store | pending | `AddResource` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Every villager then in the village is poisoned at once (poisoning a town poisons each of its members) | pending | `SetPoisoned` poisons each villager of the town (`script_containers::ForEachMember`, `ecs::object_poison::SetPoisoned`; `test/test_script_poison_natives.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Half the village's size at that moment is remembered as the death line for failure | pending | `IdSize` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After the introduction the store is poisoned: its food pile is marked poisoned (its wood is not) | pending | `SetPoisoned` poisons the store's food pile only (`ecs::object_poison::SetPoisoned`; `test/test_script_poison_natives.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../resources/stores_and_piles.md](../../resources/stores_and_piles.md) |
| The spread: a villager who takes food from a poisoned store becomes poisoned, so healed villagers fall sick again when they next eat from the store | partial 60% | the engine side works: taking food from a poisoned store poisons the villager (`GetResourceFrom`, `src/ECS/Villager/VillagerResources.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../resources/food.md](../../resources/food.md); finished by [poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md) "Villagers taking food from a poisoned pile", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| A poisoned villager regains no life by sleeping at home, and after eating while poisoned shows that it is poisoned instead of carrying on | partial 60% | no healing sleep while poisoned (`src/ECS/Villager/VillagerHome.cpp`) and the poison's life loss (`ecs::life::ProcessPoison`) are ported; the shown-poisoned step not checked; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../villager/daily_routine.md](../../villager/daily_routine.md); finished by [poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md) "A poisoned villager does not get life back from resting at home", "After a meal", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| Poisoned villagers can't work or pray, per the trader's complaint | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the claim is the dialogue's; undetermined beyond the eating and resting rules above |
| A "dead flock" is made at the village (inner 5, outer 20) but never used | pending | `FlockCreate`, `ChangeInnerOuterProperties` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene starts with the creature guide's music; an Indian trader comes out of the hut in high detail and walks to a spot in front of it, while the camera glides over (4 seconds) and turns to the hut (3 seconds) | partial 60% | `START_MUSIC` works (`src/Audio/GameMusic.cpp`); villager CREATE, high detail and the camera moves are real too; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", "Draws an object in full detail however far away it is", "Create makes a villager", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| One second after he arrives he faces the camera and plays his poisoned animation once | pending | `SET_FOCUS` and `SET_SCRIPT_STATE` work on villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor: "What's got into him?" as the camera closes in on him over 8 seconds; the advisor goes home and the dialogue closes | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The challenge is recorded in the player's challenge log: title "The Plague", success 0, alignment 0, reminder spoken by the good advisor: "The tribe has been struck down by poison. Do something!" (the snapshot stores the challenge with its title and the reminder script the log replays) | pending | `Snapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| When his animation ends he faces the camera again and plays a despairing stand twice: "We've been poisoned! We can't work, we can't pray and we can't worship you. Please help us!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera glides back to where the player had it over 14 seconds; both advisors step out | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor: "We should stop this plague from spreading." then "Let's find the source of this sickness and heal the ill." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Both advisors go home; once the camera is back the trader is poisoned and let go to live in the village, high detail and music end | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Empty the village store of food (all of it: the 5,000 added plus whatever it had), or get rid of the store altogether; either counts as removing the poison's source | pending | `GetResource` is real (`src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Heal the sick with the heal miracle | partial 60% | the heal miracle cures poison (`src/Magic/Core/SpellEvent.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../miracles/heal.md](../../miracles/heal.md); finished by [heal.md](../../miracles/heal.md) "It cures poisoning", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| Win when the source is gone and no more than a quarter of the village's current people are still poisoned, with at least one person left | pending | `IdSize` and `IdPoisonedSize` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lose when the village falls below half the size it had at the poisoning, before winning | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest has no time limit; it waits for one of these outcomes | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Comments while it runs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first time anyone in the village is not poisoned (normally the first one healed), the good advisor: "You healed one. That's good." That villager is remembered | pending | `CallNotPoisonedIn` is real (the first villager of the town, in its order, who is not poisoned); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If that remembered villager falls sick again, a man's voice: "Godly being, stop healing and find the source of the sickness!"; the log is updated to success 0, alignment 0.4, reminder (good advisor): "Perhaps you should stop healing and find out the cause of the illness." Once only | pending | `IsPoisoned` and `UpdateSnapshot` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the store still has food, the first time the hand holds poisoned food (a handful taken from the store), the good advisor: "Ugh. That food looks rancid." Once only | pending | `GetObjectHeld199` and `IsOfType` are real (`src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../hand/multi_pickup.md](../../hand/multi_pickup.md) |
| When the store is empty (or gone), the good advisor: "The poison's source is the Village Store. We can heal them now."; the log is updated to success 0.5, alignment 0.3, reminder (good advisor): "You got rid of the poison but the people need to be healed." This also stops the "You healed one" line from ever playing | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The first time, after the store is empty, anyone in the village is unpoisoned, the log is updated to success 0.75, alignment 0.3, with the reminder "Lovely. The poison's source is gone and the people you heal will survive."; then good advisor: "You're healing more now, Leader. Excellent." and "Lovely. The poison's source is gone and the people you heal will survive."; evil advisor: "And those survivors can get a little worship done." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On winning, the quest waits until the camera is within 100 of the spot in front of the trader's hut with it on screen | pending | `GameThingFieldOfView`, `PosFieldOfView` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A new trader comes out of the hut to the spot; a cut scene closes the camera on him (3 seconds, focus 2), he faces the camera and prays three times | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log is updated to success 1, alignment 0.4, with the reminder (good advisor) "The sick people aren't getting any better." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the same reminder as the failure ending, which reads oddly after a win |
| Trader: "Praise be to you, godliness! We're going to make it! Although we've suffered, our faith has endured!"; two seconds later he is let go | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On losing (under half the people left), the quest waits until the camera is within 100 of the spot in front of the hut with it on screen | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A trader comes out of the hut to the spot; the camera closes in (3 seconds, focus 2); one second after he arrives he faces the camera | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log is updated to success 1 (the challenge is over), alignment −0.4, with the reminder (good advisor) "The sick people aren't getting any better." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Trader: "Call yourself a greater power? You did nothing and now we're almost wiped out! The survivors won't forget this!"; a second later the camera returns to where the player had it and he is let go | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If everyone in the village has died, the good advisor steps out, points at the spot and says "Take people from your other Village. Everyone here has died." and "But you'll need to cure this Plague, though, Leader." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; in practice this comes right after the failure scene, since an empty village is also below half; the trader is still made at the hut |
| There is no other way to fail or abandon it: the village staying sick, or leaving it alone, only leads to the failure above as people die | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On success the lightning bolt (level 1) miracle is enabled at the village's worship site (the town found within 100 of the village centre) | partial 60% | `SET_MAGIC_IN_OBJECT` puts a miracle in a town (`src/Magic/Script/CHLWorship.cpp`); the lightning miracle exists; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../miracles/lightning.md](../../miracles/lightning.md); finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Puts a miracle inside an object", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| A cut scene pans the camera to the village centre (5 seconds, focus 2); evil advisor: "Lightning Bolt. A nifty item, I feel." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A failure gives no reward | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After either ending, if the village has fewer than 10 people, the evil advisor steps out, points at the spot by the hut and says "There aren't many people here, Boss. Take some from your other Village." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Then every villager still in the village is cured at once, whichever way it ended; the script also clears the store's poison, which in the original leaves a poisoned food pile as it is (a store's food can only be poisoned) | pending | `SetPoisoned` is real (`ecs::object_poison::SetPoisoned`; `test/test_script_poison_natives.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Villagers who died stay dead; the village keeps whatever it lost | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest gives the creature no part; it may empty the store (eating its food or taking it) or heal villagers like the player, since only the outcome is checked | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; creature eating poisoned food: [../../creature/feeding_and_thrown_things.md](../../creature/feeding_and_thrown_things.md) |

## Script quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's main loop has no wait of its own: it checks every condition continuously | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; as compiled |
