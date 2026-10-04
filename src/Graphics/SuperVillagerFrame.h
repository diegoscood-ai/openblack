/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_set>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>

// The SuperVillagers' part of the frame (ECS/SuperVillager.h), decided before the draw as Graphics/OverlayFrame.h is
// (Motor M2, dev\documentacion\motor\M2_snapshot.md): Game.cpp fills it once a frame before DrawScene
// (ecs::super_villager::FillFrame, next to FillOverlayFrame) and the Renderer reads it through
// DrawSceneDesc::superVillagers; nothing in the draw reads the SuperVillager list or asks the registry for it.
namespace openblack::graphics
{

/// What fn_008254A0 and GLandscape::Draw's swim cut need of the SuperVillager list this frame
struct SuperVillagerFrame
{
	/// The meshes drawn with the light at the default sun (fn_008254A0 0x8254C3..0x8254D1 / 0x82551F): the HD bodies
	/// of the list and their eye meshes
	std::unordered_set<entt::id_type> litByDefaultSun;
	/// The ones animated "M_P_Swim2" (0x5E4C07), list order (g_first [0xEB9A08], the newest first): cut under the water
	std::vector<entt::entity> swimmers;
};

} // namespace openblack::graphics
