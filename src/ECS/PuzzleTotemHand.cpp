/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleTotemHand.h"

#include <glm/vec2.hpp>

#include "3D/LandMorph.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/VillageTotem.h"
#include "Input/GamePackets.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::archetypes::PuzzleTotemArchetype;
using openblack::ecs::components::PuzzleTotem;
using openblack::ecs::components::Transform;

namespace
{
PuzzleTotem* TotemOf(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	return totem != entt::null && registry.Valid(totem) ? registry.TryGet<PuzzleTotem>(totem) : nullptr;
}
} // namespace

bool puzzle_totem_hand::IsPuzzleTotem(entt::entity thing)
{
	return TotemOf(thing) != nullptr;
}

bool puzzle_totem_hand::ValidForLockedSelect(entt::entity totem)
{
	return IsPuzzleTotem(totem) && abode_queries::IsFunctional(totem);
}

bool puzzle_totem_hand::IsHeld(entt::entity totem)
{
	const auto* component = TotemOf(totem);
	return component != nullptr && component->held;
}

void puzzle_totem_hand::StartLockedSelect(entt::entity totem)
{
	if (auto* component = TotemOf(totem); component != nullptr)
	{
		puzzle_totem::StartDrag(*component);
	}
}

void puzzle_totem_hand::SetHeld(entt::entity totem, bool held)
{
	auto* component = TotemOf(totem);
	if (component == nullptr)
	{
		return;
	}
	component->held = held;
	// held, it is drawn where the hand holds it; let go, as high as its height says
	if (held)
	{
		PuzzleTotemArchetype::PlaceAt(totem, component->handAltitude);
	}
	else
	{
		PuzzleTotemArchetype::Place(totem);
	}
}

void puzzle_totem_hand::Slide(entt::entity totem, float upPixels, float screenHeight)
{
	auto* component = TotemOf(totem);
	if (component == nullptr || !component->held)
	{
		return;
	}
	// the hand grips it as high as it grips a village totem's icon, over its place; going down it stops at the land
	const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(totem).position;
	const float height = object::GetHeight(totem);
	const float handY = village_totem::GripY(position.y, height);
	const float land = land_morph::CurrentAltitude()(glm::vec2(position.x, position.z));
	const float slide = village_totem::HandSlide(upPixels, screenHeight, height, handY - land);
	const auto sent = puzzle_totem::Drag(*component, slide, PuzzleTotemArchetype::TravelOf(totem));
	PuzzleTotemArchetype::PlaceAt(totem, component->handAltitude);
	if (sent.has_value())
	{
		game_packets::Packet packet {game_packets::Type::PuzzleTotemPosition, totem};
		packet.coords = object::MapCoordsOf(totem);
		packet.coords.altitude = *sent;
		game_packets::Push(packet);
	}
}

std::optional<ecs::systems::VillageTotemSystemInterface::HandHold> puzzle_totem_hand::GetHandHold(entt::entity totem)
{
	if (TotemOf(totem) == nullptr)
	{
		return std::nullopt;
	}
	const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(totem).position;
	const float y = village_totem::GripY(position.y, object::GetHeight(totem));
	return ecs::systems::VillageTotemSystemInterface::HandHold {.position = {position.x, y, position.z},
	                                                            .holdRadius = object::GetHoldRadius(totem, true)};
}

void puzzle_totem_hand::EndLockedSelect(entt::entity totem)
{
	auto* component = TotemOf(totem);
	if (component == nullptr)
	{
		return;
	}
	if (component->maxHeight == puzzle_totem::k_TapOnly)
	{
		component->height = puzzle_totem::k_TapOnly;
		return;
	}
	game_packets::Packet packet {game_packets::Type::PuzzleTotemPosition, totem};
	packet.coords = object::MapCoordsOf(totem);
	packet.coords.altitude = component->handAltitude;
	game_packets::Push(packet);
}

void puzzle_totem_hand::ApplyPosition(entt::entity totem, float altitude)
{
	auto* component = TotemOf(totem);
	if (component == nullptr)
	{
		return;
	}
	component->altitude = altitude;
	puzzle_totem::SetShare(
	    *component, puzzle_totem::ShareFromAltitude(altitude, component->restAltitude, PuzzleTotemArchetype::TravelOf(totem)));
	// while held it is drawn where the hand holds it; otherwise as high as its new height says
	SetHeld(totem, component->held);
}
