/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace openblack::ecs
{
class Registry;

/// How the original draws villagers and animals between game turns (docs/bw1-notes/animation.md; research
/// dev\tmp_dis\anim\draw_interp.md). The simulation moves them once per turn (10 per second); every rendered frame
/// fn_0051AF00 draws a moving one between its position at the start and the end of the last turn by the turn's fraction
/// (one turn behind, the two land heights lerped), Villager::Draw turns the drawn yaw towards the real one (3 rad/s, faster
/// past 90 degrees), and fn_0051B220 shears it along the slope while it stands on the land.

/// Living::ProcessLiving (0x5EC810), each turn before the living move: the start of this turn's move
void BeginMobileTurn();

/// Every frame: the drawn positions, with `turnFraction` (0..0.99, g_game+0x205D64) and the frame's game milliseconds
void UpdateMobileDrawing(float turnFraction, float milliseconds);

/// The model matrix a mobile object is drawn with this frame (CarriedProps' grip, the SuperVillagers' on-screen test and
/// swim rings; through DrawnBodyModel RenderingSystem's instance matrix and the eyes): in the physics its pose between
/// its last two turns (PhysicsDrawPose, fn_007FCE80, no slope shear), else its DrawPosition (between turns, turning),
/// else its Transform; T(p) R S with the transform's scale (lh_matrix::Model, the Set* of the original 0x423195 /
/// 0x6382B7 / 0x607606); then, with a DrawPosition and slopeShear, the slope shear of fn_0051B220 (row 1 added to rows 0
/// and 2). slopeShear false: the matrix the villagers' foot shadows are cast from (Renderer.cpp). Needs a Transform
[[nodiscard]] glm::mat4 DrawnModel(const Registry& registry, entt::entity entity, bool slopeShear = true);

/// The matrix the body is drawn with: DrawnModel, then a SuperVillager's own turn (ECS/SuperVillager.h): fn_00825530
/// copies the sheared object matrix (0x8255AB rep movsd, 12 dwords) and turns only that copy about its own Y by
/// DrawPosition::followDrawnTurn (0x825626..0x8256BB, lh_matrix::RotateY, rows 0 and 2). DrawnModel itself for every
/// other object (the turn is 0). RenderingSystem's instance matrix and the SuperVillagers' eyes
[[nodiscard]] glm::mat4 DrawnBodyModel(const Registry& registry, entt::entity entity);

/// The object jumped (EndPhysics, the hand, a script teleport): no slide from where it was
void SnapDrawPosition(entt::entity entity);

/// fn_00825530 0x8255C1..0x825626, a SuperVillager's yaw (DrawPosition::follow*) once a frame: followYaw = Wrap(it); equal
/// to Wrap(target) or snap: it takes the target, no turn; else it steps (ms x 0.001) x radiansPerSecond towards it (Wrap of
/// the difference), taking it when the difference is not more than a step (no turn). Returns the turn of the body's
/// drawn copy about its own Y (followYaw - Wrap(target), lh_matrix::RotateY; DrawPosition::followDrawnTurn), 0 for none
float StepYawFollow(float& followYaw, float target, float milliseconds, float radiansPerSecond, bool snap);

} // namespace openblack::ecs
