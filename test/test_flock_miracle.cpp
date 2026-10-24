/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flock miracles cast and run over fakes: the animals each turn makes, where they may be made, their size, age and
// owner, the cast trail of the local hand and the hand's pick-up rule. The numbers are the original's: the info file's
// flock and animal rows and the exe's constants (docs/wiki/magic/flocks.md#the-exit-loop).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCastData.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/FlockMiracle.h"
#include "Magic/Spells/SpellFlock.h"
#include "Particles/ParticleTypes.h"
#include "Particles/SpellLink.h"
#include "support/MagicFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using openblack::ecs::components::Animal;
using openblack::ecs::components::AnimalBrain;
using openblack::ecs::components::FlockSpell;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellClass;
using openblack::ecs::components::SpellCreator;
using openblack::ecs::components::Transform;

namespace
{
// The info file's rows (Scripts/info.dat): twelve birds and fourteen wolves, the alignment switch, the distance to
// travel and the hunting radius; the spell animals' rows: the birds' normal altitude, the wolf's grown-up age and hunger
constexpr uint32_t k_Birds = 12;
constexpr uint32_t k_Wolves = 14;
constexpr float k_AlignmentSwitch = -0.2f;
constexpr float k_DistanceToTravel = 800.0f;
constexpr float k_HuntingRadius = 45.0f;
constexpr float k_BirdAltitudeNormal = 45.0f;
constexpr uint32_t k_WolfGrownUpAge = 13;
constexpr uint32_t k_WolfHunger = 150;
// The exe's constants: a bird is 2.8 to 3.0 times its model, a wolf 1.5 to 2.0
constexpr float k_BirdScaleLow = 2.8f;
constexpr float k_BirdScaleHigh = 3.0f;
constexpr float k_WolfScaleLow = 1.5f;
constexpr float k_WolfScaleHigh = 2.0f;
// Twelve a second over 100 ms turns, an animal made while fewer are made than the running total: the twelve birds are
// all made in the tenth turn, the fourteen wolves in the eleventh
constexpr size_t k_TurnsForAllBirds = 10;
constexpr size_t k_TurnsForAllWolves = 11;
// The running total at the end of each turn, as the game's FPU sums it in floats (1.2000000477 a turn; the fifth turn
// ends on exactly 6), and so the animals made by then
constexpr std::array<uint32_t, 11> k_EmittedBits {0x3F99999A, 0x4019999A, 0x40666667, 0x4099999A, 0x40C00000, 0x40E66666,
                                                  0x41066666, 0x41199999, 0x412CCCCC, 0x413FFFFF, 0x41533332};
constexpr std::array<size_t, 10> k_BirdsByTurn {2, 3, 4, 5, 6, 8, 9, 10, 11, 12};
constexpr std::array<size_t, 11> k_WolvesByTurn {2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 14};

constexpr ecs::systems::ParticleSystemInterface::EffectId k_CastEffect = 77;

/// The particles as the flocks use them: what was started from a file, closed down, made a target or a spot visual
class FlockParticles final: public test::InertParticleSystem
{
public:
	using test::InertParticleSystem::Start;
	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool /*synced*/) override
	{
		started.push_back({.file = std::string(file), .origin = origin, .magnitude = magnitude});
		return k_CastEffect;
	}
	void CloseDown(EffectId id) override { closed.push_back(id); }
	void AddTarget(EffectId /*id*/, entt::entity target) override { targets.push_back(target); }
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> /*turns*/, entt::entity /*owner*/,
	                             float /*magnitude*/) override
	{
		spots.emplace_back(type, position);
		return entt::null;
	}

	struct Started
	{
		std::string file;
		glm::vec3 origin;
		float magnitude;
	};
	std::vector<Started> started;
	std::vector<EffectId> closed;
	std::vector<entt::entity> targets;
	std::vector<std::pair<SpotVisualType, glm::vec3>> spots;
};

/// No mesh is loaded: the animals have no footprint
class NoMeshBoxes final: public ecs::systems::MeshBoxProviderInterface
{
public:
	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type /*meshId*/) const override
	{
		return std::nullopt;
	}
};

/// The inert miracles with the game's flock miracles, which the flock spells' steps go through
class FlockMagic final: public test::InertMagicSystem
{
public:
	[[nodiscard]] magic::FlockMiracleInterface* Flocks() override { return &_flocks; }

private:
	magic::FlockMiracle _flocks;
};

/// A flock miracle cast by the first player over flat land, with the original's rows for the flocks and their animals
class FlockCasting: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the animals' and the miracles' log lines go nowhere
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::meshBoxProvider::emplace<NoMeshBoxes>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		_magic = &static_cast<FlockMagic&>(Locator::magicSystem::emplace<FlockMagic>());
		_particles = &static_cast<FlockParticles&>(Locator::particleSystem::emplace<FlockParticles>());
		auto info = std::make_unique<InfoConstants>();
		auto& flying = info->magicFlockFlying[0];
		flying.numberToCreate = k_Birds;
		flying.alignmentSwitch = k_AlignmentSwitch;
		flying.distanceToTravel = k_DistanceToTravel;
		auto& ground = info->magicFlockGround[0];
		ground.numberToCreate = k_Wolves;
		ground.alignmentSwitch = k_AlignmentSwitch;
		ground.distanceToTravel = k_DistanceToTravel;
		ground.huntingRadius = k_HuntingRadius;
		for (const auto type : {AnimalInfo::SpellDove, AnimalInfo::SpellBat, AnimalInfo::SpellWolf})
		{
			auto& row = info->animal.at(static_cast<size_t>(type));
			row.animalInfo = type;
			// a birth size far from the miracle's: the miracle sets its own
			row.ageToScale.values.fill(0.5f);
		}
		info->animal.at(static_cast<size_t>(AnimalInfo::SpellDove)).altitudeNormal = k_BirdAltitudeNormal;
		info->animal.at(static_cast<size_t>(AnimalInfo::SpellBat)).altitudeNormal = k_BirdAltitudeNormal;
		auto& wolf = info->animal.at(static_cast<size_t>(AnimalInfo::SpellWolf));
		wolf.grownUpAge = k_WolfGrownUpAge;
		wolf.hunger = k_WolfHunger;
		Locator::infoConstants::reset(info.release());
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] test::FakeMagicWorld& World() const { return _magic->FakeWorld(); }
	[[nodiscard]] FlockParticles& Particles() const { return *_particles; }

	/// The first player casts the flock with its hand 30 m above (x, z): a script's or computer's cast unless `human`
	[[nodiscard]] entt::entity Cast(MagicType type, float x, float z, bool human = false, bool myInterface = false)
	{
		const auto spell = magic::AllocSpell(
		    type, {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null});
		auto& component = Registry().Get<Spell>(spell);
		component.isHumanPlayerCasting = human;
		component.isMyInterfaceCasting = myInterface;
		magic::SpellCastData castData {.magnitude = 1.0f, .chants = 5000.0f, .duration = -1.0f, .maxObjectsToCreate = -1};
		particles::ProcessInfo info;
		info.handPos = glm::vec3(x, 30.0f, z);
		info.cameraForward = glm::vec3(0.0f, -0.5f, 1.0f);
		EXPECT_EQ(_flocks.Start(spell, glm::vec3(x, 0.0f, z), &castData, info), 1);
		return spell;
	}

	/// One game turn of the miracle
	void Turn(entt::entity spell) { static_cast<void>(_flocks.ProcessTurn(spell)); }

	[[nodiscard]] static int Created(entt::entity spell) { return Registry().Get<FlockSpell>(spell).created; }
	[[nodiscard]] static std::vector<entt::entity> Animals(entt::entity spell) { return magic::spell_objects::Objects(spell); }

private:
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	FlockMagic* _magic {nullptr};
	FlockParticles* _particles {nullptr};
	magic::FlockMiracle _flocks;
};
} // namespace

// Flying flock: casting

TEST(FlockEmit, TheTotalStepsInFloatsAsTheGamesFpuSumsIt)
{
	float emitted = 0.0f;
	for (const auto bits : k_EmittedBits)
	{
		emitted = magic::spell_flock::NextEmitted(emitted);
		EXPECT_EQ(std::bit_cast<uint32_t>(emitted), bits);
	}
}

TEST(FlockBirth, ABirthPointIsNudgedInFloatsAsTheGamesFpuRoundsIt)
{
	// a spawn coordinate (MapCoords) and the draw of GameFloatRand(0.2): the exe's steps, each rounded to a float
	struct Nudge
	{
		int32_t mapCoord;
		uint32_t drawBits;
		int32_t nudged;
	};
	constexpr std::array<Nudge, 6> k_Nudges {{{15214158, 0x3E054FB7, 15214357},
	                                          {5124560, 0x3D95C943, 5124384},
	                                          {9170155, 0x3BF5BB47, 9169549},
	                                          {16516814, 0x3DAB4887, 16516707},
	                                          {16210580, 0x3C41B17D, 16210002},
	                                          {9456736, 0x3E0126F6, 9456907}}};
	for (const auto& nudge : k_Nudges)
	{
		EXPECT_EQ(magic::spell_flock::Jittered(nudge.mapCoord, std::bit_cast<float>(nudge.drawBits)), nudge.nudged)
		    << nudge.mapCoord;
	}
}

TEST(FlockBirth, ABirthHeightIsEasedInFloatsAsTheGamesFpuRoundsIt)
{
	// the last two hand heights and the fraction between them: the exe's (to - from) x f + from, each step a float
	struct Ease
	{
		uint32_t from;
		uint32_t to;
		uint32_t fraction;
		uint32_t height;
	};
	constexpr std::array<Ease, 4> k_Eases {{{0x4190B456, 0x420BBAFD, 0x3F312236, 0x41EDF23A},
	                                        {0x41AC8ECA, 0x40D694A8, 0x3FBE0F5B, 0xBF004860},
	                                        {0x41877969, 0x426867D7, 0x3F4C407D, 0x42471E8E},
	                                        {0x421DDCD5, 0x41AC7397, 0x3F695DE2, 0x41B91E58}}};
	for (const auto& ease : k_Eases)
	{
		const float height = magic::spell_flock::EasedHeight(std::bit_cast<float>(ease.from), std::bit_cast<float>(ease.to),
		                                                     std::bit_cast<float>(ease.fraction));
		EXPECT_EQ(std::bit_cast<uint32_t>(height), ease.height);
	}
}

TEST_F(FlockCasting, AllTwelveBirdsAreMadeInTheTenthTurnAndNoMore)
{
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f);
	for (const auto made : k_BirdsByTurn)
	{
		Turn(spell);
		EXPECT_EQ(Animals(spell).size(), made);
	}
	EXPECT_EQ(Animals(spell).size(), k_Birds);
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
	// all made: no more
	Turn(spell);
	EXPECT_EQ(Animals(spell).size(), k_Birds);
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
	// every bird is a target of the miracle's effect, and a dove for a caster that is not evil
	EXPECT_EQ(Particles().targets.size(), k_Birds);
	for (const auto bird : Animals(spell))
	{
		EXPECT_EQ(Registry().Get<Animal>(bird).type, AnimalInfo::SpellDove);
	}
}

TEST_F(FlockCasting, BirdsOutsideAHumanCastersInfluenceAreSkippedButCounted)
{
	World().influence = false;
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f, true);
	for (size_t turn = 0; turn < k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
	}
	EXPECT_TRUE(Animals(spell).empty());
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
	// counted as made: none is tried again
	World().influence = true;
	Turn(spell);
	EXPECT_TRUE(Animals(spell).empty());
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
}

TEST_F(FlockCasting, OnlyAHumanCasterNeedsInfluenceForItsBirds)
{
	World().influence = false;
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f);
	for (size_t turn = 0; turn < k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
	}
	EXPECT_EQ(Animals(spell).size(), k_Birds);
}

TEST_F(FlockCasting, BirdsThatWouldStartOffTheMapAreSkippedButCounted)
{
	const auto spell = Cast(MagicType::FlockFlying, -30.0f, 1000.0f);
	for (size_t turn = 0; turn < k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
	}
	EXPECT_TRUE(Animals(spell).empty());
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
}

TEST_F(FlockCasting, BirdsKeepBeingMadeAfterTheMiracleIsOver)
{
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f);
	Turn(spell);
	ASSERT_FALSE(Animals(spell).empty());
	ASSERT_LT(Animals(spell).size(), k_Birds);
	magic::OpsOf(SpellClass::FlockFlying).closeDown(spell);
	ASSERT_TRUE(Registry().Get<Spell>(spell).closedDown);
	for (size_t turn = 1; turn < k_TurnsForAllBirds; ++turn)
	{
		const int before = Created(spell);
		Turn(spell);
		EXPECT_GT(Created(spell), before) << turn;
	}
	EXPECT_EQ(Created(spell), static_cast<int>(k_Birds));
	EXPECT_EQ(Animals(spell).size(), k_Birds);
}

TEST_F(FlockCasting, EveryBirdIsTwoPointEightToThreeTimesItsModelWhateverItsBirthSize)
{
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f);
	for (size_t turn = 0; turn < k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
	}
	ASSERT_EQ(Animals(spell).size(), k_Birds);
	for (const auto bird : Animals(spell))
	{
		const auto& scale = Registry().Get<Transform>(bird).scale;
		EXPECT_GE(scale.x, k_BirdScaleLow);
		EXPECT_LE(scale.x, k_BirdScaleHigh);
		EXPECT_EQ(scale.y, scale.x);
		EXPECT_EQ(scale.z, scale.x);
	}
}

TEST_F(FlockCasting, TheLocalCastersHandLeavesSparklesUntilEveryBirdIsMade)
{
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f, true, true);
	ASSERT_EQ(Particles().started.size(), 1u);
	EXPECT_EQ(Particles().started[0].file, particles::ParticleTypeFile(ParticleType::FlockFlyingCastGood));
	EXPECT_EQ(Particles().started[0].file, "SF_FlockFlyingCastGood");
	EXPECT_FLOAT_EQ(Particles().started[0].origin.x, 1000.0f);
	EXPECT_FLOAT_EQ(Particles().started[0].origin.z, 1000.0f);
	EXPECT_FLOAT_EQ(Particles().started[0].magnitude, 1.0f);
	for (size_t turn = 0; turn < k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
		// still being made: the trail goes on
		EXPECT_TRUE(Particles().closed.empty()) << turn;
	}
	ASSERT_EQ(Created(spell), static_cast<int>(k_Birds));
	// all made: the trail closes
	Turn(spell);
	ASSERT_FALSE(Particles().closed.empty());
	EXPECT_EQ(Particles().closed.front(), k_CastEffect);
}

TEST_F(FlockCasting, AnEvilCastersHandLeavesSmokeAndMakesBats)
{
	ecs::effects::alignment::SetClamped(PlayerNames::PLAYER_ONE, -0.5f);
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f, true, true);
	ASSERT_EQ(Particles().started.size(), 1u);
	EXPECT_EQ(Particles().started[0].file, "SF_FlockFlyingCastEvil");
	Turn(spell);
	ASSERT_FALSE(Animals(spell).empty());
	EXPECT_EQ(Registry().Get<Animal>(Animals(spell).front()).type, AnimalInfo::SpellBat);
	ecs::effects::alignment::SetClamped(PlayerNames::PLAYER_ONE, 0.0f);
}

TEST_F(FlockCasting, OnlyTheLocalCastersHandLeavesATrail)
{
	const auto spell = Cast(MagicType::FlockFlying, 1000.0f, 1000.0f, true, false);
	for (size_t turn = 0; turn <= k_TurnsForAllBirds; ++turn)
	{
		Turn(spell);
	}
	EXPECT_TRUE(Particles().started.empty());
	EXPECT_TRUE(Particles().closed.empty());
}

// Ground flock: casting

TEST_F(FlockCasting, AllFourteenWolvesAreMadeInTheEleventhTurnOnTheGround)
{
	const auto spell = Cast(MagicType::FlockGround, 1000.0f, 1000.0f);
	for (const auto made : k_WolvesByTurn)
	{
		Turn(spell);
		EXPECT_EQ(Animals(spell).size(), made);
	}
	EXPECT_EQ(Animals(spell).size(), k_Wolves);
	EXPECT_EQ(Created(spell), static_cast<int>(k_Wolves));
	Turn(spell);
	EXPECT_EQ(Animals(spell).size(), k_Wolves);
	// on the land (flat, at 0), though the hand is 30 m up; a puff of magic where each appears
	for (const auto wolf : Animals(spell))
	{
		EXPECT_EQ(Registry().Get<Animal>(wolf).type, AnimalInfo::SpellWolf);
		EXPECT_FLOAT_EQ(Registry().Get<Transform>(wolf).position.y, 0.0f);
	}
	EXPECT_EQ(Particles().spots.size(), k_Wolves);
}

TEST_F(FlockCasting, WolvesOffTheMapOrOutsideAHumanCastersInfluenceAreSkippedButCounted)
{
	const auto offTheMap = Cast(MagicType::FlockGround, -30.0f, 1000.0f);
	World().influence = false;
	const auto noInfluence = Cast(MagicType::FlockGround, 1000.0f, 1000.0f, true);
	for (size_t turn = 0; turn < k_TurnsForAllWolves; ++turn)
	{
		Turn(offTheMap);
		Turn(noInfluence);
	}
	EXPECT_TRUE(Animals(offTheMap).empty());
	EXPECT_EQ(Created(offTheMap), static_cast<int>(k_Wolves));
	EXPECT_TRUE(Animals(noInfluence).empty());
	EXPECT_EQ(Created(noInfluence), static_cast<int>(k_Wolves));
	EXPECT_TRUE(Particles().spots.empty());
}

TEST_F(FlockCasting, EveryWolfIsOnePointFiveToTwoTimesItsModelWhateverItsBirthSize)
{
	const auto spell = Cast(MagicType::FlockGround, 1000.0f, 1000.0f);
	for (size_t turn = 0; turn < k_TurnsForAllWolves; ++turn)
	{
		Turn(spell);
	}
	ASSERT_EQ(Animals(spell).size(), k_Wolves);
	for (const auto wolf : Animals(spell))
	{
		const auto& scale = Registry().Get<Transform>(wolf).scale;
		EXPECT_GE(scale.x, k_WolfScaleLow);
		EXPECT_LE(scale.x, k_WolfScaleHigh);
		EXPECT_EQ(scale.y, scale.x);
	}
}

TEST_F(FlockCasting, WolvesAreBornGrownUpHungryAndTheCastersAndCannotBePickedUp)
{
	const auto spell = Cast(MagicType::FlockGround, 1000.0f, 1000.0f);
	Turn(spell);
	ASSERT_FALSE(Animals(spell).empty());
	for (const auto wolf : Animals(spell))
	{
		const auto& animal = Registry().Get<Animal>(wolf);
		EXPECT_EQ(animal.age, k_WolfGrownUpAge + 1);
		EXPECT_EQ(animal.player, static_cast<int32_t>(PlayerNames::PLAYER_ONE));
		const auto* brain = Registry().TryGet<AnimalBrain>(wolf);
		ASSERT_NE(brain, nullptr);
		EXPECT_EQ(brain->hunger, static_cast<int16_t>(k_WolfHunger));
		// the species' pick-up flag, 0 in the info file
		EXPECT_FALSE(ecs::animal_ai::ValidForPlaceInHand(wolf));
		// its run is set up at once, turning towards it: the lean takes the spell wolf's second
		EXPECT_FLOAT_EQ(brain->bank.duration, 1.0f);
		EXPECT_LE(std::abs(brain->bank.destination), 0.5f);
	}
}

TEST(FlockWolves, TheSpellWolvesLeanByTheirSpeedAndTurnOverASecond)
{
	namespace detail = openblack::ecs::animal_ai::detail;
	// the exe's own answers: a second to lean, by |speed x turn| x 0.5 / 20000 radians and at most 0.5, the speed in
	// MapCoords a turn and the turn in game angle units
	EXPECT_EQ(detail::TimeToBank(AnimalInfo::SpellWolf), 1.0f);
	struct Lean
	{
		float speed;
		float turn;
		uint32_t bits;
	};
	constexpr std::array<Lean, 7> k_Leans {{{9830.0f, 50.0f, 0x3F000000},
	                                        {100.0f, 3.0f, 0x3BF5C28F},
	                                        {1000.0f, -7.0f, 0x3E333333},
	                                        {200.0f, 1.0f, 0x3BA3D70A},
	                                        {9830.0f, 0.0f, 0x00000000},
	                                        {4000.0f, 5.0f, 0x3F000000},
	                                        {333.0f, 7.0f, 0x3D6EB1C4}}};
	for (const auto& lean : k_Leans)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(detail::BankAngle(AnimalInfo::SpellWolf, lean.speed, lean.turn)), lean.bits)
		    << lean.speed << " x " << lean.turn;
	}
	// the other kinds, as the exe answers: the birds 0.5 whatever the turn, over 2 s (the spell birds over 0.5 s); the
	// land's wolves and every other ground animal never lean
	EXPECT_EQ(detail::TimeToBank(AnimalInfo::Dove), 2.0f);
	EXPECT_EQ(detail::BankAngle(AnimalInfo::Dove, 100.0f, 3.0f), 0.5f);
	EXPECT_EQ(detail::TimeToBank(AnimalInfo::SpellDove), 0.5f);
	EXPECT_EQ(detail::BankAngle(AnimalInfo::SpellBat, 100.0f, 3.0f), 0.5f);
	EXPECT_EQ(detail::TimeToBank(AnimalInfo::Wolf), 0.0f);
	EXPECT_EQ(detail::BankAngle(AnimalInfo::Wolf, 9830.0f, 50.0f), 0.0f);
	EXPECT_EQ(detail::BankAngle(AnimalInfo::Lion, 9830.0f, 50.0f), 0.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: the spell animals' rows the tests above use
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FlockAnimalRows, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto& wolf = info->animal.at(static_cast<size_t>(AnimalInfo::SpellWolf));
	EXPECT_EQ(wolf.grownUpAge, k_WolfGrownUpAge);
	EXPECT_EQ(wolf.hunger, k_WolfHunger);
	// the hand can pick up none of the miracles' animals
	EXPECT_EQ(wolf.playerCanPickUp, 0u);
	EXPECT_EQ(info->animal.at(static_cast<size_t>(AnimalInfo::SpellDove)).playerCanPickUp, 0u);
	EXPECT_EQ(info->animal.at(static_cast<size_t>(AnimalInfo::SpellBat)).playerCanPickUp, 0u);
	EXPECT_FLOAT_EQ(info->animal.at(static_cast<size_t>(AnimalInfo::SpellDove)).altitudeNormal, k_BirdAltitudeNormal);
}
