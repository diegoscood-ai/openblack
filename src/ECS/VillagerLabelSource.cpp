/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerLabelSource.h"

#include <cstring>

#include <string>
#include <utility>

#include <glm/vec3.hpp>

#include "Creature/LeashPull.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/MobileDrawing.h"
#include "ECS/PlayerColours.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager_label_source
{

namespace
{
namespace labels = gui::villager_labels;

/// A name from the table, a byte a character
std::u16string FromTable(const char* text, size_t size)
{
	const auto length = strnlen(text, size);
	std::u16string wide(length, u'\0');
	for (size_t i = 0; i < length; ++i)
	{
		wide[i] = static_cast<char16_t>(static_cast<unsigned char>(text[i]));
	}
	return wide;
}

/// The colour of the player owning the villager's town, none for no town
std::optional<glm::u8vec3> OwnerColour(const Registry& registry, entt::entity town, int32_t landNumber)
{
	if (town == entt::null || !registry.Valid(town))
	{
		return std::nullopt;
	}
	const auto* owned = registry.TryGet<const components::Town>(town);
	if (owned == nullptr)
	{
		return std::nullopt;
	}
	// the neutral player's colour is black, which the name takes as white
	const auto argb = player_colours::Colour(owned->owner, landNumber);
	return glm::u8vec3((argb >> 16u) & 0xFFu, (argb >> 8u) & 0xFFu, argb & 0xFFu);
}
} // namespace

std::vector<labels::Request> Gather(const Frame& frame)
{
	using namespace components;
	std::vector<labels::Request> requests;
	// No name is made while the help system has the wide screen on
	if (frame.wideScreen || frame.screen.x <= 0 || frame.screen.y <= 0 || !Locator::infoConstants::has_value())
	{
		return requests;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& info = Locator::infoConstants::value();
	for (const auto entity : frame.drawn)
	{
		if (!registry.Valid(entity) || !registry.AllOf<Villager, Transform>(entity))
		{
			continue;
		}
		// The villager in the hand is not drawn, nor its name
		if (frame.inHand.has_value() && *frame.inHand == entity)
		{
			continue;
		}
		const auto* named = registry.TryGet<const NamedVillager>(entity);
		const auto size = labels::NameSize({
		    .named = named != nullptr,
		    .inScript = script_held::IsInScript(entity),
		    .nameHidden = registry.AllOf<NameHidden>(entity),
		    // A named villager speaks in its own bubble only to tell of new e-mail, which openblack does not have
		    .speaking = false,
		    .nameFade = frame.nameFade,
		});
		if (!size.has_value())
		{
			continue;
		}
		// The point over its head, where its model is drawn, as the game projects it: only one on the screen
		const auto point = DrawnPosition(registry, entity) + glm::vec3(0.0f, labels::k_HeadHeight, 0.0f);
		const auto projected = creature_leash::ProjectRopePoint(frame.worldToClipping, frame.nearClip, frame.screen, point);
		if (!projected.projected || projected.pixel.x < 0 || projected.pixel.y < 0 || projected.pixel.x >= frame.screen.x ||
		    projected.pixel.y >= frame.screen.y)
		{
			continue;
		}
		const auto& row = info.specialVillager.at(named->row);
		const auto& villager = registry.Get<const Villager>(entity);
		requests.push_back({
		    .pixel = projected.pixel,
		    .depth = projected.depth,
		    .point = point,
		    .size = *size,
		    .text = FromTable(row.name.data(), row.name.size()),
		    .colour = labels::NameColour(OwnerColour(registry, villager.town, frame.landNumber)),
		});
	}
	return requests;
}

} // namespace openblack::ecs::villager_label_source
