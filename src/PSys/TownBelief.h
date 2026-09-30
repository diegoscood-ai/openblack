/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "PSysManager.h"

namespace openblack::psys::town_belief
{
/// The belief symbols over every town centre (TOWN_BELIEF, SF_TownBelief: TownCentre::CreatePSys 0x69BC10,
/// UR_TownCentreBelief 0x69BF30, ParticlePlayerSymbol / PlayerSymbolSprite::Draw 0x69D7E0). Steps once per rendered
/// frame with dt = 0.1 s like the original (TownCentre::DrawAll from Process3dEngine).
void Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out);
void Clear();
} // namespace openblack::psys::town_belief
