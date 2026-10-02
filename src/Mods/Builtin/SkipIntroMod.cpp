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
// plays everything, as that default answer; this mod gives one of the other three answers instead. The script itself
// then skips: SetupLand1 reads CAN_SKIP_TUTORIAL / CAN_SKIP_CREATURE_TRAINING / IS_KEEPING_OLD_CREATURE
// (challenge.chl 25398..25455) and LandControl1 leaves out FollowUs, CitadelGuide (the citadel is built at once) and
// ChooseYourCreature. The fourth answer also leaves out CreaturesInGlade, the creature choice in the glade: and that
// is the one that takes the camera and the dialogue, fades to black, flies the camera and plays START_MUSIC(63)
// (challenge.chl 44028..44691). See docs/bw1-notes/map-loading.md.
//
// Even with the fourth answer the script still runs CreatureDevSeeHome, which in its skipping branch (challenge.chl
// 7056..7085) takes the camera and the dialogue for one tick, flicks the wide screen on and off, snaps the camera over
// the village and fades in. Dropping that is what the "free start" option does; it is **not** original.

namespace openblack::mods
{
namespace
{
class SkipIntroMod final: public Mod
{
public:
	SkipIntroMod()
	    : Mod({"game.skip-intro", "Skip the intro",
	           "Gives one of the original's \"skip\" answers to the skip-tutorial question it asked at the start of a "
	           "new game (openblack does not ask it): Land 1 starts without the opening (FollowUs) and the citadel "
	           "guide, with the citadel already built, and with the last answer without the creature glade either. "
	           "\"Free start\" goes further than the original and leaves the opening to the player: nothing moves the "
	           "camera, locks the interface or plays the script's music",
	           "Game", true, true})
	{
		AddOption({"skip",
		           "Skip",
		           {k_SkipTutorial, k_SkipCreatureTraining, k_SkipEverything},
		           2}); // the fourth answer: the only one that leaves the opening to the player
		AddOption({"free start", "Free start", {"on", "off"}, 0});
	}

	void Apply() override
	{
		// The SkipBox answers (callback 0x544480, jump table 0x5445A0): 1 = bit 23 of g_game+0x14, 2 = bits 23 and 24,
		// 3 = bits 23, 24 and 25. With 3, CHLApi also answers the CURRENT_PROFILE_HAS_CREATURE that SetupLand1 ands
		// with bit 25 (challenge.chl 25432..25434); openblack has no player profiles, so the mod answers for them
		const auto& skip = GetChoice("skip");
		int choice = 3;
		if (skip == k_SkipTutorial)
		{
			choice = 1;
		}
		else if (skip == k_SkipCreatureTraining)
		{
			choice = 2;
		}
		auto& config = Locator::config::value();
		config.skipTutorialChoice = IsEnabled() ? choice : 0;
		config.skipIntroFreeStart = IsEnabled() && GetChoice("free start") == "on";
	}

private:
	static constexpr const char* k_SkipTutorial = "tutorial";
	static constexpr const char* k_SkipCreatureTraining = "tutorial and creature training";
	static constexpr const char* k_SkipEverything = "tutorial, creature training and the glade";
};
} // namespace

void RegisterSkipIntroMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<SkipIntroMod>());
}

} // namespace openblack::mods
