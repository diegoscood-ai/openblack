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
#include "GameClock.h"
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

/// PSysUtilityPSys (0xD4E0E8, 0x40 bytes made by PSysGlobal::InitializeOneTimeOnly 0x68F779): +0 the trail (48), +4
/// its "active", +8 the recognised sparkles (35), +0xC SF_OnFire (37), +0x14 SF_LightningStrike (61), +0x18
/// SF_ManaPathNew (22), +0x1C SF_BeliefSprite (24), +0x20 the selection (28), +0x24 its "active". (+0x10, the
/// exploded meshes' SF_ExplodeObject, is PSys/Rules/ExplodeObject.cpp's own)
struct UtilityPSys
{
	uint32_t trail {0};
	bool trailActive {false};
	uint32_t recognised {0};
	uint32_t onFire {0};
	uint32_t lightningStrike {0};
	uint32_t manaPath {0};
	uint32_t belief {0};
	uint32_t selection {0};
	bool selectionActive {false};
};
UtilityPSys g_Utility;
std::vector<magic::gestures::RecognisedGesture> g_Pending;

/// fn_00671110 / fn_00671260 / fn_006711D0: PSysInterface::Create(NULL, type, 0, 0, 1.0, NET 0) once. The trail
/// (fn_00671110 0x671172..0x671197) and the recognised sparkles (fn_00671260 0x6712CD..0x6712EA) then SetPlayer (vt
/// 0x20) the local player (g_game +0x205A59; openblack: PLAYER_ONE, inferido), so SF_GestureChain's
/// ParticleChainCreator0 (UsePlayerColor 1) is drawn in its colour; the selection (fn_006711D0) gets no player
uint32_t CreateOnce(uint32_t& slot, ParticleType type, bool localPlayer, bool perFrame = true,
                    game_random::psys::NetGameType net = game_random::psys::NetGameType::Local)
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
			slot = manager::StartForSpell(std::string(file), glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, nullptr, net);
			if (perFrame)
			{
				manager::SetPerFrame(slot);
			}
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
/// fn_006717F0 / fn_00671740 / fn_00671B40 / fn_00671CD0 / fn_00672100 / fn_006719E0: the slot made when missing,
/// Process_(info, [0xD01A38]) (vt 0xFC) with every field 0 but power 1.0 (+0x30) and enabled (+0x38); 5 deletes it
/// (vt 4) and clears the slot, made again on the next turn
void StepTurn(uint32_t& slot, ParticleType type, bool localPlayer, game_random::psys::NetGameType net)
{
	if (CreateOnce(slot, type, localPlayer, false, net) == 0)
	{
		return;
	}
	ProcessInfo info;
	info.power = 1.0f;
	info.enabled = true;
	if (!manager::ProcessForSpell(slot, info, static_cast<float>(game_clock::MsPerTurn()) * 0.001f))
	{
		slot = 0;
	}
}
} // namespace

void utility::ProcessTurn()
{
	// the gates [0xC029FC] (+0x0C), [0xC02A00] (+0x18, +0x1C) and [0xC02A04] (+0x08) are 1 in .data and never written
	// (refs: reads only); the slots' NET_GAME_TYPE: 1 for +0x0C (0x6716EE) and +0x14 (0x67198E), 0 for the others
	using game_random::psys::NetGameType;
	StepTurn(g_Utility.onFire, ParticleType::OnFire, false, NetGameType::Synced);
	StepTurn(g_Utility.manaPath, ParticleType::ManaPath, false, NetGameType::Local);
	// +0x1C and +0x08: SetPlayer (vt 0x20) the local player, g_game + 0x18 + [g_game +0x205A59] x 0xA60 (PLAYER_ONE)
	StepTurn(g_Utility.belief, ParticleType::BeliefSprite, true, NetGameType::Local);
	StepTurn(g_Utility.recognised, ParticleType::Gesture, true, NetGameType::Local);
	StepTurn(g_Utility.lightningStrike, ParticleType::LightningStrike, false, NetGameType::Synced);
}

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
		// fn_00689790 0x68989E..0x6898C2 compares fabs(x), fabs(y), fabs(z) with the double at 0x8C79D8 =
		// 9.9999997473787516e-05, the float 1e-4f widened (so the float compare is exact)
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
	// PSysUtilityPSys: SF_Gesture (35) is stepped once a turn (ProcessTurn, fn_00672100) and drawn every frame with the
	// turn's fraction (fn_00671DA0 0x671DD3); its rule takes the record
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
	// the recognised sparkles (35) are stepped once a turn (ProcessTurn) and drawn here with Draw_(1) (0x671DD3).
	// (pending) the original draws them only while MyInterface +0x3A0 != 0
}

void utility::Reset()
{
	g_Utility = UtilityPSys {};
	g_Pending.clear();
}
