/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::physics
{
struct PhysicsObject;
}

// MapShield (SpellShield.cpp of the original, 0x72BE20..0x72D850): the world object a shield spell makes. MagicShield
// is invisible (the dome is the spell's SF_DefenseSphere); PhysicalShield is the solid MSH_S_SOLID_SHIELD that grows,
// spins and bobs, a physics obstacle that pays chants per impact, with its SF_PhysicalShieldFX. Wiki:
// docs/bw1-notes/magic.md ("Escudos").

namespace openblack::magic::map_shield
{

/// The mesh of both (GMapShieldInfo 0xDA05D0 / 0xDA06D8 and MapShield::GetMesh 0x72C1B0: 0x22A)
constexpr MeshId k_Mesh = MeshId::SpellSolidShield;
/// ProcessShield 0x72D190's constants (0x9828B0..0x9828D0) and DrawShield's
constexpr float k_ScalePerRadius = 0.017f;
constexpr float k_MaxStartSpin = 3.0f;
constexpr float k_HiddenTime = 0.5f;
constexpr float k_GrowTime = 1.5f;
constexpr float k_SpinDownTime = 6.0f;
constexpr float k_FadeTime = 1.5f;
constexpr float k_DieTimeFactor = 1.5f; ///< 0x9828C8: deleted once dieTime > k_FadeTime x this (2.25 s)
constexpr float k_EndSpin = 0.15f;
constexpr float k_BobSpeed = 1.3f;
constexpr double k_RescaleDelta = 0.3; ///< 0x900C70
constexpr uint8_t k_MinAlpha = 40;

/// ProcessShield's curves, t in seconds since the creation turn: the shrink over the first 0.5 s (1 - t / 0.5, the
/// shield is not drawn then), then the grow x + x^2 - x^3 (x = (t - 0.5) / 1.5) and the spin-down y + y^2 - y^3
/// (y = (t - 0.5) / 6), both 1 once past their end
struct Curves
{
	float grow;       ///< A
	float spinDown;   ///< B (0 while t < 0.5: the spin does not move)
	bool spinning;    ///< t >= 0.5
};
[[nodiscard]] Curves CurvesAt(float seconds);

/// MapShield::Create 0x72BE20 (pos, spell, radius): MagicShield (type 19, fn_0072C250) or PhysicalShield (type 20,
/// fn_0072C9F0 + fn_0072CD40); entt::null for another type. `position` is MapCoords (y above the land, 0).
entt::entity Create(const glm::vec3& position, entt::entity spell, float radius);

/// fn_0072BF80 (from Spell::ProcessSpells): ProcessShield (vt 0x868) of every available shield
void ProcessShields();
/// fn_0072BF50 (from Spell::DrawSpells): DrawShield (vt 0x86C) every frame
void DrawShields();

/// vt 0x6A4 SetDying: MagicShield 0x72C320 goes at once (3); PhysicalShield 0x72D170 lets go of the spell and fades
/// out by itself (1)
int SetDying(entt::entity shield);
/// vt 0xC ToBeDeleted: PhysicalShield 0x72CC50 (its FX) then MapShield 0x72C0F0 (out of the list)
void ToBeDeleted(entt::entity shield);

/// vt 0x870 IsPointDefinietlyWithinShieldVolume: MagicShield 0x72B850 a sphere of the spell's radius, PhysicalShield
/// 0x72B8E0 a cone of its own 2D radius and height. `point` is MapCoords (y above the land).
[[nodiscard]] bool IsPointDefinitelyWithinShieldVolume(entt::entity shield, const glm::vec3& point);
/// fn_0072B990 (Reaction::ApplyReactionToLivingObjectsAtSquare 0x6E4031): true when the living is under a shield (its
/// distance below the shield's 2D radius) that the reaction's source is not definitely inside: it ignores the reaction.
/// MapCoords (y above the land).
[[nodiscard]] bool IsReactionBlockedByShield(const glm::vec3& living, const glm::vec3& source);

/// Object::Get2DRadius 0x638180 / GetHeight 0x638120 of a shield: the mesh's size x Object::GetScale
[[nodiscard]] float Get2DRadius(entt::entity shield);
[[nodiscard]] float GetHeight(entt::entity shield);
/// MapShield::GetPlayer 0x72C150: its spell's player
[[nodiscard]] bool GetPlayer(entt::entity shield, PlayerNames& player);
/// MapShield::CreatureMustAvoid 0x72C170 (vt 0x614): a creature that is not controlled by a script (+0x24 & 0x400,
/// 0x72C17B) and whose player (vt 0x1C) is not the shield's (0x72C190) must keep out of it; anything else, including no
/// creature at all, is 0. openblack has no creature class, so its player comes as an argument (std::nullopt = the
/// original's NULL GPlayer) and (pendiente) nobody asks yet: in the original the caller is the creature's path finding
[[nodiscard]] bool CreatureMustAvoid(entt::entity shield, entt::entity creature, std::optional<PlayerNames> creaturePlayer);

// ---- physics (ECS/Physics/PhysicsObjects.cpp asks) ----

/// InteractsWithPhysicsObjects: PhysicalShield 0x72D600 1, MagicShield 0
[[nodiscard]] bool InteractsWithPhysicsObjects(entt::entity shield);
/// GetPhysicsConstantsType 0x72D7E0: 10
constexpr int k_PhysicsConstantsType = 10;
/// The scale its body is built with (Object::GetScale, which lags the drawn one by up to 0.3)
[[nodiscard]] float CollisionScale(entt::entity shield);
/// PhysicalShield::ReactToPhysicsImpact 0x72D610: a hit by something that destroys abodes, while the spell has
/// strength, is a SpellEvent 5 and costs |v| x mass x chantCostPerImpactMomentum x 0.0001 (forced); then the struck or
/// destroyed reaction
void ReactToPhysicsImpact(entt::entity shield, const ecs::physics::PhysicsObject& po);
/// PhysicalShield::IsEffectReceiver 0x72CC80: no effect or burn 0 (MagicShield: never)
[[nodiscard]] bool IsEffectReceiver(entt::entity shield, float burn);

/// g_game +0x205CA4: the shields, newest first
[[nodiscard]] const std::vector<entt::entity>& Shields();
/// A land is loaded
void Clear();

} // namespace openblack::magic::map_shield
