/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Audio.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <imgui.h>

#include "Audio/Audio.h"
#include "Audio/Device/Device.h"
#include "Audio/Device/OutputSwitches.h"
#include "Audio/Device/WaveBuffers.h"
#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/QMixerLaws.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameMusic.h"
#include "Audio/MainVolumesSetup.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/SoundMap.h"
#include "Common/MachineSetup.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AbodeSounds.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/ThingFunctional.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Villager/VillagerChild.h"
#include "ECS/Weather/Climate.h"
#include "EngineConfig.h"
#include "Help/AdvisorVoices.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::debug::gui;

constexpr ImVec4 k_RedColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
constexpr ImVec4 k_GreenColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

namespace
{
/// The advisors' voices (Locator::helpTextSystem); stops with a message when there are none
help::AdvisorVoices& TheAdvisorVoices()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("audio window: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value().GetVoices();
}

/// The switches that silence the game's sounds and music for testing: off, their channels play at gain 0. They live
/// with the running audio, so closing the window leaves them as they are
void GameSwitches()
{
	if (!Locator::audioState::has_value())
	{
		return;
	}
	// A pause given in the next turn, as a script gives one, to set beside the pause key's, given from the frame
	if (Locator::config::has_value() && MenuClick(ImGui::Button("Pause at the end of this turn")))
	{
		Locator::config::value().pauseInNextTurn = true;
	}
	auto& switches = audio::GetOutputSwitches();
	bool sounds = switches.sounds;
	if (MenuClick(ImGui::Checkbox("Game sounds", &sounds)))
	{
		switches.sounds = sounds;
	}
	ImGui::SameLine();
	bool music = switches.music;
	if (MenuClick(ImGui::Checkbox("Game music", &music)))
	{
		switches.music = music;
	}
	ImGui::Separator();
}

/// The 16 sample channels (audio::sample_play) and the sample main volume
/// The .sad fields a channel's start applied (sample_play::AppliedOverrides), by name
std::string AppliedFields(uint32_t overrides)
{
	constexpr std::array<std::string_view, 11> k_Fields {"pitch", "pan", "x",   "y",     "z",   "volume",
	                                                     "loops", "min", "max", "scale", "mode"};
	std::string names;
	for (size_t bit = 0; bit < k_Fields.size(); ++bit)
	{
		if ((overrides & (1u << bit)) != 0)
		{
			names += names.empty() ? "" : " ";
			names += k_Fields.at(bit);
		}
	}
	return names.empty() ? "-" : names;
}

void SampleChannels()
{
	// As the options dialog's slider: slider = main * (1 / 127) when it opens, main = int(slider * 127)
	if (Locator::config::has_value())
	{
		auto& config = Locator::config::value();
		float slider = static_cast<float>(static_cast<double>(audio::SampleMainVolume()) * static_cast<double>(1.0f / 127.0f));
		if (ImGui::SliderFloat("Sample volume", &slider, 0.0f, 1.0f))
		{
			config.audioSampleMainVolume =
			    static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(slider) * static_cast<double>(127.0f)));
		}
		ImGui::SameLine();
		ImGui::Text("AudioSampleMasterVolume %u", config.audioSampleMainVolume);
		// The two main volumes kept for this computer (the options box reads them as it shows and keeps them as it goes)
		if (const auto folder = audio::main_volumes::Folder(); folder.has_value())
		{
			ImGui::Text("Kept in %s", (*folder / machine_setup::k_SetupFile).generic_string().c_str());
			if (ImGui::Button("Read the kept volumes"))
			{
				audio::main_volumes::Read();
			}
			ImGui::SameLine();
			if (ImGui::Button("Keep the volumes now"))
			{
				audio::main_volumes::Keep();
			}
		}
		else
		{
			ImGui::TextUnformatted("The main volumes are not kept in this run");
		}
	}
	ImGui::Text("Device (LHWaveIsInstalled) %s, active (LHWaveIsActive) %s, wave buffers %zu alive / %zu made",
	            audio::device::IsOpen() ? "yes" : "no", audio::sample_play::IsActive() ? "yes" : "no",
	            audio::wave_buffers::Alive(), audio::wave_buffers::Made());
	if (ImGui::BeginTable("SampleChannels", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		for (const char* name : {"#", "Sample", "Bank", "Owner", "Prio", "Vol", "Pitch", "3D", "Playing", "From the .sad"})
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
			ImGui::TableNextColumn();
			ImGui::Text("%s", AppliedFields(channel.overrides).c_str());
		}
		ImGui::EndTable();
	}
}

/// The advisors' sentence (owner 0x270C on the channels: the speaker and the sentence); the other voices are the
/// channels of the owners 0x270D..0x270F above
void Voices()
{
	auto& advisors = TheAdvisorVoices();
	const int speaker = advisors.GetSpeaker();
	ImGui::Text("Advisors: speaker %d, sentence %d, talking good %s / evil %s, time %.2f s", speaker, advisors.GetSentence(),
	            advisors.IsTalking(0) ? "yes" : "no", advisors.IsTalking(1) ? "yes" : "no",
	            speaker >= 0 ? advisors.SentenceTime(speaker) : 0.0f);
	// the last spooky voice's pan, the one 2D sound heard off centre (90 ahead before the first)
	if (Locator::audioState::has_value())
	{
		const int pan = audio::spooky::GetState().options.pan;
		ImGui::Text("Spooky voice: pan %d (azimuth %d)", pan, audio::qmixer::PanAzimuth(pan));
	}
}

/// The music engine's 6 channels (audio::music: status, bank, chunks and volume)
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

constexpr ImGuiTableFlags k_AtmosTableFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

/// An atmos type's name without its "ATMOS_TYPE_" prefix
std::string_view AtmosTypeName(size_t type)
{
	constexpr std::string_view k_Prefix = "ATMOS_TYPE_";
	const std::string_view name = audio::k_AtmosTypes.at(type).name;
	return name.starts_with(k_Prefix) ? name.substr(k_Prefix.size()) : name;
}

/// A bank sample as "<id> (<name>)", or its hash when it is not loaded
std::string AtmosSampleName(entt::id_type sample)
{
	if (Locator::resources::has_value())
	{
		auto& sounds = Locator::resources::value().GetSounds();
		if (sounds.Contains(sample))
		{
			const auto sound = sounds.Handle(sample);
			return fmt::format("{} ({})", sound->id, sound->name);
		}
	}
	return fmt::format("#{}", sample);
}

/// The ambient banks (audio::atmos_banks), read only: each bank's target and volume, the loops, the loose samples due
/// next and the ones playing, then the climates
void Atmos()
{
	const auto view = audio::atmos_banks::GetView();
	if (!view.initialised)
	{
		ImGui::TextUnformatted("The ambient banks are not registered yet (no land played)");
		return;
	}
	const auto group = view.banks.at(0).group;
	ImGui::Text("Turn counter %u  group %u (%s)  loops %zu  loose samples %zu  playing %zu", view.counter, group,
	            group == 2 ? "evil" : "good", view.loops.size(), view.queue.size(), view.playing.size());

	if (ImGui::CollapsingHeader("Banks", ImGuiTreeNodeFlags_DefaultOpen) &&
	    ImGui::BeginTable("AtmosBanks", 5, k_AtmosTableFlags, ImVec2(0.0f, 280.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"Type", "Target", "Current", "Volume (0-127)", "Group"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		// NONE has no bank
		for (size_t i = 1; i < audio::k_AtmosTypeCount; ++i)
		{
			const auto& bank = view.banks.at(i);
			const auto name = AtmosTypeName(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (!bank.registered)
			{
				ImGui::TextColored(k_RedColor, "%.*s (no bank)", static_cast<int>(name.size()), name.data());
			}
			else if (bank.volume > 0)
			{
				ImGui::TextColored(k_GreenColor, "%.*s", static_cast<int>(name.size()), name.data());
			}
			else
			{
				ImGui::Text("%.*s", static_cast<int>(name.size()), name.data());
			}
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", static_cast<double>(bank.target));
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", static_cast<double>(bank.current));
			ImGui::TableNextColumn();
			ImGui::ProgressBar(static_cast<float>(bank.volume) / 127.0f, ImVec2(-1.0f, 0.0f),
			                   std::to_string(bank.volume).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%u", bank.group);
		}
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Loops") && ImGui::BeginTable("AtmosLoops", 5, k_AtmosTableFlags, ImVec2(0.0f, 180.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"Sample", "Bank", "Group", "Fade / volume", "Playing"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		for (const auto& loop : view.loops)
		{
			const auto bank = AtmosTypeName(loop.bank);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(AtmosSampleName(loop.sample).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%.*s", static_cast<int>(bank.size()), bank.data());
			ImGui::TableNextColumn();
			ImGui::Text("%u", loop.group);
			ImGui::TableNextColumn();
			ImGui::Text("%d / %d", loop.fade, loop.volume);
			ImGui::TableNextColumn();
			ImGui::TextColored(loop.playing ? k_GreenColor : k_RedColor, "%s", loop.playing ? "yes" : "no");
		}
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Upcoming loose samples") &&
	    ImGui::BeginTable("AtmosQueue", 5, k_AtmosTableFlags, ImVec2(0.0f, 180.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"Sample", "Bank", "Group", "Frequency", "In turns"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		for (const auto& loose : view.queue)
		{
			const auto bank = AtmosTypeName(loose.bank);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(AtmosSampleName(loose.sample).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%.*s", static_cast<int>(bank.size()), bank.data());
			ImGui::TableNextColumn();
			ImGui::Text("%u", loose.group);
			ImGui::TableNextColumn();
			ImGui::Text("%d", loose.frequency);
			ImGui::TableNextColumn();
			ImGui::Text("%d", static_cast<int32_t>(loose.next - view.counter));
		}
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Playing loose samples") &&
	    ImGui::BeginTable("AtmosPlaying", 3, k_AtmosTableFlags, ImVec2(0.0f, 120.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"Bank", "Group", "Volume (0-127)"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		for (const auto& playing : view.playing)
		{
			const auto bank = AtmosTypeName(playing.bank);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%.*s", static_cast<int>(bank.size()), bank.data());
			ImGui::TableNextColumn();
			ImGui::Text("%u", playing.group);
			ImGui::TableNextColumn();
			ImGui::ProgressBar(static_cast<float>(playing.volume) / 127.0f, ImVec2(-1.0f, 0.0f),
			                   std::to_string(playing.volume).c_str());
		}
		ImGui::EndTable();
	}

	// The storms are in the Weather window
	if (Locator::weatherSystem::has_value() && ImGui::CollapsingHeader("Climates") &&
	    ImGui::BeginTable("AtmosClimates", 6, k_AtmosTableFlags, ImVec2(0.0f, 140.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"Climate", "Centre / radius", "Rain desire", "Raining days", "Temperature", "Wind"})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		weather::climate::ForEach([](const weather::climate::Climate& climate) {
			const auto centre = climate.Centre();
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d%s (info %d)  storms %zu / %d", climate.id, climate.world ? " world" : "", climate.info,
			            climate.storms.size(), climate.maxStorms);
			ImGui::TableNextColumn();
			ImGui::Text("(%.0f, %.0f) r %.0f", static_cast<double>(centre.x), static_cast<double>(centre.z),
			            static_cast<double>(climate.outerRadius));
			ImGui::TableNextColumn();
			ImGui::ProgressBar(climate.rain.desire, ImVec2(-1.0f, 0.0f));
			ImGui::TableNextColumn();
			ImGui::Text("%d%s", climate.rain.rainingDays, (climate.rain.flags & 1) != 0 ? " falling" : "");
			ImGui::TableNextColumn();
			ImGui::Text("%.1f -> %.1f", static_cast<double>(climate.temperature),
			            static_cast<double>(climate.targetTemperature));
			ImGui::TableNextColumn();
			ImGui::Text("(%.0f, %.0f)", static_cast<double>(climate.windX), static_cast<double>(climate.windZ));
		});
		ImGui::EndTable();
	}
}

/// The buildings' own sounds: each windmill's hum (its tag, whether the windmill works, whether the hum plays) and a
/// button for each creche's children's laugh
void BuildingSounds()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No land.");
		return;
	}
	using ecs::systems::AbodeSounds;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* tags = Locator::soundTagSystem::has_value() ? &Locator::soundTagSystem::value() : nullptr;
	ImGui::TextUnformatted("Windmills' hum");
	if (ImGui::BeginTable("Windmills", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Windmill");
		ImGui::TableSetupColumn("Tag");
		ImGui::TableSetupColumn("Works");
		ImGui::TableSetupColumn("Hum");
		ImGui::TableHeadersRow();
		registry.Each<const ecs::components::Abode>([tags](entt::entity entity, const ecs::components::Abode&) {
			if (ecs::abodes::TypeOf(entity) != AbodeType::Windmill)
			{
				return;
			}
			const auto tag = tags != nullptr
			                     ? tags->FindThingTag(entity, AbodeSounds::k_WindmillSample, AbodeSounds::k_WindmillBank)
			                     : std::nullopt;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", static_cast<uint32_t>(entity));
			ImGui::TableNextColumn();
			if (tag.has_value())
			{
				ImGui::Text("%u", static_cast<uint32_t>(*tag));
			}
			else
			{
				ImGui::TextUnformatted("none");
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(ecs::abode_queries::IsFunctional(entity) ? "yes" : "no");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(tag.has_value() && tags->Playing(*tag) ? "playing" : "silent");
		});
		ImGui::EndTable();
	}
	ImGui::Separator();
	// a village totem's rising sound, whose tag ends the turn the totem stops working
	ImGui::TextUnformatted("Village totems' rising sound");
	if (ImGui::BeginTable("Totems", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Totem");
		ImGui::TableSetupColumn("Tag");
		ImGui::TableSetupColumn("Works");
		ImGui::TableSetupColumn("Sound");
		ImGui::TableHeadersRow();
		registry.Each<const ecs::components::VillageTotem>([tags](entt::entity entity, const ecs::components::VillageTotem&) {
			constexpr int k_TotemRisingSample = 0xB;
			const auto tag =
			    tags != nullptr ? tags->FindThingTag(entity, k_TotemRisingSample, audio::SfxBank::InGame) : std::nullopt;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", static_cast<uint32_t>(entity));
			ImGui::TableNextColumn();
			if (tag.has_value())
			{
				ImGui::Text("%u", static_cast<uint32_t>(*tag));
			}
			else
			{
				ImGui::TextUnformatted("none");
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(ecs::thing_functional::OfThing(entity) ? "yes" : "no");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(tag.has_value() && tags->Playing(*tag) ? "playing" : "silent");
		});
		ImGui::EndTable();
	}
	ImGui::Separator();
	// the buildings' ambience of the time of day, rolled as each is drawn
	if (ImGui::TreeNode("Buildings' ambience"))
	{
		if (Locator::abodeSoundSystem::has_value())
		{
			ImGui::Text("Sounds played since the game started: %u", Locator::abodeSoundSystem::value().GetSoundCount());
		}
		else
		{
			ImGui::TextUnformatted("No building sounds service.");
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Creches' laugh (as a child at the creche plays it)");
	registry.Each<const ecs::components::Abode>([](entt::entity entity, const ecs::components::Abode&) {
		if (ecs::abodes::TypeOf(entity) != AbodeType::Creche)
		{
			return;
		}
		const auto label = fmt::format("Laugh at creche {}", static_cast<uint32_t>(entity));
		if (MenuClick(ImGui::Button(label.c_str())) && Locator::dynamicsSystem::has_value())
		{
			ecs::villager::PlayCrecheLaugh(entity);
		}
	});
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
	// (openblack, debug) the sample played 2D with no owner through audio::PlaySoundEffect: the .sad decides its loops,
	// mode and volume
	audio::PlayOptions options;
	options.sound = _selectedSound;
	options.owner = audio::Owner::None();
	options.is3D = false;
	options.pan = _selectedPan;
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
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
	ImGui::SliderInt("2D pan", &_selectedPan, 0, 180);
	ImGui::SetItemTooltip("Where Play is heard: 0 on the left, 90 ahead (as every 2D sample but the spooky voices), 180 "
	                      "on the right");
	ImGui::SameLine();
	// The scrolls' and signs' click as a script activates them (the "did you know" signs play none)
	ImGui::Text("Scroll activation clicks: %u", ecs::script_highlight::ActivationClicks());
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

		// the registered banks (audio::banks), by their sound group ("InGame.sad")
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
	// The music types started as the script's START_MUSIC does and stopped as STOP_MUSIC (type 0): the music engine
	// streams them (audio::music)
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
	GameSwitches();
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
		if (ImGui::BeginTabItem("Atmos"))
		{
			ImGui::Text("The ambient banks, their loops and loose samples, and the climates");
			ImGui::Separator();
			Atmos();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Buildings"))
		{
			ImGui::Text("The buildings' own sounds");
			ImGui::Separator();
			BuildingSounds();
			ImGui::EndTabItem();
		}
	}
	ImGui::EndTabBar();
	ImGui::Separator();
}

void Audio::Update() noexcept {}

void Audio::ProcessEventOpen(const SDL_Event&) noexcept {}

void Audio::ProcessEventAlways(const SDL_Event&) noexcept {}
