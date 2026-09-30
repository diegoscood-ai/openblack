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
namespace components
{
/// A prop in a villager's hand (ECS/CarriedProps.h): drawn like any mesh, moved every frame
struct CarriedProp
{
	entt::entity owner {entt::null};
	int32_t type {0};
};
} // namespace components

/// The object a villager carries, drawn in its hand like the original (docs/bw1-notes/animation.md; research
/// dev\tmp_dis\anim\sounds_props.md B): Villager::Draw -> fn_0051BAF0 draws CarriedObject::Get3DCarriedObject(+0xF1)
/// (one mesh per CARRIED_OBJECT, CarriedObject::Init 0x462530) linked to bone 15 of the villager's pose, the grip at the
/// end of its -X arm (LH3DStaticObject::SetLinkedPosition 0x815FC0: rows -X, -Z, -Y of the bone, no offset). Not while
/// the villager is hidden or in the hand. Runs after the poses are computed.
void UpdateCarriedProps();

} // namespace openblack::ecs
