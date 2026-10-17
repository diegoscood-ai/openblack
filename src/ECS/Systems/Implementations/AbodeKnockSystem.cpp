/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AbodeKnockSystem.h"

#include <cstdint>

#include <vector>

#include "Audio/Audio.h"
#include "ECS/AbodeKnock.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

bool AbodeKnockSystem::Tap(entt::entity abode, glm::vec3 handPoint, bool ownHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the building's town (none for a building without one) and the read-out started or held
	_knockedTown = ecs::abode_villagers::TownOf(abode);
	_remaining = ecs::abode_knock::Knock(_remaining);
	// For any abode, before the type test, each inhabitant reacts to the tap (VillagerEmergency.h). (openblack) a copy
	// of the list; the original walks it live, and the reaction does not change it
	const std::vector<entt::entity> inhabitants = registry.Get<Abode>(abode).inhabitants;
	for (const auto villager : inhabitants)
	{
		ecs::villager::SetStateWhenTappedOnAbode(villager);
	}
	// only an abode whose ABODE_TYPE has the living-quarters bit knocks; the houses A..F and the windmill have it, the
	// civic buildings (totem, storage pit, creche, workshop, wonder, graveyard, town centre, football pitch, spell
	// dispenser, field) do not.
	const auto type = ecs::abodes::TypeOf(abode);
	if (!type.has_value() || !ecs::abode_knock::KnocksBack(*type))
	{
		return true;
	}
	// the local player's hand knocks on the roof (the Ctap_house animation) at the hand's own position, before the
	// sound
	if (ownHand && Locator::handSystem::has_value())
	{
		auto& hands = Locator::handSystem::value();
		const auto hand = hands.GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
		hands.StartFixedPosAnimation("Ctap_house", registry.Get<const Transform>(hand).position);
	}
	// the knock sound: bank InGame, sample 110 G_KnockRoofMulti plus a counter (0..8 in turn), owned by the abode, 3D,
	// not tracked, at the hand's point; mode and loops stay the defaults (3 and 0), and the .sad gives the sample 5 %
	// pitch spread and min / max 100 / 150.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 110 + audio::NextCounter(audio::Counter::KnockRoof)};
	options.owner = audio::Owner::Thing(abode);
	options.is3D = true;
	options.track = false;
	options.position = handPoint;
	audio::PlaySoundEffect(options);
	return true;
}

void AbodeKnockSystem::Update(std::chrono::milliseconds frame, const glm::vec3& cameraRight)
{
	// nothing changes while the read-out isn't showing: the steps, the scale and the camera's right stay as they were
	if (_remaining == 0)
	{
		return;
	}
	_remaining = ecs::abode_knock::Tick(_remaining, static_cast<uint32_t>(frame.count()));
	_steps = ecs::abode_knock::ReadoutSteps(_remaining);
	_scale = ecs::abode_knock::StepsScale(_steps);
	_right = cameraRight;
}

std::vector<KnockReadoutPerson> AbodeKnockSystem::PeopleOver(entt::entity abode) const
{
	std::vector<KnockReadoutPerson> people;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Abode>(abode);
	const auto* transform = registry.TryGet<const Transform>(abode);
	const auto type = ecs::abodes::TypeOf(abode);
	if (component == nullptr || transform == nullptr || (type.has_value() && !ecs::abode_knock::DrawsReadout(*type)))
	{
		return people;
	}
	const bool knockedTown = ecs::abode_villagers::TownOf(abode) == _knockedTown;
	const bool held = component->heldVillagersHome;
	if (!ecs::abode_knock::ShowsRow(_steps, knockedTown, held))
	{
		return people;
	}
	// a held villager's home shows its one teal person instead of its row
	const auto places = held ? 1 : static_cast<int32_t>(ecs::abode_villagers::MaxVillagers(abode));
	const auto adults = held ? ecs::abode_knock::k_HeldVillagersHome : static_cast<int32_t>(component->adultCount);
	// a row shows nothing once the read-out has run down, but the teal person does
	if (!ecs::abode_knock::RowDrawn(_remaining, adults))
	{
		return people;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const float halfHeight = mesh != nullptr ? ecs::object::MeshHalfHeight(mesh->id) : 0.0f;
	const auto start = ecs::abode_knock::RowStart(ecs::abode_knock::RowPoint(transform->position, halfHeight), _right, places);
	for (const auto& marker : ecs::abode_knock::Readout(places, adults, _scale))
	{
		people.push_back({.position = ecs::abode_knock::MarkerPoint(start, _right, marker.along),
		                  .size = marker.size,
		                  .colour = marker.colour});
	}
	return people;
}
