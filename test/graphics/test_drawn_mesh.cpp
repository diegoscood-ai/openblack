/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/DrawnMesh.h"

namespace components = openblack::ecs::components;
namespace detail_level = openblack::graphics::detail_level;
using openblack::ecs::drawn_mesh::Of;

namespace
{
constexpr components::Mesh k_Std {.id = 112, .submeshId = 0, .bbSubmeshId = 0};
constexpr components::DetailMesh k_High {.high = 263};
constexpr components::DrawMesh k_PartlyBuilt {.id = 77, .submeshId = 2, .bbSubmeshId = 3};
} // namespace

TEST(DrawnMesh, BelowTheTopLevelTheMeshIsDrawn)
{
	for (uint8_t level = 0; level < detail_level::k_Top; ++level)
	{
		EXPECT_EQ(Of(nullptr, k_Std, &k_High, level).id, k_Std.id) << static_cast<int>(level);
	}
}

TEST(DrawnMesh, AtTheTopLevelTheHighMeshIsDrawnWithTheMeshSubMeshes)
{
	constexpr components::Mesh k_WithSubMeshes {.id = 112, .submeshId = 1, .bbSubmeshId = -1};
	const auto drawn = Of(nullptr, k_WithSubMeshes, &k_High, detail_level::k_Top);
	EXPECT_EQ(drawn.id, k_High.high);
	EXPECT_EQ(drawn.submeshId, 1);
	EXPECT_EQ(drawn.bbSubmeshId, -1);
}

TEST(DrawnMesh, WithoutAHighMeshTheMeshIsDrawnAtEveryLevel)
{
	EXPECT_EQ(Of(nullptr, k_Std, nullptr, detail_level::k_Top).id, k_Std.id);
	EXPECT_EQ(Of(nullptr, k_Std, nullptr, detail_level::k_Default).id, k_Std.id);
}

TEST(DrawnMesh, APartlyBuiltModelWinsAtEveryLevel)
{
	for (const uint8_t level : {uint8_t {0}, detail_level::k_Default, detail_level::k_Top})
	{
		const auto drawn = Of(&k_PartlyBuilt, k_Std, &k_High, level);
		EXPECT_EQ(drawn.id, k_PartlyBuilt.id);
		EXPECT_EQ(drawn.submeshId, k_PartlyBuilt.submeshId);
		EXPECT_EQ(drawn.bbSubmeshId, k_PartlyBuilt.bbSubmeshId);
	}
}
