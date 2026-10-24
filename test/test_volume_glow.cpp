/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The glowing shell's rules (Particles/VolumeGlow): its strength, reach, colour, texture slide, how side on its corners
// are seen, and its bands' triangles. Then UR_VolFXOnObject (Particles/ParticleVolumeGlowRules.cpp) run on the
// artefact's magic file with fake objects, and the shells the draw frame makes of it (particles::draw::AddShells). The
// game's list registers the rule (particles::RegisterAll).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/GlintTargetsInterface.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/VolumeGlow.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace volume_glow = openblack::particles::volume_glow;

namespace
{
constexpr float k_Epsilon = 1e-5f;
constexpr float k_Step = 0.1f;
} // namespace

TEST(VolumeGlowRules, StrengthIsThePowerClampedToOne)
{
	EXPECT_EQ(volume_glow::Strength(-1.0f), 0.0f);
	EXPECT_EQ(volume_glow::Strength(0.0f), 0.0f);
	EXPECT_EQ(volume_glow::Strength(0.4f), 0.4f);
	EXPECT_EQ(volume_glow::Strength(1.5f), 1.0f);
	EXPECT_EQ(volume_glow::Strength(std::nanf("")), 0.0f);
}

TEST(VolumeGlowRules, ReachGrowsWithStrengthUpToTheMostThenStops)
{
	// An artefact's shell: from a tenth to eight tenths of its half diagonal
	const volume_glow::Settings artefact {.zoomMin = 0.1f, .zoomMax = 0.8f};
	EXPECT_NEAR(volume_glow::Zoom(artefact, 0.0f, 1.0f), 0.1f, k_Epsilon);
	EXPECT_NEAR(volume_glow::Zoom(artefact, 0.5f, 1.0f), 0.45f, k_Epsilon);
	// An artefact glows only once worth more than 1, so its shell always reaches the furthest, whatever its worth
	EXPECT_NEAR(volume_glow::Zoom(artefact, 1.5f, 1.0f), 0.8f, k_Epsilon);
	EXPECT_NEAR(volume_glow::Zoom(artefact, 30.0f, 1.0f), 0.8f, k_Epsilon);
	// Times the scale of the atom kept on the object
	EXPECT_NEAR(volume_glow::Zoom(artefact, 1.0f, 2.0f), 1.6f, k_Epsilon);
	// One whose strength sets its alpha keeps the least reach, whatever the atom's scale
	auto fading = artefact;
	fading.alphaFromStrength = true;
	EXPECT_NEAR(volume_glow::Zoom(fading, 1.0f, 2.0f), 0.1f, k_Epsilon);
}

TEST(VolumeGlowRules, AlphaIsTheRulesOrScaledByStrengthRoundedDown)
{
	volume_glow::Settings settings {.alpha = 207};
	EXPECT_EQ(volume_glow::Alpha(settings, 0.3f), 207u);
	settings.alphaFromStrength = true;
	EXPECT_EQ(volume_glow::Alpha(settings, 0.5f), 103u);
	EXPECT_EQ(volume_glow::Alpha(settings, 2.0f), 207u);
}

TEST(VolumeGlowRules, ColourIsThePlayersWhenAskedAndFadesWithItsCollection)
{
	const volume_glow::Settings players {.alpha = 207, .usePlayerColour = true};
	EXPECT_EQ(volume_glow::Argb(players, 207, 255, 0xFF4646u), 0xCFFF4646u);
	// A collection half faded: whole steps rounded down
	EXPECT_EQ(volume_glow::Argb(players, 207, 128, 0xFF4646u), (103u << 24u) | 0xFF4646u);
	// White without a player, or for a rule that doesn't take one
	EXPECT_EQ(volume_glow::Argb(players, 207, 255, std::nullopt), 0xCFFFFFFFu);
	const volume_glow::Settings white {.alpha = 207};
	EXPECT_EQ(volume_glow::Argb(white, 207, 255, 0xFF4646u), 0xCFFFFFFFu);
}

TEST(VolumeGlowRules, TheTextureSlidesByTheGamesMillisecondsAndWraps)
{
	const auto offset = volume_glow::UvOffset({0.3f, 0.31f}, 5000);
	EXPECT_NEAR(offset.x, 0.5f, 1e-4f);
	EXPECT_NEAR(offset.y, 0.55f, 1e-4f);
	const auto back = volume_glow::UvOffset({-0.3f, 0.0f}, 5000);
	EXPECT_NEAR(back.x, -0.5f, 1e-4f);
	EXPECT_EQ(volume_glow::SinceStarted(7000, 2000), 5000u);
	EXPECT_EQ(volume_glow::SinceStarted(1000, 2000), 0u);
}

TEST(VolumeGlowRules, TheGamesTimeIsATenthOfASecondATurnAndTheMillisecondsIntoTheNext)
{
	EXPECT_EQ(volume_glow::GameMilliseconds(0, 0.0f), 0u);
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 0.0f), 2500u);
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 0.456f), 2545u);
	// Never past 99 into a turn
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 1.0f), 2599u);
}

TEST(VolumeGlowRules, TheViewNormalIsWidenedByTheProjectionThenEachAxisMadeOneLong)
{
	const std::array<glm::vec3, 3> level {glm::vec3 {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
	// Axes along the camera's own are left as they are
	const auto side = volume_glow::ViewNormal({1.0f, 0.0f, 0.0f}, level, {1.43f, 1.79f});
	EXPECT_NEAR(side.x, 1.0f, k_Epsilon);
	EXPECT_NEAR(side.y, 0.0f, k_Epsilon);
	// An axis half towards the camera leans across once widened
	const std::array<glm::vec3, 3> turned {glm::vec3 {0.6f, 0.0f, 0.8f}, {0.0f, 1.0f, 0.0f}, {-0.8f, 0.0f, 0.6f}};
	const auto leaning = volume_glow::ViewNormal({1.0f, 0.0f, 0.0f}, turned, {2.0f, 2.0f});
	EXPECT_NEAR(leaning.x, 1.2f / std::sqrt(1.44f + 0.64f), k_Epsilon);
	EXPECT_NEAR(leaning.z, 0.8f / std::sqrt(1.44f + 0.64f), k_Epsilon);
}

TEST(VolumeGlowRules, TheInnerEdgeGlowsTheMoreSideOnItIsSeen)
{
	// Facing the camera: dark
	EXPECT_EQ(volume_glow::EdgeLevel({0.0f, 0.0f}, false), 0u);
	// Wholly side on, a model without bones reaches only a little of the colour
	EXPECT_EQ(volume_glow::EdgeLevel({1.0f, 0.0f}, false), 24u);
	EXPECT_EQ(volume_glow::EdgeLevel({0.7071f, 0.7071f}, false), 24u);
	// A normal widened by the projection can lean further
	EXPECT_EQ(volume_glow::EdgeLevel({1.2f, 0.6f}, false), 125u);
	// A model moved by bones measures side on by the sum of the two
	EXPECT_EQ(volume_glow::EdgeLevel({0.7071f, 0.7071f}, true), 151u);
	EXPECT_EQ(volume_glow::EdgeLevel({1.5f, 1.5f}, true), 255u);
	EXPECT_EQ(volume_glow::EdgeLevel({0.5f, 0.0f}, true), 0u);
}

TEST(VolumeGlowRules, TheInnerColourScalesItsChannelsAsWholeBytesKeepingItsAlpha)
{
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 255), 0xCFFF4646u);
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 24), 0xCF170606u);
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 0), 0xCF000000u);
}

TEST(VolumeGlowRules, TextureCoordinatesRunAcrossByTheNormalAndBackToTheOuterEdge)
{
	const auto inner = volume_glow::TextureCoordinates(0.5f, {0.25f, 0.4f}, {0.7f, 0.2f}, false);
	EXPECT_NEAR(inner.x, 0.6f, k_Epsilon);
	EXPECT_NEAR(inner.y, 0.4f, k_Epsilon);
	const auto outer = volume_glow::TextureCoordinates(0.5f, {0.25f, 0.4f}, {0.7f, 0.2f}, true);
	EXPECT_NEAR(outer.x, 0.6f, k_Epsilon);
	EXPECT_NEAR(outer.y, 0.2f, k_Epsilon);
}

TEST(VolumeGlowRules, EachEdgeOfEachTriangleBecomesABandOfTwoTriangles)
{
	const std::vector<uint16_t> triangle {0, 1, 2};
	const auto bands = volume_glow::ShellTriangles(triangle);
	const std::vector<uint32_t> expected {0, 1, 3, 3, 2, 0, 2, 3, 5, 5, 4, 2, 4, 5, 1, 1, 0, 4};
	EXPECT_EQ(bands, expected);
	const std::vector<uint16_t> two {0, 1, 2, 2, 1, 3};
	EXPECT_EQ(volume_glow::ShellTriangles(two).size(), 36u);
	EXPECT_TRUE(volume_glow::ShellTriangles(std::vector<uint16_t> {0, 1}).empty());
}

namespace
{
const auto k_Rock = static_cast<entt::entity>(7);

/// Objects standing where they are told; the others are not there
class StandingObjects final: public ecs::systems::GlintTargetsInterface
{
public:
	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override
	{
		if (const auto found = objects.find(object); found != objects.end())
		{
			return found->second;
		}
		return std::nullopt;
	}
	[[nodiscard]] uint32_t TargetPointCount(entt::entity /*object*/) const override { return 0; }
	[[nodiscard]] std::optional<glm::vec3> TargetPoint(entt::entity /*object*/, uint32_t /*index*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] float TargetScale(entt::entity /*object*/) const override { return 1.0f; }

	std::map<entt::entity, glm::vec3> objects;
};

/// The artefact's magic file as the game ships it, without its fading rules
std::string ArtefactGlowFile()
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 0\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS UR_VolFXOnObject UR_VolFXOnObject0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Alpha INTEGER 207\n";
	text += "PROPERTY AnimSpeedU FLOAT 0.3\n";
	text += "PROPERTY AnimSpeedV FLOAT 0.31\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY Group INTEGER 0\n";
	text += "PROPERTY MatInd INTEGER 2\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += "PROPERTY TextFracU FLOAT 0.7\n";
	text += "PROPERTY TextFracV FLOAT 0.2\n";
	text += "PROPERTY ZoomProportionMax FLOAT 0.8\n";
	text += "PROPERTY ZoomProportionMin FLOAT 0.1\n";
	text += "PROPERTY UsePlayerColor BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 255\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY InitialScale FLOAT 1\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

/// Fake objects in the locator; the previous ones come back after
class VolumeGlowEffectTest: public ::testing::Test
{
protected:
	void SetUp() override { world = &static_cast<StandingObjects&>(Locator::glintTargets::emplace<StandingObjects>()); }

	/// The artefact's magic at a strength
	static std::unique_ptr<particles::Effect> Make(float magnitude)
	{
		auto file = particles::File::Parse(ArtefactGlowFile(), "test");
		EXPECT_TRUE(file.has_value());
		return std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)), glm::vec3(0.0f),
		                                           magnitude);
	}

	/// The effect's glowing atoms, as its draw walk gives them
	static std::vector<particles::Effect::DrawAtom> Glowing(const particles::Effect& effect)
	{
		std::vector<particles::Effect::OrderedItem> items;
		std::vector<particles::Effect::DrawChain> chains;
		effect.CollectOrdered(1.0f, items, chains);
		std::vector<particles::Effect::DrawAtom> glowing;
		for (const auto& item : items)
		{
			if (item.chain < 0 && item.atom.objectGlow.has_value())
			{
				glowing.push_back(item.atom);
			}
		}
		return glowing;
	}

	StandingObjects* world {nullptr};

private:
	test::RestoreService<Locator::glintTargets> _restoreObjects;
};
} // namespace

TEST(ParticleVolumeGlowRules, TheGameRegistersTheRule)
{
	EXPECT_NE(particles::FindModifierFactory("UR_VolFXOnObject"), nullptr);
}

TEST_F(VolumeGlowEffectTest, AnArtefactsShellIsKeptOnItInItsOwnersColourAndDrawnWhereItStands)
{
	world->objects[k_Rock] = {10.0f, 2.0f, 20.0f};
	auto effect = Make(1.5f);
	EXPECT_TRUE(effect->UnportedClasses().empty());
	effect->SetPlayer(0);
	effect->AddTarget(k_Rock);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 1u);
	// Drawn as its shell only: never as a sprite or a point
	std::vector<particles::Effect::DrawAtom> plain;
	effect->Collect(1.0f, plain, particles::Creator::Kind::Point);
	effect->Collect(1.0f, plain, particles::Creator::Kind::Sprite);
	EXPECT_TRUE(plain.empty());

	const auto glowing = Glowing(*effect);
	ASSERT_EQ(glowing.size(), 1u);
	const auto& glow = *glowing[0].objectGlow;
	EXPECT_EQ(glow.object, k_Rock);
	EXPECT_NEAR(glow.zoom, 0.8f, k_Epsilon);
	// Player 0's colour, at the rule's alpha
	EXPECT_EQ(glow.argb, 0xCFFF4646u);
	EXPECT_EQ(glow.texture, 2);
	EXPECT_EQ(glowing[0].position, glm::vec3(10.0f, 2.0f, 20.0f));

	particles::draw::Frame frame;
	frame.sorted.shells.push_back({glowing[0].position, glowing[0], 1, 1.0f});
	particles::draw::AddShells(frame, glow.startedAt);
	ASSERT_EQ(frame.shells.size(), 1u);
	const auto& shell = frame.shells[0];
	EXPECT_EQ(shell.object, k_Rock);
	EXPECT_EQ(shell.path, particles::DrawPath::Sorted);
	EXPECT_EQ(shell.sortPoint, glm::vec3(10.0f, 2.0f, 20.0f));
	const auto name = "raw/" + particles::TextureBaseName("S_VolLight3");
	EXPECT_EQ(shell.texture, entt::hashed_string(name.c_str()).value());
	EXPECT_EQ(shell.alphaTexture, entt::hashed_string((name + "a").c_str()).value());
	EXPECT_NEAR(shell.textureFraction.x, 0.7f, k_Epsilon);
	EXPECT_EQ(shell.uvOffset, glm::vec2(0.0f));

	// The object gone, its shell goes with it
	world->objects.clear();
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

TEST_F(VolumeGlowEffectTest, AWeakerEffectReachesLessAndATargetThatIsNotThereMakesNoShell)
{
	world->objects[k_Rock] = {0.0f, 0.0f, 0.0f};
	auto effect = Make(0.5f);
	effect->AddTarget(static_cast<entt::entity>(99));
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	effect->AddTarget(k_Rock);
	effect->Step(k_Step);
	effect->Step(k_Step);
	const auto glowing = Glowing(*effect);
	ASSERT_EQ(glowing.size(), 1u);
	EXPECT_NEAR(glowing[0].objectGlow->zoom, 0.45f, k_Epsilon);
	// Without a player it is white
	EXPECT_EQ(glowing[0].objectGlow->argb, 0xCFFFFFFFu);
}

TEST_F(VolumeGlowEffectTest, TheShellsTextureSlidesByTheGamesTimeAndStandsStillWhilePaused)
{
	world->objects[k_Rock] = {0.0f, 0.0f, 0.0f};
	auto effect = Make(1.5f);
	effect->AddTarget(k_Rock);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	const auto glowing = Glowing(*effect);
	ASSERT_EQ(glowing.size(), 1u);
	const auto started = glowing[0].objectGlow->startedAt;
	const auto offsetAt = [&glowing](uint64_t gameMilliseconds) {
		return particles::draw::ShellOf(glowing[0], particles::DrawPath::Sorted, gameMilliseconds).uvOffset;
	};
	EXPECT_EQ(offsetAt(started), glm::vec2(0.0f));
	// Two seconds of the game's time later it has slid by its speeds
	const auto slid = offsetAt(started + 2000);
	EXPECT_NEAR(slid.x, 0.6f, 1e-4f);
	EXPECT_NEAR(slid.y, 0.62f, 1e-4f);
	// Paused, the game's time stands still, and so does the texture, however long the frames take
	EXPECT_EQ(offsetAt(started + 2000), slid);
}

TEST_F(VolumeGlowEffectTest, AQueuedEffectsShellIsDrawnWithItsEffect)
{
	world->objects[k_Rock] = {1.0f, 0.0f, 1.0f};
	auto effect = Make(1.5f);
	effect->AddTarget(k_Rock);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	particles::draw::Frame frame;
	particles::manager::OrderedEffect queued {1, particles::DrawPath::Queued, glm::vec3(0.0f), 1.0f, {}, {}};
	effect->CollectOrdered(1.0f, queued.items, queued.chains);
	frame.queued.push_back(std::move(queued));
	particles::draw::AddShells(frame, 0);
	ASSERT_EQ(frame.shells.size(), 1u);
	EXPECT_EQ(frame.shells[0].path, particles::DrawPath::Queued);
	ASSERT_NE(frame.shells[0].atom, nullptr);
	EXPECT_TRUE(frame.shells[0].atom->objectGlow.has_value());
	frame.Clear();
	EXPECT_TRUE(frame.shells.empty());
	// The debug bar's switch that hides the shells is off by default
	EXPECT_FALSE(particles::draw::ShellDebug {}.hidden);
}
