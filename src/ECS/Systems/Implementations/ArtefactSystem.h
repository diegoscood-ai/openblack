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
#include <entt/entity/fwd.hpp>

#include "ECS/Components/TownArtefact.h"
#include "ECS/Systems/ArtefactSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class ArtefactSystem final: public ArtefactSystemInterface
{
public:
	ArtefactSystem() = default;
	ArtefactSystem(const ArtefactSystem&) = delete;
	ArtefactSystem& operator=(const ArtefactSystem&) = delete;
	ArtefactSystem(ArtefactSystem&&) = delete;
	ArtefactSystem& operator=(ArtefactSystem&&) = delete;
	~ArtefactSystem() override;

	void AddToTown(entt::entity object, entt::entity town, PlayerNames player) override;
	void AddToSite(entt::entity object, entt::entity site, PlayerNames player) override;
	void AddToNoTown(entt::entity object, PlayerNames player) override;
	void Taken(entt::entity object, PlayerNames player) override;
	void ProcessTown(entt::entity town) override;
	bool OfferRelaxation(entt::entity town, entt::entity villager) override;
	void PowerUpSite(entt::entity site, uint32_t dancers) override;
	void SetWorth(entt::entity object, float worth) override;
	[[nodiscard]] uint32_t CountFor(PlayerNames player) const override;
	[[nodiscard]] float ImpressiveModifier(entt::entity object) const override;
	void ClearMap() override;
	[[nodiscard]] uint32_t CountGone() const override;

private:
	/// A town's artefacts, newest first
	[[nodiscard]] std::vector<entt::entity> ArtefactsOf(entt::entity town) const;
	/// A town's turn of one of its artefacts
	void Process(entt::entity object, components::TownArtefact& artefact);
	/// The dance is over, or made to end: the worth its dancers raised, and where they go
	void StopDanceIfNecessary(entt::entity object, components::TownArtefact& artefact, bool force);
	/// While its dance is on in a town that isn't its owner's, the town gives the owner belief now and then
	void GiveBeliefIfNecessary(entt::entity object, components::TownArtefact& artefact);
	/// A villager's wish to join the artefact's dance
	[[nodiscard]] float JoiningDesire(entt::entity object, const components::TownArtefact& artefact,
	                                  entt::entity villager) const;
	/// The villager joins the artefact's dance, made if need be, and waits for it to start. Whether it joined.
	bool JoinDance(entt::entity object, components::TownArtefact& artefact, entt::entity villager);
	/// Its worth is set: it glows once worth enough, as strongly as it is worth
	void SetValue(entt::entity object, components::TownArtefact& artefact, float value);
	/// It starts glowing, and its owner's symbol is made, once it is worth enough and has no glow
	void CreateEffectIfNecessary(entt::entity object, components::TownArtefact& artefact);
	/// It leaves its town's and site's artefacts: a dance under way sends its dancers back to their lives
	void Remove(components::TownArtefact& artefact);
	/// It goes, its object having gone: out of its town and site, then its dance goes, its dancers leaving it, and its
	/// glow goes at once
	void Delete(components::TownArtefact& artefact);
	/// An artefact's object is destroyed: one among its town's artefacts is kept, as it was, for its town's next turn
	void OnDestroyed(entt::registry& registry, entt::entity object);
	/// Told of every artefact's object destroyed from now on, or no more
	void Listen();
	void StopListening();
	/// Every villager of the dance goes into a state
	void SetDancersState(entt::entity dance, VillagerStates state) const;
	/// A new artefact, worth what its kind impresses villagers, or the one it already is
	components::TownArtefact& Record(entt::entity object, PlayerNames player);

	/// How many artefacts have joined towns, to keep each town's newest first
	uint64_t _joined {0};
	/// The artefacts of towns whose objects have gone since their towns' last turn of them, as they were then
	std::vector<components::TownArtefact> _gone;
	/// The registry told to call OnDestroyed, if any
	Registry* _listening {nullptr};
};

} // namespace openblack::ecs::systems
