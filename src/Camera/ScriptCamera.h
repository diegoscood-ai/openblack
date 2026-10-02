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
#include <functional>
#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"

namespace openblack
{
class Camera;
struct CameraTrack;
class CameraWayRunner;
} // namespace openblack

/// The script camera of runblack.exe W120 (docs/bw1-notes/script-camera.md, dev\tmp_dis\camara\original.md): the part of
/// GCamera (0x1D8 bytes) the script moves and the script camera mode CameraModeScript (0x461180, vtable 0x8C7D5C).
///
/// - GCamera keeps the camera in Zoomers (LH3DLib, Common/Zoomer.h): focus x/y/z at +0x88/+0xB8/+0xE8, position at
///   +0x118/+0x148/+0x178 and the field of view (radians) at +0x1A8. GCamera::Update 0x441F80 moves them once a frame.
/// - START_CAMERA_CONTROL pushes a CameraModeScript (fn_00461140); while it lives the player's mode (CameraModeNew3)
///   does not move the camera and no other task gets it (CameraModeScript::CanExit 0x461B70). Releasing it
///   (fn_006ECD70) gives the player a new CameraModeNew3 that starts where the script left the camera.
/// - CameraModeScript is a CameraModeFollow (ctor 0x44B800 with no thing, time factor 0.2, "behind" 1, no zoom): it can
///   follow a thing with the focus (+0x4C, FOCUS_FOLLOW) and with the position (+0x08, POSITION_FOLLOW) once a frame
///   (CameraModeFollow::Update 0x44C160).
/// - openblack has no mode stack: the player's mode is Camera + DefaultWorldCameraModel, and this module stands for
///   GCamera's zoomers only while a script mode lives (the player's camera is not built on Zoomers, inferred to make
///   no visible difference: the hand-over copies the drawn camera both ways).
namespace openblack::script_camera
{

/// GCamera::SetCameraFov(fn_00443670, 0) in the ctor 0x441A78: fn_00443670 = fld [0x8C762C] = 1.2217305 (70 degrees)
constexpr float k_DefaultFov = 1.2217305f;
/// GCamera::Update 0x441FB0..0x441FC1: the camera's seconds of a frame are at most 0.1 ([0x8AB22C])
constexpr float k_MaxFrameSeconds = 0.1f;
/// CameraMode::Arrived 0x441700 and GCamera::Update 0x4421D5: [0x8AA3B0] = 0.001, a squared distance
constexpr float k_ArrivedDistanceSquared = 0.001f;
/// GCamera::Update 0x44222C..0x44232A: the world's disc around (2560, 0, 2560) ([0x8C7618]); a destination farther
/// than sqrt(1.225e7) = 3500 ([0x8C7614]) is pulled back to d / (|d| * 0.000285796 [0x8C7610]) in 3 s (0x40400000)
constexpr float k_DiscCentre = 2560.0f;
constexpr float k_DiscRadiusSquared = 1.225e7f;
constexpr float k_DiscScale = 0.000285795948f; // 0x3995D6E2
constexpr float k_DiscSeconds = 3.0f;
/// GCamera::Update 0x44242B..0x4424AB: the drawn camera stays 1 m ([0x8AA390]) above the landscape
constexpr float k_GroundClearance = 1.0f;
/// SET_CAMERA_LENS 0x6EE2E0 / MOVE_CAMERA_LENS 0x6EE280 / CAMERA_PROPERTIES 0x6EE025: degrees to radians [0x92B20C]
constexpr float k_DegreesToRadians = 0.0174532924f; // 0x3C8EFA35

/// CameraModeScript ctor 0x461189: CameraModeFollow's time factor +0x18 (0x3E4CCCCD)
constexpr float k_ScriptFollowTimeFactor = 0.2f;
/// CameraModeFollow::Update 0x44C16B..0x44C1A2: the follow distance +0x14 kept in 2 ([0x8C785C]) .. 1500 ([0x8C78E4])
constexpr float k_FollowMinDistance = 2.0f;
constexpr float k_FollowMaxDistance = 1500.0f;
/// 0x44C758 / 0x44BF7E: the follow pitch +0x10 is at least [0x8C78E0] = 0x3E7775FA
constexpr float k_FollowMinPitch = 0.241660982f;
/// 0x44C1A5..0x44C1DA: the seconds since the last mode change (GCamera +0x68) that end the slower start: 2 ([0x8C7874])
constexpr float k_FollowSettleSeconds = 2.0f;
/// GCamera::GetThingViewingDistance 0x441F20: GetHeight (vt +0x42C) x 8 ([0x8C7108])
constexpr float k_ThingViewingDistanceFactor = 8.0f;
/// 0x44BFD1 / 0x44C7B1: a MobileWallHug's GameAngle (+0x5C) x 2 x [0x8C78DC] (0x3AC90FDB, pi / 2048) - [0x8C78D8]
/// (0x3FC90FDB, pi / 2)
constexpr float k_FollowAngleScale = 0.00153398083f;
constexpr float k_HalfPi = 1.57079637f;
/// GetHeadingAndPitchFromPoints 0x4428D0: |dx| and |dz| below [0x8C7620] = 0.01 (a double) give heading 0 and pitch
/// 0x3FC50A6B
constexpr double k_VerticalEpsilon = 0.01;
constexpr float k_VerticalPitch = 1.53938043f;
/// fn_007FAA50 0x7FAA6B: dx^2 + dz^2 <= [0x9A2BAC] (0x358637BD) has no heading
constexpr float k_NoHeadingSquared = 9.99999997e-07f;
/// GetHeadingAndPitchFromPoints 0x44293F: [0x8C36A0] = 0x40490FDB
constexpr float k_Pi = 3.14159274f;
/// fn_006ED710: the facing heading's limit 8 pi ([0x942190] = 0x41C90FDB), 2 pi ([0x8AB210] = 0x40C90FDB) and the
/// pitch 0.1 (0x3DCCCCCD, 0x6ED7D0)
constexpr float k_FaceMaxHeading = 25.1327419f;
constexpr float k_TwoPi = 6.28318548f;
constexpr float k_FacePitch = 0.1f;

struct Vec3Zoomer
{
	std::array<Zoomer, 3> axis;

	void SetPosition(const glm::vec3& v);
	void SetDestination(const glm::vec3& v, float seconds); // SetDestinationWithSpeedAndTime(v.i, 0, seconds)
	void Update(float seconds);
	[[nodiscard]] glm::vec3 Value() const;
	[[nodiscard]] glm::vec3 Destination() const;
};

/// The state: GCamera's zoomers, the script mode and its camera path
struct State
{
	Vec3Zoomer position; ///< GCamera +0x118
	Vec3Zoomer focus;    ///< GCamera +0x88
	Zoomer fov;          ///< GCamera +0x1A8, radians
	/// GCamera +0x68: seconds since the last mode change (0 in SwitchToViewMode 0x441CD0, += the frame's camera seconds
	/// at 0x441FCD, 2 after fn_0044BB30 0x44C141)
	float modeSeconds = 0.0f;
	/// The CameraModeScript is alive (+0x48 = 1 from the ctor 0x461180; Delete 0x4611E0 sets 0)
	bool scriptMode = false;
	/// RUN_CAMERA_PATH: the track (+0x58, ScriptedCamera::Create 0x447060), null = none, and the Running of its
	/// position way (ScriptedCamera +4). The focus way's Running (+8) is never evaluated by UpdatePath 0x461AB0
	std::shared_ptr<const CameraTrack> track;
	std::unique_ptr<CameraWayRunner> positionRunner;
	/// +0x5C: the game ms run along the path (CameraModeScript::UpdatePath 0x461AB0)
	int32_t pathMs = 0;
	/// CameraModeFollow (+0x08..+0x1C) and CameraModeScript +0x4C. The pitch and the distance are not set by the ctors
	/// (0x44B800 calls Set(0), which writes neither): (inferido) never read before Set / fn_0044BA90 / CAMERA_PROPERTIES
	/// write them, so 0 here
	entt::entity positionThing = entt::null;     ///< +0x08
	entt::entity focusThing = entt::null;        ///< +0x4C (GetFocusThing 0x4611F0: this, else +0x08)
	float heading = 0.0f;                        ///< +0x0C, radians
	float pitch = 0.0f;                          ///< +0x10, radians
	float distance = 0.0f;                       ///< +0x14
	float timeFactor = k_ScriptFollowTimeFactor; ///< +0x18
	bool behind = true;                          ///< +0x1C: the heading is relative to a MobileWallHug's angle
	/// STORE_CAMERA_DETAILS 0x6EE330: GScript +0x54 (the drawn position) and +0x60 (the drawn focus); not the FOV
	glm::vec3 storedPosition {0.0f};
	glm::vec3 storedFocus {0.0f};
	/// The FOV last given to the renderer (radians). The config's FOV is only rewritten when the zoomer leaves it, so a
	/// player's own FOV stays until a script changes the lens
	float appliedFov = k_DefaultFov;

	State();
	~State();
};

/// The one GCamera of the running game
State& Get();

/// A new map (GCamera ctor 0x441A78: the FOV at 70 degrees at once, no script mode; GScript::Reset 0x6EB2D0)
void Reset();

/// fn_00461140 (START_CAMERA_CONTROL 0x6ECCBA): null when GCamera::CantExitCurrentMode 0x441B70, i.e. a script mode
/// is alive (CameraModeScript::CanExit 0x461B70 = +0x48 == 0; the player's CameraModeNew3 can always be left). The new
/// mode does not touch the zoomers (0x461180, 0x44B800): they start from the drawn camera `origin` / `focus`
/// (aproximado: the original's zoomers keep heading for the destination the player had). It follows nothing, with time
/// factor 0.2 and "behind" on (0x461185..0x461193), heading 0 (Set(0) 0x44BA7D), and the mode's seconds from 0
/// (SwitchToViewMode 0x44B947)
bool Begin(const glm::vec3& origin, const glm::vec3& focus);
/// fn_006ECD70 0x6ECDB1..0x6ECE48: the script mode deleted (vt+0x30) when it is the current one; the FOV back to 70
/// degrees in 0.5 s either way (0x6ECE35). Returns true when a script mode was deleted (the player's mode is then
/// created from where the camera is: CameraModeNew3 0x4572E0 -> Initialise 0x456640)
bool End();
[[nodiscard]] bool Active();
/// A script mode drives the camera: alive, and not the land's opening under mod game.skip-intro's "free start" (not
/// original: that opening keeps its mode but leaves the camera to the player)
[[nodiscard]] bool Drives();

/// CameraModeScript::SetCameraPosition 0x461370 / SetCameraFocus 0x4612B0: the path dropped (fn_00461A60), the follow
/// of that part dropped (Set(0) 0x44BA00, which with "behind" also sets the heading to 0 / SetCameraFocus(0) 0x4619B0)
/// and the zoomers set (Zoomer::SetPosition)
void SetPosition(const glm::vec3& position);
void SetFocus(const glm::vec3& focus);
/// CameraModeScript::MoveCameraPosition 0x4616F0 / MoveCameraFocus 0x461430: the same drops, then each zoomer heads for
/// the point with speed 0 in `seconds` (camera seconds: the wall clock)
void MovePosition(const glm::vec3& position, float seconds);
void MoveFocus(const glm::vec3& focus, float seconds);
/// GCamera::SetPositionAndFocus 0x4438C0: all six zoomers set (no matter the mode). (aproximado) The original does
/// nothing when both are already the drawn camera (it only clears IsMoving +0x74); here they are set anyway
void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus);
/// CameraModeScript::RunPath fn_00461A80: Reset 0x461A30 (the path and the focus follow +0x4C dropped; the position
/// follow +0x08 stays), then the track `number` of camera.edt, from its start
void RunPath(int32_t number);
/// GCamera::SetCameraFov 0x443680 (`fov` radians, `seconds` of game time)
void SetFov(float fov, float seconds);

// ---- CameraModeFollow (the script mode's follows) --------------------------------------------------------------------

/// What CameraModeFollow and fn_006ED710 read of a game thing (things are entt::entity, as CHLApi's objects)
struct ThingInfo
{
	/// Its MapCoords (+0x14) as a point: x, z / 6553.6 ([0x8AA3A4]), y = LH3DIsland::GetAltitude 0x803090 + the altitude
	/// above the land (+0x1C) (0x44BCD2..0x44BD03)
	glm::vec3 mapPoint {0.0f};
	/// GetHeight vt +0x42C
	float height = 0.0f;
	/// An Object with a Game3DObject (+0x40): its matrix's position (Game3DObject +0x38), which CameraModeFollow::Update
	/// uses instead of the MapCoords (0x44C513..0x44C539, 0x44C715..0x44C73B; not fn_0044BB30). For villagers and
	/// animals that matrix is the one drawn between turns (fn_0051AF00): openblack's DrawPosition
	std::optional<glm::vec3> drawnPoint;
	/// GameThing::IsAvailable vt +0x2C: GameThing 0x401810 = !(byte +0xA & 1); Villager 0x751D50 also 0 while its
	/// final state (vt +0xB04) is 14 DYING. Read only by FocusFollow (0x4619BD) and Validate (0x461273 / 0x44BB13)
	bool available = true;
	/// IsFlock vt +0x3EC; Flock::GetFlockPos 0x530570 as a point (the leader's MapCoords, else the flock's own) and the
	/// leader's GetHeight (the flock's +0x40 list node -> +8), none without a leader
	bool isFlock = false;
	glm::vec3 flockPoint {0.0f};
	std::optional<float> leaderHeight;
	/// dynamic_cast<MobileWallHug*>: its GameAngle +0x5C (2048 per circle)
	std::optional<uint16_t> wallHugAngle;
	/// GetFacingDirection vt +0x4EC (GameThingWithPos 0x4024B0 = 0; MobileWallHug 0x60C020 =
	/// ConvertGameAngleToScawenAngle(+0x5C); Creature 0x477EC0 = its LH3DCreature's angle + 2 pi - 2.5)
	float facingDirection = 0.0f;
};
/// nullopt: no such thing, or GameThing::IsAvailable (vt +0x2C) is 0
using ThingReader = std::function<std::optional<ThingInfo>(entt::entity)>;

/// The pure parts, exposed for the tests

/// 0x44C1A5..0x44C1E1: T = (modeSeconds > 2 ? 1 : (modeSeconds / 2) x (1 - 2) + 2) x timeFactor: twice the factor right
/// after a mode change, the factor itself from 2 s on
[[nodiscard]] float FollowSeconds(float modeSeconds, float timeFactor);
/// 0x44C16B..0x44C1A2: <= 2 -> 2; < 1500 -> itself; else 1500
[[nodiscard]] float ClampFollowDistance(float distance);
/// 0x44C758..0x44C778: <= 0.241661 -> 0.241661 (the caller stores it back in +0x10)
[[nodiscard]] float FollowPitch(float pitch);
/// 0x44C775..0x44C7C1: with "behind" and a MobileWallHug, heading - (float(2 a) x pi / 2048 - pi / 2); else heading
[[nodiscard]] float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle);
/// GCamera::GetThingViewingDistance 0x441F20: height x 8
[[nodiscard]] float ThingViewingDistance(float height);
/// GCamera::SetPointFromPointDistanceHeadingAndPitch 0x442810: p + d (sin h cos q, sin q, cos h cos q)
[[nodiscard]] glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch);
/// fn_007FA990(x, y): the arc tangent of y / x by octants (fpatan of the smaller over the larger), as atan2(y, x)
[[nodiscard]] float ArcTan2(float x, float y);
/// GCamera::GetHeadingAndPitchFromPoints 0x4428D0(a, b): v = a - b; |v.x| and |v.z| < 0.01 -> heading 0, pitch 1.5393804;
/// else heading = pi - fn_007FAA50(v) (0 when v.x^2 + v.z^2 <= 1e-6, else ArcTan2(-v.z, v.x)) and pitch =
/// ArcTan2(sqrt(v.x^2 + v.z^2), v.y)
void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch);
/// The point CameraModeFollow aims at for a thing: a flock's GetFlockPos with half its leader's height; anything else its
/// MapCoords point (with `update`, CameraModeFollow::Update, the Game3DObject's position when it has one) and half its
/// height (Update 0x44C454..0x44C555 / 0x44C648..0x44C754; fn_0044BB30 0x44BC5C..0x44BD19 / 0x44BDF2..0x44BECE)
[[nodiscard]] glm::vec3 FollowPoint(const ThingInfo& thing, bool update);
/// fn_006ED710's focus: the MapCoords point and half the height, no flock nor Game3DObject (0x6ED728..0x6ED76F)
[[nodiscard]] glm::vec3 FacePoint(const ThingInfo& thing);
/// fn_006ED710 0x6ED774..0x6ED7D9: heading = facing (>= 8 pi: "Invalid heading", and on), less 2 pi while > 2 pi;
/// SetPointFromPointDistanceHeadingAndPitch(focus, distance, heading, 0.1)
[[nodiscard]] glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance);

/// CameraModeScript::SetCameraFocus(thing) 0x4619B0 (FOCUS_FOLLOW, SET_FOCUS_FOLLOW): the path dropped, +0x50 = -1, the
/// focus follows the thing if it is available, else nothing (+0x4C = 0)
void FocusFollow(entt::entity thing);
/// CameraModeFollow::Set 0x44BA00 (POSITION_FOLLOW, SET_POSITION_FOLLOW): +0x08 = thing; with a thing the heading and
/// pitch of the zoomers' destinations (position from focus, GetHeadingAndPitchFromPoints) and the distance
/// GetThingViewingDistance; then with "behind" the heading is 0. The path is not dropped
void PositionFollow(entt::entity thing);
/// fn_0044BA90 (FOCUS_AND_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW): as Set, with `distance` as the distance and the
/// heading kept even with "behind"
void FocusAndPositionFollow(entt::entity thing, float distance);
/// fn_0044BB30 (SET_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW): the zoomers placed at once (Zoomer::SetPosition) on
/// the follow's points (MapCoords, not the Game3DObject; the distance not clamped), the pitch clamped and kept, and the
/// mode's seconds at 2 (0x44C141): the next frames follow at the factor's pace
void PlaceFollowNow();
/// CAMERA_PROPERTIES 0x6EE0C1..0x6EE0D9: +0x14 = distance, +0x18 = time factor, +0x0C = heading (radians), +0x1C = behind
/// (+0x20 = 0: no zoom, already so)
void SetFollowProperties(float distance, float timeFactor, float heading, bool behind);
/// CameraModeScript::Validate 0x461270 then CameraModeFollow::Validate 0x44BB10: a thing that is no longer available is
/// dropped (GCamera::Validate 0x441F50, once a turn from GGame::ProcessTurn 0x54E74E)
void Validate();

/// fn_006ED710(thing, d, &position, &focus) (SET/MOVE_CAMERA_TO_FACE_OBJECT): nullopt when the thing is not there
/// ("no object to face", 0xC0C218)
struct FacePoints
{
	glm::vec3 position;
	glm::vec3 focus;
};
[[nodiscard]] std::optional<FacePoints> FaceObject(entt::entity thing, float distance);

/// CameraModeScript::Arrived 0x461B40: with a path, its duration <= the ms run; else CameraMode::Arrived 0x441700:
/// position and focus both within sqrt(0.001) of their zoomers' destinations
[[nodiscard]] bool ScriptArrived();

/// One frame of GCamera::Update 0x441F80 for the zoomers: the mode's seconds (0x441FCD), the mode's Update (vt+0x08:
/// CameraModeScript::Update 0x461290 -> UpdatePath 0x461AB0 with the frame's game ms, then CameraModeFollow::Update
/// 0x44C160), then the six zoomers (camera seconds, at most 0.1), the disc of the world, and the FOV zoomer (game
/// seconds, 0x4424F6..0x4425C3)
void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds);

/// The camera GCamera::Update draws (0x4421D5..0x4424AB): position and focus of the zoomers, the position nudged when
/// they meet, and both lifted so that the (nudged) position stays `k_GroundClearance` above `groundAt(x, z)`. A NaN in
/// the zoomers gives the last good drawn camera (0x4420D9..0x4421D5, statics 0xC59B48 / 0xC59B38, first (1000, 0, 1000))
struct Drawn
{
	glm::vec3 origin;
	glm::vec3 focus;
};
[[nodiscard]] Drawn DrawnCamera(const std::function<float(float, float)>& groundAt);

/// Game.cpp, once a frame instead of the player's Camera::Update when a script mode drives (Drives) and the citadel is
/// not shown (GCamera::Update 0x442337..0x4423F4 keeps the drawn camera there): Frame + DrawnCamera written to the camera. Always: the FOV to the
/// projection when it changed. Returns true when the script drove the camera (the player's model must not)
bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds);

namespace detail
{
/// Test hook: when set, the things are read from here instead of the entities registry (nullptr: the registry again)
void SetThingReaderForTests(ThingReader reader);
} // namespace detail

} // namespace openblack::script_camera
