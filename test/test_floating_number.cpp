/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Common/FixedFormat.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Registry.h"

using namespace openblack;
namespace fn = openblack::ecs::floating_number;

TEST(FixedFormat, WholeNumbersThreeWideRoundHalfAwayFromNothing)
{
	EXPECT_EQ(fixed_format::Fixed(30.0, 3, 0), " 30");
	EXPECT_EQ(fixed_format::Fixed(100.0, 3, 0), "100");
	EXPECT_EQ(fixed_format::Fixed(5.0, 3, 0), "  5");
	// The game's C library rounds a half up
	EXPECT_EQ(fixed_format::Fixed(12.5, 3, 0), " 13");
	EXPECT_EQ(fixed_format::Fixed(0.5, 3, 0), "  1");
	EXPECT_EQ(fixed_format::Fixed(12.49, 3, 0), " 12");
	EXPECT_EQ(fixed_format::Fixed(1234.0, 3, 0), "1234");
	EXPECT_EQ(fixed_format::Fixed(-2.5, 3, 0), " -3");
	EXPECT_EQ(fixed_format::Fixed(3.14159, 0, 2), "3.14");
	EXPECT_EQ(fixed_format::Fixed(0.05, 0, 1), "0.1");
}

// The original's C library takes the number to 17 significant digits before it rounds at the precision, so a number
// held a little under a half rounds down, and a negative zero has no sign
TEST(FixedFormat, ItRoundsTheNumberAsHeldAsTheOriginalDoes)
{
	EXPECT_EQ(fixed_format::Fixed(-0.0, 3, 0), "  0");
	EXPECT_EQ(fixed_format::Fixed(0.49999999999999994, 3, 0), "  0");
	EXPECT_EQ(fixed_format::Fixed(2.675, 0, 2), "2.67");
	EXPECT_EQ(fixed_format::Fixed(0.15, 0, 1), "0.1");
	// A negative number that rounds to zero keeps its sign
	EXPECT_EQ(fixed_format::Fixed(-0.4, 3, 0), " -0");
	EXPECT_EQ(fixed_format::Fixed(-0.0, 0, 3), "0.000");
}

// Cases written by the original game's C library, the double given by its bits. They include the library's rounding of an
// exact half in the 18th digit, which goes down for some numbers and up for others, and its words for an infinity and
// a NaN.
TEST(FixedFormat, ItWritesWhatTheOriginalWrites)
{
	struct Case
	{
		uint64_t bits;
		int width;
		int precision;
		std::string_view written;
	};
	constexpr std::array k_Cases {
	    Case {0x0000000000000000, 3, 0, "  0"},                        // 0.0
	    Case {0xBDDB7CDFD9D7BDBB, 0, 2, "-0.00"},                      // -1e-10
	    Case {0x3FE0000000000000, 3, 0, "  1"},                        // 0.5
	    Case {0x4004000000000000, 3, 0, "  3"},                        // 2.5
	    Case {0xC004000000000000, 3, 0, " -3"},                        // -2.5
	    Case {0x3FC0000000000000, 0, 2, "0.13"},                       // 0.125
	    Case {0xBFC0000000000000, 0, 2, "-0.13"},                      // -0.125
	    Case {0x3FD8000000000000, 0, 2, "0.38"},                       // 0.375
	    Case {0x3F789374BC6A7EFA, 0, 2, "0.01"},                       // 0.006
	    Case {0x3F3A36E2EB1C432D, 0, 2, "0.00"},                       // 0.0004
	    Case {0x4058E00000000000, 3, 0, "100"},                        // 99.5
	    Case {0x408F3FFFFFF29407, 0, 3, "1000.000"},                   // 999.9999999
	    Case {0x4480F0CF064DD592, 0, 0, "10000000000000000000000"},    // 1e+22
	    Case {0x44B52D02C7E14AF6, 0, 2, "99999999999999992000000.00"}, // 1e+23
	    Case {0x4450000000000000, 0, 0, "1180591620717411300000"},     // 1.1805916207174113e+21
	    Case {0x0000000000000001, 0, 9, "0.000000000"},                // 5e-324
	    Case {0x3FB999999999999A, 0, 9, "0.100000000"},                // 0.1
	    Case {0x3FD5555555555555, 0, 9, "0.333333333"},                // 0.3333333333333333
	    Case {0x419D6F34547E6B75, 0, 9, "123456789.123456790"},        // 123456789.12345679
	    Case {0x3FF0147AE147AE14, 0, 2, "1.00"},                       // 1.005
	    Case {0x3E90C6F7A0B5ED8D, 0, 6, "0.000000"},                   // 2.5e-07
	    Case {0x42BE7010EE346150, 12, 7, "33466669216865.3120000"},    // 33466669216865.312
	    Case {0x431B64C44D66EEF1, 0, 1, "1927654661536700.3"},         // 1927654661536700.2
	    Case {0xC2E420443C47C60C, 0, 7, "-177030530481712.3700000"},   // -177030530481712.38
	    Case {0x3F5F53B80DF8E707, 0, 7, "0.0019121"},                  // 0.00191205
	    Case {0xC0B05CAA3D70A3D7, 12, 2, "    -4188.67"},              // -4188.665
	    Case {0x7FF0000000000000, 0, 2, "1.#J"},                       // inf
	    Case {0xFFF0000000000000, 5, 0, "   -1"},                      // -inf
	    Case {0x7FF0000000000000, 0, 9, "1.#INF00000"},                // inf
	    Case {0x7FF8000000000000, 12, 3, "       1.#QO"},              // nan
	    Case {0xFFF8000000000000, 0, 3, "-1.#IO"},                     // nan
	    Case {0x400FED74927913E8, 3, 5, "3.99095"},                    // 3.990945
	    Case {0x411EBD23DC28F5C3, 12, 4, " 503624.9650"},              // 503624.965
	    Case {0xC192085ADE000000, 12, 3, "-75634359.500"},             // -75634359.5
	    Case {0x411C2ED600000000, 5, 0, "461750"},                     // 461749.5
	    Case {0x40BABB8000000000, 3, 0, "6844"},                       // 6843.5
	    Case {0xC0F0FEDA00000000, 12, 7, "-69613.6250000"},            // -69613.625
	    Case {0x40E3F054CCCCCCCD, 12, 1, "     40834.7"},              // 40834.65
	    Case {0x015C2220E196A67D, 5, 6, "0.000000"},                   // 4.102468536925693e-302
	    Case {0x40641333F430C050, 0, 4, "160.6001"},                   // 160.60009202501033
	    Case {0x40B0E98391000000, 5, 0, " 4330"},                      // 4329.513931274414
	    Case {0xC0FE623000000000, 3, 5, "-124451.00000"},              // -124451.0
	    Case {0xC0ACF15EB851EB85, 0, 2, "-3704.68"},                   // -3704.685
	    Case {0xC04222C3C9EECBFB, 5, 4, "-36.2716"},                   // -36.2716
	    Case {0x406089F6A0000000, 0, 4, "132.3114"},                   // 132.3113555908203
	    Case {0x41566625C999999A, 5, 4, "5871767.1500"},               // 5871767.15
	    Case {0x4126AA277D70A3D7, 0, 6, "742675.745000"},              // 742675.745
	    Case {0x4104F1EC00000000, 0, 0, "171582"},                     // 171581.5
	};
	for (const auto& [bits, width, precision, written] : k_Cases)
	{
		EXPECT_EQ(fixed_format::Fixed(std::bit_cast<double>(bits), width, precision), written)
		    << std::hex << bits << " %" << std::dec << width << "." << precision << "f";
	}
}

TEST(FixedFormat, ANumberIsWrittenIntoTheWords)
{
	EXPECT_EQ(fixed_format::WithNumber(u"People Worshipping: %3.0f%%", 40.0), u"People Worshipping:  40%");
	EXPECT_EQ(fixed_format::WithNumber(u"Amount: %3.0f", 7.5), u"Amount:   8");
	EXPECT_EQ(fixed_format::WithNumber(u"No number", 7.0), u"No number");
	EXPECT_EQ(fixed_format::WithNumber(u"%d stays", 7.0), u"%d stays");
}

TEST(FloatingNumber, ItRisesTenUnitsASecondForFiveSeconds)
{
	ecs::components::FloatingNumber number {.text = " 30", .position = glm::vec3(0.0f, 2.0f, 0.0f)};
	EXPECT_TRUE(fn::Step(number, 0.5f));
	EXPECT_FLOAT_EQ(number.position.y, 7.0f);
	EXPECT_FLOAT_EQ(number.life, 4.5f);
	EXPECT_TRUE(fn::Step(number, 4.5f));
	EXPECT_FALSE(fn::Step(number, 0.01f));
}

TEST(FloatingNumber, ItFadesInItsLastSecond)
{
	EXPECT_EQ(fn::Alpha(5.0f), 0xFF);
	EXPECT_EQ(fn::Alpha(1.0f), 0xFF);
	EXPECT_EQ(fn::Alpha(0.5f), 127);
	EXPECT_FALSE(fn::Alpha(0.003f).has_value());
	EXPECT_FALSE(fn::Alpha(-0.1f).has_value());
}

TEST(FloatingNumber, ItsTextEndsAtItsPoint)
{
	EXPECT_EQ(fn::TextPlace(glm::vec2(100.0f, 50.0f), 24.0f), glm::vec2(76.0f, 50.0f));
}

TEST(FloatingNumber, ItIsPlacedAtTheWholePixelOnTheScreen)
{
	const glm::ivec2 screen {640, 480};
	EXPECT_EQ(fn::ScreenPixel(glm::vec3(100.7f, 50.2f, 0.5f), screen), glm::vec2(100.0f, 50.0f));
	EXPECT_EQ(fn::ScreenPixel(glm::vec3(639.9f, 479.9f, 0.5f), screen), glm::vec2(639.0f, 479.0f));
	EXPECT_EQ(fn::ScreenPixel(glm::vec3(0.0f, 0.0f, 0.5f), screen), glm::vec2(0.0f, 0.0f));
	EXPECT_FALSE(fn::ScreenPixel(glm::vec3(640.0f, 10.0f, 0.5f), screen).has_value());
	EXPECT_FALSE(fn::ScreenPixel(glm::vec3(10.0f, 480.0f, 0.5f), screen).has_value());
	EXPECT_FALSE(fn::ScreenPixel(glm::vec3(-1.5f, 10.0f, 0.5f), screen).has_value());
}

TEST(FloatingNumber, ThoseWhoseTimeIsUpGo)
{
	ecs::Registry registry;
	const auto early = registry.Create();
	registry.Assign<ecs::components::FloatingNumber>(early, ecs::components::FloatingNumber {.life = 0.5f});
	const auto late = registry.Create();
	registry.Assign<ecs::components::FloatingNumber>(late, ecs::components::FloatingNumber {.life = 5.0f});
	fn::StepAll(registry, 1.0f);
	EXPECT_FALSE(registry.Valid(early));
	EXPECT_TRUE(registry.Valid(late));
}
