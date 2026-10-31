/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand pulling the tied leash's rope (creature_leash's pull): finding the rope under the mouse, the rope taking the
// press from what else is under the hand, and the pull itself, action state 21. Wiki:
// docs/wiki/creature/leash.md#pulling-a-tied-leash

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <limits>
#include <optional>
#include <utility>

#include "3D/Billboard.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Creature/LeashPull.h"
#include "Creature/LeashRope.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandCursorDepth.h"
#include "ECS/LeashHandTie.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "HandSystem.h"
#include "Input/GameCursor.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::Transform;

namespace
{
/// The player's creature whose leash is tied, with that leash
std::optional<std::pair<entt::entity, const CreatureLeash::Worn*>> TiedLeash()
{
	if (!Locator::leashSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& leash = Locator::leashSystem::value();
	const auto creature = leash.PlayersCreature(PlayerNames::PLAYER_ONE);
	if (!creature.has_value() || !leash.IsLeashed(*creature) || !leash.TiedTo(*creature).has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashes = registry.Valid(*creature) ? registry.TryGet<const CreatureLeash>(*creature) : nullptr;
	if (leashes == nullptr || !leashes->worn.has_value() || !leashes->worn->ropeStarted)
	{
		return std::nullopt;
	}
	return std::pair(*creature, &*leashes->worn);
}

/// Where the pull's length is measured to: the tied thing's place on the map, at the land's height there plus its
/// altitude over the land
std::optional<glm::vec3> TiedThingPoint(entt::entity tied)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(tied) || registry.TryGet<const Transform>(tied) == nullptr || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto place = ecs::object::MapCoordsOf(tied);
	return creature_leash::TiedPointOf(place, Locator::terrainSystem::value().GetHeightAt(map_coords::ToMetres(place)));
}
} // namespace

std::optional<float> HandSystem::TiedRopeDepthUnderHand() const noexcept
{
	const auto tied = TiedLeash();
	if (!tied.has_value() || !Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto& rope = tied->second->rope;
	const auto& camera = Locator::camera::value();
	const auto size = Locator::windowing::value().GetSize();
	// the camera as the game computes it on the CPU, and the near clip its screen points are found with
	const auto matrices = graphics::billboard::CameraFrame::From(camera).clipMatrices;
	const float nearClip = camera.GetPixelNearClip();
	// the rope's points from the collar, as the game finds them: openblack's rope runs from what it is tied to
	std::array<glm::vec3, leash_rope::k_PointCount> world {};
	std::array<creature_leash::RopeScreenPoint, leash_rope::k_PointCount> screen {};
	for (size_t i = 0; i < world.size(); ++i)
	{
		world.at(i) = leash_rope::Point(rope, leash_rope::k_PointCount - 1 - i);
		screen.at(i) = creature_leash::ProjectRopePoint(matrices.worldToClipping, nearClip, size, world.at(i));
	}
	const auto hit =
	    creature_leash::PickRope(input::GameCursorRef(), screen, static_cast<uint16_t>(size.x), rope.look.halfWidth);
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return creature_leash::RopeHitDepth(matrices.worldToCamera, world.at(hit->segment), world.at(hit->segment + 1), hit->along);
}

bool HandSystem::RopeTakesThePick() noexcept
{
	_ropeUnderHand = false;
	const auto ropeDepth = TiedRopeDepthUnderHand();
	if (!ropeDepth.has_value())
	{
		return false;
	}
	// its rival: the object under the hand, else the screen object, else the land with its margin (an endless depth
	// with no land)
	using creature_leash::RopeRival;
	auto rival = RopeRival::Land;
	float rivalDepth = _cursorLandDistance.has_value() ? *_cursorLandDistance + hand_cursor_depth::k_LandMargin
	                                                   : std::numeric_limits<float>::max();
	if (_cursorObject.has_value())
	{
		const bool creature = std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(*_cursorObject);
		rival = creature ? RopeRival::Creature : RopeRival::Object;
		rivalDepth = _cursorObjectDistance.value_or(std::numeric_limits<float>::max());
	}
	else if (_screenObject.has_value())
	{
		rival = RopeRival::ScreenObject;
		rivalDepth = _screenObjectDepth;
	}
	if (!creature_leash::RopeTakesThePick(rival, rivalDepth, *ropeDepth))
	{
		return false;
	}
	if (rival == RopeRival::Object)
	{
		_cursorObject.reset();
	}
	else if (rival == RopeRival::ScreenObject)
	{
		_screenObject.reset();
	}
	_ropeUnderHand = true;
	return true;
}

void HandSystem::StartLeashPull() noexcept
{
	const auto tied = TiedLeash();
	if (!tied.has_value())
	{
		return;
	}
	_leashPull = tied->first;
	// the two ropes from the hand, to the collar and to what the leash is tied to, laid now
	Locator::leashSystem::value().StartPull(tied->first);
}

void HandSystem::UpdateLeashPull(bool actionReleased) noexcept
{
	if (!_leashPull.has_value())
	{
		return;
	}
	const auto creature = *_leashPull;
	const auto tied = TiedLeash();
	// untied meanwhile (or the creature gone): the pull ends, nothing sent
	if (!tied.has_value() || tied->first != creature)
	{
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().EndPull(creature);
		}
		_leashPull.reset();
		return;
	}
	if (!actionReleased)
	{
		return;
	}
	// let go: the hand's distance from where the tied thing stands, no less than twice its radius across the ground,
	// becomes the tied leash's length, and the creature is sent to the land under the hand
	const auto tiedTo = Locator::leashSystem::value().TiedTo(creature);
	const auto at = tiedTo.has_value() ? TiedThingPoint(*tiedTo) : std::nullopt;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* hand = registry.TryGet<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (at.has_value() && hand != nullptr)
	{
		const float full = creature_leash::PullLength(hand->position, *at, ecs::object::Get2DRadius(*tiedTo));
		ecs::leash_tie::OnHandPullLetGo(PlayerNames::PLAYER_ONE, full, _interactionPoint.value_or(glm::vec3(0.0f)));
	}
	Locator::leashSystem::value().EndPull(creature);
	_leashPull.reset();
}
