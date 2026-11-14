/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleCameraModel.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/ScreenPick.h"
#include "3D/TempleChallengeRoom.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "Common/X87Precision.h"
#include "Common/X87Vector.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "Help/HelpSystem.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/TempleHelpScripts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using input::BindableActionMap;

namespace
{
// The game casts the mouse's rays through the near plane, which the temple's cameras put this far out
constexpr float k_NearClip = 0.2f;
constexpr float k_FieldOfView = 90.0f;

/// Hits within this of the room's axis are on the pool, and doors only beyond it
constexpr float k_PoolRadius = 16.0f;

// The main room camera's views, looking down at the pool and up at the room
constexpr glm::vec3 k_LowOrigin {0.0f, 14.4f, 14.4f};
constexpr glm::vec3 k_HighOrigin {0.0f, 8.0f, 20.0f};
constexpr glm::vec3 k_LowFocus {0.0f, 0.0f, 4.0f};
constexpr glm::vec3 k_HighFocus {0.0f, 12.0f, 0.0f};
/// Where the main room's camera leans when the player takes it over
constexpr float k_StartLean = 0.7f;
/// The main room's lean aimed at as the player takes the camera back from a scroll it had not come all the way to
constexpr float k_LeanAfterLook = 1.0f;
constexpr float k_MaxLean = 1.5f;
/// How long the turn and lean take to catch up with the player
constexpr float k_OrbitEaseTime = 0.3f;
/// How far, in pixels, the mouse moves from where it was pressed before the press is a drag rather than a click
constexpr float k_DragDistance = 3.0f;
/// Holding the press on the pool's map: how far from the point pressed the camera goes, how fast it gets there, how far
/// it turns and tilts as the mouse crosses the screen, and between which tilts
constexpr float k_MapDistance = 6.0f;
constexpr float k_MapZoomSpeed = 2.0f;
constexpr float k_MapTurnPerScreen = 6.0f;
constexpr float k_MapTiltPerScreen = 2.0f;
constexpr float k_MapMinPitch = 0.62831855f;
constexpr float k_MapMaxPitch = 1.1780972f;

// The way through a door of the main room, facing the door ahead along x: up to the door, then through it
constexpr TempleCameraModel::Pose k_DoorApproach {{0.0f, 25.0f, 0.0f}, {15.0f, 20.0f, 0.0f}};
constexpr TempleCameraModel::Pose k_DoorEntry {{100.0f, 8.0f, 0.0f}, {120.0f, 10.0f, 0.0f}};
/// The door to the room of scrolls isn't one the camera goes through
constexpr uint32_t k_ScrollWall = 6;
/// The temple starts the doorway the camera walks through this far before the start of its swing, at this rate a
/// second: it waits over a second, then is open as the camera reaches it
constexpr float k_DoorOpenFrom = -0.5f;
constexpr float k_DoorOpenRate = 0.4f;
/// Walking back to the main room from another room, the doorway opens from the start of its swing, at this rate
constexpr float k_DoorBackOpenFrom = 0.0f;
constexpr float k_DoorBackOpenRate = 2.0f;
/// The room's camera turns any doorway left open round to close, this long into a room's path, at this rate
constexpr float k_IntroDoorCloseTime = 0.4f;
constexpr float k_IntroDoorCloseRate = 1.6f;
/// The main room's camera, easing in from another room's, looks for its start along its path's first three seconds,
/// a tenth of a second at a time, from a distance no point is beyond
constexpr float k_IntroSearchEnd = 3.0f;
constexpr float k_IntroSearchStep = 0.1f;
constexpr float k_NoIntroPointYet = 1e20f;

/// The camera's views of a room of pictures, as the game sets each room up: the camera and where it looks, low and
/// looking up at lean 0 and high and looking down at lean 1, about the room's centre, turned by the camera's yaw and
/// raised by its height
struct PictureRoom
{
	glm::vec3 centre;
	glm::vec3 lowOrigin;
	glm::vec3 highOrigin;
	glm::vec3 lowFocus;
	glm::vec3 highFocus;
	float maxHeight;
	/// The credits room turns about where its path ends instead
	bool aroundPathEnd;
	/// The door of the main room the room is behind, which the camera walks back through
	uint32_t door;
};
constexpr float k_PictureMinHeight = 9.0f;
constexpr PictureRoom k_ChallengeRoom {{87.0f, 0.0f, -87.0f},
                                       {0.0f, -7.0f, 8.0f},
                                       {0.0f, 7.0f, 10.0f},
                                       {0.0f, 6.0f, -4.0f},
                                       {0.0f, -6.0f, -4.0f},
                                       80.0f,
                                       false,
                                       7};
constexpr PictureRoom k_SaveGameRoom {{-87.0f, 0.0f, -87.0f},
                                      {0.0f, -7.0f, 8.0f},
                                      {0.0f, 7.0f, 10.0f},
                                      {0.0f, 6.0f, -4.0f},
                                      {0.0f, -6.0f, -4.0f},
                                      80.0f,
                                      false,
                                      5};
constexpr PictureRoom k_OptionsRoom {{87.0f, 0.0f, 87.0f},
                                     {0.0f, -7.0f, 20.0f},
                                     {0.0f, 7.0f, 20.0f},
                                     {0.0f, 6.0f, -4.0f},
                                     {0.0f, -6.0f, -4.0f},
                                     30.0f,
                                     false,
                                     1};
constexpr PictureRoom k_MultiRoom {{-87.0f, 0.0f, 87.0f},
                                   {0.0f, -7.0f, 20.0f},
                                   {0.0f, 7.0f, 20.0f},
                                   {0.0f, 6.0f, -4.0f},
                                   {0.0f, -6.0f, -4.0f},
                                   30.0f,
                                   false,
                                   3};
constexpr PictureRoom k_CreditsRoom {
    {-123.0f, 0.0f, 0.0f}, {0.0f, -0.1f, 0.1f}, {0.0f, 0.1f, 0.1f}, {0.0f, 0.5f, -1.0f}, {0.0f, -0.5f, -1.0f}, 30.0f, true, 4};
/// The rooms of pictures' walls, about their centres
constexpr float k_PictureRoomRadius = 37.0f;
/// The lean and height a room of pictures' camera starts at
constexpr float k_PictureStartLean = 0.6f;
constexpr float k_PictureStartHeight = 6.0f;
/// How long the turn, lean and height take to catch up with the player
constexpr float k_PictureYawEaseTime = 0.2f;
constexpr float k_PictureLeanEaseTime = 0.3f;
constexpr float k_PictureHeightEaseTime = 0.15f;
/// The lean follows the height, from this at the lowest the camera goes, rising by this for each unit up
constexpr float k_PictureLowLean = 0.2f;
constexpr float k_PictureLeanPerHeight = 0.0084507046f;
/// How fast the arrow keys lean and raise the camera
constexpr float k_PictureLeanSpeed = 1.85f;
constexpr float k_PictureRiseSpeed = 30.0f;
/// How fast a room of pictures turns by itself under a dialog, in radians a second, once it has sped up over a second
constexpr float k_PictureDialogTurnSpeed = 0.25f;
/// How long a room of pictures' camera takes to ease onto its aim at the full blend toward a close look
constexpr float k_PictureLookEaseTime = 0.5f;
/// The lean a room of pictures' camera heads for while it looks at something close up, and the one it leans back to
/// when the player takes it away
constexpr float k_PictureLookLean = 0.5f;
constexpr float k_PictureLeftLookLean = 0.2f;
/// The ring the rooms' doors back to the main room are on, about the main room's centre
constexpr float k_PictureDoorRadius = 87.0f;
/// The creature's room's ring, and the main room's door the creature's room is behind
constexpr float k_CreatureDoorRadius = 78.0f;
constexpr uint32_t k_CreatureDoor = 0;
// The way back to the main room through a room's door, facing the main room along x: halfway from the camera to the
// door, then through it
constexpr TempleCameraModel::Pose k_DoorBack {{40.0f, 16.0f, 0.0f}, {20.0f, 10.0f, 0.0f}};

// The creature's room: its walls, and its camera's look
constexpr TempleCameraModel::Cylinder k_CreatureRoomCylinder {{120.0f, 0.0f, -120.0f}, TempleCameraModel::k_RoomRadius, false};
/// How far ahead the look's point is
constexpr float k_LookDistance = 20.0f;
/// How far up and down the look turns
constexpr float k_LookMaxPitch = 1.4959966f;
/// How fast the arrow keys speed up the look's turn, and how quickly it slows
constexpr float k_LookAcceleration = 0.3f;
constexpr float k_LookSlowing = 10.0f;
/// How long the camera takes to catch up with the look
constexpr float k_LookEaseTime = 0.15f;

const PictureRoom* PictureRoomOf(TempleRoom room)
{
	switch (room)
	{
	case TempleRoom::Challenge:
		return &k_ChallengeRoom;
	case TempleRoom::SaveGame:
		return &k_SaveGameRoom;
	case TempleRoom::Options:
		return &k_OptionsRoom;
	case TempleRoom::Multi:
		return &k_MultiRoom;
	case TempleRoom::Credits:
		return &k_CreditsRoom;
	case TempleRoom::Main:
	case TempleRoom::CreatureCave:
	case TempleRoom::Unknown:
		break;
	}
	return nullptr;
}

/// The kind of thing the mouse is over: the pool, the floor or a wall
enum class HitKind : uint8_t
{
	Pool,
	Floor,
	Wall,
};

std::optional<HitKind> KindOf(const std::optional<TempleCameraModel::RoomHit>& hit)
{
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	if (hit->distance < k_PoolRadius)
	{
		return HitKind::Pool;
	}
	return hit->floor ? HitKind::Floor : HitKind::Wall;
}

/// Turns a point about the room's axis, as the main room's camera does by its matrix
glm::vec3 TurnAbout(glm::vec3 point, float angle)
{
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	return {point.x * cosine - point.z * sine, point.y, point.x * sine + point.z * cosine};
}

TempleCameraModel::Pose Mix(const TempleCameraModel::Pose& from, const TempleCameraModel::Pose& to, float amount)
{
	return {glm::mix(from.origin, to.origin, amount), glm::mix(from.focus, to.focus, amount)};
}

/// Where the creature's room's copy of the creature stands, and its size, while there is one
std::optional<std::pair<glm::vec3, float>> CaveCopy()
{
	std::optional<std::pair<glm::vec3, float>> found;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::CreatureCopy, const ecs::components::Transform>(
	        [&found](const ecs::components::CreatureCopy& copy, const ecs::components::Transform& transform) {
		        found = std::make_pair(transform.position, copy.size);
	        });
	return found;
}

/// The tattoo editor, while the game has one
ecs::systems::TattooEditorSystemInterface* TattooEditor()
{
	return Locator::tattooEditorSystem::has_value() ? &Locator::tattooEditorSystem::value() : nullptr;
}
} // namespace

TempleCameraModel::Cylinder TempleCameraModel::CylinderOf(Room room)
{
	if (const auto* picture = PictureRoomOf(room); picture != nullptr)
	{
		return {picture->centre, k_PictureRoomRadius};
	}
	if (room == Room::CreatureCave)
	{
		return k_CreatureRoomCylinder;
	}
	return {glm::vec3(0.0f), k_RoomRadius};
}

std::optional<TempleCameraModel::RoomHit> TempleCameraModel::RayCastRoom(glm::vec3 eye, glm::vec3 nearPoint)
{
	return RayCastRoom(eye, nearPoint, Cylinder {glm::vec3(0.0f), k_RoomRadius});
}

std::optional<TempleCameraModel::RoomHit> TempleCameraModel::RayCastRoom(glm::vec3 eye, glm::vec3 nearPoint,
                                                                         const Cylinder& room)
{
	// The game casts into a room about the room's centre: both points are taken about it first, then the way from the
	// eye to the near plane's point
	const auto origin = eye - room.centre;
	const auto direction = (nearPoint - room.centre) - origin;
	const float radius = room.radius;

	// The floor, when the ray points at it enough
	std::optional<float> floorDistance;
	if (room.floor && std::abs(direction.y) > 0.01f)
	{
		if (const float t = -(origin.y / direction.y); t > 0.0f)
		{
			floorDistance = t;
		}
	}

	// The walls, from outside them or within, each sum in the game's order
	const float a = (direction.z * direction.z) + (direction.x * direction.x);
	const float halfB = (direction.z * origin.z) + (origin.x * direction.x);
	const float b = halfB + halfB;
	const float c = ((origin.z * origin.z) + (origin.x * origin.x)) - (radius * radius);
	const float discriminant = (b * b) - ((c * a) * 4.0f);
	std::optional<float> wallDistance;
	if (discriminant > 0.0f)
	{
		const float root = std::sqrt(discriminant);
		const float t = (c >= 0.0f ? (-b - root) : (root - b)) / (a + a);
		if (t > 0.0f)
		{
			wallDistance = t;
		}
	}

	bool onFloor = false;
	float t = 0.0f;
	if (wallDistance.has_value() && (!floorDistance.has_value() || *wallDistance < *floorDistance))
	{
		t = *wallDistance;
	}
	else if (floorDistance.has_value())
	{
		t = *floorDistance;
		onFloor = true;
	}
	else
	{
		return std::nullopt;
	}

	// The wall's normal is back along the ray, made one long with its squares added x, z then y
	glm::vec3 normal {0.0f, 1.0f, 0.0f};
	if (!onFloor)
	{
		normal = -direction;
		if (normal != glm::vec3(0.0f))
		{
			normal = game_math::x87::Scaled<game_math::x87::SumOrder::Xzy>(normal);
		}
	}
	const glm::vec3 point {(direction.x * t) + origin.x, (t * direction.y) + origin.y, (direction.z * t) + origin.z};
	return RoomHit {
	    .distance = std::sqrt((point.z * point.z) + (point.x * point.x)),
	    .angle = game_math::x87::Store(game_math::x87::Atan2(point.z, point.x)),
	    .height = point.y + room.centre.y,
	    .floor = onFloor,
	    .point = point + room.centre,
	    .normal = normal,
	};
}

std::optional<uint32_t> TempleCameraModel::DoorAt(const RoomHit& hit, bool mainRoom)
{
	// A door is in the walls between these heights, in the middle of an eighth of the room
	constexpr float k_DoorBottom = 7.0f;
	constexpr float k_DoorTop = 23.0f;
	constexpr float k_DoorStart = 6.4581857f;
	constexpr float k_DoorWidth = 0.35f;
	constexpr float k_MainRoomDoorMargin = 0.1f;
	constexpr float k_DoorBackMargin = 0.13f;
	const float margin = mainRoom ? k_MainRoomDoorMargin : k_DoorBackMargin;
	if (hit.distance <= k_PoolRadius || hit.floor || hit.height <= k_DoorBottom || hit.height >= k_DoorTop)
	{
		return std::nullopt;
	}
	float within = hit.angle + k_DoorStart;
	uint32_t door = 0;
	while (within > glm::quarter_pi<float>())
	{
		within -= glm::quarter_pi<float>();
		++door;
	}
	if (within >= k_DoorWidth - margin || within <= margin)
	{
		return std::nullopt;
	}
	return door & 7u;
}

std::optional<TempleCameraModel::Room> TempleCameraModel::RoomBehindDoor(uint32_t door)
{
	switch (door)
	{
	case 0:
		return Room::CreatureCave;
	case 1:
		return Room::Options;
	case 3:
		return Room::Multi;
	case 4:
		return Room::Credits;
	case 5:
		return Room::SaveGame;
	case 7:
		return Room::Challenge;
	default:
		// The way out
		return std::nullopt;
	}
}

TempleCameraModel::Pose TempleCameraModel::OrbitPose(float yaw, float lean)
{
	return {TurnAbout(glm::mix(k_LowOrigin, k_HighOrigin, lean), yaw), TurnAbout(glm::mix(k_LowFocus, k_HighFocus, lean), yaw)};
}

float TempleCameraModel::DragLean(float mouseDown, float dt, float mouseX)
{
	constexpr float k_DragLeanRate = 0.06f;
	return ((mouseDown * dt) * k_DragLeanRate) * (1.0f - std::abs(mouseX));
}

TempleCameraModel::EdgeLean TempleCameraModel::EdgeLeanOf(float mouseY, bool overPool, float leanTarget)
{
	constexpr float k_Edge = 0.8f;
	constexpr float k_EdgeRampRate = 0.2f;
	constexpr float k_PoolRampRate = 0.1f;
	if (mouseY > k_Edge)
	{
		return {.speed = (mouseY - k_Edge) * -0.8f, .rampRate = k_EdgeRampRate};
	}
	if (mouseY < -k_Edge)
	{
		return {.speed = (-mouseY - k_Edge) * 0.8f, .rampRate = k_EdgeRampRate};
	}
	if (overPool)
	{
		return {.speed = leanTarget < 0.333f ? leanTarget * -0.3f : -0.1f, .rampRate = k_PoolRampRate};
	}
	return {};
}

float TempleCameraModel::LeanStep(float speed, float dt, float ramp)
{
	return speed * ((dt * 1.5f) * (1.0f - std::cos(ramp * glm::pi<float>())));
}

TempleCameraModel::Pose TempleCameraModel::MapLook(glm::vec3 focus, float yawAtPress, float pitchAtPress,
                                                   glm::vec2 movedOverScreen)
{
	const float yaw = yawAtPress - (movedOverScreen.x * k_MapTurnPerScreen);
	const float pitch = std::clamp(pitchAtPress + (movedOverScreen.y * k_MapTiltPerScreen), k_MapMinPitch, k_MapMaxPitch);
	const auto around = glm::vec3(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw));
	return Pose {focus + (around * k_MapDistance), focus};
}

TempleCameraModel::OrbitAims TempleCameraModel::OrbitKeys(OrbitAims aims, float dt, bool left, bool right, bool up, bool down)
{
	if (left)
	{
		aims.yaw += dt * 2.0f;
	}
	if (right)
	{
		aims.yaw -= dt * 2.0f;
	}
	if (up)
	{
		aims.lean += dt * 0.85f;
	}
	if (down)
	{
		aims.lean -= dt * 0.85f;
	}
	aims.lean = std::clamp(aims.lean, 0.0f, k_MaxLean);
	return aims;
}

float TempleCameraModel::CaveZoomStep(float zoom, float aim, float dt)
{
	if (zoom < aim)
	{
		return std::min(zoom + dt, aim);
	}
	if (zoom > aim)
	{
		return std::max(zoom - 2.0f * dt, aim);
	}
	return zoom;
}

float TempleCameraModel::CaveLookEaseTime(float zoom)
{
	return (zoom * 0.1f) + k_LookEaseTime;
}

glm::vec2 TempleCameraModel::CaveDragLook(glm::vec2 lookAtPress, glm::vec2 pressMouse, glm::vec2 mouse, float width)
{
	const float across = 2.0f * std::tan(glm::radians(k_FieldOfView) * 0.5f);
	const auto moved = (pressMouse - mouse) / width;
	return {(across * moved.x) + lookAtPress.x, (across * moved.y) + lookAtPress.y};
}

TempleCameraModel::CaveLook TempleCameraModel::CaveKeysStep(CaveLook look, float dt, bool left, bool right, bool up, bool down)
{
	if (left)
	{
		look.speed.x -= dt * k_LookAcceleration;
	}
	if (right)
	{
		look.speed.x += dt * k_LookAcceleration;
	}
	if (up)
	{
		look.speed.y -= dt * k_LookAcceleration;
	}
	if (down)
	{
		look.speed.y += dt * k_LookAcceleration;
	}
	look.look += look.speed;
	look.atPress += look.speed;
	// It never looks straight up or down
	look.look.y = std::clamp(look.look.y, -k_LookMaxPitch, k_LookMaxPitch);
	look.atPress.y = std::clamp(look.atPress.y, -k_LookMaxPitch, k_LookMaxPitch);
	// both turns are brought back by whole turns
	look.atPress.x = WrapTurn(look.atPress.x);
	look.look.x = WrapTurn(look.look.x);
	look.speed *= std::exp(-k_LookSlowing * dt);
	return look;
}

float TempleCameraModel::WrapTurn(float turn)
{
	while (turn < 0.0f)
	{
		turn += glm::two_pi<float>();
	}
	while (glm::two_pi<float>() < turn)
	{
		turn -= glm::two_pi<float>();
	}
	return turn;
}

TempleCameraModel::DoorWalk TempleCameraModel::DoorWalkStep(DoorStage stage, float time, float dt, bool newPress)
{
	constexpr float k_HeadThroughAfter = 1.5f;
	constexpr float k_ThroughAfter = 5.0f;
	DoorWalk walk {.stage = newPress ? DoorStage::Skipped : stage, .time = time};
	if (walk.stage == DoorStage::Approaching && walk.time > k_HeadThroughAfter)
	{
		walk.stage = DoorStage::Entering;
		walk.headsThrough = true;
	}
	walk.time += dt;
	// halfway along, with the doorway near open, the camera is into the room
	if (walk.time > k_ThroughAfter)
	{
		walk.stage = DoorStage::Through;
	}
	return walk;
}

glm::vec3 TempleCameraModel::PointAtViewDepth(glm::vec3 eye, glm::vec3 direction, glm::vec3 forward, float depth)
{
	const float along = glm::dot(direction, forward);
	return along > 0.0f ? eye + direction * (depth / along) : eye;
}

glm::vec3 TempleCameraModel::NextRoomCursorPoint(glm::vec3 kept, std::optional<glm::vec3> meshPick,
                                                 std::optional<glm::vec3> floor, std::optional<glm::vec3> creature)
{
	auto point = meshPick.value_or(kept);
	if (floor.has_value())
	{
		point = *floor;
	}
	return creature.value_or(point);
}

float TempleCameraModel::MainRoomEaseTime(float mapBlend)
{
	constexpr float k_MapBlendEase = 0.2f;
	return mapBlend > 0.0f ? (mapBlend * k_MapBlendEase) + 0.0f : 0.0f;
}

bool TempleCameraModel::CaveWalksBack(bool newPress, bool pressed, int32_t door, int32_t lastDoor, float pointFromMiddle,
                                      float pointFromCamera)
{
	constexpr float k_NearTheDoorSide = 79.0f;
	constexpr float k_NearTheCamera = 25.0f;
	if (!(pointFromMiddle < k_NearTheDoorSide) || !pressed)
	{
		return false;
	}
	if (newPress && door == static_cast<int32_t>(k_CreatureDoor))
	{
		return true;
	}
	return door == lastDoor && pointFromCamera < k_NearTheCamera;
}

TempleCameraModel::Pose TempleCameraModel::TurnToDoor(const Pose& pose, int32_t door)
{
	const float angle = static_cast<float>(door) * glm::quarter_pi<float>();
	return {TurnAbout(pose.origin, angle), TurnAbout(pose.focus, angle)};
}

bool TempleCameraModel::IsPictureRoom(Room room)
{
	return PictureRoomOf(room) != nullptr;
}

TempleCameraModel::Pose TempleCameraModel::PictureOrbitPose(Room room, float yaw, float lean, float height, glm::vec3 pathEnd)
{
	// Where a room of pictures' camera is and looks, at a turn, lean and height
	const auto& picture = *PictureRoomOf(room);
	const auto centre = picture.aroundPathEnd ? glm::vec3(pathEnd.x, 0.0f, pathEnd.z) : picture.centre;
	const glm::vec3 raise {0.0f, height, 0.0f};
	return {
	    TurnAbout(glm::mix(picture.lowOrigin, picture.highOrigin, lean), yaw) + centre + raise,
	    TurnAbout(glm::mix(picture.lowFocus, picture.highFocus, lean), yaw) + centre + raise,
	};
}

float TempleCameraModel::PictureStartYaw(const Pose& pathEnd)
{
	// A quarter turn on from the way the path ends looking, which the orbit's views look along unturned
	const auto along = pathEnd.focus - pathEnd.origin;
	return std::atan2(along.z, along.x) + glm::half_pi<float>();
}

float TempleCameraModel::CloseLookAlong(float blend)
{
	// None at no blend, an unordered one counting as none, and all of the way past a full one
	if (!(blend > 0.0f))
	{
		return 0.0f;
	}
	blend = std::min(blend, 1.0f);
	// The cosine is taken wider than a float, and only its difference from one is rounded to a float
	const float angle = blend * glm::pi<float>();
	return static_cast<float>(1.0 - std::cos(static_cast<double>(angle))) * 0.5f;
}

TempleCameraModel::Pose TempleCameraModel::TowardCloseLook(const Pose& room, const Pose& look, float blend)
{
	if (!(blend > 0.0f))
	{
		return room;
	}
	const float along = CloseLookAlong(blend);
	return {room.origin + (look.origin - room.origin) * along, room.focus + (look.focus - room.focus) * along};
}

float TempleCameraModel::PictureEaseTime(float blend)
{
	return k_PictureLookEaseTime * blend;
}

TempleCameraModel::Pose TempleCameraModel::LookPose(glm::vec3 from, float heading, float pitch)
{
	// The point at the look's distance, heading and pitch from the camera, mirrored through the camera
	const glm::vec3 behind {std::sin(heading) * std::cos(pitch), std::sin(pitch), std::cos(heading) * std::cos(pitch)};
	return {from, from - behind * k_LookDistance};
}

glm::vec2 TempleCameraModel::HeadingAndPitch(const Pose& look)
{
	const auto behind = look.origin - look.focus;
	const float across = std::sqrt(behind.x * behind.x + behind.z * behind.z);
	if (across < 1e-4f)
	{
		// Straight up or down
		return {0.0f, 1.5393804f};
	}
	return {std::atan2(behind.x, behind.z), std::atan2(behind.y, across)};
}

TempleCameraModel::TempleCameraModel(Paths paths, Room room)
    : _paths(std::move(paths))
{
	_lean.SetPosition(0.5f);
	// Each room of pictures' camera starts at its lean and height
	for (size_t i = 0; i < k_RoomCount; ++i)
	{
		const auto pictureRoom = static_cast<Room>(i);
		if (IsPictureRoom(pictureRoom))
		{
			ResetPictureCamera(pictureRoom);
			auto& picture = _pictures.at(i);
			picture.yaw.SetPosition(picture.yawTarget);
			picture.lean.SetPosition(picture.leanTarget);
			picture.height.SetPosition(picture.heightTarget);
		}
	}

	// The room's camera starts at the start of its path
	if (const auto& path = _paths.at(static_cast<size_t>(room)); path)
	{
		const auto start = path->SampleAt(std::chrono::milliseconds::zero());
		_origin.SetPosition(start.position);
		_focus.SetPosition(start.focus);
		_target = {start.position, start.focus};
	}
}

TempleCameraModel::~TempleCameraModel()
{
	// Leaving the temple closes the editor its creature's room opened
	CloseTattooEditor();
}

TempleCameraModel::Room TempleCameraModel::GetRoom() const
{
	return Locator::temple::value().GetCurrentRoom();
}

float TempleCameraModel::NearestIntroTime(const CameraPath& path, glm::vec3 camera)
{
	float nearestTime = 0.0f;
	float nearest = k_NoIntroPointYet;
	// Every tenth of a second, counted up in floats, while under three seconds
	for (float time = 0.0f; time < k_IntroSearchEnd; time += k_IntroSearchStep)
	{
		const auto key = path.SampleAt(std::chrono::milliseconds(static_cast<int64_t>(time * 1000.0f)));
		const auto d = key.position - camera;
		// Summed in the game's order, the height and depth first
		const float squared = (d.z * d.z + d.y * d.y) + d.x * d.x;
		if (squared < nearest)
		{
			nearest = squared;
			nearestTime = time;
		}
	}
	return nearestTime;
}

void TempleCameraModel::StartIntro(Room room, bool blendFromCurrent)
{
	// The room's camera starts its path in
	Locator::temple::value().SetCurrentRoom(room);
	// Each room has a camera of its own, so a scroll looked at in the room left is left with it, along with its text in
	// front of it, which only that room draws
	_lookingAtSubMesh = false;
	_subMeshZoom = 0.0f;
	_subMeshLook.reset();
	ResetCaveTargets();
	_state = State::Intro;
	_nextState = State::Intro;
	_introTime = 0.0f;
	_blendTime = 0.0f;
	_blendFromPrevious = blendFromCurrent;
	if (blendFromCurrent)
	{
		_previousOrigin = _origin;
		_previousFocus = _focus;
		// Easing in from another room, the main room's camera takes up its path where it passes nearest the camera
		if (const auto& path = _paths.at(static_cast<size_t>(room)); room == Room::Main && path)
		{
			_introTime = NearestIntroTime(*path, _origin.GetCurrentValue());
		}
	}
	else
	{
		// Coming into the temple, the main room's camera starts with no door lit
		_highlightedDoor.reset();
	}
	// (openblack, a readout at debug) how the room's camera comes in: cut to its path, or eased from where the camera
	// was, and the time its path starts at
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		const glm::vec3 from = _origin.GetCurrentValue();
		log->debug("Temple camera: {} comes in along its path from {:.1f} s, {} ({:.1f}, {:.1f}, {:.1f})", TempleRoomName(room),
		           _introTime, blendFromCurrent ? "eased from the camera at" : "cut with no blend from the camera at", from.x,
		           from.y, from.z);
	}
}

void TempleCameraModel::GoToRoom(Room room)
{
	// Going to the room the player is in does nothing, not even shut the doorway
	if (!ChangesRoom(GetRoom(), room))
	{
		return;
	}
	Locator::temple::value().GetDoors().FastClose();
	// Going straight to a room skips its path: the room's camera starts over, takes over from the camera, and
	// settles where the player has it
	if (IsPictureRoom(room))
	{
		ResetPictureCamera(room);
	}
	StartIntro(room, true);
	_nextState = ControlOf(room);
	ChangeState();
	const Input still {.button = 0,
	                   .mouse = _lastMouse,
	                   .mouseScreen = glm::vec2(0.0f),
	                   .hit = std::nullopt,
	                   .door = std::nullopt,
	                   .doorBack = std::nullopt};
	Step(1.0f, still);
	Step(1.0f, still);
}

bool TempleCameraModel::GoThroughDoorTo(Room room)
{
	if (_state != State::Orbit)
	{
		return false;
	}
	for (uint32_t door = 0; door < 8; ++door)
	{
		if (RoomBehindDoor(door) == room)
		{
			_doorStart = TurnToDoor(k_DoorApproach, door);
			_doorEnd = TurnToDoor(k_DoorEntry, door);
			_doorRoom = room;
			_nextState = State::ThroughDoor;
			ChangeState();
			return true;
		}
	}
	return false;
}

void TempleCameraModel::HandleActions(std::chrono::microseconds /*dt*/)
{
	// The keys for the rooms are handled by the game, as they work outside the temple too
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> TempleCameraModel::Update(std::chrono::microseconds dt,
                                                                                    const Camera& camera)
{
	const float seconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
	const auto& actions = Locator::gameActionSystem::value();

	Input input {};
	// A double click comes to the temple's camera once, as its second press
	if (Locator::gameActionSystem::value().TakeDoubleClick())
	{
		input.button = 2;
	}
	else if (actions.GetAny(BindableActionMap::MOVE, BindableActionMap::ACTION))
	{
		input.button = 1;
	}
	input.mouse = glm::vec2(actions.GetMousePosition());
	// While a script holds the cinema bars, as when the advisors introduce a room, the rooms take no press
	const auto* helpSystem = help::Get();
	const auto pressed = input.button;
	input.button =
	    PressUnderCinemaBars(_pressWaitsForRelease, input.button, helpSystem != nullptr && helpSystem->GetWideScreen() != 0);
	const auto underBars = input.button;
	// A press on one of the room's controls goes to the control, which the room's camera then doesn't see
	if (Locator::temple::value().HoldControl(input.button != 0, input.mouse.y))
	{
		input.button = 0;
	}
	// OPENBLACK_TEMPLE_CAMERA_TRACE=1: each frame a button is down, what the cinema bars and the room's controls left of it
	static const bool k_PressTrace = std::getenv("OPENBLACK_TEMPLE_CAMERA_TRACE") != nullptr;
	if (debug_env::TraceLogs(k_PressTrace) && pressed != 0)
	{
		spdlog::get("game")->info(
		    "Temple press: button {} at ({:.0f}, {:.0f}), {} under the cinema bars, {} past the room's controls",
		    static_cast<int>(pressed), input.mouse.x, input.mouse.y, static_cast<int>(underBars),
		    static_cast<int>(input.button));
	}
	if (Locator::windowing::has_value())
	{
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		input.mouseScreen = input.mouse / (size * 0.5f) - 1.0f;
		input.screen = size;

		// The game casts from the camera through the mouse on its near plane
		const auto rayDirection = camera.RayFromEye(input.mouse / size).direction;
		const auto forward = camera.GetForward();
		if (const float along = glm::dot(rayDirection, forward); along > 0.0f)
		{
			const auto nearPoint = camera.GetOrigin() + rayDirection * (k_NearClip / along);
			_cursorRay = Pose {camera.GetOrigin(), camera.GetOrigin() + rayDirection};
			input.eye = camera.GetOrigin();
			input.hit = RayCastRoom(camera.GetOrigin(), nearPoint, CylinderOf(GetRoom()));
			// The doors back to the main room are on a ring about its centre
			if (GetRoom() != Room::Main)
			{
				const float ringRadius = GetRoom() == Room::CreatureCave ? k_CreatureDoorRadius : k_PictureDoorRadius;
				if (const auto ring = RayCastRoom(camera.GetOrigin(), nearPoint,
				                                  Cylinder {glm::vec3(0.0f), ringRadius, CylinderOf(GetRoom()).floor});
				    ring.has_value())
				{
					input.doorBack = DoorAt(*ring, false);
				}
			}
		}
	}
	if (GetRoom() == Room::CreatureCave && Locator::windowing::has_value())
	{
		// The creature's room finds its targets where their points are projected onto the screen
		using namespace CreatureCaveTargets;
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		// The creature is a target only while the player has one. It is found by its body under the cursor, as it is
		// drawn; its point of the room's mesh, which has no reach, takes only its very pixel
		const bool hasCreature =
		    Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().GetCreature().has_value();
		if (hasCreature && size.x > 0.0f && size.y > 0.0f)
		{
			input.overCaveCreature = Locator::creatureCaveSystem::value().CursorOverCaveCreature({
			    .worldToClip = camera.GetViewProjectionMatrix(),
			    .resolution = size,
			    .near = k_NearClip,
			    .xScale = camera.GetProjectionMatrix()[0][0],
			    .camera = camera.GetOrigin(),
			    .cursor = input.mouse,
			});
		}
		for (size_t i = 0; i < k_Count; ++i)
		{
			const auto target = static_cast<Target>(i);
			if (target == Target::Creature && !hasCreature)
			{
				continue;
			}
			const auto place = target == Target::Exit ? std::optional<glm::vec3>(k_ExitPlace)
			                                          : CaveMeshPoint(k_FirstLookPoint + static_cast<uint32_t>(i));
			glm::vec3 screen;
			if (place.has_value() && camera.ProjectWorldToScreen(*place, glm::vec4(0.0f, 0.0f, size), screen))
			{
				input.caveTargets.at(i) = glm::vec2(screen);
			}
		}
	}
	if (input.hit.has_value())
	{
		input.door = DoorAt(*input.hit);
	}
	// This frame's, none where the cursor meets nothing of the room's cylinder
	_cursorHit = input.hit;

	// The temple moves its doorway on the walk through a door as the camera stood at the frame's start, then the
	// camera steps, then the doorway swings
	if (const auto call = DoorwayAtFrameStart(_state == State::ThroughDoor, _doorStage, GetRoom(), _doorRoom); call.has_value())
	{
		auto& doors = Locator::temple::value().GetDoors();
		if (call->kind == DoorwayCall::Kind::FastClose)
		{
			doors.FastClose();
		}
		else
		{
			doors.Open(call->leaf, call->from, call->rate);
		}
	}
	Step(seconds, input);
	FollowTemple();
	Locator::temple::value().GetDoors().Update(seconds);

	auto origin = _origin.GetCurrentValue();
	auto focus = _focus.GetCurrentValue();
	// In the main and the creature's rooms, the camera goes over to the scroll it looks at along a cosine. The rooms of
	// pictures aim their camera there instead, which then eases onto its aim (Step).
	if (_subMeshLook.has_value() && _subMeshZoom > 0.0f && !IsPictureRoom(GetRoom()))
	{
		const float along = (1.0f - std::cos(_subMeshZoom * glm::pi<float>())) * 0.5f;
		origin = glm::mix(origin, _subMeshLook->origin, along);
		focus = glm::mix(focus, _subMeshLook->focus, along);
	}
	// In the creature's room, it goes over to the target zoomed to in a straight line
	if (_caveZoom > 0.0f)
	{
		origin = glm::mix(origin, _caveLook.origin, _caveZoom);
		focus = glm::mix(focus, _caveLook.focus, _caveZoom);
	}
	return CameraInterpolationUpdateInfo {origin, focus, std::chrono::microseconds::zero()};
}

void TempleCameraModel::Step(float dt, const Input& input)
{
	// The room's camera aims, then the camera eases after its aim
	_originTime = 0.5f;
	_focusTime = 0.5f;
	_easeToTarget = true;
	if (input.hit.has_value())
	{
		_lastHitAngle = input.hit->angle;
	}
	if (input.button != 0 && !_wasPressed)
	{
		_pressHit = input.hit;
		_lastMouse = input.mouse;
		_pressMouse = input.mouse;
	}
	if (IsPictureRoom(GetRoom()))
	{
		UpdatePictureCamera(dt);
	}

	// Looking at a scroll, the room's controls rest, and a press elsewhere or the arrow keys send the camera back, as in
	// the creature's room
	const bool inControl = _state == ControlOf(GetRoom());
	// A room of pictures' camera, with the player in control, looks this whole frame, the one it is taken away in too
	const bool pictureLook = _lookingAtSubMesh && inControl && IsPictureRoom(GetRoom());
	bool leftByPlayer = false;
	const bool wasLooking = _lookingAtSubMesh;
	if (_lookingAtSubMesh)
	{
		const auto& actions = Locator::gameActionSystem::value();
		const bool turning = actions.GetAny(BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_RIGHT,
		                                    BindableActionMap::MOVE_FORWARDS, BindableActionMap::MOVE_BACKWARDS);
		// the press the rooms' cameras are given is held from the press to its release, so a button held down takes
		// the camera back as a new press does
		leftByPlayer = input.button != 0 || turning;
		if (leftByPlayer || !inControl)
		{
			_lookingAtSubMesh = false;
			// the main room's camera given back from a scroll leans all the way up when the player takes it back; then,
			// whoever ended the look, a look that had come all the way to the scroll (this frame's step in included)
			// starts the player facing the scroll again, leaning a little up, as the room's camera does as it goes back
			// to its own state; one that had not keeps its turn
			if (GetRoom() == Room::Main && _state == State::Orbit)
			{
				if (leftByPlayer)
				{
					_leanTarget = k_LeanAfterLook;
				}
				if (MainRoomStartsAfreshAfterLook(temple_challenge_room::BlendStep(_subMeshZoom, true, dt)))
				{
					StartOrbitAfresh();
				}
			}
		}
		// The player taking the camera away from the scroll stops the scroll's help
		if (leftByPlayer)
		{
			temple_help::GameScripts scripts;
			temple_help::LeaveScroll(scripts, GetRoom());
		}
	}
	// the blend toward what it looks at, which a click on a challenge's picture also reads. A room of pictures' camera
	// aims with the blend it had before rising.
	if (pictureLook)
	{
		UpdatePictureLook(dt, leftByPlayer);
	}
	else
	{
		// the main room's look still takes its step in on the frame it is left in, as the room's camera blends before
		// it goes back to its own state
		_subMeshZoom = temple_challenge_room::BlendStep(
		    _subMeshZoom, LookBlendStepsIn(_lookingAtSubMesh, wasLooking, GetRoom() == Room::Main), dt);
	}
	if (!_lookingAtSubMesh && _subMeshZoom <= 0.0f)
	{
		_subMeshLook.reset();
	}
	// the main room's camera looking at a scroll aims from the room's view at its turn's and lean's aims, where the
	// player last sent them, and eases onto that aim at once
	if (_lookingAtSubMesh && GetRoom() == Room::Main)
	{
		if (_state == State::Orbit)
		{
			_target = OrbitPose(_yawTarget, _leanTarget);
		}
		_originTime = 0.0f;
		_focusTime = 0.0f;
	}
	_hoveredDoor = _state == State::Orbit ? input.door : std::nullopt;
	_highlightedDoor = NextHighlightedDoor(GetRoom(), _highlightedDoor, input.door);
	_overPool = _state == State::Orbit && KindOf(input.hit) == HitKind::Pool;
	_pressingPool = _state == State::Orbit && input.button != 0 && KindOf(_pressHit) == HitKind::Pool;
	_overWayBack = false;
	if (input.doorBack.has_value() && _state == ControlOf(GetRoom()))
	{
		const auto* picture = PictureRoomOf(GetRoom());
		_overWayBack = GetRoom() == Room::CreatureCave ? *input.doorBack == k_CreatureDoor
		                                               : picture != nullptr && *input.doorBack == picture->door;
	}

	if (GetRoom() == Room::CreatureCave)
	{
		UpdateCaveTargets(dt, input);
	}

	if (!_lookingAtSubMesh && !pictureLook)
	{
		switch (_state)
		{
		case State::Intro:
			UpdateIntro(dt, input);
			break;
		case State::Orbit:
			UpdateOrbit(dt, input);
			break;
		case State::Look:
			UpdateLook(dt, input);
			break;
		case State::PictureOrbit:
			UpdatePictureOrbit(dt, input);
			break;
		case State::ThroughDoor:
			UpdateThroughDoor(dt, input);
			break;
		}
	}
	ChangeState();

	if (_easeToTarget)
	{
		_origin.SetDestinationWithTime(_target.origin, _originTime);
		_focus.SetDestinationWithTime(_target.focus, _focusTime);
	}
	_origin.Update(dt);
	_focus.Update(dt);
	_wasPressed = input.button != 0;
}

void TempleCameraModel::UpdateIntro(float dt, const Input& input)
{
	const auto control = ControlOf(GetRoom());
	_introTime += dt;
	_blendTime += dt;
	const auto& path = _paths.at(static_cast<size_t>(GetRoom()));
	if (!path)
	{
		_nextState = control;
		return;
	}

	const auto key = path->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(_introTime * 1000.0f)));
	_target = {key.position, key.focus};
	const float duration = static_cast<float>(path->GetDuration().count()) * 0.001f;
	// The player takes over half a second after the path ends, or with a click, which also covers the screen with the
	// temple's fade at once
	if (_introTime > duration + 0.5f)
	{
		_nextState = control;
	}
	if (input.button != 0 && !_wasPressed)
	{
		_nextState = control;
		Locator::temple::value().CoverWithFade();
	}

	// The path eases into where the player takes over over its last second
	if (const float intoControl = _introTime - (duration - 1.0f); intoControl > 0.0f)
	{
		_target = Mix(_target, ControlPose(GetRoom()), std::min(intoControl, 1.0f));
	}
	// Coming through a door, the camera eases off the way it was going over the first two seconds
	if (const float fromPrevious = (2.0f - _blendTime) * 0.5f; fromPrevious > 0.0f && _blendFromPrevious)
	{
		_previousOrigin.Update(dt);
		_previousFocus.Update(dt);
		const float amount = std::min(fromPrevious, 1.0f);
		_target = Mix(_target, {_previousOrigin.GetCurrentValue(), _previousFocus.GetCurrentValue()}, amount);
		_originTime *= 1.0f - amount;
		_focusTime *= 1.0f - amount;
	}
}

void TempleCameraModel::UpdateOrbit(float dt, const Input& input)
{
	const auto& actions = Locator::gameActionSystem::value();
	const auto pressKind = KindOf(_pressHit);

	// Dragging the floor or the walls turns the room to keep them under the mouse, and up and down leans
	if (input.button != 0 && (pressKind == HitKind::Floor || pressKind == HitKind::Wall))
	{
		float turn = _pressHit->angle - _lastHitAngle;
		if (turn < -glm::pi<float>())
		{
			turn += glm::two_pi<float>();
		}
		if (turn > glm::pi<float>())
		{
			turn -= glm::two_pi<float>();
		}
		if (_wasPressed)
		{
			_yawTarget = _yaw.value + turn;
			_leanTarget += DragLean(input.mouse.y - _lastMouse.y, dt, input.mouseScreen.x);
		}
		_lastMouse = input.mouse;
	}
	// Holding the press on the pool draws the camera to the point pressed on the island's map, 6 units from it, and
	// dragging turns about it and tilts. A double click there leaves the temple for that place.
	if (input.button != 0 && pressKind == HitKind::Pool && _pressHit.has_value())
	{
		if (!_wasPressed)
		{
			// The turn and tilt from the point to where the camera is
			_mapFocus = _pressHit->point;
			const auto away = _target.origin - _mapFocus;
			const auto direction = glm::length(away) > 0.0f ? glm::normalize(away) : away;
			_mapYaw = std::atan2(direction.z, direction.x);
			_mapPitch = std::atan2(direction.y, glm::length(glm::vec2(direction.x, direction.z)));
		}
		const auto moved = (input.mouse - _pressMouse) / glm::max(input.screen, glm::vec2(1.0f));
		_subMeshLook = MapLook(_mapFocus, _mapYaw, _mapPitch, moved);
		_subMeshZoom = std::min(_subMeshZoom + (dt * k_MapZoomSpeed), 1.0f);

		if (const auto leave = MapDoubleClickLeave(input.button, _leavingByMap, _mapFocus); leave.has_value())
		{
			_mapDoubleClick = *leave;
		}
	}

	// With the mouse free, the screen's top and bottom edges lean, and the pool slowly draws the camera down to it
	float leanSpeed = 0.0f;
	if (input.button == 0)
	{
		const auto edge = EdgeLeanOf(input.mouseScreen.y, KindOf(input.hit) == HitKind::Pool, _leanTarget);
		leanSpeed = edge.speed;
		_edgeRamp = edge.rampRate.has_value() ? (dt * *edge.rampRate) + _edgeRamp : 0.0f;
		_edgeRamp = std::clamp(_edgeRamp, 0.0f, 1.0f);
	}
	_leanTarget += LeanStep(leanSpeed, dt, _edgeRamp);
	// OPENBLACK_TEMPLE_CAMERA_TRACE=1: the main room's lean each frame it moves (docs/wiki/project/debugging.md)
	static const bool k_LeanTrace = std::getenv("OPENBLACK_TEMPLE_CAMERA_TRACE") != nullptr;
	if (debug_env::TraceLogs(k_LeanTrace) && (leanSpeed != 0.0f || input.button != 0))
	{
		spdlog::get("game")->info("Temple lean: mouse ({:.3f}, {:.3f}), button {}, edge speed {:.4f}, ramp {:.4f}, aim {:.4f}",
		                          input.mouseScreen.x, input.mouseScreen.y, static_cast<int>(input.button), leanSpeed,
		                          _edgeRamp, _leanTarget);
	}

	// The arrow keys turn and lean
	const auto aims = OrbitKeys({.yaw = _yawTarget, .lean = _leanTarget}, dt, actions.Get(BindableActionMap::MOVE_LEFT),
	                            actions.Get(BindableActionMap::MOVE_RIGHT), actions.Get(BindableActionMap::MOVE_FORWARDS),
	                            actions.Get(BindableActionMap::MOVE_BACKWARDS));
	_yawTarget = aims.yaw;
	_leanTarget = aims.lean;

	_yaw.SetDestinationWithSpeedAndTime(_yawTarget, 0.0f, k_OrbitEaseTime);
	_lean.SetDestinationWithSpeedAndTime(_leanTarget, 0.0f, k_OrbitEaseTime);
	_yaw.Update(dt);
	_lean.Update(dt);
	_target = OrbitPose(_yaw.value, _lean.value);
	_originTime = MainRoomEaseTime(_subMeshZoom);
	_focusTime = _originTime;

	// Clicking a door goes through it
	if (input.button != 0 && !_wasPressed && input.door.has_value() && *input.door != k_ScrollWall)
	{
		// The door of a room that is not open leaves the camera where it is, and the help tells the player so. The
		// way out is always open.
		if (const auto room = RoomBehindDoor(*input.door); room.has_value())
		{
			auto& temple = Locator::temple::value();
			if (!temple.IsRoomAvailable(*room))
			{
				temple.TellRoomNotAvailable();
				return;
			}
		}
		_doorStart = TurnToDoor(k_DoorApproach, *input.door);
		_doorEnd = TurnToDoor(k_DoorEntry, *input.door);
		_doorRoom = RoomBehindDoor(*input.door);
		_nextState = State::ThroughDoor;
	}
}

void TempleCameraModel::UpdateLook(float dt, const Input& input)
{
	// The creature's room: the camera stays where the room's path ends, and dragging turns the look with the
	// mouse, by as much as the lens sees across the screen
	const auto& actions = Locator::gameActionSystem::value();
	if (input.button != 0 && !_wasPressed)
	{
		_lookAtPress = _look;
	}
	if ((input.button == 0 && _wasPressed) || input.button == 2)
	{
		_lookAtPress = _look;
	}
	if (input.button != 0 && Locator::windowing::has_value())
	{
		const float width = static_cast<float>(Locator::windowing::value().GetSize().x);
		_look = CaveDragLook(_lookAtPress, _pressMouse, input.mouse, width);
	}

	// The arrow keys speed up its turn, which slows as they're let go
	const auto keyed =
	    CaveKeysStep({.look = _look, .atPress = _lookAtPress, .speed = _lookSpeed}, dt,
	                 actions.Get(BindableActionMap::MOVE_LEFT), actions.Get(BindableActionMap::MOVE_RIGHT),
	                 actions.Get(BindableActionMap::MOVE_FORWARDS), actions.Get(BindableActionMap::MOVE_BACKWARDS));
	_look = keyed.look;
	_lookAtPress = keyed.atPress;
	_lookSpeed = keyed.speed;

	_target = LookPose(_lookFrom, _look.x, _look.y);
	// the ease reads the zoom before this frame's step
	_originTime = CaveLookEaseTime(_caveZoomAtFrameStart);
	_focusTime = _originTime;
	// Clicking the room's door walks back through it, from halfway between the camera and the door; so does any press
	// on the same door as last time (or on no door when it never has) once the camera is within 25 of the room's cursor
	// point, near the door's side of the temple. The walk turns by the door under the cursor, none (-1) included
	const auto point = Locator::temple::has_value() ? Locator::temple::value().GetRoomCursorPoint() : glm::vec3(0.0f);
	const int32_t door = input.doorBack.has_value() ? static_cast<int32_t>(*input.doorBack) : -1;
	if (CaveWalksBack(input.button != 0 && !_wasPressed, input.button != 0, door, _caveLastDoor, glm::length(point),
	                  glm::length(point - input.eye)))
	{
		_caveLastDoor = door;
		WalkBackThrough(door);
	}
}

std::optional<glm::vec3> TempleCameraModel::CaveMeshPoint(uint32_t index)
{
	// The room's object gives the place of its mesh's point
	const entt::id_type creatureRoom = entt::hashed_string("temple/interior/creature_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(creatureRoom) || index >= meshes.Handle(creatureRoom)->GetExtraMetrics().size())
	{
		return std::nullopt;
	}
	return glm::vec3(meshes.Handle(creatureRoom)->GetExtraMetrics()[index][3]);
}

void TempleCameraModel::UpdateCaveTargets(float dt, const Input& input)
{
	using namespace CreatureCaveTargets;
	_caveTarget = !(_state == State::Look && !_lookingAtSubMesh) ? std::nullopt
	              : input.overCaveCreature                       ? std::optional(Target::Creature)
	                                                             : TargetAt(input.caveTargets, input.mouse, input.screen.y);
	// OPENBLACK_TEMPLE_CAMERA_TRACE=1: the cave's targets on the screen, and the one under the pointer, each time it
	// changes (docs/wiki/project/debugging.md)
	static const bool k_CaveTrace = std::getenv("OPENBLACK_TEMPLE_CAMERA_TRACE") != nullptr;
	if (debug_env::TraceLogs(k_CaveTrace) && _caveTarget != _tracedCaveTarget)
	{
		_tracedCaveTarget = _caveTarget;
		std::string places;
		for (size_t i = 0; i < input.caveTargets.size(); ++i)
		{
			const auto& place = input.caveTargets.at(i);
			places +=
			    place.has_value() ? fmt::format(" {} ({:.0f}, {:.0f})", i, place->x, place->y) : fmt::format(" {} none", i);
		}
		spdlog::get("game")->info(
		    "Temple cave: the pointer at ({:.0f}, {:.0f}) is over target {}; the targets:{}", input.mouse.x, input.mouse.y,
		    _caveTarget.has_value() ? std::to_string(static_cast<int>(*_caveTarget)) : std::string("none"), places);
	}

	// A press anywhere zooms back, and a click on a target, while the camera is all the way back, zooms to it. Zoomed
	// onto the creature, the tattoo editor stays open
	if (input.button != 0 && !_wasPressed)
	{
		_caveZoomTarget = 0.0f;
		_caveZoomedTo = ZoomedToAfterPress(_caveZoomedTo);
		_pressDragged = false;
	}
	if (input.button != 0 && glm::any(glm::greaterThan(glm::abs(input.mouse - _pressMouse), glm::vec2(k_DragDistance))))
	{
		_pressDragged = true;
	}
	const bool clicked = (input.button == 0 && _wasPressed) || input.button == 2;
	if (clicked && !_pressDragged && _caveZoom == 0.0f && _caveTarget.has_value())
	{
		ZoomToCaveTarget(*_caveTarget);
		if (debug_env::TraceLogs(k_CaveTrace))
		{
			spdlog::get("game")->info("Temple cave: target {} clicked: the zoom aims at {:.1f}, zoomed to {}",
			                          static_cast<int>(*_caveTarget), _caveZoomTarget,
			                          _caveZoomedTo.has_value() ? std::to_string(static_cast<int>(*_caveZoomedTo))
			                                                    : std::string("none"));
		}
	}
	const auto& actions = Locator::gameActionSystem::value();
	if (actions.GetAny(BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_RIGHT, BindableActionMap::MOVE_FORWARDS,
	                   BindableActionMap::MOVE_BACKWARDS))
	{
		CloseTattooEditor();
		_caveZoomTarget = 0.0f;
		_caveZoomedTo.reset();
	}
	// Zoomed onto the creature the camera takes the editor's view as it turns it, and zooms back out once it has closed
	if (_caveZoomedTo == Target::Creature)
	{
		const auto* editor = TattooEditor();
		if (editor != nullptr && editor->IsOpen())
		{
			_creatureView = editor->GetView();
			_caveLook = {_creatureView->eye, _creatureView->focus};
		}
		else
		{
			_tattooEditorOpened = false;
			_caveZoomTarget = 0.0f;
			_caveZoomedTo.reset();
		}
	}
	else if (_tattooEditorOpened)
	{
		// The editor the zoom onto the creature opened lasts only while the room is zoomed to the creature
		CloseTattooEditor();
	}

	// It zooms in over a second and back in half that, and zoomed to the exit, the player leaves the temple
	_caveZoomAtFrameStart = _caveZoom;
	const bool zoomingIn = _caveZoom < _caveZoomTarget;
	_caveZoom = CaveZoomStep(_caveZoom, _caveZoomTarget, dt);
	if (zoomingIn && _caveZoom == 1.0f && _caveZoomedTo == Target::Exit)
	{
		Locator::temple::value().RequestLeave();
	}
}

void TempleCameraModel::ZoomToCaveTarget(CreatureCaveTargets::Target target)
{
	using namespace CreatureCaveTargets;
	switch (target)
	{
	case Target::Creature:
		// The editor opens as the camera starts zooming onto the creature
		if (!OpenTattooEditor())
		{
			return;
		}
		break;
	case Target::Exit:
		// The camera goes to the exit as the temple fades to white
		_caveLook = {k_ExitEye, k_ExitPlace};
		Locator::temple::value().FadeToWhite();
		break;
	case Target::Belts:
	case Target::Medals:
	{
		// The belts and the medals start their help
		temple_help::GameScripts scripts;
		temple_help::ZoomToCaveTarget(scripts, target);
		const auto offset = static_cast<uint32_t>(target);
		const auto eye = CaveMeshPoint(k_FirstEyePoint + offset);
		const auto look = CaveMeshPoint(k_FirstLookPoint + offset);
		if (!eye.has_value() || !look.has_value())
		{
			return;
		}
		_caveLook = {*eye, *look};
		break;
	}
	default:
		// The fourth place does nothing
		return;
	}
	_caveZoomTarget = 1.0f;
	_caveZoomedTo = target;
}

void TempleCameraModel::ResetCaveTargets()
{
	CloseTattooEditor();
	_caveTarget.reset();
	_caveZoomedTo.reset();
	_caveZoom = 0.0f;
	_caveZoomAtFrameStart = 0.0f;
	_caveZoomTarget = 0.0f;
}

bool TempleCameraModel::OpenTattooEditor()
{
	auto* editor = TattooEditor();
	if (editor == nullptr || !Locator::creatureCaveSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto creature = Locator::creatureCaveSystem::value().GetCreature();
	if (!creature.has_value())
	{
		return false;
	}
	if (_tattooEditorOpened && editor->IsOpen())
	{
		// Zoomed back by a press, the editor is still open on the tattoos it opened with
		_caveLook = {_creatureView->eye, _creatureView->focus};
		return true;
	}
	if (!_creatureView.has_value())
	{
		// The editor looks at the room's copy of the creature, where it was put as the room started, at its size
		const auto copy = CaveCopy();
		if (!copy.has_value())
		{
			return false;
		}
		_creatureView = creature_tattoo_editor::CaveView(copy->first, copy->second);
	}
	editor->Open(*creature, *_creatureView);
	_tattooEditorOpened = true;
	_caveLook = {_creatureView->eye, _creatureView->focus};
	return true;
}

void TempleCameraModel::CloseTattooEditor()
{
	auto* editor = TattooEditor();
	if (!std::exchange(_tattooEditorOpened, false) || editor == nullptr || !editor->IsOpen())
	{
		return;
	}
	_creatureView = editor->GetView();
	editor->Close();
}

std::optional<CreatureCaveTargets::Target>
TempleCameraModel::ZoomedToAfterPress(std::optional<CreatureCaveTargets::Target> zoomedTo)
{
	return zoomedTo == CreatureCaveTargets::Target::Creature ? zoomedTo : std::nullopt;
}

void TempleCameraModel::ResetPictureCamera(Room room)
{
	// Looking along the end of the room's path, half leaning and low
	auto& picture = _pictures.at(static_cast<size_t>(room));
	picture.yawTarget = PictureStartYaw(PathEnd(room));
	picture.leanTarget = k_PictureStartLean;
	picture.heightTarget = k_PictureStartHeight;
}

void TempleCameraModel::UpdatePictureCamera(float dt)
{
	const auto room = GetRoom();
	const auto& pictureRoom = *PictureRoomOf(room);
	auto& picture = _pictures.at(static_cast<size_t>(room));
	picture.leanTarget = std::clamp(picture.leanTarget, 0.0f, 1.0f);
	picture.heightTarget = std::clamp(picture.heightTarget, k_PictureMinHeight, pictureRoom.maxHeight);
	picture.yaw.SetDestinationWithSpeedAndTime(picture.yawTarget, 0.0f, k_PictureYawEaseTime);
	picture.lean.SetDestinationWithSpeedAndTime(picture.leanTarget, 0.0f, k_PictureLeanEaseTime);
	picture.height.SetDestinationWithSpeedAndTime(picture.heightTarget, 0.0f, k_PictureHeightEaseTime);
	picture.yaw.Update(dt);
	picture.lean.Update(dt);
	picture.height.Update(dt);

	// The higher the camera, the more it looks down. Under a dialog it levels out, other than in the library, where the
	// story's history is read in a dialog while the player looks about.
	float leanForHeight = (picture.heightTarget - k_PictureMinHeight) * k_PictureLeanPerHeight + k_PictureLowLean;
	if (_dialogOpen && !pictureRoom.aroundPathEnd)
	{
		leanForHeight = 0.0f;
	}
	picture.leanTarget += (leanForHeight - picture.leanTarget) * (1.0f - std::exp(-dt));
}

void TempleCameraModel::UpdatePictureOrbit(float dt, const Input& input)
{
	// A room of pictures, with the player in control
	const auto room = GetRoom();
	const auto& pictureRoom = *PictureRoomOf(room);
	auto& picture = _pictures.at(static_cast<size_t>(room));
	const auto pressKind = KindOf(_pressHit);

	// Dragging the floor or the walls turns the room and raises the camera to keep them under the mouse. A dialog takes
	// the mouse, and so does the press the player took the camera away from a close look with, until it is let go.
	if (input.button == 0)
	{
		_pressLeftLook = false;
	}
	if (!_pressLeftLook && !_dialogOpen && input.button != 0 && (pressKind == HitKind::Floor || pressKind == HitKind::Wall) &&
	    input.hit.has_value())
	{
		const auto aims =
		    PictureDragAims({picture.yawTarget, picture.leanTarget, picture.heightTarget}, picture.yaw.value,
		                    picture.height.value, _pressHit->angle, _pressHit->height, input.hit->angle, input.hit->height);
		picture.yawTarget = aims.yaw;
		picture.heightTarget = aims.height;
	}

	if (const auto turn = PictureTurnByItself(_pictureOrbitTime, dt, _dialogOpen, pictureRoom.aroundPathEnd,
	                                          room == Room::Multi && _futureWordsShown);
	    turn.has_value())
	{
		picture.yawTarget += *turn;
	}
	else
	{
		UpdatePictureKeys(dt, picture);
	}

	// The camera aims at the room's view, or on its way back from a close look between the two, and eases onto its aim
	// the slower the closer it still is to the look; once back it stays on the view
	_target = PictureOrbitPose(room, picture.yaw.value, picture.lean.value, picture.height.value, PathEnd(room).origin);
	if (_subMeshLook.has_value())
	{
		_target = TowardCloseLook(_target, *_subMeshLook, _subMeshZoom);
	}
	_originTime = PictureEaseTime(_subMeshZoom);
	_focusTime = _originTime;

	// Clicking the room's door walks back through it, from halfway between the camera and the door
	if (input.button != 0 && !_wasPressed && input.doorBack == pictureRoom.door)
	{
		WalkBackThrough(pictureRoom.door);
	}
	// TODO(raffclar): a room of pictures can also look at its pictures
}

void TempleCameraModel::UpdatePictureLook(float dt, bool leftByPlayer)
{
	// Looking at something close up, a room of pictures' camera aims from the room's view toward the look, as far as
	// the blend has come, and eases onto its aim in the half second it is given every frame. The room's camera heads up
	// to the aim's height, half leaning.
	const auto room = GetRoom();
	auto& picture = _pictures.at(static_cast<size_t>(room));
	_target = PictureOrbitPose(room, picture.yaw.value, picture.lean.value, picture.height.value, PathEnd(room).origin);
	if (_subMeshLook.has_value())
	{
		_target = TowardCloseLook(_target, *_subMeshLook, _subMeshZoom);
	}
	picture.leanTarget = k_PictureLookLean;
	picture.heightTarget = _target.origin.y;
	_subMeshZoom = temple_challenge_room::BlendStep(_subMeshZoom, true, dt);
	if (leftByPlayer)
	{
		// Taken away by the player, the room turns to face the way the look faced and leans back, and the press that
		// took it away does not drag the room
		picture.yawTarget = PictureStartYaw(_target);
		picture.leanTarget = k_PictureLeftLookLean;
		_pressLeftLook = true;
	}
	if (!_lookingAtSubMesh)
	{
		// Back with the room, the camera heads for the height it starts at, and the room's turn by itself starts afresh
		picture.heightTarget = k_PictureStartHeight;
		_pictureOrbitTime = 0.0f;
	}
}

std::optional<float> TempleCameraModel::PictureTurnByItself(float& turnTime, float dt, bool dialogOpen, bool aroundPathEnd,
                                                            bool futureWordsShown)
{
	// Under a dialog the room turns slowly by itself, speeding up over a second from when it starts, rather than with
	// the arrow keys. The library keeps still. With no dialog, the future room turns by itself in the same way while
	// it shows its words.
	if (!dialogOpen && !futureWordsShown)
	{
		return std::nullopt;
	}
	const float speedUp = std::clamp(turnTime, 0.0f, 1.0f);
	turnTime += dt;
	if (dialogOpen && aroundPathEnd)
	{
		return 0.0f;
	}
	return speedUp * dt * k_PictureDialogTurnSpeed;
}

void TempleCameraModel::UpdatePictureKeys(float dt, PictureCamera& picture)
{
	// The arrow keys turn, and lean and raise or lower the camera, which the lean then follows
	const auto& actions = Locator::gameActionSystem::value();
	const auto aims =
	    PictureKeysStep({picture.yawTarget, picture.leanTarget, picture.heightTarget}, dt,
	                    actions.Get(BindableActionMap::MOVE_LEFT), actions.Get(BindableActionMap::MOVE_RIGHT),
	                    actions.Get(BindableActionMap::MOVE_FORWARDS), actions.Get(BindableActionMap::MOVE_BACKWARDS));
	picture.yawTarget = aims.yaw;
	picture.leanTarget = aims.lean;
	picture.heightTarget = aims.height;
}

TempleCameraModel::PictureAims TempleCameraModel::PictureKeysStep(PictureAims aims, float dt, bool left, bool right,
                                                                  bool forwards, bool backwards)
{
	if (left)
	{
		aims.yaw += dt;
	}
	if (right)
	{
		aims.yaw -= dt;
	}
	float tilt = aims.lean * 2.0f - 1.0f;
	if (forwards)
	{
		tilt -= dt * k_PictureLeanSpeed;
		aims.height -= tilt * dt * k_PictureRiseSpeed;
	}
	if (backwards)
	{
		tilt += dt * k_PictureLeanSpeed;
		aims.height -= tilt * dt * k_PictureRiseSpeed;
	}
	aims.lean = tilt * 0.5f + 0.5f;
	return aims;
}

TempleCameraModel::PictureAims TempleCameraModel::PictureDragAims(PictureAims aims, float yawReached, float heightReached,
                                                                  float pressAngle, float pressHeight, float hitAngle,
                                                                  float hitHeight)
{
	float turn = pressAngle - hitAngle;
	if (turn < -glm::pi<float>())
	{
		turn += glm::two_pi<float>();
	}
	if (turn > glm::pi<float>())
	{
		turn -= glm::two_pi<float>();
	}
	aims.yaw = yawReached + turn;
	aims.height = pressHeight - hitHeight + heightReached;
	return aims;
}

TempleCameraModel::Pose TempleCameraModel::WalkBackStart(const Pose& aim, const Pose& through)
{
	Pose start {(through.origin + aim.origin) * 0.5f, (through.focus + aim.focus) * 0.5f};
	start.origin.y = through.origin.y;
	return start;
}

void TempleCameraModel::WalkBackThrough(int32_t door)
{
	// Kept in the room once the game is over, going back out through its door leaves the temple
	if (auto& temple = Locator::temple::value(); temple.GetKeptRoom().has_value())
	{
		temple.RequestLeave();
		return;
	}
	// The rooms' cameras head halfway from where they are to the door, at its height, then through it
	const auto through = TurnToDoor(k_DoorBack, door);
	_doorStart = WalkBackStart(_target, through);
	_doorEnd = through;
	_doorRoom = Room::Main;
	_nextState = State::ThroughDoor;
}

TempleCameraModel::State TempleCameraModel::ControlOf(Room room) const
{
	if (room == Room::Main)
	{
		return State::Orbit;
	}
	if (room == Room::CreatureCave)
	{
		return State::Look;
	}
	return State::PictureOrbit;
}

TempleCameraModel::Pose TempleCameraModel::PathEnd(Room room) const
{
	const auto& path = _paths.at(static_cast<size_t>(room));
	if (!path)
	{
		return {_origin.GetCurrentValue(), _focus.GetCurrentValue()};
	}
	const auto end = path->SampleAt(path->GetDuration());
	return {end.position, end.focus};
}

TempleCameraModel::Pose TempleCameraModel::ControlPose(Room room) const
{
	switch (ControlOf(room))
	{
	case State::Orbit:
		// The main room's camera eases into its start, unturned
		return OrbitPose(0.0f, k_StartLean);
	case State::PictureOrbit:
	{
		const auto& picture = _pictures.at(static_cast<size_t>(room));
		return PictureOrbitPose(room, picture.yaw.value, picture.lean.value, picture.height.value, PathEnd(room).origin);
	}
	default:
		// The creature's room's camera takes over where its path ends
		return PathEnd(room);
	}
}

void TempleCameraModel::StartOrbitAfresh()
{
	// The main room's camera starts the player facing the scroll, leaning a little up
	_yawTarget = 0.0f;
	_leanTarget = k_StartLean;
	_yaw.SetPosition(0.0f);
	_lean.SetPosition(k_StartLean);
}

void TempleCameraModel::UpdateThroughDoor(float dt, const Input& input)
{
	// The camera waits by the door, then goes through it
	const auto walk = DoorWalkStep(_doorStage, _doorTime, dt, input.button != 0 && !_wasPressed);
	if (walk.headsThrough)
	{
		_target = _doorEnd;
		_origin.SetDestinationWithTime(_doorEnd.origin, 7.0f);
		_focus.SetDestinationWithTime(_doorEnd.focus, 7.0f);
	}
	_doorStage = walk.stage;
	_doorTime = walk.time;
	_originTime = 3.0f;
	_focusTime = 3.0f;
	_easeToTarget = false;
}

void TempleCameraModel::ChangeState()
{
	if (_nextState == _state)
	{
		return;
	}
	switch (_nextState)
	{
	case State::Intro:
		_introTime = 0.0f;
		_blendTime = 0.0f;
		break;
	case State::Orbit:
		StartOrbitAfresh();
		break;
	case State::Look:
	{
		ResetCaveTargets();
		// The creature's room: the look starts along the end of the room's path
		const auto end = PathEnd(GetRoom());
		_lookFrom = end.origin;
		_look = HeadingAndPitch(end);
		_lookAtPress = _look;
		_lookSpeed = glm::vec2(0.0f);
		break;
	}
	case State::PictureOrbit:
		// A room of pictures starts its orbit afresh
		_pictureOrbitTime = 0.0f;
		break;
	case State::ThroughDoor:
		_target = _doorStart;
		_origin.SetDestinationWithTime(_doorStart.origin, 3.0f);
		_focus.SetDestinationWithTime(_doorStart.focus, 3.0f);
		_doorTime = 0.0f;
		_doorStage = DoorStage::Approaching;
		_easeToTarget = false;
		break;
	}
	_state = _nextState;
}

void TempleCameraModel::LookAtSubMesh(glm::vec3 position, glm::vec3 lookAt)
{
	// The temple wooshes as it asks for a look while the camera isn't looking at something already
	_subMeshLook = Pose {position, lookAt};
	_lookingAtSubMesh = true;
}

std::optional<TempleCameraModel::DoorwayCall> TempleCameraModel::DoorwayAtFrameStart(bool throughDoor, DoorStage stage,
                                                                                     Room room, std::optional<Room> doorRoom)
{
	if (!throughDoor)
	{
		return std::nullopt;
	}
	switch (stage)
	{
	case DoorStage::Approaching:
		return DoorwayCall {.kind = DoorwayCall::Kind::FastClose};
	case DoorStage::Entering:
		if (room == Room::Main)
		{
			// The doorway waits a moment, then swings open ahead of the camera; the way out's swings and sounds with no
			// leaf to turn
			return DoorwayCall {.kind = DoorwayCall::Kind::Open,
			                    .leaf = doorRoom.has_value() ? TempleDoors::LeafOf(*doorRoom)
			                                                 : std::optional<uint32_t>(TempleDoors::k_ExitLeaf),
			                    .from = k_DoorOpenFrom,
			                    .rate = k_DoorOpenRate};
		}
		// Back into the main room, its doorway to this room swings open quickly ahead of the camera, and the main room's
		// camera turns it round to close behind it
		return DoorwayCall {.kind = DoorwayCall::Kind::Open,
		                    .leaf = TempleDoors::LeafOf(room),
		                    .from = k_DoorBackOpenFrom,
		                    .rate = k_DoorBackOpenRate};
	case DoorStage::Through:
	case DoorStage::Skipped:
		break;
	}
	return std::nullopt;
}

void TempleCameraModel::FollowTemple()
{
	// The temple draws the room the camera is heading into, and takes the player into it
	auto& temple = Locator::temple::value();
	auto& doors = temple.GetDoors();
	if (_state != State::ThroughDoor)
	{
		temple.SetTransitionRoom(std::nullopt);
		if (_state == State::Orbit || _state == State::Look || _state == State::PictureOrbit)
		{
			// Any door the player has the room's camera back by is shut
			doors.FastClose();
		}
		else if (_state == State::Intro && _blendTime > k_IntroDoorCloseTime)
		{
			const bool wasOpening = doors.GetLeaf().has_value() && doors.GetSwing() <= 1.0f;
			doors.Close(k_IntroDoorCloseRate);
			// (openblack, a readout at debug) the open doorway turned round to close behind the camera coming in
			if (const auto log = spdlog::get("game"); wasOpening && log != nullptr)
			{
				log->debug("Temple doors: the doorway turned round to close behind the camera at {:.2f} s of {}'s coming "
				           "in, its swing now {:.2f}",
				           _blendTime, TempleRoomName(GetRoom()), doors.GetSwing());
			}
		}
		return;
	}
	// (the doorway's calls of the walk are the frame's start's, DoorwayAtFrameStart)
	switch (_doorStage)
	{
	case DoorStage::Approaching:
		temple.SetTransitionRoom(std::nullopt);
		break;
	case DoorStage::Entering:
		temple.SetTransitionRoom(InMainRoom() ? _doorRoom : std::optional<Room>(Room::Main));
		break;
	case DoorStage::Through:
	case DoorStage::Skipped:
		// Through, the doorway swings on open behind the camera, until the next room's camera turns it round to close.
		// Skipping cuts to the room, which shuts it.
		temple.SetTransitionRoom(std::nullopt);
		if (!_doorRoom.has_value())
		{
			temple.RequestLeave();
		}
		else if (_doorStage == DoorStage::Through)
		{
			// The door's path runs on into the room's own, unbroken
			StartIntro(*_doorRoom, true);
		}
		else
		{
			// Clicking on the way through cuts to the room, behind the temple's fade
			temple.GoToRoom(*_doorRoom);
		}
		break;
	}
}

void TempleCameraModel::SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) {}

glm::vec3 TempleCameraModel::GetTargetOrigin() const
{
	return _target.origin;
}

glm::vec3 TempleCameraModel::GetTargetFocus() const
{
	return _target.focus;
}

std::optional<CameraModel::Lens> TempleCameraModel::GetLens() const
{
	// The temple's cameras' lens
	return Lens {.horizontalFieldOfView = k_FieldOfView, .nearClip = k_NearClip};
}
