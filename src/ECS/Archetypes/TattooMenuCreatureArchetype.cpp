/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooMenuCreatureArchetype.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Creature/CreatureMarks.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/TattooMenuCreature.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

std::optional<entt::entity> TattooMenuCreatureArchetype::Create(const tattoo_menu::Shown& shown)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// a creature's body, which its skin is painted for, of no player: its tattoos are painted with the local player's
	// masks. It has no mesh and no place in the world, so nothing that draws, moves or picks the world's things finds it
	registry.Assign<Creature>(entity, Creature {
	                                      .owner = PlayerNames::NEUTRAL,
	                                      .leashable = false,
	                                      .species = shown.species,
	                                      .mind = 0,
	                                      .size = tattoo_menu::k_Size,
	                                  });
	registry.AssignState<CreatureMorph>(entity, CreatureMorph {.shownFatness = 0.5f, .drawn = shown.morph, .revision = 0});
	registry.AssignState<CreatureAnimation>(entity);
	registry.AssignState<CreatureEyes>(entity);
	registry.AssignState<CreatureHair>(entity);
	registry.AssignState<CreatureTattoos>(entity);
	// its skin's marks are the ones the profile's physique file keeps: its drops of blood, then its wounds
	auto& marks = registry.AssignState<CreatureMarks>(entity);
	for (const auto word : shown.firstMarks)
	{
		marks.marks.blood.push_back(creature_marks::FromFileWord(word));
	}
	for (const auto word : shown.secondMarks)
	{
		marks.marks.wounds.push_back(creature_marks::FromFileWord(word));
	}
	registry.AssignState<CreatureSkin>(entity);
	// it stands at the world's origin facing as its mesh does, at its species' drawn scale for the size of 1
	const float scale = CreatureArchetype::DrawnScale(shown.species, tattoo_menu::k_Size);
	registry.Assign<TattooMenuCreature>(entity, TattooMenuCreature {
	                                                .shown = shown,
	                                                .life = {},
	                                                .model = glm::scale(glm::mat4(1.0f), glm::vec3(scale)),
	                                            });
	return entity;
}
