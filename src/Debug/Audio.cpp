/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Audio.h"

#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "Audio/Services/Advisor.h"
#include "Audio/Audio.h"
#include "Audio/GAudio/BankTables.h"
#include "Audio/GAudio/Banks.h"
#include "Audio/Device/Device.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/LH/MusicBank.h"
#include "Audio/LH/MusicStream.h"
#include "Audio/Device/WaveBuffers.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::debug::gui;

const ImVec4 k_RedColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
const ImVec4 k_GreenColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

namespace
{
/// GAudio's 16 sample channels (audio::sample_play) and the sample master volume
void SampleChannels()
{
	// The options dialog's slider 0x64: slider = master * (1 / 127) when it opens, master = ftol(slider * 127)
	// (DialogBoxOptions::ControlCallback 0x5145A3 -> fn_00428600 LHSampleSetMasterVolume)
	if (Locator::config::has_value())
	{
		auto& config = Locator::config::value();
		float slider = static_cast<float>(static_cast<double>(audio::SampleMasterVolume()) *
		                                  static_cast<double>(1.0f / 127.0f));
		if (ImGui::SliderFloat("Sample volume", &slider, 0.0f, 1.0f))
		{
			config.audioSampleMasterVolume =
			    static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(slider) * static_cast<double>(127.0f)));
		}
		ImGui::SameLine();
		ImGui::Text("AudioSampleMasterVolume %u", config.audioSampleMasterVolume);
	}
	ImGui::Text("Device (LHWaveIsInstalled) %s, active (LHWaveIsActive) %s, wave buffers %zu alive / %zu made",
	            audio::device::IsOpen() ? "yes" : "no", audio::sample_play::IsActive() ? "yes" : "no",
	            audio::wave_buffers::Alive(), audio::wave_buffers::Made());
	if (ImGui::BeginTable("SampleChannels", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		for (const char* name : {"#", "Sample", "Bank", "Owner", "Prio", "Vol", "Pitch", "3D", "Playing"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		const auto channels = audio::sample_play::Channels();
		for (size_t i = 0; i < channels.size(); ++i)
		{
			const auto& channel = channels[i];
			const auto* sound = audio::sample_play::GetSound(channel.sound);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%zu", i);
			ImGui::TableNextColumn();
			ImGui::Text("%d %s", channel.sample, sound != nullptr ? sound->name.c_str() : "");
			ImGui::TableNextColumn();
			ImGui::Text("%s", audio::BankGroup(static_cast<audio::BankId>(channel.bank)).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d:%u", static_cast<int>(channel.owner.kind),
			            channel.owner.kind == audio::Owner::Kind::Thing ? static_cast<uint32_t>(channel.owner.thing)
			                                                            : channel.owner.id);
			ImGui::TableNextColumn();
			ImGui::Text("%d", channel.priority);
			ImGui::TableNextColumn();
			ImGui::Text("%d", channel.volume);
			ImGui::TableNextColumn();
			ImGui::Text("%d", channel.pitch);
			ImGui::TableNextColumn();
			ImGui::Text("%s%s%s", channel.is3D ? "3D" : "2D", channel.track ? " track" : "", channel.atmos ? " atmos" : "");
			ImGui::TableNextColumn();
			ImGui::TextColored(channel.playing ? k_GreenColor : k_RedColor, "%s", channel.playing ? "yes" : "no");
		}
		ImGui::EndTable();
	}
}

/// The advisors' sentence (HelpDude, owner 0x270C on the channels: g_speaker 0xD15AA0, g_sentence 0xD15A9C); the other
/// voices are the channels of the owners 0x270D..0x270F above
void Voices()
{
	const int speaker = audio::advisor::Speaker();
	ImGui::Text("Advisors: speaker %d, sentence %d, talking good %s / evil %s, time %.2f s", speaker,
	            audio::advisor::Sentence(), audio::advisor::IsTalking(0) ? "yes" : "no",
	            audio::advisor::IsTalking(1) ? "yes" : "no", speaker >= 0 ? audio::advisor::SentenceTime(speaker) : 0.0f);
}

/// LHMusic's 6 channels (audio::music: LH_MusicInfo +0x24 status, +0x58 bank, +0x48 / +0x4C chunks, +0x34 / +0x30
/// volume)
void MusicChannels()
{
	auto* system = audio::music::Get();
	if (system == nullptr)
	{
		ImGui::Text("Music: not started");
		return;
	}
	if (ImGui::BeginTable("MusicChannels", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		for (const char* name : {"#", "Status", "Bank", "Chunk", "Volume"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		system->With([](audio::MusicEngine& engine) {
			for (int i = 0; i < audio::k_MusicChannelCount; ++i)
			{
				const auto& channel = engine.GetChannel(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				ImGui::Text("%d", static_cast<int>(channel.status));
				ImGui::TableNextColumn();
				ImGui::Text("%s", channel.bank != nullptr ? channel.bank->GetPath().filename().string().c_str() : "");
				ImGui::TableNextColumn();
				ImGui::Text("%u / %u", channel.playingChunk, channel.chunkCount);
				ImGui::TableNextColumn();
				ImGui::Text("%d -> %d", channel.current, channel.target);
			}
		});
		ImGui::EndTable();
	}
}
} // namespace

Audio::Audio() noexcept
    : Window("Audio Player", ImVec2(600.0f, 600.0f))
{
}

void Audio::PlaySelected() const noexcept
{
	if (_selectedSound == 0)
	{
		return;
	}
	// (openblack, debug) the sample as a 2D LH_SamplePlayOptions of no owner through GAudio::PlaySoundEffect 0x429E30:
	// the .sad decides its loops, mode and volume
	audio::PlayOptions options;
	options.sound = _selectedSound;
	options.owner = audio::Owner::None();
	options.is3D = false;
	audio::PlaySoundEffect(options);
}

void Audio::Sounds() noexcept
{
	using namespace std::literals;
	if (ImGui::Button("Play"))
	{
		PlaySelected();
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop all"))
	{
		audio::StopAllSoundEffects();
	}
	ImGui::Separator();
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::BeginChild("SoundPacks", ImVec2(ImGui::GetContentRegionAvail().x / 2, ImGui::GetContentRegionAvail().y),
	                  ImGuiChildFlags_Borders);
	if (ImGui::BeginTable("SoundPackTable", 2,
	                      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
	                          ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Sounds", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		// the banks LHBankRegister registered (audio::banks), by their sound group ("InGame.sad")
		for (size_t bank = 1; bank <= audio::banks::Count(); ++bank)
		{
			const auto id = static_cast<audio::BankId>(bank);
			const auto name = audio::BankGroup(id);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (ImGui::Selectable(name.c_str(), _selectedSoundPack == name, ImGuiSelectableFlags_SpanAllColumns))
			{
				_selectedSoundPack = name;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%zu", audio::banks::Samples(id).size());
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("Sounds", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
	                  ImGuiChildFlags_Borders);

	ImGui::Separator();
	const float extraPadding = ImGui::GetStyle().ItemSpacing.x * 2;
	const float firstColumnWidth = ImGui::CalcTextSize("123").x + extraPadding;
	const std::string lastColumnString = "Length (s)";
	const float lastColumnWidth = ImGui::CalcTextSize(lastColumnString.c_str()).x + extraPadding;

	if (ImGui::BeginTable("SoundTable", 3,
	                      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
	                          ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, firstColumnWidth);
		ImGui::TableSetupColumn("Name / Play", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(lastColumnString.c_str(), ImGuiTableColumnFlags_WidthFixed, lastColumnWidth);
		ImGui::TableHeadersRow();

		for (size_t bank = 1; bank <= audio::banks::Count(); ++bank)
		{
			const auto id = static_cast<audio::BankId>(bank);
			if (_selectedSoundPack != audio::BankGroup(id))
			{
				continue;
			}

			for (auto soundId : audio::banks::Samples(id))
			{
				auto sound = Locator::resources::value().GetSounds().Handle(soundId);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::Selectable(("##" + std::to_string(soundId)).c_str(), _selectedSound == soundId,
				                      ImGuiSelectableFlags_SpanAllColumns))
				{
					// Play the sound if already selected
					if (_selectedSound == soundId)
					{
						PlaySelected();
					}

					_selectedSound = soundId;
				}
				ImGui::SameLine();
				ImGui::Text("%u", sound->id);

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", sound->name.c_str());
				ImGui::TableSetColumnIndex(2);
				auto length = sound->duration;
				ImGui::TextColored(length < 0 ? k_RedColor : k_GreenColor, "%s",
				                   length < 0 ? "N/A" : std::to_string(length).c_str());
			}
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void Audio::Music() noexcept
{
	// The music types of 0x9C9748 started as the script's START_MUSIC 0x70FB20 does (GAudio::StartScriptMusic
	// 0x428230) and stopped as STOP_MUSIC 0x70FB90 (type 0): the music engine streams them (audio::music)
	if (ImGui::Button("Play") && _selectedMusicType != 0)
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->ScriptStartMusic(_selectedMusicType);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->ScriptStopMusic();
		}
	}
	ImGui::Separator();
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::BeginChild("MusicTypes", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
	                  ImGuiChildFlags_Borders);
	for (size_t type = 1; type < audio::k_MusicBanks.size(); ++type)
	{
		const auto& entry = audio::k_MusicBanks.at(type);
		const auto label = fmt::format("{:2} {} ({})", type, entry.name, entry.path);
		const bool selected = _selectedMusicType == static_cast<int>(type);
		if (ImGui::Selectable(label.c_str(), selected))
		{
			_selectedMusicType = static_cast<int>(type);
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void Audio::Draw() noexcept
{
	const ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
	if (ImGui::BeginTabBar("Tabs", tabBarFlags))
	{
		if (ImGui::BeginTabItem("Sound"))
		{
			ImGui::Text("View sound packs and their contents");
			ImGui::Separator();
			Audio::Sounds();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Music"))
		{
			ImGui::Text("The music types (START_MUSIC / STOP_MUSIC)");
			ImGui::Separator();
			Audio::Music();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Channels"))
		{
			ImGui::Text("GAudio's 16 sample channels (LHSamplePlay), the voices and LHMusic's 6 channels");
			ImGui::Separator();
			SampleChannels();
			ImGui::Separator();
			Voices();
			ImGui::Separator();
			MusicChannels();
			ImGui::EndTabItem();
		}
	}
	ImGui::EndTabBar();
	ImGui::Separator();
}

void Audio::Update() noexcept {}

void Audio::ProcessEventOpen(const SDL_Event&) noexcept {}

void Audio::ProcessEventAlways(const SDL_Event&) noexcept {}
