# Temple hand

Inside the player's temple the hand has a state of its own: it hangs on the line of sight through the cursor just short
of the room's walls and floor, turns to face them, and plays its idle animation. The temple's rooms, tooltips and what
the hand does in them are in [../temple/](../temple/).

**Progress: 8/17 done, 4 partial — 59%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Placement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand goes into its temple state whenever the player is inside the temple | partial | `HandSystem::RequiredHandState` gives the CITADEL state inside the temple (`game_clock::IsInsideCitadel`), but the hand is still placed by the world's rules (`HandSystem::ResolveCursorPoint` and `Place`), with the room meshes picked as objects; the original's own temple state is not ported (rendering.md, Pending) |
| Entering, it starts from where it was and eases from there | partial | The world's hand distance zoomer carries on from where the hand was; no temple-specific easing |
| It hangs on the line of sight through the cursor, 3.25 units short of where the line meets the room | todo | No temple gap: the hand goes to the room mesh's hit point as over any object |
| It is never nearer than 1 unit before the limits are applied | todo | Only the world's clamp of the camera distance to 2 and the hand's reach |
| Its distance from the camera is kept between 4 and 300 | todo | Only the world's clamp (2 to the reach) |
| It eases away from the camera over 0.4 seconds and towards it over 0.2 | todo | The world's 0.1 s / 0.28 s zoomer is used |
| It is scaled by its distance from the camera as outside | done | `HandSystem::Place` scales the hand by its camera distance, inside as outside |

## Facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its up turns over 0.4 seconds to the face of the room the cursor is on | partial | The world's up zoomers (0.4 s) over the land normal or the picked face; not a temple rule of its own |
| It faces along the line of sight through the cursor | done | `HandSystem::HandMatrixRotation`: the heading from the camera to cursor ray, as outside |

## Animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays its idle wiggle all the time, looping by the frame's time | partial | The normal state's clip rule picks the clip (Cwiggle, or Cstroke over a room mesh picked as an object), not a constant wiggle |
| It leans with the trailing cursor, clamped to 80 pixels, unless the lean is tiny | done | The same lean layers as outside (`HandSystem::Update`, `src/3D/HandAnimator.cpp`) |
| Entering the temple blends the hand from its last pose | done | The change to the CITADEL state starts the 0.13 s state blend (`HandSystem::Update`, `HandCrossFade` in `src/3D/HandCrossFade.h`), with the real frame time inside |

## Around it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No camera edge hints are offered in the temple | done | The temple's own camera model (`src/Camera/TempleCameraModel.cpp`); see [../camera/temple_camera.md](../camera/temple_camera.md) |
| The hand's glow never lies on water in the temple | done | `Renderer::DrawPass` draws the temple instead of the world, so `DrawHandWaterGlow` never runs inside; test `HandWaterGlow.NeverShowsInsideTheTemple` (`test/graphics/test_hand_water_glow.cpp`) |
| The miracles don't hear the hand in the temple | todo | No temple test in `hand_casting::Update` or `gestures::ProcessPowerUpSystem` (`src/Magic`) |
| The temple places the hand's tooltips itself | done | `TempleInterior` puts the tooltip where the cursor meets the room (`SetHandOnScreen`), read by `Game::LogicFrame` |
| Clicking doors, scrolls, the creature cave and the map is handled by the temple | done | `TempleInterior::HoldControl`, `TempleCameraModel` (doors, scrolls, cave, map); see [../temple/](../temple/) |
