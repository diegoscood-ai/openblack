/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

// The flock miracles (SpellFlock.cpp of the original; M4c): SpellFlock (SpellWithObjects; ctor 0x7231C0) and its two
// classes, SpellFlockFlying (AllocSpell 0x723100, 0x120 bytes: doves, or bats for an evil player) and SpellFlockGround
// (AllocSpell 0x723180, 0x110 bytes: wolves). The animals are real ones of the animals' AI (ECS/AnimalAI.h: CreateAnimal,
// MoveTo, SetStateRaw...), with what their spell classes SpellDove / SpellBat (AnimalDove.cpp) and SpellWolf
// (AnimalSpellWolf.cpp) do differently kept here: the 20-turn fade instead of dying, and the wolves' corridor.
// Research: dev\tmp_dis\miracles\resources.md §5 and impl\m4c\*.asm; wiki: docs/bw1-notes/magic.md ("Bandadas").

namespace openblack::magic
{

/// SpellFlock +0xF4..+0x10C and SpellFlockFlying +0x110..+0x11C
struct SpellFlockData
{
	entt::entity flock {entt::null}; ///< +0xF4 the Flock (components::Flock), new at InitWithPos
	int created {0};                 ///< +0xF8 the animals tried so far (a skipped one counts too)
	float emitted {0.0f};            ///< +0xFC the emit accumulator (12 per second)
	/// +0x100 / +0x104 / +0x108: the last spawn point, MapCoords x, z (6553.6 per metre, ftol) and the height above the
	/// land (metres)
	glm::ivec2 lastSpawn {0};
	float lastSpawnHeight {0.0f};
	uint32_t castPsys {0}; ///< +0x110 (flying only) SF_FlockFlyingCast*, a PSys without a spell stepped every frame
	unsigned int castTurn {0}; ///< (openblack) the game turn of InitWithPos, for OPENBLACK_TEST_FLOCK_SHOT
};

/// What the animal classes SpellDove / SpellBat (0x178 bytes) and SpellWolf (0x19C bytes) add to their Animal: the
/// fade (SpellDove +0x148..+0x174, SpellWolf +0x168..+0x194, an LH3DLib Zoomer on the alpha 0..255: value +0x148 /
/// +0x168, destination +0x14C / +0x16C) and the wolf's corridor (+0x154..+0x164) and player (+0x198)
struct SpellFlockAnimal
{
	entt::entity spell {entt::null};
	bool wolf {false};
	Zoomer fade;               ///< value 255, destination 255 at the ctor (0x41F280 / 0x420930)
	glm::vec2 normal {1.0f, 0.0f}; ///< +0x154 / +0x15C: the unit normal of the start -> destination line (x, z)
	float offset {0.0f};       ///< +0x160: -(normal . start)
	float halfWidth {0.0f};    ///< +0x164: the hunting radius (GMagicFlockGroundInfo.huntingRadius, 45 m)
	glm::vec2 destination {0.0f}; ///< +0x148: the wolf's final destination (also given to the animals' AI)
	glm::vec3 previous {0.0f}; ///< +0x2C (MapCoords): where it was at the last spell turn (the shield test's segment)
};

namespace spell_flock
{
constexpr float k_EmitPerSecond = 12.0f;    ///< GetNumToEmitPerSecond 0x7230E0 / 0x7230F0 (0x9819B0 / 0x9819B4)
constexpr float k_AngleVariation = 2.0f;    ///< GetAngleVariation 0x723550 / 0x723560 (0x9819C0 / 0x9819C4)
constexpr float k_SpawnJitter = 0.1f;       ///< 0x9819CC / 0x9819D8: GameFloatRand(0.2) - 0.1 m on x and z
constexpr float k_FlyingScale = 2.8f;       ///< 0x9819D0: GetScale() x 2.8 + GameFloatRand(3 - 2.8)
constexpr float k_FlyingScaleTop = 3.0f;    ///< 0x9819D4
constexpr float k_GroundScale = 1.5f;       ///< 0x9819DC: GetScale() x 1.5 + GameFloatRand(2 - 1.5)
constexpr float k_GroundScaleTop = 2.0f;    ///< 0x9819E0
constexpr float k_MinTravel = 10.0f;        ///< 0x8AB414: fn_00723570 halves the distance while it is above this
constexpr float k_WolfArrive = 30.0f;       ///< 0x8BF51C: SpellWolf::MoveToPos 0x421300 dies within 30 m of the end
constexpr int k_TurnsToDieOver = 20;        ///< SpellDove 0x41F620 / SpellWolf 0x420D50 GetNumTurnsToDieOver
constexpr float k_FullAlpha = 255.0f;       ///< 0x437F0000 (0x41F280 / 0x420930)
constexpr int k_MagicObjectCreated = 9;     ///< SPOT_VISUAL_TYPE of the wolves' puff (0x724625)

/// GetNumberToCreate 0x723B80 / 0x724250: fistp(numberToCreate x GetTribalPower) (the FPU rounds to nearest)
[[nodiscard]] int NumberToCreate(uint32_t numberToCreate, float tribalPower);
/// GetParticleType 0x723A30: FLOCK_FLYING_RAIN_EVIL (125) when the player's alignment < alignmentSwitch, else GOOD
/// (124); the cast effect of InitWithPos 0x723A80 is 122 / 123 the same way
[[nodiscard]] bool IsEvil(float alignment, float alignmentSwitch);
/// the flying animal of 0x723CFE: GAnimalInfo 21 SpellBat (evil) or 20 SpellDove
[[nodiscard]] AnimalInfo FlyingAnimal(float alignment, float alignmentSwitch);

/// fn_00723570's direction d (x, z): a human caster's camera forward (PSysProcessInfo +0x18 = spell +0x7C), otherwise
/// castPos - handPos; (1, 0) when |d|^2 < 0.0001 (0x8BF518)
[[nodiscard]] glm::vec2 Direction(bool human, glm::vec3 cameraForward, glm::vec3 castPos, glm::vec3 handPos);
/// fn_00723570's side: a human caster's cross product of v = normalize(spawn - castPos) with the normalised d
/// (v.z d.x - v.x d.z): +1 above 0.1 (0x8AB22C), -1 below -0.1 (0x8C9B2C); otherwise (and for the AI) +1 for an even
/// `created`, -1 for an odd one
[[nodiscard]] float Side(bool human, glm::vec2 direction, glm::vec2 spawn, glm::vec2 castPos, int created);
/// fn_00723570's angle: GetAngleVariation x created x side / GetNumberToCreate
[[nodiscard]] float Angle(int created, float side, int numberToCreate);
/// fn_00518BF0 with the Y rotation of fn_00723570 (m00 = m22 = cos, m02 = sin, m20 = -sin; a row vector):
/// (x cos - z sin, x sin + z cos)
[[nodiscard]] glm::vec2 Rotate(glm::vec2 d, float angle);
/// fn_00723570's destination for a distance: d set to that length (fn_006805F0), then the spawn point's MapCoords with
/// their 10 m cell (the high words) moved to ftol((cell x 10 + d) / 10), the sub-cell part kept. `spawn` in MapCoords.
[[nodiscard]] glm::ivec2 DestinationAt(glm::ivec2 spawn, glm::vec2 direction, float distance);
/// fn_00723570: the destination T of a new animal; from 800 m (GetDistanceToTravel) halved while T is off the map and
/// the next distance is over 10 m. `inBounds` is MapCoords::InBounds 0x6042C0. False when T is still off the map.
template <class InBounds>
bool Destination(glm::ivec2 spawn, glm::vec2 direction, float distance, InBounds inBounds, glm::ivec2& out)
{
	do
	{
		out = DestinationAt(spawn, direction, distance);
		distance *= 0.5f;
	} while (!inBounds(out) && distance > k_MinTravel);
	return inBounds(out);
}

/// The spawn loop's point between the last two hand positions (0x723D0A..0x723DA7): f = (created - prevEmit) /
/// (emit - prevEmit); x and z are old + fistp((new - old) x f), the height old + (new - old) x f
[[nodiscard]] glm::ivec2 SpawnPoint(glm::ivec2 from, glm::ivec2 to, float f);

/// MapCoords (x, z) <-> metres: ftol(x x 6553.6) (0x8AC400) and x x 10 / 65536 (0x8AA3A4)
[[nodiscard]] glm::ivec2 ToMapCoords(glm::vec2 metres);
[[nodiscard]] glm::vec2 ToMetres(glm::ivec2 mapCoords);

/// fn_00420F50 (SpellWolf, start, destination, halfWidth): the corridor's normal (DZ, -DX) / |D| of D = destination -
/// start ((1, 0) when |D|^2 < 0.0001) and its offset -(normal . start); both in metres
void SetupCorridor(SpellFlockAnimal& wolf, glm::vec2 start, glm::vec2 destination, float halfWidth);
/// SpellWolf::MoveToPos 0x421300's end: GUtils::GetDistanceInMetres 0x74CD70 (position, +0x148) < 30 -> SetDying (the
/// state itself is the animals': ECS/AnimalPredators.cpp SpellWolfMoveToPos)
[[nodiscard]] bool WolfArrived(const SpellFlockAnimal& wolf, glm::vec2 position);
/// SpellWolf::IsPosOnCorridor 0x420E10: |normal . p + offset| <= halfWidth, and the point is not more than halfWidth
/// behind the wolf along the corridor (measured from the corner of the wolf's 10 m cell, its MapCoords' high words)
[[nodiscard]] bool IsPosOnCorridor(const SpellFlockAnimal& wolf, glm::vec2 wolfPosition, glm::vec2 point);

/// SpellDove::SetDying 0x41F5C0 / SpellWolf::SetDying 0x420CF0: while the fade's destination is not 0 (once only), the Zoomer
/// (vt+0xBD4, fn_0041F2F0 / fn_00420A20 = SetDestinationWithSpeedAndTime(0, 0, t)) goes to 0 over GetNumTurnsToDieOver
/// turns (20 x 100 ms = 2 s). Never Living::SetDying: the animal keeps moving while it fades.
void StartFade(SpellFlockAnimal& animal);
/// ProcessFadeOut 0x41F4C0 / 0x420BF0 (Animal::ProcessBySpell 0x417700 from the spell's object loop 0x721040): one
/// turn of the fade; true when the alpha is exactly 0 (ToBeDeleted)
[[nodiscard]] bool ProcessFade(SpellFlockAnimal& animal);

// ---- for the animals' AI (a SpellWolf hunts inside its corridor: SpellWolf::IsHuntingTargetValid 0x420D60) ----

/// The spell data of an animal of a flock miracle, nullptr for any other
[[nodiscard]] const SpellFlockAnimal* AnimalOf(entt::entity animal);
/// IsPosOnCorridor 0x420E10 on the entities (false when `wolf` is not a flock wolf); SpellWolf::IsHuntingTargetValid
/// 0x420D60 (ECS/AnimalPredators.cpp, the prey search fn_004196D0 and Animal::HuntingMoveToPos 0x418DB0) uses it
[[nodiscard]] bool IsOnCorridor(entt::entity wolf, glm::vec2 point);
} // namespace spell_flock
} // namespace openblack::magic
