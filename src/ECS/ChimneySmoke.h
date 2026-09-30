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
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/ChimneySmoke.h"

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::components
{
struct Transform;
}

/// The chimney smoke of the houses (LH3DSmoke, research dev\tmp_dis\aldeanos\smoke.md; wiki rendering.md "Humo de las
/// chimeneas"). The renderer calls UpdateHandWind once per frame, then, for every Abode on screen, UpdateState (the
/// rules of Abode::Draw) and Advance (the Z-sorter callback fn_007F8E00, which simulates while it draws).
namespace openblack::ecs::chimney_smoke
{

/// One visible puff of this frame, as LH3DSprite::Draw 0x840530 gets it
struct DrawnPuff
{
	glm::vec3 position;
	float halfWidth; ///< the sprite's size (+0xC), world units at the puff's depth
	float angle;     ///< in the screen plane: x_v = cos x + sin y, y_v = -sin x + cos y
	uint32_t cell;   ///< 0..15, 8 cells per row of smoke.raw
	uint32_t argb;   ///< alpha << 24 | the smoke's RGB
};

/// The chimney in world space: LH3DStaticObject::GetChimneyPos 0x7F9F10, the mesh's point through the object's 3 x 3
/// matrix (rotation and scale) plus its position (with the foundation altitude)
glm::vec3 ChimneyWorldPosition(const glm::vec3& meshPoint, const components::Transform& transform);

/// Abode::CallVirtualFunctionsForCreation 0x403200: gives the abode its smoke when its mesh has a chimney (flag 0x400);
/// grey 0x808080 for a workshop, white otherwise. Does nothing for a mesh without one.
void Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform, bool workshop);

/// LH3DSmoke::Create 0x7F8B60: a new smoke at a chimney (10 hidden puffs, ages i x 90, random angles and spins)
components::ChimneySmoke Create(const glm::vec3& chimney, uint32_t rgb);

/// GLandscape::Draw 0x5E4310..0x5E448E (once per frame, before the objects are drawn): the hand position, wind and
/// speed that fn_007F8E00 reads, from the hand's 3D object and GInterfaceStatus::HandVelocity (fn_005DBC60, advanced here
/// once per game turn)
void UpdateHandWind();

/// Abode::Draw 0x516288..0x5162E0, for an abode on screen this frame: lit = PresentAtHome != 0, or a workshop making a
/// scaffold (Workshop+0xC4 != 0). Returns whether the smoke goes to the Z-sorter (LH3DSmoke::AddDrawing: state != 3).
bool UpdateState(components::ChimneySmoke& smoke, bool lit);

/// fn_007F8E00: advances the 10 puffs by g_game_time_inc (milliseconds, 0 while paused) and appends the visible ones,
/// in their order 0..9; a dying smoke that drew nothing dies (state 3)
void Advance(components::ChimneySmoke& smoke, float milliseconds, std::vector<DrawnPuff>& drawn);

/// OPENBLACK_TEST_CHIMNEY=all: every chimney smokes (for screenshots while no villager ever goes home)
bool ForcedByTestHook();

} // namespace openblack::ecs::chimney_smoke
