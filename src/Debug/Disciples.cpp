/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Disciples.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/DiscipleMarks.h"
#include "ECS/DiscipleRules.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
/// The disciples' names, by VillagerDisciple
constexpr std::array<const char*, 13> k_Names {"none",         "farmer",     "forester",   "fisherman", "builder",
                                               "breeder",      "protection", "missionary", "craftsman", "trader",
                                               "change house", "worship",    "from vortex"};

const char* NameOf(uint8_t disciple)
{
	return disciple < k_Names.size() ? k_Names.at(disciple) : "?";
}

/// The villager's top state, by name
std::string_view StateNameOf(const ecs::components::LivingAction& action)
{
	const auto state = static_cast<size_t>(action.states.at(0));
	return state < k_VillagerStateStrings.size() ? k_VillagerStateStrings.at(state) : std::string_view("?");
}

/// The love-making states out of doors
bool IsLoveState(const ecs::components::LivingAction& action)
{
	const auto state = static_cast<VillagerStates>(action.states.at(0));
	return state == VillagerStates::StartHavingSex || state == VillagerStates::HavingSex ||
	       state == VillagerStates::StopHavingSex;
}
} // namespace

Disciples::Disciples() noexcept
    : Window("Disciples", ImVec2(440.0f, 320.0f))
{
}

void Disciples::Draw() noexcept
{
	bool shown = ecs::disciple_marks::IsShown();
	if (ImGui::Checkbox("Draw the glows and the symbol", &shown))
	{
		ecs::disciple_marks::SetShown(shown);
	}
	// the colour the symbol is drawn in
	const auto tint = disciple::TintOf(disciple::k_SymbolColour);
	ImGui::ColorButton("##symbol", ImVec4(tint.r, tint.g, tint.b, tint.a), ImGuiColorEditFlags_NoTooltip);
	ImGui::SameLine();
	ImGui::Text("Symbol colour 0x%08X", disciple::k_SymbolColour);
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No land loaded");
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	ImGui::SeparatorText("Disciples");
	int count = 0;
	registry.Each<const ecs::components::Villager>([&count, &registry](entt::entity entity,
	                                                                   const ecs::components::Villager& v) {
		if ((v.flags & ecs::components::Villager::k_FlagDisciple) == 0)
		{
			return;
		}
		++count;
		ImGui::Text("#%u: %s (picked up as %s)", entt::to_integral(entity), NameOf(v.discipleType), NameOf(v.discipleOnPickUp));
		if (v.discipleType == static_cast<uint8_t>(VillagerDisciple::Trader))
		{
			ImGui::SameLine();
			ImGui::Text("trades %s with #%u", v.tradeResource == ResourceType::Food ? "food" : "wood",
			            entt::to_integral(v.tradeTown));
		}
		// where its glow stands this frame, for a kind that glows: over the villager as it is drawn
		const auto* body = registry.TryGet<const ecs::components::Transform>(entity);
		if (body != nullptr && ecs::villager::DiscipleMarker(v.discipleType) != 0)
		{
			const auto glow = disciple::GlowPoint(ecs::DrawnPosition(registry, entity), body->scale.x);
			ImGui::TextUnformatted(fmt::format("    glow at {:.2f} {:.2f} {:.2f}", glow.x, glow.y, glow.z).c_str());
		}
		// a breeder: what it does, its spot, its partner and its pregnancy
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(entity);
		if (v.discipleType == static_cast<uint8_t>(VillagerDisciple::Breeder) && action != nullptr)
		{
			const auto partner =
			    v.targetThing == entt::null ? std::string("none") : fmt::format("#{}", entt::to_integral(v.targetThing));
			ImGui::TextUnformatted(fmt::format("    {}, spot {:.1f} {:.1f}, partner {}, pregnant {}", StateNameOf(*action),
			                                   map_coords::ToMetres(v.wanderArea.x), map_coords::ToMetres(v.wanderArea.z),
			                                   partner, v.pregnancy)
			                           .c_str());
		}
	});
	// the villagers making love out of doors, with their partners
	ImGui::SeparatorText("Making love");
	int lovers = 0;
	registry.Each<const ecs::components::Villager, const ecs::components::LivingAction>(
	    [&lovers](entt::entity entity, const ecs::components::Villager& v, const ecs::components::LivingAction& action) {
		    if (!IsLoveState(action))
		    {
			    return;
		    }
		    ++lovers;
		    ImGui::TextUnformatted(fmt::format("#{}: {} with #{}", entt::to_integral(entity), StateNameOf(action),
		                                       entt::to_integral(v.targetThing))
		                               .c_str());
	    });
	if (lovers == 0)
	{
		ImGui::TextDisabled("Nobody: make a breeder by a home");
	}
	// each town's room as the move into a town, the breeders and the house building read it now, beside the copy of
	// its last town turn
	ImGui::SeparatorText("Towns' room");
	registry.Each<const ecs::components::Town>([](entt::entity entity, const ecs::components::Town& town) {
		const auto room = ecs::town_stats::RoomOf(entity);
		ImGui::TextUnformatted(fmt::format("Town {}: {} free adult places (at its turn {}), places {} adults (at its turn "
		                                   "{}, the shuffle reads them now) {} children, room for a newcomer {}",
		                                   town.id, room.freeAdultPlaces, town.stats.freeAdultPlaces, room.adultPlaces,
		                                   town.stats.adultPlaces, room.childPlaces,
		                                   ecs::town_villagers::IsSpaceForNewVillager(entity) ? "yes" : "no")
		                           .c_str());
		// its buildings as the house and workshop building read them now, beside the town turn's copy
		const auto buildings = ecs::town_stats::BuildingsOf(entity);
		const auto houses = static_cast<size_t>(AbodeNumber::A);
		const auto workshops = static_cast<size_t>(AbodeNumber::Workshop);
		ImGui::TextUnformatted(fmt::format("  buildings: {} with places (at its turn {}), {} civic ({}), {} of number A ({}), "
		                                   "{} workshops ({})",
		                                   buildings.abodesWithPlaces, town.stats.abodesWithPlaces, buildings.civicBuildings,
		                                   town.stats.civicBuildings, buildings.abodesByNumber.at(houses),
		                                   town.stats.abodesByNumber.at(houses), buildings.abodesByNumber.at(workshops),
		                                   town.stats.abodesByNumber.at(workshops))
		                           .c_str());
		// the wood at its building sites as the wood desire reads it now, beside the town turn's copy
		ImGui::TextUnformatted(fmt::format("  wood at its sites: {:.0f} (at its turn {:.0f})",
		                                   ecs::town_stats::WoodAtSites(entity), town.stats.woodAtSites)
		                           .c_str());
	});
	// the towns preached to
	std::vector<entt::entity> towns;
	registry.Each<const ecs::components::Town>([&towns](entt::entity entity, const ecs::components::Town& town) {
		towns.push_back(entity);
		if (!town.missionaries.empty())
		{
			ImGui::Text("Town %u: %zu missionaries", town.id, town.missionaries.size());
		}
	});
	if (count == 0)
	{
		ImGui::TextDisabled("None: hold one of your villagers by a field, a tree or a building");
	}
	const auto held = Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().GetHeld() : std::nullopt;
	if (!held.has_value() || !registry.Valid(*held) || !registry.AllOf<ecs::components::Villager>(*held))
	{
		return;
	}
	// made a disciple of a kind here, and held as that kind, so that nothing round it changes it while it is in the hand
	const auto make = [&registry, held](VillagerDisciple type, entt::entity tradeTown) {
		ecs::villager::SetVillagerDisciple(*held, entt::null, type, 0);
		auto& v = registry.Get<ecs::components::Villager>(*held);
		v.discipleOnPickUp = static_cast<uint8_t>(type);
		v.tradeTown = tradeTown;
	};
	if (!towns.empty())
	{
		_tradeTown = std::clamp(_tradeTown, 0, static_cast<int>(towns.size()) - 1);
		const auto label = [&registry, &towns](int i) {
			return fmt::format("Town {}", registry.Get<const ecs::components::Town>(towns.at(static_cast<size_t>(i))).id);
		};
		if (ImGui::BeginCombo("Trade town", label(_tradeTown).c_str()))
		{
			for (int i = 0; i < static_cast<int>(towns.size()); ++i)
			{
				if (ImGui::Selectable(label(i).c_str(), i == _tradeTown))
				{
					_tradeTown = i;
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::Button("Make it a trader"))
		{
			make(VillagerDisciple::Trader, towns.at(static_cast<size_t>(_tradeTown)));
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("Make it a craftsman"))
	{
		make(VillagerDisciple::Craftsman, entt::null);
	}
	ImGui::SameLine();
	if (ImGui::Button("Make it a missionary"))
	{
		make(VillagerDisciple::Missionary, entt::null);
	}
	ImGui::SameLine();
	if (ImGui::Button("Make it a breeder"))
	{
		make(VillagerDisciple::Breeder, entt::null);
	}
	ImGui::SeparatorText("Round the villager in the hand, most interesting first");
	for (const auto thing : ecs::villager::DiscipleThingsNear(*held))
	{
		ImGui::Text("#%u", entt::to_integral(thing));
	}
}
