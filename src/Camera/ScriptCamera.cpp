/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "EngineConfig.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::script_camera
{

State::State()
{
	fov.SetPosition(k_DefaultFov);
}

State::~State() = default;

State& Get()
{
	static State state;
	return state;
}

namespace
{
ThingReader& TestReader()
{
	static ThingReader reader;
	return reader;
}

/// The game angle of a MobileWallHug in openblack: an animal's AnimalBrain::angle is +0x5C itself; a villager keeps it
/// in WallHug::yAngle as ConvertGameAngleTo3D(a) (ECS/Villager/VillagerCore.h), turned back here (aproximado: rounded to
/// the nearest 2048th). A creature's is not kept (no creature body nor AI): none (aproximado)
std::optional<uint16_t> WallHugAngleOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* brain = registry.TryGet<const ecs::components::AnimalBrain>(entity); brain != nullptr)
	{
		return brain->angle;
	}
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		if (const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(entity); wallHug != nullptr)
		{
			const auto turns = std::lround(wallHug->yAngle * static_cast<float>(gutils::k_GameAngleCircle) / k_TwoPi);
			return static_cast<uint16_t>(static_cast<uint32_t>(turns) & static_cast<uint32_t>(gutils::k_GameAngleMask));
		}
	}
	return std::nullopt;
}

/// The thing as the original reads it, from the entities registry. (aproximado) A valid entity stands for
/// GameThing::IsAvailable (vt +0x2C), as GScript::GetScriptGameThing does in CHLApi.cpp; anything but a flock needs a
/// Transform (its MapCoords). (inferido) An entity with a Transform and a Mesh is an Object with a Game3DObject, whose
/// matrix is the Transform
std::optional<ThingInfo> RegistryThing(entt::entity entity)
{
	if (!Locator::entitiesRegistry::has_value() || entity == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	ThingInfo info;
	info.height = ecs::object::GetHeight(entity); // vt +0x42C
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
	{
		// IsFlock (vt +0x3EC). Flock::GetFlockPos 0x530570: the leader's MapCoords (+0x40 -> +8 -> +0x14), else the
		// flock's own +0x14 (Flock::domainCentre). The leader is the first member (ECS/AnimalAI.cpp)
		info.isFlock = true;
		const auto own = ecs::map_coords::ToWorld(ecs::map_coords::FromWorld(flock->domainCentre));
		info.mapPoint = own;
		info.flockPoint = own;
		if (!flock->members.empty() && registry.Valid(flock->members.front()))
		{
			const auto leader = flock->members.front();
			info.flockPoint = ecs::map_coords::ToWorld(ecs::object::MapCoordsOf(leader));
			info.leaderHeight = ecs::object::GetHeight(leader);
		}
		return info;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	info.mapPoint = ecs::map_coords::ToWorld(ecs::object::MapCoordsOf(entity));
	if (const auto* drawn = registry.TryGet<const ecs::components::DrawPosition>(entity); drawn != nullptr)
	{
		info.drawnPoint = drawn->position; // the villager's / animal's matrix drawn this frame (ECS/MobileDrawing.h)
	}
	else if (registry.AllOf<ecs::components::Mesh>(entity))
	{
		info.drawnPoint = transform->position;
	}
	// Villager::IsAvailable 0x751D50: not while dying
	if (registry.AllOf<ecs::components::Villager>(entity) &&
	    ecs::villager::GetFinalState(entity) == VillagerStates::Dying)
	{
		info.available = false;
	}
	info.wallHugAngle = WallHugAngleOf(registry, entity);
	if (info.wallHugAngle.has_value())
	{
		info.facingDirection = gutils::ConvertGameAngleToScawenAngle(*info.wallHugAngle); // MobileWallHug 0x60C020
	}
	// Creature 0x477EC0 reads its LH3DCreature (+0x160 -> +0x58 -> +0x84), not ported: 0 as a GameThingWithPos 0x4024B0
	// (aproximado)
	return info;
}

std::optional<ThingInfo> ReadThing(entt::entity entity)
{
	if (entity == entt::null)
	{
		return std::nullopt;
	}
	if (const auto& reader = TestReader(); reader)
	{
		return reader(entity);
	}
	return RegistryThing(entity);
}

void DropPath(State& state)
{
	// fn_00461A60: the ScriptedCamera freed (fn_00446AC0) and +0x58 = 0
	state.positionRunner.reset();
	state.track.reset();
}

/// CameraModeFollow::Set(0) 0x44BA00: no position thing; with "behind" (+0x1C) the heading is 0 (0x44BA76..0x44BA7D)
void StopPositionFollow(State& state)
{
	state.positionThing = entt::null;
	if (state.behind)
	{
		state.heading = 0.0f;
	}
}

/// CameraModeScript::GetFocusThing 0x4611F0: +0x4C, else +0x08
entt::entity FocusThingOf(const State& state)
{
	return state.focusThing != entt::null ? state.focusThing : state.positionThing;
}

/// The follow's script part of the mode, as the ctors leave it (0x44B835..0x44B867, 0x461180..0x4611BC)
void ResetFollow(State& state)
{
	state.positionThing = entt::null;
	state.focusThing = entt::null;
	state.heading = 0.0f;
	state.pitch = 0.0f;
	state.distance = 0.0f;
	state.timeFactor = k_ScriptFollowTimeFactor;
	state.behind = true;
}

/// Where the position follows a thing to: SetPointFromPointDistanceHeadingAndPitch(point, +0x14, heading, +0x10) with
/// the pitch clamped and kept (0x44BF7E..0x44BFFC, 0x44C758..0x44C7DB)
glm::vec3 PositionFor(State& state, const ThingInfo& thing, bool update)
{
	const auto point = FollowPoint(thing, update);
	state.pitch = FollowPitch(state.pitch);
	const float heading = FollowHeading(state.heading, state.behind, thing.wallHugAngle);
	return PointFromDistanceHeadingAndPitch(point, state.distance, heading, state.pitch);
}

/// CameraModeFollow::Update 0x44C160 for the script mode: +0x20 (zoom to town) is 0 and +0x44 is 0 (0x4611B5), so
/// neither the zoom branch (0x44C248) nor the keys (0x44C7EE) run, and it leaves at 0x44CB3D (__RTDynamicCast to
/// CameraModeScript) after the inclusion check, whose answer it does not use
void UpdateFollow(State& state)
{
	state.distance = ClampFollowDistance(state.distance);                     // 0x44C16B..0x44C1A2
	const float seconds = FollowSeconds(state.modeSeconds, state.timeFactor); // 0x44C1A5..0x44C1E1
	// 0x44C1E5..0x44C245: a creature focus thing writes 0xCC62E4..0xCC6310 (1 - life, its body's +0x1C / +0x30), read
	// elsewhere: not ported
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetDestinationWithTime(FollowPoint(*focus, true), seconds); // 0x44C555..0x44C597
	}
	// else GetComputerPlayerFocus (vt +0x4C) != -1: the computer player's hand (0x44C59C..), not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetDestinationWithTime(PositionFor(state, *thing, true), seconds); // 0x44C8C2..0x44C8FB
	}
	// else GetComputerPlayerFollow (vt +0x50) != -1: from the computer player's hand (0x44C905..), not ported
}
} // namespace

void Reset()
{
	auto& state = Get();
	DropPath(state);
	ResetFollow(state);
	state.scriptMode = false;
	state.modeSeconds = 0.0f;
	state.fov.SetPosition(k_DefaultFov); // GCamera ctor 0x441A78..0x441A83
}

bool Begin(const glm::vec3& origin, const glm::vec3& focus)
{
	auto& state = Get();
	if (state.scriptMode) // GCamera::CantExitCurrentMode 0x441B70 -> CameraModeScript::CanExit 0x461B70
	{
		return false;
	}
	DropPath(state); // ctor 0x461180: +0x58 = 0
	ResetFollow(state);
	state.scriptMode = true;
	state.modeSeconds = 0.0f; // SwitchToViewMode 0x441CD0 from the CameraModeFollow ctor 0x44B947
	state.position.SetPosition(origin);
	state.focus.SetPosition(focus);
	return true;
}

bool BeginFrom(const Zoomer3d& origin, const Zoomer3d& focus)
{
	if (!Begin(origin.GetCurrentValue(), focus.GetCurrentValue()))
	{
		return false;
	}
	auto& state = Get();
	state.position = origin; // GCamera +0x118 / +0x88: the same zoomers, still heading where they were
	state.focus = focus;
	return true;
}

void HandBack(Zoomer3d& origin, Zoomer3d& focus)
{
	const auto& state = Get();
	origin = state.position;
	focus = state.focus;
}

bool End()
{
	auto& state = Get();
	const bool wasScript = state.scriptMode;
	// CameraModeNew3 0x4572E0 -> Initialise 0x456640: the player's mode starts from GCamera's zoomers. Only when the
	// script mode drove the camera (with "free start" or the camera test hooks the player kept it)
	if (wasScript && Drives() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	if (wasScript) // 0x6ECDBF..0x6ECE2E: Delete (vt+0x30) and a new CameraModeNew3 (0x4572E0)
	{
		state.scriptMode = false;
		DropPath(state);
		ResetFollow(state);
		state.modeSeconds = 0.0f; // the new CameraModeNew3's SwitchToViewMode
	}
	SetFov(help::script_control::k_ScriptEndFov, help::script_control::k_ScriptEndFovTime); // 0x6ECE35..0x6ECE48
	return wasScript;
}

bool Active()
{
	return Get().scriptMode;
}

bool Drives()
{
	// Test hooks (not original): OPENBLACK_CAMERA_LOCK / OPENBLACK_CAMERA_FLY hold the camera every turn for the team's
	// screenshots (Worship/WorshipDebugHooks.cpp), so the script mode leaves it to them
	static const bool s_testCameraHook =
	    std::getenv("OPENBLACK_CAMERA_LOCK") != nullptr || std::getenv("OPENBLACK_CAMERA_FLY") != nullptr;
	const auto& control = help::script_control::GetCameraControl();
	return Get().scriptMode && !s_testCameraHook && !help::script_control::IsFreeStartTask(control, control.owner);
}

void SetPosition(const glm::vec3& position)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state); // 0x46137E: Set(0); +0x54 = -1
	state.position.SetPosition(position);
}

void SetFocus(const glm::vec3& focus)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null; // 0x4612BE: SetCameraFocus(0) 0x4619B0; +0x50 = -1
	state.focus.SetPosition(focus);
}

void MovePosition(const glm::vec3& position, float seconds)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state); // 0x461702
	state.position.SetDestinationWithTime(position, seconds);
}

void MoveFocus(const glm::vec3& focus, float seconds)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null; // 0x461442
	state.focus.SetDestinationWithTime(focus, seconds);
}

void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus)
{
	auto& state = Get();
	state.position.SetPosition(position);
	state.focus.SetPosition(focus);
}

void RunPath(int32_t number)
{
	auto& state = Get();
	// fn_00461A80: Reset 0x461A30 (+0x4C = 0, +0x48 = 1, the old ScriptedCamera freed, +0x50 = +0x54 = -1; +0x08 kept)
	DropPath(state);
	state.focusThing = entt::null;
	state.track = LoadCameraTrack(number); // ScriptedCamera::Create 0x447060 ("Cannot load track No %d" when missing)
	if (state.track != nullptr)
	{
		state.positionRunner = std::make_unique<CameraWayRunner>(state.track->position);
	}
	state.pathMs = 0; // +0x5C
}

void SetFov(float fov, float seconds)
{
	auto& state = Get();
	// GCamera::SetCameraFov 0x443680: t == 0 or t < 0.001 sets it; else the quartic towards fov with speed 0 (inline
	// copy of 0x407D60, 0x4436CB..0x4437D7)
	state.fov.SetDestinationWithSpeedAndTime(fov, 0.0f, seconds);
}

// ---- CameraModeFollow --------------------------------------------------------------------------------------------------

float FollowSeconds(float modeSeconds, float timeFactor)
{
	// fcomp [0x8C7874]; test ah, 0x41: <= 2 takes the slow start; 1 [0x8C7870], 2 [0x8C786C]
	const float pace = modeSeconds > k_FollowSettleSeconds ? 1.0f : modeSeconds / k_FollowSettleSeconds * (1.0f - 2.0f) + 2.0f;
	return pace * timeFactor; // fmul [esi + 0x18]
}

float ClampFollowDistance(float distance)
{
	if (!(distance > k_FollowMinDistance)) // test ah, 0x41 (0x44C177)
	{
		return k_FollowMinDistance;
	}
	return distance < k_FollowMaxDistance ? distance : k_FollowMaxDistance; // test ah, 1 (0x44C187)
}

float FollowPitch(float pitch)
{
	return pitch > k_FollowMinPitch ? pitch : k_FollowMinPitch; // test ah, 0x41 (0x44C766)
}

float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle)
{
	if (!behind || !wallHugAngle.has_value())
	{
		return heading;
	}
	// xor ecx, ecx; mov cx, [eax + 0x5C]; shl ecx, 1; fild; fmul [0x8C78DC]; fsub [0x8C78D8]; fsubr heading
	const float angle = static_cast<float>(static_cast<uint32_t>(*wallHugAngle) << 1u) * k_FollowAngleScale - k_HalfPi;
	return heading - angle;
}

float ThingViewingDistance(float height)
{
	return height * k_ThingViewingDistanceFactor;
}

glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch)
{
	// 0x442811..0x442856: cos q kept as a float, then each product in order
	const float cosPitch = std::cos(pitch);
	const float z = std::cos(heading) * cosPitch * distance + p.z;
	const float y = std::sin(pitch) * distance + p.y;
	const float x = std::sin(heading) * cosPitch * distance + p.x;
	return {x, y, z};
}

float ArcTan2(float x, float y)
{
	// fn_007FA990 (the symbols call it ??GLHPoint): fpatan with st1 = the quotient, st0 = 1
	if (!(x < y) && !(-y > x)) // 0x7FA990..0x7FA9AE: x >= |y|
	{
		return std::atan(y / x);
	}
	if (!(y < x) && !(-x > y)) // 0x7FA9BD..0x7FA9DB: y >= |x|
	{
		return static_cast<float>(1.5707963705062866 - static_cast<double>(std::atan(x / y))); // [0x8C7B48]
	}
	if (!(-y < x) && x < y) // 0x7FA9F0..0x7FAA0E: x <= -|y|
	{
		const double turn = 3.1415927410125732; // [0x8D45D0]
		const double a = std::atan(y / x);
		return static_cast<float>(y < 0.0f ? a - turn : a + turn); // 0x7FAA10..0x7FAA34
	}
	return static_cast<float>(-1.5707963705062866 - static_cast<double>(std::atan(x / y))); // [0x9361E8], 0x7FAA3B
}

void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch)
{
	const glm::vec3 v = a - b; // 0x4428D3..0x4428F3
	if (std::abs(static_cast<double>(v.x)) < k_VerticalEpsilon && std::abs(static_cast<double>(v.z)) < k_VerticalEpsilon)
	{
		heading = 0.0f;          // 0x442925
		pitch = k_VerticalPitch; // 0x44292B: 0x3FC50A6B
		return;
	}
	// fn_007FAA50 (the symbols call it SetUnitDirectionVectorFromScreenPoint): 0 when x^2 + z^2 <= 1e-6, else
	// fn_007FA990(-z, x)
	const float horizontal = v.x * v.x + v.z * v.z;
	const float direction = horizontal > k_NoHeadingSquared ? ArcTan2(-v.z, v.x) : 0.0f;
	heading = k_Pi - direction; // fsubr [0x8C36A0]
	// 0x442950..0x442976: fn_007FA990(sqrt(z^2 + x^2), y)
	pitch = ArcTan2(std::sqrt(v.z * v.z + v.x * v.x), v.y);
}

glm::vec3 FollowPoint(const ThingInfo& thing, bool update)
{
	if (thing.isFlock) // IsFlock vt +0x3EC (0x44C460 / 0x44BC61)
	{
		auto point = thing.flockPoint; // GetFlockPos 0x530570: GetAltitude + its altitude (0x44C49D..0x44C4C4)
		if (thing.leaderHeight.has_value())
		{
			point.y = *thing.leaderHeight * 0.5f + point.y; // fmul [0x8AA3B4]; fadd (0x44C541..0x44C551)
		}
		return point;
	}
	auto point = thing.mapPoint; // 0x44C4D4..0x44C50F
	if (update && thing.drawnPoint.has_value())
	{
		point = *thing.drawnPoint; // 0x44C513..0x44C539: [Object +0x40] + 0x38
	}
	point.y = thing.height * 0.5f + point.y; // 0x44C541..0x44C551
	return point;
}

glm::vec3 FacePoint(const ThingInfo& thing)
{
	auto point = thing.mapPoint;             // 0x6ED728..0x6ED759
	point.y = thing.height * 0.5f + point.y; // 0x6ED75E..0x6ED76F
	return point;
}

glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance)
{
	float heading = facing;
	if (!(heading < k_FaceMaxHeading)) // 0x6ED77E..0x6ED789
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Invalid heading"); // 0xC0C208, and on
		}
	}
	while (heading > k_TwoPi) // 0x6ED798..0x6ED7C2
	{
		heading -= k_TwoPi;
	}
	return PointFromDistanceHeadingAndPitch(focus, distance, heading, k_FacePitch); // 0x6ED7D0..0x6ED7D9
}

void FocusFollow(entt::entity thing)
{
	auto& state = Get();
	DropPath(state); // 0x4619B4
	// 0x4619BD..0x4619DC: IsAvailable (vt +0x2C) -> +0x4C = thing, else 0
	const auto info = ReadThing(thing);
	state.focusThing = info.has_value() && info->available ? thing : entt::null;
}

void PositionFollow(entt::entity thing)
{
	auto& state = Get();
	state.positionThing = thing; // 0x44BA0C
	if (thing != entt::null)
	{
		// 0x44BA11..0x44BA62: from the zoomers' destinations (+4 of each), the position from the focus
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		const auto info = ReadThing(thing);
		state.distance = ThingViewingDistance(info.has_value() ? info->height : 0.0f); // 0x44BA6B
	}
	if (state.behind)
	{
		state.heading = 0.0f; // 0x44BA76..0x44BA7D
	}
}

void FocusAndPositionFollow(entt::entity thing, float distance)
{
	auto& state = Get();
	state.positionThing = thing; // 0x44BA9C
	if (thing != entt::null)
	{
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		state.distance = distance; // 0x44BAFE
	}
}

void PlaceFollowNow()
{
	auto& state = Get();
	// +0x20 == 0 (0x44BB46): the follow's points, Zoomer::SetPosition 0x441AC0 (and its inline copy for x)
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetPosition(FollowPoint(*focus, false)); // 0x44BD1D..0x44BD47
	}
	// else the computer player's hand (0x44BD52..), not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetPosition(PositionFor(state, *thing, false)); // 0x44BF73..0x44C13C, no distance clamp
	}
	// else the computer player's hand (0x44C056..), not ported
	state.modeSeconds = k_FollowSettleSeconds; // 0x44C141: [0x8C7874] -> GCamera +0x68
}

void SetFollowProperties(float distance, float timeFactor, float heading, bool behind)
{
	auto& state = Get();
	state.distance = distance;     // +0x14
	state.timeFactor = timeFactor; // +0x18
	state.behind = behind;         // +0x1C
	state.heading = heading;       // +0x0C
}

void Validate()
{
	auto& state = Get();
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	if (state.focusThing != entt::null && !available(state.focusThing)) // 0x461273..0x461283
	{
		state.focusThing = entt::null;
	}
	if (state.positionThing != entt::null && !available(state.positionThing)) // 0x44BB13..0x44BB23
	{
		state.positionThing = entt::null;
	}
}

std::optional<FacePoints> FaceObject(entt::entity thing, float distance)
{
	const auto info = ReadThing(thing);
	if (!info.has_value())
	{
		// 0x6ED717..0x6ED725: "no object to face" (0xC0C218), then the original reads through the null thing (inferido:
		// it would crash; here nothing is done)
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "no object to face");
		}
		return std::nullopt;
	}
	FacePoints points {};
	points.focus = FacePoint(*info);
	points.position = FacePosition(points.focus, info->facingDirection, distance); // GetFacingDirection vt +0x4EC
	return points;
}

bool ScriptArrived()
{
	const auto& state = Get();
	if (state.track != nullptr) // CameraModeScript::Arrived 0x461B40
	{
		return state.track->position.duration <= state.pathMs;
	}
	// CameraMode::Arrived 0x441700..0x441835
	const auto dp = state.position.GetCurrentValue() - state.position.GetDestination();
	const auto df = state.focus.GetCurrentValue() - state.focus.GetDestination();
	return glm::dot(dp, dp) < k_ArrivedDistanceSquared && glm::dot(df, df) < k_ArrivedDistanceSquared;
}

namespace
{
void UpdatePath(State& state, uint32_t gameMs)
{
	// CameraModeScript::UpdatePath 0x461AB0: the game ms of the frame (g_game_time_inc), the sample clamped to the
	// position way's duration, the focus on the focus way at the position Running's segment and parameter
	// (fn_008439C0), and both set at once (GCamera::SetPositionAndFocus 0x4438C0)
	state.pathMs += static_cast<int32_t>(gameMs);
	const auto sample = std::clamp(state.pathMs, 0, state.track->position.duration);
	const auto position = state.positionRunner->Get(sample);
	const auto focus = state.track->focus.Bezier(state.positionRunner->Segment(), state.positionRunner->Parameter());
	SetPositionAndFocus(position, focus);
}
} // namespace

void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	const float dt = std::min(cameraSeconds, k_MaxFrameSeconds); // 0x441FB0..0x441FC1
	state.modeSeconds += dt;                                     // 0x441FCD..0x441FD0, before the mode's Update
	if (state.scriptMode)
	{
		// GCamera::Validate 0x441F50 runs once a turn (GGame::ProcessTurn 0x54E74E, Game.cpp); until then a thing
		// that has gone is not read (ReadThing gives nothing for an invalid entity)
		if (state.track != nullptr)
		{
			UpdatePath(state, gameMs); // CameraModeScript::Update 0x461290
		}
		UpdateFollow(state); // 0x4612A1: CameraModeFollow::Update 0x44C160
	}
	state.position.Update(dt); // 0x441FEE..0x442029
	state.focus.Update(dt);
	// 0x44222C..0x44232A: the position's destination kept inside the disc of the world
	const glm::vec3 centre(k_DiscCentre, 0.0f, k_DiscCentre);
	const auto d = state.position.GetDestination() - centre;
	if (const float d2 = glm::dot(d, d); d2 > k_DiscRadiusSquared)
	{
		state.position.SetDestinationWithTime(d / (std::sqrt(d2) * k_DiscScale) + centre, k_DiscSeconds);
	}
	state.fov.Update(gameSeconds); // 0x4424F6..0x4425C3: g_game_time_inc * 0.001, not the camera's seconds
}

Drawn DrawnCamera(const std::function<float(float, float)>& groundAt)
{
	const auto& state = Get();
	static glm::vec3 s_goodOrigin(1000.0f, 0.0f, 1000.0f); // 0xC59B48
	static glm::vec3 s_goodFocus(1000.0f, 0.0f, 1000.0f);  // 0xC59B38
	Drawn drawn {state.position.GetCurrentValue(), state.focus.GetCurrentValue()};
	// 0x4420D9..0x4421D5: a NaN component -> the last good one
	for (int i = 0; i < 3; ++i)
	{
		drawn.origin[i] = std::isnan(drawn.origin[i]) ? s_goodOrigin[i] : drawn.origin[i];
		drawn.focus[i] = std::isnan(drawn.focus[i]) ? s_goodFocus[i] : drawn.focus[i];
	}
	s_goodOrigin = drawn.origin;
	s_goodFocus = drawn.focus;
	// 0x4421D5..0x442228: position and focus never the same point
	if (const auto d = drawn.origin - drawn.focus; glm::dot(d, d) < k_ArrivedDistanceSquared)
	{
		drawn.origin.x -= 1.0f;
		drawn.origin.y += 1.0f;
	}
	// 0x44242B..0x4424AB, measured under the nudged position (not in CameraModeFree, nor with GCamera+0x78 set: neither
	// exists here)
	if (const float lift = groundAt(drawn.origin.x, drawn.origin.z) + k_GroundClearance - drawn.origin.y; lift > 0.0f)
	{
		drawn.origin.y += lift;
		drawn.focus.y += lift;
	}
	return drawn;
}

bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	Frame(cameraSeconds, gameMs, gameSeconds);

	// 0x442337..0x4423F4: inside the citadel (g_game+0x205A28 == 1, here the temple interior) the drawn camera stays
	const bool insideCitadel = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool drive = Drives() && !insideCitadel;
	if (drive)
	{
		const auto drawn = DrawnCamera([](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z))
			                                           : -1e30f;
		});
		camera.SetOrigin(drawn.origin);
		camera.SetFocus(drawn.focus);
	}

	// LH3DTech::ChangeFov 0x8195B0 with the zoomer's value (0x4425C3; not inside the citadel, 0x4424F6); openblack keeps
	// the field of view in the config's degrees, which every rebuild of the projection reads
	if (!insideCitadel && state.fov.value != state.appliedFov && Locator::config::has_value() &&
	    Locator::windowing::has_value())
	{
		state.appliedFov = state.fov.value;
		auto& config = Locator::config::value();
		config.cameraXFov = state.fov.value == k_DefaultFov ? 70.0f : glm::degrees(state.fov.value);
		camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
		                                      config.cameraNearClip, config.cameraFarClip);
	}
	return drive;
}

namespace detail
{
void SetThingReaderForTests(ThingReader reader)
{
	TestReader() = std::move(reader);
}
} // namespace detail

} // namespace openblack::script_camera
