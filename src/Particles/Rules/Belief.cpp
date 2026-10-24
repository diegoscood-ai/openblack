/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_BeliefSprite (SF_BeliefSprite, the belief utility effect): every belief sprite the towns queued becomes a sparkle
// in the believed player's colour, which rises, wiggles, pulses and fades. Wiki:
// docs/wiki/physics/particle-rules.md#the-belief-sprites.

#include <cstdint>

#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Services/SpellSounds.h"
#include "ECS/Town/TownBelief.h"
#include "Particles/BeliefSpriteMaths.h"
#include "Particles/Noise.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
namespace maths = particles::maths;

/// The rule's data on an atom: where the sprite started and the alpha it started with
struct BeliefData
{
	glm::vec3 start {0.0f};
	uint8_t alpha {0};
};

maths::BeliefSpriteRule ReadRule(const Object& object)
{
	const maths::BeliefSpriteRule defaults;
	return {
	    .riseSpeed = object.Float("RiseSpeed", defaults.riseSpeed),
	    .maxWiggleOffset = object.Float("MaxWiggleOffset", defaults.maxWiggleOffset),
	    .ageAtMaxOffset = object.Float("AgeAtMaxOffset", defaults.ageAtMaxOffset),
	    .noiseFreq = object.Float("NoiseFreq", defaults.noiseFreq),
	    .scaleMax = object.Float("ScaleMax", defaults.scaleMax),
	    .scaleMin = object.Float("ScaleMin", defaults.scaleMin),
	    .scaleFreq = object.Float("ScaleFreq", defaults.scaleFreq),
	    .fadeTimeStart = object.Float("FadeTimeStart", defaults.fadeTimeStart),
	    .fadeTimeEnd = object.Float("FadeTimeEnd", defaults.fadeTimeEnd),
	    .alphaMin = object.Int("AlphaMin", defaults.alphaMin),
	    .alphaMax = object.Int("AlphaMax", defaults.alphaMax),
	    .valueAlphaMin = object.Int("ValueAlphaMin", defaults.valueAlphaMin),
	    .valueAlphaMax = object.Int("ValueAlphaMax", defaults.valueAlphaMax),
	};
}

/// UR_BeliefSprite, a create rule. DieAge and UseLinearNoise are read by the editor only: the rule never uses them
class BeliefSprite final: public Modifier
{
public:
	explicit BeliefSprite(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , rule(ReadRule(object))
	    , sound(ReadSoundAction(object, "SoundOfBelief"))
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
		// every queued sprite, the last one first
		while (const auto sprite = ecs::town_belief::PopBeliefSprite())
		{
			NewSprite(effect, collection, *made, *sprite);
		}
		// every atom of the collection, the new ones too
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto& data = DataOf(atom);
			const float age = effect.AtomAge(atom);
			atom.position =
			    maths::BeliefSpritePosition(rule, data.start, age, static_cast<uint8_t>(atom.random), noise::SignedValueNoise);
			atom.colour[3] = maths::BeliefSpriteAlpha(rule, data.alpha, age);
			// only a sprite pulses
			if (atom.creator != nullptr && atom.creator->kind == Creator::Kind::Sprite)
			{
				atom.stretch = maths::BeliefSpriteStretch(rule, age, static_cast<uint8_t>(atom.random));
			}
			if (maths::BeliefSpriteExpired(rule, age))
			{
				effect.NotifyAtomRemoved(atom);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	maths::BeliefSpriteRule rule;
	SoundAction sound; ///< SoundOfBelief

private:
	/// The data of this rule on the atom (found, or new)
	BeliefData& DataOf(Atom& atom) const { return AtomDataOf<BeliefData>(atom, this); }

	/// A new atom at the sprite's point, its start alpha from the belief it shows, the sprite's colour with the creator's
	/// alpha, rule scale 1; the sound when it is worth more than ValueAlphaMin
	void NewSprite(Effect& effect, Collection& collection, const Creator& made,
	               const ecs::town_belief::BeliefSprite& sprite) const
	{
		auto& atom = effect.NewAtom(collection, &made, nextGroups);
		auto& data = DataOf(atom);
		data.start = sprite.position;
		data.alpha = maths::BeliefSpriteStartAlpha(rule, sprite.amount);
		atom.colour[0] = static_cast<uint8_t>(sprite.colour >> 16);
		atom.colour[1] = static_cast<uint8_t>(sprite.colour >> 8);
		atom.colour[2] = static_cast<uint8_t>(sprite.colour);
		atom.position = sprite.position;
		atom.ruleScale = 1.0f;
		if (maths::BeliefSpritePlaysSound(rule, sprite.amount))
		{
			audio::spell_sounds::StartSound(effect, atom, sound);
		}
	}
};
} // namespace

void openblack::particles::RegisterBeliefRules()
{
	RegisterModifier("UR_BeliefSprite", MakeModifierOf<BeliefSprite>);
}
