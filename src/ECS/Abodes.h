/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The Abode class of the original (Abode.cpp 0x401350..0x409000): every village building, the fields, the totem, the
/// storage pit, the town centre and the spell dispenser included. Only the interface tap lives here for now (milestone
/// B8 of dev\tmp_dis\audio\PLAN.md): knocking on a roof.
namespace openblack::ecs::abodes
{

/// Abode::GetAbodeType 0x4061F0: the ABODE_TYPE of the abode's GAbodeInfo (+0x28 -> +0x120). nullopt when no info record
/// matches (openblack keeps the abode number and the mesh, not the info pointer)
[[nodiscard]] std::optional<AbodeType> TypeOf(entt::entity abode);

/// Abode::InterfaceValidToTap 0x406820: always 1, every abode can be tapped
[[nodiscard]] bool InterfaceValidToTap(entt::entity abode);

/// Abode::InterfaceTap 0x406830, the packet 0x20 of a tap (GInterface::SendTap 0x5D38A0). `handPosition` is the tapping
/// interface status' +0xC8 (the hand's point). The original also remembers the abode's town in [0xC4CC6C] (0x40683F),
/// counts the knock in HowManyPeople::KnockKnock 0x829690, wakes the abode's villagers
/// (Villager::SetStateWhenTappedOnAbode 0x752B80 for the whole +0xA0 list) and, for the local player, plays the hand's
/// knocking animation 0x39 (CHand::StartFixedPosAnimation 0x46C050): none of those exist in openblack yet, so only the
/// sound is here.
void InterfaceTap(entt::entity abode, const glm::vec3& handPosition);

} // namespace openblack::ecs::abodes
