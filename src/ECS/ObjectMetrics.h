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

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/AxisAlignedBoundingBox.h"
#include "Enums.h"

/// The original's object size family: Object::Get2DRadius 0x638180 (vt +0x64), Object::GetRadius 0x638110 (vt +0x60),
/// Object::GetHeight 0x638120 (vt +0x42C), Object::GetScale 0x402520 (vt +0x120) and the routines built on them. All of
/// them read the LH3DMesh fields LH3DMesh::ComputeBoundingBox 0x8081B0 fills at load: +0x24 / +0x28 / +0x2C the half
/// extents (max - min) x 0.5 of the union of the sub-meshes (0x80831C..0x80835B) and +0x30 the half diagonal
/// sqrt((hz^2 + hy^2) + hx^2) (0x80835E..0x808379).
///
/// Two levels, because the original has both and they disagree on purpose:
/// - The mesh level (MeshHalfExtents, MeshRadius2D, MeshHalfHeight, MeshHalfDiagonal): the fields read inline from the
///   Game3DObject's mesh, with no class override (IsSuitableForFixed 0x603E1E..0x603E5B, fn_00604020 0x604042, Scaffold
///   0x6E956A and 0x6EAC14..0x6EAC56, 0x7350A5, Field::Draw 0x5287B9..0x5287D3, PhysOb::Initialise 0x7FB7D9, Tree::Draw
///   0x74ABB0 ...). A Field there is its mesh, not 5 m.
/// - The object level (GetScale, Get2DRadius, GetRadius, GetHeight and the derived ones): the virtual calls, with the
///   overrides the Object-derived vtables (the ??_7 of symbols.txt with Object 0x638180 or an override at +0x64) hold:
///   Field 0x528E80 and FishFarm 0x52C470 = 5, PileFood / MagicFood / PuzzleGrain 0x66F180 = x GetProportionRaised,
///   MagicTeleport 0x5FCCB0 = 6, MagicFireBall 0x682D20 / 0x682D30, Creature 0x477F40 / 0x477F50 / 0x47B190. Trees,
///   rocks, abodes, villagers, animals, the citadel heart and the wood piles override none of the four. Each derived
///   routine lists its own overrides (GetBoundingSphere: Living, MobileStatic, Creature; GetTopPos: MapShield; ...).
/// - Not covered: the classes that are not Objects (GameThing / GameThingWithPos vtables). The Citadel 0x8C7E68 (the
///   Planned* and SpellSeedGraphic too) keeps GameThing 0x405140 / 0x405150 = 0 and GameThingWithPos 0x405500 = 0 with
///   GetScale 0x4247E0 = 1; SpellShield 0x72B440 / 0x72B450 (GetSpellMagnitude 0x7202C0), SpellStormAndTornado
///   0x72D950 / 0x72D960, Town 0x73D6E0, GArena 0x424780, Reaction 0x55C7D0, BuildingSite 0x43D050 and AtomCore
///   0x673C70 have their own GetRadius / Get2DRadius; GStreetLight 0x735110 (radius 20, fn_00735060) and Mist 0x6067D0
///   (Mist::Get2DRadius 0x606660) their own GetDistanceFromObject(MapCoords). None of them is asked here: openblack's
///   temple is the CitadelHeart, which is an Object (pendiente if one of them ever is).
///
/// Port each site to the level the original uses there. The game logic runs with the FPU at 24 bits (fn_007DEE00), so
/// everything is float, no double and no FMA.
namespace openblack::ecs::object
{

/// [0x8AB6E4] = 5.0: Field::Get2DRadius 0x528E80, FishFarm::Get2DRadius 0x52C470 and their GetMeshRadius 0x528A30 /
/// 0x52C480 (no mesh, no scale)
constexpr float k_FieldRadius = 5.0f;
/// [0x92C108] = 6.0 (.data, one reader 0x5FCCA1): MagicTeleport::Get2DRadius 0x5FCCB0 -> fn_005FCCA0
constexpr float k_MagicTeleportRadius = 6.0f;
/// [0x935910] = 1.0 (.data, one reader 0x682D29): MagicFireBall::Get2DRadius 0x682D20 = GetScale x this
constexpr float k_MagicFireBallRadius = 1.0f;
/// [0x8C2C40] = 15.0: Creature::GetHeight 0x477F50 = the body's user size x 15
constexpr float k_CreatureHeightPerScale = 15.0f;
/// [0x933014] = 0.05 (.data): the floor GetProportionRaised gives a non-empty pile (PileFood 0x66EBCA, PileWood 0x66F1EC)
constexpr float k_ProportionFloor = 0.05f;
/// [0x8AB274] = 0.75: Object::GetHoldRadius 0x638C00 of an ABOVE hold, x GetHeight
constexpr float k_HoldAboveHeightFactor = 0.75f;
/// [0x8AB244] = 0.2: Tree::GetHoldRadius 0x74B610 and DeadTree::GetHoldRadius 0x5110E0, x Get2DRadius
constexpr float k_TreeHoldFactor = 0.2f;
/// [0x8D6974] = 0.35: DeadTree::GetDefaultFireRadius 0x510E10, x GetHeight
constexpr float k_DeadTreeFireFactor = 0.35f;
/// [0x99C9EC] = 14.0: WorshipSite::GetRealRadius 0x77DDD0 (WorshipSite::GetDefaultFireRadius 0x77DE10 jumps to it)
constexpr float k_WorshipSiteRadius = 14.0f;
/// [0x8AA3A0] = 1.05 and [0x8AA39C] = 0.0005: Object::GetVillagerHugRadius 0x4026B0 = Get2DRadius x 1.05 + 0.0005
constexpr float k_HugRadiusFactor = 1.05f;
constexpr float k_HugRadiusMargin = 0.0005f;
/// [0x8AB22C] = 0.1 and [0x8AB3D4] = 0.25: Tree::GetVillagerHugRadius 0x74A1A0 and Tree::GetRoutePlanRadius 0x74A140 =
/// min(Get2DRadius x 0.1, 0.25)
constexpr float k_TreeHugFactor = 0.1f;
constexpr float k_TreeHugMax = 0.25f;
/// [0x8CA268] = 0.33: CitadelHeart::GetRoutePlanRadius 0x4680C0 = Get2DRadius x 0.33
constexpr float k_CitadelHeartRoutePlanFactor = 0.33f;
/// [0x99C9E8] = 12.55 and [0x99C9E4] = 26.1: WorshipSite::CalculateCentrePos 0x77DD40, the site's local point
/// (12.55, 0, -26.1) through its matrix
constexpr float k_WorshipSiteCentreRight = 12.55f;
constexpr float k_WorshipSiteCentreBack = 26.1f;

// ---- Mesh level (no override) -------------------------------------------------------------------------------------

/// LH3DMesh +0x24 / +0x28 / +0x2C of a box: (max - min) x 0.5 [0x8AA3B4] (0x80831C..0x80835B)
[[nodiscard]] glm::vec3 HalfExtents(const AxisAlignedBoundingBox& box);
/// LH3DMesh +0x30 of the half extents: sqrt((hz hz + hy hy) + hx hx), in that order (0x80835E..0x808379)
[[nodiscard]] float HalfDiagonal(glm::vec3 half);
/// s x max(hx, hz): the fcompp of Object::Get2DRadius 0x6381B1..0x6381E2 (hx when hz < hx, else hz; one product)
[[nodiscard]] float Radius2D(glm::vec3 half, float scale);
/// 2 x (hy x s): Object::GetHeight 0x638136..0x63813D (fmul then fadd st0, st0)
[[nodiscard]] float Height(glm::vec3 half, float scale);

/// The half extents of a loaded mesh (LH3DMesh +0x24..+0x2C, 0x8081B0); nothing when the mesh is not loaded
[[nodiscard]] std::optional<glm::vec3> MeshHalfExtents(entt::id_type meshId);
/// LH3DMesh +0x30 (0x808379), no scale; 0 without the mesh
[[nodiscard]] float MeshHalfDiagonal(entt::id_type meshId);
/// The inline Get2DRadius: s x max(+0x24, +0x2C) (IsSuitableForFixed 0x603E1E, fn_00604020 0x604042, Scaffold 0x6E956A
/// and 0x6EAC22, 0x7350A5); 0 without the mesh
[[nodiscard]] float MeshRadius2D(entt::id_type meshId, float scale);
/// The inline whole height 2 x (s x +0x28) (Tree::Draw 0x74ABA2..0x74ABC0: GetScale, fmul [m + 0x28], fadd st0, st0);
/// 0 without the mesh
[[nodiscard]] float MeshHeight(entt::id_type meshId, float scale);
/// The inline half height +0x28, no scale (Field::Draw 0x5287C5, PhysOb::Initialise 0x7FB7D9, Tree::Draw 0x74ABB0); 0
/// without the mesh
[[nodiscard]] float MeshHalfHeight(entt::id_type meshId);

/// The mesh of an object (the Mesh component's id), when it has one and it is loaded
[[nodiscard]] std::optional<glm::vec3> ObjectHalfExtents(entt::entity object);

// ---- GetProportionRaised (PileResource vt +0x86C) -------------------------------------------------------------------

/// PileFood::GetProportionRaised 0x66EB60: p = amount (fild qword, unsigned) / maxInPot (fidiv); p < 0 -> 0 and no
/// floor; p > 1 -> 1; p == 0 stays 0 (0x66EBB7..0x66EBC2: an empty pile is 0); else p = (1 - 0.05) p + 0.05. Then
/// 1 - (1 - p)^2 clamped to 0..1
[[nodiscard]] float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot);
/// PileWood::GetProportionRaised 0x66F1B0: p = amount / maxInPot; p > 0 -> (1 - 0.05) p + 0.05; then clamped to 0..1
/// (no square)
[[nodiscard]] float PileWoodProportionRaised(uint32_t amount, uint32_t maxInPot);
/// The pile's own one by its info's potType (PileFood / PileWood); 1 for a pot or anything else, which has no such
/// method (inferido: never asked)
[[nodiscard]] float GetProportionRaised(entt::entity pile);

// ---- Object level (the virtual calls, with the class overrides) -----------------------------------------------------

/// Object::GetScale vt +0x120: Object 0x402520 = the field +0x50 (openblack: the Transform's uniform scale, x), a map
/// shield's object scale (MapShield::objectScale, SetScale 0x639200), Creature 0x47B190 = CreaturePhysical::GetUserSize
/// 0x4EF4F0 (inferido: the Transform's scale). 0 without a Transform
[[nodiscard]] float GetScale(entt::entity object);
/// The field +0x50 Object::GetHeight reads directly (0x638139), not the virtual GetScale
[[nodiscard]] float GetScaleField(entt::entity object);

/// Object::Get2DRadius 0x638180 itself (the non-virtual body PileFood 0x66F192 calls): GetScale (vt +0x120) x
/// max(+0x24, +0x2C); 0 without a mesh (GameThing 0x405150)
[[nodiscard]] float ObjectGet2DRadius(entt::entity object);
/// Object::GetHeight 0x638120 itself: 2 x +0x28 x the field +0x50; 0 without a mesh (GameThingWithPos 0x405500)
[[nodiscard]] float ObjectGetHeight(entt::entity object);

/// Get2DRadius vt +0x64 of the object's class: Field 0x528E80 / FishFarm 0x52C470 = 5; MagicTeleport 0x5FCCB0 = 6;
/// MagicFireBall 0x682D20 = GetScale x 1; PileFood / MagicFood / PuzzleGrain 0x66F180 = GetProportionRaised x
/// Object::Get2DRadius; Creature 0x477F40 reads the LH3DCreature (+0x160 -> +0x58 -> +0x5228), not ported: the Object
/// one stands in (inferido); every other Object class Object 0x638180 (the non-Object overrides: see the top)
[[nodiscard]] float Get2DRadius(entt::entity object);
/// GetRadius vt +0x60: Object 0x638110 = jmp [vt +0x64] (Creature 0x4792C0 is the same read as its Get2DRadius)
[[nodiscard]] float GetRadius(entt::entity object);
/// GetHeight vt +0x42C: MagicFireBall 0x682D30 = jmp [vt +0x64]; Creature 0x477F50 = user size x 15; every other class
/// Object 0x638120 (Field, FishFarm and PileFood keep it)
[[nodiscard]] float GetHeight(entt::entity object);
/// GetTopPos vt +0x630: Object 0x638160 = the MapCoords altitude (+0x1C, above the ground) + GetHeight (vt +0x42C);
/// MapShield / MagicShield / PhysicalShield 0x72C1C0 = 0 (fld [0x8AA398])
[[nodiscard]] float GetTopPos(entt::entity object);
/// GetHeightForHandAboveInteractObject vt +0x64C: Object 0x638150 = jmp [vt +0x42C]; FishFarm 0x52C840 = 5
/// (fld [0x8AB6E4])
[[nodiscard]] float GetHeightForHandAboveInteractObject(entt::entity object);
/// GetMeshRadius vt +0x568: Object 0x636BD0 = the mesh's +0x30, no scale; Field 0x528A30 / FishFarm 0x52C480 = 5
[[nodiscard]] float GetMeshRadius(entt::entity object);

// ---- Derived (built on the object level, each its own routine) ------------------------------------------------------

/// GetHoldRadius vt +0x590: Tree 0x74B610 / DeadTree 0x5110E0 = Get2DRadius x 0.2; Object 0x638C00 = GetHeight x 0.75
/// when GetHoldType (vt +0x58C) is ABOVE (1), else Get2DRadius. The hold type is the hand's (HandSystem::HoldType), so
/// the caller says whether it is ABOVE. SpellSeed 0x728640 (GetScale x info +0x150) needs the seed info: the caller's
[[nodiscard]] float GetHoldRadius(entt::entity object, bool holdTypeAbove);
/// GetDefaultFireRadius vt +0x5F4: Object 0x639AC0 = jmp [vt +0x64]; DeadTree 0x510E10 = GetHeight x 0.35; WorshipSite
/// 0x77DE10 = GetRealRadius 0x77DDD0 = 14
[[nodiscard]] float GetDefaultFireRadius(entt::entity object);
/// GetVillagerHugRadius: Object 0x4026B0 = Get2DRadius x 1.05 + 0.0005; Tree 0x74A1A0 = min(Get2DRadius x 0.1, 0.25)
[[nodiscard]] float GetVillagerHugRadius(entt::entity object);
/// GetRoutePlanRadius(NULL creature): Object 0x6384C0 = Get2DRadius (0x6384CF); Tree 0x74A140 = min(Get2DRadius x 0.1,
/// 0.25); CitadelHeart 0x4680C0 = Get2DRadius x 0.33 (openblack: the Temple). The creature branch of 0x6384D8 (its
/// NavRadius 0x480A60) is not ported
[[nodiscard]] float GetRoutePlanRadius(entt::entity object);
/// GetDistanceFromObject(Object*) vt +0x6C4: Object 0x637FB0 = GetDistanceInMetres(a, b) 0x74CD70 - (R2D(b) + R2D(a)),
/// the radii added first (0x637FDF) and the sum subtracted from the distance (fsubr 0x637FE5); WorshipSite 0x77DE20 =
/// GetDistanceInMetres(CalculateCentrePos 0x77DD40, b) - (GetRealRadius 0x77DDD0 (14) + R2D(b)) (0x77DE36..0x77DE60)
[[nodiscard]] float GetDistanceFromObject(entt::entity object, entt::entity other);
/// GetDistanceFromObject(MapCoords) vt +0x13C: GameThingWithPos 0x5702B0 (Object 0x4027C0 calls it) =
/// GetDistanceInMetres - GetRadius (vt +0x60). No Object class overrides it (GStreetLight and Mist do: see the top)
[[nodiscard]] float GetDistanceFromObject(entt::entity object, glm::vec3 point);
/// Object::IsTouching(Object*, margin) 0x637E00: GetDistanceFromObject (vt +0x6C4) <= margin
[[nodiscard]] bool IsTouching(entt::entity object, entt::entity other, float margin);
/// Object::IsTouching(MapCoords) 0x637E30: GetDistanceFromObject (vt +0x13C) <= 0
[[nodiscard]] bool IsTouching(entt::entity object, glm::vec3 point);

struct BoundingSphere
{
	glm::vec3 centre;
	float radius;
};
/// GetBoundingSphere vt +0x798: Object 0x637730 = h = GetHeight x 0.5 (0x63773C), r = sqrt(R2D R2D + h h)
/// (0x63774D..0x637762); the centre is the MapCoords' x, z (fild x 10 / 65536, 0x637781..0x637795) and y =
/// (GetAltitude 0x803090 + altitude) + h (0x637771..0x637798). The ground is the island's (LandIsland::HeightAt,
/// through map_coords::ToWorld). Living 0x5ED2F0 (villagers and animals) and MobileStatic 0x608F40 (rocks, dead and
/// felled trees, bonfires, fragments, magic teleports) are the same with R2D x 0.5 (fmul [0x8AA3B4] at 0x5ED30D /
/// 0x608F5D). Creature 0x479970 = LH3DCreature::GetBoundingSphere 0x47F8D0, not ported: the Object one stands in
/// (inferido)
[[nodiscard]] BoundingSphere GetBoundingSphere(entt::entity object);

/// WorshipSite::CalculateCentrePos 0x77DD40: the matrix's right x 12.55 - its forward x 26.1 + its position, per
/// component in that order (0x77DD61..0x77DDB1), then made a MapCoords (0x77DDBD; openblack keeps the point). The
/// matrix is the site's Transform (inferido: [this + 0x40] + 0x14, the same reading as WorshipScore's)
[[nodiscard]] glm::vec3 WorshipSiteCentre(entt::entity site);

namespace detail
{
/// Test hook: when set, the mesh boxes come from here instead of the resources (the tests have no meshes)
using MeshBoxProvider = std::optional<AxisAlignedBoundingBox> (*)(entt::id_type meshId);
void SetMeshBoxProviderForTests(MeshBoxProvider provider);
} // namespace detail

} // namespace openblack::ecs::object
