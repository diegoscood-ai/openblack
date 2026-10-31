/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraBookmarkSystem.h"

#include <utility>

#include "3D/LandIslandInterface.h"
#include "Camera/BookmarkMarkers.h"
#include "ECS/Archetypes/CameraBookmarkArchetype.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The land, when there is one
const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}

/// Whether a bookmark's thing is still in the registry with a place of its own
bool ThingIsPlaced(const ecs::Registry& registry, const CameraBookmark& bookmark)
{
	return bookmark.target.has_value() && registry.Valid(*bookmark.target) && registry.AllOf<Transform>(*bookmark.target);
}

/// A set bookmark's marker: 2 above where its thing is drawn this frame; without a thing (or once it is gone) 2 above
/// the land point of the bookmark's map position
glm::vec3 MarkerPositionOf(const ecs::Registry& registry, const LandIslandInterface* island, const CameraBookmark& bookmark)
{
	if (ThingIsPlaced(registry, bookmark))
	{
		auto point = ecs::DrawnPosition(registry, *bookmark.target);
		point.y += bookmark_markers::k_HeightAboveTheLand;
		return point;
	}
	return bookmark_markers::MarkerPosition(island, *bookmark.position);
}
} // namespace

bool CameraBookmarkSystem::Initialize()
{
	// a new land: every bookmark made again, unset, and the view before a jump forgotten
	_bookmarks = archetypes::CameraBookmarkArchetype::CreateAll();
	_beforeJump = {};
	return true;
}

void CameraBookmarkSystem::Update(uint32_t frameGameMs, bool scriptWideScreen)
{
	_pulse = bookmark_pulse::Step(_pulse, frameGameMs);
	const glm::vec2 halfSize = bookmark_pulse::MarkerHalfSize(_pulse);
	const LandIslandInterface* island = Island();
	const bool shown = bookmark_markers::MarkersShown(_enabled, scriptWideScreen);
	// each set bookmark's marker above the land point of its map position, as the land is this frame, while they are
	// shown; a marker has a Transform only while it is drawn, and one already drawn is moved in place
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const CameraBookmark>(
	    [&registry, &halfSize, island, shown](entt::entity entity, const CameraBookmark& bookmark) {
		    const bool drawn = registry.AllOf<Transform>(entity);
		    if (!shown || !bookmark.position.has_value())
		    {
			    if (drawn)
			    {
				    registry.Remove<Transform>(entity);
			    }
			    return;
		    }
		    const auto position = MarkerPositionOf(registry, island, bookmark);
		    if (!drawn)
		    {
			    registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(halfSize, 1.0f));
			    return;
		    }
		    auto& transform = registry.Get<Transform>(entity);
		    transform.position = position;
		    transform.scale = glm::vec3(halfSize, 1.0f);
	    });
}

void CameraBookmarkSystem::SetBookmark(uint8_t index, const map_coords::MapCoords& position, std::optional<entt::entity> target,
                                       const glm::vec3& cameraOrigin)
{
	if (index < _bookmarks.size())
	{
		_followed.reset();
		_current = index;
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = _bookmarks.at(index);
		auto& bookmark = registry.Get<CameraBookmark>(entity);
		// on a thing, at its map position; else at the one given
		const bool onThing = target.has_value() && registry.Valid(*target) && registry.AllOf<Transform>(*target);
		bookmark.target = onThing ? target : std::nullopt;
		bookmark.position = onThing ? ecs::object::MapCoordsOf(*target) : position;
		// the camera, from the land point of that position
		bookmark.cameraOffset = cameraOrigin - map_coords::ToWorld(Island(), *bookmark.position);
	}
	else
	{
		assert(false);
	}
}

bool CameraBookmarkSystem::IsSet(uint8_t index) const
{
	if (index == k_BeforeJumpBookmark)
	{
		return _beforeJump.position.has_value();
	}
	if (index >= _bookmarks.size())
	{
		return false;
	}
	const auto* bookmark = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CameraBookmark>(_bookmarks.at(index));
	return bookmark != nullptr && bookmark->position.has_value();
}

std::optional<BookmarkFlight> CameraBookmarkSystem::JumpTo(uint8_t index, const BookmarkJumpFrom& from)
{
	if (!IsSet(index) || from.insideCitadel)
	{
		return std::nullopt;
	}
	_current = index;
	if (_followed != index)
	{
		_followed.reset();
	}
	auto flight = FlightTo(index);
	// then the view it leaves: the interface status's place, and the camera from its land point
	_beforeJump.position = from.statusPlace;
	_beforeJump.cameraOffset = from.cameraOrigin - map_coords::ToWorld(Island(), from.statusPlace);
	return flight;
}

std::optional<BookmarkFlight> CameraBookmarkSystem::ToggleFollow(uint8_t index, const BookmarkJumpFrom& from)
{
	if (!IsSet(index))
	{
		return std::nullopt;
	}
	const auto& bookmark = std::as_const(Locator::entitiesRegistry::value()).Get<const CameraBookmark>(_bookmarks.at(index));
	if (!bookmark.target.has_value())
	{
		// on no thing: a jump, and nothing followed
		auto flight = JumpTo(index, from);
		_followed.reset();
		return flight;
	}
	_followed = _followed == index ? std::nullopt : std::optional(index);
	return std::nullopt;
}

std::optional<BookmarkFlight> CameraBookmarkSystem::FlightTo(uint8_t index) const
{
	if (index == k_BeforeJumpBookmark)
	{
		if (!_beforeJump.position.has_value())
		{
			return std::nullopt;
		}
		const auto focus = map_coords::ToWorld(Island(), *_beforeJump.position);
		return BookmarkFlight {.origin = focus + _beforeJump.cameraOffset, .focus = focus};
	}
	if (index >= _bookmarks.size())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* bookmark = registry.TryGet<const CameraBookmark>(_bookmarks.at(index));
	if (bookmark == nullptr || !bookmark->position.has_value())
	{
		return std::nullopt;
	}
	// its land point as it is now, and the camera at the offset it had from it
	const auto focus = map_coords::ToWorld(Island(), *bookmark->position);
	return BookmarkFlight {.origin = focus + bookmark->cameraOffset, .focus = focus};
}

std::optional<BookmarkFlight> CameraBookmarkSystem::ProcessTurn(bool playersCamera, bool cameraInputCounted,
                                                                const BookmarkJumpFrom& from)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<CameraBookmark>([&registry](CameraBookmark& bookmark) {
		if (!bookmark.position.has_value() || !bookmark.target.has_value())
		{
			return;
		}
		// a thing no longer available is forgotten: the bookmark stays where the thing last was
		if (!ecs::IsAvailable(*bookmark.target) || !ThingIsPlaced(registry, bookmark))
		{
			bookmark.target.reset();
			return;
		}
		bookmark.position = ecs::object::MapCoordsOf(*bookmark.target);
	});
	// the followed bookmark is jumped to while the player's camera has the view; another camera ends the follow
	std::optional<BookmarkFlight> flight;
	if (!playersCamera)
	{
		_followed.reset();
	}
	else if (_followed.has_value())
	{
		flight = JumpTo(*_followed, from);
	}
	// so does a turn whose camera input counted for the camera help
	if (cameraInputCounted)
	{
		_followed.reset();
	}
	return flight;
}

void CameraBookmarkSystem::ClearBookmark(uint8_t index)
{
	if (index < _bookmarks.size())
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& bookmark = registry.Get<CameraBookmark>(_bookmarks.at(index));
		bookmark.position.reset();
		bookmark.target.reset();
	}
	else
	{
		assert(false);
	}
}
