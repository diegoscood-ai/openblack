/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object near the action's point when the click hit none and the point it searches around, the land behind the
// hand seen from the camera; and a hand demo's click found again on the recorded kind of thing. Wiki:
// docs/wiki/hand/picking.md#object-under-the-cursor, docs/wiki/project/debugging.md#hand-demos.

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/HandNearObject.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "EngineConfig.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// (debug) OPENBLACK_HAND_TRACE: the press's search and a hand demo's click are logged at debug level, in the game
/// log. Off, nothing is logged and nothing else changes
std::shared_ptr<spdlog::logger> PressTraceLogger()
{
	static const bool k_Enabled = openblack::debug_env::HandTrace();
	return k_Enabled ? spdlog::get("game") : nullptr;
}

/// (debug) an entity as the press trace names it: its number and its script type
std::string DescribeForTrace(std::optional<entt::entity> object)
{
	if (!object)
	{
		return "none";
	}
	if (!Locator::entitiesRegistry::value().Valid(*object))
	{
		return std::to_string(entt::to_integral(*object)) + " (gone)";
	}
	return std::to_string(entt::to_integral(*object)) + " (script type " +
	       std::to_string(static_cast<int>(openblack::ecs::script_type::TypeOf(*object))) + ")";
}
} // namespace

void HandSystem::UpdatePointBehindHand() noexcept
{
	// Once a drawn frame, in the landscape draw: the box (min / max) of the hand's bones 1..n-1 (element 1 first, the
	// root left out), the camera's ray through its centre onto the land (with the plane y = 0 within 7500 m when the
	// land is missed): hit or not, and the point (x, 0, z)
	_pointBehindHand.reset();
	// the bones in the world as the hand finished them, or the model-space ones under the hand's matrix before that
	const auto* world = GetWorldBoneMatrices();
	const auto* bones = world != nullptr ? world : GetBoneMatrices();
	if (bones == nullptr || bones->empty() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::pickingSystem::has_value())
	{
		return;
	}
	const glm::mat4 model = world != nullptr ? glm::mat4(1.0f) : GetHandMatrix();
	glm::vec3 lo(std::numeric_limits<float>::max());
	glm::vec3 hi(std::numeric_limits<float>::lowest());
	for (size_t i = 1; i < bones->size(); ++i)
	{
		const glm::vec3 joint = glm::vec3(model * (*bones)[i][3]);
		lo = glm::min(lo, joint);
		hi = glm::max(hi, joint);
	}
	const glm::vec3 centre = (lo + hi) * 0.5f;
	const glm::vec3 camera = Locator::camera::value().GetOrigin();
	if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(camera, centre, camera))
	{
		_pointBehindHand = glm::vec3(hit->x, 0.0f, hit->z);
	}
}

std::optional<entt::entity> HandSystem::FindObjectNearMapCoord(glm::vec3 at) const noexcept
{
	namespace map_coords = openblack::map_coords;
	const auto trace = PressTraceLogger();
	// nothing without the point behind the hand or with it at (0, 0, 0)
	if (!_pointBehindHand)
	{
		if (trace)
		{
			trace->debug("Press search: no point behind the hand, nothing taken");
		}
		return std::nullopt;
	}
	// MapCoords c of that point: x, z = m x 6553.6 truncated toward zero, altitude = 0 - the ground height (so 0 only on a cell
	// at height 0)
	const glm::vec3 c = *_pointBehindHand;
	if (map_coords::ToFixed(c.x) == 0 && map_coords::ToFixed(c.z) == 0 &&
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(c.x, c.z)) == 0.0f)
	{
		if (trace)
		{
			trace->debug("Press search: the point behind the hand is the map's corner, nothing taken");
		}
		return std::nullopt;
	}
	// c on the water: the shoal near the action's point (xz distance^2 < 4), then the fish farm of that shoal.
	// (approximate) ecs::FindFishFarmAt walks the farms, not the shoal list then the farm: they only differ where
	// shoals overlap
	// (inferred) !IsLand stands for the original's water test
	if (!hand_detail::IsLand(c))
	{
		if (const auto farm = ecs::FindFishFarmAt(at); farm)
		{
			if (trace)
			{
				trace->debug("Press search: on the water, takes the fish farm {}", DescribeForTrace(farm));
			}
			return farm;
		}
	}
	// the nearest object but a fragment within 5 m of c, if the action's point is not nearer to c
	const auto& registry = Locator::entitiesRegistry::value();
	const auto lookup = [&registry](entt::entity object) -> std::optional<ecs::hand_near::Candidate> {
		if (!ecs::IsAvailable(object))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const Transform>(object);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		return ecs::hand_near::Candidate {.position = transform->position, .fragment = registry.AllOf<Fragment>(object)};
	};
	if (!trace)
	{
		return ecs::hand_near::NearestToLandBehindHand(c, at, ecs::map_cells::ForEachInCell, lookup);
	}
	// (debug) the same search, with its inputs, every object it looks at and its pick logged
	trace->debug("Press search: behind the hand ({:.3f}, {:.3f}), the action point ({:.3f}, {:.3f}, {:.3f}), {:.3f} m from it",
	             c.x, c.z, at.x, at.y, at.z, gutils::GetDistanceInMetres(at, c));
	const auto tracedLookup = [&lookup, &trace, c](entt::entity object) -> std::optional<ecs::hand_near::Candidate> {
		const auto candidate = lookup(object);
		if (!candidate)
		{
			trace->debug("Press search:   {} skipped: not available or no position", DescribeForTrace(object));
		}
		else
		{
			trace->debug("Press search:   {} at ({:.3f}, {:.3f}), {:.3f} m{}", DescribeForTrace(object), candidate->position.x,
			             candidate->position.z, gutils::GetDistanceInMetres(c, candidate->position),
			             candidate->fragment ? ", a fragment, left out" : "");
		}
		return candidate;
	};
	const auto found = ecs::hand_near::NearestToLandBehindHand(c, at, ecs::map_cells::ForEachInCell, tracedLookup);
	trace->debug("Press search: takes {}", DescribeForTrace(found));
	return found;
}

void HandSystem::TracePick() noexcept
{
	const auto trace = PressTraceLogger();
	if (!trace || _tracePickLogged || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_tracePickLogged = true;
	const auto name = [](PickVerdict verdict) {
		switch (verdict)
		{
		case PickVerdict::NoMesh:
			return "no mesh or an empty box";
		case PickVerdict::DroppedFromAbove:
			return "dropped from above";
		case PickVerdict::TestedAsCreature:
			return "left to the creature's body";
		case PickVerdict::SkippedHand:
			return "skipped: the hand, or in it, or its effects";
		case PickVerdict::SkippedNotInteractable:
			return "skipped: not interactable";
		case PickVerdict::SkippedPlinthOrSeeThrough:
			return "skipped: a plinth stone or a see-through copy";
		case PickVerdict::SkippedNotDrawn:
			return "skipped: not drawn";
		case PickVerdict::SkippedLamp:
			return "skipped: a lamp with no collision";
		case PickVerdict::SkippedMovesWithHand:
			return "skipped: moves with the hand";
		case PickVerdict::CircleMiss:
			return "outside its screen circle";
		case PickVerdict::SphereMiss:
			return "its sphere misses the ray, or starts past the nearest";
		case PickVerdict::TriangleMiss:
			return "triangle miss";
		case PickVerdict::Hit:
			return "hit, the nearest so far";
		case PickVerdict::HitNotNearer:
			return "hit, not nearer";
		case PickVerdict::SphereHit:
			return "invisible sphere or body hit, the nearest so far";
		case PickVerdict::InvisibleSphereMiss:
			return "invisible sphere missed";
		}
		return "?";
	};
	const auto& land = _tracePickLand;
	trace->debug("Pick: the nearest object {} at {:.3f}; the land at {}; the land in front {}, inside its footprint {}; "
	             "the object kept {}",
	             DescribeForTrace(land.picked), land.t,
	             land.landDistance ? fmt::format("{:.3f}", *land.landDistance) : std::string("none"), land.landBefore,
	             land.insideFootprint, land.kept);
	// the objects dropped from above in one line, every other one in its own
	std::string dropped;
	size_t droppedCount = 0;
	for (const auto& entry : _tracePick)
	{
		if (entry.verdict == PickVerdict::DroppedFromAbove)
		{
			dropped += (droppedCount++ == 0 ? "" : " ") + std::to_string(entt::to_integral(entry.entity));
		}
	}
	trace->debug("Pick:   {} dropped from above: {}", droppedCount, dropped);
	for (const auto& entry : _tracePick)
	{
		if (entry.verdict == PickVerdict::DroppedFromAbove)
		{
			continue;
		}
		const bool hasT = entry.verdict == PickVerdict::Hit || entry.verdict == PickVerdict::HitNotNearer ||
		                  entry.verdict == PickVerdict::SphereHit;
		const auto sphere =
		    entry.sphere ? fmt::format(" (centre ({:.3f}, {:.3f}, {:.3f}), radius {:.3f}; at its depth {:.3f} the ray passes "
		                               "{:.3f} from it)",
		                               entry.sphere->centre.x, entry.sphere->centre.y, entry.sphere->centre.z,
		                               entry.sphere->radius, entry.sphere->depth, entry.sphere->pass)
		                 : std::string();
		trace->debug("Pick:   {}: {}{}{}", DescribeForTrace(entry.entity), name(entry.verdict),
		             hasT ? fmt::format(" at {:.3f}", entry.t) : std::string(), sphere);
	}
}

void HandSystem::TracePress(bool searched, std::optional<entt::entity> found) noexcept
{
	const auto cleared = std::exchange(_traceClearedByDemoClick, std::nullopt);
	const auto trace = PressTraceLogger();
	if (!trace || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	TracePick();
	if (!searched)
	{
		trace->debug("Press: under the cursor {}, hovered {}, held {}: no search near the action's point",
		             DescribeForTrace(_cursorObject), DescribeForTrace(_hovered), DescribeForTrace(_held));
		return;
	}
	trace->debug("Press: nothing under the cursor, the search near the action's point takes {}{}", DescribeForTrace(found),
	             found ? "" : ": the press is on the land");
	if (found || !cleared)
	{
		return;
	}
	// a hand demo's click cleared the object under the cursor and the search did not give it back: that object's own tests
	const auto& registry = Locator::entitiesRegistry::value();
	const auto object = *cleared;
	if (!registry.Valid(object))
	{
		trace->debug("Press: the demo click had cleared {}", DescribeForTrace(object));
		return;
	}
	const auto* spellIcon = registry.TryGet<const SpellIcon>(object);
	trace->debug(
	    "Press: the demo click had cleared {}: a spell icon {} (its seed graphic {}), a seed graphic {}, available {}, "
	    "interactable {}, valid to tap {}, in the map cells {}",
	    DescribeForTrace(object), spellIcon != nullptr,
	    spellIcon != nullptr ? DescribeForTrace(spellIcon->graphic) : std::string("none"),
	    registry.AllOf<SpellSeedGraphic>(object), ecs::IsAvailable(object), Interactable(object),
	    worship::InterfaceValidToTap(object, PlayerNames::PLAYER_ONE), ecs::map_cells::IsObjectInMap(object));
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform != nullptr && _pointBehindHand)
	{
		const auto at = _interactionPoint.value_or(glm::vec3(0.0f));
		trace->debug("Press: it is at ({:.3f}, {:.3f}, {:.3f}), {:.3f} m from the point behind the hand; the action point "
		             "({:.3f}, {:.3f}, {:.3f}) is {:.3f} m from it",
		             transform->position.x, transform->position.y, transform->position.z,
		             gutils::GetDistanceInMetres(*_pointBehindHand, transform->position), at.x, at.y, at.z,
		             gutils::GetDistanceInMetres(*_pointBehindHand, at));
	}
}

void HandSystem::RetargetDemoClick(const hand_demo::Record& click) noexcept
{
	if (!hand_demo::ClickCanRetarget(click.message) || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the thing under the hand, as the scripts see it; its sub-type is read only when its type is the recorded one. While
	// a press grabs a thing, the action keeps that thing as its own, so the click's release is compared with it
	const auto compared =
	    hand_demo::ClickComparedThing(_cursorObject, _pendingPick.has_value() ? _collideObject : std::nullopt);
	std::optional<hand_demo::ThingUnderHand> underHand;
	if (compared && registry.Valid(*compared) && ecs::IsAvailable(*compared))
	{
		const auto type = ecs::script_type::TypeOf(*compared);
		underHand = hand_demo::ThingUnderHand {.type = type,
		                                       .subtype = static_cast<uint32_t>(type) == click.objectType
		                                                      ? ecs::script_type::SubtypeOf(*compared)
		                                                      : ecs::script_type::k_NoSubtype,
		                                       .at = ecs::object::MapCoordsOf(*compared)};
	}
	const auto trace = PressTraceLogger();
	const auto target = hand_demo::TargetOfClick(click, underHand);
	if (trace)
	{
		TracePick();
		const auto recorded = map_coords::ToWorld(hand_demo::RecordedPlace(click));
		trace->debug("Demo click: message {}, recorded type {} sub-type {} at ({:.3f}, {:.3f}); under the cursor {}; {}",
		             click.message, click.objectType, click.objectSubtype, recorded.x, recorded.z,
		             DescribeForTrace(_cursorObject),
		             target == hand_demo::ClickTarget::UnderHand ? "kept"
		             : target == hand_demo::ClickTarget::NoThing ? "cleared: the recording had no thing"
		                                                         : "looking for the nearest of the recorded kind");
	}
	switch (target)
	{
	case hand_demo::ClickTarget::UnderHand:
		return;
	case hand_demo::ClickTarget::NoThing:
		// the click's object is cleared; the action's point stays
		if (trace)
		{
			_traceClearedByDemoClick = _cursorObject;
		}
		_cursorObject.reset();
		_cursorObjectDistance.reset();
		return;
	case hand_demo::ClickTarget::NearestOfKind:
		break;
	}
	// the nearest thing of the recorded kind within the reach of the recorded place, measured to it (to the totem for a
	// worship site); the thing's sub-type is not read when any will do
	const auto place = hand_demo::RecordedPlace(click);
	const auto found = ecs::map_cells::FindNearForScript(
	    place,
	    [&click](entt::entity thing) {
		    const auto type = ecs::script_type::TypeOf(thing);
		    if (static_cast<uint32_t>(type) != click.objectType)
		    {
			    return false;
		    }
		    const auto subtype = click.objectSubtype == ecs::script_type::k_AnySubtype ? ecs::script_type::k_NoSubtype
		                                                                               : ecs::script_type::SubtypeOf(thing);
		    return hand_demo::TakesRecordedClick(
		        click, {.type = type, .subtype = subtype, .at = ecs::map_cells::ScriptDistancePoint(thing)});
	    },
	    hand_demo::k_RetargetReach);
	const bool isFound = found != entt::null;
	const auto outcome =
	    hand_demo::OutcomeOfFind(isFound, isFound && ecs::script_type::IsObject(found), isFound && Interactable(found));
	if (trace)
	{
		// (debug) what the search for the recorded kind found, how far from the recorded place, and what it does to the
		// click
		const auto foundAt = isFound ? std::optional(ecs::map_cells::ScriptDistancePoint(found)) : std::nullopt;
		trace->debug("Demo click: the search for the recorded kind within {:.1f} m found {}{}; {}", hand_demo::k_RetargetReach,
		             DescribeForTrace(isFound ? std::optional(found) : std::nullopt),
		             foundAt ? fmt::format(" at {:.3f} m from the recorded place, an object {}, interactable {}",
		                                   gutils::GetDistanceInMetres(place, *foundAt), ecs::script_type::IsObject(found),
		                                   Interactable(found))
		                     : std::string(),
		             outcome == hand_demo::FoundClick::Thing     ? "the click lands on it"
		             : outcome == hand_demo::FoundClick::NoThing ? "the click lands on no thing"
		                                                         : "the click stays on what is under the cursor");
	}
	// An object found makes the click again from it, its point cleared, so that the press looks for a thing from the
	// map's origin; with the vanilla bugs fixed the point stays where the click was
	if (hand_demo::ClearsClickPoint(outcome) && (!Locator::config::has_value() || !Locator::config::value().fixVanillaBugs))
	{
		_interactionPoint = glm::vec3(0.0f);
	}
	switch (outcome)
	{
	case hand_demo::FoundClick::Unchanged:
		// none found, or not an object: the click stays on what is under the hand
		return;
	case hand_demo::FoundClick::Thing:
		_cursorObject = found;
		_cursorObjectDistance = click.objectDistance;
		return;
	case hand_demo::FoundClick::NoThing:
		// an object the hand cannot act on: the click lands on no thing
		if (trace)
		{
			_traceClearedByDemoClick = _cursorObject;
		}
		_cursorObject.reset();
		_cursorObjectDistance.reset();
		return;
	}
}
