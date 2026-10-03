/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// InfluenceCircle (fn_008265F0..fn_00827210) and the hand's crossing of it (fn_00827820): the circles
// GGame::Update3DInfluence 0x555280 rebuilds every 10 turns when a radius moved (one per citadel and per town with
// influence), their overlaps, the alpha their draw gives them, and the ripple plus the sound a hand that crosses one
// makes (fn_0x005e5cd0 0x5E61B0). Milestone B8 of dev\tmp_dis\audio\PLAN.md (ui_creature.md §2.2). The drawing is the
// Renderer's (Graphics/RendererInfluence.cpp). Research: dev\_scratch\coordinador\spec_influence_circle.md.

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "3D/LandMorph.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "Influence.h"
#include "InfluenceState.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The eight players of the original ([0xEB9A48] and [0xEB9A1C] are arrays of 8)
constexpr size_t k_Players = 8;
/// [0x8C5840]: fn_00555240's threshold (strict). (inferido) a float
constexpr float k_RebuildThreshold = 0.01f;
/// 0x555286: Update3DInfluence runs on the turns that are a multiple of 10
constexpr uint32_t k_RebuildTurns = 10;
/// InfluenceCircle::Draw(1): the camera gate [0x8AB41C], the height of the full alpha [0x8C7B34], the ramp's 0.01
/// [0x8C4B10] and 120 [0x8CF338], the full alpha 0x78 (0x826CA2)
constexpr float k_CameraGate = 100.0f;
constexpr float k_CameraFull = 200.0f;
constexpr float k_RampScale = 0.01f;
constexpr float k_RampAlpha = 120.0f;
constexpr uint8_t k_FullAlpha = 0x78;
/// fn_00827110 0x8271B9..0x8271F5: the hidden closing column's colour, white with alpha 0
constexpr uint32_t k_HiddenClosingColour = 0x00FFFFFFu;
/// fn_00827250: the ripple's life (0x827273) and the smallest sprite size [0x8BF518]
constexpr int32_t k_RippleLifeMs = 2000;
constexpr float k_RippleMinSize = 0.0001f;
/// fn_00827500: the fade's 0.001 [0x8AC418] under 1000 ms, the growth's 0.001 x 10 ([0x8AB414]), the wrap 14
/// [0x99C9EC] and 1 / 14 [0x9A3934], the alpha's 255 [0x8AB270]
constexpr int32_t k_RippleFadeMs = 1000;
constexpr float k_RippleFadeRate = 0.001f;
constexpr float k_Milli = 0.001f;
constexpr float k_RippleGrowth = 10.0f;
constexpr float k_RippleWrap = 14.0f;
constexpr float k_RippleInverseWrap = 0.0714285746f;
constexpr float k_RippleAlpha = 255.0f;
/// fn_00827670: the stop |b - a|^2 <= 1 [0x8AA390] and the half [0x8AA3B4]
constexpr float k_BisectStop = 1.0f;
constexpr float k_Half = 0.5f;
/// [0x8C78D8] pi / 2: the ripple's yaw is the border's tangent
constexpr float k_HalfPi = 1.57079637f;
/// 2 pi, the float [0x8AB210] of fn_008265F0. (inferido) the same constant in GetYAngle 0x841260 (+ 2 pi to a negative
/// angle) and as the bound of the ripple sprites' Random(0, 2 pi) (0x827250..0x82743F): neither push was read
constexpr float k_TwoPi = 6.28318548f;

/// [0xEB9A48]: the hand was inside a circle of this player on the previous frame, and [0xEB9A68]: that array is filled
/// in. Statics of the process in the original too: InfluenceCircle::Reset (GGame::ClearMap) only empties the circle
/// list, nothing clears these.
std::array<bool, k_Players> g_WasInside {};
bool g_HaveState = false;
/// [0xEA9EF0..0xEA9EF8]: the hand's point of the previous call (x and z; y is written 0, 0x82786A / 0x827A1F), which
/// every call overwrites with the current one (0x827A12..0x827A2A), the first included
glm::vec3 g_PreviousPoint {0.0f};

/// fn_00827210: the point is inside the circle when dx * dx + dz * dz < r * r (strictly, 0x82723C)
bool Inside(const glm::vec3& centre, float radius, const glm::vec3& point)
{
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	return dx * dx + dz * dz < radius * radius;
}

/// fn_00827110(this, other): every column of `circle` inside `other` is hidden. Columns 0..N - 1 keep their RGB with
/// alpha 0 (0x827134..0x8271B3), the closing column N becomes white with alpha 0 (0x8271B9..0x8271F5), so its
/// triangles fade towards white: a quirk of the original, kept. (inferido) the column's point is the ctor's ground
/// vertex (cos r + cx, sin r + cz; r + cx, cz for N), which fn_00827110 computes again the same way
void HideContact(influence::Circle& circle, const influence::Circle& other)
{
	const size_t segments = circle.Segments();
	for (size_t i = 0; i <= segments; ++i)
	{
		if (!Inside(other.centre, other.radius, circle.curtain.positions.at(3 * i)))
		{
			continue;
		}
		for (size_t k = 0; k < 3; ++k)
		{
			auto& colour = circle.curtain.colours.at(3 * i + k);
			colour = i < segments ? colour & 0x00FFFFFFu : k_HiddenClosingColour;
		}
		circle.hidden.at(i) = 1;
	}
}

/// fn_00827040(this, older) for one older circle of the list (0x826B96..0x826BAD, newest first): nothing when either is
/// dead or their players differ; d is the 3D distance of the centres (0x82706F..0x8270B9); d + r < r_older kills this
/// one, else d + r_older < r kills the older one, else each hides its columns inside the other, this one first
/// (0x8270F9..0x827104). A circle killed later in the same ctor may already have hidden columns of others (kept)
void Overlap(influence::Circle& circle, influence::Circle& older)
{
	if (circle.dead || older.dead || circle.player != older.player)
	{
		return;
	}
	const glm::vec3 d = circle.centre - older.centre;
	const float distance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	if (distance + circle.radius < older.radius)
	{
		circle.dead = true;
	}
	else if (distance + older.radius < circle.radius)
	{
		older.dead = true;
	}
	else
	{
		HideContact(circle, older);
		HideContact(older, circle);
	}
}

/// InfluenceCircle::Add 0x826FA0: the ctor fn_008265F0 (the curtain, the head of the list, the overlaps with the older
/// circles), then every dead circle deleted (0x826FFA..0x82701F)
void Add(std::vector<influence::Circle>& circles, PlayerNames player, const glm::vec3& centre, float radius)
{
	// +0x2C = the colour index & 7 (0x826632); every vertex takes g_players_color of it, alpha 0
	const auto index = static_cast<size_t>(player) & 7u;
	influence::Circle circle {static_cast<PlayerNames>(index), centre, radius,
	                          land_morph::InfluenceCurtain(land_morph::CurrentAltitude(), centre, radius,
	                                                       influence::k_CircleColours.at(index)),
	                          {}};
	// +0x18: (3N + 3) x 4 bytes allocated (0x82673A), N + 1 used
	circle.hidden.assign(circle.Segments() + 1, 0);
	circles.insert(circles.begin(), std::move(circle));
	for (size_t i = 1; i < circles.size(); ++i)
	{
		Overlap(circles.front(), circles.at(i));
	}
	std::erase_if(circles, [](const influence::Circle& c) { return c.dead; });
}

/// fn_008277B0(previous, current, player): the first circle of the player that has one of the two points inside and
/// the other outside, or none
const influence::Circle* CrossedCircle(std::span<const influence::Circle> circles, const glm::vec3& previous,
                                       const glm::vec3& current, size_t player)
{
	for (const auto& circle : circles)
	{
		if (static_cast<size_t>(circle.player) == player &&
		    Inside(circle.centre, circle.radius, previous) != Inside(circle.centre, circle.radius, current))
		{
			return &circle;
		}
	}
	return nullptr;
}

/// fn_00827250(point, colour, yaw), the ripple's ctor: life 2000 ms (0x827273), the colour +0x34, the 7 sprites of
/// LH3DSprite::Create(7, 1) 0x8404A0 at the point with the smoke material [0xEA1ABC] (materials::k_Smoke) and cell 63
/// (+0x28 |= 0x3F), size max(2 i, 0.0001) ([0x8BF518]; the origin +0x18 / +0x1C is SetToZero's 0, so its rescale keeps
/// it 0), angle Random(0, 2 pi) (0x81D180), flag 1; the matrix +0x38..+0x64 is RotX(pi / 2) (double [0x8C7B48]) then
/// the yaw (0x82734E..0x827434). (aproximado) its rows are written as exactly (c, 0, s), (s, 0, -c), (0, 1, 0): the
/// original's cos(pi / 2) leaves a residue of about 4e-8 in them
influence::Ripple MakeRipple(const glm::vec3& point, uint32_t colour, float yaw)
{
	influence::Ripple ripple;
	ripple.point = point;
	ripple.life = k_RippleLifeMs;
	ripple.colour = colour;
	const float c = std::cos(yaw);
	const float s = std::sin(yaw);
	ripple.matrix[0] = glm::vec4(c, 0.0f, s, 0.0f);
	ripple.matrix[1] = glm::vec4(s, 0.0f, -c, 0.0f);
	ripple.matrix[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	ripple.matrix[3] = glm::vec4(point, 1.0f);
	for (size_t i = 0; i < influence::Ripple::k_Sprites; ++i)
	{
		const float size = 2.0f * static_cast<float>(i);
		ripple.sizes.at(i) = size > k_RippleMinSize ? size : k_RippleMinSize;
		ripple.angles.at(i) = game_random::crt::Random(0.0f, k_TwoPi);
		ripple.flags.at(i) = 1;
	}
	return ripple;
}

/// GetYAngle(centre, point) 0x841260: atan2(dz, dx), + 2 pi when negative
float YAngle(const glm::vec3& from, const glm::vec3& to)
{
	float angle = std::atan2(to.z - from.z, to.x - from.x);
	if (angle < 0.0f)
	{
		angle += k_TwoPi;
	}
	return angle;
}

/// The ripple of a crossing of `circle` (fn_00827820 0x8278F2..0x8279E2): the inside point is the previous one when
/// Contains(previous), else the current one (0x8278F2..0x82792C), both at y = 0; fn_00827670 finds the edge between
/// them; y = max(GetAltitude(point), hand.y) (0x82794F..0x8279A5); yaw = GetYAngle(centre, point) + pi / 2
influence::Ripple CrossingRipple(const influence::Circle& circle, const glm::vec3& previous, const glm::vec3& handPosition)
{
	const glm::vec3 current(handPosition.x, 0.0f, handPosition.z);
	const bool previousInside = Inside(circle.centre, circle.radius, previous);
	auto point = influence::detail::CrossingPoint(circle.centre, circle.radius, previousInside ? previous : current,
	                                              previousInside ? current : previous);
	const float altitude = land_morph::CurrentAltitude()(glm::vec2(point.x, point.z));
	point.y = altitude > handPosition.y ? altitude : handPosition.y;
	const float yaw = YAngle(circle.centre, point) + k_HalfPi;
	return MakeRipple(point, influence::k_CircleColours.at(static_cast<size_t>(circle.player) & 7u), yaw);
}
} // namespace

void influence::NoteInfluence(float now, float last)
{
	if (std::fabs(now - last) > k_RebuildThreshold)
	{
		detail::Globals().circlesDirty = true;
	}
}

void influence::ForceNeedUpdateInfluence()
{
	detail::Globals().circlesDirty = true;
}

void influence::Update3DInfluence()
{
	auto& globals = detail::Globals();
	// 0x555286: the dirty byte and GameTurn % 10 == 0
	if (!globals.circlesDirty || game_clock::Turn() % k_RebuildTurns != 0)
	{
		return;
	}
	// InfluenceCircle::Reset 0x826C50
	globals.circles.clear();
	auto& registry = Locator::entitiesRegistry::value();
	// GPlayer +0xA48: the first temple of each player, as CitadelInfluenceAt
	std::array<entt::entity, k_Players> citadels {};
	citadels.fill(entt::null);
	registry.Each<const Temple>([&citadels](entt::entity entity, const Temple& temple) {
		const auto index = static_cast<size_t>(temple.owner);
		if (index < citadels.size() && citadels.at(index) == entt::null)
		{
			citadels.at(index) = entity;
		}
	});
	// GetNextPlayer 0x5508A0 stops at the neutral player (type 3, 0x5508C6). (inferido) a player the land did not make
	// has no citadel and no town, so every slot before the neutral one is walked
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::NEUTRAL); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		if (const auto citadel = citadels.at(p); citadel != entt::null)
		{
			const auto* transform = registry.TryGet<const Transform>(citadel);
			const float radius = CitadelRadius(citadel); // Citadel::GetInfluence 0x464090
			if (radius != 0.0f && transform != nullptr)
			{
				Add(globals.circles, player, transform->position, radius); // 0x55530E
			}
			if (auto* stored = registry.TryGet<CitadelInfluence>(citadel); stored != nullptr)
			{
				stored->drawnRadius = radius; // citadel +0x78, always
			}
		}
		// the town list GPlayer +0xA50 (map_cells::TownsOf: the oldest first)
		for (const auto town : ecs::map_cells::TownsOf(player))
		{
			auto* townInfluence = registry.TryGet<TownInfluence>(town);
			const auto* transform = registry.TryGet<const Transform>(town);
			if (townInfluence == nullptr || transform == nullptr)
			{
				continue;
			}
			const float radius = townInfluence->radius; // town +0x5C8
			if (radius != 0.0f)
			{
				Add(globals.circles, player, transform->position, radius); // 0x555354
			}
			townInfluence->drawnRadius = radius; // town +0xF24, always
		}
	}
	globals.circlesDirty = false; // 0x555384
}

std::span<const influence::Circle> influence::Circles()
{
	return detail::GlobalsOrDefault().circles;
}

bool influence::BoundaryShown(PlayerNames player)
{
	const auto index = static_cast<size_t>(player);
	const auto& shown = detail::GlobalsOrDefault().boundaryShown;
	return index < shown.size() && shown.at(index);
}

void influence::ShowBoundary(PlayerNames player)
{
	const auto index = static_cast<size_t>(player);
	auto& shown = detail::Globals().boundaryShown;
	if (index < shown.size())
	{
		shown.at(index) = true;
	}
}

std::optional<uint8_t> influence::CurtainAlpha(float cameraY)
{
	if (cameraY <= k_CameraGate)
	{
		return std::nullopt;
	}
	if (cameraY >= k_CameraFull)
	{
		return k_FullAlpha;
	}
	// ftol truncates: 0..119
	return static_cast<uint8_t>(static_cast<int32_t>((cameraY - k_CameraGate) * k_RampScale * k_RampAlpha));
}

void influence::SetCurtainAlpha(uint8_t alpha)
{
	auto* globals = detail::TryGlobals();
	if (globals == nullptr)
	{
		return;
	}
	for (auto& circle : globals->circles)
	{
		// 0x826F0D..0x826F1E: the cache +0x34, then the latch [0xEB9A1C + 4 player]
		if (circle.alphaCache == alpha || !globals->boundaryShown.at(static_cast<size_t>(circle.player) & 7u))
		{
			continue;
		}
		circle.alphaCache = alpha;
		const size_t segments = circle.Segments();
		for (size_t j = 0; j <= segments; ++j)
		{
			if (circle.hidden.at(j) != 0)
			{
				continue;
			}
			// the middle vertex of the column, colour +4 of the triple (0x826F3B..0x826F4B)
			auto& colour = circle.curtain.colours.at(3 * j + 1);
			colour = (colour & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha) << 24);
		}
	}
}

void influence::UpdateRipples(uint32_t gameTimeIncMs)
{
	auto* globals = detail::TryGlobals();
	if (globals == nullptr)
	{
		return;
	}
	// fn_008274A0: life -= g_game_time_inc; a negative one returns 1 and fn_00827450 deletes the ripple
	std::erase_if(globals->ripples, [gameTimeIncMs](Ripple& ripple) {
		ripple.life -= static_cast<int32_t>(gameTimeIncMs);
		return ripple.life < 0;
	});
}

std::span<const influence::Ripple> influence::Ripples()
{
	return detail::GlobalsOrDefault().ripples;
}

std::array<influence::RippleSprite, influence::Ripple::k_Sprites> influence::DrawRipple(size_t index,
                                                                                         uint32_t gameTimeIncMs)
{
	std::array<RippleSprite, Ripple::k_Sprites> sprites {};
	auto* globals = detail::TryGlobals();
	if (globals == nullptr || index >= globals->ripples.size())
	{
		return sprites;
	}
	auto& ripple = globals->ripples.at(index);
	// fade = life < 1000 ? life x 0.001 : 1; g = g_game_time_inc x 0.001 x 10
	const float fade = ripple.life < k_RippleFadeMs ? static_cast<float>(ripple.life) * k_RippleFadeRate : 1.0f;
	const float growth = static_cast<float>(gameTimeIncMs) * k_Milli * k_RippleGrowth;
	for (size_t i = 0; i < Ripple::k_Sprites; ++i)
	{
		float size = ripple.sizes.at(i) + growth;
		if (size > k_RippleWrap)
		{
			ripple.flags.at(i) = 0;
			size -= k_RippleWrap * static_cast<float>(static_cast<int32_t>(size * k_RippleInverseWrap));
		}
		// the alpha byte +0x37 of the ripple's colour. (inferido) s / 14 as a division: only the wrap's 1 / 14 was read
		const auto alpha =
		    static_cast<uint32_t>(static_cast<int32_t>((1.0f - size / k_RippleWrap) * fade * k_RippleAlpha)) & 0xFFu;
		ripple.colour = (ripple.colour & 0x00FFFFFFu) | (alpha << 24);
		ripple.sizes.at(i) = size > k_RippleMinSize ? size : k_RippleMinSize;
		sprites.at(i) = {ripple.sizes.at(i), ripple.angles.at(i), ripple.colour};
	}
	return sprites;
}

glm::vec3 influence::detail::CrossingPoint(const glm::vec3& centre, float radius, glm::vec3 inside, glm::vec3 outside)
{
	for (;;)
	{
		const float dx = outside.x - inside.x;
		const float dz = outside.z - inside.z;
		const glm::vec3 middle = (inside + outside) * k_Half;
		if (dx * dx + dz * dz <= k_BisectStop)
		{
			return middle;
		}
		(Inside(centre, radius, middle) ? inside : outside) = middle;
	}
}

bool influence::HandCrossedInfluence(const glm::vec3& handPosition)
{
	// fn_00827820: [0xEB9A6C] = 0 (0x82783D), then the per-player "the hand is inside one of this player's circles" bits
	// (0x82785D..0x82789A), over the list [0xEB9A14] the draw uses
	const auto circles = Circles();
	std::array<bool, k_Players> inside {};
	for (const auto& circle : circles)
	{
		if (Inside(circle.centre, circle.radius, handPosition))
		{
			inside[static_cast<size_t>(circle.player)] = true;
		}
	}
	bool crossed = false;
	if (!g_HaveState)
	{
		// 0x8279F8..0x827A10: the first call only remembers them, [0xEB9A68] = 1
		g_WasInside = inside;
		g_HaveState = true;
	}
	else
	{
		for (size_t player = 0; player < k_Players; ++player)
		{
			if (inside[player] == g_WasInside[player])
			{
				continue; // 0x8278AF: unchanged
			}
			g_WasInside[player] = inside[player]; // 0x8278C7, before the search
			// 0x8278CE: fn_008277B0 looks for the player's circle whose edge the hand crossed between the previous point
			// and this one; none (a circle that appeared, grew or shrank under a still hand) makes no ripple and no sound
			const auto* circle = CrossedCircle(circles, g_PreviousPoint, handPosition, player);
			if (circle == nullptr)
			{
				continue;
			}
			// 0x8278E0..0x8278EC: only when that circle's player has [0xEB9A1C] set (BoundaryShown), which the border's
			// draw reads too (0x826F15)
			if (!BoundaryShown(circle->player))
			{
				continue;
			}
			// the ripple (fn_00827670, fn_00827250) at the head of [0xEB9A44]; then [0xEB9A6C] = 1 (0x8279E2)
			auto& ripples = detail::Globals().ripples;
			ripples.insert(ripples.begin(), CrossingRipple(*circle, g_PreviousPoint, handPosition));
			crossed = true;
		}
	}
	// 0x827A12..0x827A2A: the point is remembered on every call
	g_PreviousPoint = glm::vec3(handPosition.x, 0.0f, handPosition.z);
	return crossed;
}

void influence::detail::ResetHandCrossing()
{
	g_WasInside = {};
	g_HaveState = false;
	g_PreviousPoint = glm::vec3(0.0f);
}

void influence::ProcessHandCrossing(const glm::vec3& handPosition)
{
	// 0x5E61B5..0x5E61C4: fn_00827820, then the sound only when it set [0xEB9A6C]
	if (!HandCrossedInfluence(handPosition))
	{
		return;
	}
	// 0x5E61C6..0x5E621E: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 52
	// G_HandThroughInfluence_01, no owner (+0x20 = 0), is3D 1 (+0x08), track 0 (+0x0C), at the hand's point
	// [0xE9A100]; mode and loops stay the ctor's (3 and 0) and the .sad overrides the mode with 1 (a new channel every
	// time) and gives volume 40 and min / max 100 / 300. Going in and coming out sound the same, and several players'
	// circles crossed at once still sound once.
	// (pending) GInterface::StartImmersion(6, 0x80000000) after it (0x5E6224): force feedback, not ported.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 52};
	options.owner = audio::Owner::None();
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}
