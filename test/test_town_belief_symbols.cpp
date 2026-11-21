/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' symbols as particles draw them: the town centres' belief symbols, the pure motion of UR_TownCentreBelief
// (src/Particles/TownBeliefMaths.h), the colours a player's symbol is drawn with (src/Particles/PlayerSymbol.h), the symbol
// sprite creator and the registered classes, on fake data

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/PlayerSymbolSheet.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TownBeliefSymbols.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/PlayerSymbol.h"
#include "Particles/TownBelief.h"
#include "Particles/TownBeliefMaths.h"
#include "support/MapFakes.h"
#include "support/ParticleShown.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// SF_TownBelief's UR_TownCentreBelief0
town_belief::Params FileParams()
{
	return {.radiusAt0 = 0.3f,
	        .radiusAt1 = 0.6f,
	        .speedAt0 = 1.0f,
	        .speedAt1 = 2.0f,
	        .scaleAt0 = 0.5f,
	        .scaleAt1 = 1.0f,
	        .heightAt1 = 2.0f,
	        .heightPerLevel = 2.5f,
	        .useFixedScale = true,
	        .fixedScale = 0.01f,
	        .timeBetweenFightsAt0 = 10.0f,
	        .timeBetweenFightsAt1 = 3.0f,
	        .fightDurationAt0 = 1.5f,
	        .fightDurationAt1 = 3.0f,
	        .speedUpDuringFight = 2.0f,
	        .fightFracWhenEmitting = 0.2f,
	        .angleSpeedPhaseSpeed = 0.2f};
}

/// A random source that hands out its numbers in turn and records what it was asked
struct Draws
{
	std::vector<float> values;
	std::vector<std::array<float, 2>> asked;
	size_t next {0};
	float operator()(float a, float b)
	{
		asked.push_back({a, b});
		return values.at(next++);
	}
};
} // namespace

TEST(TownBeliefSymbols, TheClassesOfTheTownBeliefFileAreRegistered)
{
	EXPECT_NE(FindModifierFactory("UR_TownCentreBelief"), nullptr);
	EXPECT_NE(FindCondition("EC_CollectionShouldBeEmitting"), nullptr);
}

TEST(TownBeliefSymbols, ACollectionShouldBeEmittingOnlyWithItsEmittingFlag)
{
	const auto test = FindCondition("EC_CollectionShouldBeEmitting");
	ASSERT_NE(test, nullptr);
	const auto file = File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n", "test");
	ASSERT_TRUE(file.has_value());
	const Effect effect(std::make_shared<const File>(*file), glm::vec3(0.0f), 1.0f);
	const Object condition;
	Collection collection;
	EXPECT_FALSE(test(effect, condition, nullptr, collection)); // 3 when made
	collection.flags |= 0x04;
	EXPECT_TRUE(test(effect, condition, nullptr, collection));
}

TEST(TownBeliefSymbols, BeliefIsShownBetweenNoneAndAll)
{
	EXPECT_EQ(town_belief::Shown(-0.5f), 0.0f);
	EXPECT_EQ(town_belief::Shown(0.25f), 0.25f);
	EXPECT_EQ(town_belief::Shown(3.0f), 1.0f);
	EXPECT_EQ(town_belief::Shown(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(TownBeliefSymbols, RankCountsThePlayersBelievedInMoreTiesGoingToTheLater)
{
	// player 1 most, then 0 and 3 tied (3 ranks first, being later), the neutral slot counts too
	const std::array<float, 8> beliefs {0.4f, 0.9f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.5f};
	EXPECT_EQ(town_belief::Rank(beliefs, 1), 0);
	EXPECT_EQ(town_belief::Rank(beliefs, 3), 2);
	EXPECT_EQ(town_belief::Rank(beliefs, 0), 3);
	// above 1 counts as 1: tied with player 1, so the later one ranks first
	const std::array<float, 8> capped {2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	EXPECT_EQ(town_belief::Rank(capped, 1), 0);
	EXPECT_EQ(town_belief::Rank(capped, 0), 1);
}

TEST(TownBeliefSymbols, TheWayRoundGrowsWithTheCameraDistanceUpToTenTimes)
{
	const auto params = FileParams();
	EXPECT_EQ(town_belief::DistanceScale(params, 50.0f), 1.0f);
	EXPECT_FLOAT_EQ(town_belief::DistanceScale(params, 400.0f), 4.0f);
	EXPECT_EQ(town_belief::DistanceScale(params, 5000.0f), 10.0f);
	auto fixed = params;
	fixed.useFixedScale = false;
	EXPECT_EQ(town_belief::DistanceScale(fixed, 5000.0f), 1.0f);
}

TEST(TownBeliefSymbols, TheMostBelievedStandsStillOverTheTotemSizedByItsBelief)
{
	Draws draws;
	town_belief::Symbol symbol {.around = 1.0f, .tilt = 0.5f};
	const auto look = town_belief::Step(symbol, FileParams(), 0.5f, 0, 0.1f, 3.0f, std::ref(draws));
	EXPECT_EQ(look.offset, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(look.scale, 0.75f);
	EXPECT_EQ(look.fight, 0.0f);
	EXPECT_EQ(symbol.around, 1.0f);
	EXPECT_EQ(symbol.tilt, 0.5f);
	EXPECT_TRUE(draws.asked.empty());
}

TEST(TownBeliefSymbols, ASymbolOfNoBeliefHasNoSize)
{
	Draws draws;
	town_belief::Symbol symbol;
	EXPECT_EQ(town_belief::Step(symbol, FileParams(), 0.0f, 3, 0.1f, 1.0f, std::ref(draws)).scale, 0.0f);
}

TEST(TownBeliefSymbols, AnotherTurnsALevelUpForEachPlayerBelievedInMore)
{
	Draws draws;
	town_belief::Symbol symbol {.around = 0.0f, .tilt = 0.0f};
	const auto look = town_belief::Step(symbol, FileParams(), 1.0f, 2, 0.1f, 2.0f, std::ref(draws));
	// radius 0.6 and speed 2 / 0.6 at full belief; the sway 0.02 after the step
	const float speed = 2.0f / 0.6f;
	EXPECT_FLOAT_EQ(symbol.sway, 0.02f);
	EXPECT_FLOAT_EQ(symbol.around, speed * 0.1f);
	EXPECT_FLOAT_EQ(symbol.tilt, ((std::cos(0.02f) + 1.0f) * 0.25f + 0.5f) * speed * 0.1f * 0.846f);
	// two levels up, at the radius, times the distance scale
	EXPECT_NEAR(look.offset.y, (2.0f * 2.5f + std::sin(symbol.tilt) * 0.6f) * 2.0f, 1e-5f);
	EXPECT_NEAR(glm::length(glm::vec2(look.offset.x, look.offset.z)), std::cos(symbol.tilt) * 0.6f * 2.0f, 1e-5f);
	EXPECT_TRUE(draws.asked.empty());
}

TEST(TownBeliefSymbols, TheSecondFightsTheFirstNowAndThenComingDownToItsLevel)
{
	const auto params = FileParams();
	Draws draws {.values = {1.0f, 1.0f}};
	town_belief::Symbol symbol;
	// no wait at first: the clock passes the length 0 and the next wait and fight are drawn, the wait first
	auto look = town_belief::Step(symbol, params, 0.5f, 1, 0.1f, 1.0f, std::ref(draws));
	ASSERT_EQ(draws.asked.size(), 2u);
	EXPECT_EQ(draws.asked[0], (std::array<float, 2> {0.5f, 1.5f}));
	EXPECT_FLOAT_EQ(symbol.untilFight, (3.0f - 10.0f) * 0.5f + 10.0f);
	EXPECT_FLOAT_EQ(symbol.fightLength, (3.0f - 1.5f) * 0.5f + 1.5f);
	EXPECT_EQ(symbol.intoFight, 0.0f);
	EXPECT_EQ(look.fight, 0.0f);
	// waiting: it counts down by dt and does not fight
	look = town_belief::Step(symbol, params, 0.5f, 1, 0.1f, 1.0f, std::ref(draws));
	EXPECT_FLOAT_EQ(symbol.untilFight, 6.5f - 0.1f);
	EXPECT_EQ(look.fight, 0.0f);
	// halfway into a fight it is at its height and down at the first's level
	symbol.untilFight = 0.0f;
	symbol.intoFight = 1.125f - 0.1f;
	look = town_belief::Step(symbol, params, 0.5f, 1, 0.1f, 1.0f, std::ref(draws));
	EXPECT_FLOAT_EQ(look.fight, 1.0f);
	EXPECT_NEAR(look.offset.y, std::sin(symbol.tilt) * 0.45f, 1e-6f);
	EXPECT_EQ(draws.asked.size(), 2u);
}

TEST(TownBeliefSymbols, AFightThrowsSparksAtItsHeight)
{
	const auto params = FileParams();
	EXPECT_EQ(town_belief::SparkAlpha(0.0f), 0u);
	EXPECT_EQ(town_belief::SparkAlpha(0.5f), 255u);
	EXPECT_EQ(town_belief::SparkAlpha(1.0f), 0u);
	// 255 (1 - (2 x 0.25 - 1)^2) = 191.25, truncated
	EXPECT_EQ(town_belief::SparkAlpha(0.25f), 191u);
	EXPECT_FALSE(town_belief::Sparking(params, 0.2f));
	EXPECT_TRUE(town_belief::Sparking(params, 0.21f));
}

TEST(TownBeliefSymbols, SymbolsGoToTheActivePlayersBeforeTheNeutralOne)
{
	// player one human, three a computer god, the neutral player in slot 7
	std::array<int32_t, 8> types {1, 0, 0, 2, 0, 0, 0, 3};
	EXPECT_EQ(town_belief::SymbolPlayers([&types](int slot) { return types.at(static_cast<size_t>(slot)); }),
	          (std::vector<int> {0, 3}));
	// after a player, a neutral one stops the walk
	types = {1, 3, 2, 0, 0, 0, 0, 3};
	EXPECT_EQ(town_belief::SymbolPlayers([&types](int slot) { return types.at(static_cast<size_t>(slot)); }),
	          (std::vector<int> {0}));
	types = {0, 0, 0, 0, 0, 0, 0, 3};
	EXPECT_TRUE(town_belief::SymbolPlayers([&types](int slot) { return types.at(static_cast<size_t>(slot)); }).empty());
}

// The original's PlayerSymbolSprite::SetAlphaColor run through the oracle: the symbol the player's colour times the
// particle's in four channels, (c p) >> 8; the glow that colour at (alpha x 100) >> 8
TEST(TownBeliefSymbols, ASymbolsColoursAreThePlayersTimesTheParticles)
{
	struct Case
	{
		uint32_t player, particle, symbol, glow;
	};
	constexpr std::array<Case, 5> k_Cases {{
	    {.player = 0xFF4646u, .particle = 0xFFFFFFFFu, .symbol = 0xFEFE4545u, .glow = 0x63FE4545u},
	    {.player = 0x47FF54u, .particle = 0xFFFFFFFFu, .symbol = 0xFE46FE53u, .glow = 0x6346FE53u},
	    {.player = 0xE347FFu, .particle = 0x80FFFFFFu, .symbol = 0x7FE246FEu, .glow = 0x31E246FEu},
	    {.player = 0x4777FFu, .particle = 0xFF808080u, .symbol = 0xFE233B7Fu, .glow = 0x63233B7Fu},
	    {.player = 0xFFA247u, .particle = 0x01FFFFFFu, .symbol = 0x00FEA146u, .glow = 0x00FEA146u},
	}};
	for (const auto& c : k_Cases)
	{
		const auto colours = player_symbol::ColoursOf(c.player, c.particle);
		EXPECT_EQ(colours.symbol, c.symbol) << std::hex << c.player;
		EXPECT_EQ(colours.glow, c.glow) << std::hex << c.player;
		EXPECT_EQ(colours.whiteGlow, (c.glow & 0xFF000000u) | 0x00FFFFFFu) << std::hex << c.player;
	}
}

TEST(TownBeliefSymbols, TheLocalPlayersSymbolIsItsProfilesAndEveryOtherPlayersItsCellOfTheSheet)
{
	using player_symbol::SymbolSource;
	EXPECT_EQ(player_symbol::SourceOf(0, 0), SymbolSource::Profile);
	EXPECT_EQ(player_symbol::SourceOf(3, 3), SymbolSource::Profile);
	EXPECT_EQ(player_symbol::SourceOf(2, 0), SymbolSource::Sheet);
	EXPECT_EQ(player_symbol::SourceOf(0, 3), SymbolSource::Sheet);

	// the local player's: its profile's symbol's cell of ChooseSymbol, whatever its number; past the sixteen the first
	const auto& local = player_symbol::SymbolCreator(2, 2, 7);
	EXPECT_EQ(local.texture, "ChooseSymbol");
	EXPECT_EQ(local.fileOffset, 7);
	EXPECT_EQ(player_symbol::SymbolCreator(2, 2, 0).fileOffset, 0);
	EXPECT_EQ(player_symbol::SymbolCreator(2, 2, 16).fileOffset, 0);
	// another player's: its own cell of the players' symbol sheet, four to a row, one frame, whatever the profile's
	const auto& other = player_symbol::SymbolCreator(3, 0, 7);
	EXPECT_EQ(other.kind, Creator::Kind::Sprite);
	EXPECT_EQ(other.texture, player_symbol_sheet::k_TextureName);
	EXPECT_EQ(other.fileOffset, 3);
	EXPECT_EQ(other.spritesPerRow, 4);
	EXPECT_EQ(other.numFrames, 1);
	EXPECT_EQ(player_symbol::SymbolCreator(0, 1, 7).texture, player_symbol_sheet::k_TextureName);
	EXPECT_EQ(player_symbol::SymbolCreator(0, 1, 7).fileOffset, 0);
	// the name the creators carry is the sheet's id in the texture cache
	EXPECT_EQ(player_symbol_sheet::k_TextureId, entt::hashed_string("player_symbols/sheet").value());
}

TEST(TownBeliefSymbols, ASymbolIsDrawnAsTwoGlowsThenTheSymbol)
{
	const player_symbol::PlayerSymbol symbol {.player = 3, .cellA = 5.0f, .cellB = 9.0f, .spin = 0.0f};
	const auto colours = player_symbol::ColoursOf(0xFF4646u, 0xFFFFFFFFu);
	std::vector<Effect::DrawAtom> out;
	player_symbol::Draw(symbol, glm::vec3(1.0f, 2.0f, 3.0f), 0.75f, colours, nullptr, out);
	ASSERT_EQ(out.size(), 3u);
	EXPECT_EQ(out[0].creator, &player_symbol::GlowCreator());
	EXPECT_EQ(out[1].creator, &player_symbol::GlowCreator());
	EXPECT_EQ(out[2].creator, &player_symbol::SymbolCreator(3));
	EXPECT_FLOAT_EQ(out[0].scale, 1.125f);
	EXPECT_FLOAT_EQ(out[2].scale, 0.75f);
	EXPECT_EQ(out[0].frame, 5.0f);
	EXPECT_EQ(out[1].frame, 9.0f);
	EXPECT_EQ(out[0].alpha, 99.0f);
	EXPECT_EQ(out[1].colour, (std::array<uint8_t, 3> {255, 255, 255}));
	EXPECT_EQ(out[2].alpha, 254.0f);
	EXPECT_EQ(out[2].colour, (std::array<uint8_t, 3> {0xFE, 0x45, 0x45}));
}

// A town centre's effect is made where the centre is made functional; with no particle system or no game files (a unit
// test's world) none is made, and the centre keeps no effect
TEST(TownBeliefSymbols, NoCentreEffectIsMadeWithoutTheParticleSystemOrTheGamesFiles)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::filesystem> restoreFiles;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::filesystem::reset();
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = registry.Create();
	town_belief::MakeEffect(centre);
	EXPECT_EQ(registry.TryGet<ecs::components::TownBeliefSymbols>(centre), nullptr);
}

namespace
{
/// The hand icon's mesh and the Norse plinth's, with their boxes from the game's mesh pack
constexpr entt::id_type k_HandIconMesh = 1;
constexpr entt::id_type k_PlinthMesh = 2;
std::optional<AxisAlignedBoundingBox> TotemBoxes(entt::id_type id)
{
	switch (id)
	{
	case k_HandIconMesh:
		// its lowest and highest points, as the pack's floats
		return AxisAlignedBoundingBox {{-0.88547069f, std::bit_cast<float>(0xBEA5C360U), -0.99318051f},
		                               {1.59252882f, std::bit_cast<float>(0x40F2B652U), 1.65791178f}};
	case k_PlinthMesh:
		return AxisAlignedBoundingBox {{-3.21349502f, -0.0000105f, -2.24865246f}, {3.21383905f, 11.45960045f, 3.76208544f}};
	default:
		return std::nullopt;
	}
}
} // namespace

// The symbols turn over the totem's icon, raised by the icon mesh's height at the totem's scale and HeightAt1, never
// over the plinth by the plinth's height; the numbers are the original rule's own, for the hand icon
TEST(TownBeliefSymbols, TheSymbolsTurnOverTheTotemsIconRaisedByTheIconsHeight)
{
	using namespace ecs::components;
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	test::FakeMeshBoxProvider boxes;
	boxes.meshBox = &TotemBoxes;
	Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = registry.Create();
	EXPECT_FALSE(town_belief::SymbolBase(centre, 2.0f).has_value());

	const auto totem = [&registry, centre](glm::vec3 iconAt, float scale) {
		const auto icon = registry.Create();
		registry.Assign<Transform>(icon, iconAt, glm::mat3(1.0f), glm::vec3(scale));
		registry.Assign<Mesh>(icon, k_HandIconMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		const auto plinth = registry.Create();
		registry.Assign<Transform>(plinth, iconAt - glm::vec3(0.0f, 2.729f, 0.0f), glm::mat3(1.0f), glm::vec3(scale));
		registry.Assign<Mesh>(plinth, k_PlinthMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<VillageTotem>(plinth, VillageTotem {.townCentre = centre, .icon = icon});
		return plinth;
	};
	const auto first = totem(glm::vec3(10.0f, 24.0f, 20.0f), 1.0f);
	auto base = town_belief::SymbolBase(centre, 2.0f);
	ASSERT_TRUE(base.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(base->x), 0x41200000U);
	EXPECT_EQ(std::bit_cast<uint32_t>(base->y), 0x4207A251U); // 33.908512
	EXPECT_EQ(std::bit_cast<uint32_t>(base->z), 0x41A00000U);

	// a totem at a scale of 0.99, about that of Land 1's Norse town centre, raises them by less
	registry.Destroy(first);
	totem(glm::vec3(1805.22f, 30.0f, 2691.31f), 0.99f);
	base = town_belief::SymbolBase(centre, 2.0f);
	ASSERT_TRUE(base.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(base->x), 0x44E1A70AU);
	EXPECT_EQ(std::bit_cast<uint32_t>(base->y), 0x421F5156U); // 39.82943
	EXPECT_EQ(std::bit_cast<uint32_t>(base->z), 0x452834F6U);
}

namespace
{
/// One atom of ParticleSymbolSpriteCreator, made as the effect starts
constexpr std::string_view k_SymbolSpriteEffect = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticleSymbolSpriteCreator ParticleSymbolSpriteCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 2
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticleSymbolSpriteCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
)";

std::unique_ptr<Effect> SymbolSpriteEffect(int player)
{
	auto file = File::Parse(std::string(k_SymbolSpriteEffect), "SF_SymbolSpriteTest");
	EXPECT_TRUE(file.has_value());
	auto effect = std::make_unique<Effect>(std::make_shared<const File>(std::move(*file)), glm::vec3(4.0f, 5.0f, 6.0f), 1.0f);
	effect->SetPlayer(player);
	return effect;
}
} // namespace

TEST(TownBeliefSymbols, TheSymbolSpriteCreatorIsRegisteredAndNotASprite)
{
	ASSERT_NE(FindCreatorFactory("ParticleSymbolSpriteCreator"), nullptr);
	const auto file = File::Parse(std::string(k_SymbolSpriteEffect), "SF_SymbolSpriteTest");
	ASSERT_TRUE(file.has_value());
	const auto creator = FindCreatorFactory("ParticleSymbolSpriteCreator")(*file->Find("ParticleSymbolSpriteCreator0"));
	ASSERT_NE(creator, nullptr);
	// not drawn by the sprite pass as a sheet of its own: its atoms are drawn as their player's symbol
	EXPECT_EQ(creator->kind, Creator::Kind::Other);
	EXPECT_FLOAT_EQ(creator->initialScale, 2.0f);
}

TEST(TownBeliefSymbols, ASymbolSpriteAtomIsDrawnAsItsEffectsPlayersSymbol)
{
	const game_random::testing::ScopedState random;
	auto effect = SymbolSpriteEffect(4);
	effect->Step(0.1f);
	std::vector<Effect::DrawAtom> sprites;
	openblack::test::ShowEveryAtom(*effect);
	effect->Collect(1.0f, sprites, Creator::Kind::Sprite);
	ASSERT_EQ(sprites.size(), 3u);
	ASSERT_NE(sprites[0].atom, nullptr);
	ASSERT_NE(sprites[0].atom->playerSymbol, nullptr);
	EXPECT_EQ(sprites[0].atom->playerSymbol->player, 4);
	EXPECT_EQ(sprites[2].creator, &player_symbol::SymbolCreator(4));
	// the symbol its drawn size (InitialScale 2), the glows 1.5 times that, at the atom
	EXPECT_FLOAT_EQ(sprites[2].scale, 2.0f);
	EXPECT_FLOAT_EQ(sprites[0].scale, 3.0f);
	EXPECT_EQ(sprites[2].position, sprites[0].atom->current.position);
	// no other pass draws it
	std::vector<Effect::DrawAtom> others;
	openblack::test::ShowEveryAtom(*effect);
	effect->Collect(1.0f, others, Creator::Kind::Other);
	EXPECT_TRUE(others.empty());
}
