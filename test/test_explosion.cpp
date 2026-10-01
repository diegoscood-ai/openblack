/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The beam explosion (M6b): UR_ChangeScaleXYZ 0x6A5240, UR_MoveAtom 0x6A5E50, the UR_Explosion event cadence
// (0x67ECE0: a SpellEvent 2 per step for TimeToDoEventsFor after InitialDelay), SetPSysCloseDown 0x6A26D0, and the mesh
// particles' draw: UsePlayerColor (fn_006A85E0) and FaceCamera (Particle3DObj::DrawAt 0x679FD0); the key-point splines
// (KPSplineInterpolator, fn_005B3760 / EvalAtT 0x6A7EB0) and ParticleGoodEvilCreator 0x6AAA00.

#include <cmath>

#include <memory>
#include <numbers>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "ECS/SmokyStuff.h"
#include "PSys/Creators/Mesh.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/Rules/Explosion.h"
#include "PSys/Rules/KeyPoints.h"
#include "PSys/SpellLink.h"

using namespace openblack;

namespace
{
/// The control / parent / blast structure of SF_BeamExplosionSingle (group 8 -> 7 -> 6), with its UR_Explosion values
constexpr std::string_view k_Blast = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT 25
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Control
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 8
PROPERTY NextGroups ARRAY SIZE 1 7
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS SetPSysCloseDown SetPSysCloseDown_Control
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionCollectionDelay_CloseDown
PROPERTY Group INTEGER 8
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionDelay EventConditionCollectionDelay_CloseDown
BEGINPROPERTIES
PROPERTY DelayTime FLOAT 6
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_Explosion UR_Explosion0
BEGINPROPERTIES
PROPERTY BlastSpeed FLOAT 50
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 6
PROPERTY InitialDelay FLOAT 0.4
PROPERTY MaxDistance FLOAT 15
PROPERTY MaxObjectsToDelete INTEGER 15
PROPERTY MaxObjectsToExplode INTEGER 15
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR NULL_STRING
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SmokeDelay FLOAT 1.2
PROPERTY SpreadSpeed FLOAT 20
PROPERTY TimeToDoEventsFor FLOAT 5
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_ExplosionParent
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 7
PROPERTY NextGroups ARRAY SIZE 1 6
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
)";

/// Counts the events the rules send, and answers 1 (applied)
class RecordingSink final: public psys::SpellSink
{
public:
	int SpellEvent(const psys::SpellEventInfo& event) override
	{
		events.push_back(event);
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	std::vector<psys::SpellEventInfo> events;
};

std::shared_ptr<const psys::File> Parse(std::string_view text, const char* name)
{
	auto file = psys::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const psys::File>(*file) : nullptr;
}
} // namespace

TEST(Explosion, changeScaleXYZ)
{
	// SF_BeamExplosionFX's cones: 0 -> 0.1 in XZ over 0.1 s with Y 8 (stretch = Y / XZ), then 0.1 -> 12
	float scale = 1.0f;
	float stretch = 1.0f;
	EXPECT_TRUE(psys::explosion::ChangeScaleXYZ(0.0f, 0.1f, 0.0f, 0.1f, 0.0f, 0.1f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 0.0f);
	EXPECT_FLOAT_EQ(stretch, 0.0f); // XZ <= 0.0001: no stretch
	EXPECT_TRUE(psys::explosion::ChangeScaleXYZ(0.05f, 0.1f, 0.0f, 0.1f, 0.0f, 0.1f, 8.0f, 8.0f, scale, stretch));
	EXPECT_NEAR(scale, 0.05f, 1e-6f);
	EXPECT_NEAR(stretch, 8.0f / 0.05f, 1e-2f);
	// after StopTime only the first step writes the stop values
	EXPECT_TRUE(psys::explosion::ChangeScaleXYZ(0.55f, 0.1f, 0.1f, 0.5f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 12.0f);
	EXPECT_NEAR(stretch, 8.0f / 12.0f, 1e-6f);
	scale = 3.0f;
	EXPECT_FALSE(psys::explosion::ChangeScaleXYZ(0.7f, 0.1f, 0.1f, 0.5f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 3.0f);
	// before StartTime: nothing
	EXPECT_FALSE(psys::explosion::ChangeScaleXYZ(0.05f, 0.1f, 0.1f, 1.0f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
}

TEST(Explosion, moveAtom)
{
	// SF_BeamExplosionFX's column: from 120 m up to the ground in 0.4 s
	const glm::vec3 start(0.0f, 120.0f, 0.0f);
	const glm::vec3 stop(0.0f);
	glm::vec3 p(7.0f);
	EXPECT_TRUE(psys::explosion::MoveAtom(0.0f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_FLOAT_EQ(p.y, 120.0f);
	EXPECT_TRUE(psys::explosion::MoveAtom(0.1f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_NEAR(p.y, 90.0f, 1e-4f);
	// the step that reaches StopTime lands on the stop point
	EXPECT_TRUE(psys::explosion::MoveAtom(0.3f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_NEAR(p.y, 0.0f, 1e-4f);
	p = glm::vec3(7.0f);
	EXPECT_FALSE(psys::explosion::MoveAtom(0.5f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_FLOAT_EQ(p.y, 7.0f);
	// MoveSmoothly: t^2 (3 - 2t)
	EXPECT_TRUE(psys::explosion::MoveAtom(0.1f, 0.01f, 0.0f, 0.4f, true, start, stop, p));
	const float t = 0.25f;
	EXPECT_NEAR(p.y, 120.0f * (1.0f - t * t * (3.0f - 2.0f * t)), 1e-3f);
}

TEST(Explosion, blastEventsCadenceAndCloseDown)
{
	const auto file = Parse(k_Blast, "SF_BeamExplosionTest");
	ASSERT_NE(file, nullptr);
	RecordingSink sink;
	psys::Effect effect(file, glm::vec3(100.0f, 0.0f, 200.0f), 1.0f, 3);
	effect.SetSink(&sink);
	sink.events.clear(); // the start event
	int steps = 0;
	int firstPoint = -1;
	int points = 0;
	for (; steps < 120 && !effect.Closing(); ++steps)
	{
		effect.Step(0.1f);
		for (const auto& event : sink.events)
		{
			if (event.type == psys::SpellEventInfo::Point)
			{
				++points;
				if (firstPoint < 0)
				{
					firstPoint = steps;
				}
				EXPECT_FLOAT_EQ(event.position.x, 100.0f);
				EXPECT_FLOAT_EQ(event.position.z, 200.0f);
				EXPECT_FLOAT_EQ(event.strength, 1.0f);
				EXPECT_FALSE(event.checkShields);
			}
		}
		sink.events.clear();
	}
	// one blast event per step from after InitialDelay (0.4 s) until InitialDelay + TimeToDoEventsFor (5.4 s)
	EXPECT_GE(firstPoint, 3);
	EXPECT_LE(firstPoint, 6);
	EXPECT_GE(points, 48);
	EXPECT_LE(points, 51);
	// SetPSysCloseDown after the control collection's 6 s delay
	EXPECT_TRUE(effect.Closing());
	EXPECT_GE(steps, 59);
	EXPECT_LE(steps, 62);
}

TEST(Explosion, playerColourTint)
{
	// fn_006A85E0: white x the red player's colour (0xFF4646) at blend 1
	auto c = psys::TintWithPlayerColour({255, 255, 255, 255}, 0xFFFF4646u, 1.0f);
	EXPECT_EQ(c[0], 254); // 255 x 255 >> 8
	EXPECT_EQ(c[1], 69);
	EXPECT_EQ(c[2], 69);
	EXPECT_EQ(c[3], 254);
	// the dome's blend 0.5 (b = 127): 255 + ((70 - 255) x 127 >> 8) = 163 for green and blue
	c = psys::TintWithPlayerColour({255, 255, 255, 255}, 0xFFFF4646u, 0.5f);
	EXPECT_EQ(c[0], 254);
	EXPECT_EQ(c[1], 162); // 255 x 163 >> 8
	EXPECT_EQ(c[2], 162);
	// the neutral player's black is white
	c = psys::TintWithPlayerColour({200, 100, 50, 255}, 0xFF000000u, 1.0f);
	EXPECT_EQ(c[0], 199);
	EXPECT_EQ(c[1], 99);
	EXPECT_EQ(c[2], 49);
}

TEST(Explosion, meshFacesTheCamera)
{
	// Particle3DObj::DrawAt with FaceCamera: turned about Y so the frame follows the camera in x, z; Y x HeightStretch
	glm::mat3 axes(1.0f);
	graphics::billboard::ParticleYaw(axes, glm::vec3(0.0f), glm::vec3(0.0f, 50.0f, -10.0f), 2.0f);
	// d = position - camera = (0, +1) in x, z: theta = atan2(1, 0) - atan2(1, 0) = 0, unchanged
	EXPECT_NEAR(axes[0].x, 1.0f, 1e-5f);
	EXPECT_NEAR(axes[2].z, 1.0f, 1e-5f);
	EXPECT_NEAR(axes[1].y, 2.0f, 1e-5f);
	// the camera on the +x side: d = (-1, 0), theta = pi - pi / 2
	axes = glm::mat3(1.0f);
	graphics::billboard::ParticleYaw(axes, glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
	EXPECT_NEAR(axes[0].z, 1.0f, 1e-5f); // r0' = cos r0 + sin r2 = r2
	EXPECT_NEAR(axes[2].x, -1.0f, 1e-5f); // r2' = cos r2 - sin r0 = -r0
	EXPECT_NEAR(glm::length(axes[0]), 1.0f, 1e-5f);
}

TEST(Explosion, keyPointSplines)
{
	using namespace psys::key_points;
	// two keys with zero end slopes (every rule's ctor sets the flag): the Hermite smoothstep 3t^2 - 2t^3
	const auto clamped = Make({0.0f, 0.0f, 1.0f, 1.0f});
	EXPECT_NEAR(Evaluate(clamped, 0.5f, -1.0f), 0.5f, 1e-5f);
	EXPECT_NEAR(Evaluate(clamped, 0.25f, -1.0f), 0.15625f, 1e-5f);
	// without the flag (1e30 slopes) the spline is natural: a line through two keys
	const auto natural = Make({0.0f, 0.0f, 1.0f, 1.0f}, false);
	EXPECT_NEAR(Evaluate(natural, 0.25f, -1.0f), 0.25f, 1e-5f);
	// SF_HealChakraPU's UR_KPStretchHeight keys: through every key, flat at the ends
	const auto heal = Make({0.0f, 0.1f, 2.0f, 1.0f, 6.0f, 0.1f});
	EXPECT_NEAR(Evaluate(heal, 0.0f, -1.0f), 0.1f, 1e-5f);
	EXPECT_NEAR(Evaluate(heal, 2.0f, -1.0f), 1.0f, 1e-5f);
	EXPECT_NEAR(Evaluate(heal, 6.0f, -1.0f), 0.1f, 1e-5f);
	EXPECT_NEAR(Evaluate(heal, 0.01f, -1.0f), 0.1f, 1e-3f);
	EXPECT_GT(Evaluate(heal, 3.0f, -1.0f), 0.1f);
	// fewer than two keys: the caller's value stays
	EXPECT_FLOAT_EQ(Evaluate(Make({1.0f, 2.0f}), 0.5f, 7.0f), 7.0f);
	// the odd last value is dropped
	EXPECT_EQ(Make({0.0f, 1.0f, 2.0f}).keys.size(), 1u);
}

TEST(Explosion, goodEvilCreatorWithoutAPlayerIsGood)
{
	constexpr std::string_view k_Creators = R"(BEGINPROPERTIES
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ENDPROPERTIES
BEGINCLASS ParticlePointCreator Good
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 4
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator Evil
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleGoodEvilCreator Choice
BEGINPROPERTIES
PROPERTY PCreatorEvil PERSIS_PNTR Evil
PROPERTY PCreatorGood PERSIS_PNTR Good
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom Make
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR Choice
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
)";
	const auto file = Parse(k_Creators, "SF_GoodEvilTest");
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f, 5);
	effect.Step(0.1f);
	effect.Step(0.1f);
	std::vector<psys::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, psys::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 1u);
	// CreateParticle 0x6AAA00: no player -> PCreatorGood, its InitialScale
	EXPECT_EQ(atoms.front().creator, effect.FindCreator("Good"));
	EXPECT_FLOAT_EQ(atoms.front().scale, 4.0f);
}

// The ground mark's dust (fn_00825240 0x8252EB, ECS/GroundMarks): SmokyStuff::Create(point, 1, 1.0, -1), mode 1, every
// puff at 1.5 x size per second (0x823DA7)
TEST(Explosion, groundMarkDustIsSmokyStuffMode1)
{
	ecs::smoky_stuff::Clear();
	ecs::smoky_stuff::Create(glm::vec3(10.0f, 5.0f, 20.0f), 1, 1.0f, 0xFFFFFFFFu);
	ASSERT_EQ(ecs::smoky_stuff::Get().size(), 1u);
	const auto& cloud = ecs::smoky_stuff::Get().front();
	EXPECT_EQ(cloud.mode, 1);
	for (const auto& puff : cloud.puffs)
	{
		EXPECT_NEAR(glm::length(puff.velocity), 1.5f, 1e-4f);
	}
	ecs::smoky_stuff::Clear();
}
