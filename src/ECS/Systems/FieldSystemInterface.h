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

#include <optional>

#include <entt/entity/fwd.hpp>

#include "3D/FieldCrop.h"

namespace openblack::ecs::components
{
struct Field;
}

namespace openblack::ecs::systems
{

/// The crops growing in the towns' fields (components::Field)
class FieldSystemInterface
{
public:
	virtual ~FieldSystemInterface() = default;

	/// Once a game turn: every field's turn, in the registry's order; each field's crop grows on its own turn in every
	/// ten, by the land's alignment and the weather
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// A field's draw, on the frames the object draw list draws it: a crop that shows starts its one-second ease to the
	/// height its food gives again and steps it by the frame's whole game milliseconds (none while paused); one too
	/// young or too empty is hidden and not stepped. Whether the crop shows, so is drawn and tested on the screen
	virtual bool Draw(entt::entity field, uint32_t frameGameMs) = 0;

	/// How a field's crop looks, or nothing while it is too small to show
	[[nodiscard]] virtual std::optional<field_crop::Look> GetLook(const components::Field& field) const = 0;
};

} // namespace openblack::ecs::systems
