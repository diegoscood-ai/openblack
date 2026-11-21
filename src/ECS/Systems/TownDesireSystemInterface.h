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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// What the towns want (components::TownDesire, see town_desire). Each town works out its desires in its own process
/// (town_process), so there is no turn-wide pass here
class TownDesireSystemInterface
{
public:
	virtual ~TownDesireSystemInterface() = default;
	/// Offers a villager to its town's desires, most wanted first, past its trigger; `satisfy` tries a desire and
	/// returns 1 when the villager took it up. 1 when one did.
	virtual uint32_t OfferVillager(entt::entity town, bool child, float trigger,
	                               const std::function<uint32_t(TownDesireInfo)>& satisfy) = 0;
	/// A desire of a town as the villagers see it, and as the town feels it; none without a town
	[[nodiscard]] virtual float GetDesire(entt::entity town, TownDesireInfo desire) const = 0;
	[[nodiscard]] virtual float GetRawDesire(entt::entity town, TownDesireInfo desire) const = 0;
	/// The desire a town wants most this turn, None without a town
	[[nodiscard]] virtual TownDesireInfo GetMostWanted(entt::entity town) const = 0;
	/// The scripts' boost to a desire; `resort` puts the town's order right at once
	virtual void SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort) = 0;

	/// Once a frame: each town's store flies a flag for some of its desires (made with the town). The flags of a town
	/// that has gone go too
	virtual void UpdateFlags() = 0;
	/// A storage pit of the town has just been made, a plan's or a whole one: the town's flags go to their places on it
	/// (whichever store the town works from)
	virtual void OnStoragePitMade(entt::entity town, entt::entity pit) = 0;
	/// A flag's Draw, which the object draw list calls on the frames it lists the flag, by the game's time passed in the
	/// frame and the camera's eye: it rises as high as its town wants its thing and turns to the eye while the store
	/// stands built, and is put straight at the desire while it does not show. Whether it shows (its mesh is then
	/// drawn); false for anything not a flag
	[[nodiscard]] virtual bool DrawFlag(entt::entity flag, float seconds, const glm::vec3& eye) = 0;
	/// The town's turn for its flags: each takes the town's raw desire with the boosts, which the flag's frames fly it
	/// for until the next turn
	virtual void ProcessFlags(entt::entity town) = 0;
	/// What the hand shows over a flag: its desire, whose words it shows among the game's tooltips, and its town's raw
	/// desire. None for anything not a flag showing.
	struct FlagToolTip
	{
		TownDesireInfo desire;
		float rawDesire;
	};
	[[nodiscard]] virtual std::optional<FlagToolTip> GetFlagToolTip(entt::entity flag) const = 0;
};

} // namespace openblack::ecs::systems
