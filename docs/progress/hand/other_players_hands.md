# Other players' hands

Every god in a game has a hand of its own: the rival gods of the story and of skirmishes move theirs about the land to
pick things up, throw, cast and work their creatures, and in network games each player sees the others' hands. The rules
of network play are in [../multiplayer/](../multiplayer/).

**Progress: 2/27 done, 0 chosen, 15 partial, 0 inferred — 41%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Each player's hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player has a hand of their own, with what it holds, where it is, its speed, and the camera it looks from | partial 70% | One `HandSystem` for the local player (its hand entities, held object, spring velocity, synced hand and camera in the turn state, `HandTurn.cpp`); each computer player has a `ComputerHand` on its entity with its point, speeds, steps and what it holds (`src/ECS/Components/ComputerHand.h`, see [the computer hands](../../../hand/computer-hands.md#openblack)); network players have no hand |
| A hand holds one thing (or one handful) at a time; a full hand takes nothing more | partial 80% | Done for the local hand (`HandSystem::ApplyPlaceInHand` refuses a full hand) and for a computer hand, which keeps its held flag and the one thing it holds (`computer_hand::PickUpTurn`, `test/test_computer_hand.cpp`); no network hands |
| Each player's hand remembers the last thing it picked up and the last it dropped, forgetting them when they are destroyed | partial 50% | The last thing dropped is kept per player, the player's hand and the computer hands alike, a later drop by another hand taking it over (`ecs::last_dropped`, `src/ECS/LastDropped.cpp`; tests in `test/test_building_sites.cpp`, `test/creature/test_creature_mimic.cpp`); nothing keeps the last thing picked up |
| Things a hand holds that are destroyed leave it at the next turn | partial 85% | `HandSystem::ValidateHands` (`HandTurn.cpp`) drops a held object that is no longer interactable at the turn; a computer hand sweeps gone things from its steps and hold each turn (`computer_hand::SweepGone`, `test/test_computer_hand.cpp`); no test of the player's hand's part, no network hands |
| Each hand has its own leash | partial 40% | `LeashSystem` keeps each leash's player; only the local hand works one (a computer hand's leash step is not ported) |
| A hand's interaction locked onto something carries on each turn only while that thing says so and the hand stays inside its player's influence | partial 50% | The locked select lasts while the object is available, has influence at the hand's point and its interact step says so (`HandTurn.cpp`); the local hand only, a computer hand's locked steps are not ported |
| A hand's place is reported as a point on the map, which miracles and scripts use | partial 80% | `HandSystem::GetPlayerHandPositions` and the synced hand of the turn state (`HandTurn.cpp`); a computer hand keeps its map place (`computer_hand::KeepMapPlace`, `test/test_computer_hand.cpp`); no network hands |
| Only the local player's hand plays the guidance heartbeat and changes the local interface | partial 80% | The guidance heartbeat plays for the local player (`src/Audio/Services/Guidance.cpp`, `test/audio/test_guidance.cpp`); with no network hands, the other half (another player's hand changes nothing local) cannot be tried |

## The rival gods' hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god's hand is not drawn: only what it holds is, sliding with the hand | done | The original draws no computer hand, only what it holds; openblack draws the held thing at the hand's sliding point and no hand (`RenderingSystem::BeginPrepareDraw`, `computer_hand::HeldDrawPoint`, `test/test_computer_hand.cpp`); see [the computer hands](../../../hand/computer-hands.md#what-is-drawn) |
| It moves to a place, to an object or to its creature's head, and can hold over an object | partial 60% | The moves over the ground, straight, to a thing and the put-at (`computer_hand::MoveOverGround`, `MoveStraight`, `MoveToThing`, `PutAt`), through the turn of its steps (`ComputerPlayerSystem::ProcessTurn`); the steps not yet ported fail |
| It moves inside its own influence, keeping its height over the land as it goes | partial 50% | It keeps its height over the land (`computer_hand::HoverOverGround`, `MoveOverGround`); the influence limit is not listed as ported ([the computer hands](../../../hand/computer-hands.md#openblack)) |
| It picks up villagers, rocks, trees, food and wood, and drops them on a town, a worship site, a building site or a store | partial 70% | `computer_hand::PickUpTurn` and `DropTurn` through `GameHandRunner` (taken into the hand as the player's hand takes things; let go with no speed as that player's throw, `computer_hand::LetGoCreditedTo`); a scaffold it takes leaves its site (`scaffolds::OnPickedUp`); a field it picks up gives no food pile yet |
| It throws things at a place, at another god's town or creature | pending | The hand only lets go with no speed (`physics::from_hand::Throw`); no throwing step is ported |
| It gives things to its creature | pending | Not in our tree |
| It strokes and slaps its creature, and puts its leash on things | pending | Not in our tree; see [creature_contact.md](creature_contact.md) |
| It draws gestures and casts miracles from icons or gestures | pending | Not in our tree; see [../miracles/](../miracles/) |
| It combines scaffolds and makes disciples | pending | Not in our tree |
| It takes food and wood out of stores to use | pending | Not in our tree |
| How long each of these takes is estimated before it is started | pending | Not in our tree |
| Another god's fireball can be caught by the player's hand | pending | No magic fireball in our tree |
| Another god's creature can be held and stroked or slapped by the player's hand | done | Our hand holds the player's own creature or an allied player's (`creature_hand::IsFriendlyTo` over the owner's alliances, `test_leash_ownership.cpp`), as the original. Our wiki differs: the original holds only its own or an allied player's creature, not any god's ([page](../../../hand/states.md#the-hand-on-a-creature)) |

## Network play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What a hand does is sent to the other players as messages and applied at the start of the next turn on every machine | partial 50% | The hand's actions go through the packet queue and are applied at the next turn's start (`src/Input/GamePackets.cpp`, `HandTurn.cpp`; test `test_game_packets`), on the one machine only |
| The hand's and camera's movements are sent to the other players, no more often than needed | partial 40% | The hand and camera sync packets with their countdown are sent to the local queue (`HandTurn.cpp`); no network |
| Other players' hands are drawn where their messages put them | pending | No other players' hands in our tree |
| Locking onto an object is started and ended by message, so all machines agree | partial 50% | The start and end locked select packets (`HandSystem::SendStartLockedSelect`, `SendEndLockedSelect`) on the one machine |
| A hand's gesture is sent as its recognised result, not as its mouse movements | pending | No network in our tree; see [../gesture/](../gesture/) |
| The hand's held miracle sends messages every turn while it needs them | partial 50% | `HandSystem::HandCastNeedsContinualPackets` keeps the sync packets every turn while a hand cast needs them (`HandTurn.cpp`); no network |
