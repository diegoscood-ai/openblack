/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Magic.h"

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicModel.h"
#include "MiraclesCaster.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::magic_window;

namespace
{
void Row(std::string_view label, const std::string& value)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(value.c_str());
}

std::string YesNo(bool value)
{
	return value ? "yes" : "no";
}
} // namespace

Magic::Magic() noexcept
    : Window("Magic", ImVec2(560.0f, 640.0f))
    , _caster(std::make_unique<miracles::GameSpellCaster>())
    , _dispenserCreator(std::make_unique<miracles::GameDispenserCreator>())
{
}

void Magic::Draw() noexcept
{
	if (!Locator::infoConstants::has_value())
	{
		ImGui::TextUnformatted("No info.dat loaded");
		return;
	}
	if (ImGui::BeginTabBar("MagicTabs"))
	{
		if (ImGui::BeginTabItem("Running"))
		{
			DrawMiracles();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Miracle"))
		{
			DrawSelected();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("All miracles"))
		{
			DrawTable();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Particles"))
		{
			DrawParticles();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void Magic::DrawSelected() noexcept
{
	const auto& info = Locator::infoConstants::value();
	if (ImGui::BeginCombo("Magic type", MagicName(info, _selected).c_str()))
	{
		for (size_t i = 0; i < magic::k_MagicTypeCount; ++i)
		{
			const auto type = static_cast<MagicType>(i);
			if (ImGui::Selectable(MagicName(info, type).c_str(), type == _selected))
			{
				_selected = type;
			}
		}
		ImGui::EndCombo();
	}

	const auto& record = magic::GetMagicInfo(info, _selected);
	const auto& effect = magic::GetMagicEffectInfo(info, _selected);
	const auto slot = magic::SlotOf(_selected);
	const auto seed = magic::GetFirstSpellSeedForMagicType(info, _selected);
	const auto powerUp = magic::GetPowerUpGestureForMagicType(info, _selected);

	if (ImGui::BeginTable("MagicInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		Row("Section", fmt::format("{} #{}", SectionName(slot.section), slot.index));
		Row("Cost to create", fmt::format("{:.0f}", effect.costToCreate));
		Row("Prayer power when cast", fmt::format("{:.0f}", effect.initialChants));
		Row("Upkeep per turn", fmt::format("{:.1f}", effect.costPerGameTurn));
		Row("Cost per event", fmt::format("{:.1f}", effect.costPerEvent));
		Row("Cost per shield impact", fmt::format("{:.1f}", effect.costPerShieldCollide));
		Row("Maintained", YesNo(magic::IsMaintainedSpell(_selected)));
		Row("Recharged by its caster", YesNo(record.isSpellRecharged != 0));
		Row("Cheaper with tribal power", YesNo(effect.divideCostsByTribalPower == 1));
		Row("Timer, one shot", Seconds(effect.timerWhenOneShot));
		Row("Timer, player", Seconds(effect.timerWhenPlayerCasting));
		Row("Timer, creature", Seconds(effect.timerWhenCreatureCasting));
		Row("Timer, computer player", Seconds(effect.timerWhenComputerPlayerCasting));
		Row("Creature casts from above", YesNo(magic::IsCreatureCastFromAbove(info, _selected)));
		Row("Aggressive range", fmt::format("{:.0f} to {:.0f}", effect.agressiveRangeMin, effect.agressiveRangeMax));
		Row("Perceived power", fmt::format("{:.2f}", record.perceivedPower));
		Row("First seed", SeedName(info, seed));
		Row("Power-up level", PowerUpLevelName(powerUp.level));
		Row("Power-up gesture", fmt::format("{}", static_cast<uint32_t>(powerUp.gesture)));
		if (const auto* creatureSpell = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, _selected))
		{
			Row("Total duration", Seconds(creatureSpell->totalDuration));
		}
		if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, _selected))
		{
			Row("Radius", fmt::format("{:.0f} to {:.0f} (normal cost at {:.0f})", radius->minRadius, radius->maxRadius,
			                          radius->radiusForNormalCost));
		}
		ImGui::EndTable();
	}
}

void Magic::DrawTable() noexcept
{
	const auto& info = Locator::infoConstants::value();
	constexpr auto k_Flags =
	    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("AllMagic", 7, k_Flags))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(1, 1);
	ImGui::TableSetupColumn("Magic type");
	ImGui::TableSetupColumn("Create");
	ImGui::TableSetupColumn("When cast");
	ImGui::TableSetupColumn("Per turn");
	ImGui::TableSetupColumn("Per event");
	ImGui::TableSetupColumn("Player");
	ImGui::TableSetupColumn("Creature");
	ImGui::TableHeadersRow();
	for (size_t i = 0; i < magic::k_MagicTypeCount; ++i)
	{
		const auto type = static_cast<MagicType>(i);
		const auto& effect = magic::GetMagicEffectInfo(info, type);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable(MagicName(info, type).c_str(), type == _selected, ImGuiSelectableFlags_SpanAllColumns))
		{
			_selected = type;
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", static_cast<double>(effect.costToCreate));
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", static_cast<double>(effect.initialChants));
		ImGui::TableNextColumn();
		ImGui::Text("%.1f", static_cast<double>(effect.costPerGameTurn));
		ImGui::TableNextColumn();
		ImGui::Text("%.1f", static_cast<double>(effect.costPerEvent));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Seconds(effect.timerWhenPlayerCasting).c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Seconds(effect.timerWhenCreatureCasting).c_str());
	}
	ImGui::EndTable();
}

void Magic::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
