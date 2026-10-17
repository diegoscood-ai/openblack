/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature as a miracle's caster on the miracles' core: letting go of what it holds stops only a miracle that lasts
// while held and closes its beams down, which it keeps, a cast at nothing, of nothing or too tired to pay for comes to
// nothing, its body's energy test is at the size it is drawn at, and each turn a creature's miracle takes its hands and
// the way from them to what it was cast at, unless it is cast from above, where it keeps the point it flows from and
// the way is straight down from there. A cast from above flows from ten above what it is cast at, or 1.3 times its
// height when that is more; its two beams are each aimed at that point as they are made. Each turn the beams of a cast
// from above go back to the hand's mirror and the hand of the body as posed, closing down or not, and what it held that
// has gone is let go of. The test holds the registry, the tables, the miracles' service whose stores the spells' code
// reaches and a fake of the particles; the locator only injects them.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The particles as letting go reaches them: the spot visuals it closes down, in order
class ClosingParticles final: public test::InertParticleSystem
{
public:
	void CloseDownSpotVisual(entt::entity object) override { closed.push_back(object); }

	std::vector<entt::entity> closed;
};

class MagicCreatureCastingTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_magic = &static_cast<ecs::systems::MagicSystem&>(Locator::magicSystem::emplace<ecs::systems::MagicSystem>());
		_particles = &static_cast<ClosingParticles&>(Locator::particleSystem::emplace<ClosingParticles>());
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		// a miracle that costs a tenth of the energy of a creature of size 1 and strength one half
		auto info = std::make_unique<InfoConstants>();
		auto& species = info->creature.at(creature::InfoRow(CreatureType::GiantApe));
		species.chantsPerEnergy = 36000.0f;
		species.spellEnergyFloor = 0.005f;
		species.spellSizeFactor = 1.0f;
		info->magicEffect.at(static_cast<size_t>(MagicType::LightningBolt)).costToCreate = 9000.0f;
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	/// A creature of size 1 and strength 0.5, full of energy, tired by so much
	entt::entity Caster(float exhaustion = 0.0f)
	{
		const auto creature = _registry->Create();
		auto& body = _registry->Assign<ecs::components::Creature>(creature);
		body.species = CreatureType::GiantApe;
		body.size = 1.0f;
		body.strength = 0.5f;
		_registry->Assign<Transform>(creature, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& needs = _registry->Assign<CreatureNeeds>(creature).needs;
		needs.energy = 1.0f;
		needs.exhaustion = exhaustion;
		return creature;
	}

	/// A plain fireball on the list, lasting so many seconds (below 0 while it is held)
	entt::entity Fireball(float duration)
	{
		const auto spell = _registry->Create();
		auto& component = _registry->Assign<Spell>(spell);
		component.magicType = MagicType::Fireball;
		component.spellClass = SpellClass::General;
		component.creator = SpellCreator {.kind = SpellCreator::Kind::Thing};
		component.duration = duration;
		_magic->SpellStore().Spells().push_back(spell);
		return spell;
	}

	/// Something standing on the land, as a spot visual's object stands
	entt::entity Thing()
	{
		const auto thing = _registry->Create();
		_registry->Assign<Transform>(thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	}

	[[nodiscard]] ecs::Registry& Registry() const { return *_registry; }
	[[nodiscard]] ecs::systems::MagicSystem& Magic() const { return *_magic; }
	[[nodiscard]] InfoConstants& Info() const { return *_info; }
	[[nodiscard]] ClosingParticles& Particles() const { return *_particles; }

private:
	test::RestoreService<Locator::entitiesRegistry> _keepRegistry;
	test::RestoreService<Locator::infoConstants> _keepInfo;
	test::RestoreService<Locator::magicSystem> _keepMagic;
	test::RestoreService<Locator::animalSystem> _keepAnimals;
	test::RestoreService<Locator::particleSystem> _keepParticles;
	ecs::Registry* _registry {nullptr};
	ecs::systems::MagicSystem* _magic {nullptr};
	InfoConstants* _info {nullptr};
	ClosingParticles* _particles {nullptr};
};
} // namespace

TEST_F(MagicCreatureCastingTest, ACreatureThatHoldsNothingLetsGoOfNothing)
{
	const auto creature = Caster();
	Magic().ReleaseCreatureCast(creature);
	EXPECT_FALSE(Registry().AllOf<CreatureCasting>(creature));
	// nor one that has gone
	const auto gone = Registry().Create();
	Registry().Destroy(gone);
	Magic().ReleaseCreatureCast(gone);
}

TEST_F(MagicCreatureCastingTest, LettingGoStopsOnlyAMiracleThatLastsWhileHeldAndClosesItsBeams)
{
	const auto held = Caster();
	const auto timed = Caster();
	const auto whileHeld = Fireball(-1.0f);
	const auto forAWhile = Fireball(5.0f);
	const std::array<entt::entity, 2> beams {Thing(), Thing()};
	Registry().Assign<CreatureCasting>(held, CreatureCasting {.held = whileHeld, .beams = beams});
	Registry().Assign<CreatureCasting>(timed, CreatureCasting {.held = forAWhile});

	Magic().ReleaseCreatureCast(held);
	Magic().ReleaseCreatureCast(timed);
	EXPECT_TRUE(Registry().Get<Spell>(whileHeld).closedDown);
	EXPECT_FALSE(Registry().Get<Spell>(forAWhile).closedDown);
	for (const auto creature : {held, timed})
	{
		EXPECT_EQ(Registry().Get<CreatureCasting>(creature).held, entt::entity {entt::null});
	}
	// the beams close down, the one at the hand's mirror first, and are kept: their objects go only with their effects
	EXPECT_EQ(Particles().closed, (std::vector<entt::entity> {beams[0], beams[1]}));
	for (size_t i = 0; i < beams.size(); ++i)
	{
		EXPECT_TRUE(Registry().Valid(beams.at(i)));
		EXPECT_EQ(Registry().Get<CreatureCasting>(held).beams.at(i), beams.at(i));
	}
}

TEST_F(MagicCreatureCastingTest, ACastAtNothingOfNothingOrTooTiredComesToNothing)
{
	const auto creature = Caster();
	const auto target = Thing();
	const auto gone = Registry().Create();
	Registry().Destroy(gone);
	EXPECT_FALSE(Magic().CastByCreature(creature, MagicType::LightningBolt, gone));
	EXPECT_FALSE(Magic().CastByCreature(creature, MagicType::None, target));
	EXPECT_FALSE(Magic().CastByCreature(target, MagicType::LightningBolt, target));
	// 0.85 less the 0.1 the miracle would cost is not above 0.75
	const auto tired = Caster(0.75f);
	EXPECT_FALSE(Magic().CastByCreature(tired, MagicType::LightningBolt, target));
	EXPECT_TRUE(magic::Spells().empty());
	EXPECT_FALSE(Registry().AllOf<CreatureCasting>(tired));
}

TEST_F(MagicCreatureCastingTest, TheBodysEnergyIsTestedAtTheSizeItIsDrawnAt)
{
	const auto creature = Caster(0.75f);
	EXPECT_FALSE(magic::creator::CreatureCanPayFor(creature, 9000.0f));
	EXPECT_TRUE(magic::creator::CreatureCanPayFor(Caster(0.74f), 9000.0f));
	// drawn at size 3, the same miracle tires it by 9000 / ((3 + 0.5 + 1) x 36000)
	Registry().Assign<CreatureDrawPose>(creature).size = 3.0f;
	EXPECT_TRUE(magic::creator::CreatureCanPayFor(creature, 9000.0f));
	// a creature with no body can't
	Registry().Remove<CreatureNeeds>(creature);
	EXPECT_FALSE(magic::creator::CreatureCanPayFor(creature, 9000.0f));
}

TEST_F(MagicCreatureCastingTest, ACreaturesMiracleTakesTheWayFromItsHandsOrFromAboveStraightDown)
{
	const auto creature = Caster();
	const auto creator = magic::creator::OfCreature(creature);
	EXPECT_EQ(creator.kind, SpellCreator::Kind::Creature);
	EXPECT_EQ(creator.entity, creature);
	// before its body is posed its hands stay where they were, and the way from them is made one long
	psys::ProcessInfo info {.interfacePos = glm::vec3(3.0f, 14.0f, 4.0f), .handPos = glm::vec3(0.0f, 14.0f, 0.0f)};
	magic::creator::UpdateSpellInfo(creator, entt::null, info);
	EXPECT_EQ(info.handPos, glm::vec3(0.0f, 14.0f, 0.0f));
	const float inverse = 1.0f / std::sqrt(((4.0f * 4.0f) + (0.0f * 0.0f)) + (3.0f * 3.0f));
	EXPECT_EQ(info.cameraForward, glm::vec3(3.0f * inverse, 0.0f, 4.0f * inverse));
	// a miracle cast from above keeps the point it flows from, and its way is from there down to the target
	const_cast<GMagicInfo&>(magic::GetMagicInfo(Info(), MagicType::LightningBolt)).isCreatureCastFromAbove = 1;
	const auto spell = Registry().Create();
	Registry().Assign<Spell>(spell).magicType = MagicType::LightningBolt;
	psys::ProcessInfo above {.interfacePos = glm::vec3(3.0f, 14.0f, 4.0f), .handPos = glm::vec3(3.0f, 30.0f, 4.0f)};
	magic::creator::UpdateSpellInfo(creator, spell, above);
	EXPECT_EQ(above.handPos, glm::vec3(3.0f, 30.0f, 4.0f));
	EXPECT_EQ(above.cameraForward, glm::vec3(0.0f, -1.0f, 0.0f));
	// every turn, wherever the target has gone
	above.interfacePos = glm::vec3(3.0f, 30.0f, 8.0f);
	magic::creator::UpdateSpellInfo(creator, spell, above);
	EXPECT_EQ(above.handPos, glm::vec3(3.0f, 30.0f, 4.0f));
	EXPECT_EQ(above.cameraForward, glm::vec3(0.0f, 0.0f, 1.0f));
}

TEST_F(MagicCreatureCastingTest, FromAboveItFlowsFromTenAboveOrFromOnePointThreeTimesTheHeight)
{
	using ecs::systems::MagicSystem;
	EXPECT_EQ(MagicSystem::CastFromAboveRaise(0.0f), 10.0f);
	EXPECT_EQ(MagicSystem::CastFromAboveRaise(7.0f), 10.0f);
	EXPECT_EQ(MagicSystem::CastFromAboveRaise(-5.0f), 10.0f);
	EXPECT_FLOAT_EQ(MagicSystem::CastFromAboveRaise(10.0f), 13.0f);
	EXPECT_FLOAT_EQ(MagicSystem::CastFromAboveRaise(20.0f), 26.0f);
	// a height that is no number raises it by ten
	EXPECT_EQ(MagicSystem::CastFromAboveRaise(std::numeric_limits<float>::quiet_NaN()), 10.0f);
}

namespace
{
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

glm::mat4 At(float x, float y, float z)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

/// A caster whose species' rig is in the cache: its hand is bone 2, the file's table pairing it with bone 3, on a body
/// of a root, an arm and the two hands
class MagicCreatureBeamsTest: public MagicCreatureCastingTest
{
protected:
	void SetUp() override
	{
		MagicCreatureCastingTest::SetUp();
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::resources::emplace<resources::Resources>();
		_folder.emplace(std::string("openblack_test_magic_creature_beams_") +
		                ::testing::UnitTest::GetInstance()->current_test_info()->name());
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		block.points.rightHand = 2;
		// a number before the bones' table, then the table
		block.tail = {0, 0, 1, 3, 2};
		test::creature_block::WriteSpec(_folder->Path(), true, block.specVersion, {{"move", {"Cstand"}}});
		Locator::resources::value().GetCreatureRigs().Load(
		    creature::GetRigId(CreatureType::GiantApe), resources::CreatureRigLoader::FromBufferTag {},
		    test::creature_block::Build(block), _folder->Path(), _folder->Path() / "CreatureMesh");
	}

	/// A caster posed with its hand and the hand's mirror at those places
	entt::entity PosedCaster(glm::vec3 hand, glm::vec3 mirror)
	{
		const auto creature = Caster();
		Registry().AssignState<CreatureAnimation>(creature).skeleton.parents = {k_NoParent, 0, 1, 0};
		Registry().AssignState<CreatureTurnPose>(creature).current = Body(hand, mirror);
		return creature;
	}

	static std::vector<glm::mat4> Body(glm::vec3 hand, glm::vec3 mirror)
	{
		return {At(0.0f, 0.0f, 0.0f), At(5.0f, 20.0f, 0.0f), At(hand.x, hand.y, hand.z), At(mirror.x, mirror.y, mirror.z)};
	}

	[[nodiscard]] glm::vec3 PositionOf(entt::entity thing) const { return Registry().Get<Transform>(thing).position; }

private:
	const test::ScopedDefaultFileSystem _fileSystem;
	test::RestoreService<Locator::resources> _keepResources;
	std::optional<test::creature_block::TempFolder> _folder;
};
} // namespace

TEST_F(MagicCreatureBeamsTest, EachTurnTheBeamsGoToTheMirrorAndTheHandAsPosed)
{
	const auto creature = PosedCaster(glm::vec3(8.0f, 20.0f, 1.0f), glm::vec3(-8.0f, 20.0f, 2.0f));
	const std::array<entt::entity, 2> beams {Thing(), Thing()};
	Registry().Assign<CreatureCasting>(creature, CreatureCasting {.beams = beams});

	Magic().KeepCreatureBeams();
	// the first at the hand's mirror, the second at the hand
	EXPECT_EQ(PositionOf(beams[0]), glm::vec3(-8.0f, 20.0f, 2.0f));
	EXPECT_EQ(PositionOf(beams[1]), glm::vec3(8.0f, 20.0f, 1.0f));

	// posed again, they follow the body the next turn
	Registry().Get<CreatureTurnPose>(creature).current = Body(glm::vec3(9.0f, 25.0f, 3.0f), glm::vec3(-9.0f, 26.0f, 4.0f));
	Magic().KeepCreatureBeams();
	EXPECT_EQ(PositionOf(beams[0]), glm::vec3(-9.0f, 26.0f, 4.0f));
	EXPECT_EQ(PositionOf(beams[1]), glm::vec3(9.0f, 25.0f, 3.0f));

	// while the hand holds the creature, the pose kept then
	Registry().Get<CreatureTurnPose>(creature).frozen = Body(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f));
	Magic().KeepCreatureBeams();
	EXPECT_EQ(PositionOf(beams[0]), glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(PositionOf(beams[1]), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(MagicCreatureBeamsTest, WhatHasGoneIsLetGoOfAndTheRestStillFollows)
{
	const auto creature = PosedCaster(glm::vec3(8.0f, 20.0f, 1.0f), glm::vec3(-8.0f, 20.0f, 2.0f));
	const auto whileHeld = Fireball(-1.0f);
	const std::array<entt::entity, 2> beams {Thing(), Thing()};
	Registry().Assign<CreatureCasting>(creature, CreatureCasting {.held = whileHeld, .beams = beams});

	// the mirror's beam gone, and the miracle marked to go
	Registry().Destroy(beams[0]);
	Registry().Assign<Unavailable>(whileHeld);
	Magic().KeepCreatureBeams();
	const auto& cast = Registry().Get<CreatureCasting>(creature);
	EXPECT_EQ(cast.held, entt::entity {entt::null});
	EXPECT_EQ(cast.beams[0], entt::entity {entt::null});
	EXPECT_EQ(cast.beams[1], beams[1]);
	EXPECT_EQ(PositionOf(beams[1]), glm::vec3(8.0f, 20.0f, 1.0f));

	// so letting go no longer stops the miracle that was going anyway
	Magic().ReleaseCreatureCast(creature);
	EXPECT_FALSE(Registry().Get<Spell>(whileHeld).closedDown);
}

TEST_F(MagicCreatureBeamsTest, BeamsLetGoOfStillFollowTheHandsUntilTheyGo)
{
	const auto creature = PosedCaster(glm::vec3(8.0f, 20.0f, 1.0f), glm::vec3(-8.0f, 20.0f, 2.0f));
	const std::array<entt::entity, 2> beams {Thing(), Thing()};
	Registry().Assign<CreatureCasting>(creature, CreatureCasting {.held = Fireball(-1.0f), .beams = beams});

	// let go of, the beams close down
	Magic().ReleaseCreatureCast(creature);
	EXPECT_EQ(Particles().closed, (std::vector<entt::entity> {beams[0], beams[1]}));

	// posed again, they still follow the body the next turn
	Registry().Get<CreatureTurnPose>(creature).current = Body(glm::vec3(9.0f, 25.0f, 3.0f), glm::vec3(-9.0f, 26.0f, 4.0f));
	Magic().KeepCreatureBeams();
	EXPECT_EQ(PositionOf(beams[0]), glm::vec3(-9.0f, 26.0f, 4.0f));
	EXPECT_EQ(PositionOf(beams[1]), glm::vec3(9.0f, 25.0f, 3.0f));
	const auto& cast = Registry().Get<CreatureCasting>(creature);
	EXPECT_EQ(cast.beams, beams);

	// once their effects are done their objects go, and the creature lets go of them
	Registry().Destroy(beams[0]);
	Registry().Destroy(beams[1]);
	Magic().KeepCreatureBeams();
	EXPECT_EQ(cast.beams[0], entt::entity {entt::null});
	EXPECT_EQ(cast.beams[1], entt::entity {entt::null});
}

TEST_F(MagicCreatureBeamsTest, BeforeItsBodyIsPosedTheBeamsStayWhereTheyAre)
{
	const auto creature = Caster();
	const std::array<entt::entity, 2> beams {Thing(), Thing()};
	Registry().Assign<CreatureCasting>(creature, CreatureCasting {.beams = beams});
	Magic().KeepCreatureBeams();
	for (const auto beam : beams)
	{
		EXPECT_EQ(PositionOf(beam), glm::vec3(120.0f, 0.0f, 100.0f));
	}
}

namespace
{
/// The particles as a cast's beams use them: each spot visual made is an object of its own, and what each is aimed at
/// is kept
class BeamParticles final: public test::InertParticleSystem
{
public:
	struct Started
	{
		entt::entity visual;
		SpotVisualType type;
		glm::vec3 position;
		entt::entity owner;
	};

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> /*turns*/, entt::entity owner,
	                             float /*magnitude*/) override
	{
		const auto visual = Locator::entitiesRegistry::value().Create();
		started.push_back({.visual = visual, .type = type, .position = position, .owner = owner});
		return visual;
	}
	void AddSpotVisualTarget(entt::entity visual, glm::vec3 position) override { aimed.emplace_back(visual, position); }

	std::vector<Started> started;
	std::vector<std::pair<entt::entity, glm::vec3>> aimed;
};
} // namespace

TEST_F(MagicCreatureBeamsTest, EachBeamIsAimedAtThePointTheMiracleFlowsFromAsItIsMade)
{
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& particles = static_cast<BeamParticles&>(Locator::particleSystem::emplace<BeamParticles>());
	const auto creature = PosedCaster(glm::vec3(8.0f, 20.0f, 1.0f), glm::vec3(-8.0f, 20.0f, 2.0f));
	const glm::vec3 aim {3.0f, 24.0f, 4.0f};

	const auto beams = Magic().StartCreatureBeams(creature, aim);
	// the first at the hand's mirror, the second at the hand, both the creature's magic beams
	ASSERT_EQ(particles.started.size(), 2u);
	EXPECT_EQ(beams[0], particles.started[0].visual);
	EXPECT_EQ(beams[1], particles.started[1].visual);
	EXPECT_EQ(particles.started[0].position, glm::vec3(-8.0f, 20.0f, 2.0f));
	EXPECT_EQ(particles.started[1].position, glm::vec3(8.0f, 20.0f, 1.0f));
	for (const auto& beam : particles.started)
	{
		EXPECT_EQ(beam.type, SpotVisualType::MagicBeam);
		EXPECT_EQ(beam.owner, creature);
	}
	// each aimed at the point, as it is made
	ASSERT_EQ(particles.aimed.size(), 2u);
	EXPECT_EQ(particles.aimed[0].first, beams[0]);
	EXPECT_EQ(particles.aimed[0].second, aim);
	EXPECT_EQ(particles.aimed[1].first, beams[1]);
	EXPECT_EQ(particles.aimed[1].second, aim);
}

TEST_F(MagicCreatureBeamsTest, NoBeamsWithoutTheParticlesOrABodyToPose)
{
	const test::RestoreService<Locator::particleSystem> keepParticles;
	const auto creature = PosedCaster(glm::vec3(8.0f, 20.0f, 1.0f), glm::vec3(-8.0f, 20.0f, 2.0f));
	Locator::particleSystem::reset();
	const std::array<entt::entity, 2> none {entt::null, entt::null};
	EXPECT_EQ(Magic().StartCreatureBeams(creature, glm::vec3(0.0f)), none);
	// a creature with no posed body makes none either
	auto& particles = static_cast<BeamParticles&>(Locator::particleSystem::emplace<BeamParticles>());
	EXPECT_EQ(Magic().StartCreatureBeams(Caster(), glm::vec3(0.0f)), none);
	EXPECT_TRUE(particles.started.empty());
	EXPECT_TRUE(particles.aimed.empty());
}
