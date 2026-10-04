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
#include "3D/ObjectMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "CameraShake.h"
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
#include "GameClock.h"
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
	info.radius2d = ecs::object::Get2DRadius(entity); // CameraModeTwoObjects::Update 0x462041 (vt +0x64)
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

/// SwitchToViewMode 0x441CD0 for a new CameraModeTwoObjects
void PushDual(State& state, const DualMode& mode, const Zoomer3d& origin, const Zoomer3d& focus)
{
	if (!state.scriptMode && state.duals.empty())
	{
		// Over the player's mode: GCamera's zoomers are the same for every mode, so the dual camera goes on from wherever
		// the player's were heading (as BeginFrom)
		state.position = origin;
		state.focus = focus;
	}
	state.duals.push_back(mode);
	state.modeSeconds = 0.0f; // 0x441D30
}

/// The last dual camera gone with no script mode under it (PopViewMode 0x441C50 / CheckStackedModesForValidity
/// 0x441D40): the player's CameraModeNew3 is current again and goes on from GCamera's zoomers, so they are handed back
/// to the player's camera (as End). Only when this module drove the camera (`drove`: not with the camera test hooks).
/// (inferido) CameraModeNew3's Restart (vt +0x10) was not read
void HandBackAfterDual(const State& state, bool drove)
{
	if (drove && !state.scriptMode && state.duals.empty() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
}

/// CameraModeTwoObjects::Update 0x461DE0, once a frame while it is the current mode (GCamera::Update 0x441FD9)
void UpdateDualMode(State& state, DualMode& mode)
{
	// 0x461DED..0x461E26: the seconds of the destinations, with no time factor
	const float pace =
	    state.modeSeconds > k_DualSettleSeconds ? 1.0f : state.modeSeconds / k_DualSettleSeconds * (1.0f - 2.0f) + 2.0f;
	// (aproximado) the original reads +0x08 / +0x0C with no check (a null thing would crash it before the turn's
	// CheckStackedModesForValidity drops the mode); here a thing that cannot be read leaves the zoomers alone this frame
	const auto a = ReadThing(mode.a);
	if (!a.has_value())
	{
		return;
	}
	std::optional<ThingInfo> b;
	if (mode.twoObjects)
	{
		b = ReadThing(mode.b);
		if (!b.has_value())
		{
			return;
		}
	}
	// 0x461E2A..0x461EB2: the MapCoords points (x, z x 1/6553.6 [0x8AA3A4], GetAltitude 0x803090 + the altitude +0x1C;
	// not the Game3DObject nor a flock's GetFlockPos); B is the point without +0x1C
	const glm::vec3 pointA = a->mapPoint;
	const glm::vec3 pointB = b.has_value() ? b->mapPoint : mode.point;
	// 0x461EB6..0x461F10: (B + A) x 0.5 [0x8AA3B4]
	const glm::vec3 middle((pointB.x + pointA.x) * 0.5f, (pointB.y + pointA.y) * 0.5f, (pointB.z + pointA.z) * 0.5f);
	// 0x461F14..0x461F4A: (A's GetHeight (vt +0x42C) + B's, 1 without B) x 0.5
	const float meanHeight = (a->height + (b.has_value() ? b->height : k_DualPointHeight)) * 0.5f;
	// 0x461F4E..0x461FAE: the larger of A's height and B's (0 without B; test ah, 0x41: A only when strictly larger)
	const float otherHeight = b.has_value() ? b->height : 0.0f;
	const float maxHeight = a->height > otherHeight ? a->height : otherHeight;
	// 0x461FAE..0x461FFE: the focus heads for the middle raised by half the mean height (0x407D60 on x, y, z, speed 0)
	const glm::vec3 focus(middle.x, meanHeight * 0.5f + middle.y, middle.z);
	state.focus.SetDestinationWithTime(focus, pace);
	mode.pitch = FollowPitch(mode.pitch); // 0x462003..0x462021: at least [0x8C78E0], stored back
	// 0x462024..0x462080: an Object's Get2DRadius (vt +0x64), else 30
	const float radiusA = a->radius2d.value_or(k_DualDefaultRadius);
	const float radiusB = b.has_value() && b->radius2d.has_value() ? *b->radius2d : k_DualDefaultRadius;
	// 0x462082..0x4620B9: ((|A - B| in x / z + B's radius) + A's radius) x +0x28 + the larger height x 1.4
	const float dx = pointA.x - pointB.x;
	const float dz = pointA.z - pointB.z;
	const float apart = std::sqrt(dx * dx + dz * dz);
	const float distance = ((apart + radiusB) + radiusA) * mode.distanceFactor + maxHeight * k_DualHeightFactor;
	// 0x4620BF..0x462122: v = B - A; the heading is +0x20 less v's direction (fn_007FAA50: 0 when x^2 + z^2 <= 1e-6, else
	// fn_007FA990(-z, x)), or +0x20 itself when |v.x| and |v.z| are both <= 0.01
	const glm::vec3 v = pointB - pointA;
	float heading = mode.heading;
	if (static_cast<double>(std::abs(v.x)) > k_DualFlatEpsilon || static_cast<double>(std::abs(v.z)) > k_DualFlatEpsilon)
	{
		// call 0x7FAA50 (0x462117), its value still on the FPU stack: fsubr [esi + 0x20] (24 bits, 0x46211C); fstp 0x462122
		heading = static_cast<float>(static_cast<double>(mode.heading) - lh_matrix::GetYAngle(v));
	}
	// 0x462126..0x462318: SetPointFromPointDistanceHeadingAndPitch 0x442810 from the focus; the position heads for it
	// (0x407D60 on x and y, its inline copy on z)
	const auto position = PointFromDistanceHeadingAndPitch(focus, distance, heading, mode.pitch);
	state.position.SetDestinationWithTime(position, pace);
}

/// GCamera::CheckStackedModesForValidity 0x441D40 (once a turn, GGame::ProcessTurn 0x54E743) for the dual cameras:
/// CameraModeTwoObjects::IsStillValid 0x461D90 = +0x2C when +0x08 (and with +0x1C, +0x0C) is there and its IsAvailable
/// (vt +0x2C) is 1, else 0 -> deleted (vt+0 with 1, 0x441D86). A current one deleted -> the new current's Restart (vt
/// +0x10, 0x441DEB: nothing for CameraModeScript / CameraModeTwoObjects); the mode's seconds are not reset. The script
/// mode's own IsStillValid 0x4611D0 is +0x48, which End keeps. The last one gone over the player's mode hands the
/// zoomers back (HandBackAfterDual)
void CheckDualModes(State& state)
{
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	const bool drove = Drives();
	auto& duals = state.duals;
	for (auto it = duals.begin(); it != duals.end();) // from the bottom of the stack (0x441D57)
	{
		const bool things = available(it->a) && (!it->twoObjects || available(it->b));
		it = things && it->alive ? std::next(it) : duals.erase(it);
	}
	HandBackAfterDual(state, drove);
}
} // namespace

bool HasMode()
{
	const auto& state = Get();
	return state.scriptMode || !state.duals.empty();
}

bool ScriptModeCurrent()
{
	const auto& state = Get();
	return state.scriptMode && state.duals.empty();
}

bool DualCurrent()
{
	return !Get().duals.empty();
}

void StartDual(entt::entity a, entt::entity b, const Zoomer3d& origin, const Zoomer3d& focus)
{
	auto& state = Get();
	// 0x461BD5..0x461C1E: the current mode's +0x08 and +0x0C compared. (aproximado) a current point camera's +0x0C is
	// whatever new left there (fn_00461CB0 does not write it); here it is null
	if (!state.duals.empty() && state.duals.back().a == a && state.duals.back().b == b)
	{
		return;
	}
	DualMode mode;
	mode.a = a;                                 // +0x08 (0x461BC8)
	mode.b = b;                                 // +0x0C (0x461BCB)
	mode.twoObjects = true;                     // +0x1C (0x461BCE)
	mode.distanceFactor = k_DualDistanceFactor; // +0x28 (0x461C2C)
	PushDual(state, mode, origin, focus);       // +0x2C = 1, +0x20, +0x24 (0x461C25..0x461C41)
}

void StartDualWithPoint(entt::entity a, const glm::vec3& point, const Zoomer3d& origin, const Zoomer3d& focus)
{
	auto& state = Get();
	// 0x461CE9..0x461D54: the current mode's +0x08 and its point (fcomp ==, x, y, z) compared. (aproximado) a current
	// two things camera's +0x10 is whatever new left there; here (0, 0, 0)
	if (!state.duals.empty() && state.duals.back().a == a && state.duals.back().point == point)
	{
		return;
	}
	DualMode mode;
	mode.a = a;                                      // +0x08 (0x461CC0)
	mode.point = point;                              // +0x10 (0x461CCC..0x461CDF)
	mode.twoObjects = false;                         // +0x1C (0x461CE2)
	mode.distanceFactor = k_DualPointDistanceFactor; // +0x28 (0x461D62)
	PushDual(state, mode, origin, focus);
}

bool UpdateDual(entt::entity a, entt::entity b)
{
	auto& state = Get();
	if (state.duals.empty())
	{
		return false;
	}
	auto& mode = state.duals.back();
	mode.a = a;             // 0x461C98
	mode.b = b;             // 0x461C9B
	mode.twoObjects = true; // 0x461C9E
	return true;
}

bool ReleaseDual()
{
	auto& state = Get();
	if (state.duals.empty())
	{
		return false;
	}
	const bool drove = Drives();
	state.duals.back().alive = false; // Delete 0x461C50 (vt+0x30, 0x6ED44D)
	state.duals.pop_back();           // PopViewMode 0x441C50: Cleanup (vt+0x18, nothing), deleted, the index -1
	state.modeSeconds = 0.0f;         // 0x441C86
	HandBackAfterDual(state, drove);
	return true;
}

void Reset()
{
	auto& state = Get();
	DropPath(state);
	ResetFollow(state);
	state.scriptMode = false;
	state.modeSeconds = 0.0f;
	state.fov.SetPosition(k_DefaultFov); // GCamera ctor 0x441A78..0x441A83
	state.duals.clear();
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
	ReleaseDual(); // 0x6ECDB1: GScript::ReleaseDualCamera 0x6ED410 (one dual camera)
	// 0x6ECDD0..0x6ECE2E: only a current CameraModeScript is deleted; with another mode current (a second dual camera, or
	// the player's) "We are in the wrong camera mode! - excep" (0xC0C14C) and the script mode, if any, stays
	const bool wasScript = ScriptModeCurrent();
	if (!wasScript)
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "We are in the wrong camera mode! - excep");
		}
	}
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
	return HasMode() && !s_testCameraHook && !help::script_control::IsFreeStartTask(control, control.owner);
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

void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch)
{
	const glm::vec3 v = a - b; // 0x4428D3..0x4428F3
	if (std::abs(static_cast<double>(v.x)) < k_VerticalEpsilon && std::abs(static_cast<double>(v.z)) < k_VerticalEpsilon)
	{
		heading = 0.0f;          // 0x442925
		pitch = k_VerticalPitch; // 0x44292B: 0x3FC50A6B
		return;
	}
	// fn_007FAA50 (the symbols call it SetUnitDirectionVectorFromScreenPoint), call 0x44293A: 0 when x^2 + z^2 <= 1e-6,
	// else fn_007FA990(-z, x); its value still on the FPU stack: fsubr [0x8C36A0] (24 bits, 0x44293F); fstp 0x44294D
	heading = static_cast<float>(static_cast<double>(k_Pi) - lh_matrix::GetYAngle(v));
	// 0x442950..0x442976: fn_007FA990(sqrt(z^2 + x^2), y) (call 0x44296D), fstp dword 0x442976
	pitch = static_cast<float>(lh_matrix::ArcTanOctant(std::sqrt(v.z * v.z + v.x * v.x), v.y));
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
	CheckDualModes(state); // GCamera::CheckStackedModesForValidity 0x441D40 comes just before (0x54E743)
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
	// A current dual camera answers CameraMode::Arrived 0x441700 (vtable 0x8C7DD0 +0x34), path or not
	if (state.duals.empty() && state.track != nullptr) // CameraModeScript::Arrived 0x461B40
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
	if (!state.duals.empty())
	{
		UpdateDualMode(state, state.duals.back()); // vt+0x08 of the current mode: CameraModeTwoObjects::Update 0x461DE0
	}
	else if (state.scriptMode)
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

void ApplyShake(Camera& camera, const glm::vec3& lastDrawn)
{
	// LH3DTech::UpdateCamera 0x819920 (GCamera::Update 0x442622, outside the citadel and not playing back), whatever the
	// mode: fn_008210C0 shakes the drawn camera only, from the camera drawn the frame before (g_camera); GCamera's
	// zoomers keep their values. openblack: Camera::SetDrawOffset, which GetOrigin/GetFocus(Current) and the view add
	auto position = camera.GetOriginZoomer().GetCurrentValue();
	auto focus = camera.GetFocusZoomer().GetCurrentValue();
	const bool insideCitadel = game_clock::IsInsideCitadel(); // g_game +0x205A28 == 1
	if (insideCitadel)
	{
		camera.SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	}
	else
	{
		const auto base = position;
		const auto baseFocus = focus;
		camera_shake::Adjust(lastDrawn, position, focus);
		camera.SetDrawOffset(position - base, focus - baseFocus); // 0 when no shake moved it (the offset stays otherwise)
	}
	// LH3DRender::StartFrame 0x82F270 -> fn_00821270 with g_delta_time, once a drawn frame (inferido: after the camera,
	// GGame::ProcessGraphicsEngine 0x54D879 runs before the draw)
	camera_shake::Tick(game_clock::FrameRealMs());
}

bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	Frame(cameraSeconds, gameMs, gameSeconds);

	// 0x442337..0x4423F4: inside the citadel (g_game+0x205A28 == 1) the drawn camera stays
	const bool insideCitadel = game_clock::IsInsideCitadel();
	const bool drive = Drives() && !insideCitadel;
	if (drive)
	{
		auto drawn = DrawnCamera([](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z))
			                                           : -1e30f;
		});
		// LH3DTech::UpdateCamera 0x819920 (GCamera::Update 0x442622, outside the citadel): the shake moves the drawn
		// camera only, from the camera drawn the frame before (g_camera); the zoomers keep their values
		camera_shake::Adjust(camera.GetOrigin(), drawn.origin, drawn.focus);
		camera.SetOrigin(drawn.origin);
		camera.SetFocus(drawn.focus);
	}
	// LH3DRender::StartFrame 0x82F270 -> fn_00821270 with g_delta_time, once a drawn frame (inferido: after the camera,
	// GGame::ProcessGraphicsEngine 0x54D879 runs before the draw). (aproximado) the player's camera is not shaken yet
	camera_shake::Tick(game_clock::FrameRealMs());

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
