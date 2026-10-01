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
	// fn_006C8920 with the ctor's 32 x 64 frames (0x6AA739..0x6AA740): V runs along the chain, U over one frame column
	psys::ChainCreator chain;
	EXPECT_EQ(chain.frameHeight, 64);
	EXPECT_EQ(chain.frameWidth, 32);
	EXPECT_EQ(chain.numTexturesForWholeChain, -1);
	chain.numTexturesForWholeChain = 4;
	// 4 textures over 8 segments: two segments each, the first stretch with FrameOfTail, the last with FrameOfHead
	const auto first = chain.SegmentUv(0, 8, 0.0f);
	EXPECT_FLOAT_EQ(first[0].x, 0.0f);
	EXPECT_FLOAT_EQ(first[0].y, 0.0f);
	EXPECT_FLOAT_EQ(first[1].x, 0.125f);
	EXPECT_FLOAT_EQ(first[2].y, 0.125f);
	chain.frameOfHead = 2;
	chain.fileOffset = 5;
	const auto last = chain.SegmentUv(7, 8, 0.0f);
	EXPECT_FLOAT_EQ(last[0].x, 0.875f); // (5 + 2) x 32 / 256
	EXPECT_FLOAT_EQ(last[1].x, 1.0f);
	EXPECT_FLOAT_EQ(last[0].y, 0.125f); // the second of its two segments
	EXPECT_FLOAT_EQ(last[3].y, 0.25f);
	// -1: one texture per segment, the inner ones with frame 0 (+ FileOffset); the scroll on every v
	chain.numTexturesForWholeChain = -1;
	const auto inner = chain.SegmentUv(3, 8, 0.1f);
	EXPECT_FLOAT_EQ(inner[0].x, 0.625f);
	EXPECT_FLOAT_EQ(inner[0].y, 0.1f);
	EXPECT_FLOAT_EQ(inner[2].y, 0.25f + 0.1f);
}

// fn_006C8920 through ChainCreator::SegmentUv, with the ctor's defaults (0x6AA739..0x6AA747: 64 high, 32 wide) and
// SF_LightningBolt's 4 repeats on a 10 joint fork: U across the ribbon is frame 0 (texels 0..32), V along it is 64
// texels per repeat. uv[0] = (u0, v0), uv[1] = (u1, v0), uv[2] = (u0, v1), uv[3] = (u1, v1)
TEST(Lightning, chainSegmentUvRepeats)
{
	psys::ChainCreator chain;
	chain.numTexturesForWholeChain = 4;
	auto uv = chain.SegmentUv(0, 9, 0.0f); // repeat 0 holds segments 0..1
	EXPECT_FLOAT_EQ(uv[0].x, 0.0f);
	EXPECT_FLOAT_EQ(uv[1].x, 0.125f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.125f);
	uv = chain.SegmentUv(1, 9, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
	uv = chain.SegmentUv(2, 9, 0.0f); // repeat 1 starts again at the frame's top
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	uv = chain.SegmentUv(8, 9, 0.0f); // repeat 3 holds segments 6..8
	EXPECT_FLOAT_EQ(uv[0].y, 64.0f * 2.0f / 3.0f / 256.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
	// SF_GestureChain: FrameOfTail 1 in the first repeat, FrameOfHead 2 in the last, 0 between, all + FileOffset 5
	chain.numTexturesForWholeChain = 5;
	chain.frameOfHead = 2;
	chain.frameOfTail = 1;
	chain.fileOffset = 5;
	EXPECT_FLOAT_EQ(chain.SegmentUv(0, 9, 0.0f)[0].x, 6.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(4, 9, 0.0f)[0].x, 5.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(8, 9, 0.0f)[0].x, 7.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(8, 9, 0.0f)[1].x, 1.0f);
	// -1: one repeat per segment (CreateChain 0x6AA8DF)
	chain.numTexturesForWholeChain = -1;
	uv = chain.SegmentUv(3, 8, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
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
	// not a sprite: stamped into the land's cells (light_map_atoms::SubmitFrame -> land_light::AddStamp, fn_006CA280)
	EXPECT_EQ(lightMap->kind, psys::Creator::Kind::Other);
	EXPECT_EQ(lightMap->numFrames, 16);
	EXPECT_EQ(lightMap->bitmap, nullptr); // no such file here
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
