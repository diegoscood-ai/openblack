/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandMagicFX.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "PSys/Rules/SurfRevol.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
/// fn_0068CC70: GJUtils::GetSharedMesh(".\data\spells\meshes\Power_Up_Band.L3d") (the same shared mesh as the
/// worship icons' power-up band)
constexpr auto k_BandMesh = entt::hashed_string("Power_Up_Band");

// The PHandFX constants (ctor 0x68CB10)
constexpr float k_BandScale = 10.0f;          ///< +0x0C
constexpr float k_BandSpin = 12.0f;           ///< +0x14 rad/s, x (1 + 0.2 index)
constexpr float k_BandOffset = 10.0f;         ///< +0x18 along the bone's y
constexpr float k_BandStep = 40.0f;           ///< +0x1C per index
constexpr float k_ChargeDurationFrom = 3.5f;  ///< +0x24
constexpr float k_ChargeDurationTo = 1.0f;    ///< +0x28
constexpr float k_ChargeIntervalFrom = 6.0f;  ///< +0x2C
constexpr float k_ChargeIntervalTo = 0.3f;    ///< +0x30
constexpr uint8_t k_TemporaryAlpha1 = 120;    ///< +0x34
constexpr uint8_t k_TemporaryAlpha0 = 20;     ///< +0x35
constexpr uint8_t k_PermanentAlpha1 = 130;    ///< +0x36
constexpr uint8_t k_PermanentAlpha0 = 20;     ///< +0x37
constexpr float k_BandDuration = 0.85f;       ///< +0x38
constexpr float k_FlyScale = 4.0f;            ///< +0x3C: the distance in front of the camera (inf; no near + 0.2 clamp)
constexpr float k_FlyShrink = 0.5f;           ///< +0x40
constexpr float k_GlowAlpha = 0.8f;           ///< the +0x54 target
constexpr float k_GlowRate = -20.0f;          ///< +0x5C frames per second
constexpr int k_GlowFrames = 32;              ///< +0x60
constexpr float k_DelayedStart = 2.4f;        ///< fn_0068DE90
constexpr int k_MaxPermanentBands = 5;

/// PHandFX::Band (0x48 bytes, vtable 0x936B1C; ctor fn_0068CA30)
struct Band
{
	entt::entity entity {entt::null}; ///< +0x1C the LH3DObject
	int index {0};                    ///< +0x40
	float angle {0.0f};               ///< +0x20
	float time {0.0f};                ///< +0x24
	float start {0.0f};               ///< +0x28
	float duration {0.0f};            ///< +0x2C
	uint8_t alpha0 {0};               ///< +0x30
	uint8_t alpha1 {0};               ///< +0x31
	bool permanent {false};           ///< +0x3C
	bool done {false};                ///< +0x44
	bool reverse {false};             ///< +0x46
};

struct State
{
	std::vector<Band> permanent; ///< +0x44 / +0x48, the newest first
	std::vector<Band> temporary; ///< +0x4C / +0x50, the newest first
	float glowAlpha {0.0f};      ///< +0x54
	float glowFrame {0.0f};      ///< +0x58
	float chargeTimer {0.0f};    ///< +0x6C
	bool wasCharging {false};    ///< +0x70
	bool charging {false};       ///< +0x71
	glm::vec2 glowUv {0.0f};
	// CHand +0x494C..: the in-hand effect
	uint32_t inHandEffect {0};
	entt::entity inHandSeed {entt::null}; ///< CHand +0x4950
	bool inHandAtBone {false};            ///< CHand +0x4954
};
State g_State;

bool LoadBandMesh()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(k_BandMesh))
	{
		return true;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		meshes.Load(k_BandMesh, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "Meshes" / "Power_Up_Band.L3d"));
		// CreatePUBand 0x727097..0x7270B8 (and PHandFX fn_0068CC70 0x68CC7D..0x68CC99, the same mesh): GJUtils::GetSharedMesh
		// with MaterialProperties {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so SetMaterialProperties 0x57E120
		// turns the band into mode 13 (SRCALPHA / ONE, no Z write, fn_0082ECD0)
		meshes.Handle(k_BandMesh)->SetMaterialProperties(
		    {.additive = true, .zWrite = false, .doubleSided = true, .change = true, .alpha = true});
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Hand FX: cannot load Power_Up_Band.L3d: {}", e.what());
		return false;
	}
	return true;
}

void PlayInGame(audio::SoundId id)
{
	const auto sound = static_cast<entt::id_type>(id);
	if (Locator::audio::has_value() && Locator::resources::value().GetSounds().Contains(sound))
	{
		Locator::audio::value().PlaySound(sound, audio::PlayType::Once);
	}
}

/// fn_0068CA30: the band's LH3DObject (here an entity, invisible until it starts)
Band MakeBand(int index, float start, bool permanent, float duration, uint8_t alpha0, uint8_t alpha1, bool reverse)
{
	Band band;
	band.index = index;
	band.start = start;
	band.permanent = permanent;
	band.duration = duration;
	band.alpha0 = alpha0;
	band.alpha1 = alpha1;
	band.reverse = reverse;
	if (LoadBandMesh() && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		band.entity = registry.Create();
		registry.Assign<Transform>(band.entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(band.entity, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<Alpha>(band.entity, 0.0f);
		// Band::Draw 0x68D849..0x68D8AB, every draw: +0x4C = GetPlayerColour 0x64D800 of the local player (g_game
		// +0x205A59; openblack: PLAYER_ONE, inferido) with the band's alpha byte (components::Alpha, DrawBand), and
		// 0x68D8B1 +0x50 (the specular) = the per-channel lerp of the ctor's colours +0x34 / +0x38 (fn_0068CA30 args 8,
		// 9), 0 for every caller (fn_0068CCC0, fn_0068CD30, fn_0068CDA0, DoRemoveFromHandVisual 0x68CF05 / 0x68CF07)
		const uint32_t rgb = psys::surf_revol::PlayerColour(static_cast<int>(PlayerNames::PLAYER_ONE));
		registry.Assign<ObjectColour>(
		    band.entity,
		    ObjectColour {{static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)}});
		registry.Assign<HandFxPart>(band.entity);
		registry.SetDirty();
	}
	return band;
}

void DestroyBand(Band& band)
{
	if (band.entity != entt::null && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(band.entity))
		{
			registry.Destroy(band.entity);
			registry.SetDirty();
		}
	}
	band.entity = entt::null;
}

/// fn_0068CCC0: a permanent band (index = count + 1), at the head
void AddPermanentBand(float start)
{
	auto band = MakeBand(static_cast<int>(g_State.permanent.size()) + 1, start, true, k_BandDuration, k_PermanentAlpha0,
	                     k_PermanentAlpha1, false);
	g_State.permanent.insert(g_State.permanent.begin(), band);
}

/// fn_0068CD30: a temporary band (index = count), at the head
void AddTemporaryBand(float start)
{
	auto band = MakeBand(static_cast<int>(g_State.temporary.size()), start, false, k_BandDuration, k_TemporaryAlpha0,
	                     k_TemporaryAlpha1, false);
	g_State.temporary.insert(g_State.temporary.begin(), band);
}

/// fn_0068CDA0: a charge band of charge c: duration lerp(3.5, 1, c), alpha lerp(3, 5, c) -> lerp(15, 50, c)
void AddChargeBand(float charge)
{
	const float duration = k_ChargeDurationFrom + (k_ChargeDurationTo - k_ChargeDurationFrom) * charge;
	const auto alpha0 = static_cast<uint8_t>(static_cast<int>(3.0f + 2.0f * charge));
	const auto alpha1 = static_cast<uint8_t>(static_cast<int>(15.0f + 35.0f * charge));
	auto band = MakeBand(static_cast<int>(g_State.temporary.size()), 0.0f, false, duration, alpha0, alpha1, false);
	g_State.temporary.insert(g_State.temporary.begin(), band);
}

/// The hand's root bone in the world (the first 0x30 bytes of CHand +0x47F0)
glm::mat4 HandBoneMatrix()
{
	if (!Locator::handSystem::has_value())
	{
		return glm::mat4(1.0f);
	}
	const auto& hand = Locator::handSystem::value();
	glm::mat4 bone(1.0f);
	if (const auto* bones = hand.GetBoneMatrices(); bones != nullptr && !bones->empty())
	{
		bone = (*bones)[0];
	}
	return hand.GetHandMatrix() * bone;
}

/// LH3D's camera matrix 0xEA1CF8 (inf: the camera's world matrix) with the band 4 m in front, at half its size. The
/// handedness (right = up x forward) and the column order are also inferido: they decide the side the bands fly from.
glm::mat4 CameraFlyMatrix()
{
	if (!Locator::camera::has_value())
	{
		return glm::mat4(1.0f);
	}
	const auto& camera = Locator::camera::value();
	const auto forward = glm::normalize(camera.GetForward());
	const auto up = glm::normalize(camera.GetUp());
	const auto right = glm::normalize(glm::cross(up, forward));
	glm::mat4 world(glm::vec4(right, 0.0f), glm::vec4(up, 0.0f), glm::vec4(forward, 0.0f), glm::vec4(camera.GetOrigin(), 1.0f));
	return world * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, k_FlyScale)) *
	       glm::scale(glm::mat4(1.0f), glm::vec3(k_FlyShrink));
}

void SetTransform(entt::entity entity, const glm::mat4& matrix, float alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == entt::null || !registry.Valid(entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(entity);
	const float scale = glm::length(glm::vec3(matrix[0]));
	transform.position = glm::vec3(matrix[3]);
	transform.scale = glm::vec3(scale);
	transform.rotation = scale > 1e-6f ? glm::mat3(matrix) / scale : glm::mat3(1.0f);
	registry.Get<Alpha>(entity).value = alpha;
}

/// PHandFX::Band::Draw 0x68D6D0: spins once grown, flies from the camera (the fly matrix) onto the hand's root bone
/// (a permanent band lerps the matrices, a temporary one slerps), alpha lerp(alpha0, alpha1, f)
void DrawBand(Band& band, float dt, const glm::mat4& bone, const glm::mat4& fly)
{
	band.time += dt;
	const float t = band.time - band.start;
	if (t <= 0.0f)
	{
		SetTransform(band.entity, glm::mat4(1.0f), 0.0f);
		return;
	}
	band.angle = std::fmod(band.angle + (1.0f + 0.2f * static_cast<float>(band.index)) * k_BandSpin * dt, glm::two_pi<float>());
	float f = t / band.duration;
	if (f > 1.0f)
	{
		band.done = true;
	}
	if (band.reverse)
	{
		f = 1.0f - f;
	}
	f = std::clamp(f, 0.0f, 1.0f);
	const float alpha = (static_cast<float>(band.alpha0) + (static_cast<float>(band.alpha1) - static_cast<float>(band.alpha0)) * f) / 255.0f;
	// 10 I, spun about y only once it has arrived, at y = 10 + 40 index along the bone (hand model units)
	glm::mat4 local = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, k_BandOffset + k_BandStep * static_cast<float>(band.index), 0.0f));
	if (f >= 1.0f)
	{
		local = local * glm::rotate(glm::mat4(1.0f), band.angle, glm::vec3(0.0f, 1.0f, 0.0f));
	}
	local = local * glm::scale(glm::mat4(1.0f), glm::vec3(k_BandScale));
	const glm::mat4 onHand = bone * local;
	if (f >= 1.0f)
	{
		SetTransform(band.entity, onHand, alpha);
		return;
	}
	glm::mat4 result;
	if (band.permanent)
	{
		result = fly + (onHand - fly) * f;
	}
	else
	{
		// fn_0044CF90 / fn_0044E9F0: the rotations slerped, the translations and the scales lerped
		const float s0 = glm::length(glm::vec3(fly[0]));
		const float s1 = glm::length(glm::vec3(onHand[0]));
		const auto q0 = glm::quat_cast(glm::mat3(fly) / std::max(s0, 1e-6f));
		const auto q1 = glm::quat_cast(glm::mat3(onHand) / std::max(s1, 1e-6f));
		const auto rotation = glm::mat4_cast(glm::slerp(q0, q1, f));
		const glm::vec3 position = glm::mix(glm::vec3(fly[3]), glm::vec3(onHand[3]), f);
		result = glm::translate(glm::mat4(1.0f), position) * rotation * glm::scale(glm::mat4(1.0f), glm::vec3(s0 + (s1 - s0) * f));
	}
	SetTransform(band.entity, result, alpha);
}

const ecs::components::SpellSeed* InHandSeed()
{
	if (g_State.inHandSeed == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(g_State.inHandSeed) ? registry.TryGet<const ecs::components::SpellSeed>(g_State.inHandSeed) : nullptr;
}
} // namespace

void hand_fx::RemoveAllPermBands()
{
	for (auto& band : g_State.permanent)
	{
		DestroyBand(band);
	}
	g_State.permanent.clear();
}

void hand_fx::DoRemoveFromHandVisual()
{
	// LH_SAMPLE_G_SHAKEHAND_01 (0x77), then one temporary band going back: alpha 5 -> 50 reversed, 1 s
	PlayInGame(audio::SoundId::G_ShakeHand_01);
	auto band = MakeBand(static_cast<int>(g_State.temporary.size()), 0.0f, false, k_ChargeDurationTo, 5, 50, true);
	g_State.temporary.insert(g_State.temporary.begin(), band);
}

void hand_fx::AddSpellToHandVisuals(bool delayed)
{
	const float base = delayed ? k_DelayedStart : 0.0f;
	for (int i = 1; i <= 5; ++i)
	{
		AddTemporaryBand(static_cast<float>(i) * 0.1f + base);
	}
	PlayInGame(audio::SoundId::G_SpellPowerUpBand); // 0x23, IN_GAME
}

void hand_fx::SetPULevel(int level, bool delayed)
{
	const int difference = level - GetPULevel();
	if (difference > 0)
	{
		for (int i = 0; i < difference && GetPULevel() != k_MaxPermanentBands; ++i)
		{
			AddPermanentBand(delayed ? k_DelayedStart : 0.0f);
		}
	}
	else
	{
		for (int i = 0; i < -difference && !g_State.permanent.empty(); ++i)
		{
			// fn_0068D000: the newest one goes
			DestroyBand(g_State.permanent.front());
			g_State.permanent.erase(g_State.permanent.begin());
		}
	}
}

int hand_fx::GetPULevel()
{
	return static_cast<int>(g_State.permanent.size());
}

void hand_fx::StartTribalPowerRing(int /*tribe*/)
{
	// CreateTribalPowerColumn 0x68DEF0 -> PowerSpinRunner::Create 0x66F730 (the tribe's name spinning): not drawn
}

void hand_fx::StopTribalPowerRing() {}

void hand_fx::ReleaseOrCreateTribalPowerRing() {}

void hand_fx::Update(float seconds)
{
	auto& s = g_State;
	auto& registry = Locator::entitiesRegistry::value();
	// the glow (0x68D0C0 step 1): a Magic / MagicLiving object (info class 3 / 10) or a spell seed in the hand
	// TODO(hand): the info class 3 / 10 test of 0x68D0C0 (only IsSpellSeed, vt +0x4C4, is ported)
	s.glowAlpha = 0.0f;
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject();
		    held && registry.Valid(*held) && registry.AllOf<ecs::components::SpellSeed>(*held))
		{
			s.glowAlpha = k_GlowAlpha;
		}
	}
	// the charge bands while one of the player's icons charges for this hand (fn_0064BAB0, M7's icons)
	s.wasCharging = s.charging;
	const auto* icons = gestures::GetIconProvider();
	s.charging = icons != nullptr && icons->AnyIconChargingForHand();
	const bool started = !s.wasCharging && s.charging;
	// (StopImmersion(10) on the way out, StartImmersion(10) while charging: force feedback, not ported)
	if (s.charging)
	{
		s.chargeTimer += seconds;
		const float charge = std::clamp(icons->MaxChargeFraction(), 0.0f, 1.0f); // clamped in 0x68D0C0 step 2
		if (started || s.chargeTimer >= k_ChargeIntervalFrom + (k_ChargeIntervalTo - k_ChargeIntervalFrom) * charge)
		{
			s.chargeTimer = 0.0f;
			AddChargeBand(charge);
		}
	}
	// the flowing texture's frame: += dt x -20 in [0, 64), the cell (frame % 32) of an 8 x 4 atlas
	if (s.glowAlpha > 0.01f) // 0x68D0C0 step 3
	{
		s.glowFrame += seconds * k_GlowRate;
		const float wrap = static_cast<float>(k_GlowFrames * 2);
		if (k_GlowRate > 0.0f && s.glowFrame >= wrap)
		{
			s.glowFrame = std::fmod(s.glowFrame, wrap);
		}
		else if (k_GlowRate <= 0.0f && s.glowFrame < 0.0f)
		{
			s.glowFrame = std::fmod(s.glowFrame, wrap) + wrap;
		}
		const int frame = static_cast<int>(s.glowFrame) % k_GlowFrames; // int(+0x58) % 32: truncated (0x68D0C0)
		s.glowUv = glm::vec2(static_cast<float>(frame % 8) * 0.125f, static_cast<float>(frame / 8) * 0.125f);
	}
	// the bands on the hand's root bone: the permanent ones, then the temporary ones (a finished one goes)
	const auto bone = HandBoneMatrix();
	const auto fly = CameraFlyMatrix();
	for (auto& band : s.permanent)
	{
		DrawBand(band, seconds, bone, fly);
	}
	for (auto it = s.temporary.begin(); it != s.temporary.end();)
	{
		if (it->done)
		{
			DestroyBand(*it);
			it = s.temporary.erase(it);
			continue;
		}
		DrawBand(*it, seconds, bone, fly);
		++it;
	}
}

hand_fx::Glow hand_fx::GetGlow()
{
	return {g_State.glowAlpha, g_State.glowUv};
}

void hand_fx::CreateInHandEffect(entt::entity seed)
{
	ReleaseInHandEffect();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(seed) || !registry.AllOf<ecs::components::SpellSeed>(seed))
	{
		return;
	}
	const auto& component = registry.Get<const ecs::components::SpellSeed>(seed);
	const auto& info = seed::InfoOf(component);
	// fn_007285E0: the level's GMagicInfo.particleTypeInHand
	const auto type = GetMagicInfoFromPULevel(Locator::infoConstants::value(), info, component.powerUp).particleTypeInHand;
	g_State.inHandSeed = seed;
	g_State.inHandAtBone = info.attachInHandEffectToBone == 1; // (inferido: read, but the bone is not applied yet)
	const auto file = psys::ParticleTypeFile(type);
	if (file.empty())
	{
		return;
	}
	glm::vec3 handPos(0.0f);
	if (Locator::handSystem::has_value())
	{
		handPos = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	// PSysInterface::Create(NULL, type, 0, 0, 1.0, NET 0), SetPlayer, SetOrigin(hand +0x78); stepped by the hand
	g_State.inHandEffect = psys::manager::StartForSpell(std::string(file), handPos, glm::vec3(0.0f), 1.0f, nullptr);
	psys::manager::SetPerFrame(g_State.inHandEffect);
	if (auto* effect = psys::manager::Find(g_State.inHandEffect); effect != nullptr)
	{
		effect->SetPlayer(static_cast<int>(component.creator.player));
	}
}

void hand_fx::ReleaseInHandEffect()
{
	if (g_State.inHandEffect != 0)
	{
		psys::manager::Delete(g_State.inHandEffect);
	}
	g_State.inHandEffect = 0;
	g_State.inHandSeed = entt::null;
}

void hand_fx::UpdateInHandEffect(float milliseconds)
{
	auto& s = g_State;
	if (s.inHandEffect == 0)
	{
		return;
	}
	const auto* seed = InHandSeed();
	auto* effect = psys::manager::Find(s.inHandEffect);
	if (effect == nullptr || !Locator::handSystem::has_value())
	{
		s.inHandEffect = 0;
		return;
	}
	const auto& hand = Locator::handSystem::value();
	psys::ProcessInfo info;
	// +0x4964 (info +0x0C): the hand bone's position with attachInHandEffectToBone (bone CHand +0xB4 + 4 x CHand +0x98,
	// UNVERIFIED which), else the hand (+0x78). (inferido: inHandAtBone is ignored, the hand origin is always used)
	info.handPos = glm::vec3(hand.GetHandMatrix()[3]);
	info.interfacePos = info.handPos;
	info.power = seed != nullptr ? seed->psysPower : 1.0f; // SpellSeed::GetPSysPower 0x7298F0
	info.enabled = true;
	effect->SetMagnitude(hand.GetHandScale());
	// Process_(&info, max(1, g_game_time_inc)); drawn only once the seed is ready (Draw_(1.0, false) in the hand pass)
	if (seed != nullptr && !seed->ready)
	{
		return; // (inf) not stepped either until it is drawn
	}
	if (!psys::manager::ProcessForSpell(s.inHandEffect, info, std::max(1.0f, milliseconds) * 0.001f))
	{
		s.inHandEffect = 0; // fn_0046E780: finished
	}
}

void hand_fx::Reset()
{
	RemoveAllPermBands();
	for (auto& band : g_State.temporary)
	{
		DestroyBand(band);
	}
	g_State = State {};
}
