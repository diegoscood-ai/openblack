/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>

#include <entt/resource/resource.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/CameraPath.h"
#include "3D/CreatureCaveTargets.h"
#include "3D/TempleInteriorInterface.h"
#include "CameraModel.h"
#include "Common/Zoomer.h"
#include "Creature/TattooEditor.h"

namespace openblack
{

/// The camera inside the temple and its rooms' cameras. Each room's camera comes in along the room's path, then the
/// player takes it over:
/// - in the main room they turn the room around them and lean in and out, by dragging its walls, with
///   the arrow keys and at the screen's top and bottom edges, and walk through its doors by clicking them
/// - in the creature's room they look around from where its path ends, by dragging and with the
///   arrow keys
/// - in the rooms of pictures (the challenge, save game, options, multiplayer and credits rooms, which share one kind
///   of camera) they turn the room around them and raise and lower the camera, by dragging its walls
///   and with the arrow keys, and walk back to the main room by clicking its door
class TempleCameraModel final: public CameraModel
{
public:
	using Room = TempleRoom;
	/// Each room but TempleRoom::Unknown
	static constexpr size_t k_RoomCount = 7;
	using Paths = std::array<entt::resource<CameraPath>, k_RoomCount>;

	struct Pose
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};

	/// Where a ray from the camera meets a room: the cylinder of its walls or its floor
	struct RoomHit
	{
		/// From the room's axis
		float distance;
		/// Around the room's axis, as atan2 of z and x
		float angle;
		float height;
		bool floor;
		glm::vec3 point;
		/// What the hand turns to: up from the floor, and back along the ray from the walls
		glm::vec3 normal;
	};

	/// The radius of the main room's walls
	static constexpr float k_RoomRadius = 45.0f;

	/// A room's cylinder of walls about its axis, standing on its floor
	struct Cylinder
	{
		glm::vec3 centre;
		float radius;
		/// Whether rays meet its floor, which the creature's room has none of
		bool floor {true};
	};
	/// The cylinder each room's camera casts the cursor against
	[[nodiscard]] static Cylinder CylinderOf(Room room);
	/// A double click while the press holds the camera on the island's map: the point of the map it asks to leave the
	/// temple for, the press's own point; none for any other button or once a leave is taken
	[[nodiscard]] static std::optional<glm::vec3> MapDoubleClickLeave(uint8_t button, bool leaving, glm::vec3 mapFocus) noexcept
	{
		return button == 2 && !leaving ? std::optional(mapFocus) : std::nullopt;
	}
	/// The rooms' press while a script holds the cinema bars: none, and a button held through them is not a press until it
	/// is let go, as the rooms hear a press only as the button goes down
	[[nodiscard]] static constexpr uint8_t PressUnderCinemaBars(bool& waitsForRelease, uint8_t button, bool cinemaBars) noexcept
	{
		if (cinemaBars)
		{
			waitsForRelease = button != 0;
			return 0;
		}
		if (waitsForRelease)
		{
			waitsForRelease = button != 0;
			return 0;
		}
		return button;
	}

	/// A ray from origin along direction, which is scaled to the near plane as the game's picking rays are, against a
	/// room at the origin
	[[nodiscard]] static std::optional<RoomHit> RayCastRoom(glm::vec3 origin, glm::vec3 direction);
	[[nodiscard]] static std::optional<RoomHit> RayCastRoom(glm::vec3 origin, glm::vec3 direction, const Cylinder& room);
	/// The main room's door a hit on its walls is in: the eighth of the room it is in, when
	/// it is within that eighth's door
	/// The main room's doors leave a tenth of a radian clear at each side of their eighth of the wall; the doors back to it
	/// from the other rooms leave 0.13
	[[nodiscard]] static std::optional<uint32_t> DoorAt(const RoomHit& hit, bool mainRoom = true);
	/// The room behind a door of the main room: none behind the door out and the wall of the scrolls
	[[nodiscard]] static std::optional<Room> RoomBehindDoor(uint32_t door);
	/// The main room's view at a turn about the room and a lean between looking down at the pool, at 0, and up at the
	/// room, at 1
	[[nodiscard]] static Pose OrbitPose(float yaw, float lean);
	/// Dragging the main room's floor or walls up or down leans the view by the mouse's move down the screen in pixels
	/// times the frame's time and 0.06, less towards the screen's sides (the mouse's x from -1 to 1)
	[[nodiscard]] static float DragLean(float mouseDown, float dt, float mouseX);
	/// With no button down, how the main room's view leans by itself: the speed, and how fast its ramp rises (none: the
	/// ramp drops to 0). Past 0.8 of the way to the screen's top or bottom edge it leans away from it, faster the
	/// nearer the edge; else over the pool it leans down to it
	struct EdgeLean
	{
		float speed {0.0f};
		std::optional<float> rampRate;
	};
	[[nodiscard]] static EdgeLean EdgeLeanOf(float mouseY, bool overPool, float leanTarget);
	/// The lean this frame from that speed and the ramp (from 0 to 1, eased along half a cosine)
	[[nodiscard]] static float LeanStep(float speed, float dt, float ramp);
	/// The main room's camera eases onto its aim at once, and over a fifth of a second times the blend while it goes over
	/// to the pool's map. Looking at a scroll it eases at once
	[[nodiscard]] static float MainRoomEaseTime(float mapBlend);
	/// The main room's arrow keys: left and right turn by two radians a second, up and down lean by 0.85 a second; the
	/// lean held between none and 1.5
	struct OrbitAims
	{
		float yaw {0.0f};
		float lean {0.0f};
	};
	[[nodiscard]] static OrbitAims OrbitKeys(OrbitAims aims, float dt, bool left, bool right, bool up, bool down);
	/// The creature room's zoom toward its aim: in over a second, back in half that
	[[nodiscard]] static float CaveZoomStep(float zoom, float aim, float dt);
	/// The creature room's camera eases onto its look over 0.15 s, and a tenth of a second longer the more it is zoomed
	[[nodiscard]] static float CaveLookEaseTime(float zoom);
	/// The point on the cursor's line from the eye that is the given depth along the view (the camera's forward)
	[[nodiscard]] static glm::vec3 PointAtViewDepth(glm::vec3 eye, glm::vec3 direction, glm::vec3 forward, float depth);
	/// Where the cursor meets the room as a room is drawn: the room's mesh where it is picked, else the point kept; the
	/// floor where the cursor meets it in place of either; and the creature's body under the cursor over all of them
	[[nodiscard]] static glm::vec3 NextRoomCursorPoint(glm::vec3 kept, std::optional<glm::vec3> meshPick,
	                                                   std::optional<glm::vec3> floor, std::optional<glm::vec3> creature);
	/// Walking through a door: the camera heads to the door over 3 s; past 1.5 s it heads through over 7 s; past 5 s it is
	/// through. A new press skips the walk
	enum class DoorStage : uint8_t
	{
		Approaching,
		Entering,
		Through,
		Skipped,
	};
	struct DoorWalk
	{
		DoorStage stage {DoorStage::Approaching};
		float time {0.0f};
		/// The stage turned to heading through the door this frame
		bool headsThrough {false};
	};
	[[nodiscard]] static DoorWalk DoorWalkStep(DoorStage stage, float time, float dt, bool newPress);
	/// What a walk through a door asks of the temple's doorway, by the walk as it stood at the frame's start, before the
	/// camera steps: heading to the door it is shut at once; heading through, the doorway ahead swings open from the main
	/// room (the way out's with no leaf of its own), or the main room's doorway to this room quickly from another room
	struct DoorwayCall
	{
		enum class Kind : uint8_t
		{
			FastClose,
			Open,
		};
		Kind kind {Kind::FastClose};
		std::optional<uint32_t> leaf;
		float from {0.0f};
		float rate {0.0f};
	};
	[[nodiscard]] static std::optional<DoorwayCall> DoorwayAtFrameStart(bool throughDoor, DoorStage stage, Room room,
	                                                                    std::optional<Room> doorRoom);
	/// The creature room's look dragged: the look at the press plus the lens's width across the screen (2 tan of half
	/// the field of view) times the mouse's move back from the press over the screen's width, on both axes
	[[nodiscard]] static glm::vec2 CaveDragLook(glm::vec2 lookAtPress, glm::vec2 pressMouse, glm::vec2 mouse, float width);
	/// A turn brought into [0, 2 pi] by whole turns, one at a time
	[[nodiscard]] static float WrapTurn(float turn);
	/// Held on the pool's map, the camera looks at the point pressed from 6 away: the turn and the pitch from the point
	/// to the camera at the press, less 6 turns' worth of the screen's widths moved across and plus 2 of its heights moved
	/// down, the pitch held between a fifth of a turn and three eighths of half a turn
	[[nodiscard]] static Pose MapLook(glm::vec3 focus, float yawAtPress, float pitchAtPress, glm::vec2 movedOverScreen);
	/// A pose about the main room's centre turned to face one of its doors
	[[nodiscard]] static Pose TurnToDoor(const Pose& pose, int32_t door);
	/// Whether a press walks back out of the creature's room: the room's cursor point within 79 of the temple's middle,
	/// and either a new press on its door (0), or any press while the door under the cursor (or -1, none) is the one the
	/// room last walked back through (none at first) and the camera within 25 of that point
	[[nodiscard]] static bool CaveWalksBack(bool newPress, bool pressed, int32_t door, int32_t lastDoor, float pointFromMiddle,
	                                        float pointFromCamera);
	/// Whether a room is one of the rooms of pictures, which share one kind of camera
	[[nodiscard]] static bool IsPictureRoom(Room room);
	/// The view of a room of pictures at a turn about its centre, a lean between looking up, at 0, and
	/// down, at 1, and a height. The credits room turns about where its path ends instead of its centre.
	[[nodiscard]] static Pose PictureOrbitPose(Room room, float yaw, float lean, float height, glm::vec3 pathEnd);
	/// The turn a room of pictures' camera starts at, which looks along the end of the room's path
	[[nodiscard]] static float PictureStartYaw(const Pose& pathEnd);
	/// How far a room of pictures' camera aims from its view of the room toward a close look, at a blend toward it: along
	/// half a cosine, none at a blend of 0 or under (or an unordered one) and all of the way at 1 or over
	[[nodiscard]] static float CloseLookAlong(float blend);
	/// Whether the main room's camera starts afresh as the player takes it back from a scroll: only when the look had
	/// come all the way to the scroll (the blend exactly 1); before, it keeps its turn and lean
	[[nodiscard]] static constexpr bool MainRoomStartsAfreshAfterLook(float blend) { return blend == 1.0f; }
	/// A room of pictures' camera's aim between its view of the room and a close look, at a blend toward the look
	[[nodiscard]] static Pose TowardCloseLook(const Pose& room, const Pose& look, float blend);
	/// How long a room of pictures' camera, with the player in control, takes to ease onto its aim at a blend toward a
	/// close look: half a second at the full blend, and none once it is back with the room, where it stays on its aim
	[[nodiscard]] static float PictureEaseTime(float blend);
	/// Where along the main room's path its camera starts when it eases in from another room's: of the path's first
	/// three seconds, in tenths of a second, the first time whose point is nearest the camera
	[[nodiscard]] static float NearestIntroTime(const CameraPath& path, glm::vec3 camera);
	/// A room of pictures' aims: its turn, its lean (0 to 1) and its camera's height
	struct PictureAims
	{
		float yaw;
		float lean;
		float height;
	};
	/// The arrow keys' step of a room of pictures' aims: left and right turn by the frame's seconds; forwards and back
	/// tilt the lean, taken from -1 to 1, by 1.85 a second and move the height against the tilt by 30 times it a
	/// second, each in turn, then the lean is brought back to 0 to 1
	[[nodiscard]] static PictureAims PictureKeysStep(PictureAims aims, float dt, bool left, bool right, bool forwards,
	                                                 bool backwards);
	/// Dragging a room of pictures' floor or walls: the turn aimed at is the turn reached plus the turn from the pressed
	/// point's angle to the cursor's, wrapped to half a turn either way, and the height aimed at the height reached plus
	/// the pressed point's height less the cursor's
	[[nodiscard]] static PictureAims PictureDragAims(PictureAims aims, float yawReached, float heightReached, float pressAngle,
	                                                 float pressHeight, float hitAngle, float hitHeight);
	/// Where a room of pictures' walk back starts: halfway between the camera's aim and the door's way through, at the
	/// door's height
	[[nodiscard]] static Pose WalkBackStart(const Pose& aim, const Pose& through);
	/// The creature's room camera's look from a point, as a heading about the vertical and a pitch up and down
	[[nodiscard]] static Pose LookPose(glm::vec3 from, float heading, float pitch);
	/// The heading and pitch of a look at a point, as the game's camera works them out
	[[nodiscard]] static glm::vec2 HeadingAndPitch(const Pose& look);
	/// What the creature's room is zoomed to after a press, which zooms it back: nothing, except the creature, which
	/// stays the target zoomed to so that the tattoo editor stays open and can still be closed
	[[nodiscard]] static std::optional<CreatureCaveTargets::Target>
	ZoomedToAfterPress(std::optional<CreatureCaveTargets::Target> zoomedTo);

	TempleCameraModel(Paths paths, Room room);
	~TempleCameraModel() final;

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) final;
	void HandleActions(std::chrono::microseconds dt) final;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) final;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const final;
	[[nodiscard]] glm::vec3 GetTargetFocus() const final;
	[[nodiscard]] std::optional<Lens> GetLens() const final;

	/// Comes into a room along its path. When blending, the camera eases from where it is onto the path.
	void StartIntro(Room room, bool blendFromCurrent);
	/// Cuts to a room, past its path, as the temple does when it sends the player straight to a room
	void GoToRoom(Room room);
	/// Where the cursor meets the room's cylinder this frame, if it does, which the room puts the hand on when it
	/// is the floor
	[[nodiscard]] const std::optional<RoomHit>& GetCursorHit() const { return _cursorHit; }
	/// The line from the camera through the cursor, which the game picks the room's mesh along
	[[nodiscard]] const std::optional<Pose>& GetCursorRay() const { return _cursorRay; }
	/// Walks through the main room's door to a room, as clicking it does, while the player has the main room's camera
	/// @return Whether the camera is on its way
	bool GoThroughDoorTo(Room room);
	/// Looks at a scroll from close by, from position at lookAt. A press elsewhere, or the arrow keys, sends the camera
	/// back.
	void LookAtSubMesh(glm::vec3 position, glm::vec3 lookAt);
	/// How close the camera has come to what it looks at, from 0 to 1
	[[nodiscard]] float GetSubMeshZoom() const { return _subMeshZoom; }
	/// How long the camera's last step gave it to ease onto its aim, in seconds
	[[nodiscard]] float GetEaseTime() const { return _originTime; }
	/// The main room's lean the player aims at, which the screen's edges, a drag and the arrow keys move
	[[nodiscard]] float GetLeanTarget() const { return _leanTarget; }
	/// How far the main room's edge or pool lean has ramped up, from 0 to 1
	[[nodiscard]] float GetEdgeRamp() const { return _edgeRamp; }
	/// Whether the player has the room's camera, past its path in
	[[nodiscard]] bool IsInControl() const { return _state == ControlOf(GetRoom()); }
	/// Whether a dialog is up over the temple, as the game's options are in the Game Options room, which takes the rooms
	/// of pictures' cameras from the player
	void SetDialogOpen(bool open) { _dialogOpen = open; }
	/// How much a room of pictures turns by itself this frame, or nothing when the arrow keys turn it instead
	/// @param turnTime How long the room has turned by itself, which the turn speeds up over. It goes on by dt.
	/// @param aroundPathEnd The room turns about where its path ends, as the library does, which keeps still
	/// @param futureWordsShown The room is the future room and it shows its words
	[[nodiscard]] static std::optional<float> PictureTurnByItself(float& turnTime, float dt, bool dialogOpen,
	                                                              bool aroundPathEnd, bool futureWordsShown);
	/// Whether the future room shows its words. While it does, its camera turns the room slowly by itself.
	void SetFutureWordsShown(bool shown) { _futureWordsShown = shown; }
	/// The main room's door the cursor is over, while the player has its camera
	[[nodiscard]] std::optional<uint32_t> GetHoveredDoor() const { return _hoveredDoor; }
	/// The door whose sign the main room lights: the one under the cursor, found every frame the main room's camera
	/// runs, whatever it is doing, and kept while the player is in another room
	[[nodiscard]] static std::optional<uint32_t> NextHighlightedDoor(Room room, std::optional<uint32_t> kept,
	                                                                 std::optional<uint32_t> underCursor)
	{
		return room == Room::Main ? underCursor : kept;
	}
	[[nodiscard]] std::optional<uint32_t> GetHighlightedDoor() const { return _highlightedDoor; }
	/// Whether going straight to a room does anything (shuts the main room's doorway, and goes): only to another room
	[[nodiscard]] static bool ChangesRoom(Room current, Room asked) { return asked != current; }
	/// Whether the cursor is over the main room's pool, and whether a press began on it
	[[nodiscard]] bool IsOverPool() const { return _overPool; }
	[[nodiscard]] bool IsPressingPool() const { return _pressingPool; }
	/// Whether the cursor is over the door a room other than the main room is left by, while the player has its camera
	[[nodiscard]] bool IsOverWayBack() const { return _overWayBack; }
	/// Whether the camera looks at a scroll from close by
	[[nodiscard]] bool IsLookingAtSubMesh() const { return _lookingAtSubMesh; }
	/// The creature's room's target the cursor is over, while the player has its camera
	[[nodiscard]] std::optional<CreatureCaveTargets::Target> GetCaveTarget() const { return _caveTarget; }
	/// Whether the camera is zooming to one of the creature's room's targets, or there
	[[nodiscard]] bool IsZoomingToCaveTarget() const { return _caveZoomTarget == 1.0f; }
	/// The point of the pool the island's map was double clicked at, once, in the camera's space: the player asks to
	/// leave the temple for that place
	[[nodiscard]] std::optional<glm::vec3> TakeMapDoubleClick() { return std::exchange(_mapDoubleClick, std::nullopt); }
	/// The temple takes the leave the map was double clicked for: later double clicks ask for nothing. One it refuses
	/// leaves the map's double click free
	void TakeMapLeave() { _leavingByMap = true; }

private:
	enum class State : uint8_t
	{
		Intro,
		/// The main room's control
		Orbit,
		/// The creature's room's control
		Look,
		/// The rooms of pictures' control
		PictureOrbit,
		ThroughDoor,
	};
	/// How far through a door the camera is
	struct Input
	{
		/// 0, held, or double clicked
		uint8_t button;
		glm::vec2 mouse;
		/// The mouse from -1 to 1 across the screen, down it
		glm::vec2 mouseScreen;
		/// The screen's size in pixels
		glm::vec2 screen;
		std::optional<RoomHit> hit;
		/// Where the camera drawn last is
		glm::vec3 eye {0.0f};
		std::optional<uint32_t> door;
		/// The door back to the main room the cursor is over, from the other rooms
		std::optional<uint32_t> doorBack;
		/// Where on the screen the creature's room's targets are, in pixels down from the top
		std::array<std::optional<glm::vec2>, CreatureCaveTargets::k_Count> caveTargets;
		/// The cursor is over the creature's body in its room, which makes the creature the target ahead of the others
		bool overCaveCreature {false};
	};

	void Step(float dt, const Input& input);
	void UpdateIntro(float dt, const Input& input);
	void UpdateOrbit(float dt, const Input& input);
	void UpdateLook(float dt, const Input& input);
	/// The creature's room's targets: a click on one zooms to it, a press anywhere or the arrow keys zoom back,
	/// and the exit, zoomed to, leaves the temple
	void UpdateCaveTargets(float dt, const Input& input);
	/// The place of a point of the creature's room's mesh
	[[nodiscard]] static std::optional<glm::vec3> CaveMeshPoint(uint32_t index);
	/// Zooms the camera to a target of the creature's room
	void ZoomToCaveTarget(CreatureCaveTargets::Target target);
	/// Zoomed back to no target at once
	void ResetCaveTargets();
	/// Zoomed onto the creature, the tattoo editor opens on it with the cave's view of it, which the camera then takes
	/// as the editor turns it; false, and nothing opened, without a creature or the room's places
	bool OpenTattooEditor();
	/// The tattoo editor the zoom onto the creature opened closes as it is, keeping the view it had turned to
	void CloseTattooEditor();
	/// A room of pictures' camera's work in every state: the turn, lean and height ease after their targets
	void UpdatePictureCamera(float dt);
	void UpdatePictureOrbit(float dt, const Input& input);
	/// A room of pictures' camera looking at something close up, the frame the player takes it away in too
	void UpdatePictureLook(float dt, bool leftByPlayer);
	/// As when the temple cuts straight to a room: back to looking along the end of the room's path
	void ResetPictureCamera(Room room);
	/// Walks back to the main room through the door of it a room is behind
	void WalkBackThrough(int32_t door);
	/// The door the creature's room last walked back through, -1 before it ever has, kept for the whole game
	int32_t _caveLastDoor {-1};
	/// The control state of a room's camera
	[[nodiscard]] State ControlOf(Room room) const;
	/// Where a room's camera has the player as it takes over, which its path eases into over its last second
	[[nodiscard]] Pose ControlPose(Room room) const;
	/// The pose at the end of a room's path
	[[nodiscard]] Pose PathEnd(Room room) const;
	void UpdateThroughDoor(float dt, const Input& input);
	void ChangeState();
	void FollowTemple();
	[[nodiscard]] Room GetRoom() const;
	[[nodiscard]] bool InMainRoom() const { return GetRoom() == Room::Main; }

	/// Each room's path in, data/citadel/engine/<room>.cam
	Paths _paths;
	State _state {State::Intro};
	State _nextState {State::Intro};

	// Where the camera is and looks, easing to the targets over the times
	Zoomer3 _origin;
	Zoomer3 _focus;
	Pose _target {};
	float _originTime {0.5f};
	float _focusTime {0.5f};
	bool _easeToTarget {true};

	// The path in
	float _introTime {0.0f};
	float _blendTime {0.0f};
	bool _blendFromPrevious {false};
	Zoomer3 _previousOrigin;
	Zoomer3 _previousFocus;

	// The main room's turn and lean, and where they're heading. The lean starts at half way, as the constructor puts it.
	Zoomer _yaw;
	Zoomer _lean;
	float _yawTarget {0.0f};
	float _leanTarget {0.5f};
	/// How long the mouse has been at the edge of the screen, building up the lean's speed
	float _edgeRamp {0.0f};

	// The creature's room: the look from the end of its path, where it was when the mouse was pressed, and how fast the
	// arrow keys turn it
	glm::vec3 _lookFrom {0.0f};
	glm::vec2 _look {0.0f};
	glm::vec2 _lookAtPress {0.0f};
	glm::vec2 _lookSpeed {0.0f};

	/// A room of pictures' turn, lean and height, kept for each room while the player is in the temple
	struct PictureCamera
	{
		Zoomer yaw;
		Zoomer lean;
		Zoomer height;
		float yawTarget {0.0f};
		float leanTarget {0.0f};
		float heightTarget {0.0f};
	};
	std::array<PictureCamera, k_RoomCount> _pictures {};
	/// The arrow keys' turn, lean and rise of a room of pictures' camera
	static void UpdatePictureKeys(float dt, PictureCamera& picture);

	std::optional<RoomHit> _cursorHit;
	/// From the camera, towards the point the cursor is over
	std::optional<Pose> _cursorRay;

	// The press the mouse is held from
	bool _wasPressed {false};
	bool _dialogOpen {false};
	bool _futureWordsShown {false};
	bool _overPool {false};
	bool _pressingPool {false};
	bool _overWayBack {false};
	/// How long a room of pictures' camera has turned by itself, which its turn speeds up over
	float _pictureOrbitTime {0.0f};
	std::optional<RoomHit> _pressHit;
	float _lastHitAngle {0.0f};
	glm::vec2 _lastMouse {0.0f};
	glm::vec2 _pressMouse {0.0f};

	/// The main room's camera starts facing the scroll, unturned and leaning a little up: as it takes control, and as it
	/// is given back from a scroll it had come all the way to
	void StartOrbitAfresh();
	// Looking at a scroll from close by, and how close, which eases in at 0.75 a second and out at 0.7
	std::optional<Pose> _subMeshLook;
	bool _lookingAtSubMesh {false};
	float _subMeshZoom {0.0f};
	/// The press the player took a room of pictures' camera away from a close look with is still held
	bool _pressLeftLook {false};
	/// A button held through a script's cinema bars, not a press until it is let go
	bool _pressWaitsForRelease {false};
	/// Holding the press on the pool's map: the point pressed, which the camera turns about, and the turn and tilt it
	/// had from it as the press began
	glm::vec3 _mapFocus {0.0f};
	float _mapYaw {0.0f};
	float _mapPitch {0.0f};
	std::optional<glm::vec3> _mapDoubleClick;
	bool _leavingByMap {false};
	std::optional<uint32_t> _hoveredDoor;
	std::optional<uint32_t> _highlightedDoor;

	// The creature's room's targets: the one the cursor is over, the one zoomed to and where the camera looks at it from,
	// and how far it has zoomed, from 0 to 1, after the zoom's target
	std::optional<CreatureCaveTargets::Target> _caveTarget;
	std::optional<CreatureCaveTargets::Target> _caveZoomedTo;
	Pose _caveLook {};
	float _caveZoom {0.0f};
	/// The zoom as the frame began, before its step, which the look's ease reads
	float _caveZoomAtFrameStart {0.0f};
	float _caveZoomTarget {0.0f};
	/// The tattoo editor's view of the creature, set up the first time it opens in a visit to the temple and kept as the
	/// editor turns it, so that it opens again as it was left
	std::optional<creature_tattoo_editor::Orbit> _creatureView;
	/// Whether the zoom onto the creature opened the tattoo editor, which this camera then closes; one opened from
	/// elsewhere is left alone
	bool _tattooEditorOpened {false};
	/// Whether the mouse has moved since it was pressed, which makes the press a drag rather than a click
	bool _pressDragged {false};

	// Through a door
	float _doorTime {0.0f};
	DoorStage _doorStage {DoorStage::Approaching};
	Pose _doorStart {};
	Pose _doorEnd {};
	std::optional<Room> _doorRoom;
};

} // namespace openblack
