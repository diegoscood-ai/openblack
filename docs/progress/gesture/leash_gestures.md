# Leash gestures

The player puts the leash on their creature and picks which leash it wears by drawing gestures. What each leash does
to the creature is in [../creature/leash.md](../creature/leash.md).

**Progress: 0/15 done, 0 partial — 0%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

A scribble takes the leash off or closes the picker: see [scribble.md](scribble.md).

## Putting the leash on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The square spiral is the leash gesture, taken from the game's tables | todo | `leashSelectionStart` is read into `src/InfoConstants.h` but nothing uses it; `ProcessPowerUpSystem` has no leash step |
| The leash gesture is only waited for when the player has a creature that is not fighting and no picker is open | todo | Not waited for: the leash goes on with the L key or a click on the creature (`src/Creature/LeashKeys.h`) |
| It is also not waited for while the miracle selection is open | todo | No leash gesture |
| An unleashed creature needs to know at least one leash, a leashed one at least two | todo | No leash gesture; the keys only step through leashes the creature knows (`src/Creature/LeashKeys.h`) |
| On an unleashed creature the gesture puts the leash on | todo | No leash gesture; the L key puts the picked leash on (`LeashSystem::PressKey`) |
| If the creature knows two or more leashes, the leash picker opens as well | todo | No leash picker in our tree |
| On a creature already leashed, the gesture opens the leash picker | todo | No leash picker |
| Recognising the leash gesture shows its trail on the land with the recognition sound | todo | No leash gesture |

## The leash picker

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The picker waits for the gesture of each leash the creature knows, except the one it wears | todo | No leash picker; V and B change the leash instead (`src/Creature/LeashKeys.h`) |
| The leash of aggression is a vertical scribble, the leash of learning an E, the leash of compassion a heart | todo | `leashSelectionGestures` is read into `src/InfoConstants.h` but unused |
| Drawing a leash's gesture changes the creature's leash to it and closes the picker | todo | No leash picker; `LeashSystem::ChangeType` is reached only from the keys |
| The picker closes by itself after 25 seconds | todo | `leashSelectionSystemTimeOut` is read into `src/InfoConstants.h` but unused |
| The picker closes when the leash comes off the creature | todo | No leash picker |
| While the picker is open, the gestures of the leashes on offer are shown as icons | todo | No leash picker |
| Only the player who leashed the creature can change or remove the leash by gesture | n/a | Only one player owns a creature in openblack's single-player game |
| On the first land in a single-player game, drawing a square wave in the picker starts a hidden script | todo | No leash picker |
