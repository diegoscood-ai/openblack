/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicPiles.h"

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Pot.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
glm::vec3 OnLand(const glm::vec3& position)
{
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                         : position.y;
	return {position.x, ground, position.z};
}
} // namespace

entt::entity objects::CreateMagicResourcePile(const glm::vec3& position, std::optional<PlayerNames> player, ResourceType type,
                                              uint32_t amount, bool allowEmpty)
{
	// fn_005FA8B0; CallVirtualFunctionsForCreation (vt 0x658) is the pile's rise out of the land (PotArchetype::Create).
	// MagicFood::CallVirtualFunctionsForCreation 0x5FAAB0, after PileFood's (0x66E1A0), also calls two setters on its
	// Game3DObject (+0x40): vt 0x78(0) at 0x5FAAC8 and vt 0x80(0) at 0x5FAAD2, LH3DObject's fn_008168A0 and
	// fn_007F9880 (vtable 0x9A2974): obj+4 bit 0x40 cleared, so the pile receives no projected shadow (the receiver test
	// 0x80E457, vt+0x7C fn_007F9870), and bit 0x1000 cleared. (aproximado) bw1-decomp names the slots
	// SetCastDynamicShadow / SetShadowOnTexture; both take their argument in edx (0x5FAAD0 `xor edx, edx`).
	// Nothing to do here: RenderingSystem.cpp's ReceivesDynamicShadow names this very call (0x5FAAC8) for the MagicFood
	// and HandFood pot types, CastsStaticShadow rejects every Pot, and Graphics/ShadowList.cpp's CastsPhysicsShadow
	// rejects every Pot too.
	switch (type)
	{
	case ResourceType::Food:
		return CreateMagicFood(position, player, amount, allowEmpty);
	case ResourceType::Wood:
		return CreateMagicWood(position, player, amount, allowEmpty);
	default:
		return entt::null;
	}
}

entt::entity objects::CreateMagicFood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                                      bool allowEmpty)
{
	// PileFood(pos, GPotInfo 0xD4D308 = info 10, amount, town, 0, 0, 1.0); the scale 0.3 is PotArchetype's
	const auto pile = ecs::archetypes::PotArchetype::Create(OnLand(position), 0.0f, PotInfo::MagicFood,
	                                                        static_cast<int32_t>(amount), allowEmpty);
	if (pile != entt::null)
	{
		// +0xBC (0x5FA9F0): NULL -> g_game +0x18 + byte g_game[0x205A5B] * 0xA60, the neutral player (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
		// CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9 -> Object::InsertMapObject 0x636740): the pile
		// (type 21, counted as fixed) at the tail of its cell's fixed list at once
		ecs::map_cells::InsertMapObject(pile);
	}
	return pile;
}
