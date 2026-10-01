/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The heal miracle's PSys classes (M4, PSys/Rules/Heal.cpp): UR_HealSpellChakra's fade and its "wait for targets"
// flag, CreateRuleFusedSphericalExplode and UR_HealInHand, run on small effects without the game.

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/Zip.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Rules/Heal.h"

using namespace openblack;

namespace
{
std::shared_ptr<const psys::File> Parse(const std::string& text)
{
	auto file = psys::File::Parse(text, "test");
	return file.has_value() ? std::make_shared<const psys::File>(std::move(*file)) : nullptr;
}

const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

const std::string k_Point = "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
                            "ENDPROPERTIES\nENDCLASS\n";

/// Data\Spells\ZSpellFiles\<name>_txt.zzz: the length then a deflate stream (PSysFile.cpp does the same)
std::optional<psys::File> LoadSpellFile(const std::filesystem::path& root, const std::string& name)
{
	std::ifstream stream(root / "Data" / "Spells" / "ZSpellFiles" / (name + "_txt.zzz"), std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return psys::File::Parse(std::string(inflated.begin(), inflated.end()), name);
}
} // namespace

TEST(Heal, classesAreRegistered)
{
	EXPECT_NE(psys::FindModifierFactory("UR_HealSpellChakra"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("CreateRuleFusedSphericalExplode"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_HealInHand"), nullptr);
}

TEST(Heal, chakraFade)
{
	// SF_HealChakra: AtomAgeMaxAlpha 1.5, AtomAgeZeroAlpha 3
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(0.0f, 1.5f, 3.0f), 0.0f);
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(0.75f, 1.5f, 3.0f), 0.5f);
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(1.5f, 1.5f, 3.0f), 1.0f);
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(2.25f, 1.5f, 3.0f), 0.5f);
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(3.0f, 1.5f, 3.0f), 0.0f);
	EXPECT_FLOAT_EQ(psys::heal::ChakraFade(4.0f, 1.5f, 3.0f), 0.0f);
	// the sprites' alpha at the peak: MaxAlpha 100.46 truncated; the glow SpecularColor (200, 255, 255) x t
	EXPECT_EQ(static_cast<int>(psys::heal::ChakraFade(1.5f, 1.5f, 3.0f) * 100.46f), 100);
	EXPECT_EQ(static_cast<int>(psys::heal::ChakraFade(1.2f, 1.5f, 3.0f) * 200.0f), 160);
}

TEST(Heal, chakraWaitsForTargetsUntilClosed)
{
	// UR_HealSpellChakra alone and no targets: no atoms, but flag 2 keeps the effect until it closes down
	const auto file = Parse(k_Header + k_Point +
	                        "BEGINCLASS UR_HealSpellChakra C0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 1 1\n"
	                        "ENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, 1);
	for (int i = 0; i < 5; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 0u);
	EXPECT_FALSE(effect.Finished());
	effect.CloseDown();
	effect.Step(0.1f);
	EXPECT_TRUE(effect.Finished());
}

TEST(Heal, fusedSphericalExplode)
{
	const auto file = Parse(k_Header + k_Point +
	                        "BEGINCLASS ConstFloatProvider F0\nBEGINPROPERTIES\nPROPERTY ConstValue FLOAT 0.6\n"
	                        "ENDPROPERTIES\nENDCLASS\n"
	                        "BEGINCLASS CreateRuleFusedSphericalExplode E0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 0\n"
	                        "PROPERTY NumAtoms INTEGER 5\nPROPERTY FuseTime FLOAT 0.25\nPROPERTY OnlyHemisphere BOOL 1\n"
	                        "PROPERTY MinSpeed PERSIS_PNTR F0\nPROPERTY MaxSpeed PERSIS_PNTR F0\n"
	                        "ENDPROPERTIES\nENDCLASS\n"
	                        "BEGINCLASS UpdateRuleGravity G0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY Gravity FLOAT 0\nPROPERTY MaxSpeed FLOAT 100\nPROPERTY UseDamping BOOL 0\n"
	                        "ENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(10.0f, 5.0f, 20.0f), 1.0f, 7);
	// the fuse: nothing while the collection is younger than FuseTime (ages 0, 0.1, 0.2)
	for (int i = 0; i < 3; ++i)
	{
		effect.Step(0.1f);
		EXPECT_EQ(effect.AtomCount(), 0u);
	}
	// then NumAtoms atoms at once, all at the one speed, in the upper half; once only
	effect.Step(0.1f);
	ASSERT_EQ(effect.AtomCount(), 5u);
	effect.Step(0.1f);
	EXPECT_EQ(effect.AtomCount(), 5u);
	std::vector<psys::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, psys::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 5u);
	for (const auto& atom : atoms)
	{
		// made at the origin and moved by a gravity-free UpdateRuleGravity in the step they were made and the next:
		// 2 x 0.1 s at 0.6 m/s, never downwards
		EXPECT_NEAR(glm::distance(atom.position, glm::vec3(10.0f, 5.0f, 20.0f)), 0.12f, 1e-4f);
		EXPECT_GE(atom.position.y, 5.0f);
	}
}

TEST(Heal, healInHandWiggle)
{
	// two atoms of a root collection: the effect's origin x sin(age x WiggleFreq x 2 pi), the second with the other sign
	const auto file = Parse(k_Header + k_Point +
	                        "BEGINCLASS CreateRuleSphere S0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY NumAtoms INTEGER 2\n"
	                        "PROPERTY Radius FLOAT 0\nENDPROPERTIES\nENDCLASS\n"
	                        "BEGINCLASS UR_HealInHand W0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY WiggleFreq FLOAT 0.25\nENDPROPERTIES\nENDCLASS\n");
	ASSERT_NE(file, nullptr);
	const glm::vec3 origin(4.0f, 2.0f, -1.0f);
	psys::Effect effect(file, origin, 1.0f, 3);
	effect.Step(0.1f);
	effect.Step(0.1f);
	std::vector<psys::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, psys::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 2u);
	const float s = std::sin(0.1f * 0.25f * 2.0f * std::numbers::pi_v<float>);
	EXPECT_NEAR(glm::distance(atoms[0].position, origin * s), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::distance(atoms[1].position, -origin * s), 0.0f, 1e-4f);
}

/// With OPENBLACK_GAME_PATH set to the install: the real heal files use these classes with these values
TEST(Heal, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto chakra = LoadSpellFile(game, "SF_HealChakra");
	ASSERT_TRUE(chakra.has_value());
	const auto* rule = chakra->Find("UR_HealSpellChakra0");
	ASSERT_NE(rule, nullptr);
	EXPECT_FLOAT_EQ(rule->Float("AtomAgeMaxAlpha", 0.0f), 1.5f);
	EXPECT_FLOAT_EQ(rule->Float("AtomAgeZeroAlpha", 0.0f), 3.0f);
	EXPECT_FLOAT_EQ(rule->Float("MaxAlpha", 0.0f), 100.46f);
	EXPECT_EQ(rule->Int("SpecularColorR", 0), 200);
	EXPECT_TRUE(rule->Bool("TakeCentrePos", false));
	EXPECT_EQ(rule->Array("NextGroups"), std::vector<int>({1}));
	const auto* burst = chakra->Find("CreateRuleFusedSphericalExplode0");
	ASSERT_NE(burst, nullptr);
	EXPECT_EQ(burst->Int("NumAtoms", 0), 5);
	EXPECT_FLOAT_EQ(burst->Float("FuseTime", -1.0f), 0.0f);
	EXPECT_TRUE(burst->Bool("OnlyHemisphere", false));
	// the chakra alone, no Mist creator: every class of the file has a factory
	for (const auto& object : chakra->objects)
	{
		if (object.properties.contains("Group"))
		{
			EXPECT_NE(psys::FindModifierFactory(object.className), nullptr) << object.className;
		}
	}
	const auto inHand = LoadSpellFile(game, "SF_HealChakraInHand");
	ASSERT_TRUE(inHand.has_value());
	const auto* wiggle = inHand->Find("UR_HealInHand0");
	ASSERT_NE(wiggle, nullptr);
	EXPECT_FLOAT_EQ(wiggle->Float("WiggleFreq", 0.0f), 0.6f);
}
