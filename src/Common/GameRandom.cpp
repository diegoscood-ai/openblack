/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameRandom.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <bit>
#include <map>
#include <string>
#include <thread>
#include <utility>

#include <spdlog/spdlog.h>

#include "GameClock.h"

namespace openblack::game_random
{
namespace
{
/// The game's random state: GData +8 / +0xC, PSysManager's two pointers [0xD4E0C0] / [0xD4E0BC] (one stream in
/// both), the game thread's CRT seed (_tiddata +0x14, __getptd 0x7D232B: one per thread in the original, the game
/// thread's here) and the (openblack) test hooks
struct State
{
	Seeds seeds;
	psys::Stream stream {psys::Stream::None};
	uint32_t crtSeed {1}; ///< __initptd 0x7D2323: _holdrand = 1
	std::function<uint32_t(uint32_t)> randForTests;
	std::function<float(float)> floatRandForTests;
	uint32_t outsideStepDraws {0};
#ifndef NDEBUG
	std::thread::id owner;
#endif
};

State& S() noexcept
{
	static State s_state;
	return s_state;
}

/// (openblack) GRand has no lock in the original: it lives in the game thread. Debug builds check nobody else draws
void CheckThread() noexcept
{
#ifndef NDEBUG
	auto& s = S();
	if (s.owner != std::thread::id {})
	{
		assert(s.owner == std::this_thread::get_id() && "game_random: drawn from another thread than Init / Reset's");
	}
#endif
}

void TakeThread() noexcept
{
#ifndef NDEBUG
	S().owner = std::this_thread::get_id();
#endif
}

/// The original's trace (GData::Rand 0x510650 / FloatRand 0x5106B0, when the FILE* [0xCD3C80] is set): the seed
/// before the draw, GameTurn (GData +0x10) and the caller. (openblack) OPENBLACK_TRACE_GAME_RAND=1, spdlog trace level
bool TraceOn() noexcept
{
	static const bool s_on = [] {
		const char* value = std::getenv("OPENBLACK_TRACE_GAME_RAND");
		return value != nullptr && value[0] == '1';
	}();
	return s_on;
}

void Trace(const char* what, uint32_t seed, const std::source_location& where)
{
	if (!TraceOn())
	{
		return;
	}
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		// 0xBE899C "Rand(). Seed %d, GameTurn %d, (%s:%d)" / 0xBE89D0 "FloatRand(). Seed %d, GameTurn %d, (%s:%d)"
		logger->trace("{}(). Seed {}, GameTurn {}, ({}:{})", what, static_cast<int32_t>(seed), game_clock::Turn(),
		              where.file_name(), where.line());
	}
}

/// (openblack) OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1: each psys draw outside an effect's step is counted, its site logged
/// the first time and the totals printed at exit
struct OutsideReport
{
	std::map<std::string, uint32_t> sites;
	~OutsideReport()
	{
		for (const auto& [site, count] : sites)
		{
			std::fprintf(stderr, "game_random: %u psys draws outside a step at %s\n", count, site.c_str());
		}
	}
};

bool OutsideHookOn() noexcept
{
	static const bool s_on = [] {
		const char* value = std::getenv("OPENBLACK_TEST_PSYS_RAND_OUTSIDE");
		return value != nullptr && value[0] == '1';
	}();
	return s_on;
}

void CountOutside(const std::source_location& where)
{
	++S().outsideStepDraws;
	if (!OutsideHookOn())
	{
		return;
	}
	static OutsideReport s_report;
	const auto site = std::string(where.file_name()) + ":" + std::to_string(where.line());
	if (++s_report.sites[site] == 1)
	{
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			logger->warn("game_random: psys draw outside an effect's step at {}", site);
		}
	}
}

/// 0x6DE53C / 0x6DE59F / 0x5106FE: fcomp 0.0; fnstsw; test ah,0x40: C3 is set for +-0 and for NaN (unordered)
bool ZeroOrNaN(float x) noexcept
{
	return x == 0.0f || std::isnan(x);
}

/// (u x x) x k: fild qword (0x51072A, exact for u <= 65534), fmul x (0x510732), fmul [0x8D6050] (0x510736)
float GDataFloat(uint32_t u, float x) noexcept
{
	float r = static_cast<float>(u);
	r = r * x;
	r = r * FloatRandScale();
	return r;
}

/// (u x k) x x: fild qword, fmul [0xD4E0B8] (0x672AD7 / 0x672B2D), fmul x (0x672ADD / 0x672B33)
float PSysFloat(uint32_t u, float x) noexcept
{
	float r = static_cast<float>(u);
	r = r * FloatRandScale();
	r = r * x;
	return r;
}
} // namespace

float FloatRandScale() noexcept
{
	return std::bit_cast<float>(k_FloatRandScaleBits);
}

float CrtRandomScale() noexcept
{
	return std::bit_cast<float>(k_CrtRandomScaleBits);
}

uint32_t LHRand(uint32_t n, uint32_t& seed) noexcept
{
	// 0x7DB608..0x7DB614: three lea and shl 5 make 9377 s, + 0x24DF
	uint32_t s = seed * 9377u + 0x24DFu;
	// 0x7DB620: ror 0xD, stored rotated (0x7DB629)
	s = std::rotr(s, 13);
	seed = s;
	// 0x7DB62B: div, unsigned
	return s % n;
}

float FloatRand(float x, uint32_t& seed) noexcept
{
	if (ZeroOrNaN(x))
	{
		return 0.0f;
	}
	const uint32_t u = LHRand(k_FloatRandRange, seed);
	return GDataFloat(u, x);
}

void Init() noexcept
{
	TakeThread();
	// 0x54F4AF: mov eax, 0x88F89F; 0x54F4B4 local, 0x54F4BA synced
	S().seeds = {k_InitialSeed, k_InitialSeed};
}

void Reset() noexcept
{
	TakeThread();
	// GData::Reset 0x510750: +8 .. +0x24 to 0 (the two seeds; GameTurn +0x10 is game_clock's)
	S().seeds = {0, 0};
}

Seeds Save() noexcept
{
	return S().seeds;
}

void Load(const Seeds& seeds) noexcept
{
	S().seeds = seeds;
}

const Seeds& Current() noexcept
{
	return S().seeds;
}

uint32_t GameRand(uint32_t n, std::source_location where) noexcept
{
	CheckThread();
	auto& s = S();
	// (openblack) the tests' hook, before the 0-for-0 test as VillagerCore's was
	if (s.randForTests)
	{
		return s.randForTests(n);
	}
	Trace("Rand", s.seeds.synced, where);
	// 0x510693: 0 for 0, no draw
	if (n == 0)
	{
		return 0;
	}
	return LHRand(n, s.seeds.synced);
}

float GameFloatRand(float x, std::source_location where) noexcept
{
	CheckThread();
	auto& s = S();
	if (s.floatRandForTests)
	{
		return s.floatRandForTests(x);
	}
	// 0x6DE534..0x6DE53F: 0 for +-0 / NaN before GData::FloatRand (which tests it again, 0x5106F6)
	if (ZeroOrNaN(x))
	{
		return 0.0f;
	}
	Trace("FloatRand", s.seeds.synced, where);
	return FloatRand(x, s.seeds.synced);
}

float GameFloatRange(float a, float b, std::source_location where) noexcept
{
	// 0x5E1CE0: fld b; fsub a; fstp (rounded), GameFloatRand(..., "LandAlignement.cpp", 0x32) 0x5E1CF3, fadd a
	const float d = b - a;
	const float r = GameFloatRand(d, where);
	return r + a;
}

uint32_t LocalRand(int32_t n) noexcept
{
	CheckThread();
	// 0x6DE574: test eax,eax
	if (n == 0)
	{
		return 0;
	}
	// 0x6DE57F..0x6DE587: LHRand on +0x205A3C; a negative n divides as a huge unsigned
	return LHRand(static_cast<uint32_t>(n), S().seeds.local);
}

float LocalFloatRand(float x) noexcept
{
	CheckThread();
	// 0x6DE597..0x6DE5A2
	if (ZeroOrNaN(x))
	{
		return 0.0f;
	}
	// 0x6DE5B9..0x6DE5DA: LHRand(0xFFFF) on +0x205A3C, fild, fmul x, fmul [0x8D6050]
	const uint32_t u = LHRand(k_FloatRandRange, S().seeds.local);
	return GDataFloat(u, x);
}

// ---- psys ----------------------------------------------------------------------------------------------------------

psys::StepScope::StepScope(NetGameType type) noexcept
{
	// 0x673346..0x673371: +0xAC == 0 -> 0x672B10 / 0x672B40, else 0x672AB0 / 0x672AF0
	S().stream = type == NetGameType::Synced ? Stream::Synced : Stream::Local;
}

psys::StepScope::~StepScope()
{
	// 0x67349B / 0x6734A5: back to 0x672990 / 0x6729A0, not to what was set before
	S().stream = Stream::None;
}

psys::Stream psys::Active() noexcept
{
	return S().stream;
}

float psys::FloatRand(float x, std::source_location where) noexcept
{
	switch (S().stream)
	{
	case Stream::Synced:
	{
		// 0x672AB0: GameRand(0xFFFF, "PSys3d.cpp", 0x3FB), no 0-for-0 test on x
		const uint32_t u = GameRand(k_FloatRandRange, where);
		return PSysFloat(u, x);
	}
	case Stream::Local:
	{
		// 0x672B10: LocalRand(0xFFFF)
		const uint32_t u = LocalRand(static_cast<int32_t>(k_FloatRandRange));
		return PSysFloat(u, x);
	}
	case Stream::None:
		break;
	}
	// 0x672990: fld 0.0
	CountOutside(where);
	return 0.0f;
}

float psys::FloatRand(float a, float b, std::source_location where) noexcept
{
	// 0x6729C0: fld b; fsub a; fstp [esp] (rounded), call [0xD4E0C0], fadd a (0x6729D2)
	const float d = b - a;
	const float r = FloatRand(d, where);
	return r + a;
}

int32_t psys::Rand(int32_t n, std::source_location where) noexcept
{
	switch (S().stream)
	{
	case Stream::Synced:
		// 0x672AF0: GameRand(n, "PSys3d.cpp", 0x400)
		return static_cast<int32_t>(GameRand(static_cast<uint32_t>(n), where));
	case Stream::Local:
		// 0x672B40: LocalRand(n)
		return static_cast<int32_t>(LocalRand(n));
	case Stream::None:
		break;
	}
	// 0x6729A0: xor eax, eax
	CountOutside(where);
	return 0;
}

glm::vec3 psys::RandR3(std::source_location where) noexcept
{
	if (S().stream == Stream::None)
	{
		// (aproximado) 0x672A66 would loop forever on (-1, -1, -1); the original never calls it outside a step
		CountOutside(where);
		static bool s_warned = false;
		if (!s_warned)
		{
			s_warned = true;
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->warn("game_random: PSysRandR3 outside an effect's step ({}:{}), (-1, -1, -1)", where.file_name(),
				             where.line());
			}
		}
		return {-1.0f, -1.0f, -1.0f};
	}
	glm::vec3 p;
	float sum = 0.0f;
	do
	{
		// 0x6729F6..0x672A37: FloatRand(2) - 1 into +0, +4, +8, in that order
		p.x = FloatRand(2.0f, where) - 1.0f;
		p.y = FloatRand(2.0f, where) - 1.0f;
		p.z = FloatRand(2.0f, where) - 1.0f;
		// 0x672A3E..0x672A57: (z z + y y) + x x
		const float zz = p.z * p.z;
		const float yy = p.y * p.y;
		const float xx = p.x * p.x;
		sum = zz + yy;
		sum = sum + xx;
		// 0x672A59..0x672A66: fcomp 1.0; test ah,0x41; je: again while sum > 1
	} while (sum > 1.0f);
	return p;
}

// ---- crt -----------------------------------------------------------------------------------------------------------

int32_t crt::Rand() noexcept
{
	CheckThread();
	auto& seed = S().crtSeed;
	// 0x7C883F..0x7C8853
	seed = seed * 0x343FDu + 0x269EC3u;
	return static_cast<int32_t>((seed >> 16u) & 0x7FFFu);
}

void crt::Srand(uint32_t seed) noexcept
{
	// 0x7C8833
	S().crtSeed = seed;
}

float crt::Random(float a, float b) noexcept
{
	// 0x81D181..0x81D19E: fild rand(); fmul [0x9A3700]; fld b; fsub a; fmulp; fadd a
	float r = static_cast<float>(Rand());
	r = r * CrtRandomScale();
	const float d = b - a;
	r = r * d;
	return r + a;
}

// ---- testing -------------------------------------------------------------------------------------------------------

void testing::SetGameRand(std::function<uint32_t(uint32_t)> rand, std::function<float(float)> floatRand)
{
	S().randForTests = std::move(rand);
	S().floatRandForTests = std::move(floatRand);
}

uint32_t testing::OutsideStepDraws() noexcept
{
	return S().outsideStepDraws;
}

testing::ScopedState::ScopedState(Seeds seeds, uint32_t crtSeed)
    : _seeds(S().seeds)
    , _crtSeed(S().crtSeed)
    , _stream(S().stream)
    , _rand(S().randForTests)
    , _floatRand(S().floatRandForTests)
{
	auto& s = S();
	s.seeds = seeds;
	s.crtSeed = crtSeed;
	s.stream = psys::Stream::None;
	s.randForTests = nullptr;
	s.floatRandForTests = nullptr;
}

testing::ScopedState::~ScopedState()
{
	auto& s = S();
	s.seeds = _seeds;
	s.crtSeed = _crtSeed;
	s.stream = _stream;
	s.randForTests = std::move(_rand);
	s.floatRandForTests = std::move(_floatRand);
}
} // namespace openblack::game_random
