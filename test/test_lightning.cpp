/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's PSys classes (M5): UR_Lightning / UR_LightningStrike (PSys/Rules/Lightning.cpp), the chain
// ribbon's UV layout (PSys/Creators/Chain.cpp) and the light map creator (PSys/Creators/LightMap.cpp).

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <memory>
#include <optional>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Zip.h"
#include "PSys/Creators/Chain.h"
#include "PSys/Creators/LightMap.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;

namespace
{
/// The class of that name in the file, built through the registry (nullptr when nobody registered it)
std::unique_ptr<psys::Creator> MakeCreator(const psys::File& file, const std::string& name)
{
	const auto* object = file.Find(name);
	if (object == nullptr)
	{
		return nullptr;
	}
	const auto factory = psys::FindCreatorFactory(object->className);
	return factory != nullptr ? factory(*object) : nullptr;
}

const psys::Object* Find(const psys::File& file, const std::string& className)
{
	for (const auto& object : file.objects)
	{
		if (object.className == className)
		{
			return &object;
		}
	}
	return nullptr;
}

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

TEST(Lightning, classesAreRegistered)
{
	// the registry is filled on the first lookup (PSysRegistry.cpp)
	EXPECT_NE(psys::FindModifierFactory("UR_Lightning"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_LightningStrike"), nullptr);
	EXPECT_NE(psys::FindCreatorFactory("ParticleChainCreator"), nullptr);
	EXPECT_NE(psys::FindCreatorFactory("ParticleLightMapCreator"), nullptr);
}

TEST(Lightning, chainCreatorProperties)
{
	const auto file = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                    "BEGINCLASS ParticleChainCreator Chain0\nBEGINPROPERTIES\n"
	                                    "PROPERTY NumTexturesForWholeChain INTEGER 4\n"
	                                    "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_Lightning.raw\n"
	                                    "PROPERTY UseAdditiveAlpha BOOL 1\n"
	                                    "PROPERTY MaterialUpdateZBuffer BOOL 0\n"
	                                    "PROPERTY InitialScale FLOAT 1\n"
	                                    "ENDPROPERTIES\nENDCLASS\n",
	                                    "test");
	ASSERT_TRUE(file.has_value());
	const auto creator = MakeCreator(*file, "Chain0");
	ASSERT_NE(creator, nullptr);
	const auto* chain = dynamic_cast<const psys::ChainCreator*>(creator.get());
	ASSERT_NE(chain, nullptr);
	// a chain is its own draw kind: the collection becomes one ribbon, not one sprite per joint
	EXPECT_EQ(chain->kind, psys::Creator::Kind::Chain);
	EXPECT_EQ(chain->numTexturesForWholeChain, 4);
	EXPECT_TRUE(chain->additive);
	EXPECT_FALSE(chain->writeDepth);
	// without the game's Data\Textures the name keeps the spell file's spelling
	EXPECT_TRUE(chain->texture == "S_Lightning" || chain->texture == "S_lightning");
}

TEST(Lightning, chainSegmentUv)
{
	psys::ChainCreator chain;
	chain.numTexturesForWholeChain = 4;
	// the texture is repeated 4 times over the whole chain: 8 segments give half a tile each
	EXPECT_FLOAT_EQ(chain.SegmentU(0, 8).x, 0.0f);
	EXPECT_FLOAT_EQ(chain.SegmentU(0, 8).y, 0.5f);
	EXPECT_FLOAT_EQ(chain.SegmentU(7, 8).x, 3.5f);
	EXPECT_FLOAT_EQ(chain.SegmentU(7, 8).y, 4.0f);
	// -1: one tile per segment
	chain.numTexturesForWholeChain = -1;
	EXPECT_FLOAT_EQ(chain.SegmentU(3, 8).x, 3.0f);
	EXPECT_FLOAT_EQ(chain.SegmentU(3, 8).y, 4.0f);
}

TEST(Lightning, lightMapCreatorProperties)
{
	const auto file = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                    "BEGINCLASS ParticleLightMapCreator LM0\nBEGINPROPERTIES\n"
	                                    "PROPERTY Pitch INTEGER 5\n"
	                                    "PROPERTY NumFramesInFile INTEGER 16\n"
	                                    "PROPERTY NumFramesInUse INTEGER 16\n"
	                                    "PROPERTY FrameRate FLOAT 26\n"
	                                    "PROPERTY PlayAnim BOOL 1\n"
	                                    "PROPERTY LoopAnim BOOL 0\n"
	                                    "PROPERTY RandJitter FLOAT 0\n"
	                                    "PROPERTY UseRandJitter BOOL 1\n"
	                                    "PROPERTY ShiftX FLOAT 9.97788\n"
	                                    "PROPERTY TextureFileName STRING .\\Data\\SPELLS\\LightMaps\\S_lm.raw\n"
	                                    "ENDPROPERTIES\nENDCLASS\n",
	                                    "test");
	ASSERT_TRUE(file.has_value());
	const auto creator = MakeCreator(*file, "LM0");
	ASSERT_NE(creator, nullptr);
	const auto* lightMap = dynamic_cast<const psys::LightMapCreator*>(creator.get());
	ASSERT_NE(lightMap, nullptr);
	EXPECT_EQ(lightMap->pitch, 5);
	EXPECT_EQ(lightMap->numFramesInFile, 16);
	EXPECT_EQ(lightMap->numFramesInUse, 16);
	EXPECT_TRUE(lightMap->useRandJitter);
	EXPECT_FLOAT_EQ(lightMap->shiftX, 9.97788f);
	// drawn as a flat additive quad of the 8 x 8 frame atlas
	EXPECT_EQ(lightMap->kind, psys::Creator::Kind::Sprite);
	EXPECT_EQ(lightMap->spritesPerRow, 8);
	EXPECT_EQ(lightMap->numFrames, 16);
	EXPECT_TRUE(lightMap->horizontal);
	EXPECT_TRUE(lightMap->additive);
	EXPECT_TRUE(lightMap->playAnim);
	EXPECT_FALSE(lightMap->loopAnim);
	EXPECT_FLOAT_EQ(lightMap->frameRate, 26.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: the real bolt files and every class they use
TEST(Lightning, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	const auto bolt = LoadSpellFile(root, "SF_LightningBolt");
	ASSERT_TRUE(bolt.has_value());
	// SF_LightningBolt: groups 4 (light maps) and 5 (root sprite) exist from the start, the forks are group 1
	const auto created = bolt->header.Array("InitiallyCreated");
	ASSERT_EQ(created.size(), 25u);
	EXPECT_EQ(created[4], 1);
	EXPECT_EQ(created[5], 1);
	EXPECT_EQ(created[0], 0);
	const auto* rule = Find(*bolt, "UR_Lightning");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->Int("ForkGroup", -1), 1);
	EXPECT_EQ(rule->Int("LightMapGroup", -1), 4);
	EXPECT_EQ(rule->Int("CommonGlowGroup", -1), 6);
	EXPECT_EQ(rule->Int("MaxLightningObjects", 0), 6);
	EXPECT_EQ(rule->Int("MinLightningObjects", 0), 3);
	EXPECT_EQ(rule->Int("MaxLightningObjectsAtOnce", 0), 4);
	EXPECT_EQ(rule->Int("MaxJointsPerFork", 0), 10);
	EXPECT_FLOAT_EQ(rule->Float("SplitAngle", 0.0f), 0.2f);
	EXPECT_FLOAT_EQ(rule->Float("ForkScale", 0.0f), 2.0f);
	EXPECT_FLOAT_EQ(rule->Float("AverageLightmapLife", 0.0f), 1.5f);
	EXPECT_TRUE(rule->Bool("CastingFromHand", false));
	// every class of the file is either registered or deliberately unsupported: none may be missing a factory for the
	// bolt's own classes
	for (const auto& name : {"UR_Lightning", "ParticleChainCreator", "ParticleLightMapCreator"})
	{
		const auto* object = Find(*bolt, name);
		ASSERT_NE(object, nullptr) << name;
	}
	// the power-ups only change the fork counts and scale
	const auto two = LoadSpellFile(root, "SF_LightningBoltPUTwo");
	ASSERT_TRUE(two.has_value());
	const auto* ruleTwo = Find(*two, "UR_Lightning");
	ASSERT_NE(ruleTwo, nullptr);
	EXPECT_EQ(ruleTwo->Int("MinLightningObjects", 0), 15);
	EXPECT_EQ(ruleTwo->Int("MaxLightningObjects", 0), 28);
	EXPECT_EQ(ruleTwo->Int("MaxLightningObjectsAtOnce", 0), 20);
	EXPECT_FLOAT_EQ(ruleTwo->Float("ForkScale", 0.0f), 8.0f);
	// the script / climate strike: UR_LightningStrike makes the atom whose group holds the UR_Lightning
	const auto strike = LoadSpellFile(root, "SF_LightningStrike");
	ASSERT_TRUE(strike.has_value());
	const auto* strikeRule = Find(*strike, "UR_LightningStrike");
	ASSERT_NE(strikeRule, nullptr);
	EXPECT_EQ(strikeRule->Array("NextGroups"), std::vector<int>({1}));
	const auto* strikeLightning = Find(*strike, "UR_Lightning");
	ASSERT_NE(strikeLightning, nullptr);
	EXPECT_FALSE(strikeLightning->Bool("CastingFromHand", true));
	EXPECT_EQ(strikeLightning->Int("ForkGroup", -1), 2);
	// the light map of the bolt: 16 frames of 5 x 5 RGB stacked (1200 bytes)
	const auto* lightMap = Find(*bolt, "ParticleLightMapCreator");
	ASSERT_NE(lightMap, nullptr);
	EXPECT_EQ(lightMap->Int("Pitch", 0), 5);
	EXPECT_EQ(lightMap->Int("NumFramesInFile", 0), 16);
	std::ifstream raw(root / "Data" / "Spells" / "LightMaps" / "S_lightning_lightmap_with_border.raw", std::ios::binary);
	ASSERT_TRUE(raw.is_open());
	const std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(raw)), std::istreambuf_iterator<char>());
	EXPECT_EQ(pixels.size(), 5u * 5u * 16u * 3u);
}
