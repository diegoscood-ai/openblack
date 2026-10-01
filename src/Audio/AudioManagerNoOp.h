/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "AudioManagerInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp", use interface instead.
#endif

namespace openblack::audio
{

class AudioManagerNoOp final: public AudioManagerInterface
{
public:
	const Sound& GetSound([[maybe_unused]] entt::id_type id) override
	{
		static const Sound result {};
		return result;
	}
	void Stop() override {}
	void Update() override {}
	void UpdateListener() override {}
	void CreateSoundGroup([[maybe_unused]] const std::string& name) override {}
	[[nodiscard]] SampleOutput& GetSampleOutput() override
	{
		static NullSampleOutput output;
		return output;
	}
	void AddToSoundGroup([[maybe_unused]] const std::string& name, [[maybe_unused]] entt::id_type id) override {}
	const SoundGroup& GetSoundGroup([[maybe_unused]] const std::string& name) override
	{
		static const SoundGroup result;
		return result;
	}
	const std::map<std::string, SoundGroup>& GetSoundGroups() override
	{
		static const std::map<std::string, SoundGroup> result;
		return result;
	}
};

} // namespace openblack::audio
