/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The celestial shaders the sun and the moon are drawn with, read from their sources: how the moon's face takes its
// texture coordinates and its outline (docs/wiki/world/sky.md#sky-sun-moon-and-clouds-original)

#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

/// The source folder, from ctest's environment, else as it was built: a compiler cache shared between worktrees can
/// hand this test an object built in another worktree
std::filesystem::path SourceDir()
{
	if (const auto* dir = std::getenv("OPENBLACK_SOURCE_DIR"); dir != nullptr && *dir != '\0')
	{
		return dir;
	}
	return OPENBLACK_SOURCE_DIR;
}

/// A shader's lines, without their line ends and with the spaces and tabs before and after them taken off
std::vector<std::string> Lines(const std::string& shader)
{
	std::ifstream file(SourceDir() / "assets" / "shaders" / shader);
	std::vector<std::string> lines;
	for (std::string line; std::getline(file, line);)
	{
		const auto first = line.find_first_not_of(" \t\r");
		const auto last = line.find_last_not_of(" \t\r");
		lines.push_back(first == std::string::npos ? std::string() : line.substr(first, last - first + 1));
	}
	return lines;
}

/// Where a line is, or the number of lines when it is not there
size_t Find(const std::vector<std::string>& lines, const std::string& line)
{
	return static_cast<size_t>(std::ranges::find(lines, line) - lines.begin());
}

} // namespace

// The moon's face takes its texture coordinates from the mesh's points turned by the phase, through the cosine and sine
// its draw gives: u = (x cos p - z sin p) x 0.0025 + 0.25, v = y x 0.0025 + 0.25, only when the draw asks for them
TEST(CelestialShader, TheMoonsFaceIsProjectedFromItsPointsTurnedByThePhase)
{
	const auto vertex = Lines("vs_celestial.sc");
	ASSERT_FALSE(vertex.empty());
	const auto gate = Find(vertex, "if (u_celestial.z > 0.0f)");
	const auto projection =
	    Find(vertex, "v_texcoord0.xy = vec2(a_position.x * u_celestial.x - a_position.z * u_celestial.y, a_position.y) * "
	                 "0.0025f + 0.25f;");
	ASSERT_LT(gate, vertex.size());
	ASSERT_LT(projection, vertex.size());
	EXPECT_LT(gate, projection);
	// otherwise the mesh's own coordinates
	EXPECT_LT(Find(vertex, "v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);"), gate);
}

// The moon's outline is its alpha texture's: the alpha of what is drawn is the second texture's red, when the draw asks
// for it, times the colour's alpha
TEST(CelestialShader, TheMoonsOutlineIsTheAlphaOfItsSecondTexture)
{
	const auto fragment = Lines("fs_celestial.sc");
	ASSERT_FALSE(fragment.empty());
	const auto gate = Find(fragment, "if (u_celestial.w > 0.0f)");
	const auto alpha = Find(fragment, "texel.a = texture2D(s_alpha, v_texcoord0.xy).r;");
	const auto colour = Find(fragment, "gl_FragColor = texel * u_colour;");
	ASSERT_LT(gate, fragment.size());
	ASSERT_LT(alpha, fragment.size());
	ASSERT_LT(colour, fragment.size());
	EXPECT_LT(gate, alpha);
	EXPECT_LT(alpha, colour);
}

namespace
{
/// The names a shader declares as uniforms
std::vector<std::string> Uniforms(const std::vector<std::string>& lines)
{
	std::vector<std::string> names;
	for (const auto& line : lines)
	{
		if (line.starts_with("uniform "))
		{
			const auto from = line.find(' ', std::string("uniform ").size()) + 1;
			names.push_back(line.substr(from, line.find(';') - from));
		}
	}
	return names;
}
} // namespace

// The sun and the moon light nothing and are lit by nothing: their shaders take no normal and no light, only the
// points, the texture coordinates, the textures, the colour and the moon's face inputs
TEST(CelestialShader, TheSunAndTheMoonAreDrawnWithNoLight)
{
	const auto vertex = Lines("vs_celestial.sc");
	const auto fragment = Lines("fs_celestial.sc");
	ASSERT_FALSE(vertex.empty());
	ASSERT_FALSE(fragment.empty());
	EXPECT_EQ(vertex.front(), "$input a_position, a_texcoord0");
	EXPECT_EQ(Uniforms(vertex), (std::vector<std::string> {"u_celestial"}));
	EXPECT_EQ(Uniforms(fragment), (std::vector<std::string> {"u_colour", "u_celestial"}));
	EXPECT_LT(Find(fragment, "SAMPLER2D(s_diffuse, 0);"), fragment.size());
}
