/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mesh pack (AllMeshes.g3d), the animation pack (AllAnims.anm) and loose animations (.anm), read as the game reads
// them. A mesh pack written in the test gives its meshes back as written, in order; a loose animation written in the
// test gives its header and keyframes back; the game's mesh pack holds every object mesh with its textures, and its
// animation pack every animation, each a whole animation. docs/wiki/project/progress/engine/asset_formats.md

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <ANMFile.h>
#include <PackFile.h>
#include <gtest/gtest.h>

#include "TestTempFolder.h"

using namespace openblack;

namespace
{
/// A loose animation's bytes as the game reads them: the header, then from its frames' base a word per keyframe that
/// leads, through two more words, to the keyframe's bone count, its time and its bones
[[nodiscard]] std::vector<uint8_t> AnimationBytes(const std::string& name, const std::vector<anm::ANMFrame>& frames)
{
	std::vector<uint8_t> bytes(sizeof(anm::ANMHeader));
	const auto put = [&bytes](const auto& value) {
		const auto* raw = reinterpret_cast<const uint8_t*>(&value);
		bytes.insert(bytes.end(), raw, raw + sizeof(value));
	};
	const auto at = [&bytes](size_t offset, uint32_t value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	anm::ANMHeader header {};
	std::memcpy(header.name.data(), name.data(), std::min(name.size(), header.name.size() - 1));
	header.frameCount = static_cast<uint32_t>(frames.size());
	header.framesBase = static_cast<uint32_t>(sizeof(anm::ANMHeader));
	// the table of keyframes, filled in as each keyframe is laid out after it
	const size_t table = bytes.size();
	bytes.resize(bytes.size() + frames.size() * sizeof(uint32_t));
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto pointer = static_cast<uint32_t>(bytes.size());
		at(table + i * sizeof(uint32_t), pointer);
		put(static_cast<uint32_t>(pointer + sizeof(uint32_t)));     // the keyframe pointer
		put(static_cast<uint32_t>(pointer + 2 * sizeof(uint32_t))); // the bone offset block
		put(static_cast<uint32_t>(frames.at(i).bones.size()));
		put(frames.at(i).time);
		for (const auto& bone : frames.at(i).bones)
		{
			put(bone);
		}
	}
	header.animationDataSize = static_cast<uint32_t>(bytes.size());
	std::memcpy(bytes.data(), &header, sizeof(header));
	return bytes;
}

[[nodiscard]] anm::ANMBone Bone(float seed)
{
	anm::ANMBone bone {};
	for (size_t i = 0; i < bone.matrix.size(); ++i)
	{
		bone.matrix.at(i) = seed + static_cast<float>(i);
	}
	return bone;
}

[[nodiscard]] std::optional<std::filesystem::path> GamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	return game != nullptr ? std::optional(std::filesystem::path(game)) : std::nullopt;
}
} // namespace

// A mesh pack of two meshes written in the test gives both back as written, in their order
TEST(PackFormats, AMeshPackWrittenInTheTestGivesItsMeshesBackInOrder)
{
	const test::TestTempFolder folder;
	const std::vector<uint8_t> first {'L', '3', 'D', '0', 1, 2, 3, 4, 5, 6, 7, 8};
	const std::vector<uint8_t> second {'L', '3', 'D', '0', 9, 8, 7, 6};
	{
		pack::PackFile written;
		ASSERT_EQ(written.InsertMesh(first), pack::PackResult::Success);
		ASSERT_EQ(written.InsertMesh(second), pack::PackResult::Success);
		ASSERT_EQ(written.CreateMeshBlock(), pack::PackResult::Success);
		ASSERT_EQ(written.CreateInfoBlock(), pack::PackResult::Success);
		ASSERT_EQ(written.Write(folder.Path() / "meshes.g3d"), pack::PackResult::Success);
	}
	pack::PackFile read;
	ASSERT_EQ(read.Open(folder.Path() / "meshes.g3d"), pack::PackResult::Success);
	ASSERT_EQ(read.GetMeshes().size(), 2u);
	EXPECT_EQ(read.GetMesh(0), first);
	EXPECT_EQ(read.GetMesh(1), second);
}

// A loose animation written in the test gives its name, its frames' times and every bone's matrix back
TEST(PackFormats, ALooseAnimationWrittenInTheTestGivesItsKeyframesBack)
{
	std::vector<anm::ANMFrame> frames {{.time = 0, .bones = {Bone(0.0f), Bone(20.0f)}},
	                                   {.time = 100, .bones = {Bone(40.0f), Bone(60.0f)}},
	                                   {.time = 250, .bones = {Bone(80.0f), Bone(100.0f)}}};
	anm::ANMFile read;
	ASSERT_EQ(read.Open(AnimationBytes("TestWave", frames)), anm::ANMResult::Success);
	EXPECT_EQ(std::string(read.GetHeader().name.data()), "TestWave");
	ASSERT_EQ(read.GetHeader().frameCount, 3u);
	ASSERT_EQ(read.GetKeyframes().size(), 3u);
	for (size_t f = 0; f < frames.size(); ++f)
	{
		const auto& frame = read.GetKeyframe(static_cast<uint32_t>(f));
		EXPECT_EQ(frame.time, frames.at(f).time) << f;
		ASSERT_EQ(frame.bones.size(), 2u) << f;
		for (size_t b = 0; b < frame.bones.size(); ++b)
		{
			EXPECT_EQ(frame.bones.at(b).matrix, frames.at(f).bones.at(b).matrix) << f << ", " << b;
		}
	}
}

// The game's mesh pack holds every object mesh, each a model, with a block of their textures
TEST(PackFormats, realDataTheMeshPackHoldsEveryMeshAndItsTextures)
{
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*game / "Data" / "AllMeshes.g3d"), pack::PackResult::Success);
	EXPECT_FALSE(pack.GetMeshes().empty());
	EXPECT_FALSE(pack.GetTextures().empty());
	for (size_t i = 0; i < pack.GetMeshes().size(); ++i)
	{
		const auto& mesh = pack.GetMesh(static_cast<uint32_t>(i));
		ASSERT_GE(mesh.size(), 4u) << i;
		EXPECT_EQ(std::string(mesh.begin(), mesh.begin() + 4), "L3D0") << i;
	}
}

// The game's animation pack holds every animation, each one whole: its header's frames are the keyframes it holds
TEST(PackFormats, realDataTheAnimationPackHoldsEveryAnimationWhole)
{
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*game / "Data" / "AllAnims.anm"), pack::PackResult::Success);
	ASSERT_FALSE(pack.GetAnimations().empty());
	for (size_t i = 0; i < pack.GetAnimations().size(); ++i)
	{
		anm::ANMFile animation;
		ASSERT_EQ(animation.Open(pack.GetAnimation(static_cast<uint32_t>(i))), anm::ANMResult::Success) << i;
		EXPECT_EQ(animation.GetKeyframes().size(), animation.GetHeader().frameCount) << i;
	}
}
