/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCamera.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/CameraTrack.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "Blast/CameraShake.h"
#include "Camera.h"
#include "Common/GameAngle.h"
#include "CreatureCameraModel.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyPlacement.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/LastDrawnMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreaturePose.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraStackInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::script_camera
{

using camera_stack::DualMode;
using camera_stack::ModeStack;
using camera_stack::StackedMode;

State::State()
{
	fov.SetPosition(k_DefaultFov);
}

State::~State() = default;

State& Get()
{
	return Locator::scriptState::value().Get<State>();
}

namespace
{
/// The camera's stack of modes and the models kept under the new player's modes (Locator::cameraStack)
ecs::systems::CameraStackInterface& TheStack()
{
	if (!Locator::cameraStack::has_value())
	{
		std::fputs("game: no camera stack in the locator (Locator::cameraStack)\n", stderr);
		std::abort();
	}
	return Locator::cameraStack::value();
}
} // namespace

ModeStack& Modes()
{
	return TheStack().Modes();
}

namespace
{
/// The tests' thing reader (SetThingReaderForTests; debugHooks)
struct TestReaderState
{
	ThingReader reader;
};

ThingReader& TestReader()
{
	return Locator::debugHooks::value().Get<TestReaderState>().reader;
}

/// The game angle of a wall hugger in openblack: an animal's AnimalBrain::angle is the angle itself; a villager keeps
/// it in WallHug::yAngle as ConvertGameAngleTo3D(a) (ECS/Villager/VillagerCore.h), turned back here (approximate:
/// rounded to the nearest 2048th). A creature's is not kept (no creature body nor AI): none (approximate)
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
			const auto turns = std::lround(wallHug->yAngle * static_cast<float>(game_math::k_GameAngleCircle) / k_TwoPi);
			return static_cast<uint16_t>(static_cast<uint32_t>(turns) & static_cast<uint32_t>(game_math::k_GameAngleMask));
		}
	}
	return std::nullopt;
}

/// The thing as the original reads it, from the entities registry: a valid entity (as the script's things in
/// CHLApi.cpp), its availability ecs::IsAvailable; anything but a flock needs a Transform (its map coords). (inferred)
/// An entity with a Transform and a Mesh is an Object with a 3D object, whose matrix is the Transform
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
	const auto infoAvailable = ecs::IsAvailable(entity);
	const auto infoHeight = ecs::object::GetHeight(entity);
	ThingInfo info {
	    .height = infoHeight,
	    .available = infoAvailable,
	};
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
	{
		// A flock's position: the leader's map coords, else the flock's own (Flock::domainCentre). The leader is the
		// first member (ECS/AnimalAI.cpp)
		info.isFlock = true;
		const auto own = map_coords::ToWorld(map_coords::FromWorld(flock->domainCentre));
		info.mapPoint = own;
		info.flockPoint = own;
		if (!flock->members.empty() && registry.Valid(flock->members.front()))
		{
			const auto leader = flock->members.front();
			info.flockPoint = map_coords::ToWorld(ecs::object::MapCoordsOf(leader));
			info.leaderHeight = ecs::object::GetHeight(leader);
		}
		return info;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	info.mapPoint = map_coords::ToWorld(ecs::object::MapCoordsOf(entity));
	info.radius2d = ecs::object::Get2DRadius(entity); // read by the dual camera
	info.drawnPoint = DrawnPointOf(registry, entity);
	// A villager is not available while dying
	if (registry.AllOf<ecs::components::Villager>(entity) && ecs::villager::GetFinalState(entity) == VillagerStates::Dying)
	{
		info.available = false;
	}
	info.wallHugAngle = WallHugAngleOf(registry, entity);
	if (info.wallHugAngle.has_value())
	{
		info.facingDirection = game_math::GameAngleToPhysicsRadians(*info.wallHugAngle);
	}
	// A creature faces by its body's yaw: minus its heading once it has moved, else read from how its body is turned
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		info.facingDirection = CreatureFacing(ecs::creature_pose::BodyYaw(registry, entity, transform->rotation));
	}
	return info;
}

} // namespace

std::optional<glm::vec3> DrawnPointOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	// The physics' draw sets the 3D object's matrix of what it carries every frame, before anything reads it: a thing in
	// flight is out of the map cells, so no draw of its own leaves a matrix on it then (ECS/Physics: the frame's poses).
	// Not for what the hand holds, whose draw places it, nor for a creature, whose body placement is its matrix
	if (const auto* flight = registry.TryGet<const ecs::components::PhysicsDrawPose>(entity);
	    flight != nullptr && !registry.AnyOf<ecs::components::HandDrawPose, ecs::components::CreatureBodyPlacement>(entity))
	{
		return flight->position;
	}
	if (registry.AnyOf<ecs::components::LastDrawnMatrix, ecs::components::CreatureBodyPlacement>(entity))
	{
		// a villager's, an animal's or a creature's matrix as its last draw left it (ECS/ObjectPlacement.h)
		return glm::vec3(ecs::object_placement::Cells(registry, entity)[3]);
	}
	if (const auto* drawn = registry.TryGet<const ecs::components::DrawPosition>(entity); drawn != nullptr)
	{
		return drawn->position; // not drawn yet: where it is drawn this frame (ECS/MobileDrawing.h)
	}
	if (registry.AllOf<ecs::components::Mesh>(entity))
	{
		return transform->position;
	}
	return std::nullopt;
}

namespace
{
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
	// The scripted camera path freed
	state.positionRunner.reset();
	state.track.reset();
}

/// No position thing; with "behind" the heading is 0
void StopPositionFollow(State& state)
{
	state.positionThing = entt::null;
	if (state.behind)
	{
		state.heading = 0.0f;
	}
}

/// The focus thing, else the position thing
entt::entity FocusThingOf(const State& state)
{
	return state.focusThing != entt::null ? state.focusThing : state.positionThing;
}

/// The follow's script part of the mode, as the constructors leave it
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

/// Where the position follows a thing to: PointFromDistanceHeadingAndPitch(point, distance, heading, pitch) with the
/// pitch clamped and kept
glm::vec3 PositionFor(State& state, const ThingInfo& thing, bool update)
{
	const auto point = FollowPoint(thing, update);
	state.pitch = FollowPitch(state.pitch);
	const float heading = FollowHeading(state.heading, state.behind, thing.wallHugAngle);
	return PointFromDistanceHeadingAndPitch(point, state.distance, heading, state.pitch);
}

/// The follow update for the script mode: zoom to town and the keys are off, so neither the zoom branch nor the keys
/// run, and it leaves after the inclusion check, whose answer it does not use
void UpdateFollow(State& state)
{
	state.distance = ClampFollowDistance(state.distance);
	const float seconds = FollowSeconds(state.modeSeconds, state.timeFactor);
	// A creature focus thing writes some globals (1 - life, parts of its body) read elsewhere: not ported
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetDestinationWithTime(FollowPoint(*focus, true), seconds);
	}
	// else the computer player's hand as the focus, not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetDestinationWithTime(PositionFor(state, *thing, true), seconds);
	}
	// else from the computer player's hand, not ported
}

/// The other systems' modes a push onto a full stack deleted from the bottom, the lowest entry first (the stack's
/// OtherEntries, in the order they were pushed), each through its own system, as the original deletes the bottom mode
/// whatever it is: Creature Mode deletes that follow (its camera's model, if the camera has it, giving way to the one
/// under it), a placed path goes with nothing restarted
void DeleteDroppedOtherModes()
{
	for (auto drops = Modes().TakeOtherSystemsDrops(); drops > 0; --drops)
	{
		const auto lowest = Modes().LowestOther();
		if (!lowest.has_value())
		{
			continue;
		}
		if (lowest->kind == StackedMode::Kind::Follow && Locator::creatureModeSystem::has_value())
		{
			Locator::creatureModeSystem::value().DeleteFollow(lowest->follow);
		}
		else if (lowest->kind == StackedMode::Kind::Path && Locator::cameraPathSystem::has_value())
		{
			Locator::cameraPathSystem::value().DeleteLowest();
		}
		else if (lowest->kind == StackedMode::Kind::Dance)
		{
			// the dance camera has no owner of its own: its entry goes
			std::ignore = Modes().RemoveDance(lowest->dance);
		}
	}
}

/// The kept models no player's mode keeps any more (popped, or deleted from the bottom of a full stack) go
void DropLostModels()
{
	TheStack().DropLost();
}

/// A new dual camera pushed as the current mode
void PushDual(State& state, const DualMode& mode, const Zoomer3& origin, const Zoomer3& focus)
{
	if (Modes().OverAnotherCamera())
	{
		// Over the player's mode or the creature interaction's camera: the camera's zoomers are the same for every mode, so
		// the dual camera goes on from wherever that camera's were heading (as BeginFrom)
		state.position = origin;
		state.focus = focus;
	}
	Modes().Push({.kind = StackedMode::Kind::Dual, .dual = mode}, Modes().OtherCount());
	DeleteDroppedOtherModes();
	DropLostModels();
	state.modeSeconds = 0.0f;
}

/// The last dual camera released with the player's mode under it: the player's mode is current
/// again and restarted. The camera's zoomers are the same for every mode, so they are handed back to the player's camera
/// as the dual camera left them (as End); the player's mode then starts again, keeping a fight it watched, and flies back
/// from there to the view it had before (CameraModel::Resume), its help icons by the hand faded out. When a full stack
/// had deleted the player's mode, the stack is empty: a new player's mode starts from the camera as it is, as at the end
/// of a script's camera, its mode's seconds from 0. Only when this module drove the camera (`drove`: not with the camera
/// test hooks)
void HandBackAfterDual(State& state, bool drove)
{
	if (!drove || !Modes().OverAnotherCamera() || !Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	// Over the creature interaction's camera, that camera goes on from the zoomers as they are: its restart does nothing
	if (!Modes().PlayersModeCurrent())
	{
		return;
	}
	// A placed path under it has the camera again: its restart only sets its close near plane again, which the frame's
	// camera update does, so the player's camera is not started again
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera())
	{
		return;
	}
	// A follow under it goes on following from the camera as it is, its mode's time from 0; the player's mode flies back
	// to the view it had
	if (auto* follow = dynamic_cast<CreatureCameraModel*>(&camera.GetModel()); follow != nullptr)
	{
		follow->Restart();
		return;
	}
	if (Modes().NewPlayersModeIfGone(Modes().OtherCount()))
	{
		state.modeSeconds = 0.0f;
		camera.GetModel().Restart();
	}
	else
	{
		camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
	}
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
}

/// The dual camera's update, once a frame while it is the current mode
void UpdateDualMode(State& state, DualMode& mode)
{
	// The seconds of the destinations, with no time factor
	const float pace =
	    state.modeSeconds > k_DualSettleSeconds ? 1.0f : state.modeSeconds / k_DualSettleSeconds * (1.0f - 2.0f) + 2.0f;
	// (approximate) the original reads a and b with no check (a null thing would crash it before the turn's validity
	// check drops the mode); here a thing that cannot be read leaves the zoomers alone this frame
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
	// The map coords points (x, z x 1/6553.6, the land's altitude + the altitude above it; not the 3D object nor a
	// flock's position); B is the point without the altitude above the land
	const glm::vec3 pointA = a->mapPoint;
	const glm::vec3 pointB = b.has_value() ? b->mapPoint : mode.point;
	// (B + A) x 0.5
	const glm::vec3 middle((pointB.x + pointA.x) * 0.5f, (pointB.y + pointA.y) * 0.5f, (pointB.z + pointA.z) * 0.5f);
	// (A's height + B's, 1 without B) x 0.5
	const float meanHeight = (a->height + (b.has_value() ? b->height : k_DualPointHeight)) * 0.5f;
	// The larger of A's height and B's (0 without B; A only when strictly larger)
	const float otherHeight = b.has_value() ? b->height : 0.0f;
	const float maxHeight = a->height > otherHeight ? a->height : otherHeight;
	// The focus heads for the middle raised by half the mean height (speed 0)
	const glm::vec3 focus(middle.x, meanHeight * 0.5f + middle.y, middle.z);
	state.focus.SetDestinationWithTime(focus, pace);
	mode.pitch = FollowPitch(mode.pitch); // at least k_FollowMinPitch, stored back
	// An Object's 2D radius, else 30
	const float radiusA = a->radius2d.value_or(k_DualDefaultRadius);
	const float radiusB = b.has_value() && b->radius2d.has_value() ? *b->radius2d : k_DualDefaultRadius;
	// ((|A - B| in x / z + B's radius) + A's radius) x the distance factor + the larger height x 1.4
	const float dx = pointA.x - pointB.x;
	const float dz = pointA.z - pointB.z;
	const float apart = std::sqrt(dx * dx + dz * dz);
	const float distance = ((apart + radiusB) + radiusA) * mode.distanceFactor + maxHeight * k_DualHeightFactor;
	// v = B - A; the heading is the mode's heading less v's direction (affine::GetYAngle: 0 when x^2 + z^2 <= 1e-6,
	// else affine::ArcTanOctant(-z, x)), or the mode's heading itself when |v.x| and |v.z| are both <= 0.01
	const glm::vec3 v = pointB - pointA;
	float heading = mode.heading;
	if (static_cast<double>(std::abs(v.x)) > k_DualFlatEpsilon || static_cast<double>(std::abs(v.z)) > k_DualFlatEpsilon)
	{
		// Subtracted in double precision, then rounded to float
		heading = static_cast<float>(static_cast<double>(mode.heading) - affine::GetYAngle(v));
	}
	// The position heads for the point at that distance, heading and pitch from the focus
	const auto position = PointFromDistanceHeadingAndPitch(focus, distance, heading, mode.pitch);
	state.position.SetDestinationWithTime(position, pace);
}

/// The stacked modes' validity check (once a turn) for the dual cameras: one stays while it is alive and a (and, with
/// two things, b) is there and available, else it is deleted, wherever it is on the stack. The script mode's own check
/// is its alive flag, which holds while it is on the stack. Only the current one is cleaned up: when it drove the camera
/// and the camera's other modes are under it, the zoomers, the same for every mode, are handed back to the camera as it
/// left them (as End). Nothing is restarted here; the turn's checks restart whatever is current then
/// (camera_checks::CheckModes). True when the current mode was among those deleted
bool CheckDualModes()
{
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	const bool drove = Drives();
	const bool current = Modes().RemoveInvalid([&available](const DualMode& dual) {
		return dual.alive && available(dual.a) && (!dual.twoObjects || available(dual.b));
	});
	if (current && drove && Modes().OverAnotherCamera() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	return current;
}
} // namespace

std::optional<ThingInfo> Thing(entt::entity thing)
{
	return ReadThing(thing);
}

bool HasMode()
{
	return Modes().ScriptCurrent() || Modes().DualCurrent();
}

bool ScriptModeCurrent()
{
	return Modes().ScriptCurrent();
}

bool DualCurrent()
{
	return Modes().DualCurrent();
}

void StartDual(entt::entity a, entt::entity b, const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	// The current mode's a and b compared. (approximate) a current point camera's b is whatever the allocation left
	// there (it is not written); here it is null
	if (const auto* current = Modes().CurrentDual(); current != nullptr && current->a == a && current->b == b)
	{
		return;
	}
	DualMode mode {
	    .a = a,
	    .b = b,
	    .twoObjects = true,
	    .distanceFactor = camera_stack::k_DualDistanceFactor,
	};
	PushDual(state, mode, origin, focus);
}

void StartDualWithPoint(entt::entity a, const glm::vec3& point, const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	// The current mode's a and its point (x, y, z equal) compared. (approximate) a current two things camera's point is
	// whatever the allocation left there; here (0, 0, 0)
	if (const auto* current = Modes().CurrentDual(); current != nullptr && current->a == a && current->point == point)
	{
		return;
	}
	DualMode mode {
	    .a = a,
	    .point = point,
	    .twoObjects = false,
	    .distanceFactor = k_DualPointDistanceFactor,
	};
	PushDual(state, mode, origin, focus);
}

bool UpdateDual(entt::entity a, entt::entity b)
{
	if (!DualCurrent())
	{
		return false;
	}
	auto& mode = *Modes().CurrentDual();
	mode.a = a;
	mode.b = b;
	mode.twoObjects = true;
	return true;
}

bool ReleaseDual()
{
	auto& state = Get();
	if (!DualCurrent())
	{
		return false;
	}
	const bool drove = Drives();
	Modes().PopDual(); // deleted and popped: the mode under it is current again
	state.modeSeconds = 0.0f;
	HandBackAfterDual(state, drove);
	return true;
}

bool NewPlayersModeIfGone()
{
	return Modes().NewPlayersModeIfGone(Modes().OtherCount());
}

bool StartNewPlayersModeIfGone()
{
	if (!Locator::camera::has_value() || !NewPlayersModeIfGone())
	{
		return false;
	}
	Get().modeSeconds = 0.0f;
	Locator::camera::value().GetModel().Restart();
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
	return true;
}

void ClearModes()
{
	auto& state = Get();
	DropPath(state);
	ResetFollow(state);
	TheStack().Clear();
	state.modeSeconds = 0.0f;
}

void Reset()
{
	ClearModes();
	Get().fov.SetPosition(k_DefaultFov);
}

void ModeChanged()
{
	Get().modeSeconds = 0.0f;
}

void Cover(Zoomer3& origin, Zoomer3& focus)
{
	if (Drives())
	{
		HandBack(origin, focus);
	}
	Modes().Push({.kind = StackedMode::Kind::Interaction}, Modes().OtherCount());
	DeleteDroppedOtherModes();
	DropLostModels();
}

ModeStack::Popped Uncover(const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	const auto popped = Modes().PopInteraction(Modes().OtherCount());
	if (popped == ModeStack::Popped::None)
	{
		return popped;
	}
	if (popped == ModeStack::Popped::NewPlayersMode)
	{
		state.modeSeconds = 0.0f;
	}
	if (Drives())
	{
		state.position = origin;
		state.focus = focus;
	}
	return popped;
}

bool Covered()
{
	return Modes().InteractionOnStack();
}

bool InteractionCurrent()
{
	return Modes().InteractionCurrent();
}

bool PlayerOverInteraction()
{
	const auto& modes = Modes();
	return modes.PlayerCurrent() && modes.InteractionOnStack();
}

bool TakeOffPlayersModeOverInteraction(const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	const auto id = Modes().PlayerCurrent() ? Modes().Current()->id : 0U;
	if (!Modes().PopPlayer())
	{
		return false;
	}
	DropLostModels();
	// the paths it went over are on the stack again, under the interaction's camera
	Modes().UncoverPaths(id);
	state.modeSeconds = 0.0f;
	if (Drives())
	{
		state.position = origin;
		state.focus = focus;
	}
	return true;
}

ModeStack::Popped TakeOffScriptMode(Zoomer3& origin, Zoomer3& focus)
{
	if (!ScriptModeCurrent())
	{
		return ModeStack::Popped::None;
	}
	if (Drives())
	{
		HandBack(origin, focus);
	}
	auto& state = Get();
	// the mode under it, if one of this module's, is current again; or a new player's mode on an emptied stack
	const auto popped = Modes().PopScript(Modes().OtherCount());
	DropPath(state);
	ResetFollow(state);
	state.modeSeconds = 0.0f;
	// A follow under it, current again, goes on following from the camera as it is, its time in the mode from 0
	if (auto* follow =
	        Locator::camera::has_value() ? dynamic_cast<CreatureCameraModel*>(&Locator::camera::value().GetModel()) : nullptr;
	    follow != nullptr && Modes().FollowCurrent())
	{
		follow->Restart();
	}
	return popped;
}

bool Begin(const glm::vec3& origin, const glm::vec3& focus)
{
	auto& state = Get();
	// A current script mode cannot be left, nor the creature interaction's camera; the player's mode and a dual camera can
	if (!Modes().CanLeaveCurrent())
	{
		return false;
	}
	// Over a dual camera the camera's zoomers are this module's already
	const bool overPlayer = Modes().PlayersModeCurrent();
	DropPath(state);
	ResetFollow(state);
	// A follow it goes over stays under it on the stack, with its model, and is not updated while it is not current
	Modes().Push({.kind = StackedMode::Kind::Script}, Modes().OtherCount());
	DeleteDroppedOtherModes();
	DropLostModels();
	state.modeSeconds = 0.0f; // a new mode
	if (overPlayer)
	{
		state.position.SetPosition(origin);
		state.focus.SetPosition(focus);
	}
	return true;
}

bool BeginFrom(const Zoomer3& origin, const Zoomer3& focus)
{
	const bool overPlayer = Modes().PlayersModeCurrent();
	if (!Begin(origin.GetCurrentValue(), focus.GetCurrentValue()))
	{
		return false;
	}
	if (overPlayer)
	{
		auto& state = Get();
		state.position = origin; // the same zoomers, still heading where they were
		state.focus = focus;
	}
	return true;
}

void HandBack(Zoomer3& origin, Zoomer3& focus)
{
	const auto& state = Get();
	origin = state.position;
	focus = state.focus;
}

bool End()
{
	auto& state = Get();
	ReleaseDual(); // one dual camera
	// Only a current script mode is deleted; with another mode current (a second dual camera, or the player's) "We are
	// in the wrong camera mode! - excep" and the script mode, if any, stays
	const bool wasScript = ScriptModeCurrent();
	if (!wasScript)
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "We are in the wrong camera mode! - excep");
		}
	}
	// The player's mode starts from the camera's zoomers. Only when the script mode drove the camera (with the camera
	// test hooks the player kept it)
	if (wasScript && Drives() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	if (wasScript) // deleted, and a new player's mode on top of the whole stack
	{
		// The player's mode the script's camera went over stays under the new one with its own view: the camera's model
		// as it was is kept for it. Over the creature interaction's camera, the creature hand makes the new player's
		// camera itself and keeps the interaction's
		std::unique_ptr<CameraModel> under;
		if (!Modes().InteractionOnStack() && Locator::camera::has_value())
		{
			under = Locator::camera::value().GetModel().KeptUnder();
		}
		const auto id = Modes().EndScript(under != nullptr, Modes().OtherCount());
		DeleteDroppedOtherModes();
		if (under != nullptr && Modes().KeepsModel(id))
		{
			TheStack().Keep(id, std::move(under));
		}
		// A follow the script's camera went over, whose model the camera still has, stays under the new mode with its
		// model, and the new mode is a new player's camera; the new mode's pop gives the follow the camera again
		else if (under == nullptr && id != 0 && Locator::creatureModeSystem::has_value())
		{
			std::ignore = Locator::creatureModeSystem::value().KeepUnderPlayersMode(id);
		}
		DropLostModels();
		DropPath(state);
		ResetFollow(state);
		state.modeSeconds = 0.0f; // the new player's mode
		// which goes over any miracle's path the script's camera covered: the path has the camera again only if the new
		// mode is popped
		if (Modes().CoverPaths(id))
		{
			if (const auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_DEBUG(logger, "Camera: a new player's camera went over the miracles' paths");
			}
		}
		// which starts again from the camera as it is: nothing held, nothing on its way and no fight watched
		if (Locator::camera::has_value())
		{
			Locator::camera::value().GetModel().Restart();
		}
		// and its help icons by the hand, faded out with the rotate arrow unturned
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
	}
	SetFov(help::script_control::k_ScriptEndFov, help::script_control::k_ScriptEndFovTime);
	return wasScript;
}

bool PopPlayersMode()
{
	auto& state = Get();
	if (!Locator::camera::has_value())
	{
		return false;
	}
	const auto popped = Modes().PopPlayersMode();
	if (!popped.has_value())
	{
		return false;
	}
	auto& camera = Locator::camera::value();
	state.modeSeconds = 0.0f;
	// The player's mode under it, current or not, has its own model back; or Creature Mode the follow it went over
	auto under = TheStack().TakeKept(popped->id);
	bool kept = under != nullptr;
	if (kept)
	{
		std::ignore = camera.SetModel(std::move(under));
	}
	else if (popped->id != 0 && Locator::creatureModeSystem::has_value())
	{
		// the follow it went over (its place left the camera fence, C let go of a follow over it, or a script's camera over
		// it ended)
		kept = Locator::creatureModeSystem::value().TakeBackFromUnderPlayersCamera(popped->id);
	}
	DropLostModels();
	// the paths it went over are on the stack again
	Modes().UncoverPaths(popped->id);
	const bool pathHasCamera = Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera();
	// A new player's mode that kept nothing still has the bottom one under it while that one is there
	const bool playersModeUnder = kept || (popped->id != 0 && !Modes().PlayersModeGone());
	switch (AfterPopOf(!Modes().PlayersModeCurrent(), pathHasCamera, playersModeUnder))
	{
	case AfterPop::ModeUnder:
		// A dual camera current again goes on from the camera's zoomers as they are
		if (Drives())
		{
			state.position = camera.GetOriginZoomer();
			state.focus = camera.GetFocusZoomer();
		}
		break;
	case AfterPop::PlayersModeUnder:
		// Creature Mode's follow goes on following from the camera as it is; the player's mode flies back to its view
		if (auto* follow = dynamic_cast<CreatureCameraModel*>(&camera.GetModel()); follow != nullptr)
		{
			follow->Restart();
		}
		else
		{
			camera.GetModel().Resume(camera.GetOriginZoomer().GetCurrentValue());
		}
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
		break;
	case AfterPop::NewPlayersMode:
		std::ignore = Modes().NewPlayersModeIfGone(Modes().OtherCount());
		camera.GetModel().Restart();
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
		break;
	}
	return true;
}

uint32_t PushPlayersMode()
{
	auto& state = Get();
	std::unique_ptr<CameraModel> under;
	if (Locator::camera::has_value())
	{
		under = Locator::camera::value().GetModel().KeptUnder();
	}
	// Only the player's own camera starts again as the new mode: another model (Creature Mode's follow) goes on as it is
	const bool players = under != nullptr;
	const auto id = Modes().PushPlayer(players, Modes().OtherCount());
	DeleteDroppedOtherModes();
	if (players && Modes().KeepsModel(id))
	{
		TheStack().Keep(id, std::move(under));
	}
	DropLostModels();
	state.modeSeconds = 0.0f;
	if (players)
	{
		Locator::camera::value().GetModel().Restart();
		ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
	}
	return id;
}

uint32_t PushPlayersModeOver()
{
	auto& state = Get();
	const auto id = Modes().PushPlayer(false, Modes().OtherCount());
	DeleteDroppedOtherModes();
	DropLostModels();
	state.modeSeconds = 0.0f;
	ecs::events::Publish(ecs::events::PlayerCameraRestarted {});
	return id;
}

bool HasPlayersMode(uint32_t id)
{
	return Modes().HasPlayer(id);
}

void KeepUnder(uint32_t id, std::unique_ptr<CameraModel> model)
{
	if (model != nullptr && Modes().KeepUnder(id))
	{
		TheStack().DropKept(id);
		TheStack().Keep(id, std::move(model));
	}
}

bool Active()
{
	return Modes().ScriptOnStack();
}

bool Drives()
{
	// Test hooks (not original): OPENBLACK_CAMERA_LOCK / OPENBLACK_CAMERA_FLY hold the camera every turn for the team's
	// screenshots (Worship/WorshipDebugHooks.cpp), so the script mode leaves it to them
	static const bool s_testCameraHook =
	    std::getenv("OPENBLACK_CAMERA_LOCK") != nullptr || std::getenv("OPENBLACK_CAMERA_FLY") != nullptr;
	return HasMode() && !s_testCameraHook;
}

void HandOverToAnotherMode()
{
	if (Drives() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
}

void DeleteDroppedModes()
{
	DeleteDroppedOtherModes();
	DropLostModels();
}

void TakeBackFromAnotherMode()
{
	if (Drives() && Locator::camera::has_value())
	{
		auto& state = Get();
		auto& camera = Locator::camera::value();
		state.position = camera.GetOriginZoomer();
		state.focus = camera.GetFocusZoomer();
	}
}

void SetPosition(const glm::vec3& position)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state);
	state.position.SetPosition(position);
}

void SetFocus(const glm::vec3& focus)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null;
	state.focus.SetPosition(focus);
}

void MovePosition(const glm::vec3& position, float seconds)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state);
	state.position.SetDestinationWithTime(position, seconds);
}

void MoveFocus(const glm::vec3& focus, float seconds)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null;
	state.focus.SetDestinationWithTime(focus, seconds);
}

bool DrawnThereAlready(const glm::vec3& drawn, const glm::vec3& asked)
{
	// each part compared as the game compares floats: one that is not a number reads as equal
	const auto same = [](float a, float b) { return !(a < b) && !(a > b); };
	return same(drawn.x, asked.x) && same(drawn.y, asked.y) && same(drawn.z, asked.z);
}

void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus)
{
	// Nothing is set when the camera is drawn there already, position and focus: a glide under way goes on
	if (Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		if (DrawnThereAlready(camera.GetDrawnOrigin(), position) && DrawnThereAlready(camera.GetDrawnFocus(), focus))
		{
			return;
		}
	}
	auto& state = Get();
	state.position.SetPosition(position);
	state.focus.SetPosition(focus);
}

void RunPath(int32_t number)
{
	auto& state = Get();
	// The old path and the focus follow dropped; the position follow kept
	DropPath(state);
	state.focusThing = entt::null;
	state.track = camera_edits::FindTrack(number); // "Cannot load track No %d" when missing
	if (state.track != nullptr)
	{
		state.positionRunner = std::make_unique<camera_track::WayRunner>(state.track->position);
	}
	state.pathMs = 0;
}

void SetFov(float fov, float seconds)
{
	auto& state = Get();
	// t == 0 or t < 0.001 sets it; else the quartic towards fov with speed 0
	state.fov.SetDestinationWithSpeedAndTime(fov, 0.0f, seconds);
}

// ---- Follow mode -----------------------------------------------------------------------------------------------------

float FollowSeconds(float modeSeconds, float timeFactor)
{
	// <= 2 s takes the slow start
	const float pace = modeSeconds > k_FollowSettleSeconds ? 1.0f : modeSeconds / k_FollowSettleSeconds * (1.0f - 2.0f) + 2.0f;
	return pace * timeFactor;
}

float ClampFollowDistance(float distance)
{
	if (!(distance > k_FollowMinDistance))
	{
		return k_FollowMinDistance;
	}
	return distance < k_FollowMaxDistance ? distance : k_FollowMaxDistance;
}

float FollowPitch(float pitch)
{
	return pitch > k_FollowMinPitch ? pitch : k_FollowMinPitch;
}

float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle)
{
	if (!behind || !wallHugAngle.has_value())
	{
		return heading;
	}
	// The doubled game angle in radians, less a quarter turn
	const float angle = static_cast<float>(static_cast<uint32_t>(*wallHugAngle) << 1u) * k_FollowAngleScale - k_HalfPi;
	return heading - angle;
}

float ThingViewingDistance(float height)
{
	return height * k_ThingViewingDistanceFactor;
}

glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch)
{
	// The sines and cosines unrounded, only cos q kept as a float; each product and sum then rounded to a float, in order
	const auto cosPitch = static_cast<float>(std::cos(static_cast<double>(pitch)));
	const float z = static_cast<float>(std::cos(static_cast<double>(heading)) * cosPitch) * distance + p.z;
	const float y = static_cast<float>(std::sin(static_cast<double>(pitch)) * distance) + p.y;
	const float x = static_cast<float>(std::sin(static_cast<double>(heading)) * cosPitch) * distance + p.x;
	return {x, y, z};
}

bool StraightAbove(const glm::vec3& a, const glm::vec3& b)
{
	const glm::vec3 v = a - b;
	return std::abs(static_cast<double>(v.x)) < k_VerticalEpsilon && std::abs(static_cast<double>(v.z)) < k_VerticalEpsilon;
}

void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch)
{
	const glm::vec3 v = a - b;
	if (StraightAbove(a, b))
	{
		heading = 0.0f;
		pitch = k_VerticalPitch;
		return;
	}
	// affine::GetYAngle: 0 when x^2 + z^2 <= 1e-6, else affine::ArcTanOctant(-z, x); subtracted from pi in double
	// precision, then rounded to float
	heading = static_cast<float>(static_cast<double>(k_Pi) - affine::GetYAngle(v));
	// affine::ArcTanOctant(sqrt(z^2 + x^2), y), rounded to float
	pitch = static_cast<float>(affine::ArcTanOctant(std::sqrt(v.z * v.z + v.x * v.x), v.y));
}

glm::vec3 FollowPoint(const ThingInfo& thing, bool update)
{
	if (thing.isFlock)
	{
		auto point = thing.flockPoint; // the land's altitude + its altitude above it
		if (thing.leaderHeight.has_value())
		{
			point.y = *thing.leaderHeight * 0.5f + point.y;
		}
		return point;
	}
	auto point = thing.mapPoint;
	if (update && thing.drawnPoint.has_value())
	{
		point = *thing.drawnPoint; // the 3D object's position
	}
	point.y = thing.height * 0.5f + point.y;
	return point;
}

glm::vec3 FacePoint(const ThingInfo& thing)
{
	auto point = thing.mapPoint;
	point.y = thing.height * 0.5f + point.y;
	return point;
}

float CreatureFacing(float bodyYaw)
{
	// each step rounds to a float, as the game's arithmetic does
	const float turnedOnce = bodyYaw + k_TwoPi;
	return turnedOnce - k_CreatureFacingOffset;
}

glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance)
{
	float heading = facing;
	if (!(heading < k_FaceMaxHeading))
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Invalid heading"); // and on
		}
	}
	while (heading > k_TwoPi)
	{
		heading -= k_TwoPi;
	}
	return PointFromDistanceHeadingAndPitch(focus, distance, heading, k_FacePitch);
}

void FocusFollow(entt::entity thing)
{
	auto& state = Get();
	DropPath(state);
	// An available thing is followed, else nothing
	const auto info = ReadThing(thing);
	state.focusThing = info.has_value() && info->available ? thing : entt::null;
}

void PositionFollow(entt::entity thing)
{
	auto& state = Get();
	state.positionThing = thing;
	if (thing != entt::null)
	{
		// From the zoomers' destinations, the position from the focus
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		const auto info = ReadThing(thing);
		state.distance = ThingViewingDistance(info.has_value() ? info->height : 0.0f);
	}
	if (state.behind)
	{
		state.heading = 0.0f;
	}
}

void FocusAndPositionFollow(entt::entity thing, float distance)
{
	auto& state = Get();
	state.positionThing = thing;
	if (thing != entt::null)
	{
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		state.distance = distance;
	}
}

void PlaceFollowNow()
{
	auto& state = Get();
	// No zoom: the follow's points, set at once
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetPosition(FollowPoint(*focus, false));
	}
	// else the computer player's hand, not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetPosition(PositionFor(state, *thing, false)); // no distance clamp
	}
	// else the computer player's hand, not ported
	state.modeSeconds = k_FollowSettleSeconds;
}

void SetFollowProperties(float distance, float timeFactor, float heading, bool behind)
{
	auto& state = Get();
	state.distance = distance;
	state.timeFactor = timeFactor;
	state.behind = behind;
	state.heading = heading;
}

bool Validate()
{
	auto& state = Get();
	const bool current = CheckDualModes(); // the stacked modes' check comes just before
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	if (state.focusThing != entt::null && !available(state.focusThing))
	{
		state.focusThing = entt::null;
	}
	if (state.positionThing != entt::null && !available(state.positionThing))
	{
		state.positionThing = entt::null;
	}
	return current;
}

std::optional<FacePoints> FaceObject(entt::entity thing, float distance)
{
	const auto info = ReadThing(thing);
	if (!info.has_value())
	{
		// "no object to face", then the original reads through the null thing (inferred: it would crash; here nothing
		// is done)
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "no object to face");
		}
		return std::nullopt;
	}
	FacePoints points {};
	points.focus = FacePoint(*info);
	points.position = FacePosition(points.focus, info->facingDirection, distance);
	return points;
}

bool ScriptArrived()
{
	const auto& state = Get();
	// A current dual camera answers with the zoomers' arrival, path or not
	if (!Modes().DualCurrent() && state.track != nullptr)
	{
		return state.track->position.duration <= state.pathMs;
	}
	// Both zoomers at their destinations
	const auto dp = state.position.GetCurrentValue() - state.position.GetDestination();
	const auto df = state.focus.GetCurrentValue() - state.focus.GetDestination();
	return glm::dot(dp, dp) < k_ArrivedDistanceSquared && glm::dot(df, df) < k_ArrivedDistanceSquared;
}

namespace
{
void UpdatePath(State& state, uint32_t gameMs)
{
	// The game ms of the frame, the sample clamped to the position way's duration, the focus on the focus way at the
	// position runner's segment and parameter, and both set at once (SetPositionAndFocus)
	state.pathMs += static_cast<int32_t>(gameMs);
	const auto point = camera_track::TrackPointAt(*state.track, *state.positionRunner, state.pathMs);
	SetPositionAndFocus(point.position, point.focus);
}
} // namespace

void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	const float dt = std::min(cameraSeconds, k_MaxFrameSeconds);
	state.modeSeconds += dt; // before the mode's update
	// The current mode's update; none of this module's while the creature interaction's camera is the current mode
	if (DualCurrent())
	{
		UpdateDualMode(state, *Modes().CurrentDual());
	}
	else if (ScriptModeCurrent())
	{
		// Validate runs once a turn (Game.cpp); until then a thing that has gone is not read (ReadThing gives nothing
		// for an invalid entity)
		if (state.track != nullptr)
		{
			UpdatePath(state, gameMs);
		}
		UpdateFollow(state);
	}
	state.position.Update(dt);
	state.focus.Update(dt);
	// The position's destination kept inside the disc of the world
	const glm::vec3 centre(k_DiscCentre, 0.0f, k_DiscCentre);
	const auto d = state.position.GetDestination() - centre;
	if (const float d2 = glm::dot(d, d); d2 > k_DiscRadiusSquared)
	{
		state.position.SetDestinationWithTime(d / (std::sqrt(d2) * k_DiscScale) + centre, k_DiscSeconds);
	}
	state.fov.Update(gameSeconds); // game time, not the camera's seconds
}

namespace
{
/// The last drawn point without a NaN (DrawnCamera)
struct DrawnCameraState
{
	glm::vec3 goodOrigin {1000.0f, 0.0f, 1000.0f};
	glm::vec3 goodFocus {1000.0f, 0.0f, 1000.0f};
};
} // namespace

Drawn DrawnCamera(const std::function<float(float, float)>& groundAt)
{
	const auto& state = Get();
	auto& good = Locator::scriptState::value().Get<DrawnCameraState>();
	Drawn drawn {state.position.GetCurrentValue(), state.focus.GetCurrentValue()};
	// A NaN component -> the last good one
	for (int i = 0; i < 3; ++i)
	{
		drawn.origin[i] = std::isnan(drawn.origin[i]) ? good.goodOrigin[i] : drawn.origin[i];
		drawn.focus[i] = std::isnan(drawn.focus[i]) ? good.goodFocus[i] : drawn.focus[i];
	}
	good.goodOrigin = drawn.origin;
	good.goodFocus = drawn.focus;
	return HoldDrawn(drawn, groundAt);
}

Drawn HoldDrawn(Drawn drawn, const std::function<float(float, float)>& groundAt)
{
	// Position and focus never the same point
	if (const auto d = drawn.origin - drawn.focus; glm::dot(d, d) < k_ArrivedDistanceSquared)
	{
		drawn.origin.x -= 1.0f;
		drawn.origin.y += 1.0f;
	}
	// Measured under the nudged position
	return HoldOverLand(drawn, groundAt);
}

Drawn HoldOverLand(Drawn drawn, const std::function<float(float, float)>& groundAt)
{
	// Not in the free camera mode, nor while a camera flag turns it off: neither exists here
	if (const float lift = groundAt(drawn.origin.x, drawn.origin.z) + k_GroundClearance - drawn.origin.y; lift > 0.0f)
	{
		drawn.origin.y += lift;
		drawn.focus.y += lift;
	}
	return drawn;
}

namespace
{
/// The debug camera (the debug window's free camera; scriptState)
struct DebugCameraState
{
	int32_t mode {0};
	glm::vec3 position {0.0f};
	glm::vec3 focus {0.0f};
};

DebugCameraState& DebugCamera()
{
	return Locator::scriptState::value().Get<DebugCameraState>();
}
} // namespace

void SetDebugCameraMode(int32_t mode)
{
	DebugCamera().mode = mode;
}

void SetDebugCameraPosition(const glm::vec3& position)
{
	DebugCamera().position = position;
}

void SetDebugCameraFocus(const glm::vec3& focus)
{
	DebugCamera().focus = focus;
}

int32_t DebugCameraMode()
{
	return DebugCamera().mode;
}

void ApplyDebugCamera(glm::vec3& position, glm::vec3& focus)
{
	// The debug camera replaces the drawn position (mode 2) and focus (1 and 2)
	if (DebugCamera().mode == 2)
	{
		position = DebugCamera().position;
	}
	if (DebugCamera().mode == 1 || DebugCamera().mode == 2)
	{
		focus = DebugCamera().focus;
	}
}

void ApplyShake(Camera& camera, const glm::vec3& lastDrawn, const glm::vec3& lastDrawnFocus)
{
	// Outside the citadel and not playing back, whatever the mode: the shake moves the drawn camera only, from the
	// camera drawn the frame before; the camera's zoomers keep their values. openblack: Camera::SetDrawOffset, which
	// GetOrigin/GetFocus(Current) and the view add
	auto position = camera.GetOriginZoomer().GetCurrentValue();
	auto focus = camera.GetFocusZoomer().GetCurrentValue();
	// moved when an axis compares above or below the camera drawn before (an axis that is not a number reads as equal)
	const auto differs = [](float a, float b) { return a < b || a > b; };
	Get().drawnBefore = lastDrawn;
	Get().drawnMoved = differs(position.x, lastDrawn.x) || differs(position.y, lastDrawn.y) || differs(position.z, lastDrawn.z);
	const bool insideCitadel = game_clock::IsInsideCitadel();
	camera.SetWeatherPoint(std::nullopt);
	if (insideCitadel)
	{
		camera.SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	}
	else
	{
		const auto base = position;
		const auto baseFocus = focus;
		const auto groundAt = [](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : -1e30f;
		};
		// What is drawn, in every mode, is never at its focus and stays a metre over the land, the focus raised with it;
		// the camera's zoomers keep their values. A script mode's drawn camera has it already, and stays as it is
		const auto held = HoldDrawn({.origin = position, .focus = focus}, groundAt);
		position = held.origin;
		focus = held.focus;
		// it is the held camera that is given to be drawn, and compared with the camera drawn before
		Get().drawnMoved =
		    differs(position.x, lastDrawn.x) || differs(position.y, lastDrawn.y) || differs(position.z, lastDrawn.z);
		// and the weather at the camera is read at the held camera too, before the debug camera and the shake move what
		// is drawn
		camera.SetWeatherPoint(position);
		// While the falling spell runs the camera's update takes the camera drawn the frame before (the path's) in place
		// of its own, holds it over the land without the nudge, and compares it and reads the weather there; the game
		// camera is not drawn meanwhile
		if (game_clock::SequenceMode() == game_clock::k_SequenceModeFallingSpell)
		{
			const auto drawnHeld = HoldOverLand({.origin = lastDrawn, .focus = lastDrawnFocus}, groundAt);
			Get().drawnMoved = differs(drawnHeld.origin.x, lastDrawn.x) || differs(drawnHeld.origin.y, lastDrawn.y) ||
			                   differs(drawnHeld.origin.z, lastDrawn.z);
			camera.SetWeatherPoint(drawnHeld.origin);
		}
		// The debug camera and the shake are the drawn camera's own update's, which the falling spell's path camera has
		// in its place while it runs (falling_spell::Hooks::applyCamera)
		if (game_clock::SequenceMode() == game_clock::k_SequenceModeNone)
		{
			ApplyDebugCamera(position, focus);
			camera_shake::Adjust(lastDrawn, position, focus);
		}
		camera.SetDrawOffset(position - base, focus - baseFocus); // 0 when no shake moved it (the offset stays otherwise)
	}
	// The shakes' clock with the frame's real time, once a drawn frame (inferred: after the camera, as the graphics
	// engine runs before the draw)
	camera_shake::Tick(game_clock::FrameRealMs());
}

bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	Frame(cameraSeconds, gameMs, gameSeconds);

	// Inside the citadel the drawn camera stays
	const bool insideCitadel = game_clock::IsInsideCitadel();
	const bool drive = Drives() && !insideCitadel;
	if (drive)
	{
		auto drawn = DrawnCamera([](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : -1e30f;
		});
		// The shake is ApplyShake's, once a frame for whichever camera is drawn
		camera.SetOrigin(drawn.origin);
		camera.SetFocus(drawn.focus);
	}
	// The shakes tick in ApplyShake, once a drawn frame

	// The field of view changed to the zoomer's value (not inside the citadel); openblack keeps it in the config's
	// degrees, which every rebuild of the projection reads
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
