/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand demos' cache (HandDemoManager): its loader keeps the records hand_demo::ParseRecords reads, from made-up
// bytes

#include <cstdint>
#include <cstring>

#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Input/HandDemo.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

namespace
{
/// A record of `message` with its mouse at (x, 0.5) and its visual clock at `timeMs`
void Append(std::vector<uint8_t>& bytes, uint32_t message, float x, uint32_t timeMs)
{
	const auto offset = bytes.size();
	bytes.resize(offset + hand_demo::k_RecordSize, 0);
	const float y = 0.5f;
	std::memcpy(bytes.data() + offset + 0x00, &message, sizeof(message));
	std::memcpy(bytes.data() + offset + 0x34, &x, sizeof(x));
	std::memcpy(bytes.data() + offset + 0x38, &y, sizeof(y));
	std::memcpy(bytes.data() + offset + 0x60, &timeMs, sizeof(timeMs));
}
} // namespace

TEST(HandDemoCache, TheResourcesKeepTheRecordsTheFileGives)
{
	std::vector<uint8_t> bytes;
	Append(bytes, 1, 0.25f, 100);
	Append(bytes, 2, 0.75f, 200);
	resources::HandDemoManager demos;
	constexpr auto k_Id = entt::hashed_string("Demo");
	demos.Load(k_Id.value(), resources::HandDemoLoader::FromBufferTag {}, bytes);
	ASSERT_TRUE(demos.Contains(k_Id.value()));
	const auto& records = *demos.Handle(k_Id.value());
	const auto parsed = hand_demo::ParseRecords(bytes);
	ASSERT_EQ(records.size(), 2u);
	ASSERT_EQ(parsed.size(), 2u);
	for (size_t i = 0; i < records.size(); ++i)
	{
		EXPECT_EQ(records.at(i).message, parsed.at(i).message) << i;
		EXPECT_EQ(records.at(i).mouse, parsed.at(i).mouse) << i;
		EXPECT_EQ(records.at(i).timeMs, parsed.at(i).timeMs) << i;
	}
	EXPECT_EQ(records.at(1).message, 2u);
	EXPECT_EQ(records.at(1).mouse.x, 0.75f);
	EXPECT_EQ(records.at(1).timeMs, 200u);
}
