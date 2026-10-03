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

entt::entity objects::CreateMagicWood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                                      bool allowEmpty)
{
	// PileResource(pos, GPotInfo 0xD4D1C4 = info 9, ...) -> PileWood; the scale 0.7 is PotArchetype's. There is no
	// Process and no expiry: the pile stays until it is emptied. IsAWoodPileOutsideStoragePit always answers 1 (creature
	// AI, not ported).
	// Unlike the food pile, the wood pile keeps whatever shadow settings every pile has (no extra setters):
	// MagicWood::CallVirtualFunctionsForCreation 0x600F10 is just a call to PileResource's 0x66E300, without the
	// SetCastDynamicShadow(0) / SetShadowOnTexture(0) that MagicFood::CallVirtualFunctionsForCreation 0x5FAAB0 adds
	// (MagicFood.cpp). So, like any Pot, it bakes no shadow (RenderingSystem.cpp CastsStaticShadow) but still takes the
	// dynamic one (ReceivesDynamicShadow).
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                         : position.y;
	const auto pile = ecs::archetypes::PotArchetype::Create(glm::vec3(position.x, ground, position.z), 0.0f,
	                                                        PotInfo::MagicWood, static_cast<int32_t>(amount), allowEmpty);
	if (pile != entt::null)
	{
		// +0xB4 (0x600E64..0x600E8A): the owner, NULL -> g_game +0x18 + byte g_game[0x205A5B] * 0xA60, the neutral player
		// (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
		// CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9 -> Object::InsertMapObject 0x636740): the pile
		// (type 21, counted as fixed) at the tail of its cell's fixed list at once
		ecs::map_cells::InsertMapObject(pile);
	}
	return pile;
}
