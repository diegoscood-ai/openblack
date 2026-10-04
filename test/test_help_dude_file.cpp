/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::HelpDudeFile (src/Help/HelpDudeFile.h) against Data\HelpSprite\markgood.hd / markevil.hd: HelpDude::Load
// 0x5C2194, fn_005C0F30, LoadAnims 0x5C14E0, CAnim::ReadBinary 0x860860; and the CAnim samplers (src/Help/CAnim.h)
// against values of fn_00860E00 / fn_00861EE0 / 0x839F10 run from runblack.exe under an x86 emulator.
// The files come from $OPENBLACK_TEST_BW_ROOT (the game root); the tests skip without it.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Help/CAnim.h"
#include "Help/HelpDudeFile.h"

using namespace openblack;

namespace
{
/// $OPENBLACK_TEST_BW_ROOT (the tree's convention, test_anim_effects.cpp); nullopt when unset: the tests skip
std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

/// The root for the skip messages
std::string RootName()
{
	const auto root = GameRoot();
	return root ? root->string() : std::string("(OPENBLACK_TEST_BW_ROOT not set)");
}

std::optional<help::HelpDudeFile> LoadDude(const char* name, std::vector<uint8_t>* rawOut = nullptr)
{
	const auto root = GameRoot();
	if (!root)
	{
		return std::nullopt;
	}
	const auto path = *root / "Data" / "HelpSprite" / name;
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	help::HelpDudeFile file;
	std::string error;
	EXPECT_TRUE(help::LoadHelpDudeFile(bytes, file, &error)) << error;
	if (rawOut != nullptr)
	{
		*rawOut = bytes;
	}
	return file;
}

struct Expected
{
	const char* file;
	uint32_t bones;
	float restHeight;
	const char* meshName;
	size_t meshSize;
	size_t fileSize;
	std::vector<size_t> emptySlots;
	float farDepth;
	float scale35B4;
	float haloScale;
	glm::vec3 haloOffset;
};

const Expected k_Good {"markgood.hd",
                       97,
                       63.42295f,
                       "DATA\\Yogi_Mesh.l3d",
                       169686,
                       910358,
                       {4, 26, 41, 48, 59, 60, 61, 62, 73, 74, 75, 76, 77, 78, 79},
                       6.0266666f,
                       0.02552f,
                       0.47333333f,
                       {0.0f, -0.28f, 0.0f}};
const Expected k_Evil {"markevil.hd",
                       73,
                       79.15389f,
                       "C:\\dev\\TESTBED\\DATA\\Demon_Mesh.l3d",
                       163700,
                       776556,
                       {4, 10, 25, 26, 36, 50, 51, 73, 74, 75, 76, 77, 78, 79},
                       5.64f,
                       0.026826667f,
                       0.0f,
                       {0.0f, 0.0f, 0.0f}};

void CheckFile(const Expected& expected)
{
	std::vector<uint8_t> raw;
	const auto file = LoadDude(expected.file, &raw);
	if (!file)
	{
		GTEST_SKIP() << expected.file << " not found under " << RootName();
	}
	// exact parse: the segment read to its last byte, and the segment is the whole file after its 44-byte header
	EXPECT_EQ(raw.size(), expected.fileSize);
	EXPECT_EQ(file->segmentSize + 44, raw.size());
	EXPECT_EQ(file->bytesRead, file->segmentSize);

	EXPECT_EQ(file->boneCount, expected.bones);
	EXPECT_EQ(file->animCount, 80u);
	EXPECT_EQ(file->clipCount, 80u);
	EXPECT_NEAR(file->restHeight, expected.restHeight, 1e-4f);
	EXPECT_EQ(file->meshName, expected.meshName);
	EXPECT_EQ(file->hasData, 1u);
	ASSERT_EQ(file->mesh.size(), expected.meshSize);
	EXPECT_EQ(std::string(file->mesh.begin(), file->mesh.begin() + 4), "L3D0");

	ASSERT_EQ(file->animNames.size(), 80u);
	ASSERT_EQ(file->clips.size(), 80u);
	std::vector<size_t> empty;
	for (size_t i = 0; i < 80; ++i)
	{
		EXPECT_EQ(file->animNames[i].empty(), !file->clips[i].has_value()) << i;
		if (!file->clips[i])
		{
			empty.push_back(i);
			continue;
		}
		const auto& clip = *file->clips[i];
		EXPECT_EQ(file->clipRecordSizes[i], help::CAnimBinarySize(clip) + 4) << i;
		EXPECT_EQ(clip.boneCount, expected.bones) << i;
		EXPECT_EQ(clip.frames.size(), clip.frameCount) << i;
		EXPECT_GT(clip.durationMs, 0) << i;
		for (size_t c = 0; c < clip.rotationBones.size(); ++c)
		{
			EXPECT_LT(clip.rotationBones[c], expected.bones);
			EXPECT_TRUE(c == 0 || clip.rotationBones[c - 1] < clip.rotationBones[c]);
		}
		for (size_t c = 0; c < clip.positionBones.size(); ++c)
		{
			EXPECT_LT(clip.positionBones[c], expected.bones);
			EXPECT_TRUE(c == 0 || clip.positionBones[c - 1] < clip.positionBones[c]);
		}
	}
	EXPECT_EQ(empty, expected.emptySlots);
	EXPECT_EQ(80 - empty.size(), expected.file == k_Good.file ? 65u : 66u);

	// the stand clip has a channel of each kind for every bone (the fill of fn_00860E00 indexes its key 0 by bone)
	const auto* stand = file->Clip(0);
	ASSERT_NE(stand, nullptr);
	EXPECT_EQ(stand->rotationBones.size(), expected.bones);
	EXPECT_EQ(stand->positionBones.size(), expected.bones);

	EXPECT_NEAR(file->nearDepth, 8.7333333f, 1e-5f);
	EXPECT_NEAR(file->farDepth, expected.farDepth, 1e-5f);
	EXPECT_NEAR(file->scale35B4, expected.scale35B4, 1e-6f);
	EXPECT_NEAR(file->haloScale, expected.haloScale, 1e-6f);
	EXPECT_NEAR(file->haloOffset.x, expected.haloOffset.x, 1e-6f);
	EXPECT_NEAR(file->haloOffset.y, expected.haloOffset.y, 1e-6f);
	EXPECT_NEAR(file->haloOffset.z, expected.haloOffset.z, 1e-6f);
	EXPECT_EQ(file->block2C38.size(), 0x200u);
	EXPECT_EQ(file->animEvents.size(), 80u);
	EXPECT_EQ(file->words2EE8[0], 1u);

	// the rest skeleton of the embedded L3D0, through l3d::L3DFile from memory
	help::RestSkeleton rest;
	std::string error;
	ASSERT_TRUE(help::LoadRestSkeleton(*file, rest, &error)) << error;
	EXPECT_EQ(rest.parents.size(), expected.bones);
	// LH3DAnim::SetTransform's height, which the original writes over the file's +0x10 (0x5C1250)
	EXPECT_NEAR(rest.height, file->restHeight, 1e-3f);
}

/// local = rest; SetPose(stand, standMs, fill = stand key 0); the layers; ComposeWorld under the identity
std::vector<glm::mat4> Pose(const help::HelpDudeFile& file, const help::RestSkeleton& rest, int32_t standMs,
                            const std::vector<std::tuple<size_t, int32_t, bool>>& layers)
{
	const auto& stand = *file.Clip(0);
	auto local = rest.local;
	help::SetPose(stand, standMs, rest, &stand.frames[0], local);
	for (const auto& [index, ms, middleReference] : layers)
	{
		const auto& clip = *file.Clip(index);
		const auto& reference = clip.frames[middleReference ? clip.frames.size() / 2 : 0];
		help::ApplyAdditive(clip, ms, reference, rest, local);
	}
	std::vector<glm::mat4> world;
	help::ComposeWorld(rest, local, glm::mat4(1.0f), world);
	return world;
}

struct GoldenBone
{
	size_t bone;
	glm::vec3 position; ///< LH row 3 = glm column 3
	glm::vec3 row0;     ///< LH row 0 = glm column 0
};

void CheckGolden(const std::vector<glm::mat4>& world, const std::vector<GoldenBone>& golden)
{
	for (const auto& g : golden)
	{
		ASSERT_LT(g.bone, world.size());
		const auto& m = world[g.bone];
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_NEAR(m[3][k], g.position[k], 2e-3f) << "bone " << g.bone << " position " << k;
			EXPECT_NEAR(m[0][k], g.row0[k], 1e-3f) << "bone " << g.bone << " row0 " << k;
		}
	}
}
} // namespace

TEST(HelpDudeFile, MarkGood)
{
	CheckFile(k_Good);
}

TEST(HelpDudeFile, MarkEvil)
{
	CheckFile(k_Evil);
}

TEST(HelpDudeFile, Events)
{
	const auto good = LoadDude("markgood.hd");
	const auto evil = LoadDude("markevil.hd");
	if (!good || !evil)
	{
		GTEST_SKIP() << "HelpSprite files not found under " << RootName();
	}
	// KnockScreen (58): InGame 155 twice for good, seven times for evil; HandGun (60): evil 154 at 0.48
	ASSERT_EQ(good->animEvents[58].events.size(), 2u);
	EXPECT_EQ(good->animEvents[58].events[0].sample, 155u);
	EXPECT_NEAR(good->animEvents[58].events[0].phase, 0.34053656f, 1e-6f);
	EXPECT_NEAR(good->animEvents[58].events[1].phase, 0.36297652f, 1e-6f);
	EXPECT_EQ(evil->animEvents[58].events.size(), 7u);
	ASSERT_EQ(evil->animEvents[60].events.size(), 1u);
	EXPECT_EQ(evil->animEvents[60].events[0].sample, 154u);
	EXPECT_NEAR(evil->animEvents[60].events[0].phase, 0.48095238f, 1e-6f);
	// the loop window of the nod (20)
	EXPECT_NEAR(good->animEvents[20].loopStart, 0.25238097f, 1e-6f);
	EXPECT_NEAR(good->animEvents[20].loopEnd, 0.5714286f, 1e-6f);
}

TEST(CAnim, Keys)
{
	help::CAnim loop;
	loop.durationMs = 1000;
	loop.looping = true;
	loop.frameCount = 10;
	loop.frames.resize(10);
	// a looping clip: 10 keys over 1000 ms, the last one wraps to key 0
	auto keys = help::SampleKeys(loop, 950, false);
	EXPECT_EQ(keys.key0, 9u);
	EXPECT_EQ(keys.key1, 0u);
	EXPECT_NEAR(keys.fraction, 0.5f, 1e-6f);
	// a one-shot clip: period 1000 * 10 / 9 = 1111 (integer), key 9 at the end, key1 still wraps to 0
	auto once = loop;
	once.looping = false;
	keys = help::SampleKeys(once, 1000, true);
	EXPECT_EQ(keys.key0, 9u);
	EXPECT_EQ(keys.key1, 0u);
	EXPECT_NEAR(keys.fraction, 10.0f / 1111.0f * 1000.0f - 9.0f, 1e-5f);
	keys = help::SampleKeys(once, 5000, true);
	EXPECT_EQ(keys.key0, 9u);
}

TEST(CAnim, ApplyAnimArguments)
{
	help::CAnim clip;
	clip.durationMs = 8066;
	clip.frameCount = 72;
	clip.frames.resize(72);
	clip.displacement = {1.0f, 2.0f, 3.0f};
	auto args = help::ApplyAnimArguments(clip, 1.3f, 0.5f, true);
	EXPECT_NEAR(args.phase, 0.3f, 1e-5f);
	EXPECT_EQ(args.milliseconds, static_cast<int32_t>(8066.0f * args.phase));
	EXPECT_EQ(args.referenceKey, 35u);
	EXPECT_NEAR(args.rootMove.z, 3.0f * args.phase, 1e-5f);
	args = help::ApplyAnimArguments(clip, 1.3f, 0.0f, false);
	EXPECT_EQ(args.phase, 1.0f);
	EXPECT_EQ(args.milliseconds, 8065);
	EXPECT_EQ(args.referenceKey, 0u);
	args = help::ApplyAnimArguments(clip, -0.25f, 0.0f, false);
	EXPECT_EQ(args.phase, 0.0f);
	EXPECT_EQ(args.milliseconds, 0);
}

TEST(CAnim, SampledPoses)
{
	const auto good = LoadDude("markgood.hd");
	const auto evil = LoadDude("markevil.hd");
	if (!good || !evil)
	{
		GTEST_SKIP() << "HelpSprite files not found under " << RootName();
	}
	help::RestSkeleton goodRest;
	help::RestSkeleton evilRest;
	std::string error;
	ASSERT_TRUE(help::LoadRestSkeleton(*good, goodRest, &error)) << error;
	ASSERT_TRUE(help::LoadRestSkeleton(*evil, evilRest, &error)) << error;

	// sanity: the good stand clip is the rest pose (its angles are below 2e-7, its positions the L3D's)
	std::vector<glm::mat4> local;
	help::SampleLocal(*good->Clip(0), 0.0f, goodRest, local);
	std::vector<glm::mat4> world;
	help::ComposeWorld(goodRest, local, glm::mat4(1.0f), world);
	ASSERT_EQ(world.size(), goodRest.world.size());
	for (size_t b = 0; b < world.size(); ++b)
	{
		for (int c = 0; c < 4; ++c)
		{
			for (int r = 0; r < 3; ++r)
			{
				EXPECT_NEAR(world[b][c][r], goodRest.world[b][c][r], 2e-3f) << "bone " << b;
			}
		}
	}
	// every row of a sampled rotation is a unit vector (fn_007FB5C0), whatever the clip and time
	for (size_t index : {1u, 24u, 58u, 65u})
	{
		help::SampleLocal(*good->Clip(index), 777.0f, goodRest, local);
		for (const auto& m : local)
		{
			for (int c = 0; c < 3; ++c)
			{
				const glm::vec3 row(m[c]);
				EXPECT_NEAR(std::sqrt(row.x * row.x + row.y * row.y + row.z * row.z), 1.0f, 1e-3f);
			}
		}
	}

	// golden values from runblack.exe (emulated): good = stand at 100 ms, KnockOnScreen (58) at 1234 ms and the vowel
	// E (7) at 133 ms, both from their key 0
	CheckGolden(Pose(*good, goodRest, 100, {{58, 1234, false}, {7, 133, false}}),
	            {
	                {0, {0.25844f, -11.67703f, 9.58045f}, {-1e-06f, 0.999989f, 0.0f}},
	                {5, {-0.19207f, 11.70354f, 1.36303f}, {-0.041051f, 0.839877f, -0.541074f}},
	                {48, {0.05562f, 0.12191f, -21.90013f}, {0.997793f, 0.062059f, -0.013403f}},
	                {96, {24.98054f, -18.67578f, -22.01774f}, {0.426977f, -0.263047f, -0.865082f}},
	            });
	// evil = stand at 50 ms, HandGun (60) at 700 ms from key 0, Look L/R (18) at 400 ms from its middle key
	CheckGolden(Pose(*evil, evilRest, 50, {{60, 700, false}, {18, 400, true}}),
	            {
	                {0, {-5.63581f, -18.47319f, 22.14791f}, {0.303281f, -0.462248f, -0.833254f}},
	                {5, {-5.76097f, -8.014f, 22.37665f}, {0.182539f, -0.844113f, -0.504022f}},
	                {36, {11.52593f, 9.25333f, -5.37187f}, {0.733394f, 0.196337f, -0.65061f}},
	                {72, {-22.54125f, -39.05022f, 26.48773f}, {-0.190044f, -0.756841f, -0.625002f}},
	            });
}
