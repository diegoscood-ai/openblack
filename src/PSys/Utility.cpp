/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Utility.h"

#include <cmath>

#include <array>
#include <string>

#include <glm/geometric.hpp>

#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Camera/Camera.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "3D/LandIslandInterface.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "PSysManager.h"
#include "ParticleTypes.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
namespace gestures = magic::gestures;

/// PSysUtilityPSys (0xD4E0E8): +0 the trail (48), +4 its "active", +8 the recognised sparkles (35), +0x20 the
/// selection (28), +0x24 its "active"
struct UtilityPSys
{
	uint32_t trail {0};
	bool trailActive {false};
	uint32_t recognised {0};
	uint32_t selection {0};
	bool selectionActive {false};
};
UtilityPSys g_Utility;
std::vector<magic::gestures::RecognisedGesture> g_Pending;

/// fn_00671110 / fn_00671260 / fn_006711D0: PSysInterface::Create(NULL, type, 0, 0, 1.0, NET 0) once. The trail
/// (fn_00671110 0x671172..0x671197) and the recognised sparkles (fn_00671260 0x6712CD..0x6712EA) then SetPlayer (vt
/// 0x20) the local player (g_game +0x205A59; openblack: PLAYER_ONE, inferido), so SF_GestureChain's
/// ParticleChainCreator0 (UsePlayerColor 1) is drawn in its colour; the selection (fn_006711D0) gets no player
uint32_t CreateOnce(uint32_t& slot, ParticleType type, bool localPlayer)
{
	if (slot != 0 && manager::Find(slot) == nullptr)
	{
		slot = 0;
	}
	if (slot == 0)
	{
		const auto file = ParticleTypeFile(type);
		if (!file.empty())
		{
			slot = manager::StartForSpell(std::string(file), glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, nullptr);
			manager::SetPerFrame(slot);
			if (auto* effect = manager::Find(slot); effect != nullptr && localPlayer)
			{
				effect->SetPlayer(static_cast<int>(PlayerNames::PLAYER_ONE));
			}
		}
	}
	return slot;
}

/// The trail's magnitude table (0xC0213C / 0xC0214C): camera distance {0, 50, 500, 1500} -> {0.2, 1, 1, 1.5}
float TrailScale(float distance)
{
	constexpr std::array<float, 4> k_Distances {0.0f, 50.0f, 500.0f, 1500.0f};
	constexpr std::array<float, 4> k_Values {0.2f, 1.0f, 1.0f, 1.5f};
	if (distance <= k_Distances[0])
	{
		return k_Values[0];
	}
	if (distance >= k_Distances[3])
	{
		return k_Values[3];
	}
	for (size_t i = 0; i + 1 < k_Distances.size(); ++i)
	{
		if (distance < k_Distances[i + 1])
		{
			const float t = (distance - k_Distances[i]) / (k_Distances[i + 1] - k_Distances[i]);
			return k_Values[i] + (k_Values[i + 1] - k_Values[i]) * t;
		}
	}
	return k_Values[3];
}

/// The gesture trail's condition (fn_00671DA0): the game expects a gesture
bool TrailWanted()
{
	const auto& hand = gestures::GetHandStatus();
	const auto& state = gestures::State();
	// m_Buttons bit 0x02 (no setter found) and byte [0xD17D10] (unknown) are not ported
	if (gestures::HoldingChargingSeed())
	{
		return true;
	}
	// fn_005CF200: a seed in the hand with m_Held & 8
	if (hand.heldSeed != entt::null && state.heldFlag8)
	{
		return true;
	}
	// fn_005CF1C0: the selection open with the hand ready
	if (state.selection.open && hand.handReady)
	{
		return true;
	}
	// a circle-sized seed in the hand (fn_00729AC0 == 4)
	if (hand.heldSeed != entt::null && Locator::entitiesRegistry::value().Valid(hand.heldSeed))
	{
		const auto* seed = Locator::entitiesRegistry::value().TryGet<const ecs::components::SpellSeed>(hand.heldSeed);
		if (seed != nullptr && magic::seed::InfoOf(*seed).sizingGesture == GestureType::Circle)
		{
			return true;
		}
	}
	return false;
}

void Step(uint32_t id, const glm::vec3& handPosition, bool enabled, float magnitude, float seconds)
{
	auto* effect = manager::Find(id);
	if (effect == nullptr)
	{
		return;
	}
	ProcessInfo info;
	info.handPos = handPosition; // +0x0C = CHand +0x78
	info.power = 1.0f;
	info.enabled = enabled;
	effect->SetMagnitude(magnitude); // vt 0x11C
	manager::ProcessForSpell(id, info, seconds); // vt 0xFC Process_(&info, g_game_time_inc)
}
} // namespace

void utility::GestureRecognised(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result)
{
	// fn_00689790: at least 2 samples and a gesture
	if (system.Count() < 2 || result.gesture == gestures::k_None)
	{
		return;
	}
	// the pixel box of the matched samples (0xD4EB00)
	int first = 0;
	int last = 0;
	system.KeypointIndices(result.start, result.end, first, last);
	auto box = system.Box(first, last);
	// the land points of the whole buffer, those not at (0, 0, 0)
	std::vector<glm::vec3> stroke;
	for (int i = 0; i < system.Count(); ++i)
	{
		const auto& world = system.At(i).world;
		// (aproximado) fn_00689790 0x68989E..0x6898C2 compares fabs(x), fabs(y), fabs(z) with the double at 0x8C79D8,
		// whose value the notes do not decode; 1e-4 stands for it
		if (std::abs(world.x) > 1e-4f || std::abs(world.y) > 1e-4f || std::abs(world.z) > 1e-4f)
		{
			stroke.push_back(world);
		}
	}
	if (stroke.size() < 2)
	{
		return;
	}
	// fn_0068C650 (the gesture's shape) and fn_0068C140: the box keeps its centre, its half sizes divided by the shape's
	const auto& shape = gestures::ShapeOf(result.gesture);
	{
		const float cx = (box.minX + box.maxX) * 0.5f;
		const float hx = (box.maxX - box.minX) * 0.5f / (shape.maxX - shape.minX);
		const float cz = (box.maxZ + box.minZ) * 0.5f;
		const float hz = (box.maxZ - box.minZ) * 0.5f / (shape.maxZ - shape.minZ);
		box = {cx - hx, cz - hz, cx + hx, cz + hz};
	}
	// the camera's forward (0xEA1DD4) on the ground, (1, 0, 0) if it looks straight down; r is its right
	const auto projection = gestures::sampling::CurrentProjection();
	glm::vec3 forward = Locator::camera::has_value() ? Locator::camera::value().GetForward() : glm::vec3(1.0f, 0.0f, 0.0f);
	forward.y = 0.0f;
	if (forward.x * forward.x + forward.z * forward.z < 1e-4f)
	{
		forward = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	forward = glm::normalize(forward);
	const glm::vec2 right(forward.z, -forward.x);
	// fn_00689F20: a shape point in the box's pixels, then the land under it (its altitude), or 400 m along the ray
	auto onLand = [&](const glm::vec3& point) {
		const glm::vec2 pixel(static_cast<float>(static_cast<int>((box.maxX - box.minX + 1.0f) * point.x + box.minX)),
		                      static_cast<float>(static_cast<int>((box.maxZ - box.minZ + 1.0f) * point.z + box.minZ)));
		if (auto land = gestures::sampling::ScreenToLand(pixel); land)
		{
			if (Locator::terrainSystem::has_value())
			{
				land->y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(land->x, land->z));
			}
			return *land;
		}
		const glm::vec3 direction = projection.rayDirection ? projection.rayDirection(pixel) : glm::vec3(0.0f);
		return projection.cameraPosition + direction * 400.0f; // fn_0074CAF0
	};
	// the shape is squashed vertically on the screen (x 0.75 each time) until its depth on the land is no more than
	// twice its width, at most 15 times (fn_00689790: 0.75 [0xC02650] at 0x689C20, 15 [0xC0264C] at 0x689C26, the
	// ratio against 2 [0x8AB478] at 0x689C37)
	std::vector<glm::vec3> ideal;
	float squash = 1.0f;
	for (int iteration = 1;; ++iteration)
	{
		ideal.clear();
		for (const auto& p : shape.points)
		{
			ideal.push_back(onLand(glm::vec3(p.x, p.y, (p.z - 0.5f) * squash + 0.5f)));
		}
		float minA = 1e7f;
		float maxA = -1e7f;
		float minB = 1e7f;
		float maxB = -1e7f;
		for (const auto& p : ideal)
		{
			const float a = right.x * p.x + right.y * p.z;
			const float b = glm::dot(forward, p);
			minA = std::min(minA, a);
			maxA = std::max(maxA, a);
			minB = std::min(minB, b);
			maxB = std::max(maxB, b);
		}
		const float width = std::abs(maxA - minA);
		const float ratio = width > 0.0f ? std::abs(maxB - minB) / width : 1.0f;
		squash *= 0.75f;
		if (iteration >= 15 || !(ratio > 2.0f))
		{
			break;
		}
	}
	if (ideal.empty())
	{
		return;
	}
	// the record (0x48, PSysGesture.cpp): the stroke, the ideal resampled to as many points, this interface's status
	gestures::RecognisedGesture record;
	for (const auto& p : stroke)
	{
		record.stroke.Add(p);
	}
	const size_t n = stroke.size();
	const size_t m = ideal.size();
	for (size_t k = 0; k < n; ++k)
	{
		const float u = static_cast<float>(k) * (1.0f / static_cast<float>(n - 1));
		if (u == 1.0f || m < 2)
		{
			record.ideal.Add(ideal.back());
			continue;
		}
		const float f = u * static_cast<float>(m - 1);
		const auto index = std::min(static_cast<size_t>(f), m - 2);
		record.ideal.Add(ideal[index] + (ideal[index + 1] - ideal[index]) * (f - static_cast<float>(index)));
	}
	// +0x38 the status (MyInterface +0x39C), +0x3C its hand position (status +0xC8)
	record.fromInterface = true;
	if (Locator::handSystem::has_value())
	{
		record.handPosition = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	g_Pending.push_back(std::move(record));
	// PSysUtilityPSys: SF_Gesture (35) is created once and stepped every frame (Update); its rule takes the record
	CreateOnce(g_Utility.recognised, ParticleType::Gesture, true);
}

std::vector<magic::gestures::RecognisedGesture>& utility::PendingRecognised()
{
	return g_Pending;
}

void utility::Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance)
{
	if (seconds <= 0.0f)
	{
		return; // g_game_time_inc > 0 only
	}
	// the trail (48): on while a gesture is expected (immersion 8 GESTURE_TRAIL: force feedback, not ported)
	if (const auto trail = CreateOnce(g_Utility.trail, ParticleType::GestureLocal, true); trail != 0)
	{
		g_Utility.trailActive = TrailWanted();
		Step(trail, handPosition, g_Utility.trailActive, handScale * TrailScale(cameraDistance), seconds);
	}
	// the selection (28): while the selection (or the leash selection) is open (immersion 9). Not ported: the leash
	// selection case, only the normal selection is tested
	if (const auto selection = CreateOnce(g_Utility.selection, ParticleType::SpellSelection, false); selection != 0)
	{
		const auto& state = gestures::State();
		g_Utility.selectionActive = state.selection.open && gestures::GetHandStatus().handReady;
		Step(selection, handPosition, g_Utility.selectionActive, handScale, seconds);
	}
	// the recognised sparkles (35): stepped and drawn every frame (vt 0x108)
	if (g_Utility.recognised != 0 && manager::Find(g_Utility.recognised) != nullptr)
	{
		Step(g_Utility.recognised, handPosition, true, 1.0f, seconds);
	}
}

void utility::Reset()
{
	g_Utility = UtilityPSys {};
	g_Pending.clear();
}
