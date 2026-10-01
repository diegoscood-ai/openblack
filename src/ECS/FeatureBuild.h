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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::feature_build
{

/// The built percentage of a Feature (MultiMapFixed +0x5C), the CHL property 22 BUILT_PERCENTAGE: Land 1's
/// TheMissionaries sets the ArkDryDock to 0.2 and TheMissionariesBuildingBoat adds 0.03 a hammer blow.

/// GScript::GetProperty 0x70E1A9: GetPercentBuilt (vt+0x880) of a MultiMapFixed, 1 for anything else. Only Features
/// keep it here: nothing when the object is some other MultiMapFixed (Abode...).
[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity entity);

/// GScript::SetProperty 0x70EC69 -> fn_0052EDD0: +0x5C = max(value, 0); at >= 1 MultiMapFixed::Built 0x52EBB0 (+0x5C
/// = 1, the flags +0x58 lose 0x02 and get 0x08). Then the town's building list (0x70EC9B..0x70ECD4), which a Feature
/// never has. False when the object is not a Feature.
///
/// The draw: Feature::Draw = MultiMapFixed::Draw 0x518090; Feature::IsDrawBuilding 0x527790 is !IsBuilt() for the
/// ArkDryDock (GFeatureInfo 69) only, the building site (+0x74) for the rest, which Features never have. While it
/// holds, DrawBuilding 0x517F90 draws the model partly built (fn_00816AD0, physics::PartialBuild) at
/// GetPercentForDrawBuilding 0x52EFD0 = min(GetPercentBuilt, GetPercentRepairedFromWhenDamaged = 1 when not built),
/// and nothing at 0.
bool SetBuiltPercentage(entt::entity entity, float value);

/// Test hook OPENBLACK_TEST_BUILT_PERCENTAGE="p": Land 1's ArkDryDock where TheMissionaries makes it, at p
void RunDebugHook();

} // namespace openblack::ecs::feature_build
