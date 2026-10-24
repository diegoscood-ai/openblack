/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Artefacts.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "ArtefactsModel.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Dances.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Town/ScaffoldPlans.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::artefacts;

namespace
{
/// The artefacts on the land
std::vector<ArtefactAt> ReadArtefacts(const ecs::Registry& registry)
{
	std::vector<ArtefactAt> artefacts;
	registry.Each<const ecs::components::TownArtefact>(
	    [&artefacts, &registry](entt::entity entity, const ecs::components::TownArtefact& artefact) {
		    std::optional<uint32_t> dancers;
		    size_t footpaths = 0;
		    if (ecs::dances::IsDance(registry, artefact.dance))
		    {
			    dancers = registry.Get<const ecs::components::Dance>(artefact.dance).dancers;
			    if (const auto link = ecs::footpaths::GetFootpathLink(artefact.dance); link != entt::null)
			    {
				    footpaths = registry.Get<const ecs::components::FootpathLink>(link).footpaths.size();
			    }
		    }
		    artefacts.push_back({
		        .entity = entity,
		        .town = artefact.town,
		        .site = artefact.site,
		        .inTown = artefact.inTown,
		        .inSite = artefact.inSite,
		        .player = artefact.player,
		        .value = artefact.value,
		        .dancing = artefact.dancing,
		        .dancers = dancers,
		        .footpaths = footpaths,
		    });
	    });
	return artefacts;
}
} // namespace

Artefacts::Artefacts() noexcept
    : Window("Artefacts", ImVec2(560.0f, 360.0f))
{
}

void Artefacts::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::artefactSystem::has_value())
	{
		ImGui::TextUnformatted("No artefacts: no land loaded");
		return;
	}
	const auto rows = Rows(ReadArtefacts(std::as_const(Locator::entitiesRegistry::value())));
	if (rows.empty())
	{
		ImGui::TextDisabled("No artefact on the land: put a rock down gently by a town");
	}
	// The objects the aura is laid over in the last frame drawn
	std::vector<entt::entity> glowing;
	if (Locator::particleSystem::has_value())
	{
		glowing = Locator::particleSystem::value().GetDrawStats().glowing;
	}
	auto raise = entt::entity {entt::null};
	float raisedTo = 0.0f;
	constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	if (!rows.empty() && ImGui::BeginTable("Artefacts", 8, k_Flags))
	{
		ImGui::TableSetupColumn("Entity");
		ImGui::TableSetupColumn("In");
		ImGui::TableSetupColumn("Owner");
		ImGui::TableSetupColumn("Worth");
		ImGui::TableSetupColumn("Shows");
		ImGui::TableSetupColumn("Dance");
		ImGui::TableSetupColumn("Aura");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		for (const auto& row : rows)
		{
			ImGui::PushID(static_cast<int>(entt::to_integral(row.entity)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", entt::to_integral(row.entity));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.where.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.owner.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", row.value);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.look.data(), row.look.data() + row.look.size());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.dance.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(std::ranges::find(glowing, row.entity) != glowing.end() ? "drawn over it" : "-");
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("+1 worth"))
			{
				raise = row.entity;
				raisedTo = row.value + k_WorthStep;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	// The aura: what it lies over, and a switch that leaves it undrawn (off by default; drawing only)
	ImGui::SeparatorText("Aura");
	if (glowing.empty())
	{
		ImGui::TextDisabled("Drawn over nothing: an artefact glows once worth more than 1");
	}
	for (const auto object : glowing)
	{
		ImGui::Text("Drawn over object %u", entt::to_integral(object));
	}
	if (Locator::debugHooks::has_value())
	{
		auto& shells = Locator::debugHooks::value().Get<particles::draw::ShellDebug>();
		ImGui::Checkbox("Hide the aura", &shells.hidden);
	}
	auto& system = Locator::artefactSystem::value();
	ImGui::SeparatorText("Owned in towns");
	for (int player = 0; player < static_cast<int>(PlayerNames::_COUNT); ++player)
	{
		if (const auto count = system.CountFor(static_cast<PlayerNames>(player)); count != 0)
		{
			ImGui::Text("Player %d: %u", player, count);
		}
	}
	// what a wonder planned at each town's centre would be worth: its desire for a wonder and the artefacts and
	// impressive things around (closed by default)
	if (ImGui::TreeNode("Wonder power at each town's centre"))
	{
		Locator::entitiesRegistry::value().Each<const ecs::components::Town>(
		    [](entt::entity town, const ecs::components::Town&) {
			    const auto centre = ecs::town_placement::GetTownAreaCentre(town);
			    ImGui::Text("Town %u: %.3f", entt::to_integral(town), ecs::scaffold_plans::GetWonderPower(town, centre));
		    });
		ImGui::TreePop();
	}
	// each player's tribal powers a built wonder raised above 1, as the next turn keeps them (closed by default)
	if (ImGui::TreeNode("Tribal powers raised by wonders"))
	{
		for (int player = 0; player < static_cast<int>(PlayerNames::_COUNT); ++player)
		{
			const auto& powers = magic::players::ReadMagicOf(static_cast<PlayerNames>(player));
			for (size_t tribe = 0; tribe < powers.maxTribalPower.size(); ++tribe)
			{
				if (powers.maxTribalPower.at(tribe) > 1.0f)
				{
					ImGui::Text("Player %d, tribe %zu: %.3f (now %.3f)", player, tribe, powers.maxTribalPower.at(tribe),
					            powers.tribalPower.at(tribe));
				}
			}
		}
		ImGui::TreePop();
	}
	// What a relaxing villager weighs first: its town's football pitch, which then wins over every artefact not wanted
	// more (no football is played, so a villager that chooses it does nothing)
	ImGui::SeparatorText("Relaxation");
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	registry.Each<const ecs::components::Town>([](entt::entity town, const ecs::components::Town& each) {
		const auto abodes = ecs::town_stats::AbodesOf(town);
		const auto pitch = std::ranges::find_if(
		    abodes, [](entt::entity abode) { return ecs::abodes::TypeOf(abode) == AbodeType::FootballPitch; });
		if (pitch != abodes.end())
		{
			ImGui::Text("Town %u: football pitch %u, weighed before its artefacts", each.id, entt::to_integral(*pitch));
		}
		else
		{
			ImGui::TextDisabled("Town %u: no football pitch, its artefacts only", each.id);
		}
	});
	if (raise != entt::null && Locator::entitiesRegistry::value().Valid(raise))
	{
		system.SetWorth(raise, raisedTo);
		_last = fmt::format("Artefact {} is worth {:.3f}", entt::to_integral(raise), raisedTo);
	}
	if (!_last.empty())
	{
		ImGui::Separator();
		ImGui::TextUnformatted(_last.c_str());
	}
}
