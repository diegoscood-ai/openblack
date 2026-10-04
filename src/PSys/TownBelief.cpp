/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <numbers>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
// SF_TownBelief's UR_TownCentreBelief0
constexpr float k_HeightAt1 = 2.0f;
constexpr float k_HeightPerLevel = 2.5f;
constexpr float k_RadiusAt0 = 0.3f, k_RadiusAt1 = 0.6f;
constexpr float k_ScaleAt0 = 0.5f, k_ScaleAt1 = 1.0f;
constexpr float k_SpeedAt0 = 1.0f, k_SpeedAt1 = 2.0f;
constexpr float k_PhaseSpeed = 0.2f;
constexpr float k_SpeedUpDuringFight = 2.0f;
constexpr float k_FightAt0 = 1.5f, k_FightAt1 = 3.0f;
constexpr float k_BetweenFightsAt0 = 10.0f, k_BetweenFightsAt1 = 3.0f;
/// fmod's 2 pi, the double [0x8D45D8] = 6.2831854820251465 (the float 2 pi widened) of the rule's __CIfmod calls
/// 0x69C4B7 / 0x69C4F0 / 0x69C50B
constexpr double k_FmodTwoPi = 6.2831854820251465;

/// the player colours (0xBFF0B8, by GetRemapedPlayer; the remap comes from the profile, identity here)
constexpr std::array<uint32_t, 7> k_PlayerColours = {0xFF4646, 0x47FF54, 0xE347FF, 0x47F9FF, 0xFFFD47, 0x4777FF, 0xFFA247};

struct Symbol
{
	/// PlayerSymbolSprite +0xC, +0x10: the two glows' cells, +0x14: the second glow's angle (ctor fn_0069D5A0: all 0;
	/// frame_anim::PlayerSymbolCell / PlayerSymbolSpin)
	float glowA {0.0f}, glowB {0.0f}, glowSpin {0.0f};
	/// AtomData +0x24 / +0x28 / +0x2C (0x69C4AE..0x69C510); a1 and a2 start at PSysFloatRand(2 pi) each (0x69C0C3,
	/// 0x69C0D0), phase at 0
	float a1 {0.0f}, a2 {0.0f}, phase {0.0f};
	/// AtomData +0x30 (the wait before the next fight, counted down), +0x34 (the fight's timer), +0x38 (its length):
	/// all 0 from the ctor (0x69C6C4..0x69C6D6)
	float waitLength {0.0f}, fightTimer {0.0f}, fightLength {0.0f};
	/// the last step's fight curve (the rule's local [esp+0x14]: 1 - (2f - 1)^2 in a fight, else 0), which lowers the
	/// symbol in the position the same step writes (0x69C537..)
	float fight {0.0f};
	/// this frame's glow cells and spin (PlayerSymbolSprite::Draw 0x69D7E0, from Draw_(1) 0x69BF19 once a frame)
	float cellA {0.0f}, cellB {0.0f}, spin {0.0f};
};

struct Centre
{
	std::unordered_map<int, Symbol> symbols;
};

std::unordered_map<entt::entity, Centre> g_Centres;

/// PSysFloatRand(a, b) 0x6729C0 of the rule (UR_TownCentreBelief 0x69C3E1 / 0x69C40E): the effect of a town centre is
/// local (TownCentre::CreatePSys 0x69BC31, push 0), so the local stream (Step opens its step scope)
float Rand(float a, float b)
{
	return game_random::psys::FloatRand(a, b);
}

/// PlayersSymbols (PlayerSymbol::CreateFinalTextureSymbols 0x5DEF00): the local human's cell is a copy of the
/// ChooseSymbol cell of the profile's "player symbol" (registry, 0 when missing, as on this install), so it is drawn
/// straight from ChooseSymbol (4 x 4 cells of 64 x 64). Computer players get Lethis/Kazarr/Nemesis .cps greyscale
/// images there (not done: they use ChooseSymbol cell = player number).
const Creator& SymbolCreator(int player)
{
	static std::array<Creator, 16> creators = [] {
		std::array<Creator, 16> c;
		for (int i = 0; i < 16; ++i)
		{
			c[static_cast<size_t>(i)].kind = Creator::Kind::Sprite;
			c[static_cast<size_t>(i)].texture = "ChooseSymbol";
			c[static_cast<size_t>(i)].spritesPerRow = 4;
			c[static_cast<size_t>(i)].fileOffset = i;
			c[static_cast<size_t>(i)].numFrames = 1;
		}
		return c;
	}();
	constexpr int k_ProfileSymbol = 0;
	return creators[static_cast<size_t>(player == 0 ? k_ProfileSymbol : std::clamp(player, 0, 15))];
}

const Creator& GlowCreator()
{
	static const Creator creator = [] {
		Creator c;
		c.kind = Creator::Kind::Sprite;
		c.texture = "S_SpriteSheet3";
		c.spritesPerRow = 8;
		c.numFrames = 32;
		return c;
	}();
	return creator;
}

bool IsTownCentre(const ecs::components::Abode& abode)
{
	static const std::set<AbodeNumber> k_Centres = [] {
		std::set<AbodeNumber> result;
		for (const auto& info : Locator::infoConstants::value().abode)
		{
			if (info.abodeType == AbodeType::TownCentre)
			{
				result.insert(info.abodeNumber);
			}
		}
		return result;
	}();
	return k_Centres.contains(abode.type);
}

/// A belief symbol shown over a centre: the player, its belief clamped to 0..1 and its rank
struct Shown
{
	int player;
	float belief;
	int rank;
};

/// The players with some belief in the town and their rank (fn_0073BB10): the players with more belief, or as much and a
/// higher number. The belief is the town's GBelief +0x8 (Town::belief.belief, by player number). The candidates are
/// GetNextActivePlayer 0x5508D0's slots 0..6 (UR_TownCentreBelief 0x69BFF4; the neutral one has no symbol); the rank
/// counts all 8 slots, the neutral one included (fn_0073BB10 0x73BC20..0x73BC24: inc esi; cmp esi, 8)
std::vector<Shown> ShownSymbols(const std::array<float, 8>& beliefs)
{
	constexpr int k_SymbolPlayers = 7;
	constexpr int k_RankSlots = 8;
	std::vector<Shown> result;
	for (int player = 0; player < k_SymbolPlayers; ++player)
	{
		const float b = std::clamp(beliefs.at(static_cast<size_t>(player)), 0.0f, 1.0f);
		if (b <= 0.0f)
		{
			continue;
		}
		int rank = 0;
		for (int other = 0; other < k_RankSlots; ++other)
		{
			const float ob = std::clamp(beliefs.at(static_cast<size_t>(other)), 0.0f, 1.0f);
			if (other != player && (ob > b || (ob == b && other > player)))
			{
				++rank;
			}
		}
		result.push_back({player, b, rank});
	}
	return result;
}

float Radius(const Shown& shown)
{
	return shown.rank == 0 ? 0.0f : k_RadiusAt0 + (k_RadiusAt1 - k_RadiusAt0) * shown.belief;
}

/// The town centres (an Abode whose number is a TownCentre's, with a Mesh) and their town's beliefs, in the order of
/// TownCentre::DrawAll 0x7447F0: the list g_game +0x205CFC, which the TownCentre ctor pushes at the head (0x743AC3..
/// 0x743ACA), so the newest first (here by the object creation index, Object +0x3C). (aproximado) its IsAvailable test
/// (vt+0x2C == 1, 0x744808) is not ported
template <typename Fn>
void ForEachCentre(Fn&& fn)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	std::vector<std::pair<int64_t, entt::entity>> centres;
	registry.Each<const Abode, const Transform, const Mesh>(
	    [&centres](entt::entity entity, const Abode& abode, const Transform&, const Mesh&) {
		    if (IsTownCentre(abode))
		    {
			    centres.emplace_back(ecs::object_index::Of(entity), entity);
		    }
	    });
	std::stable_sort(centres.begin(), centres.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& [index, entity] : centres)
	{
		const auto& abode = registry.Get<const Abode>(entity);
		const auto town = towns.find(abode.townId);
		if (town == towns.end() || !registry.Valid(town->second))
		{
			continue;
		}
		fn(entity, registry.Get<const Town>(town->second).belief.belief);
	}
}

/// One step of the centre's effect (fn_00673340) through UR_TownCentreBelief::ModifyAtomCollection 0x69BF30, with the
/// step's dt [0xD4E0EC]
void StepCentre(Centre& centre, const std::vector<Shown>& shown, float dt)
{
	for (const auto& symbolOf : shown)
	{
		const auto [slot, created] = centre.symbols.try_emplace(symbolOf.player);
		auto& symbol = slot->second;
		if (created)
		{
			// 0x69C0B8..0x69C0D5: the new symbol's two angles, a1 then a2
			symbol.a1 = game_random::psys::FloatRand(glm::two_pi<float>());
			symbol.a2 = game_random::psys::FloatRand(glm::two_pi<float>());
		}
		const float b = symbolOf.belief;
		const float radius = Radius(symbolOf);
		float speed = symbolOf.rank == 0 ? 0.0f : (k_SpeedAt0 + (k_SpeedAt1 - k_SpeedAt0) * b) / std::max(radius, 1e-3f);
		float fight = 0.0f;
		if (symbolOf.rank == 1)
		{
			// 0x69C388..0x69C3A7: the second symbol only. While its wait (+0x30) is > 0 it counts down by dt (0x69C473)
			if (symbol.waitLength > 0.0f)
			{
				symbol.waitLength -= dt;
			}
			else
			{
				// 0x69C3AD..0x69C3C1: the fight's timer (+0x34) += dt; past the fight's length (+0x38), the next wait
				// and fight are drawn together, wait first (0x69C3C3..0x69C424), and the timer is back to 0
				symbol.fightTimer += dt;
				if (symbol.fightTimer > symbol.fightLength)
				{
					const float wait = k_BetweenFightsAt0 + (k_BetweenFightsAt1 - k_BetweenFightsAt0) * b;
					symbol.waitLength = Rand(0.5f, 1.5f) * wait;
					const float length = k_FightAt0 + (k_FightAt1 - k_FightAt0) * b;
					symbol.fightTimer = 0.0f;
					symbol.fightLength = Rand(0.5f, 1.5f) * length;
				}
				// 0x69C427..0x69C46B: f = timer / length, 0 for <= 0 or NaN, at most 1; fight = 1 - (2f - 1)^2
				float f = symbol.fightTimer / symbol.fightLength;
				if (!(f > 0.0f))
				{
					f = 0.0f;
				}
				else if (!(f < 1.0f))
				{
					f = 1.0f;
				}
				const float g = 2.0f * f - 1.0f;
				fight = 1.0f - g * g;
			}
			// 0x69C47F..0x69C496: speed x ((SpeedUpDuringFight - 1) x fight + 1)
			speed *= (k_SpeedUpDuringFight - 1.0f) * fight + 1.0f;
		}
		symbol.fight = fight;
		// 0x69C4A5..0x69C510: phase += dt x PhaseSpeed, a1 += ((cos(phase) + 1) x 0.25 + 0.5) x speed x dt x 0.846,
		// a2 += speed x dt, each one fmod 2 pi. (aproximado) the exe keeps each sum on the x87 stack (extended) into
		// __CIfmod and takes fcos of the unrounded fmod result (0x69C4BF); here float sums and cos of the stored float
		const auto wrap = [](float x) { return static_cast<float>(std::fmod(static_cast<double>(x), k_FmodTwoPi)); };
		symbol.phase = wrap(dt * k_PhaseSpeed + symbol.phase);
		symbol.a1 = wrap(((std::cos(symbol.phase) + 1.0f) * 0.25f + 0.5f) * speed * dt * 0.846f + symbol.a1);
		symbol.a2 = wrap(speed * dt + symbol.a2);
	}
}
} // namespace

void town_belief::Clear()
{
	g_Centres.clear();
}

void town_belief::Step()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// TownCentre::ProcessPSys 0x69BCC0 -> GJPSysInterface::Process_ 0x673690 (ms = [0xD01A38], the ms of a turn, not
	// the frame's) -> fn_00673300 -> fn_00673340: dt [0xD4E0EC] = ms x 0.001 (0x673402..0x67340C)
	const float dt = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	// PlayerSymbolSprite::Draw 0x69D7E0's g_game_time_inc [0xEA9EC0] (0x69D855 fild): the frame's whole game ms, 0 paused
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	// the rule's draws run in the town centre effect's step (fn_00673340), a local one (TownCentre::CreatePSys 0x69BC31)
	const game_random::psys::StepScope step(game_random::psys::NetGameType::Local);
	ForEachCentre([&](entt::entity entity, const std::array<float, 8>& beliefs) {
		const auto shown = ShownSymbols(beliefs);
		const auto [slot, created] = g_Centres.try_emplace(entity);
		auto& centre = slot->second;
		// a new effect: CreatePSys 0x69BC10 calls ProcessPSys at once (0x69BC95) and an effect's first Process steps
		// twice (+0xAD = 1 from fn_00672B50 0x672BF7; fn_00673300 0x673308..0x67331E); then this frame's own step.
		// (aproximado) the original makes it in TownCentre::MakeFunctional (0x743F18) / ResolveLoad (0x7448D8),
		// openblack on the first frame that sees the centre
		const int steps = created ? 3 : 1;
		for (int i = 0; i < steps; ++i)
		{
			StepCentre(centre, shown, dt);
		}
		// DrawPSys 0x69BF19 Draw_(1): each symbol's PlayerSymbolSprite::Draw 0x69D7E0, once a frame
		for (const auto& symbolOf : shown)
		{
			auto& symbol = centre.symbols[symbolOf.player];
			// the second glow's sprite angle +0x14 (frame_anim::PlayerSymbolSpin, written at 0x69D8C5) and the cells of
			// the two glows, +0xC and +0x10 (frame_anim::PlayerSymbolCell, 0x69D7E0..0x69D853)
			symbol.spin = graphics::frame_anim::PlayerSymbolSpin(symbol.glowSpin, milliseconds);
			symbol.cellA = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowA, milliseconds, 0));
			symbol.cellB = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowB, milliseconds, 1));
		}
	});
}

void town_belief::Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	ForEachCentre([&](entt::entity entity, const std::array<float, 8>& beliefs) {
		const auto found = g_Centres.find(entity);
		if (found == g_Centres.end())
		{
			return; // not stepped yet
		}
		const auto& centre = found->second;
		const auto& transform = registry.Get<const Transform>(entity);
		const auto& mesh = registry.Get<const Mesh>(entity);
		// the totem (TotemStatue, TownCentre::GetTotemPos 0x743F20) + its height (Object::GetHeight 0x638120 of the icon
		// on the plinth, GBelief::DrawBelief 0x438800) + HeightAt1; without a totem, the top of the town centre's mesh
		glm::vec3 base = transform.position;
		bool totem = false;
		registry.Each<const TotemStatue, const Transform>([&](const TotemStatue& statue, const Transform& plinth) {
			if (totem || statue.townCentre != entity)
			{
				return;
			}
			float height = 0.0f;
			if (registry.Valid(statue.top))
			{
				const auto* topMesh = registry.TryGet<const Mesh>(statue.top);
				const auto* topTransform = registry.TryGet<const Transform>(statue.top);
				if (topMesh != nullptr && topTransform != nullptr && meshes.Contains(topMesh->id))
				{
					height = meshes.Handle(topMesh->id)->GetBoundingBox().Size().y * topTransform->scale.y;
				}
			}
			base = glm::vec3(plinth.position.x, statue.baseY + height + k_HeightAt1, plinth.position.z);
			totem = true;
		});
		if (!totem && meshes.Contains(mesh.id))
		{
			base.y += meshes.Handle(mesh.id)->GetBoundingBox().maxima.y * transform.scale.y + k_HeightAt1;
		}
		const float s = std::clamp(glm::distance(camera, base) * 0.01f, 1.0f, 10.0f);

		// TownCentre::DrawPSys 0x69BF19: Draw_(1), each symbol its own Z object (PlayerSymbolSprite::AddDrawing 0x69D790)
		manager::Drawable drawable {base, {}, 1.0f, manager::DrawPath::Sorted};
		for (const auto& symbolOf : ShownSymbols(beliefs))
		{
			const auto symbolIt = centre.symbols.find(symbolOf.player);
			if (symbolIt == centre.symbols.end())
			{
				continue; // not stepped yet
			}
			const auto& symbol = symbolIt->second;
			const float radius = Radius(symbolOf);
			const float scale = k_ScaleAt0 + (k_ScaleAt1 - k_ScaleAt0) * symbolOf.belief;
			const glm::vec3 position =
			    base + s * glm::vec3(radius * std::cos(symbol.a2) * std::cos(symbol.a1),
			                         radius * std::sin(symbol.a1) +
			                             (1.0f - symbol.fight) * static_cast<float>(symbolOf.rank) * k_HeightPerLevel,
			                         radius * std::sin(symbol.a2) * std::cos(symbol.a1));

			// PlayerSymbolSprite::Draw 0x69D7E0: two glows then the symbol, all additive billboards
			const float size = 1.5f * scale;
			const uint32_t rgb = k_PlayerColours[static_cast<size_t>(symbolOf.player) % k_PlayerColours.size()];
			const std::array<uint8_t, 3> colour = {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8),
			                                       static_cast<uint8_t>(rgb)};
			const glm::mat3 still(1.0f);
			// the second glow's spin carried as the atom's SetAngleY 0x674360 matrix (lh_matrix::AngleY), whose roll
			// atan2(M[0][2], M[0][0]) = +spin (billboard::Screen turns it clockwise)
			const glm::mat3 spun = lh_matrix::AngleY(symbol.spin);
			drawable.atoms.push_back({&GlowCreator(), position, still, 1.5f * size, 1.0f, 99.0f, symbol.cellA, colour});
			drawable.atoms.push_back(
			    {&GlowCreator(), position, spun, 1.5f * size, 1.0f, 99.0f, symbol.cellB, {255, 255, 255}});
			drawable.atoms.push_back({&SymbolCreator(symbolOf.player), position, still, size, 1.0f, 255.0f, 0.0f, colour});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	});
}
