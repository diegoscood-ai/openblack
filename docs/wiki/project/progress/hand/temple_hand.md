# Temple hand

Inside the player's temple the hand has a state of its own: it hangs on the line of sight through the cursor just short
of the room's walls and floor, turns to face them, and plays its idle animation. The temple's rooms, tooltips and what
the hand does in them are in [../temple/](../temple/).

**Progress: 8/17 done, 0 chosen, 4 partial, 0 inferred — 55%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Placement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand goes into its temple state whenever the player is inside the temple | partial 50% | `HandSystem::RequiredHandState` gives the CITADEL state inside the temple (`game_clock::IsInsideCitadel`), first, as the original, and the state's own distance zoomer places and sizes the hand (`HandSystem::StepCitadelDistance`). Missing: its up and its constant wiggle; the room meshes are still picked as objects by `ResolveCursorPoint`, whose point the state no longer takes ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)) |
| Entering, it starts from where it was and eases from there | done | The state's Enter puts its distance zoomer at rest at the hand's last view distance (`PrepareState`), and it eases from there ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)); test `CitadelHandDistance.ItEasesAsTheOriginalDoesWithinFourAndThreeHundred` |
| It hangs on the line of sight through the cursor, 5 units short of where the line meets the room | done | `hand_detail::CitadelRoomDistance` from `TempleInteriorInterface::GetCursorHit` (kept from the last frame that met the room), then the point that far along the mouse ray; the original's constant is 5, not 3.25 (read 2026-10-24); tests `CitadelHandDistance.*` ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)) |
| It is never nearer than 1 unit before the limits are applied | done | `hand_detail::CitadelRoomDistance` ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)); test `CitadelHandDistance.TheAimIsTheRoomsDistanceLessFiveAndAtLeastOne` |
| Its distance from the camera is kept between 4 and 300 | done | `hand_detail::StepCitadelDistance` ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)); test `CitadelHandDistance.ItEasesAsTheOriginalDoesWithinFourAndThreeHundred` |
| It eases away from the camera over 0.4 seconds and towards it over 0.2 | done | `hand_detail::StepCitadelDistance`: 0.2 s when the new distance is nearer than the zoomer's destination, else 0.4 s, stepped in the original's order (`Zoomer::UpdateInlineStartSpeedFirst`), pinned to the exe ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)); tests `CitadelHandDistance.ItEasesAsTheOriginalDoesWithinFourAndThreeHundred`, `CitadelHandDistance.ItSumsItsValueAsTheOriginalDoes` |
| It is scaled by its distance from the camera as outside | done | `HandSystem::StepCitadelDistance`: the scale of the zoomer's distance, as the original's `SetDistanceFromView`; the hand is drawn along the mouse ray at the zoomer's own distance, not its clamp ([the temple state's distance](../../../hand/states.md#the-temple-states-distance)); tests `CitadelHandDistance.ItEasesAsTheOriginalDoesWithinFourAndThreeHundred`, `HandScale.TheScaleForTheViewDistanceGivesTheOriginalsBits` |

## Facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its up turns over 0.4 seconds to the face of the room the cursor is on | partial 40% | The world's up zoomers (0.4 s) over the land normal or the picked face. Missing: the temple state's own rule for the room's faces |
| It faces along the line of sight through the cursor | done | `HandSystem::HandMatrixRotation`: the heading from the camera to cursor ray, as outside; tests `HandOrientation.FacesAlongTheLevelledLineOfSight`, `HandOrientation.BuildsTheHandsMatrixFromTheHeading` |

## Animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays its idle wiggle all the time, looping by the frame's time | partial 30% | The normal state's clip rule picks the clip (Cwiggle, or Cstroke over a room mesh picked as an object). Missing: the temple's constant wiggle, looping on the frame's time |
| It leans with the trailing cursor, clamped to 80 pixels, unless the lean is tiny | done | The same lean layers as outside (`HandSystem::Update`, `src/3D/HandAnimator.cpp`); tests `HandAnimation.TheSidewaysLeanIsTheTrailingCursorsLagClampedToEightyPixels`, `HandAnimation.ALeanOfAtMostATenThousandthIsLeftOut` |
| Entering the temple blends the hand from its last pose | done | The change to the CITADEL state starts the 0.13 s state blend (`HandSystem::Update`, `HandCrossFade` in `src/3D/HandCrossFade.h`), with the real frame time inside; test `HandCrossFade.FadesAtAnEvenPaceOverATenthAndAThirdOfASecond` |

## Around it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No camera edge hints are offered in the temple | done | The temple's own camera model (`src/Camera/TempleCameraModel.cpp`); see [../camera/temple_camera.md](../camera/temple_camera.md); test `TempleCamera.OffersTheHandNoCameraHints` |
| The hand's glow never lies on water in the temple | done | `Renderer::DrawPass` draws the temple instead of the world, so `DrawHandWaterGlow` never runs inside; test `HandWaterGlow.NeverShowsInsideTheTemple` (`test/graphics/test_hand_water_glow.cpp`) |
| The miracles don't hear the hand in the temple | done | inside the temple every pass of the interface wipes the gesture path (`PowerUpSystem`'s `insideCitadel`, the Gestures window's "Path wiped every pass: inside the temple"); proven through the real input path (2026-10-31, `tools/checks/realinput/temple_gesture.txt`): a circle drawn with a shield's seed in the hand inside the temple's main room, the game standing at 1.9 s while the player is inside (`refs/realinput/temple_gesture.log` lines 132-134), gives no gesture and casts nothing, where the same circle on the land after leaving is recognised and casts the shield (lines 136-140: `Gestures: recognised 4`, `Hand seed: cast SHIELD`, the last seed type the shield's) |
| Inside the temple the hand's tooltip is drawn at the hand's own point, as on the land; the rooms choose it | done | The rooms choose the tooltip (`TempleInterior::UpdateToolTips`), and its icon is anchored at the hand's own position on the screen (`help::input_prompt::HandAnchor` from `FillOverlayFrame`, `src/Game.cpp`); test `InputPromptIcons.TheAnchorIsTheHandsOwnPoint`; see [the temple's hand and controls](../temple/temple_hand_and_controls.md) |
| Clicking doors, scrolls, the creature cave and the map is handled by the temple | done | `TempleInterior::HoldControl`, `TempleCameraModel` (doors, scrolls, cave, map); see [../temple/](../temple/); tests `TempleCamera.ClickingADoorOnScreenFindsIt`, `TempleCamera.HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms` |
