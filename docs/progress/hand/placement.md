# Placement

Where the god hand sits in the world, how large it is drawn and which way it faces. The game keeps the hand under the
cursor at all times, hanging over the land, the sea, the object or the temple wall the cursor points at.

**Progress: 17/29 done, 6 partial — 69%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Where the hand sits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's fingertips lie on the line of sight through the cursor, so it is always under the cursor on screen | done | `HandSystem::ResolveCursorPoint` (`HandPlacement.cpp`): the hand's model origin goes on the mouse ray, at the smoothed distance |
| It is pulled back from the land towards the camera by its own height, so its fingers hang down to the land | done | `ResolveCursorPoint`: the empty hand stops 3.2 x its scale short of the land |
| Over the sea it rests on the water | done | `Game.cpp` meets the sea plane when the land is missed, and `ResolveCursorPoint` does not pull the empty hand back over the sea (land below 0.1) |
| It eases out to land further from the camera slowly and in to nearer land quickly | done | `ResolveCursorPoint`: the hand distance zoomer, 0.28 s moving away and 0.1 s coming closer, at least 1 |
| It is kept between 2 units and the hand's reach from the camera | done | `ResolveCursorPoint` clamps to [2, the reach]; the reach from SET_INTERFACE_INTERACTION (`HandSystemInterface::SetHandReach`) |
| With nothing under the cursor (the sky) it keeps its distance and stands upright | partial | With no land nor object hit the hand is not moved (`HandSystem::Place` returns); the original's distance from the hand's last place and the upright stance are not ported |
| Over an object it hangs where the line of sight meets the object's own mesh, standing on that face | done | `HandSystem::PickObjectAlongRay`: the exact triangle test of the drawn mesh; villagers and animals by their 2D radius; the up is the felt face's (`_feelUp`, `HandSystem::UpdateNormalUp`) |
| Holding something over an object, it is pulled back further by half the size of what it holds, and more near a creature | partial | `ResolveCursorPoint` pulls the held hand back by half the held object's 2D radius over an object; the creature's push is not ported |
| Over a worship icon it hangs at the icon, a set distance short of it | done | `ResolveCursorPoint`: a spell icon's point and altitude, the depth target 3.2 x the hand's size short of it |
| Gripping the land it stays on the gripped point and moves with it | done | `HandSystem::Place` (the camera state): the hand stays on the gripped land point |
| Dragging by the edge it holds its distance from the camera over 0.4 seconds, never past the land | todo | No edge drag in the wired camera (raffclar's `camera_drag` is in our tree, not wired); see [navigation.md](navigation.md) |
| It moves by the frame's real time, or by the game's time while a script holds the cinema bars | done | The hand's dt is the camera time step: the frame's real time, or the game time while a hand demo plays (`game_clock::CameraFrameMs`), the real time inside the temple. Our wiki differs: the game's time is used while a hand demo plays back, not while a script holds the bars ([page](../../bw1-notes/hand-and-interface.md#holding-spring-and-throwing-handstateholdingupdate-0x5b3c70)) |
| Its speed and direction of movement are measured each frame for throws and spins | done | The holding spring and the grain state's measure (`HandPlacement.cpp`, `HandHolding.cpp`) |
| Held to a creature it rests on the creature's body under the cursor | partial | The CREATURE state puts the hand where the creature hand puts it (`HandSystem::Place`, approximate: `_creaturePose`); see [creature_contact.md](creature_contact.md) |
| In the temple it hangs a little short of the room's surface and turns to face it | partial | The world's placement is used inside the temple, with the room meshes picked as objects; see [temple_hand.md](temple_hand.md) |
| Scripts can point the hand and camera at the player's temple | todo | Not in our tree |
| Scripts can hold the hand at a set place | partial | Only the fixed-position animation (`HandSystem::StartFixedPosAnimation`, used by an abode's tap) pins the hand; no script command does |

## Size and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is scaled so it spans 3.2 units, growing past 150 units from the camera so it keeps about the same size on screen | done | `HandSystem::Place`: the scale 3.2 x handScale / 555.294, with the original's distance curve below 10 and past 150 |
| It faces along the line of sight through the cursor, laid level | done | `HandSystem::HandMatrixRotation` (`HandFrame.cpp`) with `hand_orientation::HeadingAlongRay` (`src/3D/HandOrientation.cpp`): the heading from the camera to cursor ray, laid level; test `HandOrientation.FacesAlongTheLevelledLineOfSight` |
| Looking straight down it keeps its heading | done | The heading is only taken from the ray when it is not vertical (`hand_orientation::HeadingAlongRay`); test `HandOrientation.KeepsItsHeadingLookingStraightDown` |
| Its up eases over 0.4 seconds to the slope of the land, or the face of the object, under it | done | `HandSystem::UpdateNormalUp` (`HandFrame.cpp`): `hand_orientation::k_UpEaseSeconds` (0.4 s) to the land normal or the felt face; no unit test of the ease |
| The slope is only taken afresh when the cursor moves across the screen | done | `UpdateNormalUp` takes a new target only when the mouse's x changed |
| Dragging the land it holds its up, and stands straight up when let go | done | The camera state uses the last normal up, frozen; entering the normal state snaps the up to (0, 1, 0) (`HandSystem::Place`) |
| A right hand is the left hand's mesh mirrored | todo | Our hand is drawn unmirrored (hand-and-interface.md, Pending) |
| The player picks a left or right hand in the options | todo | No left or right hand option in our tree |
| Holding a miracle or pouring food and wood lifts and tips the hand | done | `hand_grain` and the holding branch of `HandSystem::Place`; see [pouring.md](pouring.md) |

## Showing and hiding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is hidden while the menu is open and while a script's cinematic has the interface | partial | The INVISIBLE state hides the hand (`NotDrawn`) for the interface's inactive state, as when a script holds the bars (`HandSystem::Update`); whether our menu hides it was not checked |
| Scripts can make the hand invisible and bring it back | todo | Not found in our tree (unconfirmed which commands do it) |
| Where the hand can't be drawn, a plain mouse pointer is drawn instead | todo | Not in our tree (the system cursor is not ported) |
