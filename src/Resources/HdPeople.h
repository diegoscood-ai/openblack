/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include "HdTextures.h"

namespace openblack::pack
{
struct G3DTexture;
} // namespace openblack::pack

namespace openblack::resources
{

/// Mod graphics.hd-people: the villagers' textures and meshes of AllMeshes.g3d as the mod's options say, also when they
/// change during the game (no restart): Update reloads only the villager textures and the villagers' boned meshes.
namespace hd_people
{
/// Before AllMeshes.g3d is loaded: the mod's image list (EngineConfig::hdPeopleSkins gets its ids) and the options in
/// effect from now on
HdTextures Begin();

/// Loads one pack texture, the mod's HD image instead when the textures option is on
void LoadTexture(const HdTextures& hdTextures, const std::string& name, const pack::G3DTexture& g3dTexture);

/// Once a frame, before anything is drawn: when the options differ from the ones the resources were loaded with, reloads
/// the villager textures and meshes from AllMeshes.g3d
void Update();
} // namespace hd_people

} // namespace openblack::resources
