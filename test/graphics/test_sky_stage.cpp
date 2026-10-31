/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::sky_stage (src/Graphics/SkyStage.h): what the sky stage of the main pass, the sea's reflection and the
// temple draws, and in what order (docs/wiki/world/sky.md#sky-sun-moon-and-clouds-original)

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/RenderPass.h"
#include "Graphics/SkyStage.h"

using namespace openblack::graphics;
using Part = sky_stage::Part;

namespace
{
std::vector<Part> PartsOf(std::span<const Part> parts)
{
	return {parts.begin(), parts.end()};
}
} // namespace

TEST(SkyStage, TheMainPassDrawsTheDomeTheSunAndTheMoonThenQueuesTheClouds)
{
	// The sun after the dome, the moon after the sun, before the land; the clouds go to the sorted queue of blended
	// things. Nothing else: no star field, no earthshine, no flares
	EXPECT_EQ(PartsOf(sky_stage::Parts(RenderPass::Main)),
	          (std::vector<Part> {Part::Dome, Part::Sun, Part::Moon, Part::Clouds}));
}

TEST(SkyStage, TheSeasSkyHasTheMoonButNoSun)
{
	EXPECT_EQ(PartsOf(sky_stage::Parts(RenderPass::Reflection)), (std::vector<Part> {Part::Dome, Part::Moon}));
	// no other pass draws a sun or a moon
	for (const auto pass : {RenderPass::MainBlended, RenderPass::ScreenOverlay, RenderPass::TempleMap})
	{
		EXPECT_EQ(PartsOf(sky_stage::Parts(pass)), (std::vector<Part> {Part::Dome})) << static_cast<int>(pass);
	}
}

TEST(SkyStage, InsideTheTempleTheDomeAndTheMoonAreDrawnWithoutTheSun)
{
	const auto temple = PartsOf(sky_stage::TempleParts());
	EXPECT_EQ(temple, (std::vector<Part> {Part::Dome, Part::Moon}));
	EXPECT_EQ(std::ranges::find(temple, Part::Sun), temple.end());
}
