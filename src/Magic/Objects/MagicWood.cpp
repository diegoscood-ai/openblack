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
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;
using namespace openblack::magic;

entt::entity objects::CreateMagicWood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount)
{
	// PileResource(pos, GPotInfo 0xD4D1C4 = info 9, ...) -> PileWood; the scale 0.7 is PotArchetype's. There is no
	// Process and no expiry: the pile stays until it is emptied. IsAWoodPileOutsideStoragePit always answers 1 (creature
	// AI, not ported).
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                         : position.y;
	const auto pile = ecs::archetypes::PotArchetype::Create(glm::vec3(position.x, ground, position.z), 0.0f,
	                                                        PotInfo::MagicWood, static_cast<int32_t>(amount));
	if (pile != entt::null)
	{
		// +0xB4 (0x600E64..0x600E8A): the owner, NULL -> g_game +0x18 + byte g_game[0x205A5B] * 0xA60, the neutral player
		// (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
	}
	return pile;
}
