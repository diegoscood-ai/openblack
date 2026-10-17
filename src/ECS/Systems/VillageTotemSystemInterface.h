/******************************************************************************
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
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The totems on the town centres (components::VillageTotem): they rise with the share of their town's people who
/// worship, which the player sets by taking hold of one with the hand and sliding it up or down. The hand takes hold of
/// one as a locked select: its start, the slide on every drawn frame while it is held, its end, and the share it was
/// let go at, which reaches the totem at the next turn as a packet
class VillageTotemSystemInterface
{
public:
	virtual ~VillageTotemSystemInterface() = default;

	/// The totem is given to its town's player, as it is made and whenever its town changes player: with a worship
	/// site it turns side on to the site, it wears the icon of the player's creature, and its town's share is set to
	/// none; without a town only its own share is set to none
	virtual void AddToPlayer(entt::entity totem) = 0;
	/// The totem's icon becomes the one of its town's player's creature, or the hand without one
	virtual void SetPlayersCreature(entt::entity totem) = 0;
	/// A town's player is set: the totem of each of its town centres is given to the player
	virtual void TownChangedPlayer(entt::entity town) = 0;
	/// A player's creature is set: the totem of each of the player's towns' centres wears its icon
	virtual void PlayersCreatureSet(PlayerNames player) = 0;

	/// A town's share of its people at worship is set: always none without a worship site; otherwise its totem eases
	/// there
	virtual void SetTownShare(entt::entity town, float share) = 0;
	/// Each frame: the totems ease on by the game's milliseconds, sound the bell as they arrive, and stand at their
	/// share, or at the share the hand holds one at
	virtual void Update(float gameMilliseconds) = 0;

	/// The totem a picked plinth or icon belongs to
	[[nodiscard]] virtual std::optional<entt::entity> TotemOf(entt::entity picked) const = 0;
	/// The totem a town centre has, as a script asks for it; none for anything else
	[[nodiscard]] virtual std::optional<entt::entity> TotemOfTownCentre(entt::entity centre) const = 0;

	/// The share a totem was last set to, which a script reads as its height; none for anything that is not a totem
	[[nodiscard]] virtual std::optional<float> GetShare(entt::entity object) const = 0;
	/// A script sets a totem's height: its town's share is set to it (the totem's own without a town), as given, and the
	/// hand's share is then the one set. False for anything that is not a totem
	virtual bool SetShareFromScript(entt::entity object, float share) = 0;
	/// How much of a totem is built, which a script reads as its built percentage; none for anything that is not a totem
	[[nodiscard]] virtual std::optional<float> GetBuiltPercentage(entt::entity object) const = 0;
	/// A script sets a totem's built percentage (village_totem::PercentBuiltSetTo). False for anything that is not a
	/// totem
	virtual bool SetBuiltPercentageFromScript(entt::entity object, float percent) = 0;
	/// A script sets a totem's health: its life is set to it as given, with no clamp. False for anything that is not a
	/// totem
	virtual bool SetLifeFromScript(entt::entity object, float life) = 0;

	/// Whether a player may take hold of a totem: one of the player's own towns, the totem functional, the player's
	/// temple's heart and the town's worship site built
	[[nodiscard]] virtual bool ValidForLockedSelect(entt::entity totem, PlayerNames player) const = 0;
	/// Whether the hand holds the totem
	[[nodiscard]] virtual bool IsGripped(entt::entity totem) const = 0;
	/// The hand's press starts the locked select, on the pressing player's machine: the hand holds it at the share it
	/// was set to, and the raising sound remembers that share
	virtual void StartLockedSelect(entt::entity totem) = 0;
	/// The locked select's start packet, on every machine, before the hand holds it: the creature of the gripping player
	/// sees the player wants to impress it, and the living around react to the hand using the totem
	virtual void FriendlyStartLockedSelect(entt::entity totem, PlayerNames player) = 0;
	/// The locked select's start and end packets: the hand holds it, or no longer does
	virtual void SetGripped(entt::entity totem, bool gripped) = 0;
	/// The mouse moved up the screen by some pixels since the last drawn frame while the hand holds the totem: the hand
	/// slides it, which moves only the share it holds it at
	virtual void Slide(entt::entity totem, float upPixels, float screenHeight) = 0;
	/// Where the hand holding the totem is, and how far it reaches round it
	struct HandHold
	{
		/// Over the totem's place, as high as the hand holds the icon
		glm::vec3 position {0.0f};
		/// The icon's hold radius, which the hand closes on
		float holdRadius {0.0f};
	};
	/// The hand's hold of the totem, every drawn frame while it holds it; none without its icon
	[[nodiscard]] virtual std::optional<HandHold> GetHandHold(entt::entity totem) const = 0;
	/// The locked select ends on the pressing player's machine: the share it was held at is sent, and the raising sound
	/// ends
	virtual void EndLockedSelect(entt::entity totem) = 0;
	/// The share sent at the end reaches the totem: the creature of the town's player feels the player wanted to impress
	/// when it was raised, the town's share is set to it, and the hand's share is the one set
	virtual void LetGo(entt::entity totem, float share) = 0;
};

} // namespace openblack::ecs::systems
