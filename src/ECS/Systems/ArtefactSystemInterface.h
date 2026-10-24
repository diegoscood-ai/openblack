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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The artefacts (components::TownArtefact): rocks and other loose things a player put down gently by a town or a worship
/// site. Their towns' people dance round them, which raises their worth in their owner's town and wins their owner
/// belief in any other; worthy ones glow and show their owner's symbol.
class ArtefactSystemInterface
{
public:
	virtual ~ArtefactSystemInterface() = default;

	/// An object becomes an artefact of a town, or joins it again: made worth what its kind impresses villagers if it is
	/// new, kept with its worth and owner otherwise; the player's creature may learn to copy the deed
	virtual void AddToTown(entt::entity object, entt::entity town, PlayerNames player) = 0;
	/// The same at a worship site, where it gets no dance of its own but is powered up by the site's worship
	virtual void AddToSite(entt::entity object, entt::entity site, PlayerNames player) = 0;
	/// A land script makes an object an artefact of its player in no town: made worth what its kind impresses villagers
	/// if it is new, kept with its worth and owner otherwise. Nothing by default
	virtual void AddToNoTown(entt::entity /*object*/, PlayerNames /*player*/) {}
	/// A hand takes an artefact: it leaves its town and site, its dancers going back to their lives, and whoever took it
	/// owns it
	virtual void Taken(entt::entity object, PlayerNames player) = 0;

	/// A town's turn of its artefacts, newest first, as the town takes its own turn. A dance that has started sends its
	/// dancers dancing and wins the owner belief from a town that isn't the owner's; one that is over raises the
	/// artefact's worth in its owner's town and sends its dancers back to waiting (if the town most wants to relax) or
	/// to their lives; one left with no dancers stops.
	virtual void ProcessTown(entt::entity town) = 0;
	/// A villager wanting to relax is offered its town's football pitch first, then its artefacts: it joins the dance of
	/// the artefact it wants most, if it wants one more than the pitch; the pitch chosen, it does nothing, as no football
	/// is played. Whether it joined a dance.
	virtual bool OfferRelaxation(entt::entity town, entt::entity villager) = 0;
	/// A worship site's power-up of its artefacts, every thousand turns, by how many of its player's people dance there
	virtual void PowerUpSite(entt::entity site, uint32_t dancers) = 0;
	/// An artefact's worth is set, as a dance or a site's power-up sets it: it glows once worth enough, as strongly as it
	/// is worth (the debug bar's Artefacts window)
	virtual void SetWorth(entt::entity object, float worth) = 0;

	/// How many of its towns' artefacts a player owns, for the statistics
	[[nodiscard]] virtual uint32_t CountFor(PlayerNames player) const = 0;
	/// How many times as impressive an object is for being an artefact: 1 + its worth once it glows, else 1
	[[nodiscard]] virtual float ImpressiveModifier(entt::entity object) const = 0;
};

} // namespace openblack::ecs::systems
