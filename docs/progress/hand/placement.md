# Placement

Where the god hand sits in the world, how large it is drawn and which way it faces. The game keeps the hand under the
cursor at all times, hanging over the land, the sea, the object or the temple wall the cursor points at.

**Progress: 16/27 done, 0 chosen, 10 partial, 0 inferred — 79%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Where the hand sits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's fingertips lie on the line of sight through the cursor, so it is always under the cursor on screen | done | `HandSystem::ResolveCursorPoint` (`HandPlacement.cpp`): the hand's model origin goes on the mouse ray, at the smoothed distance |
| It is pulled back from the land towards the camera by its own height, so its fingers hang down to the land | done | `ResolveCursorPoint`: the empty hand stops 3.2 x its scale short of the land |
| Over the sea it rests on the water | done | `Game.cpp` meets the sea plane when the land is missed, and `ResolveCursorPoint` does not pull the empty hand back over the sea (land below 0.1) |
| It eases out to land further from the camera slowly and in to nearer land quickly | partial 85% | `ResolveCursorPoint`: the hand distance zoomer, 0.28 s moving away and 0.1 s coming closer, at least 1 (test `test_hand_land_point`). Missing: the original's 2.0 s while a creature give is pending ([animation-and-placement.md](../../../hand/animation-and-placement.md#where-the-hand-is-placed)) |
| It is kept between 2 units and the hand's reach from the camera | done | `ResolveCursorPoint` clamps to [2, the reach]; the reach from SET_INTERFACE_INTERACTION (`HandSystemInterface::SetHandReach`) |
| With nothing under the cursor (the sky) it keeps its distance and stands upright | partial 80% | `ResolveCursorPoint`: with nothing under the hand it aims along the mouse ray at its view distance (`hand_land_point::AlongMouseRay`); the hand's land point keeps the hand's place, unshortened (test `HandLandPoint.NothingUnderTheCursorGivesTheHandsPositionUnshortened`). Missing: the upright stance over the sky is not checked against the original |
| Over an object it hangs where the line of sight meets the object's own mesh, standing on that face | done | `HandSystem::PickObjectAlongRay`: the exact triangle test of the drawn mesh; villagers and animals by their 2D radius; the up is the felt face's (`_feelUp`, `HandSystem::UpdateNormalUp`) |
| Holding something over an object, it is pulled back further by half the size of what it holds, and more near a creature | partial 60% | `ResolveCursorPoint` pulls the held hand back by half the held object's 2D radius over an object. Missing: the original subtracts the hand's size too while a creature is offered the held thing (not ported) |
| Over a worship icon it hangs at the icon, a set distance short of it | done | `ResolveCursorPoint`: a spell icon's point and altitude, the depth target 3.2 x the hand's size short of it |
| Gripping the land it stays on the gripped point and moves with it | done | `HandSystem::Place` (the camera state): the hand stays on the gripped land point |
| Dragging by the edge it holds its distance from the camera over 0.4 seconds, never past the land | partial 40% | The camera state's ease is ported for the land grip (`HandSystem::Place`: the distance eases over 0.4 s, never past the land under the cursor). Missing: the original's CAMERA hand state, which the edge drag of the new camera mode enters, is not ported ([animation-and-placement.md](../../../hand/animation-and-placement.md#pending)) |
| It moves by the frame's real time, or by the game's time while a hand demo plays back | done | The hand's dt is the camera time step: the frame's real time, or the game time while a hand demo plays (`game_clock::CameraFrameMs`), the real time inside the temple ([page](../../../hand/grab-hold-and-throw.md#holding-spring-and-throwing)) |
| Its speed and direction of movement are measured each frame for throws and spins | done | The holding spring and the grain state's measure (`HandPlacement.cpp`, `HandHolding.cpp`) |
| Held to a creature it rests on the creature's body under the cursor | partial 50% | The CREATURE state puts the hand where the creature hand puts it (`HandSystem::Place`, approximate: `_creaturePose`, capsules round the posed bones for the drawn triangles); see [creature_contact.md](creature_contact.md) |
| In the temple it hangs a little short of the room's surface and turns to face it | partial 40% | The world's placement is used inside the temple, with the room meshes picked as objects. Missing: the citadel state's own placement and facing; see [temple_hand.md](temple_hand.md) |
| The hand can be held at a set place: a fixed-position animation, or the creature fight's sparkles | partial 50% | The fixed-position animation (`HandSystem::StartFixedPosAnimation`, used by an abode's tap) pins the hand. Missing: the fixed hand position the creature fight's sparkles set ([animation-and-placement.md](../../../hand/animation-and-placement.md#where-the-hand-is-placed)). No script command holds the hand |

## Size and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is scaled so it spans 3.2 units, growing past 150 units from the camera so it keeps about the same size on screen | done | `HandSystem::Place`: the scale 3.2 x handScale / 555.294, with the original's distance curve below 10 and past 150 (`ClampedViewDistance`, then the runtime's power and the fade in the original's order, `src/ECS/HandLandPoint.cpp`); test `test_hand_scale` |
| It faces along the line of sight through the cursor, laid level | done | `HandSystem::HandMatrixRotation` (`HandFrame.cpp`) with `hand_orientation::HeadingAlongRay` (`src/3D/HandOrientation.cpp`): the heading from the camera to cursor ray, laid level; test `HandOrientation.FacesAlongTheLevelledLineOfSight` |
| Looking straight down it keeps its heading | done | The heading is only taken from the ray when it is not vertical (`hand_orientation::HeadingAlongRay`); test `HandOrientation.KeepsItsHeadingLookingStraightDown` |
| Its up eases over 0.4 seconds to the slope of the land, or the face of the object, under it | done | `HandSystem::UpdateNormalUp` (`HandFrame.cpp`): `hand_orientation::k_UpEaseSeconds` (0.4 s) to the land normal or the felt face; no unit test of the ease |
| The slope is only taken afresh when the cursor moves across the screen | done | `UpdateNormalUp` takes a new target only when the mouse's x changed |
| Dragging the land it holds its up, and stands straight up when let go | done | The camera state uses the last normal up, frozen; entering the normal state snaps the up to (0, 1, 0) (`HandSystem::Place`) |
| One hand is the other's mesh mirrored | partial 50% | `hand_detail::HandModelScale` mirrors the hand's x axis with the debug menu's Right Handed option (raffclar's; test `HandHandedness.RightHandMirrorsOnlyTheXAxis`). Missing: the original is right-handed by default (its mirror flag 0) and mirrors the left hand, our default is the unmirrored mesh; which mesh is which is not settled, and the held object's mirrored matrix is not ported (issue #28) |
| The player picks a left or right hand in the options | partial 30% | The choice exists as the debug menu's Right Handed option and the run profile's left-handed flag (`player_profiles::RunProfile`). Missing: the original's options entry that toggles it |
| Holding a miracle or pouring food and wood lifts and tips the hand | done | `hand_grain` and the holding branch of `HandSystem::Place`; see [pouring.md](pouring.md) |

## Showing and hiding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is hidden while the menu is open and while a script's cinematic has the interface | partial 50% | The INVISIBLE state hides the hand (`NotDrawn`) for the interface's inactive state, as when a script holds the bars (`HandSystem::Update`). Missing: the original hides it for four interface modes; which of ours they are, and whether our menu hides it, is not checked ([animation-and-placement.md](../../../hand/animation-and-placement.md#hidden-hand-and-chands-states)) |
| Where the hand can't be drawn, a plain mouse pointer is drawn instead | pending | Not in our tree: the original's hidden hand switches the system cursor on ([animation-and-placement.md](../../../hand/animation-and-placement.md#hidden-hand-and-chands-states)) |
