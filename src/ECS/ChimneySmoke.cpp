/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChimneySmoke.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string_view>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Game.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::ChimneySmoke;

namespace
{
/// Random 0x81D180 (a uniform float in [min, max]); the smoke is only drawn, so it has its own generator and does not
/// move the game's synced one
float Random(float min, float max)
{
	static std::mt19937 generator(0x5E0C4E);
	return std::uniform_real_distribution<float>(min, max)(generator);
}

/// fn_007F8E00's globals, set by GLandscape::Draw (crt init 0x7F8DA0 / 0x7F8DD0: handWind (1, 0, 0), handPos
/// (-10000, 0, 0); handSpeed in the bss, 0)
struct HandWind
{
	glm::vec3 handPosition {-10000.0f, 0.0f, 0.0f}; ///< [0xE9A100]
	glm::vec3 wind {1.0f, 0.0f, 0.0f};              ///< [0xE9A110]
	float speed {0.0f};                             ///< [0xE9A124]
};
HandWind g_Hand;

/// GInterfaceStatus::HandVelocity (+0x10C) and the hand position of the last turn
struct HandVelocity
{
	glm::vec3 velocity {0.0f};
	glm::vec3 lastPosition {0.0f};
	uint32_t lastTurn {0};
	bool started {false};
};
HandVelocity g_Velocity;

constexpr int32_t k_Life = 900;              ///< 0x384: the age at which a puff is reborn
constexpr float k_SpinPerSecond = 0.765f;    ///< 0x9A22E0
constexpr float k_RisePerSecond = 2.55f;     ///< 0x9A22DC, along +0xF8 = (0, 1, 0)
constexpr float k_DriftGain = 1.5f;          ///< 0x8AB24C
constexpr float k_HandRadiusSquared = 225.0f; ///< 0x9A22E4: the hand within 15 units of the chimney
constexpr float k_TurnMilliseconds = 100.0f; ///< [0xD01A38], the length of a game turn (GGame, 100 ms)
} // namespace

glm::vec3 chimney_smoke::ChimneyWorldPosition(const glm::vec3& meshPoint, const components::Transform& transform)
{
	// the object's matrix is the rotation times the scale (the same as the building's model matrix)
	return transform.position + transform.rotation * (meshPoint * transform.scale);
}

ChimneySmoke chimney_smoke::Create(const glm::vec3& chimney, uint32_t rgb)
{
	ChimneySmoke smoke;
	smoke.position = chimney;
	smoke.rgb = rgb;
	for (uint32_t i = 0; i < ChimneySmoke::k_Puffs; ++i)
	{
		auto& puff = smoke.puffs.at(i);
		// LH3DSmoke::Create: sprite pos = (0, i x 0.5, 0) + the point (NULL from Abode, so near the origin; they are all
		// hidden, and the first rebirth puts them at the chimney)
		puff.position = glm::vec3(0.0f, static_cast<float>(i) * 0.5f, 0.0f);
		puff.hidden = true;
		puff.age = static_cast<int32_t>(i) * 90;
		puff.angle = Random(0.0f, glm::pi<float>());
		puff.clockwise = (static_cast<int32_t>(Random(1.0f, 100.0f)) & 1) != 0;
		puff.velocity = glm::vec3(0.0f);
	}
	return smoke;
}

void chimney_smoke::Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform,
                           bool workshop)
{
	const auto& point = mesh.GetChimneyPos();
	if (!point.has_value())
	{
		return;
	}
	// Abode::CallVirtualFunctionsForCreation 0x403200: smoke->colour = IsWorkshop() ? 0x808080 : 0xFFFFFF
	Locator::entitiesRegistry::value().Assign<ChimneySmoke>(
	    abode, Create(ChimneyWorldPosition(*point, transform), workshop ? 0x808080u : 0xFFFFFFu));
}

void chimney_smoke::UpdateHandWind()
{
	if (!Locator::handSystem::has_value() || Game::Instance() == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto hands = Locator::handSystem::value().GetPlayerHands();
	const auto hand = hands[static_cast<size_t>(systems::HandSystemInterface::Side::Left)];
	if (!registry.Valid(hand) || !registry.AllOf<components::Transform>(hand))
	{
		return;
	}
	// the hand's 3D object (interface+0x3A0 -> CHand+0x482C) has a mesh only while the hand is in the world; openblack
	// leaves the hand at the origin when it has no place (HandSystem::GetPlayerHandPositions) (approximation)
	const glm::vec3 position = registry.Get<components::Transform>(hand).position;
	if (position == glm::vec3(0.0f))
	{
		return;
	}

	// fn_005DBC60 (GInterfaceStatus::Process 0x5DC4E0, once per turn, inferred): velocity += 0.6 x (delta x 1000 / 100 -
	// velocity), delta = the hand's motion in that turn
	const uint32_t turn = Game::Instance()->GetTurn();
	if (!g_Velocity.started)
	{
		g_Velocity = {glm::vec3(0.0f), position, turn, true};
	}
	else if (turn != g_Velocity.lastTurn)
	{
		const uint32_t turns = std::min<uint32_t>(turn - g_Velocity.lastTurn, 10u);
		const glm::vec3 delta = (position - g_Velocity.lastPosition) / static_cast<float>(turns);
		for (uint32_t i = 0; i < turns; ++i)
		{
			g_Velocity.velocity += 0.6f * (delta * (1000.0f / k_TurnMilliseconds) - g_Velocity.velocity);
		}
		g_Velocity.lastPosition = position;
		g_Velocity.lastTurn = turn;
	}

	// GLandscape::Draw 0x5E4337..: handPos = the object's position; handWind = HandVelocity; handSpeed =
	// clamp(|v| x 0.1, 0.5, 5); if v != 0, handWind = v / |v| x handSpeed
	const glm::vec3 v = g_Velocity.velocity;
	g_Hand.handPosition = position;
	g_Hand.wind = v;
	const float length = glm::length(v);
	g_Hand.speed = std::clamp(length * 0.1f, 0.5f, 5.0f);
	if (v != glm::vec3(0.0f))
	{
		g_Hand.wind = v * (g_Hand.speed / length);
	}
}

bool chimney_smoke::UpdateState(ChimneySmoke& smoke, bool lit)
{
	if (lit)
	{
		smoke.state = ChimneySmoke::State::Active;
	}
	else if (smoke.state == ChimneySmoke::State::Active)
	{
		smoke.state = ChimneySmoke::State::Dying;
	}
	return smoke.state != ChimneySmoke::State::Dead;
}

void chimney_smoke::Advance(ChimneySmoke& smoke, float milliseconds, std::vector<DrawnPuff>& drawn)
{
	// The frame's drift, the same for the 10 puffs (+0x108 "no hand" and +0x109 "burst" are never set for a chimney):
	// the hand's wind when the hand is within 15 units of the chimney and faster than 1, else a new random one
	glm::vec3 drift;
	const glm::vec3 toHand = g_Hand.handPosition - smoke.position;
	if (glm::dot(toHand, toHand) < k_HandRadiusSquared && g_Hand.speed > 1.0f)
	{
		drift = g_Hand.wind;
	}
	else
	{
		// the first Random goes to z, the second to x
		const float z = Random(-3.0f, 3.0f);
		const float x = Random(-3.0f, 3.0f);
		drift = glm::vec3(x, 0.0f, z);
	}

	// dt = g_game_time_inc x 0.001, at most 100
	const float dt = std::min(milliseconds * 0.001f, 100.0f);
	const bool dying = smoke.state == ChimneySmoke::State::Dying;
	const float spin = dt * k_SpinPerSecond;
	// The original truncates the age step every frame (ftol(dt x 255)) and loses the fraction: 4 at 60 fps (a life of
	// 3.75 s), 1 at 144 fps and 0 above 255 fps, where the smoke would stop. openblack runs without vsync by default,
	// so the fraction is kept (as the mists, RendererMists.cpp, and Clouds.cpp): the life is 900 / 255 = 3.53 s
	// (frame_anim::SmokeAgeStep: the same dt, x 255 [0x8AB270])
	const int32_t ageStep = graphics::frame_anim::SmokeAgeStep(smoke.ageRemainder, milliseconds);

	uint32_t count = 0;
	for (auto& puff : smoke.puffs)
	{
		puff.age += ageStep;
		puff.angle += puff.clockwise ? spin : -spin;
		float tau = dt;
		if (puff.age > k_Life)
		{
			// reborn at the chimney with what is left of the age, hidden if the smoke is dying
			puff.position = smoke.position;
			puff.age %= k_Life;
			puff.hidden = dying;
			puff.velocity = glm::vec3(0.0f);
			tau = static_cast<float>(puff.age) * (1.0f / 255.0f); // 0x900058
		}
		// trapezoid: v' = v + 1.5 tau W; p += (v + v') tau / 2; then the rise along (0, 1, 0) at 2.55 per second
		const glm::vec3 next = puff.velocity + k_DriftGain * tau * drift;
		puff.position += (puff.velocity + next) * (tau * 0.5f);
		puff.velocity = next;
		puff.position += glm::vec3(0.0f, 1.0f, 0.0f) * (k_RisePerSecond * tau);

		// cell (age x 45 / 900) & 15 (three turns of the animation in a life); half width max(0.0001, (age / 450 +
		// 0.5) x scale(1)); alpha 79 up to 225, then (225 - age) x 79 / 675 + 79 (integer), 0 at 900
		const auto cell = static_cast<uint32_t>(graphics::frame_anim::MistCell(puff.age));
		const float halfWidth = std::max(0.0001f, static_cast<float>(puff.age) * (1.0f / 450.0f) + 0.5f);
		const int32_t alpha = puff.age > 225 ? (225 - puff.age) * 79 / 675 + 79 : 79;
		const uint32_t argb = (static_cast<uint32_t>(alpha) << 24u) | smoke.rgb;
		if (!puff.hidden)
		{
			drawn.push_back({puff.position, halfWidth, puff.angle, cell, argb});
			++count;
		}
	}
	if (dying && count == 0)
	{
		smoke.state = ChimneySmoke::State::Dead;
	}
}

bool chimney_smoke::ForcedByTestHook()
{
	static const bool forced = []() {
		const char* value = std::getenv("OPENBLACK_TEST_CHIMNEY");
		return value != nullptr && std::string_view(value) == "all";
	}();
	return forced;
}
