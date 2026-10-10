# States overview

The god hand is always in exactly one state, picked afresh every frame from what the interface is doing and what the hand
holds. There are eleven: hidden, normal, camera, tugging, holding, totem, scooping, creature, holding a miracle, playing
a set animation, and the temple. Each state places, turns and animates the hand its own way; changing state blends the
hand from where it was drawn.

**Progress: 26/39 done, 8 partial — 77%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Choosing the state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The state is chosen again every frame, before the hand is placed and drawn | done | `HandSystem::RequiredHandState` (`HandSystem.cpp`) picks the hand's state each frame in `HandSystem::Update`, before `HandSystem::Place` |
| Inside the temple the hand is always in the temple state | done | `RequiredHandState` gives CITADEL first inside the temple; see [temple_hand.md](temple_hand.md) for how it is placed |
| Holding a miracle's seed puts it in the miracle state | done | `RequiredHandState`: GRAIN for a held spell seed; see [pouring.md](pouring.md) |
| Holding something being pulled out of the ground puts it in the tugging state | done | `RequiredHandState`: TUG while a tree is tugged (`_tug`); see [tug.md](tug.md) |
| Holding anything else puts it in the holding state | done | `RequiredHandState`: HOLDING for anything else held, and for a pile in its locked select |
| After the hand lets go of something, it waits about 180 milliseconds before going back to normal | done | `HandSystem::UpdateReleaseImpulse` (`HandTurn.cpp`): the 180 ms countdown, then the release impulse packet (the released object's spin) |
| Working a village's totem puts it in the totem state | partial | `RequiredHandState` gives TOTEM for the interface's totem state, but nothing reaches that state; see [totem.md](totem.md) |
| Interacting with a creature puts it in the creature state, if a creature is under the hand, else it stays normal | done | `RequiredHandState`: CREATURE for the interface's creature state with a creature locked for the hand, else NORMAL (`HandSystem::UpdateCreatureFrame`); tests `HandCreatureFrame.*` (`test/test_hand_creature.cpp`) |
| A set animation started by a tap (knocking on a house) puts it in the play-animation state until the animation ends | done | `HandSystem::StartFixedPosAnimation` (an abode's tap, `src/ECS/Abodes.cpp`) gives PLAY_ANIM |
| While the interface is switched off (cinematics, some script moments) the hand is hidden | done | `RequiredHandState` gives INVISIBLE for the interface's modes 1, 2, 12 and 25 (an inactive interface); `HandSystem::Update` marks the hand `NotDrawn` |
| Otherwise it is in the camera state while a drag of the camera is under way, and the normal state when not | partial | Gripping takes the camera state (`HandSystem::Place`); the CAMERA state of the camera mode's tricons is pending (`RequiredHandState`) |
| Scooping several things up puts it in the scooping state | done | `RequiredHandState`: a pile in its locked select is HOLDING. Our wiki differs: the original has no scooping state; a pile being scooped is the HOLDING state ([page](../../bw1-notes/hand-and-interface.md#the-hands-state-the-spring-the-held-object-power-ups-and-roots)) |

## Changing state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On a change, the old state is left and the new one entered, each resetting what it eases | partial | The state change starts the blend; the zoomers are reset per case (the normal state's up zoomers on entering it, the grain state on entering it), not by one Enter and Exit per state |
| Every change of state blends the hand's bones and place from where they were drawn over 0.13 seconds | done | `HandCrossFade` (`src/3D/HandCrossFade.h`, the hand system's `_stateBlend`) blends the bones from `_stateBlendFromWorld` and the place from where it was drawn (`HandSystem::Update`, `HandPlacement.cpp`), 0.13 s; tests `HandCrossFade.*` (`test/hand/test_hand_cross_fade.cpp`) |
| A change of animation inside a state blends the same way | done | No blend for a change of clip inside a state, as the original. Our wiki differs: a clip change inside a state is not blended ([page](../../bw1-notes/hand-and-interface.md#the-hands-clip-handstatenormal)) |
| Entering the normal state starts the hand's up straight and its distance where it was | done | `HandSystem::Place` snaps the up zoomers to (0, 1, 0) on entering the normal state; the hand distance zoomer carries on |
| Entering the hidden state keeps the last drawn bones, so the hand blends back in from them | done | While hidden nothing updates the pose (`HandSystem::Place` returns), so the blend back starts from the last drawn bones |
| Leaving the creature state tells the creature how it was treated, held between -1 and 1 | done | Leaving the CREATURE state sends the feedback packet with the clamped value (`ecs/CreatureHandPackets.h`); test `HandCreatureFrame.ALongHoldSendsOnlyTheFeedback` |
| Entering the creature interaction pushes a close camera on the creature, and leaving it pops the camera back | todo | The interaction camera is not ported (hand-and-interface.md, Pending) |
| A creature interaction ended within 450 milliseconds counts as a click on the player's own creature | done | `creature_hand::k_ClickMaxCameraMs` (450, `src/Creature/CreatureHandRules.h`), timed in the hand's camera ms; test `HandCreatureFrame.ALetGoWithinAClickOfTheOwnCreatureSendsTheClickThenTheFeedback` |

## What every state shares

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spring-damped copy of the cursor trails it by at most 80 pixels on each axis, with a stiffness of 20 | done | `HandSystem::Update`: the smoothed mouse clamped to 80 pixels, the velocity's 20 and 0.03 per second |
| The trailing gap leans the hand sideways and back and forth through its lean animations, unless smaller than a ten-thousandth | done | `HandSystem::Update` (the lean layers), skipped below 0.0001 |
| The hand's alignment is held between -1 and 1 and its morph updated every frame | done | `HandSystem::UpdateMorphing` (`HandMorph.cpp`) over `hand_morph::Advance` (`src/3D/HandMorph.cpp`): the local player's alignment clamped to -1..1, re-morphed when it moves 0.03; see [look_and_morph.md](look_and_morph.md) |
| The hand's time runs on the camera's clock, or the frame's while in the temple | done | `game_clock::CameraFrameMs` (the game time while a hand demo plays), `game_clock::FrameRealMs` inside the temple |
| The hand points along the line of sight through the cursor, turned only when the cursor has moved | done | `HandSystem::HandMatrixRotation`; the up target taken only when the mouse's x changed (`HandSystem::UpdateNormalUp`) |
| Whether the camera's edge hints show depends on the state: the normal, camera and holding states allow them | todo | The camera's tricons are not ported |
| The hand can be swapped between left and right at any time | todo | Our hand is drawn unmirrored; no left-handed mode |
| The hand's state is saved and loaded with the game | todo | No save of the hand in our tree |

## Each state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hidden: nothing is drawn and nothing moves | done | INVISIBLE: `NotDrawn` on the hand and no pose update |
| Normal: hovering under the cursor, feeling objects, offering camera hints at the edges | partial | Hover and the felt objects' up are done (`HandPlacement.cpp`); the edge hints are not ported |
| Camera: gripping or dragging the land or the edges | partial | The land grip only; see [navigation.md](navigation.md) |
| Tugging: pulling at something rooted until it comes free | partial | See [tug.md](tug.md) |
| Holding: carrying objects and people | done | See [holding.md](holding.md) |
| Totem: holding a village's totem and sliding it | todo | See [totem.md](totem.md) |
| Scooping: taking up several things at once | done | A pile being scooped is HOLDING; see [multi_pickup.md](multi_pickup.md) |
| Creature: held to a creature's body | partial | See [creature_contact.md](creature_contact.md) |
| Holding a miracle, and pouring | done | See [pouring.md](pouring.md) |
| Playing a set animation at a fixed place | done | `HandSystem::StartFixedPosAnimation` (PLAY_ANIM, the hand pinned at the point); see [hand_animations.md](hand_animations.md) |
| Temple | partial | See [temple_hand.md](temple_hand.md) |
