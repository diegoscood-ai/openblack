/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToBeDeleted.h"

#include <cstdio>
#include <cstdlib>

#include <utility>

#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/FootballPitch.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ParticleContainer.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishFarms.h"
#include "ECS/Flocks.h"
#include "ECS/Footpaths.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerCreatureDance.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ToBeDeletedSystemInterface.h"
#include "ECS/Town/TownDeletion.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Vortex.h"
#include "ECS/Weather/WeatherThing.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Worship/SpellSeedGraphic.h"
#include "Worship/TownCentreSpellIcon.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// The game's dead list; stops with a message when there is none (before the game or after it has gone)
systems::ToBeDeletedSystemInterface& DeadList()
{
	if (!Locator::toBeDeletedSystem::has_value())
	{
		std::fputs("ecs::ToBeDeleted: no dead list in the locator (Locator::toBeDeletedSystem)\n", stderr);
		std::abort();
	}
	return Locator::toBeDeletedSystem::value();
}

/// The object's fire ToBeDeleted(0), then unlinked (fire::ToBeDeleted unlinks it)
void DeleteFire(entt::entity entity)
{
	if (auto* effect = fire::Find(entity))
	{
		fire::ToBeDeleted(*effect);
	}
}

/// (openblack, a transition) Whether this thing's deletion already waits on the dead list while the deferral is off for
/// the rest: the classes whose readers have been made to ask whether it is still available. The original defers every
/// game thing; the classes are added here one at a time, and this goes once they all are
/// (docs/wiki/engine/loop.md, openblack's dead list). On so far: a town's artefact, whose readers (its town's turn,
/// relaxation's offer, its owner's symbol) ask whether its object is still there, so that while it waits its town still
/// offers it to relaxing villagers, as the original's; and an animated static (a gate, a plinth, a lever), whose readers
/// reach it through the map cells it leaves at once (the routes, the hand's apply) or write it with its unavailable mark
/// (the save); and a feature (a monument or a building of the landscape), which the creature's search, the renderer and
/// the physics leave out once unavailable and the save writes with its mark; and a reward chest, whose own turn, frame
/// and dust skip one that waits, the town's new miracle above it going at its mark; and a football, whose pitch lets go
/// of it at its mark and whose other readers (the creature's search, a town's playthings, the renderer and the physics)
/// leave an unavailable one out, the save writing it with its mark; and a mobile static (a rock, a toy, a bonfire),
/// which the creature's searches and looks leave out once unavailable and the save writes with its mark; and a dead
/// tree, the same, its deletion listeners told at its mark; and an influence ring, whose readers go through the game's
/// list of rings it leaves at its mark, the save writing it with its mark; and a script's highlight, which leaves the
/// highlights' list and the map cells at its mark, its effects closed, the save writing it with its mark and out of
/// the game's list of highlights; and a fragment of a building, which leaves the fragments' turn and the map cells at its
/// mark, its broken mesh freed, the save writing it with its mark and out of the game's list of fragments; and a
/// script's weather thing, which leaves the weather things' list at its mark, its storm marked to end, the save writing
/// it with its mark; and a big forest, whose forest loses it at its mark, the hand, the creature and the routes finding
/// it through the map cells it leaves then, the save writing it with its mark and out of the game's list of big
/// forests; and a fish farm, whose fishermen are sent off it at its mark, the farms' turn, searches and draw leaving an
/// unavailable one out, the save writing it with its mark and out of the game's list of farms; and a pot or pile, out
/// of the structure it was part of and its reactions gone at its mark, which the hands drop at their next turn, the
/// creatures' searches leave out, the town's temporary pot slots and the creature's meal read as the original's do
/// (with no availability test), the save writing it with its mark, out of the moving objects' list, in the food piles'
/// list; and a miracle, which leaves the miracles' list at its mark (its class's part, its seed, its effect and its
/// map seed gone), the miracles' turn, the hand's packets, the temple's map and the creature's release asking whether
/// it is still available; and a building (every abode kind, the fields, the civic buildings), a worship site and a
/// citadel's heart, whose own part runs before the mark (abodes::OnToBeDeleted: its town's lists and slots, its plans,
/// its people, its site, its piles and totems), the save writing it with its mark and out of the lists it leaves; and a
/// tree, out of its forest's lists and so the game's tree list at its mark (its deletion listeners told), the drawn
/// trees' sway and rustle and the fireflies' tree list leaving it out, the creature's searches asking whether it is
/// available, the save writing it with its mark and out of the game's list of trees; and a building site (out of its
/// town's lists and marked being deleted at its mark, its builders sent off, its scaffolds and pile gone) and a
/// scaffold (out of its site and its workshop at its mark), whose end of physics then goes on as the original's does
[[nodiscard]] bool DefersClassOf(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<TownArtefact, AnimatedStatic, Feature, Reward, Ball, MobileStatic, DeadTree, InfluenceRing,
	                      ScriptHighlight, Fragment, WeatherThing, BigForest, FishFarm, Pot, Spell, Abode, WorshipSite,
	                      CitadelHeart, Tree, BuildingSite, Scaffold>(entity);
}
} // namespace

void DeleteCreaturesOwnThings(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* particles = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	const auto deleteVisual = [particles](entt::entity& visual) {
		if (visual != entt::null && particles != nullptr)
		{
			particles->DeleteSpotVisual(visual);
		}
		visual = entt::null;
	};
	if (auto* casting = registry.TryGet<CreatureCasting>(creature); casting != nullptr)
	{
		if (const auto held = std::exchange(casting->held, entt::null); held != entt::null && registry.Valid(held))
		{
			magic::DeleteSpell(held);
		}
		for (auto& beam : casting->beams)
		{
			deleteVisual(beam);
		}
	}
	if (auto* swap = registry.TryGet<CreatureSwap>(creature); swap != nullptr)
	{
		deleteVisual(swap->beam);
	}
	// the glowing chain its gesture draws goes with its effect
	if (auto* casting = registry.TryGet<CreatureCasting>(creature); casting != nullptr)
	{
		if (const auto chain = std::exchange(casting->gestureChain, 0U); chain != 0 && particles != nullptr)
		{
			particles->Delete(chain);
		}
		casting->gestureChainClosing = false;
	}
	if (auto* leash = registry.TryGet<CreatureLeash>(creature); leash != nullptr)
	{
		deleteVisual(leash->targetMarker);
	}
	if (auto* dance = registry.TryGet<CreatureDance>(creature); dance != nullptr)
	{
		const auto own = std::exchange(dance->dance, entt::null);
		if (own != entt::null && registry.Valid(own) && Locator::danceSystem::has_value())
		{
			Locator::danceSystem::value().Destroy(own);
		}
		dance->lastPlaces.clear();
	}
}

void ToBeDeleted(entt::entity entity, bool now)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	// already unavailable, nothing (it is on the list)
	if (registry.AllOf<Unavailable>(entity))
	{
		return;
	}
	// The local player's creature is saved, its mind and its physique, as it is deleted
	if (registry.AllOf<Creature>(entity))
	{
		static_cast<void>(player_creature::SaveCreatureAsItIsDeleted(entity));
		// then the villagers it called are let go, and its leash comes off
		villager_creature_dance::ReleaseCreaturesDependents(entity);
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().TakeOff(entity);
		}
		// then its own things: its held miracle, its beams, its swap beam, its leash's mark and its dance
		DeleteCreaturesOwnThings(entity);
	}
	if (registry.AllOf<FishFarm>(entity))
	{
		// a fish farm, first: its fishermen sent off it, then out of its town's list and the game's list of farms (the
		// fish farms' turn and searches leave an unavailable one out)
		fish_farms::DeleteDependants(entity);
	}
	// The things whose deletion takes the reactions they started with it, as the game's deletion of each of their
	// classes does (the others' reactions stay until the reactions' turn finds them gone): a creature, an arena, the
	// fixed things of many map cells (a building of any kind, a feature, a big forest, a citadel's part, a fish farm, a
	// mobile static, a football pitch, a worship site, a spell icon, a totem), a ball, a particle container and a lion
	if (registry.AnyOf<Creature, Arena, Abode, Feature, AnimatedStatic, BigForest, CitadelPartBuild, CitadelHeart, FishFarm,
	                   MobileStatic, FootballPitch, WorshipSite, SpellIcon, VillageTotem, Ball, ParticleContainer>(entity) ||
	    (registry.AllOf<Animal>(entity) && registry.Get<const Animal>(entity).type == AnimalInfo::Lion))
	{
		effects::reactions::RemoveAllReactionsInitiatedByObject(entity);
	}
	if (registry.AllOf<Spell>(entity))
	{
		// a miracle: its own part, as when it ends by itself. Its class's part goes first, so a shield's walls are set
		// dying and its rings go, then its seed and its effect, and it leaves the miracles' list; then the common mark
		magic::SpellToBeDeleted(entity);
	}
	if (!now && registry.AllOf<DeadTree>(entity) && !registry.AllOf<Tree>(entity))
	{
		// a dead tree: the deletion listeners told, then it goes the way every waiting thing does, out of its cells and
		// onto the dead list
		NotifyTreeDeleted(entity, TreeDeletion::Removed);
	}
	else if (!now && registry.AllOf<Tree>(entity) && (DeadList().Deferred() || DefersClassOf(registry, entity)))
	{
		// a tree that waits: its deletion listeners told and out of its forest's lists (so out of the game's tree list),
		// then it goes the way every waiting thing does, out of its cells and onto the dead list
		MarkTreeDeleted(entity);
	}
	else if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		// a tree or a dead tree deleted at once: out of its forest, the deletion listeners told
		DeleteTree(entity);
		// then its fire, after it
		DeleteFire(entity);
		return;
	}
	if (registry.AllOf<Villager>(entity))
	{
		// a villager (ECS/Villager/VillagerDeath.h), when it is marked: its dependants (SET_DYING through the real exits, a
		// mother's orphans, out of its abode / town / the vagrants), then it stops reacting (its reaction, the mourning too)
		ecs::villager::ToBeDeletedOverride(entity);
	}
	if (registry.AnyOf<Animal, Creature>(entity) && dances::DanceOf(registry, entity) != entt::null)
	{
		// an animal or a creature that dances leaves its dance, as every living's deletion takes it out (a villager's
		// own part has)
		dances::RemoveDancer(registry, entity);
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// out of its town's list, its town cleared, then out of its flock; prey and hunter links stay (their readers ask
		// IsAvailable)
		animal_ai::Forget(entity);
	}
	if (registry.AllOf<ScriptHighlight>(entity))
	{
		// out of the highlights' list, its two effects closed
		script_highlight::OnToBeDeleted(entity);
	}
	if (auto* chest = registry.TryGet<Reward>(entity); chest != nullptr && chest->townSeed != entt::null)
	{
		// a reward chest: the town's new miracle above it goes first
		worship::seed_graphic::Delete(chest->townSeed);
		chest->townSeed = entt::null;
	}
	// a town centre's spell icon: out of its centre's slot and its town's list, and its seed graphic deleted
	worship::town_centre::OnToBeDeleted(entity);
	if (auto* orb = registry.TryGet<OneOffSpellSeed>(entity); orb != nullptr && orb->graphic != entt::null)
	{
		// a one-shot orb: the seed inside goes first, however the orb goes (eaten by a creature too)
		worship::seed_graphic::Delete(orb->graphic);
		orb->graphic = entt::null;
	}
	if (registry.AllOf<Pot>(entity))
	{
		// a pot or pile: out of the structure it is part of first (a store's pile empties its slot and takes its amount
		// off the store), then the reactions it started go with it at once
		resource_stores::RemoveFromStructure(entity);
		animal_ai::RemovePotReaction(entity);
	}
	if (registry.AllOf<LandscapeVortex>(entity))
	{
		// out of the vortexes' list, its particle systems deleted at once and its decal released
		vortex::OnDeleted(entity);
	}
	if (const auto* big = registry.TryGet<const BigForest>(entity);
	    big != nullptr && Locator::forestSystem::has_value() && ForestBigForest(big->forestId) == entity)
	{
		// a big forest: its forest loses it, the forest staying
		SetForestBigForest(big->forestId, entt::null);
	}
	if (registry.AllOf<InfluenceRing>(entity))
	{
		// an influence ring (a script's delete of one): out of the game's list of rings at once, then marked
		influence::UnlinkRing(entity);
	}
	if (registry.AllOf<WeatherThing>(entity))
	{
		// a script's weather thing: its storm is marked to end with it
		weather::weather_thing::OnToBeDeleted(entity);
	}
	if (registry.AllOf<Town>(entity))
	{
		// a town: out of its player, its lists let go and out of the game's list of towns (ECS/Town/TownDeletion.h)
		town_deletion::ToBeDeleted(entity);
	}
	if (registry.AllOf<Flock>(entity))
	{
		// a flock: out of its town's list, its members let go of it, its citadel heart's link cleared (ECS/Flocks.h)
		flocks::DeleteDependencies(entity);
		// its last member's leaving deletes the emptied flock, which finished it
		if (!ecs::IsAvailable(entity))
		{
			return;
		}
	}
	if (registry.AllOf<Dance>(entity))
	{
		// a dance: each of its dancers that is not itself going leaves it and is told it has finished dancing, and the
		// dance goes with them, as a dance that ends does (ECS/Dances.h)
		if (Locator::danceSystem::has_value())
		{
			Locator::danceSystem::value().Destroy(entity);
			return;
		}
	}
	// the buildings' part (ECS/Abodes.h): before the physics, the cells and the mark, which the original sets last, so
	// the searches it makes still see the building; others ignored
	abodes::OnToBeDeleted(entity);
	// a multi-map fixed object's footpath link goes with it, its footpaths kept (a building's rebuild plan has taken its
	// link first). A dance's or a forest's is left as it is, as the original leaves it
	if (map_cells::IsMultiCellStaticClass(entity))
	{
		footpaths::DeleteFootpathLinkOf(entity);
	}
	// (openblack, guard) the class part may have deleted this entity already: a building site's builders' exits
	// re-enter ToBeDeleted for it (BuildingSites.cpp step 4) and the inner call finished it (mark or Destroy, and its
	// DeleteFire), so this one stops here
	if (!ecs::IsAvailable(entity))
	{
		return;
	}
	if (now || !(DeadList().Deferred() || DefersClassOf(registry, entity)))
	{
		// `now` (deleted at once), or the deferral still off: out of the physics first, as
		// DynamicsSystem::RemoveObject puts a still valid entity back in its cells; then out of its cells
		Locator::dynamicsSystem::value().RemoveObject(entity);
		map_cells::RemoveMapObject(entity);
		registry.Destroy(entity);
		registry.SetDirty();
		// its fire after it, as when FireEffect's Process found the object gone (no EndOnFire on it)
		DeleteFire(entity);
		return;
	}
	// out of its cells
	map_cells::RemoveMapObject(entity);
	// marked unavailable, not yet passed, pushed at the head. The physics body stays: the original does not take it out
	// here; the physics drops the unavailable ones at the start of the next turn
	registry.Assign<Unavailable>(entity);
	DeadList().Push(entity);
	DeleteFire(entity);
}

bool IsAvailable(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && !registry.AllOf<Unavailable>(entity);
}

void ProcessDeadList(bool drain)
{
	DeadList().Process(drain);
}

void SetDeferredDeletion(bool deferred)
{
	DeadList().SetDeferred(deferred);
}

bool DeferredDeletion()
{
	return DeadList().Deferred();
}

} // namespace openblack::ecs
