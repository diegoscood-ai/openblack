/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <optional>
#include <vector>

#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/Moon.h"

namespace moon = openblack::graphics::moon;

namespace
{
/// 6 January 2000, a new moon, in days since 1970
constexpr int64_t k_NewMoonDay = 0x2AD2;

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float epsilon = 1e-5f)
{
	EXPECT_NEAR(a.x, b.x, epsilon);
	EXPECT_NEAR(a.y, b.y, epsilon);
	EXPECT_NEAR(a.z, b.z, epsilon);
}

/// The game's folder (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH), when one is set
std::optional<std::filesystem::path> GameFolder()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	if (game == nullptr || *game == '\0')
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// The face's texture coordinates as the celestial shader makes them from a point of the mesh and the face inputs
glm::vec2 FaceCoordinates(const openblack::l3d::L3DPoint& point, const glm::vec4& inputs)
{
	return (glm::vec2((point.x * inputs.x) - (point.z * inputs.y), point.y) * 0.0025f) + 0.25f;
}
} // namespace

TEST(Moon, ShowsAroundMidnight)
{
	const auto midnight = moon::Place(0.0f);
	ASSERT_TRUE(midnight.has_value());
	EXPECT_FLOAT_EQ(midnight->offset.x, 4000.0f);
	EXPECT_FLOAT_EQ(midnight->offset.y, 950.0f);
	EXPECT_NEAR(midnight->offset.z, 0.0f, 1e-3f);
	EXPECT_FLOAT_EQ(midnight->alpha, 200.0f);

	EXPECT_FALSE(moon::Place(6.0f).has_value());
	EXPECT_FALSE(moon::Place(12.0f).has_value());
	EXPECT_TRUE(moon::Place(23.0f).has_value());
}

TEST(Moon, PhaseFollowsTheRealMoon)
{
	// A whole turn at a new moon, then half way through a moon month of about 29.5 days
	EXPECT_NEAR(moon::Phase(k_NewMoonDay * 86400), 6.2831855f, 1e-5f);
	EXPECT_NEAR(moon::Phase((k_NewMoonDay + 15) * 86400 + 3600), (1.0f - (15.0f * 0.03386318f)) * 6.2831855f, 1e-4f);
}

TEST(Moon, FacesTheCamera)
{
	const glm::vec3 eye {0.0f, 100.0f, 0.0f};
	const auto view = glm::lookAt(eye, glm::vec3(4000.0f, 1000.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::vec3 position = eye + glm::vec3(4000.0f, 950.0f, 0.0f);
	const auto basis = moon::Basis(view, glm::inverse(view), position);
	// Four times the mesh's size, its axes square to each other, its third along the line from the camera
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(glm::length(basis[i]), 4.0f, 1e-4f);
	}
	EXPECT_NEAR(glm::dot(basis[0], basis[1]), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(glm::normalize(basis[2]), glm::normalize(position - eye)), 1.0f, 1e-4f);

	const auto model = moon::Model(basis, position, 0.0f);
	EXPECT_NEAR(glm::length(glm::vec3(model[0])), 4.0f * 0.65f, 1e-4f);
	EXPECT_EQ(glm::vec3(model[3]), position);
}

TEST(Moon, FacesTheEyeInTheView)
{
	const glm::mat4 identity(1.0f);
	// Straight ahead: the view's own axes, four times
	auto basis = moon::Basis(identity, identity, glm::vec3(0.0f, 0.0f, 100.0f));
	ExpectNear(basis[0], glm::vec3(4.0f, 0.0f, 0.0f));
	ExpectNear(basis[1], glm::vec3(0.0f, 4.0f, 0.0f));
	ExpectNear(basis[2], glm::vec3(0.0f, 0.0f, 4.0f));
	// Off to the right: the third axis points from the eye to the moon, the first stays level and square to it
	basis = moon::Basis(identity, identity, glm::vec3(100.0f, 0.0f, 100.0f));
	ExpectNear(glm::normalize(basis[2]), glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)));
	EXPECT_NEAR(basis[0].y, 0.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(basis[0], basis[2]), 0.0f, 1e-4f);
	// The glow's first corner has the first texture coordinates
	basis = moon::Basis(identity, identity, glm::vec3(0.0f, 0.0f, 100.0f));
	const auto glow = moon::MakeGlow(basis, glm::vec3(0.0f, 0.0f, 100.0f));
	ExpectNear(glow.corners[0], glm::vec3(-2000.0f, -2000.0f, 100.0f), 1e-2f);
	ExpectNear(glow.corners[3], glm::vec3(2000.0f, 2000.0f, 100.0f), 1e-2f);
	EXPECT_FLOAT_EQ(glow.uvs[0].y, 0.25f);
	EXPECT_FLOAT_EQ(glow.uvs[2].y, 0.49375f);
	// The mesh leans its first axis up by about 7.5 degrees, at 0.65 of the size
	const auto model = moon::Model(basis, glm::vec3(0.0f, 0.0f, 100.0f), -std::numbers::pi_v<float>);
	EXPECT_NEAR(model[0].y, 4.0f * 0.65f * std::sin(0.1308997f), 1e-4f);
	EXPECT_NEAR(model[0].x, 4.0f * 0.65f * std::cos(0.1308997f), 1e-4f);
	// The turn by the phase and a half turn, here a quarter turn: the first axis becomes the third
	const auto turned = moon::Model(glm::mat3(1.0f), glm::vec3(0.0f), -std::numbers::pi_v<float> * 0.5f);
	EXPECT_NEAR(turned[0].z, 0.65f, 1e-4f);
	EXPECT_NEAR(turned[2].x, -0.65f * std::cos(0.1308997f), 1e-4f);
}

TEST(Moon, GlowIsASquareAboutTheMoon)
{
	const glm::mat3 basis(4.0f);
	const auto glow = moon::MakeGlow(basis, glm::vec3(0.0f));
	EXPECT_EQ(glow.corners[0], glm::vec3(-2000.0f, -2000.0f, 0.0f));
	EXPECT_EQ(glow.corners[3], glm::vec3(2000.0f, 2000.0f, 0.0f));
	EXPECT_EQ(moon::GlowColour(glm::vec3(0.6f, 0.5f, 0.4f)), glm::vec3(0.1f, 0.1f, 0.1f));
}

TEST(Moon, TheSeaShowsTheGlowOfACopyMirroredThroughSeaLevel)
{
	const glm::vec3 eye {0.0f, 100.0f, 0.0f};
	const auto view = glm::lookAt(eye, glm::vec3(4000.0f, 1000.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto inverseView = glm::inverse(view);
	const glm::vec3 position = eye + glm::vec3(4000.0f, 950.0f, 0.0f);
	const auto sea = moon::SeaGlow(view, inverseView, position);

	// Mirrored back through sea level, the corners are the glow of a copy below the sea, 1050 down, facing the camera
	const glm::vec3 copy {position.x, -position.y, position.z};
	const auto expected = moon::MakeGlow(moon::Basis(view, inverseView, copy), copy);
	for (size_t i = 0; i < sea.corners.size(); ++i)
	{
		const glm::vec3 mirrored {sea.corners.at(i).x, -sea.corners.at(i).y, sea.corners.at(i).z};
		EXPECT_EQ(mirrored, expected.corners.at(i));
		EXPECT_EQ(sea.uvs.at(i), expected.uvs.at(i));
	}
	// The copy's glow faces the camera from below: square to the line from the camera to the copy
	const auto across = expected.corners[1] - expected.corners[0];
	const auto up = expected.corners[2] - expected.corners[0];
	const auto towardsCopy = glm::normalize(copy - eye);
	EXPECT_NEAR(glm::dot(glm::normalize(across), towardsCopy), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(glm::normalize(up), towardsCopy), 0.0f, 1e-4f);
	// It is not merely the sky's glow mirrored, which would face a camera mirrored through the sea
	const auto skyGlow = moon::MakeGlow(moon::Basis(view, inverseView, position), position);
	EXPECT_NE(sea.corners[2], skyGlow.corners[2]);
}

TEST(Moon, ScriptsAreToldHowNewTheMoonIs)
{
	// 1 at either end of the moon month, 0 half way through it
	EXPECT_FLOAT_EQ(moon::ScriptPercentage(0.0f), 1.0f);
	EXPECT_NEAR(moon::ScriptPercentage(3.14159274f), 0.0f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(6.2831855f), 1.0f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(1.5707964f), 0.5f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(4.712389f), 0.5f, 1e-6f);
}

TEST(Moon, MonthFractionCountsWholeDays)
{
	EXPECT_DOUBLE_EQ(moon::MonthFraction(k_NewMoonDay * 86400), 0.0);
	// Any time of the same day gives the same fraction
	EXPECT_DOUBLE_EQ(moon::MonthFraction((k_NewMoonDay + 10) * 86400),
	                 moon::MonthFraction((k_NewMoonDay + 10) * 86400 + 86399));
	EXPECT_NEAR(moon::MonthFraction((k_NewMoonDay + 10) * 86400), 10.0 * 0.03386318012808897, 1e-12);
}

TEST(Moon, DateAtFractionFindsTheNearestDay)
{
	const int64_t start = (k_NewMoonDay + 100) * 86400;
	for (const double fraction : {0.0, 0.25, 0.5, 0.75})
	{
		const auto date = moon::DateAtFraction(start, fraction);
		// Within half a moon month, at noon, and within half a day's step of the point asked for
		EXPECT_LE(std::llabs(date - start), 16 * 86400);
		EXPECT_EQ(date % 86400, 43200);
		const double apart = std::abs(moon::MonthFraction(date) - fraction);
		EXPECT_LE(std::min(apart, 1.0 - apart), 0.5 * 0.0338632);
	}
}

TEST(Moon, SkyAnglesFromTheCamera)
{
	// At midnight it is due east, about 13 degrees up
	const auto midnight = moon::Angles(moon::Offset(0.0f));
	EXPECT_NEAR(midnight.azimuth, 90.0f, 1e-3f);
	EXPECT_NEAR(midnight.elevation, 13.36f, 0.01f);
	// In the evening it is south of east, and in the morning north of east
	EXPECT_GT(moon::Angles(moon::Offset(20.0f)).azimuth, 90.0f);
	EXPECT_LT(moon::Angles(moon::Offset(4.0f)).azimuth, 90.0f);
}

TEST(Moon, TheTintsAlphaIsTheStrengthAndItsColourStaysPut)
{
	const glm::vec3 colour(0.25f, 0.5f, 0.75f);
	for (const float strength : {0.0f, 50.0f, 120.0f, 200.0f})
	{
		const auto tint = moon::Tint(colour, strength);
		EXPECT_EQ(glm::vec3(tint), colour) << strength;
		EXPECT_FLOAT_EQ(tint.a, strength / 255.0f) << strength;
		// the glow, a dim copy, at the same strength
		const auto glow = moon::GlowTint(colour, strength);
		EXPECT_EQ(glm::vec3(glow), moon::GlowColour(colour)) << strength;
		EXPECT_FLOAT_EQ(glow.a, strength / 255.0f) << strength;
	}
}

TEST(Moon, TheGlowIsAddedToTheSky)
{
	const uint64_t state = moon::GlowState();
	EXPECT_EQ(state & BGFX_STATE_BLEND_MASK, BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
	EXPECT_EQ(state & BGFX_STATE_WRITE_Z, 0u);
}

TEST(Moon, TheLandDrawnLaterCoversIt)
{
	namespace render_modes = openblack::graphics::render_modes;
	const uint64_t state = moon::State(render_modes::Cull::None);
	// Blended by its alpha, the depth written and tested less than or equal (openblack's inverted Z: greater or equal)
	EXPECT_EQ(state & BGFX_STATE_BLEND_MASK, BGFX_STATE_BLEND_ALPHA);
	EXPECT_NE(state & BGFX_STATE_WRITE_Z, 0u);
	EXPECT_EQ(state & BGFX_STATE_DEPTH_TEST_MASK, BGFX_STATE_DEPTH_TEST_GEQUAL);
	EXPECT_EQ(state & BGFX_STATE_CULL_MASK, 0u);
	// culled as the pass asks, flipped in the sea's reflection
	EXPECT_EQ(moon::State(render_modes::Cull::Ccw) & BGFX_STATE_CULL_MASK, BGFX_STATE_CULL_CCW);
	EXPECT_EQ(moon::State(render_modes::Cull::Cw) & BGFX_STATE_CULL_MASK, BGFX_STATE_CULL_CW);
}

// The moon's place is worked out from the hour the sky was given alone: every frame drawn with the same hour, as all the
// frames of a game turn are, finds the same offset and strength, to the bit
TEST(Moon, TheOffsetIsTheSameInEveryFrameOfATurn)
{
	for (const float hour : {0.0f, 2.25f, 20.5f, 23.9f})
	{
		const auto first = moon::Place(hour);
		const auto again = moon::Place(hour);
		ASSERT_TRUE(first.has_value()) << hour;
		ASSERT_TRUE(again.has_value()) << hour;
		for (int axis = 0; axis < 3; ++axis)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(again->offset[axis]), std::bit_cast<uint32_t>(first->offset[axis])) << hour;
		}
		EXPECT_EQ(std::bit_cast<uint32_t>(again->alpha), std::bit_cast<uint32_t>(first->alpha)) << hour;
	}
}

// The moon's draw hands the celestial shader the phase's cosine and sine, and asks it for the face's coordinates made
// from the turned points and for the alpha of its own texture
TEST(Moon, TheFaceIsTurnedByThePhasesCosineAndSine)
{
	EXPECT_EQ(moon::FaceInputs(0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
	for (const float phase : {0.5f, 2.5f, 3.14159274f, 5.0f, 6.2831855f})
	{
		const auto inputs = moon::FaceInputs(phase);
		EXPECT_EQ(inputs.x, std::cos(phase)) << phase;
		EXPECT_EQ(inputs.y, std::sin(phase)) << phase;
		EXPECT_EQ(inputs.z, 1.0f) << phase;
		EXPECT_EQ(inputs.w, 1.0f) << phase;
	}
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// The weather folder's moon model is a half sphere about 100 units in radius: one mesh of one part, every point between
// 100 and 105 units from its middle, its flat side facing along z with the dome behind it
TEST(MoonData, TheWeatherFoldersMoonIsAHalfSphereAboutAHundredUnitsInRadius)
{
	const auto game = GameFolder();
	if (!game || !std::filesystem::exists(*game / "Data" / "WeatherSystem" / "moon.l3d"))
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH has no Data/WeatherSystem/moon.l3d";
	}
	openblack::l3d::L3DFile file;
	ASSERT_EQ(file.Open(*game / "Data" / "WeatherSystem" / "moon.l3d"), openblack::l3d::L3DResult::Success);
	ASSERT_EQ(file.GetSubmeshHeaders().size(), 1u);
	ASSERT_EQ(file.GetPrimitiveHeaders().size(), 1u);
	const auto& points = file.GetVertices();
	ASSERT_EQ(points.size(), 65u);
	EXPECT_EQ(file.GetIndices().size(), 3u * 112u);
	float nearest = 1e9f;
	float furthest = 0.0f;
	float front = -1e9f;
	float back = 1e9f;
	for (const auto& vertex : points)
	{
		const float radius = glm::length(glm::vec3(vertex.position.x, vertex.position.y, vertex.position.z));
		nearest = std::min(nearest, radius);
		furthest = std::max(furthest, radius);
		front = std::max(front, vertex.position.z);
		back = std::min(back, vertex.position.z);
	}
	EXPECT_GT(nearest, 100.0f);
	EXPECT_LT(furthest, 105.0f);
	EXPECT_LT(front, 1e-3f);
	EXPECT_LT(back, -100.0f);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// Made from the moon model's points, the face's coordinates stay about the top-left quarter of weather.raw (a 256 by 256
// picture) at every phase, centred on its middle and reaching its edges, and the phase swings them across it
TEST(MoonData, TheFaceShowsTheTopLeftQuarterOfTheWeatherPictureSwungByThePhase)
{
	const auto game = GameFolder();
	if (!game || !std::filesystem::exists(*game / "Data" / "WeatherSystem" / "moon.l3d") ||
	    !std::filesystem::exists(*game / "Data" / "Textures" / "weather.raw"))
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH has no moon model or weather.raw";
	}
	EXPECT_EQ(ReadAll(*game / "Data" / "Textures" / "weather.raw").size(), 256u * 256u * 3u);
	openblack::l3d::L3DFile file;
	ASSERT_EQ(file.Open(*game / "Data" / "WeatherSystem" / "moon.l3d"), openblack::l3d::L3DResult::Success);
	const auto& points = file.GetVertices();
	ASSERT_FALSE(points.empty());
	// 105 units at most from the middle, so a little over a quarter of the picture either way
	constexpr float k_Reach = 105.0f * 0.0025f;
	bool swung = false;
	for (const float phase : {0.0f, 1.0f, 2.5f, 3.14159274f, 4.5f})
	{
		const auto inputs = moon::FaceInputs(phase);
		glm::vec2 low(1e9f);
		glm::vec2 high(-1e9f);
		for (const auto& vertex : points)
		{
			const auto uv = FaceCoordinates(vertex.position, inputs);
			low = glm::min(low, uv);
			high = glm::max(high, uv);
			EXPECT_LE(std::abs(uv.x - 0.25f), k_Reach) << phase;
			EXPECT_LE(std::abs(uv.y - 0.25f), k_Reach) << phase;
			swung = swung || std::abs(uv.x - FaceCoordinates(vertex.position, moon::FaceInputs(0.0f)).x) > 0.01f;
		}
		// the quarter's top and bottom edges are reached at every phase
		EXPECT_LT(low.y, 0.01f) << phase;
		EXPECT_GT(high.y, 0.49f) << phase;
	}
	EXPECT_TRUE(swung);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// The disc's outline is in weathera.raw, the one-byte alpha picture beside weather.raw: in its top-left quarter a disc of
// a quarter of the picture's width about the quarter's middle is opaque, and the quarter is clear outside it
TEST(MoonData, TheDiscsOutlineIsTheAlphaPicturesDiscInTheTopLeftQuarter)
{
	const auto game = GameFolder();
	if (!game || !std::filesystem::exists(*game / "Data" / "Textures" / "weathera.raw"))
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH has no weathera.raw";
	}
	const auto alpha = ReadAll(*game / "Data" / "Textures" / "weathera.raw");
	ASSERT_EQ(alpha.size(), 256u * 256u);
	const glm::vec2 middle(63.5f);
	int inside = 0;
	int outside = 0;
	for (int y = 0; y < 128; ++y)
	{
		for (int x = 0; x < 128; ++x)
		{
			const auto value = alpha.at(static_cast<size_t>((y * 256) + x));
			const float distance = glm::length(glm::vec2(static_cast<float>(x), static_cast<float>(y)) - middle);
			if (distance < 60.0f)
			{
				EXPECT_GE(value, 240) << x << ", " << y;
				++inside;
			}
			else if (distance > 65.0f)
			{
				EXPECT_LE(value, 4) << x << ", " << y;
				++outside;
			}
		}
	}
	EXPECT_GT(inside, 0);
	EXPECT_GT(outside, 0);
}
