/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_LightSheetOnObject (Particles/Rules/LightSheetOnObject.cpp) run on the rings' part of the singing stones' healing
// effect as the game ships it, with a fake of the particles that keeps the sheets of light it is given. The game's list
// registers the rule (particles::RegisterAll).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureArenaRing.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
constexpr float k_Step = 0.1f;

/// The particles, keeping every sheet of light they are given
class KeptSheets final: public test::InertParticleSystem
{
public:
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { sheets.push_back(sheet); }
	std::vector<std::weak_ptr<particles::LightSheet>> sheets;
};

/// The rings' group of the singing stones' healing effect as the game ships it (its fading rules left out); without
/// the radius provider when `withProvider` is false
std::string HealingRingsFile(bool withProvider = true)
{
	std::string text;
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY DeleteOnCloseDown BOOL 0\n";
	text += "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY InitiallyCreated ARRAY SIZE 25 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
	text += "PROPERTY MaxSpellAge FLOAT -1\n";
	text += "ENDPROPERTIES\n";
	text += "BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Maximum FLOAT 1000\n";
	text += "PROPERTY Minimum FLOAT 0\n";
	text += "PROPERTY ScaleBy FLOAT 1.11062\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS UR_LightSheetOnObject UR_LightSheetOnObject0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY Condition PERSIS_PNTR NULL_STRING\n";
	text += "PROPERTY Group INTEGER 1\n";
	text += "PROPERTY NextGroups ARRAY SIZE 0\n";
	text += "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\n";
	text += "PROPERTY RemoveOnCloseDown BOOL 0\n";
	text += withProvider ? "PROPERTY RadiusFP PERSIS_PNTR MagnitudeFloatProvider0\n" : "";
	text += "PROPERTY NumAtoms INTEGER 3\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	text += "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n";
	text += "BEGINPROPERTIES\n";
	text += "PROPERTY ColorA INTEGER 30\n";
	text += "PROPERTY ColorB INTEGER 255\n";
	text += "PROPERTY ColorG INTEGER 255\n";
	text += "PROPERTY ColorR INTEGER 255\n";
	text += "PROPERTY InitialScale FLOAT 1\n";
	text += "PROPERTY LoopAnim BOOL 1\n";
	text += "ENDPROPERTIES\n";
	text += "ENDCLASS\n";
	return text;
}

class LightSheetOnObjectTest: public ::testing::Test
{
protected:
	void SetUp() override { particles = &static_cast<KeptSheets&>(Locator::particleSystem::emplace<KeptSheets>()); }

	/// The effect at a point, of a magnitude
	static std::unique_ptr<particles::Effect> Make(glm::vec3 at, float magnitude, bool withProvider = true)
	{
		auto file = particles::File::Parse(HealingRingsFile(withProvider), "test");
		EXPECT_TRUE(file.has_value());
		return std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*file)), at, magnitude);
	}

	KeptSheets* particles {nullptr};

private:
	test::RestoreService<Locator::particleSystem> _restoreParticles;
};
} // namespace

TEST(LightSheetOnObjectRules, TheGameRegistersTheRule)
{
	const test::RestoreService<Locator::particleSystem> restore;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	EXPECT_NE(particles::FindModifierFactory("UR_LightSheetOnObject"), nullptr);
}

TEST_F(LightSheetOnObjectTest, ItsAtomsEachStandARingOfThePlayersDimmedColourOfTheRadiusTheMagnitudeGives)
{
	const glm::vec3 at {100.0f, 5.0f, 200.0f};
	// the singing stones' song sets its effect's magnitude to 13.5
	auto effect = Make(at, 13.5f);
	EXPECT_TRUE(effect->UnportedClasses().empty());
	effect->SetPlayer(0);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 3u);
	ASSERT_EQ(particles->sheets.size(), 3u);
	const auto expected = creature_arena_ring::Points({at.x, at.z}, 13.5f * 1.11062f, 32);
	for (const auto& kept : particles->sheets)
	{
		const auto sheet = kept.lock();
		ASSERT_NE(sheet, nullptr);
		ASSERT_EQ(sheet->Points().size(), 32u);
		for (size_t i = 0; i < expected.size(); ++i)
		{
			EXPECT_EQ(sheet->Points()[i].x, expected[i].x) << i;
			EXPECT_EQ(sheet->Points()[i].z, expected[i].y) << i;
		}
		EXPECT_EQ(sheet->Height(), 10.0f);
		EXPECT_EQ(sheet->Spread(), 1.1f);
		EXPECT_EQ(sheet->Strength(), 1.0f);
		// player 0's red (0xFF4646), each byte times the atom's alpha of 30 over 256
		EXPECT_EQ(sheet->Colour(), 0x1D0808u);
	}
	// later steps keep the same rings and make no more atoms
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 3u);
	EXPECT_EQ(particles->sheets.size(), 3u);
	// the rings go with their atoms
	effect.reset();
	for (const auto& kept : particles->sheets)
	{
		EXPECT_TRUE(kept.expired());
	}
}

TEST_F(LightSheetOnObjectTest, WithoutAPlayerItsRingsAreGreyAndWithoutAProviderTheyTakeTheDefaultRadius)
{
	auto effect = Make(glm::vec3(0.0f), 1.0f, false);
	effect->Step(k_Step);
	ASSERT_EQ(particles->sheets.size(), 3u);
	const auto sheet = particles->sheets.front().lock();
	ASSERT_NE(sheet, nullptr);
	// 0x777777, each byte times 30 over 256
	EXPECT_EQ(sheet->Colour(), 0x0D0D0Du);
	const auto expected = creature_arena_ring::Points({0.0f, 0.0f}, 20.0f, 32);
	EXPECT_EQ(sheet->Points().front().x, expected.front().x);
	EXPECT_EQ(sheet->Points()[8].z, expected[8].y);
}
