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

/// A crop field (Field, 0x124 bytes; GFieldTypeInfo, the same values for all 6 types)
struct Field
{
	static constexpr float k_AgeGrowth = 80.0f;    ///< ageGrowth: growing, then ripening
	static constexpr float k_AgeRecolt = 1200.0f;  ///< ageRecolt: ripe
	static constexpr uint8_t k_TimesToSow = 30;    ///< timesToSow: crops before it grows
	static constexpr float k_TotalFood = 350.0f;   ///< totalFoodInField
	static constexpr float k_TakenWithHand = 25.0f; ///< foodValueTakenWithHand (= HandFood amountPickedUpInitially)

	int town;               ///< +0x118, a Town::id (-1: none)
	uint8_t crops {0};      ///< +0xCC crops sown
	float growth {0.0f};    ///< +0xD0
	float food {0.0f};      ///< +0xDC
	uint8_t turnOffset {0}; ///< +0x11C, random 0..9
	openblack::Zoomer sink {}; ///< +0xE4 / +0xE8: food / 350 - 1, eased over 1 s
	float sinkTarget {0.0f};
	bool sinkStarted {false};
	/// +0xD4 / +0xD8: the farmers (8-byte nodes {next, villager}, head insertion in AddFarmer 0x528401..0x528420),
	/// newest first; the count is the size
	std::vector<entt::entity> farmers {};
	/// +0x120: the GFieldTypeInfo (the ctor 0x527E23), an index of InfoConstants::fieldType
	FieldTypeInfo type {FieldTypeInfo::Wheat};
};

} // namespace openblack::ecs::components
