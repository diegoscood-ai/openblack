/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalAnimations.h"

#include <vector>

#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::Animal;
using components::SkeletalAnimation;

namespace
{
/// the species' StandAnimation (vtable +0x854): an ANM_ index, -1 = none
int32_t StandClip(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Cow:
	case AnimalInfo::PuzzleCow:
		return 42; // A_COW_STAND
	case AnimalInfo::Sheep:
	case AnimalInfo::PuzzleSheep:
		return 142; // A_SHEEP_STAND
	case AnimalInfo::Pig:
	case AnimalInfo::PuzzlePig:
		return 126; // A_PIG_STAND
	case AnimalInfo::Horse:
	case AnimalInfo::PuzzleHorse:
		return 57; // A_HORSE_STAND
	case AnimalInfo::Lion:
	case AnimalInfo::PuzzleLion:
		return 106; // A_LION_STAND
	case AnimalInfo::Tiger:
		return 164; // A_TIGER_STAND
	case AnimalInfo::Leopard:
		return 80; // A_LEOPARD_STAND
	case AnimalInfo::Wolf:
	case AnimalInfo::PuzzleWolf:
	case AnimalInfo::SpellWolf:
		return 184; // A_WOLF_STAND
	case AnimalInfo::Tortoise:
	case AnimalInfo::PuzzleTortoise:
		return 171; // A_TORTOISE_STAND
	default:
		return -1;
	}
}
} // namespace

void UpdateAnimalAnimations()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<entt::entity, int32_t>> fresh;
	registry.Each<const Animal>([&registry, &fresh](entt::entity entity, const Animal& animal) {
		if (!registry.AllOf<SkeletalAnimation>(entity))
		{
			fresh.emplace_back(entity, StandClip(animal.type));
		}
	});
	for (const auto& [entity, clip] : fresh)
	{
		auto& animation = registry.Assign<SkeletalAnimation>(entity);
		if (clip >= 0)
		{
			animation.clip = ClipId(static_cast<uint32_t>(clip));
			animation.clipIndex = clip;
			animation.hasClip = true;
		}
	}
}

} // namespace openblack::ecs
