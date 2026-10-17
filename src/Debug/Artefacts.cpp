/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Artefacts.h"

#include <utility>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "ArtefactsModel.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "Locator.h"

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
		    if (ecs::dances::IsDance(registry, artefact.dance))
		    {
			    dancers = registry.Get<const ecs::components::Dance>(artefact.dance).dancers;
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
	auto raise = entt::entity {entt::null};
	float raisedTo = 0.0f;
	constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	if (!rows.empty() && ImGui::BeginTable("Artefacts", 7, k_Flags))
	{
		ImGui::TableSetupColumn("Entity");
		ImGui::TableSetupColumn("In");
		ImGui::TableSetupColumn("Owner");
		ImGui::TableSetupColumn("Worth");
		ImGui::TableSetupColumn("Shows");
		ImGui::TableSetupColumn("Dance");
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
			if (ImGui::SmallButton("+1 worth"))
			{
				raise = row.entity;
				raisedTo = row.value + k_WorthStep;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
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
