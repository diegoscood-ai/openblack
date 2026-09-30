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
                                              uint32_t amount)
{
	// fn_005FA8B0; CallVirtualFunctionsForCreation (vt 0x658) is the pile's rise out of the land (PotArchetype::Create).
	// MagicFood::CallVirtualFunctionsForCreation 0x5FAAB0 also calls vt 0x78(0) and vt 0x80(0) on its Game3DObject
	// (graphic flags, UNVERIFIED): not ported.
	switch (type)
	{
	case ResourceType::Food:
		return CreateMagicFood(position, player, amount);
	case ResourceType::Wood:
		return CreateMagicWood(position, player, amount);
	default:
		return entt::null;
	}
}

entt::entity objects::CreateMagicFood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount)
{
	// PileFood(pos, GPotInfo 0xD4D308 = info 10, amount, town, 0, 0, 1.0); the scale 0.3 is PotArchetype's
	const auto pile = ecs::archetypes::PotArchetype::Create(OnLand(position), 0.0f, PotInfo::MagicFood,
	                                                        static_cast<int32_t>(amount));
	if (pile != entt::null)
	{
		// +0xBC (0x5FA9F0): NULL -> g_game +0x18 + byte g_game[0x205A5B] * 0xA60, the neutral player (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
	}
	return pile;
}
