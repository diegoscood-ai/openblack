/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Magic.h"

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "Camera/Camera.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "Enums.h"
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

/// A readout of the players' computer minds: switched on, started on the land, the player named for the size of the
/// creature, and the player type. A button per player loads Khazar's creature file for them as a tortoise at the
/// camera's focus, through the script's LOAD_CREATURE, as the second land does
void DrawComputerPlayers()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!ImGui::BeginTable("ComputerPlayers", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		return;
	}
	ImGui::TableSetupColumn("Player");
	ImGui::TableSetupColumn("Switched on");
	ImGui::TableSetupColumn("Started");
	ImGui::TableSetupColumn("Creature like");
	ImGui::TableSetupColumn("Player type");
	ImGui::TableSetupColumn("Creature from a file");
	ImGui::TableHeadersRow();
	// the creature is made after the table, not while the players are walked
	std::optional<PlayerNames> load;
	registry.Each<const ecs::components::Player, const ecs::components::ComputerPlayer>(
	    [&registry, &load](entt::entity entity, const ecs::components::Player& player,
	                       const ecs::components::ComputerPlayer& mind) {
		    const auto name = [](PlayerNames each) {
			    const auto index = static_cast<size_t>(each);
			    return index < k_PlayerNamesStrs.size() ? std::string(k_PlayerNamesStrs.at(index)) : std::to_string(index);
		    };
		    const auto* magic = registry.TryGet<const ecs::components::PlayerMagic>(entity);
		    ImGui::TableNextRow();
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(name(player.name).c_str());
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(YesNo(mind.switchedOn).c_str());
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(YesNo(mind.started).c_str());
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(mind.creatureLike.has_value() ? name(*mind.creatureLike).c_str() : "-");
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(magic != nullptr ? std::to_string(magic->playerType).c_str() : "-");
		    ImGui::TableNextColumn();
		    ImGui::PushID(static_cast<int>(player.name));
		    if (ImGui::SmallButton("Load KhazarCreature"))
		    {
			    load = player.name;
		    }
		    ImGui::PopID();
	    });
	ImGui::EndTable();
	if (load.has_value() && Locator::camera::has_value())
	{
		// the tortoise's row of the creature tables
		constexpr int32_t k_Tortoise = 7;
		const auto focus = Locator::camera::value().GetFocus();
		ecs::player_creature::ScriptLoadCreature(k_Tortoise, "KhazarCreature", *load, glm::vec2(focus.x, focus.z));
	}
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
		if (ImGui::BeginTabItem("Computer players"))
		{
			DrawComputerPlayers();
			DrawAlliances();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void Magic::DrawAlliances() noexcept
{
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	ImGui::SeparatorText("Alliances");
	constexpr auto k_Players = static_cast<int>(PlayerNames::_COUNT);
	const auto name = [](int slot) { return std::string(k_PlayerNamesStrs.at(static_cast<size_t>(slot))); };
	// a row per player: their strength towards each player, blank where it is 0; allies (above 0) are marked
	if (ImGui::BeginTable("Alliances", k_Players + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("towards");
		for (int other = 0; other < k_Players; ++other)
		{
			ImGui::TableSetupColumn(name(other).c_str());
		}
		ImGui::TableHeadersRow();
		for (int player = 0; player < k_Players; ++player)
		{
			const auto alliances = ecs::alliances::AlliancesOf(static_cast<PlayerNames>(player));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(name(player).c_str());
			for (int other = 0; other < k_Players; ++other)
			{
				ImGui::TableNextColumn();
				const float strength = alliances.strength.at(static_cast<size_t>(other));
				if (strength != 0.0f)
				{
					const bool allied = ecs::alliances::IsAllied(alliances, static_cast<PlayerNames>(other));
					ImGui::TextUnformatted(fmt::format("{:.2f}{}", strength, allied ? " ally" : "").c_str());
				}
			}
		}
		ImGui::EndTable();
	}
	for (size_t i = 0; i < _allyPlayers.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::BeginCombo(i == 0 ? "Player" : "and player", name(_allyPlayers.at(i)).c_str()))
		{
			for (int slot = 0; slot < k_Players; ++slot)
			{
				if (ImGui::Selectable(name(slot).c_str(), slot == _allyPlayers.at(i)))
				{
					_allyPlayers.at(i) = slot;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
	}
	ImGui::InputFloat("Strength", &_allyStrength, 0.1f, 0.5f, "%.2f");
	if (ImGui::Button("Set alliance"))
	{
		ecs::alliances::SetAlliance(static_cast<PlayerNames>(_allyPlayers[0]), static_cast<PlayerNames>(_allyPlayers[1]),
		                            _allyStrength);
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
