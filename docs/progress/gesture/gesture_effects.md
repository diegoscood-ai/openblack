# Gesture trails and effects

What the player sees and hears when drawing gestures: the glowing chain behind the hand while it draws, and the trail
of sparkles and light a recognised gesture leaves on the land in the player's colour. Also gestures drawn by scripts and
the tutorial.

**Progress: 19/24 done, 2 partial — 83%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## A recognised gesture's trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every recognised gesture except the scribble leaves a trail on the land | done | `gestures::Success(true)` calls `psys::utility::GestureRecognised` (`src/Particles/Utility.cpp`); a scribble calls `Success(false)` |
| The trail needs at least two of the drawn points to lie on the land | done | `utility::GestureRecognised` needs at least two samples (`src/Particles/Utility.cpp`) |
| The trail is built from the points of the drawn path and from the gesture's ideal shape, fitted to where it was drawn on the screen and laid on the land under it | done | `utility::GestureRecognised`: the stroke's land points and the ideal shape fitted to the matched box and resampled (`Utility.cpp`) |
| Each gesture's ideal shape comes from its own path file in the game's data, the circle's for a gesture without one | done | `src/Magic/Gestures/GestureShapes.cpp` (`PathSymbol<n>.cam` through the resource cache, else the circle's); tests `GestureShapeLoader.*` |
| Seen from a low camera, the shape is squashed up the screen until it is no more than twice as deep as wide | done | `utility::GestureRecognised`: x 0.75, at most 15 times (`Utility.cpp`) |
| Where the screen shows no land under the shape, its point is put a fixed distance from the camera | done | 400 m along the ray (`Utility.cpp`) |
| A recognition sound plays as the trail appears, without position for the player's own gestures | done | `src/Particles/Rules/Gesture.cpp` (the recognition sample in 2D for this computer's interface) |
| Another player's gesture is heard from where their hand was | n/a | No network play |
| Sparkles in the player's colour flow from the drawn path to the gesture's shape over about half a second | done | The `UR_GesturingRecognised` rule in `src/Particles/Rules/Gesture.cpp` (in the player's colour, over TimeToIdeal) |
| The sparkles are sized by the length of the shape | done | `Gesture.cpp`: the sprites' scale is the ideal's length / 100 |
| The shape is lifted towards a camera looking down on it | done | `Gesture.cpp`: the ideal comes towards the camera (at most half way) |
| The trail is revealed from both ends | done | `Gesture.cpp`: alpha from the two ends |
| The sparkles wiggle, each in its own way, then disperse and shrink away | done | `Gesture.cpp`: value noise at shuffled phases (`src/Particles/Noise.cpp`), dispersal after DispersalTime |
| After a couple of seconds the sparkles flash and the hand glows in the player's colour, both dying away | partial | The collection's alpha pulse is in `Gesture.cpp`; the hand's colour pulse is not ported |
| A sheet of starry light rises along the shape and fades over its life | partial | The light sheet's data are kept in `Gesture.cpp`, but it is not drawn |
| The sheet of light is drawn additively, from both sides, its stars sliding along it | todo | The light sheet is not drawn |
| Gestures drawn in quick succession each leave their own trail | done | Records queue in `utility::PendingRecognised` (`Utility.cpp`), one taken per step by the rule |

## The chain behind the drawing hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chain follows the hand while it holds a miracle it can power up, or one a circle sizes | done | `TrailWanted` in `src/Particles/Utility.cpp` (a held seed with the trail shown, or a circle-sized seed); `ZR_ChainGesture` in `src/Particles/Rules/Gesture.cpp`, drawn by `src/Graphics/RendererChain.cpp` |
| The chain also follows the hand while the miracle selection is open | done | `TrailWanted`: the selection open with the hand ready (`Utility.cpp`) |
| Its links are laid each time the hand has moved far enough, older links moving down the chain | done | `ZR_ChainGesture` (`Gesture.cpp`): a joint every MinEmitDist, passed down the chain |
| The chain is sized by the hand and by how far the hand is from the camera | done | `TrailScale` in `Utility.cpp` (the hand's scale x the camera-distance table) |
| When the hand stops gesturing the chain is left to fade and goes a few seconds later | done | `ZR_ChainGesture`: emission stops and the head goes 5 s later |
| Turning the chain on or off starts or stops a force-feedback effect | n/a | Force-feedback mice are not supported |

## Gestures drawn by the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can draw a gesture's trail on the land as if the player had drawn it | todo | `PLAY_GESTURE` is a stub in `src/CHLApi.cpp` |
| Tapping some objects draws a gesture's trail over them | todo | Nothing in our tree (unconfirmed which objects) |
| The tutorial plays recorded hand demos in which the hand draws gestures for the player to copy | done | `src/Input/HandDemo.cpp` (`PLAY_HAND_DEMO`, `IS_PLAYING_HAND_DEMO` in `src/CHLApi.cpp`) replays the recorded mouse through the real hand, so its gestures are sampled and recognised like the player's |
