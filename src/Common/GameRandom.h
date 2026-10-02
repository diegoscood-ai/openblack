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

#include <functional>
#include <source_location>

#include <glm/vec3.hpp>

/// GRand (GData +8 / +0xC), PSysManager's pointers and the CRT rand() of runblack.exe, one stream each as the
/// original (docs/bw1-notes/engine-math.md "Números aleatorios").
///
/// The arithmetic is float, one operation per statement: the game runs its FPU at 24 bits (fn_007DEE00, and 0xFCFF at
/// 0x7DEE0D), so every x87 fmul / fadd rounds as a float operation does, and one operation per statement keeps the
/// compiler from contracting them (FMA).
namespace openblack::game_random
{
constexpr uint32_t k_InitialSeed = 0x88F89F;  ///< GGame::Init 0x54F4AF (local 0x54F4B4, synced 0x54F4BA)
constexpr uint32_t k_FloatRandRange = 0xFFFF; ///< LHRand(0xFFFF): 0x510714, 0x6DE5B9, 0x672ABD, 0x672B13
constexpr uint32_t k_FloatRandScaleBits = 0x37800080u; ///< [0x8D6050] = [0xD4E0B8] (0x672A94), about 1/65535
constexpr uint32_t k_CrtRandomScaleBits = 0x38000100u; ///< [0x9A3700] (0x81D18E), about 1/32767
/// [0x8D6050] = [0xD4E0B8] as a float (bit_cast, never a decimal literal)
[[nodiscard]] float FloatRandScale() noexcept;
/// [0x9A3700] as a float
[[nodiscard]] float CrtRandomScale() noexcept;

/// _LHRand 0x7DB600: s = ror32(s * 9377 + 0x24DF, 13), stored rotated (0x7DB629); returns s % n (unsigned div,
/// 0x7DB62B). n != 0: the original divides by zero (every caller tests it first)
uint32_t LHRand(uint32_t n, uint32_t& seed) noexcept;
/// GData::FloatRand 0x5106B0's body on a given seed (fn_00590DF0's lattice, tests): x == 0 or NaN -> 0 without a draw
/// (0x5106F6..0x510701), else (u x x) x k (0x51072A..0x510736)
float FloatRand(float x, uint32_t& seed) noexcept;

struct Seeds
{
	uint32_t synced {0}; ///< GData +8 = g_game +0x205A38
	uint32_t local {0};  ///< GData +0xC = g_game +0x205A3C
};
/// GGame::Init 0x54F4AF..0x54F4BA: both 0x88F89F (the start of a new campaign)
void Init() noexcept;
/// GData::Reset 0x510750 (ResetState 0x5557A0 <- ClearMap 0x552BB0 at 0x552E62 <- StartPlaygroundGame at 0x552F4F:
/// every LOAD_MAP and the skirmish playground): both 0
void Reset() noexcept;
/// WriteSafe(GData&) 0x563440: +8 (0x56345C) then +0xC (0x563494). (openblack has no saved games yet: no caller)
[[nodiscard]] Seeds Save() noexcept;
/// ReadSafe(GData&) 0x563620, the same order (+8 first, 0x563633)
void Load(const Seeds& seeds) noexcept;
[[nodiscard]] const Seeds& Current() noexcept;

/// GRand::GameRand 0x6DE510 -> GData::Rand 0x510650: 0 for 0 without a draw (0x510693), else LHRand(n, synced).
/// `where` is the original's __FILE__, __LINE__ (only for the trace, OPENBLACK_TRACE_GAME_RAND=1)
uint32_t GameRand(uint32_t n, std::source_location where = std::source_location::current()) noexcept;
/// GRand::GameFloatRand 0x6DE530 -> 0x5106B0: 0 for +-0 / NaN (0x6DE53C, 0x5106FE), else (u x x) x k, u =
/// LHRand(0xFFFF): signed like x, |r| <= 65534/65535 |x|
float GameFloatRand(float x, std::source_location where = std::source_location::current()) noexcept;
/// 0x5E1CE0 (the callback [0xEEA380], set by GLandAlignement::Open 0x5E1D4E): GameFloatRand(b - a) + a, b - a rounded
/// to a float first (fstp)
float GameFloatRange(float a, float b, std::source_location where = std::source_location::current()) noexcept;
/// GRand::LocalRand 0x6DE570 (long n, unsigned div): 0 for 0 (0x6DE574), else LHRand(n, local) (0x6DE587)
uint32_t LocalRand(int32_t n) noexcept;
/// GRand::LocalFloatRand 0x6DE590: as GameFloatRand on the local seed (0x6DE59F, 0x6DE5BE..0x6DE5DA)
float LocalFloatRand(float x) noexcept;

namespace psys
{
/// PSysInterface::NET_GAME_TYPE, the 6th argument of GJPSysInterface::Create 0x68F2F0; +0xAC = (type == 1)
/// (cmp ecx,1; sete at 0x68F3AE)
enum class NetGameType : uint8_t
{
	Local = 0,
	Synced = 1
};
/// fn_00673340: the effect's step sets [0xD4E0C0] / [0xD4E0BC] by +0xAC (0x673346..0x673371) and, at its end, puts
/// back the "0" ones (0x67349B / 0x6734A5), whatever was set before (no nesting)
class StepScope
{
public:
	explicit StepScope(NetGameType type) noexcept;
	~StepScope();
	StepScope(const StepScope&) = delete;
	StepScope& operator=(const StepScope&) = delete;
	StepScope(StepScope&&) = delete;
	StepScope& operator=(StepScope&&) = delete;
};
enum class Stream : uint8_t
{
	None,  ///< 0x672990 (0.0f) / 0x6729A0 (0): the static initialiser 0x672A80..0x672A8A
	Local, ///< 0x672B10 / 0x672B40
	Synced ///< 0x672AB0 / 0x672AF0
};
[[nodiscard]] Stream Active() noexcept;
/// PSysFloatRand 0x6729B0 -> [0xD4E0C0]: None 0; Synced GameRand(0xFFFF) (0x672AC2, it draws even for x == 0), Local
/// LocalRand(0xFFFF) (0x672B18); then (u x k) x x (0x672AD7 / 0x672ADD)
float FloatRand(float x, std::source_location where = std::source_location::current()) noexcept;
/// 0x6729C0: FloatRand(b - a) + a, b - a rounded first (fstp 0x6729C9)
float FloatRand(float a, float b, std::source_location where = std::source_location::current()) noexcept;
/// PSysRand 0x6729E0 -> [0xD4E0BC]: None 0; Synced GameRand(n) (0x672AFF, 0 for 0); Local LocalRand(n) (0x672B45)
int32_t Rand(int32_t n, std::source_location where = std::source_location::current()) noexcept;
/// PSysRandR3 0x6729F0: x, y, z = FloatRand(2) - 1 in that order, again while (z^2 + y^2) + x^2 > 1
/// (0x672A3E..0x672A66). (aproximado) outside a step the original never ends (0 - 1 everywhere, |p|^2 = 3) and never
/// gets there: (-1, -1, -1) once, with a warning
glm::vec3 RandR3(std::source_location where = std::source_location::current()) noexcept;
} // namespace psys

namespace crt
{
/// _rand 0x7C8837: s = s * 0x343FD + 0x269EC3, (s >> 16) & 0x7FFF; the seed starts at 1 (__initptd 0x7D2323)
int32_t Rand() noexcept;
/// _srand 0x7C882A
void Srand(uint32_t seed) noexcept;
/// ?Random@@YAMMM@Z 0x81D180: ((rand() x k) x (b - a)) + a, k [0x9A3700] (0x81D18E..0x81D19E)
float Random(float a, float b) noexcept;
} // namespace crt

namespace testing
{
/// (openblack) moved from VillagerCore.cpp: when set, GameRand / GameFloatRand return them, called before the 0-for-0
/// test, the seed untouched. psys's synced draws go through GameRand as 0x672AC2 does
void SetGameRand(std::function<uint32_t(uint32_t)> rand, std::function<float(float)> floatRand);
/// (openblack) how many psys draws were made outside an effect's step (OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1 logs
/// each site the first time)
[[nodiscard]] uint32_t OutsideStepDraws() noexcept;
/// (openblack) the seeds for a test, all restored on destruction (also the CRT seed, the PSys stream and the hooks)
class ScopedState
{
public:
	explicit ScopedState(Seeds seeds = {k_InitialSeed, k_InitialSeed}, uint32_t crtSeed = 1);
	~ScopedState();
	ScopedState(const ScopedState&) = delete;
	ScopedState& operator=(const ScopedState&) = delete;
	ScopedState(ScopedState&&) = delete;
	ScopedState& operator=(ScopedState&&) = delete;

private:
	Seeds _seeds;
	uint32_t _crtSeed;
	psys::Stream _stream;
	std::function<uint32_t(uint32_t)> _rand;
	std::function<float(float)> _floatRand;
};
} // namespace testing
} // namespace openblack::game_random
