/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSound.h"

#include <spdlog/spdlog.h>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

using namespace openblack;
using namespace openblack::audio;

SfxBank script_sound::BankType(int bank)
{
	if (bank <= static_cast<int>(SfxBank::None) || bank >= static_cast<int>(SfxBank::_COUNT))
	{
		return SfxBank::None;
	}
	return static_cast<SfxBank>(bank);
}

Channel script_sound::PlaySoundEffect(int sample, int bank, glm::vec3 position, bool withPosition)
{
	// 0x70F869: LH_SamplePlayOptions::ctor (the defaults of sample_play::Options)
	PlayOptions options;
	options.sample = {Bank(BankType(bank)), sample};      // +0x04 = GAudio+0x3A8 + 4 * bank (0x70F87D), +0x24 (0x70F879)
	options.owner = Owner::Key(static_cast<uint32_t>(sample)); // +0x20 = the sample number (0x70F89D)
	options.is3D = withPosition;                         // +0x08 (0x70F8B0)
	options.track = false;                               // +0x0C = 0 (0x70F8B4)
	options.position = position;                         // +0x30 / +0x34 / +0x38 (0x70F88C..0x70F8AC)
	options.keepPcm = true;                              // +0x164 = 1 (0x70F8A1)
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: PLAY_SOUND_EFFECT({}, {}, ({:.1f}, {:.1f}, {:.1f}), {})", sample, bank,
		                   position.x, position.y, position.z, withPosition ? 1 : 0);
	}
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	// 0x70F8BC: GAudio::PlaySoundEffect 0x429E30
	return audio::PlaySoundEffect(options);
}

void script_sound::StopSoundEffect(bool isSay, uint32_t id, int bank)
{
	if (!isSay)
	{
		// 0x70FB08..0x70FB11: StopPlayingSoundEffect(sample = id, owner = id, bank)
		audio::StopSoundEffect(static_cast<int>(id), Owner::Key(id), BankType(bank));
		return;
	}
	// 0x70FA8D..0x70FAAD: the narrator of HelpTextDatabase[0 < id < count ? id : 0] (helptext::GetEntry has that rule)
	const int32_t narrator = helptext::GetEntry(id).narrator;
	// 0x70FAB3 / 0x70FAB9: the say table 0x942B38 (+4 bank, +8 sample), not bound-checked: (approximated) an id past
	// the table has no voice here
	const auto voice = voices::Table().Get(id);
	const int sample = static_cast<int>(voice.sample);
	if (narrator == helptext::k_NarratorGoodSpirit)
	{
		// 0x70FAEF..0x70FAFD
		audio::StopSoundEffect(sample, Owner::Key(k_OwnerAdvisor), voice.bank);
		return;
	}
	// 0x70FAC4..0x70FAE4: 0x270E, then 0x270D
	audio::StopSoundEffect(sample, Owner::Key(k_OwnerVoiceStop), voice.bank);
	audio::StopSoundEffect(sample, Owner::Key(k_OwnerVoiceAlt), voice.bank);
}

bool script_sound::GameSoundPlaying(int sample, int bank)
{
	// 0x71025A..0x71025D: fn_0042A280(sample as the owner, sample, bank)
	return audio::IsPlaying(Owner::Key(static_cast<uint32_t>(sample)), sample, BankType(bank));
}

tags::TagId script_sound::AttachSoundTag(bool threeD, int sample, int bank, entt::entity thing)
{
	// 0x7101A3: nothing without a thing
	if (thing == entt::null)
	{
		return tags::k_NoTag;
	}
	// 0x7101A7..0x7101B9: SoundTag::Create(thing, sample, track (setne: threeD != 0), mode 2, loops 0, +0x40 0,
	// is3D threeD, bank, delay 0)
	return tags::Create(thing, sample, threeD, 2, 0, false, threeD, BankType(bank), 0);
}

void script_sound::DetachSoundTag(int sample, int bank, entt::entity thing)
{
	// 0x710212: nothing without a thing; 0x710217 SoundTag::Remove(thing, sample, bank)
	if (thing == entt::null)
	{
		return;
	}
	tags::Remove(thing, sample, BankType(bank));
}
