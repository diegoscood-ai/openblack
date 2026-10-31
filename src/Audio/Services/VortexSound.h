/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "Audio/Device/Sound.h"
#include "Audio/Services/SoundTags.h"
#include "Enums.h"

// The sound a landscape vortex keeps up from its creation (docs/wiki/audio/engine.md#pending).
namespace openblack::audio::vortex_sound
{

/// The volcano's rumble: InGame.sad sample 172 (G_Volcano_02), as the sound id the tags take
inline constexpr auto k_VolcanoSample = static_cast<entt::id_type>(SoundId::G_Volcano_02);

/// The tag a vortex gives itself when it is made: only the volcano has one, a 3D loop of its rumble on the vortex
/// itself, not tracked, mode 2, in the in-game bank, with no delay (what sound_tags::Create gives), kept until the
/// vortex goes. The In and the Out are silent.
[[nodiscard]] inline std::optional<sound_tags::TagDesc> CreationTag(VortexType type, entt::entity vortex)
{
	if (type != VortexType::Volcano)
	{
		return std::nullopt;
	}
	return sound_tags::TagDesc {.sample = k_VolcanoSample, .thing = vortex, .point = {}, .loop = true, .active = true};
}

} // namespace openblack::audio::vortex_sound
