/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Camera/CameraFrameTime.h"
#include "Camera/CameraModeStack.h"
#include "Common/Zoomer.h"

namespace openblack
{
class Camera;
class CameraModel;
} // namespace openblack
namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs
namespace openblack::edt
{
struct EDTTrack;
} // namespace openblack::edt
namespace openblack::camera_track
{
class WayRunner;
} // namespace openblack::camera_track

/// The script camera (docs/wiki/camera/script-camera.md#the-script-mode): the part of the game camera the script moves, and the
/// script camera mode.
///
/// - The game camera keeps its state in zoomers (Common/Zoomer.h): focus, position and the field of view (radians),
///   moved once a frame.
/// - START_CAMERA_CONTROL pushes a script mode; while it lives the player's mode does not move the camera and no other
///   task gets it. Releasing it gives the player a new mode that starts where the script left the camera.
/// - The script mode is a follow mode (no thing, time factor 0.2, "behind" on, no zoom): it can follow a thing with the
///   focus (FOCUS_FOLLOW) and with the position (POSITION_FOLLOW) once a frame.
/// - openblack has no mode stack: the player's mode is Camera + DefaultWorldCameraModel, and this module stands for
///   the camera's zoomers while a script mode lives. The player's Camera keeps its own Zoomer3 (GetOriginZoomer /
///   GetFocusZoomer): Begin takes them as they are and End hands them back, so the two sets behave as one.
namespace openblack::script_camera
{

/// The default field of view: 70 degrees
constexpr float k_DefaultFov = 1.2217305f;
/// The camera's seconds of a frame are at most 0.1
constexpr float k_MaxFrameSeconds = 0.1f;
/// A squared distance: closer than this to the destination counts as arrived
constexpr float k_ArrivedDistanceSquared = 0.001f;
/// The world's disc around (2560, 0, 2560); a destination farther than sqrt(1.225e7) = 3500 is pulled back to
/// d / (|d| * 0.000285796) (about 1/3499) in 3 s
constexpr float k_DiscCentre = 2560.0f;
constexpr float k_DiscRadiusSquared = 1.225e7f;
constexpr float k_DiscScale = 0.000285795948f;
constexpr float k_DiscSeconds = 3.0f;
/// The drawn camera stays 1 m above the landscape
constexpr float k_GroundClearance = 1.0f;
/// SET_CAMERA_LENS / MOVE_CAMERA_LENS / CAMERA_PROPERTIES: degrees to radians
constexpr float k_DegreesToRadians = 0.0174532924f;

/// The script mode's follow time factor
constexpr float k_ScriptFollowTimeFactor = 0.2f;
/// The follow distance is kept in 2 .. 1500
constexpr float k_FollowMinDistance = 2.0f;
constexpr float k_FollowMaxDistance = 1500.0f;
/// The follow pitch is at least this
constexpr float k_FollowMinPitch = 0.241660982f;
/// The seconds since the last mode change that end the slower start
constexpr float k_FollowSettleSeconds = 2.0f;
/// A thing's viewing distance is its height x 8
constexpr float k_ThingViewingDistanceFactor = 8.0f;
/// A wall hugger's game angle (2048 per circle) x 2 x pi / 2048 - pi / 2
constexpr float k_FollowAngleScale = 0.00153398083f;
constexpr float k_HalfPi = 1.57079637f;
/// HeadingAndPitchFromPoints: |dx| and |dz| below 0.01 (a double) give heading 0 and this near-vertical pitch
constexpr double k_VerticalEpsilon = 0.01;
constexpr float k_VerticalPitch = 1.53938043f;
/// HeadingAndPitchFromPoints: pi
constexpr float k_Pi = 3.14159274f;
/// Facing an object: the heading's limit 8 pi, 2 pi and the pitch 0.1
constexpr float k_FaceMaxHeading = 25.1327419f;
constexpr float k_TwoPi = 6.28318548f;
constexpr float k_FacePitch = 0.1f;
/// A creature faces, for a camera sent to face it, its body's yaw plus a full turn, less this
constexpr float k_CreatureFacingOffset = 2.5f;

/// The dual camera (START_DUAL_CAMERA, CREATE_DUAL_CAMERA_WITH_POINT): heading pi / 4, pitch pi / 8 and the distance
/// factor 1 with two things (camera_stack::k_DualHeading and the rest, Camera/CameraModeStack.h), 1.2 with a point
constexpr float k_DualPointDistanceFactor = 1.20000005f;
/// The seconds of a dual camera's destination are 2 right after the mode change, down to 1 at 1.5 s and on
constexpr float k_DualSettleSeconds = 1.5f;
/// The second height of the mean when B is a point
constexpr float k_DualPointHeight = 1.0f;
/// The radius of a thing that is not an Object
constexpr float k_DualDefaultRadius = 30.0f;
/// The larger height is scaled by this
constexpr float k_DualHeightFactor = 1.39999998f;
/// A double: B straight over A keeps the heading
constexpr double k_DualFlatEpsilon = 0.0099999997764825821;

/// What the pop of the current player's mode leaves current, as the camera's pop of a mode: with a mode under it, that
/// mode is current again and restarted; with none, the game makes a new player's mode
enum class AfterPop : uint8_t
{
	/// One of this module's modes (a dual camera), or a placed path, is current again: their restart does nothing more
	ModeUnder,
	/// The player's mode under it (or the camera's model it went over) is current again: it restarts, the player's own
	/// flying back to the view it had in 0.1 s, a watched fight kept
	PlayersModeUnder,
	/// Nothing under it: a new player's mode starts from the camera as it is, with no flight and no fight watched
	NewPlayersMode,
};
/// `modeCurrent`: one of this module's modes is current after the pop; `pathHasCamera`: a placed path has the camera
/// then; `playersModeUnder`: a player's mode (or the model the popped one went over) is under it
[[nodiscard]] constexpr AfterPop AfterPopOf(bool modeCurrent, bool pathHasCamera, bool playersModeUnder)
{
	if (modeCurrent || pathHasCamera)
	{
		return AfterPop::ModeUnder;
	}
	return playersModeUnder ? AfterPop::PlayersModeUnder : AfterPop::NewPlayersMode;
}

/// The state: the camera's zoomers, the script mode and its camera path
struct State
{
	Zoomer3 position;
	Zoomer3 focus;
	Zoomer fov; ///< Radians
	/// Seconds since the last mode change (0 on a mode switch, += the frame's camera seconds, 2 after PlaceFollowNow)
	float modeSeconds = 0.0f;
	/// The last frame's camera milliseconds, the whole ms the camera's frame time is made from (FrameSecondsOf), which a
	/// saved game keeps
	uint32_t frameMs = 0;
	/// Whether the last drawn frame's camera, before its shake, was not where the camera was drawn the frame before
	/// (ApplyShake), which a saved game keeps
	bool drawnMoved = false;
	/// The camera drawn the frame before the last update (ApplyShake's), which the island camera's update reads as the
	/// drawn camera and a saved game keeps inside the citadel
	glm::vec3 drawnBefore {0.0f};
	/// RUN_CAMERA_PATH: the track, null = none, and the runner of its position way. The focus way's runner is never
	/// evaluated by the path update
	std::shared_ptr<const edt::EDTTrack> track;
	std::unique_ptr<camera_track::WayRunner> positionRunner;
	/// The game ms run along the path
	int32_t pathMs = 0;
	/// The follow mode's state. The pitch and the distance are not set by the constructors: (inferred) never read
	/// before PositionFollow / FocusAndPositionFollow / CAMERA_PROPERTIES write them, so 0 here
	entt::entity positionThing = entt::null;
	entt::entity focusThing = entt::null; ///< When null, the position thing is the focus thing
	float heading = 0.0f;                 ///< Radians
	float pitch = 0.0f;                   ///< Radians
	float distance = 0.0f;
	float timeFactor = k_ScriptFollowTimeFactor;
	bool behind = true; ///< The heading is relative to a wall hugger's angle
	/// STORE_CAMERA_DETAILS: the drawn position and the drawn focus; not the FOV
	glm::vec3 storedPosition {0.0f};
	glm::vec3 storedFocus {0.0f};
	/// The FOV last given to the renderer (radians). The config's FOV is only rewritten when the zoomer leaves it, so a
	/// player's own FOV stays until a script changes the lens
	float appliedFov = k_DefaultFov;

	State();
	~State();
};

/// The camera's frame time from the frame's whole camera milliseconds: ms x 0.001, at most 0.1, each step a float
[[nodiscard]] constexpr float FrameSecondsOf(uint32_t ms)
{
	return camera_frame::CappedSeconds(ms);
}

/// The one game camera of the running game
State& Get();
/// This module's modes on the camera's stack (the script mode and the dual cameras), above the player's mode, which the
/// camera stack service keeps (Locator::cameraStack, with the camera's models the new player's modes went over). The
/// script mode's own state is State's: a second script mode can only go on top of a dual camera over the first, which
/// stays under the new player's mode its end puts on top (see ModeStack::EndScript). The stack's 12 places count the
/// player's mode and the other systems' modes under these (ModeStack::Push). The creature interaction's camera is on it
/// too (Cover / Uncover): a mode under it is not current, moves the camera nor is updated; a dual camera, which never asks
/// whether the current mode can be left, can go on top of it
[[nodiscard]] camera_stack::ModeStack& Modes();

/// A land change: the camera's modes go (no script mode, no dual mode, no path, no follow), as the map's clear empties
/// the camera's stack of modes; the lens stays as the scripts left it
void ClearModes();
/// A new camera (a new game): ClearModes, and the FOV at 70 degrees at once
void Reset();
/// Another of the game's camera modes took the camera or gave it back, as the creature hand's camera does: the mode's
/// seconds start again from 0, as for every mode the game puts on its stack of camera modes or takes off it
void ModeChanged();

/// START_CAMERA_CONTROL: false when the current mode cannot be left, i.e. the script mode is the current one or the
/// creature interaction's camera has the view (Covered; the player's mode and a dual camera can always be left). Else the
/// script mode goes on top of the current mode, a dual camera included. The new mode does not touch the zoomers: over the
/// player's mode they start from the drawn camera `origin` / `focus` (approximate: the original's zoomers keep heading for
/// the destination the player had), over a dual camera from where the dual camera has them. It follows nothing, with time
/// factor 0.2 and "behind" on, heading 0, and the mode's seconds from 0
bool Begin(const glm::vec3& origin, const glm::vec3& focus);
/// The same with the player's zoomers taken as they are (value, speed, destination and time): the camera's zoomers are
/// the same for every mode, so the script mode goes on from wherever the player's (or the dual camera under it) was
/// heading
bool BeginFrom(const Zoomer3& origin, const Zoomer3& focus);
/// The end of control: one dual camera released (ReleaseDual), then the script mode deleted when it is the current one,
/// and a new player's mode on top of the whole stack, created from where the camera is. The modes under it stay: the
/// player's mode the script's camera went over keeps its own view (a copy of the camera's model as it was), and comes
/// back if the new one is popped (PopPlayersMode); a follow of Creature Mode's it went over keeps its model under the new
/// mode, which is a new player's camera (CreatureModeSystemInterface::KeepUnderPlayersMode), and has the camera again if
/// the new one is popped. With another mode current (a second dual camera, or the player's) the script mode, if any,
/// stays. The FOV back to 70 degrees in 0.5 s either way. Returns true when the script mode was
/// deleted
bool End();
/// The camera's pop of the current mode when it is the player's (a new player's mode, or the bottom one), as the end of a
/// creature hold that did not get the view pops it: the mode is deleted, and the mode under it, if any, is current
/// again and restarts (AfterPopOf): a player's mode under it gets its own model back and flies back to the view it had
/// in 0.1 s, keeping a watched fight; with nothing under it, a new player's mode starts from the camera as it is, with
/// no flight and no fight. The mode's seconds start again from 0. False when the player's mode is not current
bool PopPlayersMode();
/// A new player's mode on top of the current mode, as the player's take-back of the camera from a miracle's path makes:
/// the mode under it keeps its own model (a copy of the camera's as it is), and the camera's model starts again from the
/// camera as it is, with no flight and no fight watched, its help icons by the hand faded out. With another model than
/// the player's own (Creature Mode's follow), nothing is kept and that model goes on as it is. The mode's seconds start
/// again from 0. Returns its number (script_camera::ModeStack::PushPlayer)
uint32_t PushPlayersMode();
/// A new player's mode on top of a model another system keeps under it itself (Creature Mode's follow leaving the camera
/// fence): nothing is kept here, the caller has given the camera its new player's camera; the mode's seconds start again
/// from 0 and its help icons by the hand fade out. Its pop asks Creature Mode to give back the follow it keeps under
/// that mode, if any (CreatureModeSystemInterface::TakeBackFromUnderPlayersCamera). Returns its number
uint32_t PushPlayersModeOver();
/// The player's mode `id` is on the camera's stack
[[nodiscard]] bool HasPlayersMode(uint32_t id);
/// The mode another system kept under the player's mode `id` is gone (Creature Mode's follow dropped under it): the
/// player's camera under that one, `model`, is kept for it from now on, while it is still on the stack
void KeepUnder(uint32_t id, std::unique_ptr<CameraModel> model);
/// The zoomers handed back to the player's camera after End (the player's mode starts from the camera's zoomers):
/// position and focus as the script left them, with their speed and destination
void HandBack(Zoomer3& origin, Zoomer3& focus);
/// A script mode is on the camera's stack, current or not
[[nodiscard]] bool Active();
/// A script mode drives the camera: alive, and not under a camera test hook
[[nodiscard]] bool Drives();
/// Another system's mode (a placed path, Creature Mode's follow) is about to go on top of the current mode: when one of
/// this module's drove the camera, the camera's zoomers, the same for every mode, are handed back as it left them
/// (HandBack), for the new mode to go on from
void HandOverToAnotherMode();
/// Another system's mode over the current one went (its pop, its take-off, the turn's check): when one of this module's
/// is the current mode again and drives the camera, it goes on from the camera's zoomers as that mode left them
void TakeBackFromAnotherMode();
/// Another system's mode (a placed path, Creature Mode's follow) was pushed onto the camera's stack of modes: the bottom
/// mode a full stack deleted for it goes through its owner, whatever its kind, as the script camera's own pushes have it
/// (another system's mode through its own system, a new player's mode with the camera's model it kept)
void DeleteDroppedModes();

/// SetPosition / SetFocus: the path dropped, the follow of that part dropped (which with "behind" also sets the
/// heading to 0 for the position) and the zoomers set (Zoomer::SetPosition)
void SetPosition(const glm::vec3& position);
void SetFocus(const glm::vec3& focus);
/// MovePosition / MoveFocus: the same drops, then each zoomer heads for the point with speed 0 in `seconds` (camera
/// seconds: the wall clock)
void MovePosition(const glm::vec3& position, float seconds);
void MoveFocus(const glm::vec3& focus, float seconds);
/// All six zoomers set (no matter the mode), unless the camera is drawn there already, position and focus, each part
/// equal or not a number: then nothing is set, and a glide under way goes on
void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus);
/// Whether a drawn point is at the point asked for, as the camera's setter compares them
[[nodiscard]] bool DrawnThereAlready(const glm::vec3& drawn, const glm::vec3& asked);
/// The path and the focus follow dropped (the position follow stays), then the track `number` of camera.edt, from its
/// start
void RunPath(int32_t number);
/// The FOV zoomer (`fov` radians, `seconds` of game time)
void SetFov(float fov, float seconds);

// ---- Follow mode (the script mode's follows) ---------------------------------------------------------------------

/// What the follow mode and FaceObject read of a game thing (things are entt::entity)
struct ThingInfo
{
	/// Its map coords as a point: x, z / 6553.6, y = the land's altitude + the altitude above the land
	glm::vec3 mapPoint {0.0f};
	float height = 0.0f;
	/// An Object with a 3D object: its matrix's position, which the follow update uses instead of the map coords (not
	/// PlaceFollowNow). DrawnPointOf gives it
	std::optional<glm::vec3> drawnPoint;
	/// Whether the thing is available: false when flagged so, and for a villager whose final state is DYING. Read only
	/// by FocusFollow and Validate
	bool available = true;
	/// A flock: its position as a point (the leader's map coords, else the flock's own) and the leader's height, none
	/// without a leader
	bool isFlock = false;
	glm::vec3 flockPoint {0.0f};
	std::optional<float> leaderHeight;
	/// A wall hugger: its game angle (2048 per circle)
	std::optional<uint16_t> wallHugAngle;
	/// The facing direction (0 for a plain thing; a wall hugger's angle in radians; a creature's CreatureFacing of its
	/// body's yaw)
	float facingDirection = 0.0f;
	/// An Object (read by the dual camera): its 2D radius; none for a container (a flock or a town).
	/// (inferred) every thing with a Transform but a flock stands for an Object
	std::optional<float> radius2d;
};
/// nullopt: no such thing, or it is not available
using ThingReader = std::function<std::optional<ThingInfo>(entt::entity)>;
/// The thing as the follow reads it (the test hook's reader when one is set); none when there is no such thing
[[nodiscard]] std::optional<ThingInfo> Thing(entt::entity thing);

/// The position of a thing's 3D object, as the follow reads it: while the physics carries it and draws it (in flight, or
/// in a creature's hand), the pose that draw gave it this frame; a creature's body where the last frame placed it; a
/// villager's or an animal's matrix as its last draw left it (in the hand, the hand's); one not drawn yet, where it is
/// drawn this frame; an object with a mesh, its own position; none for a thing without a Transform or a 3D object
[[nodiscard]] std::optional<glm::vec3> DrawnPointOf(const ecs::Registry& registry, entt::entity entity);

/// The pure parts, exposed for the tests

/// T = (modeSeconds > 2 ? 1 : (modeSeconds / 2) x (1 - 2) + 2) x timeFactor: twice the factor right after a mode
/// change, the factor itself from 2 s on
[[nodiscard]] float FollowSeconds(float modeSeconds, float timeFactor);
/// <= 2 -> 2; < 1500 -> itself; else 1500
[[nodiscard]] float ClampFollowDistance(float distance);
/// <= 0.241661 -> 0.241661 (the caller stores it back)
[[nodiscard]] float FollowPitch(float pitch);
/// With "behind" and a wall hugger, heading - (float(2 a) x pi / 2048 - pi / 2); else heading
[[nodiscard]] float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle);
/// height x 8
[[nodiscard]] float ThingViewingDistance(float height);
/// The point the scripts read for the camera's position or focus inside the citadel: far outside the world on every
/// axis
constexpr glm::vec3 k_OutOfWorldPoint {50000.0f, 50000.0f, 50000.0f};
/// What a script reads for the camera's position or focus: the drawn camera's own point, or the point outside the world
/// while the player is inside the citadel
[[nodiscard]] constexpr glm::vec3 ReadCameraPoint(glm::vec3 drawn, bool insideCitadel)
{
	return insideCitadel ? k_OutOfWorldPoint : drawn;
}
/// p + d (sin h cos q, sin q, cos h cos q): the sines and cosines unrounded, cos q a float, each product and sum a float
[[nodiscard]] glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch);
/// v = a - b; |v.x| and |v.z| < 0.01 -> heading 0, pitch 1.5393804; else heading = pi - affine::GetYAngle(v) (0 when
/// v.x^2 + v.z^2 <= 1e-6, else affine::ArcTanOctant(-v.z, v.x)) and pitch = affine::ArcTanOctant(sqrt(v.x^2 +
/// v.z^2), v.y)
void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch);
/// The drawn camera's pitch: HeadingAndPitchFromPoints from the drawn camera's place to its focus
/// (Camera::GetDrawnOrigin / GetDrawnFocus), the falling spell's path camera while it is drawn
[[nodiscard]] float DrawnPitch(const Camera& camera);
/// Whether a stands straight above or below b for the heading and pitch between them: v = a - b, |v.x| and |v.z| both
/// below 0.01, as doubles. The heading is then 0 and the pitch 1.5393804
[[nodiscard]] bool StraightAbove(const glm::vec3& a, const glm::vec3& b);
/// The point the follow mode aims at for a thing: a flock's position with half its leader's height; anything else its
/// map coords point (with `update`, the per-frame follow, the 3D object's position when it has one) and half its height
[[nodiscard]] glm::vec3 FollowPoint(const ThingInfo& thing, bool update);
/// FaceObject's focus: the map coords point and half the height, no flock nor 3D object
[[nodiscard]] glm::vec3 FacePoint(const ThingInfo& thing);
/// heading = facing (>= 8 pi: "Invalid heading", and on), less 2 pi while > 2 pi;
/// PointFromDistanceHeadingAndPitch(focus, distance, heading, 0.1)
[[nodiscard]] glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance);
/// A creature's facing, as a camera sent to face it reads it, from its body's yaw (0 along -z, a quarter turn along +x):
/// the yaw plus a full turn, then less 2.5, each step rounded to a float
[[nodiscard]] float CreatureFacing(float bodyYaw);

/// FOCUS_FOLLOW, SET_FOCUS_FOLLOW: the path dropped, the focus follows the thing if it is available, else nothing
void FocusFollow(entt::entity thing);
/// POSITION_FOLLOW, SET_POSITION_FOLLOW: with a thing the heading and pitch of the zoomers' destinations (position from
/// focus, HeadingAndPitchFromPoints) and the distance ThingViewingDistance; then with "behind" the heading is 0. The
/// path is not dropped
void PositionFollow(entt::entity thing);
/// FOCUS_AND_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW: as PositionFollow, with `distance` as the distance and the
/// heading kept even with "behind"
void FocusAndPositionFollow(entt::entity thing, float distance);
/// SET_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW: the zoomers placed at once (Zoomer::SetPosition) on the follow's
/// points (map coords, not the 3D object; the distance not clamped), the pitch clamped and kept, and the mode's seconds
/// at 2: the next frames follow at the factor's pace
void PlaceFollowNow();
/// CAMERA_PROPERTIES: distance, time factor, heading (radians) and behind (no zoom, already so)
void SetFollowProperties(float distance, float timeFactor, float heading, bool behind);
/// This module's part of the turn's camera checks (camera_checks::CheckModes): the dual cameras no longer valid are
/// deleted, the current one cleaned up first (the zoomers handed back when it drove the camera over the other modes),
/// with nothing restarted; and a followed thing that is no longer available is dropped. True when the current mode was
/// among those deleted
bool Validate();
/// After the turn's camera checks: the stack left empty with the player's own mode deleted by a full stack, and none of
/// the other systems' modes left, a new player's mode is made, the bottom from then on. True when that is the case
bool NewPlayersModeIfGone();
/// A mode popped off the stack (a follow let go of, a placed path taken off) left it empty, with the player's own mode
/// deleted by a full stack and none of the other systems' modes left: a new player's mode is made, the bottom from then
/// on, and starts from the camera as it is, with no flight back, its mode's seconds from 0. True when that is the case
bool StartNewPlayersModeIfGone();

/// SET/MOVE_CAMERA_TO_FACE_OBJECT: nullopt when the thing is not there ("no object to face")
struct FacePoints
{
	glm::vec3 position;
	glm::vec3 focus;
};
[[nodiscard]] std::optional<FacePoints> FaceObject(entt::entity thing, float distance);

/// With a path, its duration <= the ms run; else position and focus both within sqrt(0.001) of their zoomers'
/// destinations
[[nodiscard]] bool ScriptArrived();

/// One frame of the camera update for the zoomers: the mode's seconds, the mode's update (the path with the frame's
/// game ms, then the follow), then the six zoomers (camera seconds, at most 0.1), the disc of the world, and the FOV
/// zoomer (game seconds)
void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds);

/// The drawn camera: position and focus of the zoomers, the position nudged when they meet, and both lifted so that the
/// (nudged) position stays `k_GroundClearance` above `groundAt(x, z)`. A NaN in the zoomers gives the last good drawn
/// camera (first (1000, 0, 1000))
struct Drawn
{
	glm::vec3 origin;
	glm::vec3 focus;
};
[[nodiscard]] Drawn DrawnCamera(const std::function<float(float, float)>& groundAt);
/// What is drawn of any camera mode's camera: a position at its focus (within the arrival distance) is moved 1 back
/// along x and 1 up, then a position less than `k_GroundClearance` over `groundAt(x, z)` there is raised to it, the
/// focus raised with it
[[nodiscard]] Drawn HoldDrawn(Drawn drawn, const std::function<float(float, float)>& groundAt);
/// HoldDrawn's second half alone: a position less than `k_GroundClearance` over `groundAt(x, z)` there is raised to it,
/// the focus raised with it. While the falling spell runs, the camera's update holds the camera drawn the frame before
/// so, without the nudge
[[nodiscard]] Drawn HoldOverLand(Drawn drawn, const std::function<float(float, float)>& groundAt);

/// Game.cpp, once a frame instead of the player's Camera::Update when a script mode drives (Drives) and the citadel is
/// not shown (the original keeps the drawn camera there): Frame + DrawnCamera written to the camera. Always: the FOV to
/// the projection when it changed. Returns true when the script drove the camera (the player's model must not)
bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds);
/// Game.cpp, once a frame after the camera moved (script or player): SHAKE_CAMERA on the drawn camera only (every
/// mode), as Camera's draw offset; `lastDrawn` and `lastDrawnFocus` are the camera drawn the frame before
/// (Camera::GetDrawnOrigin / GetDrawnFocus). Outside the citadel the weather's point (Camera::SetWeatherPoint) is the
/// game camera held over the land, unshaken; while the falling spell runs, the moved test and the weather's point are
/// that drawn camera held over the land. Then the shakes' clock
void ApplyShake(Camera& camera, const glm::vec3& lastDrawn, const glm::vec3& lastDrawnFocus);

/// The debug camera mode (read before the shake): 2 draws the camera from the debug position towards the debug focus, 1
/// only turns the focus to it, 0 (or any other value) neither. The camera's zoomers are not touched: ApplyShake puts it
/// in the draw offset, so it holds only where the drawn camera is updated (not in the citadel). Written by
/// PLAY_JC_SPECIAL 1 / 2 (Locator::introSystem), and 0 when the intro is released and on a script reboot; the camera
/// editor's writers are debug only. (pending) the focus object is not ported
void SetDebugCameraMode(int32_t mode);
void SetDebugCameraPosition(const glm::vec3& position);
void SetDebugCameraFocus(const glm::vec3& focus);
[[nodiscard]] int32_t DebugCameraMode();
/// The debug camera on a drawn camera about to be shaken: mode 2 replaces the position and the focus, 1 the focus only.
/// The game camera's draw (ApplyShake) and the falling spell's path camera each run it before their shake
void ApplyDebugCamera(glm::vec3& position, glm::vec3& focus);

/// The creature interaction's camera, which cannot be left, takes the view: from now on it is on the stack above this
/// module's modes (none of them current) until Uncover; a dual camera started meanwhile goes on top of it. When one of them
/// moved the camera, the camera's zoomers are set to this module's as they are (value, speed, destination and time), so the
/// interaction goes on from them: the camera's zoomers are the same for every mode
void Cover(Zoomer3& origin, Zoomer3& focus);
/// The creature interaction's camera, the current mode, gave the view back: it comes off the stack, and this module's
/// top mode, if one is left, is current again (restarted as the original's: nothing for the script mode or a dual
/// camera) and goes on from the camera's zoomers as they are. With nothing left on the stack and the player's own mode
/// deleted by a full stack, a new player's mode is made (ModeStack::Popped::NewPlayersMode), which the caller starts
/// from the camera as it is. Popped::None when the interaction's camera is not current
camera_stack::ModeStack::Popped Uncover(const Zoomer3& origin, const Zoomer3& focus);
/// The creature interaction's camera is on the stack, current or under a dual camera
[[nodiscard]] bool Covered();
/// The creature interaction's camera is the current mode
[[nodiscard]] bool InteractionCurrent();
/// A new player's mode a script's end put over the creature interaction's camera is the current mode: the camera is the
/// player's, with the interaction's camera kept aside under it
[[nodiscard]] bool PlayerOverInteraction();
/// The end of a hold takes off that new player's mode: the mode under it is current again (nothing restarted for the
/// script mode, a dual camera or the interaction's camera) and goes on from the camera's zoomers as they are. False
/// when it is not the current mode
bool TakeOffPlayersModeOverInteraction(const Zoomer3& origin, const Zoomer3& focus);
/// The script mode, when it is the current one, taken off the camera by another (the end of a creature hold it kept
/// the view through): deleted, its path and its follows dropped, and the mode's seconds from 0; the camera's zoomers
/// set to this module's when it moved the camera, for the camera under it. Unlike End, the FOV is left as it is and the
/// script still thinks it has the camera. With nothing left on the stack and the player's own mode deleted by a full
/// stack, a new player's mode is made (ModeStack::Popped::NewPlayersMode), which the caller starts from the camera as it
/// is; a follow of Creature Mode's under it, current again, restarts with its time from 0. Popped::None when the
/// script mode is not the current one
camera_stack::ModeStack::Popped TakeOffScriptMode(Zoomer3& origin, Zoomer3& focus);

// ---- Dual camera ----------------------------------------------------------------------------------------------------

/// A mode of this module is the camera's current one: the script mode, or a dual camera (Drives, HAS_CAMERA_ARRIVED);
/// none while the creature interaction's camera is current
[[nodiscard]] bool HasMode();
/// The camera's current mode is the script mode (as the script's camera opcodes check): the top of the stack
[[nodiscard]] bool ScriptModeCurrent();
/// The camera's current mode is a dual camera
[[nodiscard]] bool DualCurrent();
/// START_DUAL_CAMERA: nothing when the current mode is one of the same a and b (the new one deletes itself); else
/// pushed on top of whatever mode is current, the script mode, another dual camera or the player's own (the mode's
/// seconds from 0). No mode nor citadel check. When no mode of this module was on the stack the zoomers are the player's
/// `origin` / `focus` as they are (value, speed, destination and time, as BeginFrom: the camera's zoomers are the same
/// for every mode)
void StartDual(entt::entity a, entt::entity b, const Zoomer3& origin, const Zoomer3& focus);
/// CREATE_DUAL_CAMERA_WITH_POINT: nothing when the current mode is a dual camera with the same a and the same point;
/// else a point camera with distance factor 1.2
void StartDualWithPoint(entt::entity a, const glm::vec3& point, const Zoomer3& origin, const Zoomer3& focus);
/// UPDATE_DUAL_CAMERA: a and b set on the current mode when it is a dual camera (a point camera becomes a two things
/// one). False when it is not
bool UpdateDual(entt::entity a, entt::entity b);
/// RELEASE_DUAL_CAMERA (and End): the current mode, when it is a dual camera, deleted and popped: the mode under it, the
/// script mode, another dual camera or the player's own, is current again (restarted: nothing for the script mode nor a
/// dual camera), and the mode's seconds = 0. With the player's mode under it, the zoomers go back to the player's camera
/// (as End: HandBack). False when the current mode is not a dual camera
bool ReleaseDual();

namespace detail
{
/// Test hook: when set, the things are read from here instead of the entities registry (nullptr: the registry again)
void SetThingReaderForTests(ThingReader reader);
} // namespace detail

} // namespace openblack::script_camera
