/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

#include "PSys/SoundAction.h"

// The sounds of the particle system (the miracles' loops, bangs and thunder): each is a PSysSound tied to the atom that
// started it, played from the spells.sad anim effect table (GAudio::SamplePlayAnimEffect 0x42A4B0 on the 16 channels,
// audio milestone B5) and kept alive or released once per game turn. Wiki: docs/bw1-notes/particles.md, "Sonido de las
// partículas", and docs/bw1-notes/audio.md.

namespace openblack::psys
{
class Effect;
struct Atom;
} // namespace openblack::psys

namespace openblack::audio
{

/// PSysSound (0x40 bytes, vtable 0x93A468; ctor fn_006D0F70): in the global list 0xD4EE70 and in its atom's list
struct PSysSound
{
	psys::SoundAction action;         ///< +0x1C (copied; the attribute slots are filled in by StartSound)
	float delay {0.0f};               ///< +0x34 seconds left before a Delayed sound starts
	const psys::Atom* atom {nullptr}; ///< +0x38, null once the atom stopped it or is gone
	bool released {false};            ///< +0x3C the release has been sent
	glm::vec3 position {0.0f};        ///< PSysSound::Get3DSoundPos 0x6D1000: the atom's last drawn position
	/// The channels' owner (LH_SamplePlayOptions +0x20 = this PSysSound): audio::Owner::Object(owner), registered with
	/// audio::RegisterObject for its Get3DSoundPos while the sound lives
	uint32_t owner {0};
};

namespace spell_sounds
{
/// AtomCore::StartSound 0x6745D0. Nothing for NO_SOUND; the slots come from the action (and the atom: surface with
/// USESURFACE), the sound goes to the front of the atom's list; played now at the camera distance, or distance / 347 s
/// later with the Delayed flag.
void StartSound(const psys::Effect& effect, psys::Atom& atom, const psys::SoundAction& action);
/// AtomCore::StopSound 0x674500: out of the atom's list, atom cleared; the turn update then releases it
void StopSound(psys::Atom& atom, PSysSound& sound);
/// AtomCore::StopAllSounds 0x6747E0
void StopAllSounds(psys::Atom& atom);
/// AtomCore::GetSoundOfAction 0x674550: the newest sound of that action on the atom, or null
[[nodiscard]] PSysSound* GetSoundOfAction(const psys::Atom& atom, int32_t action);

/// fn_006D11A0 (PSysGlobal::GameLoopEnd 0x68F5B0 -> fn_006D0EA0), once per game turn after the effects stepped: a live
/// looping sound is re-issued within 1200 of the camera (the bank does nothing if it is already playing), a Delayed one
/// starts when its delay runs out; once the atom is gone the sound fades by FadeStep per turn, is released once (loop
/// released with SOFTRELEASE, else stopped) and is deleted when it no longer plays.
void ProcessTurn(float turnSeconds);
/// Stops and forgets every sound (a new map)
void Clear();
[[nodiscard]] size_t Count();

// The size class (attribute 0: 1 large, 2 medium, 3 small) the rules put in the action before StartSound
/// CreateRuleAnAtom::ModifyAtomCollection 0x69F410 with a SoundRadiusFP (defaults: small 200, medium 500)
[[nodiscard]] int32_t SizeFromRadius(float radius, float small, float medium);
/// CreateWithInitialDirection 0x69E950: the throw speed fraction (0..1)
[[nodiscard]] int32_t SizeFromThrow(float fraction);
/// UpdateRuleGravityWithFloor impact fn_006A1630: the impact speed against ImpactSpeedMedium / Large
[[nodiscard]] int32_t SizeFromImpactSpeed(float speed, float medium, float large);
} // namespace spell_sounds

} // namespace openblack::audio
