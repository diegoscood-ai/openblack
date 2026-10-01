/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle's butterflies (SF_Forest groups 5-7): UR_ForestPath moves each butterfly group round a sphere
// whose radius and height follow two key-point curves, and ParticleGoodEvilCreator makes butterflies or bats by the
// caster's alignment (the ParticleAnimCreator meshes, Creators/Mesh.cpp). UR_Flocking (the butterflies round their
// group) is lane m4c's. Wiki: docs/bw1-notes/miracles.md, "Explosión de rayo y clases de PSys que faltaban (M6b)" (las clases que faltaban).

#include <cmath>

#include <memory>
#include <numbers>
#include <string>

#include "ECS/Effects/Alignment.h"
#include "Enums.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Rules/KeyPoints.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::vector<float> Floats(const Object& object, std::string_view key)
{
	const auto it = object.properties.find(key);
	return it != object.properties.end() ? it->second.numbers : std::vector<float> {};
}

/// AtomData of UR_ForestPath (0x30 bytes, ctor 0x55FAC0): +0x20 first, +0x24 / +0x28 / +0x2C three PSysFloatRand(2 pi)
struct ForestPathData
{
	bool first {true};
	float theta {0.0f};
	float phi {0.0f};
	float unused {0.0f};
};

/// UR_ForestPath::ModifyAtomCollection 0x6A3770 (DefineProperties 0x6AEE60, ctor 0x6A35D0: ThetaSpeed +0x20, PhiSpeed
/// +0x24, SphereRadius +0x28 and ScaleX/Y/Z +0x30..+0x38 all 1, ScaleSphereRadius +0x2C NULL, RadiusSpline +0x3C and
/// HeightSpline +0x48 with their own default keys, both with zero end slopes)
class ForestPath final: public Modifier
{
public:
	explicit ForestPath(const Object& object)
	    : thetaSpeed(object.Float("ThetaSpeed", 1.0f))
	    , phiSpeed(object.Float("PhiSpeed", 1.0f))
	    , sphereRadius(object.Float("SphereRadius", 1.0f))
	    , radiusScale(object.String("ScaleSphereRadius"))
	    , scale(object.Float("ScaleX", 1.0f), object.Float("ScaleY", 1.0f), object.Float("ScaleZ", 1.0f))
	    // the ctor's own keys (0x6A363B..0x6A3715, four per curve) are (no portado): every spell file gives both curves
	    , radiusCurve(key_points::Make(Floats(object, "RadiusSpline")))
	    , heightCurve(key_points::Make(Floats(object, "HeightSpline")))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// the curves at the collection's age (EvalAtT 0x6A7EB0; the outputs are not initialised:
		// (aproximado) 0 here, only reached by a curve without keys)
		const float age = effect.CollectionAge(collection);
		const float r = key_points::Evaluate(radiusCurve, age, 0.0f);
		const float h = key_points::Evaluate(heightCurve, age, 0.0f);
		constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>; // the double 0x8D45D8
		for (auto& atomPtr : collection.atoms)
		{
			auto& atom = *atomPtr;
			auto& slot = atom.modifierData[this];
			if (slot == nullptr)
			{
				slot = std::make_shared<ForestPathData>();
			}
			auto& data = *std::static_pointer_cast<ForestPathData>(slot);
			if (data.first)
			{
				data.first = false;
				data.theta = effect.Random(k_TwoPi);
				data.phi = effect.Random(k_TwoPi);
				data.unused = effect.Random(k_TwoPi);
			}
			const float atomAge = effect.AtomAge(atom);
			const float theta = std::fmod(atomAge * thetaSpeed + data.theta, k_TwoPi);
			const float phi = std::fmod(atomAge * phiSpeed + data.phi, k_TwoPi);
			// R = SphereRadius x RadiusSpline (x ScaleSphereRadius's value +0xC when there is one)
			float radius = sphereRadius * r;
			if (!radiusScale.empty())
			{
				radius *= effect.FloatProvider(radiusScale, 1.0f);
			}
			glm::vec3 p = radius * glm::vec3(std::cos(theta) * scale.x * std::cos(phi), std::sin(phi) * scale.y,
			                                  std::sin(theta) * scale.z * std::cos(phi));
			// collection +0x38 bit 0 (as UR_SphereSurfaceTracer, PSys.cpp): outside a hierarchy + GetCurrentParentPos
			if (!collection.hierarchy)
			{
				p += collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
			}
			p.y += h;
			// velocity = the move x [0xD4E0F0] (1 / the step); (port guard) the step is never 0 here
			atom.velocity = (p - atom.position) * (1.0f / std::max(effect.GetDt(), 1e-4f));
			atom.position = p;
		}
		return true;
	}
	float thetaSpeed, phiSpeed, sphereRadius;
	std::string radiusScale;
	glm::vec3 scale;
	key_points::Spline radiusCurve, heightCurve;
};

/// ParticleGoodEvilCreator (DefineProperties 0x6B40C0: +0x34 PCreatorEvil, +0x38 PCreatorGood, +0x3C AlignmentSwitch;
/// ctor 0x6AA990: AlignmentSwitch -0.5). CreateParticle 0x6AAA00: the evil creator when the effect has a player whose
/// alignment (GPlayer +0x60 -> +0x08) is under AlignmentSwitch and there is one, else the good one.
struct GoodEvilCreator final: Creator
{
	std::string evil, good;
	float alignmentSwitch {-0.5f};

	[[nodiscard]] const Creator* Resolve(const Effect& effect) const override
	{
		const auto* evilCreator = effect.FindCreator(evil);
		if (effect.GetPlayer() >= 0 && evilCreator != nullptr &&
		    ecs::effects::alignment::Get(static_cast<PlayerNames>(effect.GetPlayer())) < alignmentSwitch)
		{
			return evilCreator->Resolve(effect);
		}
		const auto* goodCreator = effect.FindCreator(good);
		return goodCreator != nullptr ? goodCreator->Resolve(effect) : this;
	}
};

std::unique_ptr<Creator> MakeGoodEvilCreator(const Object& object)
{
	auto creator = std::make_unique<GoodEvilCreator>();
	ReadCreatorProperties(object, *creator);
	creator->evil = object.String("PCreatorEvil");
	creator->good = object.String("PCreatorGood");
	creator->alignmentSwitch = object.Float("AlignmentSwitch", -0.5f);
	return creator;
}
} // namespace

void openblack::psys::RegisterForestRules()
{
	RegisterModifier("UR_ForestPath", MakeModifierOf<ForestPath>);
	RegisterCreator("ParticleGoodEvilCreator", MakeGoodEvilCreator);
}
