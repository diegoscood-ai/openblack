/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <L3DFile.h>
#include <gtest/gtest.h>

using openblack::l3d::L3DSubmeshHeader;
using openblack::l3d::LoadedFlags;

namespace
{
/// The flags as the file's 32 bits
L3DSubmeshHeader::Flags FromBits(uint32_t bits)
{
	L3DSubmeshHeader::Flags flags {};
	std::memcpy(&flags, &bits, sizeof(flags));
	return flags;
}

uint32_t ToBits(L3DSubmeshHeader::Flags flags)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &flags, sizeof(bits));
	return bits;
}
} // namespace

TEST(L3DSubmeshFlags, ASubmeshSavedWithNoLevelIsOfEveryLevel)
{
	// the forest seed's sub-mesh is saved as 0: loaded, it carries every level and the second bit of unknown2
	EXPECT_EQ(ToBits(LoadedFlags(FromBits(0x00000000u))), 0xE0000800u);
	// the other bits are kept
	EXPECT_EQ(ToBits(LoadedFlags(FromBits(0x00002001u))), 0xE0002801u);
}

TEST(L3DSubmeshFlags, ASubmeshWithALevelOrTheBitIsKeptAsSaved)
{
	// the meshes seen today carry their levels and the bit in the file
	for (const uint32_t bits : {0xE0000800u, 0xE0000801u, 0x20000000u, 0x00000800u, 0x40000000u})
	{
		EXPECT_EQ(ToBits(LoadedFlags(FromBits(bits))), bits) << std::hex << bits;
	}
}
