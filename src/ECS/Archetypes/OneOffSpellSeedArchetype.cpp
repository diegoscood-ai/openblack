/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OneOffSpellSeedArchetype.h"

#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Graphics/Lh3dColour.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/SpellSeedGraphic.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity OneOffSpellSeedArchetype::Create(const glm::vec3& position, SpellSeedType seedType, int powerUp, float scale)
{
	const auto index = static_cast<int>(seedType);
	if (index <= -1 || index >= 30) // OneOffSpellSeed::Create 0x72A2F8 / 0x72A301 (cmp -1, cmp 0x1E)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// MobileObject(pos, info, parent 0, y angle 0, scale 1): the orb is always drawn at scale 1; +0x6C keeps `scale`
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(k_MeshName), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// fn_0072A400 then the Create arguments
	auto& orb = registry.Assign<OneOffSpellSeed>(entity, seedType, scale, entt::null, 0.0f, powerUp);
	// the 4x4 texture animation (UpdateFrame 0x72A570) goes through the object's UV offset
	registry.Assign<UvScroll>(entity);
	// Draw 0x518E90 tints the object with 0x96FFFFFF ([0xBE8E8C], fn_0080BF10: diffuse alpha 0xFF * 0x96 >> 8 = 0x95) and
	// SetGlobalAlpha(1) (LH3DObject vt 0x48, flags1 0x80), which switches to the alternative mode table 0xC387C8. The
	// cap's material is already mode 12 (GJUtils::SetMaterialProperties at load, Game.cpp), which that table keeps:
	// additive SRCALPHA / ONE, alpha = texture x diffuse (0x95), Z write
	registry.Assign<Alpha>(entity,
	                       static_cast<float>(lh3d_colour::Alpha(lh3d_colour::MulShr8_4(0xFF000000u, 0x96FFFFFFu))) / 255.0f);
	// CallVirtualFunctionsForCreation 0x72A4C4: unless the object flag 0x100 (+0xA bit 0, never set on a new orb) the
	// seed inside, SpellSeedGraphic::Create 0x726F60(pos, seed, the local player (g_game +0x205A5B), 1.0, +0x78 pu), in
	// +0x70, with SetAutoUpdate(0) (Worship/SpellSeedGraphic.cpp)
	orb.graphic = worship::seed_graphic::Create(position, seedType, PlayerNames::PLAYER_ONE, 1.0f, powerUp);
	if (orb.graphic != entt::null)
	{
		worship::seed_graphic::SetAutoUpdate(orb.graphic, false);
	}
	return entity;
}
