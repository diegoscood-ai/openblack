/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// The thing's shadow on the land's texture is switched off, whatever its kind casts (abodes::CastsShadowOnTexture): the
/// beach temple puzzle puts this on its three smaller rings as it starts, and a ring it makes again later has none
struct NoShadowOnTexture
{
};

} // namespace openblack::ecs::components
