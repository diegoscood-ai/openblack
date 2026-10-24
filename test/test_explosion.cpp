/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The beam explosion: UR_ChangeScaleXYZ, UR_MoveAtom, the UR_Explosion event cadence (a SpellEvent 2 per step for
// TimeToDoEventsFor after InitialDelay), SetPSysCloseDown, and the mesh particles' draw: UsePlayerColor and FaceCamera;
// the key-point splines and ParticleGoodEvilCreator; the exploded meshes (UR_ExplodeObject and ExplodeMesh).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <memory>
#include <numbers>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/GroundMarks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "Graphics/ModelLight.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Particles/Rules/Explosion.h"
#include "Particles/Rules/KeyPoints.h"
#include "Particles/Rules/Shield.h"
#include "Particles/SpellLink.h"
#include "Resources/Resources.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

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
class RecordingSink final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		events.push_back(event);
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	std::vector<particles::SpellEventInfo> events;
};

std::shared_ptr<const particles::File> Parse(std::string_view text, const char* name)
{
	auto file = particles::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}
} // namespace

TEST(Explosion, changeScaleXYZ)
{
	// SF_BeamExplosionFX's cones: 0 -> 0.1 in XZ over 0.1 s with Y 8 (stretch = Y / XZ), then 0.1 -> 12
	float scale = 1.0f;
	float stretch = 1.0f;
	EXPECT_TRUE(particles::explosion::ChangeScaleXYZ(0.0f, 0.1f, 0.0f, 0.1f, 0.0f, 0.1f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 0.0f);
	EXPECT_FLOAT_EQ(stretch, 0.0f); // XZ <= 0.0001: no stretch
	EXPECT_TRUE(particles::explosion::ChangeScaleXYZ(0.05f, 0.1f, 0.0f, 0.1f, 0.0f, 0.1f, 8.0f, 8.0f, scale, stretch));
	EXPECT_NEAR(scale, 0.05f, 1e-6f);
	EXPECT_NEAR(stretch, 8.0f / 0.05f, 1e-2f);
	// after StopTime only the first step writes the stop values
	EXPECT_TRUE(particles::explosion::ChangeScaleXYZ(0.55f, 0.1f, 0.1f, 0.5f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 12.0f);
	EXPECT_NEAR(stretch, 8.0f / 12.0f, 1e-6f);
	scale = 3.0f;
	EXPECT_FALSE(particles::explosion::ChangeScaleXYZ(0.7f, 0.1f, 0.1f, 0.5f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
	EXPECT_FLOAT_EQ(scale, 3.0f);
	// before StartTime: nothing
	EXPECT_FALSE(particles::explosion::ChangeScaleXYZ(0.05f, 0.1f, 0.1f, 1.0f, 0.1f, 12.0f, 8.0f, 8.0f, scale, stretch));
}

TEST(Explosion, moveAtom)
{
	// SF_BeamExplosionFX's column: from 120 m up to the ground in 0.4 s
	const glm::vec3 start(0.0f, 120.0f, 0.0f);
	const glm::vec3 stop(0.0f);
	glm::vec3 p(7.0f);
	EXPECT_TRUE(particles::explosion::MoveAtom(0.0f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_FLOAT_EQ(p.y, 120.0f);
	EXPECT_TRUE(particles::explosion::MoveAtom(0.1f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_NEAR(p.y, 90.0f, 1e-4f);
	// the step that reaches StopTime lands on the stop point
	EXPECT_TRUE(particles::explosion::MoveAtom(0.3f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_NEAR(p.y, 0.0f, 1e-4f);
	p = glm::vec3(7.0f);
	EXPECT_FALSE(particles::explosion::MoveAtom(0.5f, 0.1f, 0.0f, 0.4f, false, start, stop, p));
	EXPECT_FLOAT_EQ(p.y, 7.0f);
	// MoveSmoothly: t^2 (3 - 2t)
	EXPECT_TRUE(particles::explosion::MoveAtom(0.1f, 0.01f, 0.0f, 0.4f, true, start, stop, p));
	const float t = 0.25f;
	EXPECT_NEAR(p.y, 120.0f * (1.0f - t * t * (3.0f - 2.0f * t)), 1e-3f);
}

TEST(Explosion, blastEventsCadenceAndCloseDown)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	const auto file = Parse(k_Blast, "SF_BeamExplosionTest");
	ASSERT_NE(file, nullptr);
	RecordingSink sink;
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 200.0f), 1.0f);
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
			if (event.type == particles::SpellEventInfo::Type::Point)
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
	// UsePlayerColor: white x the red player's colour (0xFF4646) at blend 1
	auto c = particles::TintWithPlayerColour({255, 255, 255, 255}, 0xFFFF4646u, 1.0f);
	EXPECT_EQ(c[0], 254); // 255 x 255 >> 8
	EXPECT_EQ(c[1], 69);
	EXPECT_EQ(c[2], 69);
	EXPECT_EQ(c[3], 254);
	// the dome's blend 0.5 (b = 127): 255 + ((70 - 255) x 127 >> 8) = 163 for green and blue
	c = particles::TintWithPlayerColour({255, 255, 255, 255}, 0xFFFF4646u, 0.5f);
	EXPECT_EQ(c[0], 254);
	EXPECT_EQ(c[1], 162); // 255 x 163 >> 8
	EXPECT_EQ(c[2], 162);
	// the neutral player's black is white
	c = particles::TintWithPlayerColour({200, 100, 50, 255}, 0xFF000000u, 1.0f);
	EXPECT_EQ(c[0], 199);
	EXPECT_EQ(c[1], 99);
	EXPECT_EQ(c[2], 49);
}

TEST(Explosion, onlyTheSpreadingDiscAndAConicalEmitterAllowedSeveralEmitMoreThanOneAtomAStep)
{
	using particles::EmitterShape;
	for (const bool allowed : {false, true})
	{
		EXPECT_FALSE(particles::EmitsSeveralAStep(EmitterShape::Simple, allowed));
		EXPECT_FALSE(particles::EmitsSeveralAStep(EmitterShape::Disk, allowed));
		EXPECT_TRUE(particles::EmitsSeveralAStep(EmitterShape::SpreadingDisk, allowed));
	}
	EXPECT_FALSE(particles::EmitsSeveralAStep(EmitterShape::Conical, false));
	EXPECT_TRUE(particles::EmitsSeveralAStep(EmitterShape::Conical, true));
}

TEST(Explosion, aBlastShattersAndRemovesUpToItsMostAndSparesTheCreaturesFromShattering)
{
	// the beam's file: at most 15 shattered and 15 removed; a thing it may destroy is counted against both
	constexpr int k_Most = 15;
	int exploded = 0;
	int deleted = 0;
	for (int i = 0; i < 20; ++i)
	{
		const auto hit = particles::explosion::HitOn(exploded, deleted, false, k_Most, k_Most);
		exploded += hit.shatter ? 1 : 0;
		deleted += hit.remove ? 1 : 0;
	}
	EXPECT_EQ(exploded, k_Most);
	EXPECT_EQ(deleted, k_Most);
	EXPECT_TRUE(particles::explosion::BlastSpent(exploded, deleted, k_Most, k_Most));
	// a creature is never shattered, though it counts as removed
	const auto creature = particles::explosion::HitOn(0, 0, true, k_Most, k_Most);
	EXPECT_FALSE(creature.shatter);
	EXPECT_TRUE(creature.remove);
	// the blast goes on while either count has room
	EXPECT_FALSE(particles::explosion::BlastSpent(k_Most, k_Most - 1, k_Most, k_Most));
	EXPECT_FALSE(particles::explosion::BlastSpent(k_Most - 1, k_Most, k_Most, k_Most));
	const auto removeOnly = particles::explosion::HitOn(k_Most, 3, false, k_Most, k_Most);
	EXPECT_FALSE(removeOnly.shatter);
	EXPECT_TRUE(removeOnly.remove);
}

TEST(Explosion, aBlastInsideAShieldStrikesItWhereTheBeamComesDownThroughIt)
{
	// a shield of 20 m round (100, 0, 100): a blast at its centre, with the beam's margin of 5, strikes the sphere grown
	// by 5 where the ray from 200 m above comes down into it, 25 m above the centre
	const particles::shields::DefensiveSphere sphere {
	    .id = 1, .owner = nullptr, .centre = {100.0f, 0.0f, 100.0f}, .radius = 20.0f};
	const glm::vec3 centre(100.0f, 0.0f, 100.0f);
	const auto impact = particles::explosion::BeamShieldImpact(sphere, centre, 5.0f);
	EXPECT_NEAR(impact.x, 100.0f, 1e-3f);
	EXPECT_NEAR(impact.y, 25.0f, 1e-3f);
	EXPECT_NEAR(impact.z, 100.0f, 1e-3f);
	// the beam's hit: at the blast's centre, no push, to the shield's spell
	const auto spell = entt::entity {42};
	const auto beam = particles::explosion::ShieldHit(centre, glm::vec3(0.0f), spell);
	EXPECT_EQ(beam.type, particles::SpellEventInfo::Type::HitSpell);
	EXPECT_EQ(beam.position, centre);
	EXPECT_EQ(beam.velocity, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(beam.strength, 1.0f);
	EXPECT_FALSE(beam.checkShields);
	EXPECT_EQ(beam.target, spell);
}

TEST(Explosion, theWaveSparksAShieldItMeetsAndPushesItOutwardFromTheBlast)
{
	// a target 10 m east of the blast, inside a shield: the hit is at the target, pushing east by the distance
	const glm::vec3 centre(100.0f, 0.0f, 100.0f);
	const glm::vec3 target(110.0f, 2.0f, 100.0f);
	const auto spell = entt::entity {7};
	const auto wave = particles::explosion::ShieldHit(target, target - centre, spell);
	EXPECT_EQ(wave.type, particles::SpellEventInfo::Type::HitSpell);
	EXPECT_EQ(wave.position, target);
	EXPECT_EQ(wave.velocity, glm::vec3(10.0f, 2.0f, 0.0f));
	EXPECT_EQ(wave.target, spell);
}

TEST(Explosion, meshFacesTheCamera)
{
	// FaceCamera: turned about Y so the frame follows the camera in x, z; Y x HeightStretch
	glm::mat3 axes(1.0f);
	graphics::billboard::ParticleYaw(axes, glm::vec3(0.0f), glm::vec3(0.0f, 50.0f, -10.0f), 2.0f);
	// d = position - camera = (0, +1) in x, z: theta = atan2(1, 0) - atan2(1, 0) = 0, unchanged
	EXPECT_NEAR(axes[0].x, 1.0f, 1e-5f);
	EXPECT_NEAR(axes[2].z, 1.0f, 1e-5f);
	EXPECT_NEAR(axes[1].y, 2.0f, 1e-5f);
	// the camera on the +x side: d = (-1, 0), theta = pi - pi / 2
	axes = glm::mat3(1.0f);
	graphics::billboard::ParticleYaw(axes, glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
	EXPECT_NEAR(axes[0].z, 1.0f, 1e-5f);  // r0' = cos r0 + sin r2 = r2
	EXPECT_NEAR(axes[2].x, -1.0f, 1e-5f); // r2' = cos r2 - sin r0 = -r0
	EXPECT_NEAR(glm::length(axes[0]), 1.0f, 1e-5f);
}

TEST(Explosion, keyPointSplines)
{
	using namespace particles::key_points;
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
	particles::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	effect.Step(0.1f);
	std::vector<particles::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, particles::Creator::Kind::Point);
	ASSERT_EQ(atoms.size(), 1u);
	// no player -> PCreatorGood, its InitialScale
	EXPECT_EQ(atoms.front().creator, effect.FindCreator("Good"));
	EXPECT_FLOAT_EQ(atoms.front().scale, 4.0f);
}

// The ground mark's dust (ECS/GroundMarks): DisappearSmoke at the point (1, 1.0, -1), mode 1, every puff growing at
// 1.5 x size per second
TEST(Explosion, groundMarkDustIsDisappearSmokeMode1)
{
	ecs::disappear_smoke::Clear();
	ecs::disappear_smoke::Create(glm::vec3(10.0f, 5.0f, 20.0f), 1, 1.0f, 0xFFFFFFFFu);
	ASSERT_EQ(ecs::disappear_smoke::Get().size(), 1u);
	const auto& cloud = ecs::disappear_smoke::Get().front();
	EXPECT_EQ(cloud.mode, 1);
	for (const auto& puff : cloud.puffs)
	{
		EXPECT_NEAR(glm::length(puff.velocity), 1.5f, 1e-4f);
	}
	ecs::disappear_smoke::Clear();
}

// A ground mark (an uprooted tree's roots pile, an explosion's crater) throws up its own white dust where it is made,
// DisappearSmoke mode 1 of size 1 with the colour -1, also when the pile's mesh is not there to be drawn
TEST(Explosion, aGroundMarkThrowsUpItsDustWhereItIsMadeEvenWithoutItsMesh)
{
	const test::RestoreService<Locator::resources> keepResources;
	Locator::resources::emplace<resources::Resources>();
	ecs::disappear_smoke::Clear();
	const glm::vec3 where(10.0f, 5.0f, 20.0f);
	EXPECT_TRUE(ecs::ground_marks::Create(where, glm::mat3(1.0f), 0.6f) == entt::null);
	ASSERT_EQ(ecs::disappear_smoke::Get().size(), 1u);
	const auto& cloud = ecs::disappear_smoke::Get().front();
	EXPECT_EQ(cloud.mode, 1);
	EXPECT_FLOAT_EQ(cloud.size, 1.0f);
	EXPECT_EQ(cloud.colour, 0xFFFFFFFFu);
	for (const auto& puff : cloud.puffs)
	{
		// each puff within the size of the point, flying out at 1.5 x the size a second
		EXPECT_LE(glm::length(puff.position - where), std::sqrt(3.0f) + 1e-4f);
		EXPECT_NEAR(glm::length(puff.velocity), 1.5f, 1e-4f);
	}
	ecs::disappear_smoke::Clear();
}

// A puff through the explosion system: the smoke is DisappearSmoke's mode 0, the dust its mode 1, in the kind's own
// colour unless given another
TEST(Explosion, aPuffOfSmokeOrDustIsDisappearSmoke)
{
	ecs::disappear_smoke::Clear();
	ecs::systems::ExplosionSystem system;
	// Through its interface, as the game calls it, where the colour may be left out
	ecs::systems::ExplosionSystemInterface& explosions = system;
	explosions.AddSmoke(glm::vec3(10.0f, 5.0f, 20.0f), 1.0f, dust_puff::Kind::Smoke);
	explosions.AddSmoke(glm::vec3(10.0f, 5.0f, 20.0f), 2.0f, dust_puff::Kind::Dust);
	explosions.AddSmoke(glm::vec3(10.0f, 5.0f, 20.0f), 1.0f, dust_puff::Kind::Smoke, 0xFFFFFFu);
	const auto& clouds = ecs::disappear_smoke::Get();
	ASSERT_EQ(clouds.size(), 3u);
	EXPECT_EQ(clouds[0].mode, 0);
	EXPECT_EQ(clouds[0].colour, 0xFFFFFFFFu);
	EXPECT_EQ(clouds[1].mode, 1);
	EXPECT_FLOAT_EQ(clouds[1].size, 2.0f);
	// A given white replaces the grey: it is not the colour that keeps it
	EXPECT_NE(clouds[2].colour, 0xFFFFFFFFu);
	EXPECT_EQ(clouds[2].colour & 0xFFFFFFu, 0xFFFFFFu);
	// The dust grows out at 1.5 x its size a second, the ground mark's dust
	for (const auto& puff : clouds[1].puffs)
	{
		EXPECT_NEAR(glm::length(puff.velocity), 3.0f, 1e-4f);
	}
	ecs::disappear_smoke::Clear();
}

namespace
{
/// A strip of `count` triangles (k, k + 1, k + 2) on zig-zag points: each one shares an edge with the one before and the
/// one after, and many edges have the same length (the qsort's ties)
particles::explode_object::SourcePrimitive Strip(int count)
{
	particles::explode_object::SourcePrimitive strip;
	for (int k = 0; k < count + 2; ++k)
	{
		strip.positions.emplace_back(static_cast<float>(k / 2), static_cast<float>(k % 2), 0.0f);
		strip.uvs.emplace_back(0.0f);
		strip.normals.emplace_back(0.0f, 0.0f, 1.0f);
	}
	for (int k = 0; k < count; ++k)
	{
		strip.triangles.push_back({static_cast<uint16_t>(k), static_cast<uint16_t>(k + 1), static_cast<uint16_t>(k + 2)});
	}
	return strip;
}

/// SF_ExplodeObject's group 0 (UR_ExplodeObject and the fragments' remove rule)
constexpr std::string_view k_ExplodeObject = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT 25
ENDPROPERTIES
BEGINCLASS UR_ExplodeObject UR_ExplodeObject0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY RandomFactor FLOAT 0.889381
PROPERTY RemoveOnCloseDown BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleOldAgeOnly RemoveRuleOldAgeOnly_Fragments
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY DieAge FLOAT 6
PROPERTY Group INTEGER 0
PROPERTY MinAtoms INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_ExplodeObject2 UR_ExplodeObject2_0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY RandomFactor FLOAT 0.889381
PROPERTY RemoveOnCloseDown BOOL 1
ENDPROPERTIES
ENDCLASS
)";
} // namespace

// ExplodeMesh: pieces walked from triangle to triangle over shared edges, MaxTrigsPerFrag (15) + 1 at most
TEST(Explosion, explodeMeshWalksSharedEdges)
{
	const auto pieces = particles::explode_object::SplitPrimitive(Strip(20), 15);
	ASSERT_EQ(pieces.size(), 2u);
	ASSERT_EQ(pieces[0].size(), 16u); // the walk adds while made <= 15
	for (uint32_t i = 0; i < 16; ++i)
	{
		EXPECT_EQ(pieces[0][i], i);
	}
	ASSERT_EQ(pieces[1].size(), 4u); // the next start is the first unused triangle
	EXPECT_EQ(pieces[1].front(), 16u);
	EXPECT_EQ(pieces[1].back(), 19u);
	// a walk ends where the last triangle has no unused neighbour: two separate triangles are two pieces
	particles::explode_object::SourcePrimitive apart = Strip(1);
	apart.positions.emplace_back(10.0f, 0.0f, 0.0f);
	apart.positions.emplace_back(11.0f, 0.0f, 0.0f);
	apart.positions.emplace_back(10.0f, 1.0f, 0.0f);
	apart.triangles.push_back({3, 4, 5});
	EXPECT_EQ(particles::explode_object::SplitPrimitive(apart, 15).size(), 2u);
	// MaxTrigsPerFrag 0: the walk stops after the first triangle (made 1 <= 0 fails): pieces of one triangle each
	EXPECT_EQ(particles::explode_object::SplitPrimitive(Strip(5), 0).size(), 5u);
	EXPECT_TRUE(particles::explode_object::SplitPrimitive({}, 15).empty());
}

// ExplodeMesh: d to the speed, the random part to |d| x RandomFactor, its Y halved
TEST(Explosion, explodeMeshPieceVelocity)
{
	const auto v = particles::explode_object::PieceVelocity(glm::vec3(3.0f, 0.0f, 4.0f), glm::vec3(0.0f), 10.0f, 0.5f,
	                                                        glm::vec3(0.0f, 2.0f, 0.0f));
	EXPECT_NEAR(v.x, 6.0f, 1e-5f);
	EXPECT_NEAR(v.y, 2.5f, 1e-5f);
	EXPECT_NEAR(v.z, 8.0f, 1e-5f);
	const auto side = particles::explode_object::PieceVelocity(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f), 10.0f, 1.0f,
	                                                           glm::vec3(0.0f, 0.0f, -3.0f));
	EXPECT_NEAR(side.y, 10.0f, 1e-5f);
	EXPECT_NEAR(side.z, -10.0f, 1e-5f); // X and Z keep their full length
	// at the origin d stays 0 and so does the random part (|d| x RandomFactor = 0)
	const auto still = particles::explode_object::PieceVelocity(glm::vec3(1.0f), glm::vec3(1.0f), 10.0f, 0.9f, glm::vec3(1.0f));
	EXPECT_FLOAT_EQ(glm::length(still), 0.0f);
}

// The mesh_pieces draw: the DrawData colour times the land light byte by byte ((c l) >> 8, alpha included); off the map
// the land light is table[255]
TEST(Explosion, explodedPieceLitColour)
{
	const glm::vec3 off(-10000.0f, 0.0f, -10000.0f);
	const uint32_t light = particles::explode_object::LandLight(off);
	EXPECT_EQ(light, land_light::CurrentTable().GetRaw(255));
	const uint32_t argb = 0xFF804020u;
	uint32_t expected = 0;
	for (const int shift : {24, 16, 8, 0})
	{
		expected |= ((((argb >> shift) & 0xFFu) * ((light >> shift) & 0xFFu)) >> 8) << shift;
	}
	EXPECT_EQ(particles::explode_object::LitColour(argb, off), expected);
	if (light == 0xFFFFFFFFu)
	{
		EXPECT_EQ(expected, 0xFE7F3F1Fu); // 255 x 255 >> 8 = 254: an opaque DrawData alpha comes out 254
	}
}

// UR_ExplodeObject: the queue emptied into one atom per piece, at its centroid in the world; only the LOD 0
// sub-meshes without status bits
TEST(Explosion, explodeObjectQueueToPieces)
{
	particles::explode_object::Clear();
	auto mesh = std::make_shared<particles::explode_object::SourceMesh>();
	mesh->subMeshes.push_back({0xE0000800u, {Strip(20)}}); // the rock's flags: LOD mask 7, status 0
	mesh->subMeshes.push_back({0x80000800u, {Strip(3)}});  // no LOD 0
	mesh->subMeshes.push_back({0xE0000810u, {Strip(3)}});  // a status bit
	const glm::vec3 position(100.0f, 10.0f, 200.0f);
	const glm::mat3 axes(2.0f); // scaled x2
	particles::explode_object::QueueMesh(mesh, axes, position, position - glm::vec3(0.0f, 5.0f, 0.0f), 10.0f, 6.0f);
	particles::explode_object::QueueMesh(nullptr, axes, position, position, 10.0f, 6.0f); // no mesh: not queued
	EXPECT_EQ(particles::explode_object::QueuedCount(), 1u);

	const auto file = Parse(k_ExplodeObject, "SF_ExplodeObjectTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	EXPECT_EQ(particles::explode_object::QueuedCount(), 0u);
	std::vector<particles::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, particles::Creator::Kind::MeshPiece);
	ASSERT_EQ(atoms.size(), 2u);
	for (const auto& atom : atoms)
	{
		ASSERT_NE(atom.atom, nullptr);
		const auto* piece = particles::explode_object::PieceOf(*atom.atom);
		ASSERT_NE(piece, nullptr);
		EXPECT_EQ(piece->source, mesh); // the source primitive
		EXPECT_EQ(piece->subMesh, 0u);
		EXPECT_EQ(piece->primitive, 0u);
		EXPECT_EQ(piece->positions.size(), 3u * piece->triangles); // three vertices per triangle
		EXPECT_EQ(piece->uvs.size(), piece->positions.size());
		EXPECT_EQ(piece->normals.size(), piece->positions.size());
	}
	EXPECT_EQ(particles::explode_object::PieceOf(*atoms[0].atom)->triangles +
	              particles::explode_object::PieceOf(*atoms[1].atom)->triangles,
	          20u);
	// the first piece (triangles 0..15 of the strip, 48 vertices) at its centroid: the strip's points k / 2, k % 2 through
	// the matrix
	glm::vec3 sum(0.0f);
	const auto strip = Strip(20);
	for (uint32_t t = 0; t < 16; ++t)
	{
		for (const auto i : strip.triangles[t])
		{
			sum += position + axes * strip.positions[i];
		}
	}
	const glm::vec3 centre = sum / 48.0f;
	const auto& first = *atoms[0].atom;
	// no movement rule in this file: the atom is still at the centroid it was made at
	EXPECT_NEAR(first.position.x, centre.x, 1e-3f);
	EXPECT_NEAR(first.position.y, centre.y, 1e-3f);
	EXPECT_NEAR(first.position.z, centre.z, 1e-3f);
	// flying away from the origin 5 m under the matrix's position, at 10 plus the random part
	EXPECT_GT(first.velocity.y, 0.0f);
	particles::explode_object::Clear();
}

namespace
{
/// A piece of `triangles` triangles of a source mesh with the id `meshId`: positions about the centroid, uvs, the
/// source normals
std::unique_ptr<particles::explode_object::Piece>
TestPiece(const std::shared_ptr<const particles::explode_object::SourceMesh>& source, uint16_t primitive, uint32_t triangles)
{
	auto piece = std::make_unique<particles::explode_object::Piece>();
	piece->source = source;
	piece->primitive = primitive;
	piece->triangles = triangles;
	for (uint32_t i = 0; i < triangles * 3; ++i)
	{
		const auto f = static_cast<float>(i);
		piece->positions.emplace_back(f - 1.0f, 0.5f * f, 2.0f - f);
		piece->uvs.emplace_back(0.1f * f, 1.0f - 0.1f * f);
		piece->normals.push_back(glm::normalize(glm::vec3(std::sin(f), 1.0f, std::cos(f))));
	}
	return piece;
}
} // namespace

// The mesh_pieces draw: the vertices through the drawn matrix into the world, each of the DrawData colour x the land light
// lit by the model light with the ambient, the alpha table when the DrawData alpha is not 0xFF
TEST(Explosion, meshPiecesBuildWorldAndLight)
{
	auto source = std::make_shared<particles::explode_object::SourceMesh>();
	source->meshId = 0x1234u;
	const auto piece = TestPiece(source, 0, 2);
	// off the map: the land light is table[255]
	particles::Effect::DrawAtom atom {
	    nullptr, glm::vec3(-10000.0f, 3.0f, -10000.0f), glm::mat3(1.0f), 2.0f, 1.5f, 255.0f, 0.0f, {0x80, 0x40, 0x20}};
	atom.rotation = glm::mat3(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const auto model = particles::mesh_pieces::DrawMatrix(atom);
	EXPECT_FLOAT_EQ(model[1][1], 3.0f); // the Y axis x the scale x the stretch
	const uint32_t argb = particles::mesh_pieces::DrawDataColour(atom);
	EXPECT_EQ(argb, 0xFF804020u);

	graphics::world_triangles::Frame frame;
	particles::mesh_pieces::AppendPiece(frame, *piece, model, argb);
	ASSERT_EQ(frame.vertices.size(), 6u);
	ASSERT_EQ(frame.batches.size(), 1u);
	EXPECT_EQ(frame.batches[0].material.meshId, 0x1234u);
	EXPECT_EQ(frame.batches[0].table, graphics::render_modes::Table::Normal);
	EXPECT_EQ(frame.batches[0].globalAlpha, 255u);
	EXPECT_EQ(frame.batches[0].firstVertex, 0u);
	EXPECT_EQ(frame.batches[0].vertexCount, 6u);
	const uint32_t base = particles::explode_object::LitColour(argb, atom.position);
	const auto light = model_light::LightInMeshSpace(model);
	for (size_t i = 0; i < 6; ++i)
	{
		const auto world = glm::vec3(model * glm::vec4(piece->positions[i], 1.0f));
		EXPECT_NEAR(frame.vertices[i].position.x, world.x, 1e-4f);
		EXPECT_NEAR(frame.vertices[i].position.y, world.y, 1e-4f);
		EXPECT_NEAR(frame.vertices[i].position.z, world.z, 1e-4f);
		EXPECT_EQ(frame.vertices[i].uv, piece->uvs[i]);
		const auto& n = piece->normals[i];
		const uint32_t lit = model_light::Apply(base, model_light::Intensity((light.z * n.z + light.y * n.y) + light.x * n.x),
		                                        model_light::k_DefaultAmbient);
		EXPECT_EQ(frame.vertices[i].abgr, graphics::world_triangles::ToAbgr(lit));
		// Apply keeps the alpha: 255 x 255 >> 8 = 254 when the land light's alpha is 0xFF
		EXPECT_EQ(frame.vertices[i].abgr >> 24, base >> 24);
	}

	// fading: the alpha table and the alpha byte for ALPHAREF
	atom.alpha = 128.0f;
	frame.Clear();
	particles::mesh_pieces::AppendPiece(frame, *piece, model, particles::mesh_pieces::DrawDataColour(atom));
	ASSERT_EQ(frame.batches.size(), 1u);
	EXPECT_EQ(frame.batches[0].table, graphics::render_modes::Table::GlobalAlpha);
	EXPECT_EQ(frame.batches[0].globalAlpha, 128u);
	EXPECT_EQ(graphics::world_triangles::ToAbgr(0x80112233u), 0x80332211u);
}

// The pieces of one source primitive come one after the other: one batch; another primitive, another alpha or another
// tag starts a new one, in order (world_triangles::Frame::Append)
TEST(Explosion, meshPiecesBatchesMerge)
{
	auto source = std::make_shared<particles::explode_object::SourceMesh>();
	source->meshId = 0x99u;
	const auto a = TestPiece(source, 0, 1);
	const auto b = TestPiece(source, 0, 3);
	const auto c = TestPiece(source, 1, 2);
	const glm::mat4 model(1.0f);
	graphics::world_triangles::Frame frame;
	particles::mesh_pieces::AppendPiece(frame, *a, model, 0xFFFFFFFFu);
	particles::mesh_pieces::AppendPiece(frame, *b, model, 0xFFFFFFFFu);
	ASSERT_EQ(frame.batches.size(), 1u);
	EXPECT_EQ(frame.batches[0].vertexCount, 12u);
	particles::mesh_pieces::AppendPiece(frame, *c, model, 0xFFFFFFFFu);
	particles::mesh_pieces::AppendPiece(frame, *a, model, 0x80FFFFFFu);
	particles::mesh_pieces::AppendPiece(frame, *a, model, 0x80FFFFFFu, &frame);
	ASSERT_EQ(frame.batches.size(), 4u);
	EXPECT_EQ(frame.batches[1].material.primitive, 1u);
	EXPECT_EQ(frame.batches[1].firstVertex, 12u);
	EXPECT_EQ(frame.batches[1].vertexCount, 6u);
	EXPECT_EQ(frame.batches[2].table, graphics::render_modes::Table::GlobalAlpha);
	EXPECT_EQ(frame.batches[2].firstVertex, 18u);
	EXPECT_EQ(frame.batches[3].tag, &frame);
	EXPECT_EQ(frame.batches[3].firstVertex, 21u);
	EXPECT_EQ(frame.vertices.size(), 24u);
	// a piece without a source or without vertices adds nothing
	particles::explode_object::Piece empty;
	particles::mesh_pieces::AppendPiece(frame, empty, model, 0xFFFFFFFFu);
	EXPECT_EQ(frame.batches.size(), 4u);
}

TEST(Explosion, aMiracleMayDestroyAnythingThatTakesEffectsButACreatureAFieldOrTheTemple)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	using namespace ecs::components;
	const auto thing = [&registry]() { return registry.Create(); };
	// any other thing that takes effects may go
	EXPECT_TRUE(particles::explosion::CanBeDestroyedBySpell(thing(), entt::null));
	const auto full = thing();
	registry.Assign<Pot>(full).amount = 10;
	EXPECT_TRUE(particles::explosion::CanBeDestroyedBySpell(full, entt::null));
	// the creature, the field and the temple's parts, its worship site and totem say no
	const auto creature = thing();
	registry.Assign<Creature>(creature);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(creature, entt::null));
	const auto field = thing();
	registry.Assign<Field>(field);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(field, entt::null));
	const auto temple = thing();
	registry.Assign<Temple>(temple);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(temple, entt::null));
	const auto part = thing();
	registry.Assign<TempleInteriorPart>(part);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(part, entt::null));
	const auto citadel = thing();
	registry.Assign<CitadelWorship>(citadel);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(citadel, entt::null));
	const auto site = thing();
	registry.Assign<WorshipSite>(site);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(site, entt::null));
	const auto totem = thing();
	registry.Assign<WorshipTotem>(totem);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(totem, entt::null));
	// nor what takes no effects: a teleport stone, a spell seed, an empty pot, a thing that is not available
	const auto stone = thing();
	registry.Assign<TeleportStone>(stone);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(stone, entt::null));
	const auto seed = thing();
	registry.Assign<SpellSeed>(seed);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(seed, entt::null));
	const auto empty = thing();
	registry.Assign<Pot>(empty).amount = 0;
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(empty, entt::null));
	const auto going = thing();
	registry.Assign<Unavailable>(going);
	EXPECT_FALSE(particles::explosion::CanBeDestroyedBySpell(going, entt::null));
}
