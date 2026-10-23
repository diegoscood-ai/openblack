# Gesture trails and effects

What the player sees and hears when drawing gestures: the glowing chain behind the hand while it draws, and the trail
of sparkles and light a recognised gesture leaves on the land in the player's colour. Also gestures drawn by scripts and
the tutorial.

**Progress: 23/25 done, 0 chosen, 0 partial, 0 inferred — 92%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

## A recognised gesture's trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every recognised gesture except the scribble leaves a trail on the land | done | `gestures::Success(true)` calls `psys::utility::GestureRecognised` (`src/Particles/Utility.cpp`); a scribble calls `Success(false)` |
| The trail needs at least two of the drawn points to lie on the land | done | `psys::utility::RecognisedTrail` (`src/Particles/Utility.cpp`): two samples, a gesture, and two of the buffer's points on the land, as the original ([hand casting](../../../magic/hand-casting.md#utility-effects)); test `GestureTrail.TheTrailNeedsTwoSamplesAGestureAndTwoLandPoints` |
| The trail is built from the points of the drawn path and from the gesture's ideal shape, fitted to where it was drawn on the screen and laid on the land under it | done | `utility::GestureRecognised`: the stroke's land points and the ideal shape fitted to the matched box and resampled (`Utility.cpp`) |
| Each gesture's ideal shape comes from its own path file in the game's data, the circle's for a gesture without one | done | `src/Magic/Gestures/GestureShapes.cpp` (`PathSymbol<n>.cam` through the resource cache, else the circle's); tests `GestureShapeLoader.*` |
| Seen from a low camera, the shape is squashed up the screen until it is no more than twice as deep as wide | done | `psys::utility::RecognisedTrail`: x 0.75 a time, at most 15 layings, while the depth across the camera's forward is more than twice the width, as the original ([hand casting](../../../magic/hand-casting.md#utility-effects)); test `GestureTrail.TheShapeIsSquashedUpTheScreenUntilNoDeeperThanTwiceItsWidth` (1, 7 and 15 layings) |
| Where the screen shows no land under the shape, its point is put a fixed distance from the camera | done | `psys::utility::k_ShapeOffTheLand` (400) to `sampling::LandOrPointAlongPixel`, as the original's 400 along the line ([hand casting](../../../magic/hand-casting.md#utility-effects)); tests `GestureTrail.OffTheLandTheShapesPointsGoFourHundredAlongTheirLine`, `GestureSampling.OffTheLandThePointIsTheLengthAlongTheLine` |
| A recognition sound plays as the trail appears, without position for the player's own gestures | done | `src/Particles/Rules/Gesture.cpp` (the recognition sample in 2D for this computer's interface) |
| Sparkles in the player's colour flow from the drawn path to the gesture's shape over about half a second | done | The `UR_GesturingRecognised` rule in `src/Particles/Rules/Gesture.cpp` (in the player's colour, over TimeToIdeal) |
| The sparkles are sized by the length of the shape | done | `Gesture.cpp`: the sprites' scale is the ideal's length / 100 |
| The shape is lifted towards a camera looking down on it | done | `Gesture.cpp`: the ideal comes towards the camera (at most half way) |
| The trail is revealed from both ends | done | `Gesture.cpp`: alpha from the two ends |
| The sparkles wiggle, each in its own way, then disperse and shrink away | done | `Gesture.cpp`: value noise at shuffled phases (`src/Particles/Noise.cpp`), dispersal after DispersalTime |
| After a couple of seconds the sparkles flash and the hand glows in the player's colour, both dying away | done | The collection's alpha pulse is in `Gesture.cpp` (seen in the scenario `miracles.gesture_circle_shield`). The hand's colour pulse is written in the original but never drawn: the hand's draw rewrites its colour and specular from the land every frame before it is drawn, and the pulse is written only in the effect's turn step ([the hand's pulse](../../../magic/hand-casting.md#the-hands-pulse)), so the hand looks the same with none |
| A sheet of starry light rises along the shape and fades over its life | done | `Gesture.cpp` starts the `particles::LightSheet` (`src/Particles/LightSheet.cpp`), the particle service moves it on every frame |
| The sheet of light is drawn additively, from both sides, its stars sliding along it | done | `particles::draw::AddLightSheets` (`src/Particles/ParticleDrawFrame.cpp`), one Z object per sheet, drawn in one pass by `Renderer::DrawParticleSurface` (`src/Graphics/RendererRevolvedSurface.cpp`) |
| Gestures drawn in quick succession each leave their own trail | done | Records queue in `utility::PendingRecognised` (`Utility.cpp`), and the rule takes the newest one per step, as the original's list ([hand casting](../../../magic/hand-casting.md#utility-effects)); test `GestureEffects.GesturesInQuickSuccessionEachLeaveTheirOwnTrailTheNewestFirst` |

## The chain behind the drawing hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chain follows the hand while it holds a miracle it can power up, or one a circle sizes | done | `TrailWanted` in `src/Particles/Utility.cpp` (a held seed with the trail shown, or a circle-sized seed); `ZR_ChainGesture` in `src/Particles/Rules/Gesture.cpp`, drawn by `src/Graphics/RendererChain.cpp` |
| The chain also follows the hand while the miracle selection is open | done | `TrailWanted`: the selection open with the hand ready (`Utility.cpp`) |
| Its links are laid each time the hand has moved far enough, older links moving down the chain | done | `ZR_ChainGesture` (`Gesture.cpp`): a joint every MinEmitDist, passed down the chain |
| The chain is sized by the hand and by how far the hand is from the camera | done | `TrailScale` in `Utility.cpp` (the hand's scale x the camera-distance table) |
| When the hand stops gesturing the chain is left to fade and goes a few seconds later | done | `ZR_ChainGesture`: emission stops and the head goes 5 s later |
| Turning the chain on or off starts or stops a force-feedback effect | pending | To port as the original's (user, 2026-10-19) with the force-feedback effects, which openblack lacks (the immersion effects and `Data/Immersion/*.ifr`, [../engine/asset_formats.md](../engine/asset_formats.md); `START_IMMERSION` and the rest are stubs in `src/CHLApi.cpp`) |

## Gestures drawn by the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a reward chest that teaches a power-up shows that power-up's gesture on the land | done | `psys::utility::ShowGestureOnLand` from the reward opening (`src/ECS/RewardOpening.cpp`) over `magic::gestures::LandGestureTrail`, as the original's faked gesture (30 m from the chest's point, for the tapping interface): the height asked at the map point each laid point falls in and every step in float, as the frozen exe at 24 bits ([hand casting](../../../magic/hand-casting.md#utility-effects)); tests `RewardOpening.AGestureShownOnTheLand*` (`test/test_reward_opening.cpp`); testbed `story.reward_chest_opens` |
| Another god's hand leaves its gesture's sparkles on the land where it made the gesture | pending | The end of the computer hand's MakeGesture step lays the gesture on the land at the step's point, 30 m across, for the god's leader interface, as `psys::utility::ShowGestureOnLand` does ([computer hands](../../../hand/computer-hands.md), [hand casting](../../../magic/hand-casting.md#utility-effects)); openblack's computer hand has the step's number but no action queues or runs it (`src/ECS/ComputerHand.cpp`): it waits for the computer gods' casting actions (Land 2's LearnGestures) |
| The tutorial plays recorded hand demos in which the hand draws gestures for the player to copy | done | `src/Input/HandDemo.cpp` (`PLAY_HAND_DEMO`, `IS_PLAYING_HAND_DEMO` in `src/CHLApi.cpp`) replays the recorded mouse through the real hand, so its gestures are sampled and recognised like the player's |
