/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The appearance rules that start and stop an atom's sounds (run per atom). The sounds themselves:
// Audio/Services/SpellSounds.h.

#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Audio/Services/SpellSounds.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/SoundAction.h"

using namespace openblack::particles;
namespace spell_sounds = openblack::audio::spell_sounds;

namespace
{
/// StartStopSoundOnCondition: SoundCondition true (or none) -> the sound once (unless the
/// atom already has one of that action); false -> that sound stopped, fading by FadeStep
class StartStopSoundOnCondition final: public Modifier
{
public:
	explicit StartStopSoundOnCondition(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , fadeStep(object.Int("FadeStep", 0))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom))
		{
			if (auto* playing = spell_sounds::GetSoundOfAction(atom, sound.action); playing != nullptr)
			{
				if (fadeStep != 0)
				{
					playing->action.fadeStep = fadeStep;
				}
				spell_sounds::StopSound(atom, *playing);
			}
			return true;
		}
		if (spell_sounds::GetSoundOfAction(atom, sound.action) == nullptr)
		{
			spell_sounds::StartSound(effect, atom, sound);
		}
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	int fadeStep;
};

/// AddSoundToAtom: once per atom, when its age reaches Delay and SoundCondition holds (or there is none);
/// StopOtherSoundsFirst stops the atom's sounds first. With DoCameraShake the camera then shakes up and down while within
/// CameraShakeRadius of the atom, at strength 1, for CameraShakeDuration seconds.
class AddSoundToAtom final: public Modifier
{
public:
	explicit AddSoundToAtom(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , stopOthers(object.Bool("StopOtherSoundsFirst", false))
	    , delay(object.Float("Delay", 0.0f))
	    , cameraShake(object.Bool("DoCameraShake", false))
	    , shakeRadius(object.Float("CameraShakeRadius", 0.0f))
	    , shakeSeconds(object.Float("CameraShakeDuration", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& done = atom.data[this].x; // the rule's per-atom data
		if (done != 0.0f || effect.AtomAge(atom) < delay)
		{
			return true;
		}
		if (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom))
		{
			return true;
		}
		if (stopOthers)
		{
			spell_sounds::StopAllSounds(atom);
		}
		spell_sounds::StartSound(effect, atom, sound);
		done = 1.0f;
		if (cameraShake)
		{
			if (!openblack::Locator::explosionSystem::has_value())
			{
				std::fputs("AddSoundToAtom: the explosion system is missing (Locator::explosionSystem)\n", stderr);
				std::abort();
			}
			const auto at = effect.GlobalPosition(atom);
			openblack::Locator::explosionSystem::value().AddShake(at, shakeRadius, 1.0f, shakeSeconds, true);
			if (openblack::debug_env::TraceLogs(openblack::debug_env::ParticleSoundTrace(), "audio"))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} shakes the camera within {:.0f} for {:.2f} s",
				                   effect.GetFile().name, shakeRadius, shakeSeconds);
			}
		}
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	bool stopOthers;
	float delay;
	bool cameraShake;
	float shakeRadius;
	float shakeSeconds;
};

/// RemoveSoundFromAtom: once per atom, when SoundCondition holds (or there is none): the
/// atom's sound of that action stopped, fading by FadeStep
class RemoveSoundFromAtom final: public Modifier
{
public:
	explicit RemoveSoundFromAtom(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , fadeStep(object.Int("FadeStep", 0))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& done = atom.data[this].x;
		if (done != 0.0f || (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom)))
		{
			return true;
		}
		if (auto* playing = spell_sounds::GetSoundOfAction(atom, sound.action); playing != nullptr)
		{
			if (fadeStep != 0)
			{
				playing->action.fadeStep = fadeStep;
			}
			spell_sounds::StopSound(atom, *playing);
		}
		done = 1.0f;
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	int fadeStep;
};
} // namespace

void openblack::particles::RegisterSoundRules()
{
	RegisterModifier("StartStopSoundOnCondition", MakeModifierOf<StartStopSoundOnCondition>);
	RegisterModifier("AddSoundToAtom", MakeModifierOf<AddSoundToAtom>);
	RegisterModifier("RemoveSoundFromAtom", MakeModifierOf<RemoveSoundFromAtom>);
}
