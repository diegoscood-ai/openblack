/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A fish of the shoal (fn_00824740, 0x24 bytes): an LH3DSprite of misc0.raw lying flat on the water
struct Fish
{
	glm::vec3 position;
	float halfSize;
	float heading;  ///< radians, the direction (cos, 0, sin)
	float speed;    ///< units per second
	float turnRate; ///< radians per second
	float frame;    ///< +0x1C animation phase 0..15 (fn_008248E0, frame_anim::FishFrame)
	float fleeTime; ///< seconds left fleeing a splash
	uint8_t cell {8}; ///< the sprite's cell (+0x28 & 0x3F), 8..23, set from the frame before it wraps
};

/// The shoal of a fish farm (FishFarm::CallVirtualFunctionsForCreation 0x52CC10, 0x68 bytes, list 0xEB99F4)
struct FishShoal
{
	static constexpr size_t k_FishCount = 15; ///< 15 x [+0x64] (= 1)
	static constexpr float k_Range = 7.0f;    ///< +0x58: the target wanders this far from the centre

	glm::vec3 centre;
	glm::vec3 target;
	float timer {0.0f}; ///< seconds until a new target
	std::array<Fish, k_FishCount> fish;
	/// +0x5C: the fish puzzle's bait (components::FishBait) the shoal swims for, or null (the fish farms). Such a shoal
	/// is not fished with the hand (fn_00824B10) and is no longer drawn once its bait is done (fn_00824DA0)
	entt::entity bait {entt::null};
	uint8_t alpha {255}; ///< this frame, from the camera distance
	bool visible {false};
	size_t shown {k_FishCount}; ///< fish shown this frame (FishFarm::VisibleFish)
};

/// The fish puzzle's net of floats (FishPlot, ctor 0x829A30, 0x90 bytes): Data\MISC\Fishplot.l3d (one float, a
/// static LH3DObject with dynamic lighting) drawn at 7 points on a circle round the bait
struct FishPlot
{
	static constexpr size_t k_Floats = 7;

	glm::vec3 centre;                         ///< +0x08
	std::array<glm::vec3, k_Floats> points;   ///< +0x14: centre + r (cos(i 2pi/7), 0, sin(i 2pi/7))
	float phase {0.0f};                       ///< +0x68: the bobbing, += 2 dt per draw under the water
	float closure {1.0f};                     ///< +0x88: 1 open .. 0 closed, the radius is 1 + 10 closure
	bool closing {false};                     ///< +0x8C
	entt::id_type mesh {0};                   ///< +0x04: "misc/Fishplot" in the mesh manager (0: not loaded)
};

/// The fish puzzle's bait (PuzzleGame type 14, fn_006D7480 0x6D7FCD, 0x2C bytes): the net is done when `need` fish of
/// the shoals swimming for it are all within `radius` (x, z) at once for `holdMs` of game time (fn_00824B90)
struct FishBait
{
	glm::vec3 position;       ///< +0x00
	float radius {11.0f};     ///< +0x0C
	uint32_t need {30};       ///< +0x10
	uint32_t holdMs {500};    ///< +0x14
	bool done {false};        ///< +0x18
	uint32_t inside {0};      ///< +0x20: the fish inside this frame
	uint32_t timerMs {0};     ///< +0x24
	FishPlot net;             ///< +0x28
};

/// A fish farm (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM, GFishFarmInfo 0). No shoal when no sea was found around it.
struct FishFarm
{
	static constexpr float k_FoodValue = 1400.0f;   ///< GFishFarmInfo.foodValue: the full stock
	static constexpr uint32_t k_GrowthTurns = 16;   ///< numGameTurnsAfterWhichFoodIsIncreased: +1 food

	std::optional<FishShoal> shoal;
	float food {k_FoodValue}; ///< +0x94, full when created
	uint32_t info {0};        ///< the script's GFishFarmInfo index (0xCCFC78 + 0x128 i; info.dat only has 0)
	/// +0x8C: the ctor 0x52C360 always takes the nearest town (Town::GetNearestTownToPos, any tribe), whatever town the
	/// script gave
	entt::entity town {entt::null};
	/// +0x80 / +0x84: the fishermen (8-byte nodes {next, villager}, head insertion in AddFisherman 0x52D25C..0x52D27B),
	/// newest first; the count is the size (fish_farms::)
	std::vector<entt::entity> fishermen {};

	/// shoal +0x64 = food / foodValue; the first 15 x that fish are shown (and swim, and can be caught)
	[[nodiscard]] size_t VisibleFish() const
	{
		return static_cast<size_t>(static_cast<float>(FishShoal::k_FishCount) * food / k_FoodValue);
	}
};

} // namespace openblack::ecs::components
