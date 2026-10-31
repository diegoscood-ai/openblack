/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_ManaPathNew (SF_ManaPathNew, a worship site's mana path and the hand's past the border): every mote of prayer
// power given to the effect becomes a spark in its colour, which runs from where it was let out to where it goes,
// weaving from side to side a little above the land, and goes once it has arrived. Wiki:
// docs/wiki/physics/particle-rules.md#the-mana-path.

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Locator.h"
#include "Particles/ManaPathMaths.h"
#include "Particles/Noise.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// A collection holding this many sparks makes no more: the motes given meanwhile are dropped
constexpr size_t k_MostSparks = 100;
/// Each spark draws its weave's frequency and size in 0..0.8, and where along the noise it starts in 0..1
constexpr float k_WeaveDraw = 0.8f;
constexpr float k_PhaseDraw = 1.0f;

/// The rule's data on an atom: its path, and how far along it the last step left it
struct ManaPathData
{
	mana_path::Path path;
	float progress {0.0f};
};

mana_path::Settings ReadSettings(const Object& object)
{
	// the rule's own defaults, for a file that leaves a property out
	return {
	    .speed = object.Float("GlowPosSpeed", 0.2f),
	    .timeToTravel = object.Float("TimeToTravel", 2.0f),
	    .noiseFrequency = object.Float("NoiseFrequency", 1.0f),
	    .noiseAmplitude = object.Float("NoiseAmplitude", 1.0f),
	    .height = object.Float("Height", 0.3f),
	    .constantSpeed = object.Bool("UseConstantSpeed", true),
	};
}

float LandHeight(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// UR_ManaPathNew, a create rule. GlowLength and Gain are read by the editor only: the rule never uses them
class ManaPathNew final: public Modifier
{
public:
	explicit ManaPathNew(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , settings(ReadSettings(object))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// no PCreator: the rule detaches
		const Creator* made = creator.empty() ? nullptr : effect.FindCreator(creator);
		if (made == nullptr)
		{
			return false;
		}
		// every mote given, the last one first; one that finds the collection full is dropped
		while (const auto point = effect.TakeSpellPoint())
		{
			if (collection.atoms.size() < k_MostSparks)
			{
				NewSpark(effect, collection, *made, *point);
			}
		}
		// every spark of the collection, the new ones too: one that had arrived by its last step goes, the others step
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			auto& data = DataOf(atom);
			if (data.progress > 1.0f)
			{
				effect.NotifyAtomRemoved(atom);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			const auto step = mana_path::Step(data.path, settings, effect.AtomAge(atom), noise::SignedValueNoise, LandHeight);
			data.progress = step.progress;
			atom.position = step.position;
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	mana_path::Settings settings;

private:
	/// The data of this rule on the atom (found, or new)
	ManaPathData& DataOf(Atom& atom) const { return AtomDataOf<ManaPathData>(atom, this); }

	/// A new atom in the mote's colour with the creator's alpha, its path worked out with its own three draws and its
	/// sprite turned along it. A path shorter than a centimetre across the ground makes no spark: the atom goes at once
	void NewSpark(Effect& effect, Collection& collection, const Creator& made, const mana_path::Spark& point) const
	{
		auto& atom = effect.NewAtom(collection, &made, nextGroups);
		const float frequency = effect.Random(k_WeaveDraw);
		const float amplitude = effect.Random(k_WeaveDraw);
		const float phase = effect.Random(k_PhaseDraw);
		auto& data = DataOf(atom);
		data.path = mana_path::MakePath(point.from, point.to, settings,
		                                {.frequency = frequency, .amplitude = amplitude, .phase = phase});
		atom.colour[0] = static_cast<uint8_t>(point.rgb >> 16);
		atom.colour[1] = static_cast<uint8_t>(point.rgb >> 8);
		atom.colour[2] = static_cast<uint8_t>(point.rgb);
		atom.rotation = affine::AngleY(mana_path::Heading(data.path));
		if (std::abs(data.path.length) < mana_path::k_ShortestPath)
		{
			effect.NotifyAtomRemoved(atom);
			std::erase_if(collection.atoms, [&atom](const auto& held) { return held.get() == &atom; });
		}
	}
};
} // namespace

void openblack::particles::RegisterManaPathRules()
{
	RegisterModifier("UR_ManaPathNew", MakeModifierOf<ManaPathNew>);
}
