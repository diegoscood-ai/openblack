/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The land blocks whose textures an object's change puts out of date: its shadow and its footprint are painted into the
/// textures of the blocks under it, so when it is put on the map's cells or taken off them, or a building is finished,
/// those blocks are painted again through the block texture queue (block_texture::BlocksMarkedBy,
/// LandIslandInterface::MarkBlocksChanged). docs/wiki/world/terrain.md#the-blocks-an-object-marks
namespace openblack::ecs::land_print_marks
{

/// Whether the object's class has its shadow put into the land's textures: trees, forests, abodes, features and the
/// fixed and mobile objects, not pots, dead trees, fields, villagers, creatures, the hand, fading things, animated
/// statics, flowers and the temple's inside. The same classes as the renderer's static shadow casters
/// (RenderingSystem's row classes)
[[nodiscard]] bool IsShadowClass(const Registry& registry, entt::entity object);

/// Whether the object's footprint goes into the land's textures, as the original's creations set it and nothing clears
/// it: a field, an animated static (the Norse gate, the Piper cave entrance), a worship site, the citadel's heart, and
/// a building made not built, also once built. Not the others (a building made whole, a feature, a tree), whatever
/// their meshes hold (docs/wiki/world/terrain.md#who-asks-for-its-footprint)
[[nodiscard]] bool HasFootprintBit(const Registry& registry, entt::entity object);

/// What an object's change reads of it: whether its shadow and its footprint go into the land's textures
struct TextureBits
{
	bool shadow {false};
	bool footprint {false};
};

/// The object's texture bits as its marks read them: the shadow for its class (IsShadowClass) unless it is a building
/// not built, and also for a building being made not built until its first insert in the map's cells is done (every
/// fixed object's shadow is on until then, and its footprint off); the footprint by HasFootprintBit
[[nodiscard]] TextureBits TextureBitsOf(const Registry& registry, entt::entity object);

/// Marks out of date the textures of the land blocks under the object, from its place, its turn, its scale and its
/// mesh (TextureBitsOf): its mesh's box when its shadow goes into the land's textures, its mesh's footprint when its
/// footprint does and the mesh has one. Nothing without a land, a Transform or a loaded mesh
void MarkBlocksUnder(entt::entity object);

} // namespace openblack::ecs::land_print_marks
