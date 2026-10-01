/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Trees.h"
#include "Locator.h"

/// The predators' lairs (Tiger / Wolf / Lion ::CalculeLairPos; research dev\tmp_dis\animals\audit_r4.md §1.8 and
/// lairs.md). The forest list, centres and grown trees come from ECS/Trees.h (read only).
namespace openblack::ecs::animal_ai::detail
{
namespace
{
using components::BigForest;
using components::Flock;
using components::Transform;
using components::Tree;

/// GUtils::SigmoidThreshold's static table 0xC23284: 41 floats, a logistic around index 20
constexpr std::array<float, 41> k_Sigmoid = {
    0.0f,          3.6e-9f,       1e-8f,         2.8e-8f,       7.78e-8f,      2.163e-7f,     6.018e-7f,
    1.674e-6f,     4.6568e-6f,    1.29542e-5f,   3.60351e-5f,   0.000100235899f, 0.000278786494f, 0.000775143097f,
    0.00215331907f, 0.00596720818f, 0.0164249446f, 0.0443917401f, 0.114437282f,  0.264424354f,   0.5f,
    0.735575676f,  0.885562718f,  0.955608249f,  0.983575046f,  0.9940328f,    0.997846663f,  0.999224842f,
    0.999721229f,  0.999899745f,  0.999963939f,  0.999987066f,  0.999995351f,  0.999998331f,  0.999999404f,
    0.999999762f,  0.99999994f,   1.0f,          1.0f,          1.0f,          1.0f};

/// GUtils::SigmoidThreshold (0x74F170): a == 1 -> 0; T[min(ftol((clamp(clamp(b) - a) + 1) x 20.5), 40)]
float SigmoidThreshold(float a, float b)
{
	if (a == 1.0f)
	{
		return 0.0f;
	}
	// x87: the clamps, the subtraction and (v + 1) x 20.5 stay in extended precision until ftol (double here)
	const double v = std::clamp(static_cast<double>(std::clamp(b, -1.0f, 1.0f)) - a, -1.0, 1.0);
	const auto i = static_cast<uint32_t>(static_cast<int32_t>((v + 1.0) * 20.5));
	return k_Sigmoid.at(std::min(i, 40u));
}

/// The 1/sqrt table 0xDA5A10 (1024 entries, filled once by 0x74F590, called from GGame::InitOneTimeOnly): entry i keeps
/// the top 10 mantissa bits (& 0x7FE000) of 1/sqrt(f), f the float 0x3F000000 | i << 14 (the exponent's low bit and 9
/// mantissa bits); an exact 1 stores 0x7FE000
const std::array<uint32_t, 1024>& InvSqrtTable()
{
	static const auto table = [] {
		std::array<uint32_t, 1024> t {};
		for (uint32_t i = 0; i < t.size(); ++i)
		{
			const auto f = std::bit_cast<float>((0x3F800000u & 0xFF003FFFu) | (i << 14));
			const double r = 1.0 / std::sqrt(static_cast<double>(f));
			t.at(i) = r == 1.0 ? 0x7FE000u : (std::bit_cast<uint32_t>(static_cast<float>(r)) & 0x7FE000u);
		}
		return t;
	}();
	return table;
}

/// _FUN_0074f620: the table's approximate 1/sqrt: exponent ((0xBE000000 - exponent bits) >> 1), mantissa from the table
float InvSqrtApprox(float x)
{
	const auto bits = std::bit_cast<uint32_t>(x);
	const uint32_t exponent = ((0xBE000000u - (bits & 0x7F800000u)) >> 1) & 0x7F800000u;
	return std::bit_cast<float>(exponent | InvSqrtTable().at((bits >> 14) & 0x3FFu));
}

/// GUtils::GetDistance (0x74CCB0) = hypotenuse (0x74F680) of the MapCoords (6553.6 per metre) difference on x / z:
/// s = (float)((dx / 65536)^2 + (dz / 65536)^2), then ftol(65536 / InvSqrtApprox(s)) (a ~0.1% approximate length)
uint32_t MapDistance(glm::vec2 a, glm::vec2 b)
{
	const auto mx = [](float metres) { return map_coords::ToFixed(metres); }; // MapCoords(LHPoint) 0x603160
	const double dx = static_cast<double>(mx(b.x) - mx(a.x)) / 65536.0;
	const double dz = static_cast<double>(mx(b.y) - mx(a.y)) / 65536.0;
	const auto s = static_cast<float>(dx * dx + dz * dz);
	return static_cast<uint32_t>(static_cast<int64_t>(65536.0 / static_cast<double>(InvSqrtApprox(s))));
}

/// fn_0053AD00(forest, d, scale): SigmoidThreshold(-0.9, (grown + growing) / 20) is computed and discarded, the score
/// is SigmoidThreshold(-0.9, -(d / scale))
float ForestScore(uint32_t forestId, float distance, float scale)
{
	[[maybe_unused]] const float discarded =
	    SigmoidThreshold(-0.9f, static_cast<float>(static_cast<uint32_t>(ForestTreeCount(forestId)) / 20u));
	return SigmoidThreshold(-0.9f, -(distance / scale));
}

/// The forest loop shared by Tiger (0x4214D2) and Wolf (0x42181A): the forest list newest first (g_game +0x205BB4,
/// next +0x44), the first forest taken unscored, a later one replaces it only if it scores strictly more
/// (fcomp; test ah, 1). The tiger also calls FindNearestDrinkingWater(me, 500) per forest; it only depends on its own
/// position and never changes the choice, so it is not ported. Returns the chosen forest's first grown tree
/// (Forest +0x48: the one nearest its centre), none when there is no forest or it has no grown tree.
std::optional<glm::vec3> BestForestLair(glm::vec2 me, uint32_t& chosen)
{
	std::optional<uint32_t> best;
	float bestScore = 0.0f;
	for (const auto id : ForestsNewestFirst())
	{
		if (!best)
		{
			best = id;
			continue;
		}
		const auto centre = ForestCentre(id);
		const float score = ForestScore(id, static_cast<float>(MapDistance(me, {centre.x, centre.z})), 1000.0f);
		if (bestScore < score)
		{
			bestScore = score;
			best = id;
		}
	}
	chosen = best.value_or(0);
	if (!best)
	{
		return std::nullopt;
	}
	const auto grown = GrownTreesByDistance(*best);
	if (grown.empty())
	{
		return std::nullopt;
	}
	return Locator::entitiesRegistry::value().Get<const Transform>(grown.front()).position;
}

/// The nearest entity by GetDistance over a list with head insertion (the newest first): a later one replaces only if
/// strictly nearer, so ties keep the newest (the object creation index stands in for the list order)
template <typename Component>
std::optional<glm::vec3> NearestNewestFirst(glm::vec2 from)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> best;
	uint32_t bestDistance = std::numeric_limits<uint32_t>::max();
	int64_t bestIndex = -1;
	registry.Each<const Component, const Transform>([&](entt::entity entity, const Component&, const Transform& transform) {
		const uint32_t d = MapDistance(from, Xz(transform));
		const int64_t index = object_index::Of(entity);
		if (!best || d < bestDistance || (d == bestDistance && index > bestIndex))
		{
			bestDistance = d;
			bestIndex = index;
			best = transform.position;
		}
	});
	return best;
}

void TraceLair(const Context& ctx, const char* rule, uint32_t forestId, glm::vec3 lair)
{
	static const bool trace = std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: lair ({}, forest {}) at ({:.1f}, {:.1f}) from ({:.1f}, {:.1f})",
		                   static_cast<uint32_t>(ctx.entity), rule, forestId, lair.x, lair.z, ctx.transform.position.x,
		                   ctx.transform.position.z);
	}
}
} // namespace

void CalculeLairPos(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || !IsLeader(ctx))
	{
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	uint32_t forestId = 0;
	switch (HunterOf(ctx.animal.type))
	{
	case Hunter::Tiger:
	{
		// Tiger::CalculeLairPos (0x421470): the best forest's grown tree nearest its centre, else where it is
		const auto lair = BestForestLair(me, forestId);
		const auto at = lair.value_or(ctx.transform.position);
		TraceLair(ctx, lair ? "tiger forest" : "tiger self", forestId, at);
		SetDomainCentre(*flock, at);
		break;
	}
	case Hunter::Wolf:
	{
		// Wolf::CalculeLairPos (0x421730). Not ported: flock +0x5C set -> nothing (the original only ever zeroes it).
		// A leader in a script (IsInScript, 0x421774): where it is
		if (script_held::IsInScript(ctx.entity))
		{
			TraceLair(ctx, "wolf in script", 0, ctx.transform.position);
			SetDomainCentre(*flock, ctx.transform.position);
			break;
		}
		// The nearest big forest (g_game +0x205CE4)
		if (const auto big = NearestNewestFirst<BigForest>(me); big)
		{
			TraceLair(ctx, "wolf big forest", 0, *big);
			SetDomainCentre(*flock, *big);
			break;
		}
		// else the tiger's forest rule (without the water search); a forest with no grown tree falls through
		if (const auto lair = BestForestLair(me, forestId); lair)
		{
			TraceLair(ctx, "wolf forest", forestId, *lair);
			SetDomainCentre(*flock, *lair);
			break;
		}
		// else the nearest tree of the tree list (g_game +0x205CDC, every tree), else where it is
		const auto tree = NearestNewestFirst<Tree>(me);
		const auto at = tree.value_or(ctx.transform.position);
		TraceLair(ctx, tree ? "wolf tree" : "wolf self", 0, at);
		SetDomainCentre(*flock, at);
		break;
	}
	default:
		// Lion::CalculeLairPos (0x420010): where it is
		SetDomainCentre(*flock, ctx.transform.position);
		break;
	}
}

void TestLairs()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto id : ForestsNewestFirst())
	{
		const auto centre = ForestCentre(id);
		const auto grown = GrownTreesByDistance(id);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Lair test: forest {} centre ({:.1f}, {:.1f}) trees {} grown {}", id, centre.x,
		                   centre.z, ForestTreeCount(id), grown.size());
	}
	std::vector<entt::entity> leaders;
	registry.Each<const components::Animal, const components::AnimalBrain>(
	    [&](entt::entity e, const components::Animal& animal, const components::AnimalBrain&) {
		    if (HunterOf(animal.type) != Hunter::None)
		    {
			    leaders.push_back(e);
		    }
	    });
	for (const auto e : leaders)
	{
		auto& animal = registry.Get<components::Animal>(e);
		Context ctx {e, animal, registry.Get<components::AnimalBrain>(e), registry.Get<Transform>(e), InfoOf(animal)};
		if (IsLeader(ctx))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Lair test: animal {} type {}", static_cast<uint32_t>(e),
			                   static_cast<int>(animal.type));
			CalculeLairPos(ctx);
		}
	}
}

} // namespace openblack::ecs::animal_ai::detail
