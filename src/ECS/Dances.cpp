/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Dances.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <utility>

#include <DanceFile.h>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// A thing that is going: gone already, or marked to go
bool Going(const Registry& registry, entt::entity thing)
{
	return !registry.Valid(thing) || registry.AllOf<Unavailable>(thing);
}
} // namespace

entt::entity dances::Create(Registry& registry, const DanceSetup& setup, std::shared_ptr<const dance::DanceFile> file)
{
	// It comes after every dance there is
	uint32_t made = 0;
	registry.Each<const Dance>([&made](entt::entity, const Dance& other) { made = std::max(made, other.made + 1); });
	const auto entity = registry.Create();
	auto& dance = registry.Assign<Dance>(entity);
	dance.made = made;
	dance.type = setup.type;
	dance.place = setup.place;
	dance.owner = setup.owner;
	dance.duration = setup.duration;
	dance.autostart = setup.autostart;
	dance.madeByScript = setup.madeByScript;
	dance.angle = setup.angle;
	// Made at a quarter speed, before its file sets its groups going
	dance_rules::SetSpeed(dance, dance_rules::k_MadeSpeed);
	// A dance always has one group, and as many as its file names. Without a file it keeps that one group, no key
	// frames and its clock at 0
	dance.groups.all.resize(1);
	if (file != nullptr)
	{
		dance.groups.all.resize(std::max<std::size_t>(1, file->groupCount));
		for (std::size_t i = 0; i < file->groupNames.size() && i < dance.groups.all.size(); ++i)
		{
			dance.groups.all[i].name = file->groupNames[i];
		}
		dance.clock = file->beat;
		dance.loopLength = file->loops;
		dance_rules::ApplyKeyFramesUpTo(dance, *file, dance.clock);
	}
	dance.file = std::move(file);
	return entity;
}

bool dances::IsDance(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Dance>(thing);
}

entt::entity dances::DanceOf(const Registry& registry, entt::entity living)
{
	const auto* dancer = registry.Valid(living) ? registry.TryGet<const Dancer>(living) : nullptr;
	return dancer != nullptr && IsDance(registry, dancer->dance) ? dancer->dance : entt::null;
}

uint32_t dances::Size(const Registry& registry, entt::entity dance)
{
	return IsDance(registry, dance) ? registry.Get<const Dance>(dance).dancers : 0;
}

bool dances::AddDancer(Registry& registry, entt::entity dance, entt::entity living, uint32_t sex, uint32_t danceType)
{
	if (!IsDance(registry, dance))
	{
		return false;
	}
	auto& data = registry.Get<Dance>(dance);
	const auto group = dance_rules::AddDancer(data, living, danceType, sex);
	if (!group.has_value())
	{
		return false;
	}
	registry.AssignOrReplace<Dancer>(living, Dancer {.dance = dance, .group = *group});
	return true;
}

void dances::RemoveDancer(Registry& registry, entt::entity living)
{
	const auto* dancer = registry.Valid(living) ? registry.TryGet<const Dancer>(living) : nullptr;
	if (dancer == nullptr)
	{
		return;
	}
	if (IsDance(registry, dancer->dance))
	{
		dance_rules::RemoveDancer(registry.Get<Dance>(dancer->dance), dancer->group, living);
	}
	registry.Remove<Dancer>(living);
}

entt::entity dances::FirstDancer(const Registry& registry, entt::entity dance, entt::entity exclude)
{
	if (!IsDance(registry, dance))
	{
		return entt::null;
	}
	// Only the groups its file names are looked in: none for a dance without a file
	const auto& data = registry.Get<const Dance>(dance);
	return dance_rules::FirstDancer(data.groups, data.file != nullptr ? data.file->groupCount : 0, exclude);
}

std::vector<entt::entity> dances::Dancers(const Registry& registry, entt::entity dance)
{
	std::vector<entt::entity> dancers;
	if (IsDance(registry, dance))
	{
		for (const auto& group : registry.Get<const Dance>(dance).groups.all)
		{
			dancers.insert(dancers.end(), group.dancers.begin(), group.dancers.end());
		}
	}
	return dancers;
}

void dances::Destroy(Registry& registry, entt::entity dance, const std::function<void(entt::entity)>& finished)
{
	if (!IsDance(registry, dance))
	{
		return;
	}
	for (const auto dancer : Dancers(registry, dance))
	{
		// A dancer that is going itself is passed over: it neither leaves the dance nor is told
		if (Going(registry, dancer))
		{
			continue;
		}
		RemoveDancer(registry, dancer);
		if (finished && registry.Valid(dancer))
		{
			finished(dancer);
		}
	}
	// TODO(opening): what it was danced about is told the dance has gone
	registry.Destroy(dance);
}

std::vector<entt::entity> dances::InTurnOrder(const Registry& registry)
{
	std::vector<std::pair<uint32_t, entt::entity>> all;
	registry.Each<const Dance>([&all](entt::entity entity, const Dance& dance) { all.emplace_back(dance.made, entity); });
	std::ranges::sort(all, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> order;
	order.reserve(all.size());
	std::ranges::transform(all, std::back_inserter(order), [](const auto& made) { return made.second; });
	return order;
}

void dances::ProcessTurn(Registry& registry, entt::entity entity, const TurnContext& context)
{
	auto& dance = registry.Get<Dance>(entity);
	if (dance.owner != entt::null && !context.available(dance.owner))
	{
		// A dance that is going itself waits, and does nothing this turn
		if (Going(registry, entity))
		{
			return;
		}
		// TODO(dance): a creature it was danced about forgets the dance
		dance.owner = entt::null;
		Destroy(registry, entity, context.finished);
		return;
	}
	// TODO(opening): a dance that follows what it is danced about moves with it; the scripts' dances don't
	std::function<void(std::size_t)> startsMove;
	if (context.playClipAgain)
	{
		startsMove = [&dance, &context](std::size_t index) {
			// Copied, as a clip played again may look at the dance
			const auto dancers = dance.groups.all.at(index).dancers;
			for (const auto dancer : dancers)
			{
				context.playClipAgain(dancer);
			}
		};
	}
	dance_rules::ProcessTurn(dance, context.turn, startsMove);
}

std::optional<dances::DancerPlace> dances::PlaceOf(const Registry& registry, entt::entity living)
{
	const auto dance = DanceOf(registry, living);
	if (dance == entt::null)
	{
		return std::nullopt;
	}
	const auto& dancer = registry.Get<const Dancer>(living);
	const auto& groups = registry.Get<const Dance>(dance).groups.all;
	if (dancer.group >= groups.size())
	{
		return std::nullopt;
	}
	const auto& dancers = groups[dancer.group].dancers;
	const auto found = std::ranges::find(dancers, living);
	if (found == dancers.end())
	{
		return std::nullopt;
	}
	return DancerPlace {.dance = dance, .group = dancer.group, .slot = static_cast<std::size_t>(found - dancers.begin())};
}

namespace
{
/// A dancer's place in its group's shape, taken through metres as the walks keep their goals
map_coords::MapCoords WalkedPlace(const Dance& dance, const dances::DancerPlace& placed)
{
	const auto& shapes = Locator::danceSystem::has_value() ? Locator::danceSystem::value().Shapes() : dance_moves::Shapes();
	const auto slot = dance_moves::SlotPosition(dance, placed.group, placed.slot, shapes);
	auto place = map_coords::FromMetres(map_coords::ToMetres(slot));
	place.altitude = slot.altitude;
	return place;
}
} // namespace

std::optional<dances::DancerTurn> dances::DancerTurnOf(const Registry& registry, entt::entity living,
                                                       const map_coords::MapCoords& at, uint16_t facing,
                                                       std::optional<uint32_t> move)
{
	const auto placed = PlaceOf(registry, living);
	if (!placed.has_value())
	{
		return std::nullopt;
	}
	const auto& dance = registry.Get<const Dance>(placed->dance);
	const auto& group = dance.groups.all[placed->group];
	DancerTurn turn {.place = WalkedPlace(dance, *placed)};
	turn.distance = gutils::GetDistanceInMetres(at, turn.place);
	// The angle from the dancer to its group's centre is the map's angle between the two points, which the game hands on
	// as a float: a whole number below 2048, kept exactly, so the facings that turn from it are worked out as the game's
	const auto action = static_cast<dance_moves::Action>(move.value_or(group.move.action));
	turn.worships = action == dance_moves::Action::Worship;
	turn.facing = dance_moves::Facing({
	    .action = action,
	    .facing = facing,
	    .towardsCentre = gutils::GetAngleFromXZ(at, dance_moves::GroupCentre(dance, placed->group)),
	    .danceAngle = dance.angle,
	    .rotation = group.rotation,
	    .second = std::bit_cast<float>(group.move.second),
	    .distance = turn.distance,
	});
	// TODO(dance): looking at the first dancer of another group (2)
	return turn;
}

std::optional<map_coords::MapCoords> dances::MovedPlace(const Registry& registry, entt::entity living)
{
	const auto placed = PlaceOf(registry, living);
	if (!placed.has_value())
	{
		return std::nullopt;
	}
	const auto& dance = registry.Get<const Dance>(placed->dance);
	if (!dance.groups.all[placed->group].moved)
	{
		return std::nullopt;
	}
	return WalkedPlace(dance, *placed);
}

uint32_t dances::DanceSex(const Registry& registry, entt::entity living)
{
	const auto* villager = registry.TryGet<const Villager>(living);
	if (villager == nullptr)
	{
		return DanceGroup::k_AnySex;
	}
	return villager->sex == Villager::Sex::FEMALE ? DanceGroup::k_Women : DanceGroup::k_Men;
}
