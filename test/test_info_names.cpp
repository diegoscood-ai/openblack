/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The names the land scripts look up in the game's tables ignore case: an abode by its tribe and its description, a
// feature and an animated static by their descriptions, whatever the case of the name the script gives.
// docs/wiki/project/progress/engine/file_system_and_packs.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
template <size_t N>
void Name(std::array<char, N>& field, std::string_view name)
{
	field = {};
	std::memcpy(field.data(), name.data(), std::min(name.size(), N - 1));
}

class InfoNames: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto info = std::make_unique<InfoConstants>();
		for (auto& abode : info->abode)
		{
			abode.tribeType = Tribe::NONE;
		}
		auto& abode = info->abode.at(4);
		abode.tribeType = Tribe::NORSE;
		Name(abode.debugString, "Abode A");
		Name(info->feature.at(3).debugString, "Pillar Chalk");
		Name(info->animatedStatic.at(2).debugString, "Norse Gate");
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// An abode is found by its tribe's name and its description joined by an underscore, in any case
TEST_F(InfoNames, AnAbodeIsFoundByItsTribeAndDescriptionInAnyCase)
{
	EXPECT_EQ(GAbodeInfo::Find("NORSE_Abode A"), static_cast<AbodeInfo>(4));
	EXPECT_EQ(GAbodeInfo::Find("norse_abode a"), static_cast<AbodeInfo>(4));
	EXPECT_EQ(GAbodeInfo::Find("NoRsE_ABODE A"), static_cast<AbodeInfo>(4));
	EXPECT_EQ(GAbodeInfo::Find("CELTIC_Abode A"), AbodeInfo::None);
	EXPECT_EQ(GAbodeInfo::Find("NORSE Abode A"), AbodeInfo::None);
}

// A feature and an animated static are found by their descriptions, in any case
TEST_F(InfoNames, AFeatureAndAnAnimatedStaticAreFoundByTheirDescriptionsInAnyCase)
{
	EXPECT_EQ(GFeatureInfo::Find("PILLAR CHALK"), static_cast<FeatureInfo>(3));
	EXPECT_EQ(GFeatureInfo::Find("pillar chalk"), static_cast<FeatureInfo>(3));
	EXPECT_EQ(GFeatureInfo::Find("Pillar Chalky"), FeatureInfo::None);
	EXPECT_EQ(GAnimatedStaticInfo::Find("NORSE GATE"), static_cast<AnimatedStaticInfo>(2));
	EXPECT_EQ(GAnimatedStaticInfo::Find("norse gate"), static_cast<AnimatedStaticInfo>(2));
	EXPECT_EQ(GAnimatedStaticInfo::Find("Norse"), AnimatedStaticInfo::None);
}
