# Creature gestures

Gestures that involve the creature beyond the leash: the player's gestures during a creature fight, and the gestures
the creature itself draws in the air when it casts a miracle. The creature's emotes (blowing a kiss, waving and so on)
are creature actions, in [../creature/](../creature/); the leash gestures are in [leash_gestures.md](leash_gestures.md).

**Progress: 0/7 done, 0 partial — 0%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## In a creature fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player's creature fights with the fight controls up, gestures choose its fight moves instead of the usual gestures | todo | `ProcessPowerUpSystem` has `fighting = false`; nothing ties gestures to the fight (`src/Creature/CreatureFight.h`) |
| Drawing a star makes the creature do its special move | todo | `creatureSpecialMoveGesture` is read into `src/InfoConstants.h` but unused; help event 19 is not ported |
| A creature that knows miracles can be made to cast them in a fight by gesture | todo | Nothing in our tree |

## The creature drawing gestures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before casting a miracle the creature draws that miracle's gesture in the air with its hand | todo | The creature's gesture layer (`src/Creature/CreatureLayers.h`) plays only nods, shakes, yawns and the like; there is no miracle gesture step |
| The creature's hand follows the shape of the gesture as it draws, along the gesture's path | todo | Nothing in our tree |
| A glowing chain trails behind the creature's hand while it draws the gesture | todo | `SF_CreatureGestureChain` is only listed in `src/Particles/ParticleTypes.cpp`; nothing starts it |
| A miracle cast on the creature shows a gesture effect that starts and dies away with the spell | todo | Nothing in our tree |

## Gestures with no use in the shipped game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's tables have gestures for zooming to the creature and for ending a gift to it, both left empty | n/a | Not used by the original game |
