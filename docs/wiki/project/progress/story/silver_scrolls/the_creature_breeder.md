# The Creature Breeder

A standing silver scroll over a breeder's kennels on Land 1 and Land 4: the breeder shows whichever of the five special
creatures (leopard, horse, mandrill, gorilla, rhino) the player has unlocked, plus the creature they last swapped away,
and lets the player swap their creature's body for one of them. Without unlocked creatures and before any breeder swap,
he only apologises. The swap itself is in [creature_swaps.md](./creature_swaps.md).

**Land:** 1 and 4 (also prepared for 2 and 5, never started there) · **Giver:** the creature breeder, a man standing by the kennels · **Script:** CreatureBreeder · **Reward:** a special creature's body for the player's creature · **Repeatable:** yes (the scroll comes back forever)

**Progress: 0/23 done, 0 chosen, 10 partial, 0 inferred — 25%**

Sources: the breeder script and its land launcher (the original source text, matching the PC game's compiled
`challenge.chl`), the two land control scripts that start it, the land map scripts, the game's text table and the
executable's creature-availability check. openblack's state is judged on this tree: Land 1's control script starts it
only in a game that skips the creature training (the start-up box's fourth answer), and Land 4's control script never
runs (the story reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked); the villager, highlight, dialogue and camera commands it needs work, but
many of the creature commands are real now in `src/CHLApi.cpp` (not checked here) and `Create` makes creatures; `IsCreatureAvailable` answers
yes for every species, as the original, and `SwapCreature` is ported (raffclar PR #384). Every row is pending unless it says otherwise.

## Where and when it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1 starts the breeder as soon as the player's creature has been chosen (or straight away when skipping to creature selection), alongside the explorers' quest | partial 50% | with the fourth tutorial answer `LandControl1` starts it straight away (no creature choice); with the second and third the creature glade, and with the first the intro, have not yet been run to their end; finished by [land1_script.md](../../scripts/land1_script.md) "Loading the land starts the story's control script"; still open: no natural run's log is cited for this start gate |
| Land 4 starts it at the beginning of the land, with the fish puzzle and before the ogre | pending | Land 4's control script has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Land 1's kennels are a Celtic house; Land 4's are a Norse house in the neutral Norse village in the far east | partial 40% | Land 1's kennels are a house made by `Land1.txt` and found with `CALL` (works); Land 4 is never reached; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Finds an object of a kind at a position"; still open: no quest run's PASS is cited for it |
| A breeder (an ordinary man) is made at his start spot; he can't be hurt or picked up | partial 60% | `CREATE` makes the villager (`VillagerArchetype`); `SET_INDESTRUCTABLE` and `SET_ID_PICKUPABLE` work; not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a villager", "Makes an object impossible to destroy", "Whether the player can pick an object up"; still open: no quest run's PASS is cited for it |
| The breeder's script runs for as long as the land does | partial 60% | the script loops for as long as the land runs; not checked in game; still open: no quest run's PASS is cited for it, and it rests on the script's own logic, which no system row finishes |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ten seconds after the breeder starts (and after every visit) a silver scroll appears 5 above the kennels | partial 60% | `CREATE_HIGHLIGHT` and the height (`SET_PROPERTY` of the Y position) work (`src/ECS/ScriptHighlight.cpp`); not checked in game; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Creates a challenge marker", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Sets the height above the land of a highlight"; still open: no quest run's PASS is cited for it |
| Clicking the scroll or the kennels opens it; the scroll is removed | partial 60% | `GAME_THING_CLICKED` and `OBJECT_DELETE` of a highlight work; not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object", "Delete with no mode: a villager"; still open: no quest run's PASS is cited for it |
| No challenge record is made: the line that would have opened one is commented out, and the title it named is "The Lost Brother" (another quest's title, with the Lost Brother's reminder), so the breeder never shows in the challenge log | partial 60% | the compiled script makes no record, and our tree runs it as compiled; no quest_check PASS of the breeder yet, so not checked in a run; still open: no quest run's PASS is cited for it, and it rests on the script's own logic, which no system row finishes |

## The breeder's show

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The current species of the player's creature is recorded | pending | `GAME_SUB_TYPE` is real and answers a creature's species as the scripts number them (`GameSubType` in `src/CHLApi.cpp`, `ecs::script_type::SubtypeOf`); not checked in play |
| A cut scene: the breeder walks to his soap box and the camera glides to the kennels over 2 seconds | partial 60% | `MOVE_GAME_THING` on a villager and the camera moves work; not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds"; still open: no quest run's PASS is cited for it |
| Each special creature that is unlocked and is not the player's current species is shown, in the order leopard, horse, mandrill, gorilla, rhino: a target sparkle for 3 seconds and the creature appears; each is placed about 6 further along a line from the last | pending | `IS_CREATURE_AVAILABLE` answers yes for all five, as the original, and `CREATE` makes them now; not checked in play |
| Every special creature counts as unlocked, and every other species always counts as available | partial 60% | no quest_check PASS of the breeder yet, so the native's code and test are the measured share; `IsCreatureAvailable` with `creature_swap::k_GameUnlocks`, pinned by `CreatureSwap.TheGameHasEveryBonusCreatureUnlocked`; the game reproduced has the unlock test patched to always answer yes ([the unlock](../../../../creature/players-creature.md#the-bonus-creatures-unlock)); unpatched, a stored code and its checksum in the game's settings decide it (a note only); finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Whether a species of creature is available to choose"; still open: no quest run's PASS is cited for it |
| The player's previous creature is shown too, if a breeder swap has happened, it differs from the current species and it isn't already among the special creatures shown | pending | no breeder swap has been checked in play (`SWAP_CREATURE` is real) |
| With one creature: a two-subject camera on it and the breeder, and "Hello. I have this special Creature for you." | pending | `CREATE` makes the creatures shown now; not checked in play; `START_DUAL_CAMERA` itself works |
| With several: the camera frames the first and last, and "Look. I have these wondrous Creatures you can choose from." | pending | `CREATE` makes the creatures shown now; not checked in play; `START_DUAL_CAMERA` itself works |
| Each shown creature gets its own swap scroll and offer (see [creature_swaps.md](./creature_swaps.md#the-breeders-offer)) | pending | `CREATE` makes the creatures shown now; not checked in play; `START_DUAL_CAMERA` itself works |
| The breeder walks back to his start; the scroll stays away until the offer closes | pending | `CREATE` makes the creatures shown now; not checked in play; `START_DUAL_CAMERA` itself works |

## The advisors' chat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cut scene where both advisors discuss swapping: evil "So Boss, you can swap your Creature if you like." good "Yes, but do we really want to switch to a new Creature after all the training we've done?" evil "Nah, you got it wrong. Our Creature's mind will get transferred across." good "So it's just the body that's changing?" evil "Yeah. You got it. Kinda like cosmetic surgery." | pending | plays only when something is shown; `IS_CREATURE_AVAILABLE` answers yes and `CREATE` makes the creatures shown now; not checked in play |
| It plays only when something is shown and the old and current species are both the ape (after swapping an ape away at the breeder and back to an ape), or for a "previous species" value that never occurs; in normal play it is almost never heard | pending | plays only when something is shown; `IS_CREATURE_AVAILABLE` answers yes and `CREATE` makes the creatures shown now; not checked in play |

## The swap at the kennels

The offer's timers, a second click, the old body walking back and every creature vanishing when the offer closes are
in [creature_swaps.md](./creature_swaps.md#the-breeders-offer).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1: the creatures are walked to two spots about 100 from the kennels, on ground some 35 higher; the cut scene's three camera spots are set but not used by the swap | pending | `SWAP_CREATURE` is real; not checked in play |
| Land 4: the creatures are walked to two spots about 30 to 50 from the kennels, at the same height | pending | `SWAP_CREATURE` is real; not checked in play |
| After the offer closes, the scroll returns 10 seconds later and the show repeats with the current unlocks | partial 60% | the script's loop runs on natives that work; with every special creature unlocked the show makes its creatures now (`CREATE`), so the scroll comes back only once their offer has closed; not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a creature"; still open: no quest run's PASS is cited for it |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The swap at the breeder records both species; the swap offered by quests only records the new one, so a creature swapped away by a quest is not offered back at the breeder, while one swapped away at the breeder is offered again on the next visit | pending | `SWAP_CREATURE` is real |
