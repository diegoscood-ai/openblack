# Creature gestures

Gestures that involve the creature beyond the leash: the player's gestures during a creature fight, and the gesture a
creature draws in the air before it casts a power-up miracle. The
creature's emotes (blowing a kiss, waving and so on) are creature actions, in [../creature/](../creature/); the leash
gestures are in [leash_gestures.md](leash_gestures.md).

**Progress: 1/5 done, 0 chosen, 0 partial, 0 inferred — 20%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

## In a creature fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player's creature fights with the fight controls up, both pickers close and the fight's gestures come before the usual ones; the move button wipes the path, and while it fights the spirals, R and the square spiral are not looked for | done | `FirstStage` with `CurrentFight` (`src/Magic/Gestures/PowerUpSystem.cpp`): the creature duelling (`IsDuelling`, the scripts' IS_FIGHTING) with the interface's fight state up (`ecs::audio_queries::LocalPlayerDirectsFight`) closes both pickers, or wipes the path while the move button is held; the fight flag keeps steps 5 to 7 off, as the original ([gestures](../../../magic/gestures.md#what-is-looked-for-and-when)); tests `LeashPicker.TheCreaturesFightWithItsControlsUpComesFirstAndClosesBoth`, `LeashPicker.TheFightIsTheLocalPlayersCreaturesDuel`; the gesture window shows the fight and its controls. The fight's own gestures are the two rows below |
| Drawing the special-move gesture (the star) makes the creature strike its special move at the opponent | pending | The fight's stage looks for `creatureSpecialMoveGesture` (8 STAR in the frozen info.dat) for every creature, with no knowledge test; recognised: the trail, help event 19, and the special move (packet 0x22, move 4) at the point where a line from the creature to its opponent meets the opponent's mesh ([gestures](../../../magic/gestures.md#what-is-looked-for-and-when)). Wired: `SpecialMoveStage` and the same line on the opponent's skin in the gesture pass's fight stage (`src/Magic/Gestures/PowerUpSystem.cpp`), help event 19, packet `FightMoveAdd` 0x22 with move 4 (`hand_fight::k_SpecialMove`) queued at the next turn by `creature_fight_packets::ApplyBodyPress`; test `SpecialMoveStage.TheSpecialMovesGestureIsLookedForFirst`; the gesture window shows the last one drawn. Not yet seen through the real input (testbed `combat.fight_miracle_gesture`) |
| A creature that knows a fight miracle (fire, lightning, heal, weak, strong) casts it at its opponent when the player draws its gesture in a fight | pending | Decoded in the frozen exe: for each seed with the fight flag (`unknown0x17C` in `src/InfoConstants.h`) whose magic the creature knows, the stage looks for the seed's gesture; recognised, a line from the creature to its opponent meeting the opponent's mesh sends packet 0x5A (the creature, the magic) with the trail, and a miss lets the pass go on ([gestures](../../../magic/gestures.md#what-is-looked-for-and-when)). Wired: `FightMiracleStage` and the line on the opponent's skin in the gesture pass's fight stage (`src/Magic/Gestures/PowerUpSystem.cpp`), packet `FightSpell` 0x5A queued at the next turn by `creature_fight_packets::ApplyFightSpell`; tests `FightMiracleStage.OnlyTheFightSeedsTheCreatureKnowsAreLookedForInOrder`, `HandFightPacketsTest.AFightMiraclesPacketQueuesTheMiracleForItsCreature`; the gesture window shows the last one drawn. Not yet seen through the real input (testbed `combat.fight_miracle_gesture`) |

## Before a cast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature casting a power-up miracle first draws the miracle's gesture in the air, pointing along its shape before its head, then holds the end of it before it casts | pending | Wired, not yet seen in play: the Gesture step's `CreatureCasting::Move::Gesture` in `src/ECS/Systems/Implementations/CreatureMindCasting.cpp` (`StepGesture`), its point and turns `creature_cast_gesture::PointOf` / `Turn` (`src/Creature/CreatureCastGesture.cpp`), pointing through `PointUntilStopped`, `UpdatePointPoint` and `StopPointing` ([the gesture](../../../creature/casting.md#the-gesture-88)); tests `CreatureCastGesture.*`; the creature readout shows it |
| A glowing chain follows the creature's hand as it draws the gesture | pending | `SF_CreatureGestureChain` (PT 31), started as the drawing starts and fed the pointing hand's place each frame by the creature's received-spell effect; not made: neither the per-frame step nor the hand's grasp point is ported ([the gesture](../../../creature/casting.md#the-gesture-88)) |
