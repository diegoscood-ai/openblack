/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The hand with a spell seed in it, for the tests that drive it through its own presses and releases: a real hand
// system over the test's registry, zeroed tables with the seed's row, the miracles' inert service (its world a
// FakeMagicWorld) and a sound tag service that records what the hand asks of it. Each is put back when the test ends.
// Include it after LOCATOR_IMPLEMENTATIONS.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "Audio/Game/BankTables.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Resources/Resources.h"
#include "support/MagicFakes.h"
#include "support/MapFakes.h"
#include "support/RecordingSoundTags.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

namespace openblack::test::hand_seed
{
using test::RecordingSoundTags;

/// The sample the hand loops while a seed waits for its gesture's release (LH_SAMPLE_G_HANDGESTURE_02)
constexpr int k_GestureLoop = 3;

class HandSeedHarness: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		EmplaceWorldSystems();
		Locator::resources::emplace<resources::Resources>();
		// no model is loaded: the seed's mesh has no box
		auto& boxes = static_cast<FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<FakeMeshBoxProvider>());
		boxes.meshBox = [](entt::id_type /*meshId*/) { return std::optional<AxisAlignedBoundingBox> {}; };
		Locator::magicSystem::emplace<InertMagicSystem>();
		_tags = &static_cast<RecordingSoundTags&>(Locator::soundTagSystem::emplace<RecordingSoundTags>());
		auto info = std::make_unique<InfoConstants>();
		_info = info.get();
		Locator::infoConstants::reset(info.release());
		// the hand is in its player's influence
		magic::gestures::HandStatus status;
		status.inInfluence = true;
		magic::gestures::SetHandStatus(std::move(status));
		_hand = std::make_unique<ecs::systems::HandSystem>();
	}
	void TearDown() override
	{
		_hand.reset();
		ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] ecs::systems::HandSystem& Hand() const { return *_hand; }
	[[nodiscard]] RecordingSoundTags& Tags() const { return *_tags; }
	[[nodiscard]] InfoConstants& Info() const { return *_info; }

	/// A ready seed of `type`, cast as `castType`, in the hand
	entt::entity HoldSeed(SpellSeedType type, SpellCastType castType)
	{
		_info->spellSeed.at(static_cast<size_t>(type)).castType = castType;
		const auto seed = Registry().Create();
		Registry().Assign<ecs::components::Transform>(seed, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& component = Registry().Assign<ecs::components::SpellSeed>(seed);
		component.seedType = type;
		component.creator.player = PlayerNames::PLAYER_ONE;
		component.ready = true;
		Hand().PlaceObjectInMagicHand(seed);
		return seed;
	}

private:
	// put back in reverse order
	RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	RestoreService<Locator::resources> _restoreResources;
	RestoreService<Locator::meshBoxProvider> _restoreMeshBoxes;
	RestoreService<Locator::magicSystem> _restoreMagic;
	RestoreService<Locator::soundTagSystem> _restoreTags;
	RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
	RecordingSoundTags* _tags {nullptr};
	std::unique_ptr<ecs::systems::HandSystem> _hand;
};
} // namespace openblack::test::hand_seed
