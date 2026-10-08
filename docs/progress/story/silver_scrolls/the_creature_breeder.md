# The Creature Breeder

A standing silver scroll over a breeder's kennels on Land 1 and Land 4: the breeder shows whichever of the five special
creatures (leopard, horse, mandrill, gorilla, rhino) the player has unlocked, plus the creature they last swapped away,
and lets the player swap their creature's body for one of them. Without unlocked creatures and before any breeder swap,
he only apologises. The swap itself is in [creature_swaps.md](./creature_swaps.md).

**Land:** 1 and 4 (also prepared for 2 and 5, never started there) · **Giver:** the creature breeder, a man standing by the kennels · **Script:** CreatureBreeder · **Reward:** a special creature's body for the player's creature · **Repeatable:** yes (the scroll comes back forever)

**Progress: 1/28 done, 11 partial — 23%**

Sources: the breeder script and its land launcher (the original source text, matching the PC game's compiled
`challenge.chl`), the two land control scripts that start it, the land map scripts, the game's text table and the
executable's creature-availability check. openblack's state is judged on this tree: Land 1's control script starts it
only in a game that skips the creature training (the start-up box's fourth answer), and Land 4's control script never
runs (the land-loading native does nothing); the villager, highlight, dialogue and camera commands it needs work, but
`IsCreatureAvailable`, `SwapCreature` and the creature commands are stubs in `src/CHLApi.cpp`, and `Create` makes no
creatures. Every row is todo unless it says otherwise.

## Where and when it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1 starts the breeder as soon as the player's creature has been chosen (or straight away when skipping to creature selection), alongside the explorers' quest | partial | with the fourth tutorial answer `LandControl1` starts it straight away (no creature choice); with the second and third the creature glade never ends, with the first the intro never finishes |
| Land 4 starts it at the beginning of the land, with the fish puzzle and before the ogre | todo | no Land 4 in our tree: `LOAD_MAP` is empty, so Land 4's control script never runs |
| Land 1's kennels are a Celtic house; Land 4's are a Norse house in the neutral Norse village in the far east | partial | Land 1's kennels are a house made by `Land1.txt` and found with `CALL` (works); Land 4 is never reached |
| Land 2 and Land 5 positions are prepared too (each exactly on a Celtic house in a neutral Celtic village) but neither land's control script starts the breeder; only test launchers outside the compiled file do | n/a | never started by the game on those lands |
| A breeder (an ordinary man) is made at his start spot; he can't be hurt or picked up | partial | `CREATE` makes the villager (`VillagerArchetype`); `SET_INDESTRUCTABLE` and `SET_ID_PICKUPABLE` work; not checked in game |
| The breeder's script runs for as long as the land does | partial | the script loops for as long as the land runs; not checked in game |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ten seconds after the breeder starts (and after every visit) a silver scroll appears 5 above the kennels | partial | `CREATE_HIGHLIGHT` and the height (`SET_PROPERTY` of the Y position) work (`src/ECS/ScriptHighlight.cpp`); not checked in game |
| While the camera is within 100 of it and it is on screen, at most every 30 seconds the evil advisor pops out, points at it and says "There's a Silver Reward Scroll down here, Boss." | partial | the shared notice script runs on working natives (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, `SPIRIT_EJECT`, `SPIRIT_POINT_POS`, `RUN_TEXT`); not checked in game |
| Clicking the scroll or the kennels opens it; the scroll is removed | partial | `GAME_THING_CLICKED` and `OBJECT_DELETE` of a highlight work; not checked in game |
| No challenge record is made: the line that would have opened one is commented out, and the title it named is "The Lost Brother" (another quest's title, with the Lost Brother's reminder), so the breeder never shows in the challenge log | done | the compiled script makes no record, and our tree runs it as compiled |

## The breeder's show

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The current species of the player's creature is recorded | todo | `GAME_SUB_TYPE` is a stub (answers 0) |
| A cut scene: the breeder walks to his soap box and the camera glides to the kennels over 2 seconds | partial | `MOVE_GAME_THING` on a villager and the camera moves work; not checked in game |
| Each special creature that is unlocked and is not the player's current species is shown, in the order leopard, horse, mandrill, gorilla, rhino: a target sparkle for 3 seconds and the creature appears; each is placed about 6 further along a line from the last | todo | `IS_CREATURE_AVAILABLE` is a stub that answers no, and `CREATE` does not make creatures |
| Whether a special creature is unlocked comes from a stored code in the game's settings (a value and its checksum); if it is missing or doesn't match, none are unlocked; every other species always counts as available | partial | `IS_CREATURE_AVAILABLE` is a stub that answers "no", which matches a game with no unlocks but never reads one; how the player got the codes is outside the game files |
| The player's previous creature is shown too, if a breeder swap has happened, it differs from the current species and it isn't already among the special creatures shown | todo | no breeder swap ever happens (`SWAP_CREATURE` is a stub) |
| With nothing to show, the breeder says "Sorry. I don't have any special Creatures for you." and walks back; this is what every player without unlocked creatures sees before any breeder swap | partial | this is the branch our tree always takes (nothing is available); `RUN_TEXT` and the walk back work; not checked in game |
| With one creature: a two-subject camera on it and the breeder, and "Hello. I have this special Creature for you." | todo | nothing is ever shown (`IS_CREATURE_AVAILABLE` is a stub); `START_DUAL_CAMERA` itself works |
| With several: the camera frames the first and last, and "Look. I have these wondrous Creatures you can choose from." | todo | nothing is ever shown (`IS_CREATURE_AVAILABLE` is a stub); `START_DUAL_CAMERA` itself works |
| Each shown creature gets its own swap scroll and offer (see [creature_swaps.md](./creature_swaps.md#the-breeders-offer)) | todo | nothing is ever shown (`IS_CREATURE_AVAILABLE` is a stub); `START_DUAL_CAMERA` itself works |
| The breeder walks back to his start; the scroll stays away until the offer closes | todo | nothing is ever shown (`IS_CREATURE_AVAILABLE` is a stub); `START_DUAL_CAMERA` itself works |

## The advisors' chat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cut scene where both advisors discuss swapping: evil "So Boss, you can swap your Creature if you like." good "Yes, but do we really want to switch to a new Creature after all the training we've done?" evil "Nah, you got it wrong. Our Creature's mind will get transferred across." good "So it's just the body that's changing?" evil "Yeah. You got it. Kinda like cosmetic surgery." | todo | plays only when something is shown, which never happens in our tree |
| It plays only when something is shown and the old and current species are both the ape (after swapping an ape away at the breeder and back to an ape), or for a "previous species" value that never occurs; in normal play it is almost never heard | todo | plays only when something is shown, which never happens in our tree |

## The swap at the kennels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1: the creatures are walked to two spots about 100 from the kennels, on ground some 35 higher; the cut scene's three camera spots are set but not used by the swap | todo | nothing is shown, and `SWAP_CREATURE` is a stub |
| Land 4: the creatures are walked to two spots about 30 to 50 from the kennels, at the same height | todo | nothing is shown, and `SWAP_CREATURE` is a stub |
| After the swap the old body walks back to the kennel spot, then vanishes with the others when the offer closes; the old species is kept to be offered next time | todo | nothing is shown, and `SWAP_CREATURE` is a stub |
| An offer that is left alone for its 60 to 120 seconds closes, and every creature on show vanishes in sparkles and smoke | todo | nothing is shown, and `SWAP_CREATURE` is a stub |
| Clicking a second creature while one is chosen cancels the choice | todo | nothing is shown, and `SWAP_CREATURE` is a stub |
| After the offer closes, the scroll returns 10 seconds later and the show repeats with the current unlocks | partial | with nothing shown the scroll comes back and the show repeats, always as "Sorry"; not checked in game |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The swap at the breeder records both species; the swap offered by quests only records the new one, so a creature swapped away by a quest is not offered back at the breeder | todo | `SWAP_CREATURE` is a stub |
| The comment above the breeder's swap asks people not to touch it because "it works and it was tricky to get to work" | n/a | developer note; nothing to do |
| The launcher for Land 1 and Land 4 finds the kennels as a house; the Land 2 and 5 entries use a plain marker | n/a | never started by the game on those lands |
