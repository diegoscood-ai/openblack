/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapShield.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellShield.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ShieldDebugHooks.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MapShield;
using ecs::components::Transform;

namespace
{
/// g_game +0x205CA4 (head) / +0x205CA8 (count), linked through +0x5C: the newest first
std::vector<entt::entity> g_Shields;
/// The first shield's creation turn (OPENBLACK_TEST_SHIELD_SHOT counts from it)
bool g_AnyCreated = false;
unsigned int g_FirstCreated = 0;

constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

float TurnSeconds()
{
	return static_cast<float>(k_TurnMs) * 0.001f; // [0xD01A38] x 0.001
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// fmod(a, 2 pi) with the double 0x8D45D8, then + 2 pi when negative
float WrapAngle(float angle)
{
	auto wrapped = static_cast<float>(std::fmod(static_cast<double>(angle), static_cast<double>(k_TwoPi)));
	if (wrapped < 0.0f)
	{
		wrapped += k_TwoPi;
	}
	return wrapped;
}

const GMagicShieldInfo* ShieldInfoOf(const MapShield& shield)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	return GetMagicInfoAs<GMagicShieldInfo>(Locator::infoConstants::value(), shield.magicType);
}

bool SpellAlive(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	return spell != entt::null && registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell);
}

/// Object::SetScale 0x639200: nothing when it is already that scale. The body the physics built at the old size goes;
/// the next moving body near it makes a new one (PhysicsObjects BeginTurn)
void SetScale(entt::entity entity, MapShield& shield, float scale)
{
	if (shield.objectScale == scale)
	{
		return;
	}
	shield.objectScale = scale;
	if (shield.kind == MapShield::Kind::Physical)
	{
		ecs::physics::PhysicsObjects::RemoveObject(entity);
	}
}

/// The world point of MapCoords (x, z, y above the land)
glm::vec3 WorldOf(const glm::vec3& mapPoint)
{
	return {mapPoint.x, LandAt(mapPoint.x, mapPoint.z) + mapPoint.y, mapPoint.z};
}

/// PhysicalShield::ProcessShield 0x72D190 (vt 0x868)
void ProcessPhysical(entt::entity entity, MapShield& shield)
{
	const float dt = TurnSeconds();
	const float t = static_cast<float>(CurrentTurn() - shield.creationTurn) * dt;
	const auto curves = map_shield::CurvesAt(t);
	if (curves.spinning)
	{
		const float spin = shield.startSpin + (shield.endSpin - shield.startSpin) * curves.spinDown;
		shield.angle = WrapAngle(shield.angle + spin * dt);
	}
	float grow = curves.grow;
	if (shield.dying)
	{
		shield.dieTime += dt;
		if (shield.dieTime > map_shield::k_FadeTime * map_shield::k_DieTimeFactor)
		{
			map_shield::ToBeDeleted(entity); // vt 0xC (0)
			return;
		}
		const float k = std::clamp(shield.dieTime / map_shield::k_FadeTime, 0.0f, 1.0f);
		grow -= grow * k;
	}
	const float scale = shield.startScale + (shield.finalScale - shield.startScale) * grow;
	shield.bob = WrapAngle(shield.bob + map_shield::k_BobSpeed * dt);
	shield.previousRotation = shield.rotation;
	shield.previousTranslation = shield.translation;
	shield.previousScale = shield.scale;
	// +0x1C: the height above the land. The bob's size goes with the new scale (the code multiplies by it, not by the
	// grow curve)
	if (const auto* info = ShieldInfoOf(shield); info != nullptr)
	{
		shield.position.y = info->shieldHeight + info->raiseWithScale * shield.finalScale +
		                    (std::sin(shield.bob) + 1.0f) * info->bobMagnitude * scale * 0.5f;
	}
	shield.scale = scale;
	if (scale != shield.previousScale && std::abs(static_cast<double>(scale - shield.objectScale)) > map_shield::k_RescaleDelta)
	{
		SetScale(entity, shield, scale);
	}
	// the matrix: the identity x scale turned by the angle about Y (rows 0 and 2: r0' = c r0 + s r2, r2' = c r2 - s r0),
	// at (x, land + height, z)
	const float c = std::cos(shield.angle);
	const float s = std::sin(shield.angle);
	glm::mat3 rotation(1.0f);
	const glm::vec3 r0 = rotation[0];
	const glm::vec3 r2 = rotation[2];
	rotation[0] = c * r0 + s * r2;
	rotation[2] = c * r2 - s * r0;
	shield.rotation = rotation;
	shield.translation = WorldOf(shield.position);
	if (TraceEnabled())
	{
		const auto* body = ecs::physics::PhysicsObjects::Find(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: turn {} PhysicalShield {} t {:.1f}: grow {:.3f} scale {:.4f} (object {:.4f}) angle {:.2f} "
		                   "height {:.2f} dying {} {:.1f}; physics body {} (radius {:.1f}, {} vertices)",
		                   CurrentTurn(), static_cast<uint32_t>(entity), t, grow, scale, shield.objectScale, shield.angle,
		                   shield.position.y, shield.dying, shield.dieTime, body != nullptr, body != nullptr ? body->body.Radius() : 0.0f,
		                   body != nullptr ? body->body.Vertices().size() : 0);
	}
	// psys->Process (vt 0x100) with a PSysProcessInfo of zeros, strength 1, enabled
	if (shield.fx != 0)
	{
		psys::ProcessInfo info;
		info.interfacePos = glm::vec3(0.0f);
		info.handPos = glm::vec3(0.0f);
		info.cameraForward = glm::vec3(0.0f);
		info.direction = glm::vec3(0.0f);
		info.power = 1.0f;
		info.curl = 0.0f;
		info.enabled = true;
		if (!psys::manager::ProcessForSpell(shield.fx, info, dt))
		{
			shield.fx = 0;
		}
	}
}

/// PhysicalShield::DrawShield 0x72CED0 (vt 0x86C), `fraction` the part of the turn since the last one
void DrawPhysical(entt::entity entity, MapShield& shield, float fraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	// the 3D object: scale and matrix lerped from the last turn's by g_game +0x205D64
	const auto lerp = [fraction](const auto& a, const auto& b) { return a + (b - a) * fraction; };
	transform->scale = glm::vec3(lerp(shield.previousScale, shield.scale));
	transform->rotation = glm::mat3(lerp(shield.previousRotation[0], shield.rotation[0]), lerp(shield.previousRotation[1], shield.rotation[1]),
	                                lerp(shield.previousRotation[2], shield.rotation[2]));
	transform->position = lerp(shield.previousTranslation, shield.translation);
	const float t = static_cast<float>(CurrentTurn() - shield.creationTurn) * TurnSeconds();
	if (SpellAlive(shield.spell))
	{
		const float strength = std::min(GetSpellStrength(shield.spell), 1.0f);
		const auto byte = static_cast<uint8_t>(static_cast<int>(strength * 255.0f) & 0xFF);
		shield.alpha = std::max(byte, map_shield::k_MinAlpha);
	}
	// AddForDrawing only past 0.5 s, with SetGlobalAlpha(1) (the blended table) and the white tint: the mesh's own
	// alpha shows through (components::Alpha 1 is that pass; 0 hides it)
	if (auto* alpha = registry.TryGet<ecs::components::Alpha>(entity); alpha != nullptr)
	{
		alpha->value = t > map_shield::k_HiddenTime ? 1.0f : 0.0f;
	}
	if (shield.fx != 0)
	{
		auto a = static_cast<float>(shield.alpha);
		if (shield.dying)
		{
			// the code's clamp of dieTime / 1.5 is inverted: 1 up to 1.5 s, then 0
			a *= shield.dieTime / map_shield::k_FadeTime <= 1.0f ? 1.0f : 0.0f;
		}
		if (auto* effect = psys::manager::Find(shield.fx); effect != nullptr)
		{
			effect->SetGlobalAlpha(static_cast<float>(static_cast<int>(a) & 0xFF)); // psys +0x14 -> +0x6C
		}
	}
}

void Unlink(entt::entity shield)
{
	std::erase(g_Shields, shield);
}
} // namespace

map_shield::Curves map_shield::CurvesAt(float seconds)
{
	if (seconds < k_HiddenTime)
	{
		return {1.0f - seconds / k_HiddenTime, 0.0f, false};
	}
	const float u = seconds - k_HiddenTime;
	const auto curve = [](float x) { return x + x * x - x * x * x; }; // x (1 + x (1 - x))
	const float grow = u < k_GrowTime ? curve(u / k_GrowTime) : 1.0f;
	const float spinDown = u < k_SpinDownTime ? curve(u / k_SpinDownTime) : 1.0f;
	return {grow, spinDown, true};
}

entt::entity map_shield::Create(const glm::vec3& position, entt::entity spell, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!SpellAlive(spell))
	{
		return entt::null;
	}
	const auto type = registry.Get<const ecs::components::Spell>(spell).magicType;
	if (type != MagicType::Shield && type != MagicType::PhysicalShield)
	{
		return entt::null;
	}
	// MapShield ctor 0x72C070: FixedObject(pos, info, 0, 1.0), the head of the list, the spell and its GMagicShieldInfo
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	auto& shield = registry.Assign<MapShield>(entity);
	shield.spell = spell;
	shield.magicType = type;
	shield.position = glm::vec3(position.x, 0.0f, position.z);
	registry.Assign<Transform>(entity, WorldOf(shield.position), glm::mat3(1.0f), glm::vec3(1.0f));
	g_Shields.insert(g_Shields.begin(), entity);
	if (!g_AnyCreated)
	{
		g_AnyCreated = true;
		g_FirstCreated = CurrentTurn();
	}
	if (type == MagicType::Shield)
	{
		// fn_0072C250: SetScale(0.017 r). Not drawn (Draw, DrawShield and ProcessShield are empty)
		shield.kind = MapShield::Kind::Magic;
		SetScale(entity, shield, k_ScalePerRadius * radius);
		registry.Get<Transform>(entity).scale = glm::vec3(shield.objectScale);
		return entity;
	}
	// fn_0072C9F0 (after fn_0072CB70's zeros and ones)
	shield.kind = MapShield::Kind::Physical;
	shield.creationTurn = CurrentTurn();
	SetScale(entity, shield, shield.finalScale); // still 1.0 here
	if (const auto* info = ShieldInfoOf(shield); info != nullptr)
	{
		shield.position.y = info->shieldHeight + info->raiseWithScale * shield.finalScale; // with that 1.0
	}
	// Spell +0x98 (fn_007202D0) is the spell's PSysProcessInfo curl (+0x64 + 0x34)
	const float curl = registry.Get<const ecs::components::Spell>(spell).processInfo.curl;
	shield.startSpin = std::clamp(curl * 1.0f, -k_MaxStartSpin, k_MaxStartSpin);
	shield.endSpin = (shield.startSpin < 0.0f ? -1.0f : 1.0f) * k_EndSpin;
	const float finalScale = k_ScalePerRadius * radius;
	shield.startScale = finalScale * 0.01f; // fn_0072C9F0 0x72CAC6: x 0.01 (0x8C5840)
	shield.finalScale = finalScale; // fn_0072D5E0
	SetScale(entity, shield, finalScale);
	// PhysicalShield::CallVirtualFunctionsForCreation 0x72CCB0: fn_0057E220(obj3D vt+0xF8 = fn_007F9E70 [this+0x7C], the
	// LH3DMesh, 5, 0xD) at 0x72CCCD and (4, 0xD) at 0x72CCE5. On MSH_S_SOLID_SHIELD the inner layer (sub-mesh 1,
	// AlphaTextured 4) becomes AlphaTexturedAlphaAdditiveNz 13 (mode fn_0082ECD0: SRCALPHA / ONE, no Z write); no
	// primitive is type 5. It changes the shared mesh, for every physical shield, as the original. The SingleMapFixed
	// base (0x52E880) and the footpath links (vt 0x78 / 0x80 / 0x88 / 0x98 / 0x1E8, fn_00644DF0) are not modelled
	if (Locator::resources::has_value())
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (const auto id = resources::HashIdentifier(k_Mesh); meshes.Contains(id))
		{
			meshes.Handle(id)->ReplaceMaterialType(5, 13);
			meshes.Handle(id)->ReplaceMaterialType(4, 13);
		}
	}
	registry.Assign<ecs::components::Mesh>(entity, resources::HashIdentifier(k_Mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<ecs::components::Alpha>(entity, 0.0f);
	// Get3DType 0x72CE50 = 1, a morphable 3D object: UpdateMelting at creation (CallVirtualFunctionsForCreation 0x72CD23,
	// SetUpPhysOb 0x72CEB8) and on every DrawShield after the lerp (0x72D01E), so it follows the land as it grows. The
	// magic shield is static (MagicShield::Get3DType 0x72C340 -> Object::Get3DType 0x6364F0) and has no mesh here
	registry.Assign<ecs::components::MorphWithTerrain>(entity, land_morph::Melting::Live);
	// fn_0072CD40: the 3D object's matrix and scale into the current and last ones, two ProcessShields to prime them
	shield.rotation = glm::mat3(1.0f);
	shield.translation = WorldOf(shield.position);
	shield.scale = shield.objectScale;
	shield.previousRotation = shield.rotation;
	shield.previousTranslation = shield.translation;
	shield.previousScale = shield.scale;
	ProcessPhysical(entity, shield);
	ProcessPhysical(entity, registry.Get<MapShield>(entity));
	// GJPSysInterface::Create(spell, PARTICLE_TYPE 0x43 SF_PhysicalShieldFX, (x, land + height, z), (0, 0, 0), 1.0), then
	// SetPlayer, AddTarget(this) (UR_AtomsAtEPTarget follows it) and SetMagnitude(radius)
	auto& physical = registry.Get<MapShield>(entity);
	const auto file = psys::ParticleTypeFile(ParticleType::PhysicalShieldFx);
	if (!file.empty())
	{
		physical.fx = psys::manager::StartForSpell(std::string(file), WorldOf(physical.position), glm::vec3(0.0f), 1.0f, nullptr);
		if (auto* effect = psys::manager::Find(physical.fx); effect != nullptr)
		{
			const auto& spellComponent = registry.Get<const ecs::components::Spell>(spell);
			if (spellComponent.hasPlayer)
			{
				effect->SetPlayer(static_cast<int>(spellComponent.player));
			}
			effect->AddTarget(entity);
			effect->SetMagnitude(radius);
		}
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: PhysicalShield {} at ({:.1f}, {:.1f}) radius {:.1f}: scale {:.4f} -> {:.4f}, spin "
		                   "{:.2f} -> {:.2f}, fx {}",
		                   static_cast<uint32_t>(entity), position.x, position.z, radius, physical.startScale,
		                   physical.finalScale, physical.startSpin, physical.endSpin, physical.fx);
	}
	return entity;
}

void map_shield::ProcessShields()
{
	if (g_AnyCreated)
	{
		shield_debug::OnTurn(g_FirstCreated, CurrentTurn()); // OPENBLACK_TEST_SHIELD_SHOT (ShieldDebugHooks.cpp)
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : std::vector<entt::entity>(g_Shields))
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		if (auto* shield = registry.TryGet<MapShield>(entity); shield != nullptr && shield->kind == MapShield::Kind::Physical)
		{
			ProcessPhysical(entity, *shield); // MagicShield::ProcessShield 0x72C2E0 is empty
		}
	}
}

void map_shield::DrawShields()
{
	// PhysicalShield::DrawShield 0x72CEEC / 0x72CF01: the lerps with g_game +0x205D64, the fraction of the turn
	const float fraction = game_clock::TurnFraction();
	auto& registry = Locator::entitiesRegistry::value();
	bool any = false;
	for (const auto entity : g_Shields)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		if (auto* shield = registry.TryGet<MapShield>(entity); shield != nullptr && shield->kind == MapShield::Kind::Physical)
		{
			DrawPhysical(entity, *shield, fraction);
			any = true;
		}
	}
	if (any)
	{
		registry.SetDirty(); // the instances move every frame
	}
}

int map_shield::SetDying(entt::entity shield)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.Valid(shield) ? registry.TryGet<MapShield>(shield) : nullptr;
	if (component == nullptr)
	{
		return 3;
	}
	if (component->kind == MapShield::Kind::Magic)
	{
		ToBeDeleted(shield); // 0x72C320: vt 0xC (0), 3
		return 3;
	}
	component->dying = true; // 0x72D170
	component->spell = entt::null;
	return 1;
}

void map_shield::ToBeDeleted(entt::entity shield)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(shield))
	{
		Unlink(shield);
		return;
	}
	if (auto* component = registry.TryGet<MapShield>(shield); component != nullptr && component->fx != 0)
	{
		psys::manager::Delete(component->fx); // PhysicalShield 0x72CC50: delete psys
		component->fx = 0;
	}
	Unlink(shield); // MapShield::ToBeDeleted 0x72C0F0
	ecs::physics::PhysicsObjects::RemoveObject(shield);
	registry.Destroy(shield); // Object::ToBeDeleted
	registry.SetDirty();
}

bool map_shield::IsPointDefinitelyWithinShieldVolume(entt::entity shield, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(shield) ? registry.TryGet<const MapShield>(shield) : nullptr;
	if (component == nullptr)
	{
		return false;
	}
	const glm::vec3 p = WorldOf(point);
	const glm::vec3 c = WorldOf(component->position);
	if (component->kind == MapShield::Kind::Magic)
	{
		// 0x72B850: the spell's Get2DRadius (its magnitude), a sphere in world points
		if (!SpellAlive(component->spell))
		{
			return false;
		}
		const float r = registry.Get<const ecs::components::Spell>(component->spell).magnitude;
		const glm::vec3 d = p - c;
		return r * r > glm::dot(d, d);
	}
	// 0x72B8E0: a cone of its own 2D radius R and height H: d^2 < R^2 (x, z) and the point's MapCoords altitude (above
	// the land) below H (1 - d / R)
	const float r = ecs::object::Get2DRadius(shield); // vt +0x64 (0x72B908)
	const float dx = p.x - c.x;
	const float dz = p.z - c.z;
	const float d2 = dx * dx + dz * dz;
	if (!(d2 < r * r))
	{
		return false;
	}
	const float h = ecs::object::GetHeight(shield); // vt +0x42C (0x72B948)
	return point.y < h - h * (std::sqrt(d2) / r);
}

bool map_shield::IsReactionBlockedByShield(const glm::vec3& living, const glm::vec3& source)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : g_Shields)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		const auto* component = registry.TryGet<const MapShield>(entity);
		if (component == nullptr)
		{
			continue;
		}
		// GetDistanceInMetres 0x74CD70 (x, z; 0x72B9B2) against the shield's Get2DRadius (vt +0x64, 0x72B9C2)
		const float d = gutils::GetDistanceInMetres(living, component->position);
		if (ecs::object::Get2DRadius(entity) > d && !IsPointDefinitelyWithinShieldVolume(entity, source))
		{
			return true;
		}
	}
	return false;
}

bool map_shield::GetPlayer(entt::entity shield, PlayerNames& player)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	if (component == nullptr || !SpellAlive(component->spell))
	{
		return false; // GameThing::GetPlayer: none
	}
	const auto& spell = Locator::entitiesRegistry::value().Get<const ecs::components::Spell>(component->spell);
	player = spell.player;
	return spell.hasPlayer;
}

bool map_shield::CreatureMustAvoid(entt::entity shield, entt::entity creature, std::optional<PlayerNames> creaturePlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	// 0x72C177..0x72C17F: no creature, or one controlled by a script (+0x24 & 0x400) -> 0
	if (creature == entt::null || !registry.Valid(creature) || ecs::script_held::IsControlledByScript(creature))
	{
		return false;
	}
	// 0x72C184..0x72C193: the creature's player (vt 0x1C) against the shield's (MapShield::GetPlayer 0x72C150), as
	// pointers: the same player (two null ones included) -> 0, anything else 1. A shield whose spell is gone (+0x60 0,
	// 0x72C155) takes GameThing::GetPlayer 0x570130: the interface's player (g_game +0x205A5B), openblack's PLAYER_ONE;
	// a live spell with no player gives NULL
	PlayerNames shieldPlayer = PlayerNames::PLAYER_ONE;
	const auto* component = registry.TryGet<const MapShield>(shield);
	const bool spellGone = component == nullptr || !SpellAlive(component->spell);
	const bool hasShieldPlayer = spellGone || GetPlayer(shield, shieldPlayer);
	if (hasShieldPlayer != creaturePlayer.has_value())
	{
		return true;
	}
	return hasShieldPlayer && *creaturePlayer != shieldPlayer;
}

bool map_shield::InteractsWithPhysicsObjects(entt::entity shield)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	return component != nullptr && component->kind == MapShield::Kind::Physical;
}

float map_shield::CollisionScale(entt::entity shield)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	// Object::GetScale vt +0x120: the shield's +0x50 (SetScale 0x639200)
	return component != nullptr ? ecs::object::GetScale(shield) : 1.0f;
}

void map_shield::ReactToPhysicsImpact(entt::entity shield, const ecs::physics::PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const MapShield>(shield);
	if (component == nullptr || po.hitBy == nullptr)
	{
		return;
	}
	// GetGameObjectWhoHitMe 0x644F00: the hitter's object; available, PhysicallyDestroysAbodes (vt 0x7A8), a spell with
	// strength
	const auto hitter = po.hitBy->entity;
	const auto spell = component->spell;
	if (!registry.Valid(hitter) || !ecs::physics::Buildings::PhysicallyDestroysAbodes(hitter) || !SpellAlive(spell) ||
	    !(GetSpellStrength(spell) > 0.0f))
	{
		return;
	}
	// SpellEvent 5 at the hitter's MapCoords (x, z and its altitude above the land as the y), no target
	const auto* transform = registry.TryGet<const Transform>(hitter);
	const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3(0.0f);
	psys::SpellEventInfo event;
	event.type = psys::SpellEventInfo::Object;
	event.position = glm::vec3(at.x, at.y - LandAt(at.x, at.z), at.z);
	event.velocity = glm::vec3(0.0f);
	event.strength = 1.0f;
	event.checkShields = false;
	event.target = entt::null;
	auto& spellComponent = registry.Get<ecs::components::Spell>(spell);
	OpsOf(spellComponent.spellClass).spellEvent(spell, event); // vt 0x52C
	// fn_0072B830: PayFor(|v| x mass x chantCostPerImpactMomentum x 0.0001, forced)
	const float momentum = glm::length(po.hitBy->body.velocity) * po.hitBy->body.Mass();
	const auto* info = ShieldInfoOf(*component);
	const float cost = momentum * (info != nullptr ? info->chantCostPerImpactMomentum : 0.0f) * 0.0001f;
	if (registry.Valid(spell))
	{
		chants::PayFor(registry.Get<ecs::components::Spell>(spell), ChantContextOf(spell), cost, true);
	}
	// 0x72D74F..0x72D788: with a town (the spell's +0xFC), Town::UpdateAggressor(EffectValues(EFFECT_TYPE 2, 0, the
	// hitter, 1.0, PhysicsObject::GetPlayer 0x647460 of it), 0) 0x73C9B0. Only its record is ported (+0xEAC the
	// aggressor, +0xEB0 the turn: 0x73CA82 / 0x73CA98); TODO(towns): the per-player aggression slots (fn_0073E0F0 on
	// town + n x 0x80 + 0x9F4, with the value plus GTownInfo +0xAC when the slot is 0 and x the +0xEB4 / +0xEB8 weight
	// that then decays by 0.9), the guidance TownAttackSFX 0x71B7C0 and the creature mimic of 0x73CAAA..0x73CB29
	if (const auto town = spell_shield::TownOf(spell); registry.Valid(town))
	{
		auto& townComponent = registry.Get<ecs::components::Town>(town);
		// PhysicsObject::GetPlayer: the player's hand threw it (po.byPlayer), else no player, and UpdateAggressor then
		// takes the interface's own (g_game +0x205A5B, 0x73C9C7): both are openblack's PLAYER_ONE
		townComponent.aggressor = PlayerNames::PLAYER_ONE;
		townComponent.aggressorTurn = CurrentTurn();
	}
	const float strength = GetSpellStrength(spell);
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: PhysicalShield {} hit by {}: momentum {:.1f}, pays {:.1f}, strength {:.3f}",
		                   static_cast<uint32_t>(shield), static_cast<uint32_t>(hitter), momentum, cost, strength);
	}
	if (strength > 0.0f)
	{
		spell_shield::UpdateStruckReaction(spell); // vt 0x51C
	}
	else
	{
		spell_shield::SetUpDestroyedReaction(spell); // vt 0x520
	}
}

bool map_shield::IsEffectReceiver(entt::entity shield, float burn)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const MapShield>(shield);
	return component != nullptr && component->kind == MapShield::Kind::Physical && burn == 0.0f;
}

const std::vector<entt::entity>& map_shield::Shields()
{
	return g_Shields;
}

void map_shield::Clear()
{
	g_AnyCreated = false;
	g_Shields.clear(); // the entities and effects go with the registry's and the manager's resets
}
