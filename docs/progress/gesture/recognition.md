# Gesture recognition

The game records the path the hand's cursor takes across the screen and compares it with a file of gesture templates
to tell when the player has drawn a shape such as a circle, a spiral or a scribble. It only ever checks for the
gestures it is waiting for at that moment.

**Progress: 29/34 done, 3 partial — 90%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## The gesture templates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The templates are read once at start-up from `Data\Gestures.jty` | done | `src/Magic/Gestures/GestureTemplates.cpp` (`Templates`, `LoadTemplates`): the bytes loaded once through the resource cache (`GestureTemplatesLoader`) and read by the gestures component's `GestureFile` (`components/gestures`); tests `GestureTemplatesLoader.readsTheRecordsOfTheBytes`, `Gestures.realData`, `GestureFile.GivesTheRecogniserTheFormerParseOfTheGamesTemplates` |
| The file holds 81 templates for 23 gesture kinds, several templates per kind (ten hearts, eleven R shapes, nine threes …) so loose drawings still match | done | `GestureTemplates.cpp` through `GestureFile`; tests `Gestures.realData` (the 81 records of the real file; each player miracle gesture recognises its own stroke only), `GestureFile.ReadsTheGamesTemplates` |
| The square spiral and square wave templates added for Creature Isle are in the file and recognised | done | Loaded with the rest of the file and matched like any template (`src/Magic/Gestures/GestureMatch.cpp`); the square spiral is recognised in `Gestures.realData`. Nothing waits for its leash use yet (see [leash_gestures.md](leash_gestures.md)) |
| Each template stores its points normalised to a unit square, the turn at each point and the heading leaving it in eighths of a turn | done | `KeySample` (point, turn, direction) in `src/Magic/Gestures/GestureTemplates.h` |
| Each template says whether it may be drawn mirrored (only the circle and the star), whether the starting direction matters and whether the shape's width against its height matters | done | `checkDirection`, `allowReverse`, `checkAspect` in `GestureTemplates.h`, used by `src/Magic/Gestures/GestureMatch.cpp`; test `Gestures.syntheticTemplates` (the mirror only with allowReverse) |
| The template file can be written back, as the developers' in-game recording tool did | partial | `GestureFile::Write` (`components/gestures`) writes the file back byte for byte (tests `GestureFile.WritesAndReadsBackTheSameTemplates`, `GestureFile.ReadsTheGamesTemplates`), but nothing in the game calls it and there is no recording tool |

## Recording the hand's path

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cursor is sampled about 34 times a second (every 28 ms), at the game's pace rather than the frame rate | done | `src/Magic/Gestures/GestureInput.cpp` (`sampling::Update`: a sample per 28 ms of mouse-event time; approximate: a frame in which the mouse moved stands for a mouse event) |
| No mouse button needs to be held: the path is recorded whenever the cursor moves | done | `GestureInput.cpp`: a sample whenever the cursor moves, no button tested |
| The last 80 samples are kept, older ones fall off | done | `GestureSystem`, a ring of 80, in `src/Magic/Gestures/GestureBuffer.h` |
| Each sample also remembers the point of land under it, so a gesture can be placed in the world | done | `Sample::world`, filled by `gestures::FeedSample` in `src/Magic/Gestures/PowerUpSystem.cpp` |
| Over the sky or off the land, the last land point is kept with the new screen point | done | `GestureSystem::AddSampleAtLastWorld` (`GestureBuffer.cpp`), called by `FeedSample` when the cursor is off the land |
| Holding the cursor still for about two seconds (70 identical samples) forgets the path | done | `GestureSystem::k_StationaryLimit` (`GestureBuffer.cpp`); test `Gestures.stationaryWipe` |
| Corners are found as the path is drawn: the sample where the path turns most since the last corner, if it turns at least about 17 degrees | done | `GestureSystem::FindCorner` with `k_CornerTurn` (`GestureBuffer.cpp`); test `Gestures.cornersOfASquare` |
| Samples closer than 4 pixels on both axes count as the same place | done | `GestureSystem::Far` (`GestureBuffer.cpp`) |
| A corner too close to the one before (under 12 pixels, or 4 pixels when the last stretch is small) is merged into it, keeping the sharper turn | done | `GestureSystem::MergeOrReject` and `LongEnough` (12 px, or 4 px under a 50 px recent box) in `GestureBuffer.cpp` |
| Each corner records the turn from the way in to the way out, and the way out as one of eight directions | done | `GestureSystem::UpdateHeading`, `Octant`, `RoundHalfDown` (`GestureBuffer.cpp`); test `Gestures.octant` |
| Pixel distances are measured on the actual screen | done | Our tree measures in the window's own pixels (`input::GameCursor`), as the original does, with no rescaling |
| After a gesture is recognised the path is forgotten and nothing is recorded for 0.4 seconds | done | `gestures::Success` clears the buffer and starts the 0.4 s cooldown; `FeedSample` takes nothing meanwhile (`PowerUpSystem.cpp`) |
| Moving the camera forgets the path being drawn, except while the camera is shaking | partial | `ProcessPowerUpSystem` clears the buffer whenever the camera's position changed (`sampling::CameraMoving`); the position read includes the shake offset, so a shaking camera wipes it too (unconfirmed what the original does while shaking) |
| A camera that follows something, or the camera of a creature fight in an arena, lets the player keep drawing while it moves | todo | `ProcessPowerUpSystem` has no camera mode that lets the player gesture while moving (no follow or arena camera) |
| Nothing is recorded while the hand is not over the world: in the temple, in cut scenes, with the interface off | partial | Only the pause stops sampling (`FeedSample`); the original's early outs (inside the temple, inactive interface, cut scenes) are not ported (comment in `ProcessPowerUpSystem`) |
| Pressing the button during a creature fight forgets the path | todo | `ProcessPowerUpSystem` has `fighting = false`: no fight interface for the player |
| Starting a cast on release or a power-up starts the path again from the current point | done | `gestures::ReseedBuffer` from `HandSystem::BeginApplyOnRelease` (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`) and from `SetupPowerUpGestures` (`src/Magic/Core/SpellSeed.cpp`) |

## Matching a path with the templates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the gestures being waited for are checked, in a fixed order; the first one recognised is acted on and there is no best-match search | done | `gestures::ProcessPowerUpSystem` in `src/Magic/Gestures/PowerUpSystem.cpp`: circle, power-ups or selection, scribble, spirals, R, each returning on the first success |
| A gesture's templates are tried in the file's order and the first that fits is taken | done | `MatchGesture` in `src/Magic/Gestures/GestureMatch.cpp` |
| A template fits when the turns at the path's corners add up to the template's turns, never straying by more than about 34 degrees on the way | done | `MatchForward` and `MatchMirror` with the maximum error (3/16 of a half turn) in `GestureMatch.cpp`; test `Gestures.syntheticTemplates` |
| A small turn (under about 30 degrees) on either side may be skipped when that brings the two closer | done | `k_SmallTurn` (21/128 of a half turn) in `GestureMatch.cpp` |
| The match may start at any corner of the path, so a stroke drawn before the gesture does not spoil it | done | `MatchForward` tries every keypoint as the start (`GestureMatch.cpp`) |
| Templates that check direction need the path to set off the same way as the template | done | `DirectionMatches` in `GestureMatch.cpp` |
| A circle or a star drawn the other way round is recognised as its mirror image | done | `MatchMirror` for templates with `allowReverse`; test `Gestures.realData` (CIRCLE and STAR mirrored) |
| Templates that check their shape need the path to be about as wide against its height (very tall, very wide, or in between) | done | `AspectFits` in `GestureMatch.cpp` (the three aspect classes) |
| A gesture can be drawn anywhere on the screen and at any size | done | Keypoints are compared by their turns and directions only; tests `Gestures.syntheticTemplates`, `GesturesMenu.ADrawnTemplateMatchesItsGestureOnly` |
| A recognised gesture is placed on the land under the middle of the box round the samples it was drawn with | done | `PacketFromResult` in `GestureMatch.cpp` (the land under the centre of the matched samples' box) |
| A circle's size is the half width of its box, measured across the land at the depth of its middle, a little enlarged | done | `PacketFromResult`: 1.05 x the half width of the box at that distance; used as the circle's size by `ProcessPowerUpSystem` |
| Recognised gestures are sent to the other players in a network game with where they were drawn | n/a | No network play |
