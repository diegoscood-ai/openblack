/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSoundEffect.h"

namespace openblack::audio
{

namespace
{
constexpr bool Is(uint16_t userParam, SoundEffectUse use)
{
	return userParam == static_cast<uint16_t>(use);
}
} // namespace

bool SoundEffectHeard(const SoundEffectConditions& conditions, ScriptSoundBank bank, uint16_t userParam)
{
	if (conditions.scriptWideScreen && Is(userParam, SoundEffectUse::QuietInCutscenes))
	{
		return false;
	}
	if (conditions.insideTemple && !Is(userParam, SoundEffectUse::HeardInTemple))
	{
		return false;
	}
	if (!conditions.gameSoundOn && bank != ScriptSoundBank::HelpSprites && bank != ScriptSoundBank::Villagers)
	{
		return false;
	}
	return !(conditions.creatureFightControl && Is(userParam, SoundEffectUse::QuietInFights));
}

} // namespace openblack::audio
