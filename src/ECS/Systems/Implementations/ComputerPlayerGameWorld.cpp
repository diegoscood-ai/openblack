/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerPlayerGameWorld.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishFarms.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectImpressiveness.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Registry.h"
#include "ECS/SavedSlot.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptTypes.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/ScaffoldPlans.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Town/Workshops.h"
#include "ECS/TownDesire.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Citadel.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"

namespace openblack::ecs::systems::computer_world
{
namespace
{
using namespace components;
using computer_player::ScoreWorld;
using map_coords::MapCoords;

/// A place on the map as a point on the land, the height not read
glm::vec3 PointOf(const MapCoords& place)
{
	const auto metres = map_coords::ToMetres(place);
	return {metres.x, 0.0f, metres.y};
}

/// What the score rules read of the game, for the mind of one player: each answer is the game's own state, read as the
/// original's mind reads it (docs/wiki/hand/computer-minds.md#scoring-a-node)
class GameScoreWorld final: public ScoreWorld
{
public:
	GameScoreWorld(Registry& registry, PlayerNames me)
	    : _registry(registry)
	    , _me(me)
	{
	}

	// ---- the game and its players

	[[nodiscard]] uint32_t Turn() const override { return game_clock::Turn(); }
	[[nodiscard]] PlayerNames Me() const override { return _me; }
	[[nodiscard]] bool IsAllied(PlayerNames player, PlayerNames other) const override
	{
		return alliances::IsAllied(player, other);
	}
	[[nodiscard]] uint32_t PlayerType(PlayerNames player) const override
	{
		return static_cast<uint32_t>(magic::players::ReadMagicOf(player).playerType);
	}
	[[nodiscard]] entt::entity PlayerThing(PlayerNames player) const override { return magic::players::EntityOf(player); }
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity thing) const override
	{
		const auto* player = Has(thing) ? Read().TryGet<const Player>(thing) : nullptr;
		return player != nullptr ? std::optional(player->name) : std::nullopt;
	}
	[[nodiscard]] float InfluencePower(PlayerNames player) const override { return influence::InfluencePower(player); }
	[[nodiscard]] float VirtualInfluenceMaxDistance() const override
	{
		return Locator::infoConstants::value().citadel.virtualInfluenceMaxDistance;
	}
	[[nodiscard]] bool InMyInfluence(const MapCoords& place) const override
	{
		return influence::CalculatePlayerInfluence(_me, PointOf(place), influence::CalcType::Default, true) > 0.0f;
	}
	[[nodiscard]] std::optional<PlayerNames> MostInfluentialPlayerAt(const MapCoords& place) const override
	{
		return effects::alignment::MostInfluentialPlayer(PointOf(place));
	}
	[[nodiscard]] MapCoords PlayerFixedPlace(PlayerNames player) const override
	{
		// the citadel's heart; without one, while its computer player is on, the computer hand's map place as of its last
		// turn of steps; else the player's hand as its interface status turned it, which only the local player's has
		if (const auto heart = worship::citadel::HeartOf(worship::citadel::Of(player)); Has(heart))
		{
			return object::MapCoordsOf(heart);
		}
		if (const auto entity = magic::players::EntityOf(player); entity != entt::null)
		{
			if (const auto* mind = Read().TryGet<const ComputerPlayer>(entity); mind != nullptr && mind->switchedOn)
			{
				const auto* hand = Read().TryGet<const ComputerHand>(entity);
				return {.x = hand != nullptr ? hand->mapX : ComputerHand::k_StartingMapPlace,
				        .z = hand != nullptr ? hand->mapZ : ComputerHand::k_StartingMapPlace};
			}
		}
		if (Locator::playerSystem::has_value() && Locator::playerSystem::value().GetLocalPlayer() == player &&
		    Locator::handSystem::has_value())
		{
			const auto hand = Locator::handSystem::value().GetTurnHand();
			return {.x = map_coords::ToFixed(hand.x), .z = map_coords::ToFixed(hand.z)};
		}
		return {};
	}
	[[nodiscard]] float AverageInfluencePower(PlayerNames player) const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().Rivalry(player).averageInfluencePower : 0.0f;
	}
	[[nodiscard]] float HarmFrom(PlayerNames player, PlayerNames by) const override
	{
		if (!Locator::playerSystem::has_value())
		{
			return 0.0f;
		}
		const auto rivalry = Locator::playerSystem::value().Rivalry(player);
		const auto index = static_cast<size_t>(by);
		return index < rivalry.harmFrom.size() ? rivalry.harmFrom.at(index) : 0.0f;
	}
	[[nodiscard]] float AlignmentValue(PlayerNames player) const override { return effects::alignment::Get(player); }

	// ---- things

	[[nodiscard]] bool IsTown(entt::entity thing) const override { return Has(thing) && Read().AllOf<Town>(thing); }
	[[nodiscard]] bool IsCreature(entt::entity thing) const override { return Has(thing) && Read().AllOf<Creature>(thing); }
	[[nodiscard]] bool IsAbode(entt::entity thing) const override { return Has(thing) && Read().AllOf<Abode>(thing); }
	[[nodiscard]] bool IsObject(entt::entity thing) const override { return Has(thing) && script_type::IsObject(thing); }
	[[nodiscard]] bool HasPlace(entt::entity thing) const override { return Has(thing) && Read().AllOf<Transform>(thing); }
	[[nodiscard]] MapCoords Place(entt::entity thing) const override
	{
		return Has(thing) ? object::MapCoordsOf(thing) : MapCoords {};
	}
	[[nodiscard]] float Life(entt::entity thing) const override { return Has(thing) ? life::LifeOf(thing) : 0.0f; }
	[[nodiscard]] entt::entity TownOf(entt::entity thing) const override
	{
		if (!Has(thing))
		{
			return entt::null;
		}
		return Read().AllOf<Town>(thing) ? thing : town_queries::GetTown(thing);
	}
	[[nodiscard]] bool ScriptControlled(entt::entity thing) const override
	{
		return Has(thing) && script_held::IsControlledByScript(thing);
	}

	// ---- creatures

	[[nodiscard]] entt::entity MyCreature() const override { return CreatureOf(_me); }
	[[nodiscard]] std::vector<entt::entity> Creatures() const override { return saved_slot::CreaturesInListOrder(Read()); }
	[[nodiscard]] std::optional<PlayerNames> OwnerOf(entt::entity creature) const override
	{
		const auto* body = Has(creature) ? Read().TryGet<const Creature>(creature) : nullptr;
		return body != nullptr ? std::optional(body->owner) : std::nullopt;
	}
	[[nodiscard]] float CreatureScale(entt::entity creature) const override
	{
		const auto* body = Has(creature) ? Read().TryGet<const Creature>(creature) : nullptr;
		return body != nullptr ? body->size : 0.0f;
	}
	[[nodiscard]] float CreatureEnergy(entt::entity creature) const override
	{
		const auto* needs = Has(creature) ? Read().TryGet<const CreatureNeeds>(creature) : nullptr;
		return needs != nullptr ? needs->needs.energy : 1.0f;
	}
	[[nodiscard]] float CreatureExhaustion(entt::entity creature) const override
	{
		const auto* needs = Has(creature) ? Read().TryGet<const CreatureNeeds>(creature) : nullptr;
		return needs != nullptr ? needs->needs.exhaustion : 0.0f;
	}
	[[nodiscard]] bool IsFighting(entt::entity creature) const override
	{
		return Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature);
	}
	[[nodiscard]] creature_body::BodyActivity CreatureBodyActivity(entt::entity creature) const override
	{
		creature_body::BodyActivity activity {};
		if (!Has(creature))
		{
			return activity;
		}
		if (const auto* animation = Read().TryGet<const CreatureAnimation>(creature); animation != nullptr)
		{
			activity.playing = creature_layers::IsPlaying(animation->body);
		}
		activity.moving =
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
		if (Locator::creatureObjectActionSystem::has_value())
		{
			const auto& actions = Locator::creatureObjectActionSystem::value();
			activity.handling = actions.GetProgress(creature).has_value() || actions.IsCatching(creature);
		}
		if (const auto* fighting = Read().TryGet<const CreatureFighting>(creature); fighting != nullptr)
		{
			activity.fighting = fighting->fighter.state != creature_fight::State::Idle;
		}
		return activity;
	}
	[[nodiscard]] std::optional<uint32_t> PlanDesire(entt::entity creature) const override
	{
		// the desire of the plan the mind holds; between plans the original keeps none either (its stop of a plan writes
		// none)
		if (!Locator::creatureMindSystem::has_value())
		{
			return std::nullopt;
		}
		const auto plan = Locator::creatureMindSystem::value().HeldPlanOf(creature);
		return plan.has_value() ? std::optional(static_cast<uint32_t>(plan->desire)) : std::nullopt;
	}
	[[nodiscard]] std::optional<Leash> LeashOn() const override
	{
		// the player's leash that is on: what it is tied to, and the turn the player's leash was last tied
		std::optional<Leash> found;
		Read().Each<const CreatureLeash>([this, &found](entt::entity /*creature*/, const CreatureLeash& leash) {
			if (!found.has_value() && leash.worn.has_value() && leash.worn->holder == _me)
			{
				found = Leash {.tiedTo = leash.worn->tiedTo.value_or(entt::null), .tiedTurn = leash.lastTiedTurn};
			}
		});
		return found;
	}
	[[nodiscard]] entt::entity LeaderLeashTiedTo() const override
	{
		// the tie comes off with the leash, as the original's taking off clears it: the leash that is on gives it
		const auto leash = LeashOn();
		return leash.has_value() ? leash->tiedTo : entt::null;
	}

	// ---- towns

	[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player) const override { return map_cells::TownsOf(player); }
	[[nodiscard]] entt::entity StoragePit(entt::entity town) const override { return town_queries::GetStoragePit(town); }
	[[nodiscard]] entt::entity WorshipSiteOf(entt::entity town) const override
	{
		const auto* magic = Has(town) ? Read().TryGet<const TownMagic>(town) : nullptr;
		return magic != nullptr && Has(magic->worshipSite) ? magic->worshipSite : entt::null;
	}
	[[nodiscard]] float TownInfluenceRadius(entt::entity town) const override { return influence::TownRadius(town); }
	[[nodiscard]] Area TownArea(entt::entity town) const override
	{
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return {};
		}
		return {.min = {.x = t->areaMin.x, .z = t->areaMin.y}, .max = {.x = t->areaMax.x, .z = t->areaMax.y}};
	}
	[[nodiscard]] MapCoords TownAreaCentre(entt::entity town) const override { return town_placement::GetTownAreaCentre(town); }
	[[nodiscard]] std::vector<entt::entity> TownAbodes(entt::entity town) const override { return town_stats::AbodesOf(town); }
	[[nodiscard]] PlayerNames TownOwner(entt::entity town) const override
	{
		const auto* t = TownData(town);
		return t != nullptr ? t->owner : PlayerNames::NEUTRAL;
	}
	[[nodiscard]] Aggression TownAggression(entt::entity town, PlayerNames attacker) const override
	{
		const auto* t = TownData(town);
		const auto index = static_cast<size_t>(attacker);
		if (t == nullptr || index >= t->aggression.aggression.size())
		{
			return {};
		}
		return {.aggression = t->aggression.aggression.at(index), .lastTurn = t->aggression.lastTurns.at(index)};
	}
	[[nodiscard]] town_aggression::AttackEffect TownLastAttackEffect(entt::entity town, PlayerNames attacker) const override
	{
		const auto* t = TownData(town);
		const auto index = static_cast<size_t>(attacker);
		if (t == nullptr || index >= t->aggression.lastEffects.size())
		{
			return {};
		}
		return t->aggression.lastEffects.at(index);
	}
	[[nodiscard]] float RivalRecentRatio(entt::entity town) const override
	{
		// the most a rival not allied to the player was believed in lately, then that rival's belief over the player's,
		// doubled and squared in floats, up to 1
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return 0.0f;
		}
		const auto& belief = t->belief;
		float best = town_belief::k_RivalRecentStart;
		size_t rival = 0;
		for (size_t slot = 0; slot < 6; ++slot)
		{
			if (slot == static_cast<size_t>(_me) || alliances::IsAllied(_me, static_cast<PlayerNames>(slot)))
			{
				continue;
			}
			if (belief.recent.at(slot) > best)
			{
				best = belief.recent.at(slot);
				rival = slot;
			}
		}
		const float mine = town_belief::GetBeliefInPlayer(belief, _me);
		if (!(best > town_belief::k_RivalRecentStart) || mine == 0.0f)
		{
			return 0.0f;
		}
		const float doubled = (belief.belief.at(rival) / mine) * 2.0f;
		const float squared = doubled * doubled;
		return squared < 1.0f ? squared : 1.0f;
	}
	[[nodiscard]] float BeliefNeededOrLeft(entt::entity town) const override
	{
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return 0.0f;
		}
		return town_belief::GetBeliefInPlayer(t->belief, _me) - town_belief::GetMaxBeliefMeNotIncluded(t->belief, _me);
	}
	[[nodiscard]] float RivalBeliefRatio(entt::entity town) const override
	{
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return 0.0f;
		}
		float best = 0.0f;
		for (auto player = computer_player::NextActivePlayer(*this, std::nullopt); player.has_value();
		     player = computer_player::NextActivePlayer(*this, player))
		{
			if (*player == _me || alliances::IsAllied(_me, *player))
			{
				continue;
			}
			if (const float value = town_belief::GetBeliefInPlayer(t->belief, *player); value > best)
			{
				best = value;
			}
		}
		const float mine = town_belief::GetBeliefInPlayer(t->belief, _me);
		return mine == 0.0f ? 0.0f : best / mine;
	}
	[[nodiscard]] MapCoords DoorPlace(entt::entity building) const override
	{
		const auto door = abode_queries::GetDoorPos(building);
		return {.x = door.x, .z = door.y};
	}
	MapCoords CongregationPlace(entt::entity town) override
	{
		const auto place = town_queries::GetCongregationPos(town);
		return {.x = place.x, .z = place.y};
	}
	[[nodiscard]] entt::entity TownCentre(entt::entity town) const override
	{
		const auto* t = TownData(town);
		return t != nullptr && Has(t->centre) ? t->centre : entt::null;
	}
	[[nodiscard]] float TownBoredom(entt::entity town, uint32_t reaction) const override
	{
		return town_belief::GetBoredomMultiplier(town, reaction);
	}
	[[nodiscard]] float AbodePercentBuilt(entt::entity abode) const override { return abodes::GetPercentBuilt(abode); }
	[[nodiscard]] float AbodeAttackDesire(entt::entity abode) const override
	{
		const auto* info = abodes::InfoOf(abode);
		return info != nullptr ? info->computerAttackDesire : 0.0f;
	}
	[[nodiscard]] float FieldAttackDesire(entt::entity field) const override
	{
		const auto* data = Has(field) ? Read().TryGet<const Field>(field) : nullptr;
		return data != nullptr ? fields::InfoOf(*data).computerAttackDesire : 0.0f;
	}

	// ---- buildings and fire

	[[nodiscard]] bool OnFire(entt::entity abode) const override
	{
		// the original asks only whether the building has a fire, not whether it burns
		return fire::Find(abode) != nullptr;
	}
	[[nodiscard]] bool FireAboveReactionTemperature(entt::entity abode) const override
	{
		const auto* burning = fire::Find(abode);
		return burning != nullptr && burning->IsAboveReactionTemperature();
	}

	// ---- the citadel

	[[nodiscard]] std::optional<Citadel> MyCitadel() const override
	{
		const auto citadel = worship::citadel::Of(_me);
		if (citadel == entt::null)
		{
			return std::nullopt;
		}
		return Citadel {.place = object::MapCoordsOf(citadel), .influence = influence::CitadelRadius(citadel)};
	}
	[[nodiscard]] entt::entity MyCitadelHeart() const override { return worship::citadel::HeartOf(worship::citadel::Of(_me)); }
	[[nodiscard]] uint32_t HeartBreakingBlowTurn(entt::entity heart) const override
	{
		const auto* data = Has(heart) ? Read().TryGet<const CitadelHeart>(heart) : nullptr;
		return data != nullptr ? data->breakingBlowTurn : 0u;
	}

	// ---- miracles

	[[nodiscard]] bool IsMagicTypeEnabled(MagicType type) const override
	{
		return magic::players::IsMagicTypeEnabled(_me, type);
	}
	[[nodiscard]] uint32_t PerceivedDesire(MagicType type) const override
	{
		return static_cast<uint32_t>(magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).perceivedPlayerDesire[0]);
	}
	[[nodiscard]] float MagicImpressiveBalance(MagicType type) const override
	{
		return magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).cpImpressiveBalance;
	}
	[[nodiscard]] float MagicAggressiveAttackValue(MagicType type) const override
	{
		return magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).aggressiveAttackValue;
	}
	[[nodiscard]] entt::entity CitadelSpellIcon(MagicType type) const override
	{
		return worship::citadel::GetSpellIcon(worship::citadel::Of(_me), type);
	}
	[[nodiscard]] entt::entity IconSite(entt::entity icon) const override
	{
		if (const auto* site = Has(icon) ? Read().TryGet<const WorshipSpellIcon>(icon) : nullptr; site != nullptr)
		{
			return site->site;
		}
		if (const auto* town = Has(icon) ? Read().TryGet<const TownCentreSpellIcon>(icon) : nullptr; town != nullptr)
		{
			return WorshipSiteOf(town->town);
		}
		return entt::null;
	}
	[[nodiscard]] float IconChantStore(entt::entity icon) const override
	{
		const auto* data = Has(icon) ? Read().TryGet<const WorshipSpellIcon>(icon) : nullptr;
		return data != nullptr ? data->chantStore : 0.0f;
	}
	[[nodiscard]] float IconChantsToCreate(entt::entity icon) const override
	{
		// the base miracle's cost, whatever seed the icon holds
		return magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), worship::icon::MagicTypeOf(icon, -1));
	}
	[[nodiscard]] float SiteChantsAvailable(entt::entity site) const override
	{
		return worship::site::Available(Read().Get<const WorshipSite>(site));
	}
	[[nodiscard]] float SiteBattery(entt::entity site) const override { return Read().Get<const WorshipSite>(site).battery; }
	[[nodiscard]] float SiteTotalChantsAvailable(entt::entity site) const override
	{
		return worship::site::TotalChantsAvailable(Read().Get<const WorshipSite>(site));
	}
	[[nodiscard]] float SiteChantsVillagersCanSupply(entt::entity site) const override
	{
		return worship::site::Capacity(Read().Get<const WorshipSite>(site));
	}
	[[nodiscard]] bool SiteBuilt(entt::entity site) const override { return abodes::IsBuilt(site); }
	[[nodiscard]] bool SiteRepaired(entt::entity site) const override { return abodes::IsRepaired(site); }
	[[nodiscard]] uint32_t SiteWorshippers(entt::entity site) const override
	{
		return static_cast<uint32_t>(worship::site::DancerCount(site));
	}
	[[nodiscard]] CastSpell SpellCast(entt::entity spell) const override
	{
		// the place is the mind's own, kept as it cast the miracle
		const auto* data = Has(spell) ? Read().TryGet<const Spell>(spell) : nullptr;
		return data == nullptr ? CastSpell {} : CastSpell {.type = data->magicType, .radius = data->magnitude};
	}

	// ---- the mind's hand and the creature's needs

	[[nodiscard]] MapCoords HandPlace() const override
	{
		const auto* hand = Hand();
		return {.x = hand != nullptr ? hand->mapX : ComputerHand::k_StartingMapPlace,
		        .z = hand != nullptr ? hand->mapZ : ComputerHand::k_StartingMapPlace};
	}
	[[nodiscard]] float CreatureDesire(entt::entity creature, uint32_t desire) const override
	{
		const auto* mind = Has(creature) ? Read().TryGet<const CreatureMindState>(creature) : nullptr;
		if (mind == nullptr || !mind->desires.has_value() || desire >= creature_desires::k_DesireCount)
		{
			return 0.0f;
		}
		return mind->desires->desires.at(desire).value;
	}
	[[nodiscard]] float CreatureLowEnergyThreshold(entt::entity creature) const override
	{
		const auto* body = Has(creature) ? Read().TryGet<const Creature>(creature) : nullptr;
		if (body == nullptr || !Locator::infoConstants::has_value())
		{
			return 0.0f;
		}
		const auto& rows = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(body->species);
		return row < rows.size() ? rows.at(row).lowEnergyThreshold : 0.0f;
	}
	[[nodiscard]] uint32_t UnknownNormalActions(entt::entity creature) const override
	{
		// the ordinary actions a computer player may teach that the creature does not know, as its mind holds them from
		// its making: its mind file's, else what it learnt, then what a script taught it or took away, in order
		const auto* mind = Has(creature) ? Read().TryGet<const CreatureMindState>(creature) : nullptr;
		if (mind == nullptr || !Locator::infoConstants::has_value())
		{
			return 0;
		}
		const auto& entries = Locator::infoConstants::value().creatireActionKnownAboutEntry;
		std::vector<bool> known(entries.size(), false);
		std::shared_ptr<const creaturemind::MindFileData> file = mind->pendingFile;
		if (file == nullptr && !mind->resourceChecked)
		{
			const auto* body = Read().TryGet<const Creature>(creature);
			if (body != nullptr && body->mind != 0 && Locator::resources::has_value())
			{
				const auto& minds = Locator::resources::value().GetCreatureMinds();
				if (minds.Contains(body->mind))
				{
					if (const auto handle = minds.Handle(body->mind); handle && handle->Loaded())
					{
						file = std::make_shared<const creaturemind::MindFileData>(handle->data);
					}
				}
			}
		}
		if (file != nullptr)
		{
			for (const auto& action : file->known.at(0))
			{
				if (action.id < known.size())
				{
					known.at(action.id) = true;
				}
			}
		}
		else if (mind->learnt.has_value())
		{
			const auto& skills = mind->learnt->knowledge.skillsKnown;
			for (size_t index = 0; index < known.size() && index < skills.size(); ++index)
			{
				known.at(index) = skills[index];
			}
		}
		for (const auto& edit : mind->scriptKnows)
		{
			if (edit.kind == static_cast<uint32_t>(CreatureActionLearningType::Normal) && edit.action < known.size())
			{
				known.at(edit.action) = edit.knows;
			}
		}
		uint32_t count = 0;
		for (size_t index = 0; index < entries.size(); ++index)
		{
			if (entries[index].field0x54 != 0 && !known.at(index))
			{
				++count;
			}
		}
		return count;
	}
	[[nodiscard]] bool AnySpellOnCreature(entt::entity creature) const override
	{
		const auto* spells = Has(creature) ? Read().TryGet<const CreatureSpells>(creature) : nullptr;
		return spells != nullptr && std::ranges::any_of(spells->spells.slots, [](const auto& slot) {
			       return slot.phase != creature_spells::Phase::Off;
		       });
	}

	// ---- food and stores

	[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town) const override { return fields::TownFields(town); }
	[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town) const override
	{
		return fish_farms::TownFishFarms(town);
	}
	[[nodiscard]] float FieldFood(entt::entity field) const override
	{
		const auto* data = Has(field) ? Read().TryGet<const Field>(field) : nullptr;
		return data != nullptr ? data->food : 0.0f;
	}
	[[nodiscard]] float FieldFoodCapacity(entt::entity field) const override
	{
		const auto* data = Has(field) ? Read().TryGet<const Field>(field) : nullptr;
		return data != nullptr ? fields::InfoOf(*data).totalFoodInField : 0.0f;
	}
	entt::entity ResourceDropObject(entt::entity town, uint32_t resource) override
	{
		// the town's store, else its temporary pot, made from the town's place when it has none
		if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
		{
			return pit;
		}
		const auto from = object::MapCoordsOf(town);
		return town_stores::GetTemporaryResourceStorePotOrPos(town, from,
		                                                      resource != 0 ? ResourceType::Wood : ResourceType::Food)
		    .pot;
	}
	[[nodiscard]] uint32_t AbodeResource(entt::entity abode, uint32_t resource) const override
	{
		return object_resources::GetResource(abode, resource != 0 ? ResourceType::Wood : ResourceType::Food);
	}
	[[nodiscard]] entt::entity StorePile(entt::entity pit, uint32_t resource, uint32_t index) const override
	{
		const auto* store = Has(pit) ? Read().TryGet<const components::StoragePit>(pit) : nullptr;
		if (store == nullptr)
		{
			return entt::null;
		}
		entt::entity pile = entt::null;
		if (resource == 0)
		{
			pile = index == 0 ? store->foodPile : entt::null;
		}
		else if (index < store->woodPiles.size())
		{
			pile = store->woodPiles.at(index);
		}
		return IsAvailable(pile) ? pile : entt::null;
	}
	[[nodiscard]] uint32_t PileAmount(entt::entity pile) const override
	{
		const auto* pot = Has(pile) ? Read().TryGet<const Pot>(pile) : nullptr;
		return pot != nullptr ? pot->amount : 0u;
	}
	[[nodiscard]] float RawDesire(entt::entity town, uint32_t desire) const override
	{
		return town_desire::GetRawDesire(town, static_cast<TownDesireInfo>(desire));
	}

	// ---- villagers

	[[nodiscard]] VillagerFacts VillagerFactsOf(entt::entity villager) const override
	{
		VillagerFacts facts {};
		const auto* data = Has(villager) ? Read().TryGet<const Villager>(villager) : nullptr;
		if (data == nullptr)
		{
			return facts;
		}
		facts.available = villager::IsVillagerAvailable(villager);
		facts.child = villager::IsChild(villager);
		facts.reachable = villager::IsReachable(villager);
		facts.idle =
		    villager::state_info::ServedDesire(villager::state_info::StateInfo(villager::GetFinalState(villager))) == -1;
		facts.atWorshipSite = (data->flags & Villager::k_FlagAtWorshipSite) != 0;
		facts.disciple = (data->flags & Villager::k_FlagDisciple) != 0;
		facts.building = data->buildingSite != entt::null;
		facts.sexuallyActive = villager::IsSexuallyActive(villager);
		facts.sex = static_cast<uint8_t>(data->sex);
		facts.discipleType = data->discipleType;
		facts.availableForWorship = villager_worship::IsAvailableForWorshipSite(villager, false);
		return facts;
	}
	[[nodiscard]] std::vector<entt::entity> VillagersAt(const MapCoords& place) const override
	{
		std::vector<entt::entity> found;
		const auto cell = map_coords::Cell(place);
		for (auto villager = map_cells::FindType(cell, ObjectType::Villager); villager != entt::null;
		     villager = map_cells::FindType(cell, ObjectType::Villager, villager))
		{
			found.push_back(villager);
		}
		return found;
	}

	// ---- the towns' wants and people

	[[nodiscard]] float TownDesire(entt::entity town, uint32_t desire) const override
	{
		return desire < town_desire::k_Count ? town_desire::GetDesire(town, static_cast<TownDesireInfo>(desire)) : 0.0f;
	}
	[[nodiscard]] float DesiredFood(entt::entity town) const override
	{
		const auto* t = TownData(town);
		return t != nullptr ? Locator::infoConstants::value().town.foodWantedMultiplier * t->stats.foodForDinner : 0.0f;
	}
	[[nodiscard]] uint32_t FoodAvailable(entt::entity town) const override
	{
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return 0;
		}
		// the food carried cut to a whole number, then the store's food, else the temporary pot's
		auto food = static_cast<uint32_t>(static_cast<int64_t>(t->stats.foodCarried));
		if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
		{
			return food + StoragePitStore::GetResource(pit, ResourceType::Food);
		}
		if (const auto pot = t->temporaryPots.at(0); Has(pot))
		{
			food += object_resources::GetResource(pot, ResourceType::Food);
		}
		return food;
	}
	[[nodiscard]] uint32_t TownDisciples(entt::entity town, uint32_t kind) const override
	{
		const auto* t = TownData(town);
		return t != nullptr && kind < t->stats.disciples.size() ? t->stats.disciples.at(kind) : 0u;
	}
	[[nodiscard]] uint32_t TownAdults(entt::entity town) const override
	{
		const auto* t = TownData(town);
		return t != nullptr ? t->stats.adults : 0u;
	}
	[[nodiscard]] uint32_t TownChildren(entt::entity town) const override
	{
		const auto* t = TownData(town);
		return t != nullptr ? t->stats.children : 0u;
	}
	[[nodiscard]] int32_t TownAdultPlaces(entt::entity town) const override
	{
		return static_cast<int32_t>(town_stats::RoomOf(town).adultPlaces);
	}
	[[nodiscard]] float TotemWorship(entt::entity town) const override
	{
		if (!Locator::villageTotemSystem::has_value())
		{
			return 0.0f;
		}
		const auto totem = Locator::villageTotemSystem::value().TotemOfTownCentre(worship::town::TownCentreOf(town));
		return totem.has_value() ? worship::percentage::GetWorshipPercentage(town) : 0.0f;
	}
	[[nodiscard]] TownWorship TownWorshipFacts(entt::entity town) const override
	{
		TownWorship facts {};
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return facts;
		}
		facts.emergency = town_queries::IsInStateOfEmergency(*t);
		// the original asks whether the town's centre works
		const auto centre = worship::town::TownCentreOf(town);
		facts.siteFunctional = centre != entt::null && abode_queries::IsFunctional(centre);
		if (const auto* magic = Read().TryGet<const TownMagic>(town); magic != nullptr)
		{
			facts.worshipping = static_cast<uint32_t>(magic->worshipping);
			facts.onTheWay = static_cast<uint32_t>(magic->onWayToWorship);
		}
		return facts;
	}
	[[nodiscard]] std::vector<entt::entity> AbodeInhabitants(entt::entity abode) const override
	{
		return abode_villagers::VillagersOf(abode);
	}
	[[nodiscard]] std::vector<entt::entity> TownHomeless(entt::entity town) const override
	{
		return town_villagers::Homeless(town);
	}

	// ---- building sites, scaffolds and workshops

	[[nodiscard]] bool IsBuildingSite(entt::entity thing) const override
	{
		return Has(thing) && Read().AllOf<BuildingSite>(thing);
	}
	[[nodiscard]] std::vector<entt::entity> TownBuildingSites(entt::entity town) const override
	{
		return building_sites::SitesOf(town);
	}
	[[nodiscard]] BuildingSiteFacts BuildingSiteFactsOf(entt::entity site) const override
	{
		BuildingSiteFacts facts {};
		if (!IsBuildingSite(site))
		{
			return facts;
		}
		facts.available = building_sites::IsAvailable(site);
		facts.building = building_sites::GetBuilding(site);
		facts.woodNeeded = building_sites::GetWoodNeededToBuild(site);
		const auto root = building_sites::GetRootBuilding(site);
		facts.rootBuilt = root != entt::null && abodes::IsBuilt(root);
		facts.desireToBeRepaired = building_sites::GetDesireToBeRepaired(site);
		facts.wood = building_sites::GetResource(site, ResourceType::Wood);
		facts.maxBuilders = building_sites::GetMaxBuilders(site);
		facts.buildersNeeded = building_sites::GetBuildersNeeded(site);
		facts.desireForVillagers = building_sites::GetDesireForVillagers(site);
		return facts;
	}
	[[nodiscard]] bool IsWorshipSite(entt::entity thing) const override
	{
		return Has(thing) && Read().AllOf<WorshipSite>(thing);
	}
	[[nodiscard]] std::vector<entt::entity> SiteTowns(entt::entity site) const override
	{
		const auto* data = Has(site) ? Read().TryGet<const WorshipSite>(site) : nullptr;
		return data != nullptr ? data->towns : std::vector<entt::entity> {};
	}
	[[nodiscard]] bool IsScaffold(entt::entity thing) const override { return scaffolds::IsScaffold(thing); }
	[[nodiscard]] bool IsWorkshop(entt::entity abode) const override { return workshops::IsWorkshop(abode); }
	[[nodiscard]] std::vector<entt::entity> WorkshopScaffolds(entt::entity workshop) const override
	{
		const auto* data = Has(workshop) ? Read().TryGet<const Workshop>(workshop) : nullptr;
		return data != nullptr ? data->scaffolds : std::vector<entt::entity> {};
	}
	[[nodiscard]] ScaffoldFacts ScaffoldFactsOf(entt::entity scaffold) const override
	{
		// busy: in a god's hand (the player's or a computer player's), or in the physics (thrown, falling or carried)
		ScaffoldFacts facts {};
		const auto* data = Has(scaffold) ? Read().TryGet<const Scaffold>(scaffold) : nullptr;
		if (data == nullptr)
		{
			return facts;
		}
		facts.inSite = data->site != entt::null;
		bool inHand = Locator::handSystem::has_value() && Locator::handSystem::value().IsInMagicHand(scaffold);
		Read().Each<const ComputerHand>(
		    [scaffold, &inHand](const ComputerHand& hand) { inHand = inHand || hand.heldObject == scaffold; });
		facts.busy = inHand || Read().AllOf<InPhysics>(scaffold);
		facts.value = static_cast<int32_t>(scaffolds::GetValue(scaffold));
		return facts;
	}
	[[nodiscard]] bool AbodeFunctional(entt::entity abode) const override { return abode_queries::IsFunctional(abode); }
	[[nodiscard]] int32_t WorkshopSpace(entt::entity workshop) const override { return workshops::GetSpaceInStore(workshop); }
	[[nodiscard]] float WorkshopMostObjects(entt::entity workshop) const override
	{
		const auto* info = abodes::InfoOf(workshop);
		return info != nullptr ? info->maxNumMobileObjectsToProduce : 0.0f;
	}
	[[nodiscard]] uint32_t ScaffoldWoodValue() const override { return Locator::infoConstants::value().scaffold.woodValue; }
	bool TryToBuildAt(entt::entity town, const MapCoords& place, int32_t scaffoldValue) override
	{
		// the town's planning into a trial plan, which leaves nothing in the world
		ScaffoldPlan trial {};
		return scaffold_plans::ChoosePlanForScaffold(town, trial, place, static_cast<uint32_t>(scaffoldValue), Tribe::NONE,
		                                             entt::null, Scaffold::k_AnyAbode, false);
	}
	[[nodiscard]] Cells MapCells() const override
	{
		return {.x = static_cast<int32_t>(map_coords::k_MapCells), .z = static_cast<int32_t>(map_coords::k_MapCells)};
	}
	[[nodiscard]] std::vector<entt::entity> CellObjects(uint16_t x, uint16_t z) const override
	{
		return map_cells::ObjectsInCell(glm::ivec2 {x, z});
	}
	[[nodiscard]] ThrowFacts ThrowFactsOf(entt::entity thing) const override
	{
		ThrowFacts facts {};
		if (!Has(thing))
		{
			return facts;
		}
		const auto& registry = Read();
		// whether a player's hand may throw it, by its class: a fragment never; a pile while it holds something; a rock,
		// the bonfire or a dead or felled tree while it is at most 3.6 m across (a radius that is no number passes); else a
		// moving object (a pot, a scaffold, a ball, an orb, a chest, the whale, a crop or a creed...), a mobile static or a
		// living tree
		if (registry.AllOf<Fragment>(thing))
		{
			facts.throwable = false;
		}
		else if (object_resources::IsPileResource(thing))
		{
			facts.throwable = registry.Get<const Pot>(thing).amount != 0;
		}
		else if (object_physics::IsRockObject(thing) || object_physics::IsBonfire(thing) || registry.AllOf<DeadTree>(thing))
		{
			facts.throwable = !(object::Get2DRadius(thing) > 3.6f);
		}
		else
		{
			facts.throwable = registry.AnyOf<MobileObject, Pot, Scaffold, Ball, OneOffSpellSeed, Reward, Shark, MobileStatic,
			                                 LandscapeVortex, TeleportStone, Tree>(thing);
		}
		auto& dynamics = Locator::dynamicsSystem::value();
		facts.breaksBuildings = dynamics.PhysicallyDestroysAbodes(thing);
		facts.tree = IsTree(thing);
		// as the hand weighs it: the thing's own weight, with no lower bound
		facts.weight = dynamics.ObjectWeight(thing);
		facts.impressiveValue = object::ImpressiveValue(thing);
		facts.artefactModifier =
		    Locator::artefactSystem::has_value() ? Locator::artefactSystem::value().ImpressiveModifier(thing) : 1.0f;
		return facts;
	}
	[[nodiscard]] float PlayerSpeed() const override
	{
		const auto* hand = Hand();
		return hand != nullptr ? hand->playerSpeed : ComputerHand::k_StartingSpeed;
	}

	// ---- forests and trees

	[[nodiscard]] uint32_t ForestGrownTrees(uint32_t forest) const override
	{
		return static_cast<uint32_t>(GrownTreesByDistance(forest).size());
	}
	[[nodiscard]] entt::entity ForestCentreTree(uint32_t forest) const override { return ecs::ForestCentreTree(forest); }
	[[nodiscard]] bool IsTree(entt::entity thing) const override { return Has(thing) && Read().AllOf<Tree>(thing); }
	[[nodiscard]] float TreeWoodValue(entt::entity tree) const override { return ecs::TreeWoodValue(tree); }
	[[nodiscard]] uint32_t TreeKindWood(entt::entity tree) const override
	{
		const auto* data = Has(tree) ? Read().TryGet<const Tree>(tree) : nullptr;
		return data != nullptr ? Locator::infoConstants::value().tree.at(static_cast<size_t>(data->type)).woodValue : 0u;
	}
	[[nodiscard]] float ThingRadius(entt::entity thing) const override { return object::GetRadius(thing); }
	[[nodiscard]] MapCoords LeaderHandPlace() const override
	{
		// the leader's reported hand: where the hand was as this turn began. A turn of steps writes it as it begins; the
		// interface's turn pass, before the players' turns, copies in the place the last turn of steps left the hand at
		const auto* hand = Hand();
		// before the hand's first turn of steps the interface still reports nowhere
		if (hand == nullptr || !hand->lastStepTurn.has_value())
		{
			return {};
		}
		const bool steppedThisTurn = hand->lastStepTurn == std::optional<uint32_t>(game_clock::Turn());
		return map_coords::FromWorld(steppedThisTurn ? hand->turnStartPoint : hand->point);
	}
	[[nodiscard]] std::optional<std::array<uint8_t, 4>> CellCorners(int32_t x, int32_t z) const override
	{
		if (x < 0 || z < 0 || x / 16 >= 32 || z / 16 >= 32 || !Locator::terrainSystem::has_value())
		{
			return std::nullopt;
		}
		const auto& island = Locator::terrainSystem::value();
		const glm::u16vec2 cell {static_cast<uint16_t>(x), static_cast<uint16_t>(z)};
		if (!island.HasBlockAt(cell))
		{
			return std::nullopt;
		}
		const auto corners = island.GetCellCorners(cell);
		return std::array<uint8_t, 4> {static_cast<uint8_t>(corners[0]), static_cast<uint8_t>(corners[1]),
		                               static_cast<uint8_t>(corners[2]), static_cast<uint8_t>(corners[3])};
	}
	[[nodiscard]] glm::vec3 WorldPoint(const MapCoords& place) const override { return map_coords::ToWorld(place); }

	// ---- the citadel's worship sites

	[[nodiscard]] std::array<entt::entity, 6> CitadelSites() const override { return worship::citadel::WorshipSitesOf(_me); }
	[[nodiscard]] float SiteFoodDesire(entt::entity site) const override { return worship::site::CalculateDesireForFood(site); }
	[[nodiscard]] int32_t SiteFoodNeededFromTown(entt::entity site) const override
	{
		return worship::site::CalculateFoodNeededFromTown(site);
	}
	[[nodiscard]] uint32_t SiteIconCount(entt::entity site) const override
	{
		const auto* data = Has(site) ? Read().TryGet<const WorshipSite>(site) : nullptr;
		return data != nullptr ? static_cast<uint32_t>(data->icons.size()) : 0u;
	}
	[[nodiscard]] std::vector<entt::entity> SiteIcons(entt::entity site) const override
	{
		const auto* data = Has(site) ? Read().TryGet<const WorshipSite>(site) : nullptr;
		return data != nullptr ? data->icons : std::vector<entt::entity> {};
	}
	[[nodiscard]] bool IconCharging(entt::entity icon) const override
	{
		const auto* data = Has(icon) ? Read().TryGet<const WorshipSpellIcon>(icon) : nullptr;
		return data != nullptr && data->charging;
	}
	[[nodiscard]] MagicType IconMagicType(entt::entity icon) const override
	{
		const auto seed = Read().Get<const SpellIcon>(icon).seedType;
		return magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed), -1);
	}
	void BuildSite(entt::entity site, float amount) override
	{
		// the site's build, which builds its altar by as much while the altar is available
		abodes::BuildBy(site, amount);
	}
	[[nodiscard]] MapCoords SiteGatePlace(entt::entity site) const override
	{
		const auto gate = worship::site::GetSpecialPos(site, worship::site::Point::Arrive);
		return gate.has_value() ? map_coords::FromWorld(*gate) : MapCoords {};
	}

	// ---- the random generator the game keeps in step

	float GameFloatRand(float most) override { return game_random::GameFloatRand(most); }
	uint32_t GameRand(uint32_t range) override { return game_random::GameRand(range); }

	// ---- the rivals

	[[nodiscard]] entt::entity CreatureOf(PlayerNames player) const override
	{
		return Locator::playerSystem::has_value()
		           ? Locator::playerSystem::value().GetPrimaryCreature(player).value_or(entt::null)
		           : entt::null;
	}
	[[nodiscard]] std::vector<MagicType> TownSpellIconTypes(entt::entity town) const override
	{
		std::vector<MagicType> types;
		const auto* magic = Has(town) ? Read().TryGet<const TownMagic>(town) : nullptr;
		if (magic == nullptr)
		{
			return types;
		}
		const auto& info = Locator::infoConstants::value();
		for (const auto icon : magic->spellIcons)
		{
			if (const auto* data = Has(icon) ? Read().TryGet<const SpellIcon>(icon) : nullptr; data != nullptr)
			{
				types.push_back(magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(info, data->seedType), -1));
			}
		}
		return types;
	}
	[[nodiscard]] float BeliefNeededToConvert(entt::entity town) const override
	{
		// another's town: its belief in its owner; the player's own: the belief of the rival whose margin over the
		// player is strictly the largest
		const auto* t = TownData(town);
		if (t == nullptr)
		{
			return 0.0f;
		}
		if (t->owner != _me)
		{
			return town_belief::GetBeliefInPlayer(town, t->owner);
		}
		const float mine = town_belief::GetBeliefInPlayer(town, _me);
		float most = -9999.0f;
		float needed = 0.0f;
		for (uint32_t slot = 0; slot < 8; ++slot)
		{
			if (slot == static_cast<uint32_t>(_me))
			{
				continue;
			}
			const float theirs = town_belief::GetBeliefInPlayer(town, static_cast<PlayerNames>(slot));
			if (const float above = theirs - mine; most < above)
			{
				most = above;
				needed = theirs;
			}
		}
		return needed;
	}

private:
	[[nodiscard]] const Registry& Read() const { return std::as_const(_registry); }
	[[nodiscard]] bool Has(entt::entity thing) const { return thing != entt::null && _registry.Valid(thing); }
	[[nodiscard]] const Town* TownData(entt::entity town) const
	{
		return Has(town) ? Read().TryGet<const Town>(town) : nullptr;
	}
	[[nodiscard]] const ComputerHand* Hand() const
	{
		const auto player = magic::players::EntityOf(_me);
		return player != entt::null ? Read().TryGet<const ComputerHand>(player) : nullptr;
	}

	Registry& _registry;
	PlayerNames _me;
};

/// What the mind's look round its hand reads of the land: a thing it remembers is a tree as its forest, else the tree,
/// or a one-shot globe, while each is still there (docs/wiki/hand/computer-minds.md#what-the-mind-sees-round-its-hand)
class GameSeenWorld final: public computer_player::SeenWorld
{
public:
	explicit GameSeenWorld(const Registry& registry)
	    : _registry(registry)
	{
	}

	[[nodiscard]] std::optional<std::vector<entt::entity>> CellObjects(uint16_t x, uint16_t z) const override
	{
		if (x >= map_coords::k_MapCells || z >= map_coords::k_MapCells)
		{
			return std::nullopt;
		}
		return map_cells::ObjectsInCell(glm::ivec2 {x, z});
	}
	[[nodiscard]] std::optional<ComputerMindSeen> SeenAs(entt::entity thing) const override
	{
		if (!IsAvailable(thing))
		{
			return std::nullopt;
		}
		if (const auto* tree = _registry.TryGet<const Tree>(thing); tree != nullptr)
		{
			if (tree->forestId != 0 && IsInForest(tree->forestId))
			{
				return ComputerMindSeen {.forest = tree->forestId};
			}
			return ComputerMindSeen {.thing = thing};
		}
		if (_registry.AnyOf<MagicTree, OneOffSpellSeed>(thing))
		{
			return ComputerMindSeen {.thing = thing};
		}
		return std::nullopt;
	}
	[[nodiscard]] bool StillThere(const ComputerMindSeen& seen) const override
	{
		return seen.forest.has_value() ? IsInForest(*seen.forest) : IsAvailable(seen.thing);
	}

private:
	const Registry& _registry;
};
} // namespace

std::unique_ptr<computer_player::ScoreWorld> MakeScoreWorld(Registry& registry, PlayerNames player)
{
	return std::make_unique<GameScoreWorld>(registry, player);
}

std::unique_ptr<computer_player::SeenWorld> MakeSeenWorld(const Registry& registry)
{
	return std::make_unique<GameSeenWorld>(registry);
}

} // namespace openblack::ecs::systems::computer_world
