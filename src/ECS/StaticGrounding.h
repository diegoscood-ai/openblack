/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

namespace openblack::ecs
{
/// Mod (not in the original, EngineConfig::groundStaticObjects): lowers floating mobile statics (rocks,
/// boulders...) until their lowest vertex touches the landscape. The original places them at GetAltitude + the
/// script altitude with no correction (MobileStatic::GetWorldMatrix 0x608DE0), so meshes whose origin differs from the
/// original ones (mods) float. Objects that are partly buried are left as they are.
class StaticGrounding
{
public:
	/// Gap between the lowest vertex and the landscape under it (negative: partly buried).
	[[nodiscard]] static float FloatingGap(entt::entity entity);
	/// Lowers the object by its floating gap (remembered so that Unground can undo it).
	static void Ground(entt::entity entity);
	/// Puts the object back where the script placed it.
	static void Unground(entt::entity entity);
	/// Grounds or restores every mobile static, e.g. when the option is toggled.
	static void ApplyToAll(bool ground);
	StaticGrounding() = delete;
};
} // namespace openblack::ecs
