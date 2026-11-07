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

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// A crop field (GFieldTypeInfo, the same values for all 6 types)
struct Field
{
	static constexpr float k_AgeGrowth = 80.0f;     ///< ageGrowth: growing, then ripening
	static constexpr float k_AgeRecolt = 1200.0f;   ///< ageRecolt: ripe
	static constexpr uint8_t k_TimesToSow = 30;     ///< timesToSow: crops before it grows
	static constexpr float k_TotalFood = 350.0f;    ///< totalFoodInField
	static constexpr float k_TakenWithHand = 25.0f; ///< foodValueTakenWithHand (= HandFood amountPickedUpInitially)

	int town;          ///< a Town::id (-1: none)
	uint8_t crops {0}; ///< crops sown
	float growth {0.0f};
	float food {0.0f};
	uint8_t turnOffset {0};    ///< random 0..9
	openblack::Zoomer sink {}; ///< food / 350 - 1, its one-second ease started again each time the crop is drawn
	/// How far the crop's last draw raised the matrix it is drawn with (2 x the sink, held at -0.8, x scale x the mesh's
	/// half height): only the drawn matrix, as the original puts its matrix back once the crop is queued; the field's
	/// own position never sinks
	float drawnRise {0.0f};
	/// the farmers, newest first; the count is the size
	std::vector<entt::entity> farmers {};
	/// (openblack) the field's dependencies have been deleted (at the mark: abodes::OnToBeDeleted). The
	/// on_destroy<Field> listener then does not delete them again: the original does it once, when the field is
	/// deleted
	bool dependantsDeleted {false};
	/// the GFieldTypeInfo, an index of InfoConstants::fieldType
	FieldTypeInfo type {FieldTypeInfo::Wheat};
};

} // namespace openblack::ecs::components
