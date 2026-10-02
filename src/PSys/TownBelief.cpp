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
/// (aproximado, port previo) the rule adds the step's dt [0xD4E0EC] (fn_00673340 0x673402..0x67340C: ms x 0.001;
/// 0x69C3AD / 0x69C476) to the fight timer and wait; here a fixed 0.1 per Collect call
constexpr float k_Step = 0.1f;

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
};

struct Centre
{
	std::unordered_map<int, Symbol> symbols;
};

std::unordered_map<entt::entity, Centre> g_Centres;
/// (openblack) the visual clock of the last collect: a frame's game ms count once however many passes collect
uint32_t g_LastVisualMs {0};

/// PSysFloatRand(a, b) 0x6729C0 of the rule (UR_TownCentreBelief 0x69C3E1 / 0x69C40E): the effect of a town centre is
/// local (TownCentre::CreatePSys 0x69BC31, push 0), so the local stream (Collect opens its step scope)
float Rand(float a, float b)
{
	return game_random::psys::FloatRand(a, b);
}

int PlayerIndex(const std::string& name)
{
	static const std::array<std::string_view, 8> k_Names = {"PLAYER_ONE", "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
	                                                        "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "PLAYER_EIGHT"};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		if (name == k_Names[i])
		{
			return static_cast<int>(i);
		}
	}
	return -1;
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
} // namespace

void town_belief::Clear()
{
	g_Centres.clear();
}

void town_belief::Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto& meshes = Locator::resources::value().GetMeshes();
	// g_game_time_inc [0xEA9EC0] (0x69D855 fild): the frame's whole game ms, 0 in pause
	const bool newFrame = game_clock::VisualMs() != g_LastVisualMs;
	g_LastVisualMs = game_clock::VisualMs();
	const auto milliseconds = newFrame ? static_cast<float>(game_clock::FrameGameMs()) : 0.0f;
	// the rule's draws run in the town centre effect's step (fn_00673340), a local one (TownCentre::CreatePSys 0x69BC31)
	const game_random::psys::StepScope step(game_random::psys::NetGameType::Local);

	registry.Each<const Abode, const Transform, const Mesh>([&](entt::entity entity, const Abode& abode,
	                                                            const Transform& transform, const Mesh& mesh) {
		if (!IsTownCentre(abode))
		{
			return;
		}
		const auto town = towns.find(abode.townId);
		if (town == towns.end() || !registry.Valid(town->second))
		{
			return;
		}
		const auto& beliefs = registry.Get<const Town>(town->second).beliefs;
		// the totem (TotemStatue, TownCentre::GetTotemPos 0x743F20) + its height (Object::GetHeight 0x638120 of the icon
		// on the plinth, GBelief::DrawBelief 0x438800) + HeightAt1; without a totem, the top of the town centre's mesh
		glm::vec3 base = transform.position;
		bool found = false;
		registry.Each<const TotemStatue, const Transform>([&](const TotemStatue& statue, const Transform& plinth) {
			if (found || statue.townCentre != entity)
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
			found = true;
		});
		if (!found && meshes.Contains(mesh.id))
		{
			base.y += meshes.Handle(mesh.id)->GetBoundingBox().maxima.y * transform.scale.y + k_HeightAt1;
		}
		const float s = std::clamp(glm::distance(camera, base) * 0.01f, 1.0f, 10.0f);
		auto& centre = g_Centres[entity];

		// TownCentre::DrawPSys 0x69BF19: Draw_(1), each symbol its own Z object (PlayerSymbolSprite::AddDrawing 0x69D790)
		manager::Drawable drawable {base, {}, 1.0f, manager::DrawPath::Sorted};
		for (const auto& [name, value] : beliefs)
		{
			const int player = PlayerIndex(name);
			const float b = std::clamp(value, 0.0f, 1.0f);
			if (player < 0 || b <= 0.0f)
			{
				continue;
			}
			// rank (fn_0073BB10): the players with more belief, or as much and a higher number
			int rank = 0;
			for (const auto& [otherName, otherValue] : beliefs)
			{
				const int other = PlayerIndex(otherName);
				const float ob = std::clamp(otherValue, 0.0f, 1.0f);
				if (other >= 0 && other != player && (ob > b || (ob == b && other > player)))
				{
					++rank;
				}
			}
			const auto [slot, created] = centre.symbols.try_emplace(player);
			auto& symbol = slot->second;
			if (created)
			{
				// 0x69C0B8..0x69C0D5: the new symbol's two angles, a1 then a2
				symbol.a1 = game_random::psys::FloatRand(glm::two_pi<float>());
				symbol.a2 = game_random::psys::FloatRand(glm::two_pi<float>());
			}
			const float radius = rank == 0 ? 0.0f : k_RadiusAt0 + (k_RadiusAt1 - k_RadiusAt0) * b;
			float speed = rank == 0 ? 0.0f : (k_SpeedAt0 + (k_SpeedAt1 - k_SpeedAt0) * b) / std::max(radius, 1e-3f);
			const float scale = k_ScaleAt0 + (k_ScaleAt1 - k_ScaleAt0) * b;
			float fight = 0.0f;
			if (rank == 1)
			{
				// 0x69C388..0x69C3A7: the second symbol only. While its wait (+0x30) is > 0 it counts down (0x69C473)
				if (symbol.waitLength > 0.0f)
				{
					symbol.waitLength -= k_Step;
				}
				else
				{
					// 0x69C3AD..0x69C3C1: the fight's timer (+0x34); past the fight's length (+0x38), the next wait and
					// fight are drawn together, wait first (0x69C3C3..0x69C424), and the timer is back to 0
					symbol.fightTimer += k_Step;
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
				speed *= 1.0f + (k_SpeedUpDuringFight - 1.0f) * fight;
			}
			symbol.phase += k_PhaseSpeed * k_Step;
			symbol.a1 += (0.5f + 0.25f * (1.0f + std::cos(symbol.phase))) * speed * k_Step * 0.846f;
			symbol.a2 += speed * k_Step;
			const glm::vec3 position =
			    base + s * glm::vec3(radius * std::cos(symbol.a2) * std::cos(symbol.a1),
			                         radius * std::sin(symbol.a1) + (1.0f - fight) * static_cast<float>(rank) * k_HeightPerLevel,
			                         radius * std::sin(symbol.a2) * std::cos(symbol.a1));

			// PlayerSymbolSprite::Draw 0x69D7E0: two glows then the symbol, all additive billboards
			const float size = 1.5f * scale;
			const uint32_t rgb = k_PlayerColours[static_cast<size_t>(player) % k_PlayerColours.size()];
			const std::array<uint8_t, 3> colour = {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8),
			                                       static_cast<uint8_t>(rgb)};
			const glm::mat3 still(1.0f);
			// the second glow's sprite angle +0x14 (frame_anim::PlayerSymbolSpin, written at 0x69D8C5) carried as the atom's
			// SetAngleY 0x674360 matrix (lh_matrix::AngleY), whose roll atan2(M[0][2], M[0][0]) = +spin (billboard::Screen
			// turns it clockwise)
			const float spin = graphics::frame_anim::PlayerSymbolSpin(symbol.glowSpin, milliseconds);
			const glm::mat3 spun = lh_matrix::AngleY(spin);
			// the cells of the two glows, +0xC and +0x10 (frame_anim::PlayerSymbolCell, 0x69D7E0..0x69D853)
			const auto cellA = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowA, milliseconds, 0));
			const auto cellB = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowB, milliseconds, 1));
			drawable.atoms.push_back({&GlowCreator(), position, still, 1.5f * size, 1.0f, 99.0f, cellA, colour});
			drawable.atoms.push_back({&GlowCreator(), position, spun, 1.5f * size, 1.0f, 99.0f, cellB, {255, 255, 255}});
			drawable.atoms.push_back({&SymbolCreator(player), position, still, size, 1.0f, 255.0f, 0.0f, colour});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	});
}
