# Other players' hands

Every god in a game has a hand of its own: the rival gods of the story and of skirmishes move theirs about the land to
pick things up, throw, cast and work their creatures, and in network games each player sees the others' hands. The rules
of network play are in [../multiplayer/](../multiplayer/).

**Progress: 0/28 done, 12 partial — 21%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Each player's hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player has a hand of their own, with what it holds, where it is, its speed, and the camera it looks from | partial | One `HandSystem` for the local player (its hand entities, held object, spring velocity, synced hand and camera in the turn state, `HandTurn.cpp`); other players have no hand |
| A hand holds one thing (or one handful) at a time; a full hand takes nothing more | partial | Done for the local hand (`HandSystem::ApplyPlaceInHand` refuses a full hand); no other hands |
| Each player's hand remembers the last thing it picked up and the last it dropped, forgetting them when they are destroyed | partial | The local hand's `_lastReleased` is cleared when it is destroyed (`HandTurn.cpp`); no other hands |
| Things a hand holds that are destroyed leave it at the next turn | partial | `HandSystem::ValidateHands` (`HandTurn.cpp`) drops a held object that is no longer interactable at the turn; local hand only |
| Each hand has its own leash | partial | `LeashSystem` keeps each leash's player; only the local hand works one |
| A hand's interaction locked onto something carries on each turn only while that thing says so and the hand stays inside its player's influence | partial | The locked select lasts while the object is available, has influence at the hand's point and its interact step says so (`HandTurn.cpp`); local hand only |
| A hand's place is reported as a point on the map, which miracles and scripts use | partial | `HandSystem::GetPlayerHandPositions` and the synced hand of the turn state (`HandTurn.cpp`); local hand only |
| Only the local player's hand plays the guidance heartbeat and changes the local interface | partial | The guidance heartbeat plays for the local player (`src/Audio/Services/Guidance.cpp`); there are no other hands to tell apart |

## The rival gods' hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god's hand is drawn moving over the land | todo | No computer god's hand in our tree |
| It moves to a place, to an object or to its creature's head, and can hold over an object | todo | Not in our tree |
| It moves inside its own influence, keeping its height over the land as it goes | todo | Not in our tree |
| It picks up villagers, rocks, trees, food and wood, and drops them on a town, a worship site, a building site or a store | todo | Not in our tree |
| It throws things at a place, at another god's town or creature | todo | Not in our tree |
| It gives things to its creature | todo | Not in our tree |
| It strokes and slaps its creature, and puts its leash on things | todo | Not in our tree; see [creature_contact.md](creature_contact.md) |
| It raises and lowers village totems | todo | Not in our tree; see [totem.md](totem.md) |
| It draws gestures and casts miracles from icons or gestures | todo | Not in our tree; see [../miracles/](../miracles/) |
| It combines scaffolds and makes disciples | todo | Not in our tree |
| It takes food and wood out of stores to use | todo | Not in our tree |
| How long each of these takes is estimated before it is started | todo | Not in our tree |
| Another god's fireball can be caught by the player's hand | todo | No magic fireball in our tree |
| Another god's creature can be held and stroked or slapped by the player's hand | todo | Our hand holds only the player's own creature (`creature_hand::IsFriendlyTo`); our tree has no alliances. Our wiki differs: the original holds only its own or an allied player's creature, not any god's ([page](../../bw1-notes/hand-and-interface.md#the-hand-on-a-creature)) |

## Network play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What a hand does is sent to the other players as messages and applied at the start of the next turn on every machine | partial | The hand's actions go through the packet queue and are applied at the next turn's start (`src/Input/GamePackets.cpp`, `HandTurn.cpp`; test `test_game_packets`), on the one machine only |
| The hand's and camera's movements are sent to the other players, no more often than needed | partial | The hand and camera sync packets with their countdown are sent to the local queue (`HandTurn.cpp`); no network |
| Other players' hands are drawn where their messages put them | todo | No other players' hands in our tree |
| Locking onto an object is started and ended by message, so all machines agree | partial | The start and end locked select packets (`HandSystem::SendStartLockedSelect`, `SendEndLockedSelect`) on the one machine |
| A hand's gesture is sent as its recognised result, not as its mouse movements | todo | No network in our tree; see [../gesture/](../gesture/) |
| The hand's held miracle sends messages every turn while it needs them | partial | `HandSystem::HandCastNeedsContinualPackets` keeps the sync packets every turn while a hand cast needs them (`HandTurn.cpp`); no network |
