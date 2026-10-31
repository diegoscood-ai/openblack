# The Riddles

A land 2 silver scroll: once the player owns the second Indian village (Town3), a woman there offers an ancient riddle
whose answer is four things to be put at once inside a ring of toadstools in the woods nearby: a wolf, something hot,
a creature's dropping and a shield miracle. Solving it raises the "great power" the riddle promised: a zebra creature
the player may swap to. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Annika, a woman of the Indian village Town3, at her hut · **Script:** LostTreasure · **Reward:** a zebra creature to swap to · **Repeatable:** no

Sources: the quest's script source (`LostTreasure.txt`), the shared creature-swap script it ends with
(`SwapCreatures.txt`), the scroll-notice and reminder helpers, and its trigger in the land's control script
(`LandControl2.txt`), all checked against the PC game's compiled `challenge.chl` (object types, distances, which shield
is which); the game's English text table for every line and its speaker. openblack's state is judged on this tree: of
the 67 script functions the quest and the scripts it starts need, 5 only logged "not implemented" in `src/CHLApi.cpp`
when they were counted on an earlier tree (at this tree the town-owner and health reads, making a creature and the
creature commands are real), and
the land's control script never runs (see the first row of [../land_2.md](../land_2.md)), so the quest never appears;
every row is pending unless the notes say otherwise. The internal script name ("lost treasure") is not shown to the player;
the scroll is titled "The Riddles".

**Progress: 0/43 done, 0 chosen, 4 partial, 0 inferred — 6%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the check for Town3 (the Indian village, id 13) at the start of the land | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real ([../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| Every 6 minutes it looks whether Town3 belongs to the player; the first time it does, the quest starts (so up to 6 minutes after taking the village) | pending | `GET_PROPERTY` of a town's player is real now (`GetProperty` in `src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A silver scroll is made over Annika's hut (the house found within 5 of its spot), 5 above it | pending | `CreateHighlight` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Eight toadstools at 0.8 size are made in a ring of radius 10 around the riddle's spot, 45 degrees apart, each turned to face the centre: the "ring of life" | pending | `CREATE_WITH_ANGLE_AND_SCALE` makes features (`CreateScriptObject` in `src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The riddle (Annika's scene)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene starts with the second generic script music; Annika (an Indian housewife) comes out of her hut in high detail and walks to a spot in front of it | partial 60% | `START_MUSIC` works (`src/Audio/GameMusic.cpp`); villager CREATE, high detail and `MOVE_GAME_THING` on villagers are real too; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", "Draws an object in full detail however far away it is", "Create makes a villager", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| The camera glides in over 3 seconds, holds 2 seconds, then closes on the hut over 3 more; when she has arrived and the camera stopped, she faces the camera, which then drifts slightly over 8 seconds | pending | `MoveCameraPosition`, `MoveCameraFocus` are real and `SET_FOCUS` works on villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The challenge is recorded in the player's challenge log: title "The Riddles", success 0, alignment 0, and as its reminder this whole scene, so replaying the reminder (or tapping the scroll again) tells the riddle again | pending | `Snapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| If her hut is damaged (below full health), the evil advisor: "The woman's saying nothing until her house is all fixed up again. Huh." and she walks home without telling the riddle | pending | `GET_PROPERTY` health is real now (`GetProperty` in `src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Otherwise, gossiping: "I have an ancient riddle for you. Solve it and great power will be yours." | pending | `RunText` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera cuts to above the ring and glides down to it over 12 seconds while she gives the clues: "Place in the ring something which howls at night." / "Something hot." / "Something unique to your Creature." / "Then all must be protected." / "You need to place all the ingredients in the ring of life at the same time." with three gossip animations between them | pending | `SetCameraPosition`, `SetCameraFocus` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera cuts back to her: "Knock on my house and I'll remind you of the clues." Two seconds later the camera glides back to where the player had it (4 seconds) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the "knock" is the scroll staying active over her hut: tapping it replays the reminder, which is this scene |
| She walks back into her hut and fades away; the music stops | pending | `OBJECT_DELETE` on a villager is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After the first telling, the good advisor: "This looks promising. I wonder what we'll be getting?" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The wolves and the ring's watch start after the first telling, even if her hut was damaged and the clues weren't given | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; quirk of the source |

## The answers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Something which howls at night": any wolf within 10 of the ring's centre, scripted or not (it need not be one of the night pack) | pending | `CallNear` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| "Something hot": anything burning within 10 of the centre; a rock heated to 500 or more counts, as does a burning tree, hut or fire | partial 60% | the fire test works (`IS_FIRE_NEAR`, `src/Magic/Script/CHLFire.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md); finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether there is fire within a radius of a position", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| "Something unique to your Creature": any creature dropping within 10 of the centre; it need not be the player's creature's | pending | `CallNear` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../creature/physiology.md](../../creature/physiology.md) |
| "Then all must be protected": a physical shield or a spiritual shield miracle within 5 of the centre | pending | openblack's `SPELL_AT_POINT` (`SpellAtPoint` in `src/Magic/Script/CHLSpells.cpp`) never finds a shield, where the original gives back the shield covering the point (test `MagicSystemTest.NoShieldIsEverFound` pins openblack's side; [spell core, Pending](../../../../magic/spell-core.md#pending); miracles/miracle_core.md "A script can find a miracle already at a point…", partial 70%), so this answer is never seen and the riddle can't be solved; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../miracles/physical_shield.md](../../miracles/physical_shield.md), [../../miracles/spiritual_shield.md](../../miracles/spiritual_shield.md) |
| All four must be there at the same moment; they are checked in order (wolf, then a tenth of a second later dropping, shield and fire) and the checking goes on until solved | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nothing checks the order they arrive in, and nothing reacts to a wolf escaping or a fire going out; there is no time limit and no failure | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; lines for these exist but are unused |
| The player may bring the four by any means (the hand, the creature, miracles) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the scripts only test positions; undetermined from them how a wolf is best kept inside |

## The night wolves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the first telling on, every night (game time after 21:00 or before 6:00) a pack of five wolves (inner 5, outer 10) appears in a forest near the ring, once that spot is off screen | pending | `FlockCreate`, `GetGameTime` are real; `PopulateContainer` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Two seconds later they go down to the water's edge, and once within 35 of it (or all dead) wander there as a flock | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| At day (after 6:00 and before 21:00) they go back into the forest and, once within 35 of it (or all dead) and the spot is off screen, the pack is removed | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The pack comes back every night for the rest of the land, even after the riddle is solved: the flag meant to stop it is never set by any script | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; quirk of the source |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When solved, the wolf and the dropping fade away and the shield found is removed (the physical shield if both kinds were there, leaving the spiritual one); the fire is left | pending | `OBJECT_DELETE` is real for these; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cut scene starts with the fourth epic music; the camera moves to look down at the ring from 15 out and 10 up over 3 seconds | partial 60% | `START_MUSIC` and the camera moves work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| The log is updated to success 1, alignment 0 (neutral), with the reminder (good advisor) "Remember, we should ask the woman in this hut about her riddles." | pending | `Snapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Both advisors step out; good advisor: "This looks promising. I wonder what we'll be getting?" (the source's comment shows a different first line about a groovy effect was meant here) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A zebra creature appears in the ring, turns to the camera and waves; the music stops | pending | CREATE makes a creature now (`CreateScriptObject`) and `CREATURE_DO_ACTION` is real (a row with an executor runs); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor: "Look. It's a zebra. What do we want with that?"; good: "If we want to swap to the Zebra, summon our Creature and click on him."; evil: "Yeah, yeah. Do we really want a Zebra, though?"; good: "A rare beast indeed. I'm all for it." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the last line isn't waited for before the dialogue ends |

## Reward: the zebra

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The zebra can't be picked up; a silver scroll stands just above its head | pending | `SetIdPickupable`, `CreateHighlight` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [creature_swaps.md](creature_swaps.md) |
| It keeps turning to look at the camera; with the camera within 100 of its scroll and the zebra on screen, the evil advisor points: "Swap your Creature with this one if you want.", at most every 60 seconds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Clicking the zebra or its scroll: if the player's creature isn't within 50 of it, evil advisor: "You'll need to bring our Creature, Boss." and the scroll returns | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Otherwise the leash is taken off, the player's creature walks up to within 15 (given 5 seconds to get within 20), and the two look each other over | pending | `DetachObjectLeash` is real; `MOVE_GAME_THING` now walks a creature under the script's control (`CreatureMindSystem::MoveForScript`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor every 30 seconds: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel."; clicking the player's own creature, or waiting 110 seconds, cancels | pending | `ClearClickedObject`, `CreateTimer` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Clicking the zebra again swaps: a two-creature camera, sparkles on both, both point at each other, the swap sound plays and the player's creature becomes the zebra; three seconds after both animations end the camera is let go | pending | `StartDualCamera`, `PlaySoundEffect` are real; `SwapCreature` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../../creature/species_choice.md](../../creature/species_choice.md) |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." The old creature stays at the spot under its own scroll and can be swapped back the same way, any number of times | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The offer never ends: the swap script sets up a random 60–120 second limit but never checks it, so the swap scroll stays for the rest of the land | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; quirk of the shared script |

## Advisors, music and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's lines are spoken by the evil advisor (the notice, the damaged-hut line, the zebra doubts), the good advisor (the hopes, the swap instructions) and Annika | pending | `SpiritEject` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../advisors.md](../advisors.md) |
| Music: the second generic script theme for Annika's scene, the fourth epic theme for the zebra's arrival; both are stopped at the end of their scenes | partial 60% | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio/GameMusic.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", "Stops the script's music", [land_2.md](../land_2.md) "Khazar introduces himself"; still open: no quest run's PASS is cited for it |
| The creature can supply the dropping and could carry or throw things in, but the quest asks nothing of it until the swap | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Script quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ring's watch has no wait while no wolf is in the ring: it checks continuously | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; as compiled |
