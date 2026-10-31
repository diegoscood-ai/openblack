/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Villagers.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <imgui.h>

#include "AnimalsModel.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Dances.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/LastDropped.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "Locator.h"
#include "MiraclesModel.h"
#include "VillagersModel.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
using ecs::components::Transform;
using ecs::components::Villager;

/// An entity's number, or none
std::string EntityText(entt::entity entity)
{
	return entity == entt::null ? std::string("none") : fmt::format("{}", entt::to_integral(entity));
}

void Line(const std::string& text)
{
	ImGui::TextUnformatted(text.c_str());
}

/// Where the player's hand is, the right one first
std::optional<glm::vec3> HandPosition()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	return debug::miracles::HandPoint(positions.at(static_cast<size_t>(Side::Left)),
	                                  positions.at(static_cast<size_t>(Side::Right)));
}

/// The priority a reaction would be taken up at now, for the kinds whose priority only reads: a burning thing in the
/// hand and a shield. None for the others
std::optional<uint8_t> PriorityNow(entt::entity villager, uint32_t id, Reaction type)
{
	if (type == Reaction::ReactToMagicShield)
	{
		return ecs::villager_shield::ReactToMagicShieldPriority(villager, id);
	}
	if (type == Reaction::ReactToBurningObjectInHand)
	{
		const auto* row = ecs::villager_reactions::SlotTypeOf(type);
		return row != nullptr && row->priority != nullptr ? std::optional(row->priority(villager, id, 0)) : std::nullopt;
	}
	return std::nullopt;
}

void ReactionLine(entt::entity villager, uint32_t id, std::string_view source)
{
	const auto* reaction = ecs::effects::reactions::Find(id);
	if (reaction == nullptr)
	{
		Line(fmt::format("{}: reaction {}, gone", source, id));
		return;
	}
	Line(fmt::format("{}: {}", source,
	                 debug::villagers::ReactionText(id, debug::miracles::ReactionName(reaction->type),
	                                                static_cast<int>(reaction->type),
	                                                PriorityNow(villager, id, reaction->type))));
	Line(fmt::format("  from {}, {}", EntityText(reaction->initiator),
	                 ecs::effects::reactions::IsAvailable(reaction) ? "available" : "not available"));
	// a new building's dance, which goes with the reaction
	if (const auto& registry = Locator::entitiesRegistry::value(); ecs::dances::IsDance(registry, reaction->dance))
	{
		Line(fmt::format("  its dance {}, {} dancing", EntityText(reaction->dance),
		                 ecs::dances::Size(registry, reaction->dance)));
	}
	// a pot's own flag: its reaction set up, until its reactions are taken away
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* pot =
	        registry.Valid(reaction->initiator) ? registry.TryGet<const ecs::components::Pot>(reaction->initiator) : nullptr;
	    pot != nullptr)
	{
		Line(fmt::format("  a pot, its reaction set up: {}", pot->reactionStarted ? "yes" : "no"));
	}
}
} // namespace

Villagers::Villagers() noexcept
    : Window("Villagers", ImVec2(440.0f, 360.0f))
{
}

void Villagers::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No villagers: no land loaded");
		return;
	}
	const ecs::Registry& registry = Locator::entitiesRegistry::value();

	// the same nearest rule as the Animals window: over the land, the first on a tie
	std::vector<debug::animals::AnimalAt> villagers;
	registry.Each<const Villager, const Transform>(
	    [&villagers](entt::entity entity, const Villager&, const Transform& transform) {
		    villagers.push_back({.entity = entity, .position = transform.position});
	    });

	ImGui::Checkbox("Keep this villager", &_keep);
	const bool keptThere = _kept != entt::null && registry.Valid(_kept) && registry.AllOf<Villager, Transform>(_kept);
	if (!_keep || !keptThere)
	{
		const auto hand = HandPosition();
		const auto nearest = hand ? debug::animals::Nearest(villagers, *hand) : std::nullopt;
		_kept = nearest.value_or(entt::null);
	}
	if (_kept == entt::null)
	{
		Line(fmt::format("{} villagers; none near the hand", villagers.size()));
		return;
	}
	const auto& transform = registry.Get<const Transform>(_kept);
	Line(fmt::format("Villager {}, of {} on the land", EntityText(_kept), villagers.size()));
	Line(fmt::format("At ({:.1f}, {:.1f}, {:.1f})", transform.position.x, transform.position.y, transform.position.z));
	DrawReactions(_kept);
}

void Villagers::DrawReactions(entt::entity villager) const noexcept
{
	const ecs::Registry& registry = Locator::entitiesRegistry::value();
	ImGui::SeparatorText("Reactions");
	bool any = false;
	if (const auto slot = ecs::villager_reactions::SlotReaction(villager); slot != 0)
	{
		ReactionLine(villager, slot, "Slot");
		Line(fmt::format("  keeping {}", EntityText(ecs::villager_reactions::SlotObject(villager))));
		any = true;
	}
	if (const auto* shield = registry.TryGet<const ecs::components::VillagerShieldState>(villager);
	    shield != nullptr && shield->reaction != 0)
	{
		ReactionLine(villager, shield->reaction, "Shield");
		any = true;
	}
	if (const auto* fire = registry.TryGet<const ecs::components::VillagerFireState>(villager);
	    fire != nullptr && fire->reaction != 0)
	{
		ReactionLine(villager, fire->reaction, "Fire");
		any = true;
	}
	if (!any)
	{
		ImGui::TextUnformatted("Reaction: none");
	}

	ImGui::SeparatorText("Effects and the hand");
	Line(fmt::format("Its town attacked lately enough for a shield: {}",
	                 debug::villagers::YesNo(ecs::villager_shield::TownAttackedLately(villager))));
	using ecs::effects::EffectValues;
	EffectValues hit {};
	hit.numbers[static_cast<size_t>(EffectValues::Number::Hit)] = 1.0f;
	EffectValues heal {};
	heal.numbers[static_cast<size_t>(EffectValues::Number::Heal)] = 1.0f;
	Line(
	    fmt::format("Takes an effect now: {}", debug::villagers::ReceiverText(ecs::effects::IsEffectReceiver(villager, hit),
	                                                                          ecs::effects::IsEffectReceiver(villager, heal))));
	Line(fmt::format("Last let go of by: {}",
	                 debug::villagers::LastLetGoText(ecs::last_dropped::WhoLastDropped(registry, villager))));
}
