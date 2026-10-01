/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mods/BuiltinMods.h"

#include <memory>

#include "EngineConfig.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"

// At every new game runblack.exe v1.42 asks whether to skip the tutorial (GGame::DoYesNoSkipTutorialRequestersIfNecessary
// 0x54CBD0 shows the SkipBox, four answers, the first one chosen by default). openblack draws no SkipBox and always
// plays everything, as that default answer; this mod gives the box's second or third answer instead. The script
// itself then skips: SetupLand1 reads CAN_SKIP_TUTORIAL / CAN_SKIP_CREATURE_TRAINING and LandControl1 leaves out
// FollowUs and CitadelGuide (the citadel is built at once) and ChooseYourCreature, and goes straight to the creature
// choice in the glade (CreaturesInGlade); see docs/bw1-notes/map-loading.md.

namespace openblack::mods
{
namespace
{
class SkipIntroMod final: public Mod
{
public:
	SkipIntroMod()
	    : Mod({"game.skip-intro", "Skip the intro",
	           "Gives the original's answer \"skip the tutorial\" to its skip-tutorial question at the start of a new "
	           "game (openblack does not ask it): Land 1 starts without the opening (FollowUs) and the citadel guide, "
	           "and goes straight to choosing the creature, as in the original. Optionally the creature training "
	           "is skipped too",
	           "Game", true})
	{
		AddOption({"skip", "Skip", {"tutorial", "tutorial and creature training"}, 0});
	}

	void Apply() override
	{
		// The SkipBox answers (callback 0x544480): 1 = bit 23 of g_game+0x14, 2 = bits 23 and 24
		const auto& skip = GetChoice("skip");
		Locator::config::value().skipTutorialChoice = !IsEnabled() ? 0 : (skip == "tutorial" ? 1 : 2);
	}
};
} // namespace

void RegisterSkipIntroMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<SkipIntroMod>());
}

} // namespace openblack::mods
