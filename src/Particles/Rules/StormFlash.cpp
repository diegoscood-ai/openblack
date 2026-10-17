/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EmitterRuleLightningSprite: the lightning sprites of the storm held in the hand, and the flash each one lights its
// cloud with. Wiki: docs/wiki/physics/particle-rules.md#the-storm-in-the-hands-flashes.

#include "StormFlash.h"

#include <string>
#include <vector>

#include "3D/MapCoords.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack::psys;

bool storm_flash::ShouldEmit(const Emission& emission, State& state, float age, float dt, size_t atomCount,
                             const std::function<float(float)>& random)
{
	if (state.first)
	{
		state.first = false;
		state.nextTime = age;
	}
	if (emission.maxTotal != -1 && state.emitted >= emission.maxTotal)
	{
		return false;
	}
	// due once age + dt passes the due time (a NaN never does)
	if (!(age + dt > state.nextTime))
	{
		return false;
	}
	// any negative MaxAtoms is no limit
	if (emission.maxAtoms >= 0 && atomCount > static_cast<size_t>(emission.maxAtoms))
	{
		return false;
	}
	const float wait = 1.0f / emission.frequency;
	state.nextTime += emission.randomise ? (random(0.75f) + 0.25f) * wait : wait;
	++state.emitted;
	return true;
}

uint32_t storm_flash::FlashSpecular(float elapsed, float specLife)
{
	const auto v = static_cast<uint32_t>(map_coords::FtoL((1.0f - elapsed / specLife) * 255.0f)) & 0xFFu;
	const uint32_t alphaBlue = (v * 255u) >> 8u;
	const uint32_t redGreen = (v * 200u) >> 8u;
	return (alphaBlue << 24u) | (redGreen << 16u) | (redGreen << 8u) | alphaBlue;
}

std::optional<uint32_t> storm_flash::FlashStep(State& state, float age, float specLife)
{
	if (!state.striking)
	{
		return std::nullopt;
	}
	const float elapsed = age - state.strikeAge;
	if (elapsed > specLife)
	{
		state.striking = false;
		return k_FlashOff;
	}
	return FlashSpecular(elapsed, specLife);
}

namespace
{
/// EmitterRuleLightningSprite: at most one sprite a step, as ShouldEmit says, each starting a flash that lights the
/// parent atom (the cloud) with its specular for SpecLife seconds
class LightningSprite final: public Modifier
{
public:
	explicit LightningSprite(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , specLife(object.Float("SpecLife", 0.5f))
	    , emission {
	          .frequency = object.Float("EmissionFreq", 0.001f),
	          .maxAtoms = object.Int("MaxAtoms", -1),
	          .maxTotal = object.Int("MaxTotalAtomsToEmit", -1),
	          .randomise = object.Bool("Randomise", true),
	      }
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// without a creator it detaches before anything else
		const auto* sprite = effect.FindCreator(creator);
		if (sprite == nullptr)
		{
			return false;
		}
		auto& state = CollectionDataOf<storm_flash::State>(collection, this);
		const float age = effect.CollectionAge(collection);
		const auto random = [&effect](float max) { return effect.Random(max); };
		if (storm_flash::ShouldEmit(emission, state, age, effect.GetDt(), collection.atoms.size(), random))
		{
			state.striking = true;
			state.strikeAge = age;
			effect.NewAtom(collection, sprite, nextGroups);
		}
		if (const auto specular = storm_flash::FlashStep(state, age, specLife);
		    specular.has_value() && collection.parent != nullptr)
		{
			collection.parent->specular = *specular;
		}
		return true;
	}
	std::string creator;
	std::vector<int> nextGroups;
	float specLife;
	storm_flash::Emission emission;
};
} // namespace

void openblack::psys::RegisterStormFlashRules()
{
	RegisterModifier("EmitterRuleLightningSprite", MakeModifierOf<LightningSprite>);
}
