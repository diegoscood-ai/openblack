/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Consciences.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Audio/Services/Guidance.h"
#include "CHLApi.h"
#include "ConsciencesModel.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "GameClock.h"
#include "Help/AdvisorVoices.h"
#include "Help/HelpDudeFile.h"
#include "Help/HelpKey.h"
#include "Help/HelpMessageSets.h"
#include "Help/HelpSystem.h"
#include "Help/SpiritsRuntime.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::consciences;

namespace
{
constexpr std::array<std::string_view, 2> k_SpiritNames {"Good spirit", "Evil spirit"};
constexpr std::array k_Overrides {help::SpeakerOverride::None, help::SpeakerOverride::SilenceGood,
                                  help::SpeakerOverride::SilenceEvil, help::SpeakerOverride::ForceGood,
                                  help::SpeakerOverride::ForceEvil};
constexpr float k_MaxClipSpeed = 4.0f;
constexpr int k_MaxSample = 999;

/// The advisors' voices (Locator::helpTextSystem); stops with a message when there are none
help::AdvisorVoices& TheAdvisorVoices()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("consciences: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value().GetVoices();
}
} // namespace

Consciences::Consciences() noexcept
    : Window("Consciences", ImVec2(560.0f, 640.0f))
{
}

void Consciences::Draw() noexcept
{
	if (help::Get() == nullptr || !Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		ImGui::TextUnformatted("No spirits: the help system has not started");
		return;
	}
	DrawSpeaker();
	for (int dude = 0; dude < help::spirits::k_Dudes; ++dude)
	{
		DrawSpirit(dude);
	}
	DrawMessageSets();
	DrawTipBubble();
	DrawHelpKey();
	DrawGuidance();
}

void Consciences::DrawSpeaker() noexcept
{
	auto& help = *help::Get();
	ImGui::SeparatorText("Who speaks");
	const auto current = help.GetSpeakerOverride();
	if (ImGui::BeginCombo("Spirits' texts", OverrideName(current).data()))
	{
		for (const auto override : k_Overrides)
		{
			if (ImGui::Selectable(OverrideName(override).data(), override == current))
			{
				help.SetSpeakerOverride(override);
			}
		}
		ImGui::EndCombo();
	}
	if (current != help::SpeakerOverride::None)
	{
		ImGui::TextDisabled("Stays with the window closed, until changed or the next land");
	}
}

void Consciences::DrawSpirit(int dude) noexcept
{
	auto& advisors = Locator::advisorSystem::value();
	auto& control = advisors.GetController();
	const auto index = static_cast<size_t>(dude);
	ImGui::PushID(dude);
	if (!ImGui::CollapsingHeader(k_SpiritNames.at(index).data(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PopID();
		return;
	}
	const auto& spirit = control.Dude(dude);
	ImGui::Text("%s, %s; emotion %u, alpha %.2f, hover (%.2f, %.2f)", ControlStateName(control.State(dude)).data(),
	            DudeStateName(spirit.State()).c_str(), spirit.Emotion(), spirit.Alpha(), spirit.Hover().x, spirit.Hover().y);
	ImGui::TextDisabled("Pointing mode %d, looking %s%s", control.PointMode(dude), control.LookOn(dude) ? "on" : "off",
	                    control.Focus() == dude ? ", in focus" : "");

	if (ImGui::Button("Appear"))
	{
		control.Appear(dude);
	}
	ImGui::SameLine();
	if (ImGui::Button("Eject"))
	{
		control.Eject(dude);
	}
	ImGui::SameLine();
	if (ImGui::Button("Home"))
	{
		control.Home(dude);
	}
	ImGui::SameLine();
	if (ImGui::Button("Vanish"))
	{
		control.Vanish(dude);
	}

	if (const auto* file = advisors.GetFile(dude); file != nullptr)
	{
		const auto clips = ClipsOf(file->animNames);
		ImGui::SliderFloat("Clip speed", &_clipSpeed.at(index), 0.1f, k_MaxClipSpeed, "%.2f");
		const auto& screen = control.GetScreen();
		const auto spot = ClipSpot(dude, {screen.width, screen.height});
		if (ImGui::Button("Stop the clip"))
		{
			control.PlayAnim(dude, static_cast<float>(spot.x), static_cast<float>(spot.y), k_StopClipSlot, 1.0f);
		}
		if (ImGui::BeginListBox("Clips", ImVec2(-1.0f, 6.0f * ImGui::GetTextLineHeightWithSpacing())))
		{
			for (const auto& clip : clips)
			{
				const auto label = fmt::format("{} {}", clip.slot, clip.name);
				if (ImGui::Selectable(label.c_str()))
				{
					control.PlayAnim(dude, static_cast<float>(spot.x), static_cast<float>(spot.y), clip.slot,
					                 _clipSpeed.at(index));
				}
			}
			ImGui::EndListBox();
		}
		ImGui::TextDisabled("A clip plays only while the spirit is out");
	}
	else
	{
		ImGui::TextDisabled("Its file is not loaded");
	}

	ImGui::InputInt("Voice sample", &_sample.at(index));
	_sample.at(index) = std::clamp(_sample.at(index), 1, k_MaxSample);
	ImGui::SameLine();
	if (ImGui::Button("Say"))
	{
		const int advisor =
		    dude == help::spirits::k_GoodDude ? help::AdvisorVoices::k_GoodSpirit : help::AdvisorVoices::k_EvilSpirit;
		TheAdvisorVoices().Say(advisor, _sample.at(index), false);
	}
	ImGui::PopID();
}

void Consciences::DrawMessageSets() noexcept
{
	if (!ImGui::CollapsingHeader("Message sets", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	auto& help = *help::Get();
	ImGui::InputText("Search", &_search);
	if (!_last.empty())
	{
		ImGui::TextUnformatted(_last.c_str());
	}
	const auto& state = help.GetMessageSets();
	constexpr auto k_Flags =
	    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("Sets", 6, k_Flags, ImVec2(0.0f, 14.0f * ImGui::GetTextLineHeightWithSpacing())))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Set");
	ImGui::TableSetupColumn("Script");
	ImGui::TableSetupColumn("Texts");
	ImGui::TableSetupColumn("Does");
	ImGui::TableSetupColumn("Sent");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	for (uint32_t set = 0; set < help::message_sets::k_Sets; ++set)
	{
		const auto& entry = help::message_sets::Get(set);
		const SetRow row {.set = set,
		                  .first = entry.first,
		                  .last = entry.last,
		                  .mode = entry.mode,
		                  .category = entry.category,
		                  .script = entry.script,
		                  .sent = state.sentCount.at(set)};
		if (!MatchesSearch(row, _search))
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(set));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%u", row.set);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(row.script.data(), row.script.data() + row.script.size());
		ImGui::TableNextColumn();
		ImGui::Text("%u-%u", row.first, row.last);
		ImGui::TableNextColumn();
		const auto mode = SetModeName(row.mode);
		ImGui::TextUnformatted(mode.data(), mode.data() + mode.size());
		ImGui::TableNextColumn();
		ImGui::Text("%u", row.sent);
		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Run"))
		{
			if (!Locator::dialogueControlSystem::has_value())
			{
				_last = fmt::format("Set {} not run: there is no dialogue control", set);
			}
			else
			{
				const bool ran = help::message_sets::RunMessageSet(help, Locator::dialogueControlSystem::value(), set,
				                                                   chlapi::ScriptVm(), game_clock::Turn());
				_last =
				    ran ? fmt::format("Ran set {}", set) : fmt::format("Set {} not run: the help scripts would not stop", set);
			}
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void Consciences::DrawTipBubble() noexcept
{
	// Folded away until opened: closed, it reads nothing
	if (!ImGui::CollapsingHeader("Did-you-know bubble"))
	{
		return;
	}
	if (!Locator::tipBubbleSystem::has_value())
	{
		ImGui::TextUnformatted("There is no tip bubble");
		return;
	}
	auto& tipBubble = Locator::tipBubbleSystem::value();
	const auto sign = tipBubble.GetSign();
	if (sign == entt::null)
	{
		ImGui::TextUnformatted("Closed: no sign tapped");
		return;
	}
	const auto& bubble = tipBubble.GetBubble();
	ImGui::Text("Sign %u, text %u, %s", entt::to_integral(sign), tipBubble.GetText(),
	            tipBubble.IsUp() ? "up" : "hidden by a script's wide screen");
	ImGui::Text("Life %.3f, scroll %.2f lines, text height %.1f", bubble.Life(), bubble.Scroll(), bubble.TextHeight());
	if (const auto anchor = tipBubble.GetAnchor(); anchor.has_value())
	{
		ImGui::TextDisabled("Hangs from (%.2f, %.2f, %.2f)", anchor->x, anchor->y, anchor->z);
	}
	if (ImGui::Button("Close the bubble"))
	{
		tipBubble.Hide();
	}
}

void Consciences::DrawHelpKey() noexcept
{
	// Folded away until opened: closed, it reads nothing
	if (!ImGui::CollapsingHeader("Help key"))
	{
		return;
	}
	if (ImGui::Button("Ask, as the Help key does"))
	{
		if (Locator::dialogueControlSystem::has_value())
		{
			help::help_key::Ask(*help::Get(), Locator::dialogueControlSystem::value(), chlapi::ScriptVm(), game_clock::Turn());
		}
	}
	const auto& last = help::help_key::LastAnswer();
	ImGui::TextUnformatted(last.empty() ? "No answer yet" : last.c_str());
}

void Consciences::DrawGuidance() noexcept
{
	// Folded away until opened: closed, it reads nothing
	if (!ImGui::CollapsingHeader("Guidance remarks"))
	{
		return;
	}
	if (ImGui::Button("End the waits"))
	{
		audio::guidance::EndWaits();
	}
	const auto& state = audio::guidance::GetState();
	for (size_t type = 0; type < state.lastPlayed.size(); ++type)
	{
		ImGui::Text("Type %2zu: last played at turn %u, %u turns ago", type, state.lastPlayed.at(type),
		            audio::guidance::TimeSinceLastPlayed(static_cast<audio::guidance::Type>(type)));
	}
}

void Consciences::Update() noexcept {}

void Consciences::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Consciences::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
