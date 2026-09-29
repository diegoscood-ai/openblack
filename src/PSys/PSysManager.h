/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "PSys.h"

namespace openblack::psys
{

/// The running effects (PSysGlobal) and the script's spot visuals (GParticleContainer, CHL SPECIAL_EFFECT_*)
namespace manager
{
/// PSysInterface::Create: an effect from a spell file (e.g. "SF_Smoke"); 0 if the file is missing
uint32_t Start(const std::string& file, glm::vec3 origin, float magnitude);
void CloseDown(uint32_t id);
void SetOrigin(uint32_t id, glm::vec3 origin);

/// GParticleContainer::CreateSpotVisualWithSpecifiedDuration 0x63E580: SPOT_VISUAL index (GSpotVisualInfo 0xD44470),
/// seconds (< 0: forever; 0: the entry's own life), an owner object it follows and whose loss ends it. Returns the
/// container object for the script (deleting it closes the effect), or entt::null.
entt::entity CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner);

/// One game turn (GParticleContainer::Process 0x63E280, Process_ with the turn length)
void ProcessTurn(float turnSeconds);
/// OPENBLACK_TEST_PSYS: a test effect once the map is loaded
void RunDebugHooks();
void Clear();

/// For the renderer: every effect with atoms to draw, interpolated since the last turn
struct Drawable
{
	glm::vec3 origin;
	std::vector<Effect::DrawAtom> atoms;
};
std::vector<Drawable> Collect();
} // namespace manager

} // namespace openblack::psys
