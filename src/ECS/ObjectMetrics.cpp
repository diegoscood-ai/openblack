/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectMetrics.h"

#include <algorithm>
#include <cmath>

#include "3D/L3DMesh.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// [0x8AA3B4] = 0.5 (ComputeBoundingBox 0x80833D, GetBoundingSphere 0x63773C)
constexpr float k_Half = 0.5f;

openblack::ecs::object::detail::MeshBoxProvider g_MeshBoxProvider = nullptr;

std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type meshId)
{
	if (g_MeshBoxProvider != nullptr)
	{
		return g_MeshBoxProvider(meshId);
	}
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return std::nullopt;
	}
	return meshes.Handle(meshId)->GetBoundingBox();
}

const ecs::Registry* RegistryOrNull()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// The info of a pot entity (its PotInfo row), when it has one
const GPotInfo* PotInfoOf(const ecs::Registry& registry, entt::entity object)
{
	const auto* pot = registry.TryGet<const Pot>(object);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
}

bool IsPileFood(const ecs::Registry& registry, entt::entity object)
{
	// PileFood, MagicFood and PuzzleGrain (vtables with 0x66F180 at +0x64) are the pots whose info potType is PileFood
	const auto* info = PotInfoOf(registry, object);
	return info != nullptr && info->potType == PotType::PileFood;
}
} // namespace

namespace openblack::ecs::object
{

glm::vec3 HalfExtents(const AxisAlignedBoundingBox& box)
{
	// fld max; fsub min; fstp; fld; fmul [0x8AA3B4]; fstp (0x80831C..0x80835B): two roundings, as here
	const glm::vec3 size = box.maxima - box.minima;
	return {size.x * k_Half, size.y * k_Half, size.z * k_Half};
}

float HalfDiagonal(glm::vec3 half)
{
	// fld hx; fld hy; fld hz; hz hz, + hy hy, + hx hx, fsqrt (0x80835E..0x808377)
	const float zz = half.z * half.z;
	const float yy = half.y * half.y;
	const float zy = zz + yy;
	const float xx = half.x * half.x;
	const float sum = zy + xx;
	return std::sqrt(sum);
}

float Radius2D(glm::vec3 half, float scale)
{
	// fld [m+0x24]; fld [m+0x2C]; fcompp; test ah, 1 (C0: hz < hx) -> hx s (0x6381CB), else hz s (0x6381DF)
	const float larger = half.z < half.x ? half.x : half.z;
	return larger * scale;
}

float Height(glm::vec3 half, float scale)
{
	// fld [m+0x28]; fmul [this+0x50]; fadd st0, st0 (0x638136..0x63813D)
	const float product = half.y * scale;
	return product + product;
}

std::optional<glm::vec3> MeshHalfExtents(entt::id_type meshId)
{
	const auto box = MeshBox(meshId);
	if (!box)
	{
		return std::nullopt;
	}
	return HalfExtents(*box);
}

float MeshHalfDiagonal(entt::id_type meshId)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? HalfDiagonal(*half) : 0.0f;
}

float MeshRadius2D(entt::id_type meshId, float scale)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? Radius2D(*half, scale) : 0.0f;
}

float MeshHeight(entt::id_type meshId, float scale)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? Height(*half, scale) : 0.0f;
}

float MeshHalfHeight(entt::id_type meshId)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? half->y : 0.0f;
}

std::optional<glm::vec3> ObjectHalfExtents(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return std::nullopt;
	}
	const auto* mesh = registry->TryGet<const Mesh>(object);
	if (mesh == nullptr)
	{
		return std::nullopt;
	}
	return MeshHalfExtents(mesh->id);
}

float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	// fild qword (the unsigned amount); fidiv max (0x66EB79..0x66EB85). With max 0 the original gets inf (-> 1) or NaN
	// (-> 0), the same as dividing by 1
	float p = static_cast<float>(amount) / static_cast<float>(std::max(1u, maxInPot));
	if (p < 0.0f)
	{
		p = 0.0f; // 0x66EB98, and no floor (jmp 0x66EBD8)
	}
	else
	{
		if (p > 1.0f)
		{
			p = 1.0f; // 0x66EBAF, then the floor
		}
		if (p > 0.0f) // 0x66EBB7..0x66EBC2: p == 0 keeps 0
		{
			const float rest = 1.0f - k_ProportionFloor; // fld 1; fsub [0x933014]
			p = rest * p;                               // fmulp
			p = p + k_ProportionFloor;                  // fadd [0x933014] (0x66EBD2)
		}
	}
	const float q = 1.0f - p; // fsubr 1 (0x66EBD8)
	const float qq = q * q;
	return std::clamp(1.0f - qq, 0.0f, 1.0f); // 0x66EBE2..0x66EC11
}

float PileWoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	float p = static_cast<float>(amount) / static_cast<float>(std::max(1u, maxInPot)); // 0x66F1C9..0x66F1D5
	if (p > 0.0f) // test ah, 0x41 (0x66F1E1)
	{
		const float rest = 1.0f - k_ProportionFloor;
		p = rest * p;
		p = p + k_ProportionFloor; // 0x66F1E6..0x66F1F4
	}
	return std::clamp(p, 0.0f, 1.0f); // 0x66F1FA..0x66F222
}

float GetProportionRaised(entt::entity pile)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(pile))
	{
		return 1.0f;
	}
	const auto* info = PotInfoOf(*registry, pile);
	if (info == nullptr)
	{
		return 1.0f;
	}
	const auto& pot = registry->Get<const Pot>(pile);
	switch (info->potType)
	{
	case PotType::PileFood:
		return PileFoodProportionRaised(pot.amount, info->maxAmountInPot);
	case PotType::PileWood:
		return PileWoodProportionRaised(pot.amount, info->maxAmountInPot);
	default:
		return 1.0f;
	}
}

float GetScaleField(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (const auto* shield = registry->TryGet<const MapShield>(object))
	{
		// the shield's Object +0x50 (SetScale 0x639200); its Transform carries the drawn scale, which runs behind
		return shield->objectScale;
	}
	const auto* transform = registry->TryGet<const Transform>(object);
	// the uniform scale: every game object's Transform scale is glm::vec3(s)
	return transform != nullptr ? transform->scale.x : 0.0f;
}

float GetScale(entt::entity object)
{
	// Object 0x402520 = fld [ecx + 0x50]; Creature 0x47B190 -> CreaturePhysical::GetUserSize 0x4EF4F0 = [[+0x160]+0x58]+0x90
	// (inferido: openblack keeps the user size as the creature's Transform scale, so it is the field's read too)
	return GetScaleField(object);
}

float ObjectGet2DRadius(entt::entity object)
{
	const auto half = ObjectHalfExtents(object);
	if (!half)
	{
		return 0.0f; // 0x6381E9 without the Game3DObject (and openblack: without a loaded mesh)
	}
	return Radius2D(*half, GetScale(object)); // GetScale vt +0x120 (0x63818E)
}

float ObjectGetHeight(entt::entity object)
{
	const auto half = ObjectHalfExtents(object);
	if (!half)
	{
		return 0.0f; // 0x638140
	}
	return Height(*half, GetScaleField(object)); // the field +0x50 (0x638139), not vt +0x120
}

float Get2DRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AnyOf<Field, FishFarm>(object))
	{
		return k_FieldRadius; // Field 0x528E80 / FishFarm 0x52C470: fld [0x8AB6E4]
	}
	if (registry->AllOf<MagicTeleport>(object))
	{
		return k_MagicTeleportRadius; // MagicTeleport 0x5FCCB0 -> 0x5FCCA0: fld [0x92C108]
	}
	if (registry->AllOf<MagicFireBall>(object))
	{
		return GetScale(object) * k_MagicFireBallRadius; // MagicFireBall 0x682D20: vt +0x120 x [0x935910]
	}
	if (IsPileFood(*registry, object))
	{
		// PileFood 0x66F180: GetProportionRaised (vt +0x86C) stored, then call 0x638180 and fmul (0x66F197)
		const float proportion = GetProportionRaised(object);
		return ObjectGet2DRadius(object) * proportion;
	}
	// Creature 0x477F40 = the LH3DCreature's +0x5228 (inferido: not ported, the Object formula stands in)
	return ObjectGet2DRadius(object);
}

float GetRadius(entt::entity object)
{
	return Get2DRadius(object); // Object 0x638110: mov eax, [ecx]; jmp [eax + 0x64]
}

float GetHeight(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AllOf<MagicFireBall>(object))
	{
		return Get2DRadius(object); // MagicFireBall 0x682D30: jmp [vt + 0x64]
	}
	if (registry->AllOf<Creature>(object))
	{
		// Creature 0x477F50: fld [[[+0x160]+0x58]+0x90] (the user size, GetUserSize 0x4EF4F0); fmul [0x8C2C40].
		// (inferido): openblack has no CreaturePhysical, so the Transform's scale stands in for the user size
		return GetScale(object) * k_CreatureHeightPerScale;
	}
	return ObjectGetHeight(object);
}

float GetTopPos(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	const auto* transform = registry != nullptr && registry->Valid(object) ? registry->TryGet<const Transform>(object) : nullptr;
	const float altitude = transform != nullptr ? map_coords::FromWorld(transform->position).altitude : 0.0f;
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<MapShield>(object))
	{
		return 0.0f; // MapShield 0x72C1C0: fld [0x8AA398]
	}
	return GetHeight(object) + altitude; // 0x638160: [ecx + 0x1C] kept, vt +0x42C, fadd
}

float GetHeightForHandAboveInteractObject(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<FishFarm>(object))
	{
		return k_FieldRadius; // FishFarm 0x52C840: fld [0x8AB6E4]
	}
	return GetHeight(object); // 0x638150: jmp [vt + 0x42C]
}

float GetMeshRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AnyOf<Field, FishFarm>(object))
	{
		return k_FieldRadius; // Field 0x528A30 / FishFarm 0x52C480
	}
	const auto half = ObjectHalfExtents(object);
	return half ? HalfDiagonal(*half) : 0.0f; // Object 0x636BD0: fld [m + 0x30], no scale
}

float GetHoldRadius(entt::entity object, bool holdTypeAbove)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AnyOf<Tree, DeadTree>(object))
	{
		return Get2DRadius(object) * k_TreeHoldFactor; // 0x74B612 / 0x5110E2: vt +0x64; fmul [0x8AB244]
	}
	if (holdTypeAbove)
	{
		return GetHeight(object) * k_HoldAboveHeightFactor; // 0x638C14: vt +0x42C; fmul [0x8AB274]
	}
	return Get2DRadius(object); // 0x638C26
}

float GetDefaultFireRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object))
	{
		if (registry->AllOf<DeadTree>(object))
		{
			return GetHeight(object) * k_DeadTreeFireFactor; // 0x510E12: vt +0x42C; fmul [0x8D6974]
		}
		if (registry->AllOf<WorshipSite>(object))
		{
			return k_WorshipSiteRadius; // 0x77DE10 -> 0x77DDD0: fld [0x99C9EC]
		}
	}
	return Get2DRadius(object); // Object 0x639AC0: jmp [vt + 0x64]
}

namespace
{
float TreeHugRadius(entt::entity object)
{
	// 0x74A1A5..0x74A1CA: Get2DRadius x 0.1; fcomp 0.25; > 0.25 -> 0.25, else Get2DRadius x 0.1 again
	const float scaled = Get2DRadius(object) * k_TreeHugFactor;
	return scaled > k_TreeHugMax ? k_TreeHugMax : scaled;
}
} // namespace

float GetVillagerHugRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Tree>(object))
	{
		return TreeHugRadius(object); // Tree 0x74A1A0
	}
	const float scaled = Get2DRadius(object) * k_HugRadiusFactor; // 0x4026B5: fmul [0x8AA3A0]
	return scaled + k_HugRadiusMargin;                            // 0x4026BB: fadd [0x8AA39C]
}

float GetRoutePlanRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Tree>(object))
	{
		return TreeHugRadius(object); // Tree 0x74A140 (its own copy of 0x74A1A0)
	}
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Temple>(object))
	{
		return Get2DRadius(object) * k_CitadelHeartRoutePlanFactor; // CitadelHeart 0x4680C0: vt +0x64; fmul [0x8CA268]
	}
	return Get2DRadius(object); // Object 0x6384C0 with no creature (0x6384CF)
}

namespace
{
std::optional<glm::vec3> PositionOf(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry->TryGet<const Transform>(object);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}
} // namespace

float GetDistanceFromObject(entt::entity object, entt::entity other)
{
	if (const auto* registry = RegistryOrNull();
	    registry != nullptr && registry->Valid(object) && registry->AllOf<WorshipSite, Transform>(object))
	{
		// WorshipSite 0x77DE20: GetDistanceInMetres(CalculateCentrePos, b) (0x77DE36..0x77DE3C), vt +0x64 of b
		// (0x77DE4C), GetRealRadius 0x77DDD0 + it (fadd 0x77DE5A), fsubr (0x77DE60)
		const auto b = PositionOf(other);
		const float distance = b ? gutils::GetDistanceInMetres(WorshipSiteCentre(object), *b) : 0.0f;
		const float otherRadius = Get2DRadius(other);
		const float radii = k_WorshipSiteRadius + otherRadius;
		return distance - radii;
	}
	const auto a = PositionOf(object);
	const auto b = PositionOf(other);
	const float distance = a && b ? gutils::GetDistanceInMetres(*a, *b) : 0.0f; // 0x637FC1
	const float otherRadius = Get2DRadius(other);                               // 0x637FD1
	const float radii = Get2DRadius(object) + otherRadius;                      // 0x637FDC..0x637FDF
	return distance - radii;                                                    // fsubr [esp] (0x637FE5)
}

float GetDistanceFromObject(entt::entity object, glm::vec3 point)
{
	const auto a = PositionOf(object);
	const float distance = a ? gutils::GetDistanceInMetres(*a, point) : 0.0f; // 0x5702BC
	return distance - GetRadius(object);                                     // vt +0x60; fsubr (0x5702CF)
}

bool IsTouching(entt::entity object, entt::entity other, float margin)
{
	return GetDistanceFromObject(object, other) <= margin; // fcomp; test ah, 0x41 (0x637E0D..0x637E16)
}

bool IsTouching(entt::entity object, glm::vec3 point)
{
	return GetDistanceFromObject(object, point) <= 0.0f; // fcomp [0x8AA398] (0x637E3D)
}

BoundingSphere GetBoundingSphere(entt::entity object)
{
	const float h = GetHeight(object) * k_Half; // 0x637736..0x637746
	float r = Get2DRadius(object);              // 0x63774A
	if (const auto* registry = RegistryOrNull();
	    registry != nullptr && registry->Valid(object) &&
	    registry->AnyOf<Villager, Animal, MobileStatic, DeadTree, Fragment, MagicTeleport>(object))
	{
		// Living 0x5ED2F0 / MobileStatic 0x608F40: the same routine with the radius halved (fmul [0x8AA3B4] at
		// 0x5ED30D / 0x608F5D) before the square
		r = r * k_Half;
	}
	const float rr = r * r;
	const float hh = h * h;
	const float radius = std::sqrt(rr + hh); // 0x63774D..0x637762
	const auto position = PositionOf(object).value_or(glm::vec3(0.0f));
	// MapCoords::GetLHPoint: x, z = fild x 10 / 65536; y = GetAltitude 0x803090 + altitude (fst at 0x63777E), then + h
	auto centre = map_coords::ToWorld(map_coords::FromWorld(position));
	centre.y = centre.y + h; // 0x637798
	return {centre, radius};
}

glm::vec3 WorshipSiteCentre(entt::entity site)
{
	const auto* registry = RegistryOrNull();
	const auto* transform = registry != nullptr && registry->Valid(site) ? registry->TryGet<const Transform>(site) : nullptr;
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	// 0x77DD61..0x77DDB1, per component: (m[i] x 12.55 - m[6 + i] x 26.1) + m[9 + i], the right and forward rows and
	// the position; spelled out so that no FMA joins them
	const glm::vec3& right = transform->rotation[0];
	const glm::vec3& forward = transform->rotation[2];
	glm::vec3 centre;
	for (int i = 0; i < 3; ++i)
	{
		const float r = right[i] * k_WorshipSiteCentreRight;
		const float f = forward[i] * k_WorshipSiteCentreBack;
		const float d = r - f;
		centre[i] = d + transform->position[i];
	}
	return centre;
}

map_coords::MapCoords MapCoordsOf(entt::entity object)
{
	const auto position = PositionOf(object);
	return position ? map_coords::FromWorld(*position) : map_coords::MapCoords {};
}

map_coords::MapCoords GetNearestPosOfObject(entt::entity object, entt::entity other)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other)); // 0x636D44
	const float otherRadius = Get2DRadius(other);                         // 0x636D54
	const float radius = Get2DRadius(object) + otherRadius;               // 0x636D5F..0x636D62
	return me + gutils::GetPosFromAngle(angle, radius);                   // 0x636D74, + 0x636D84
}

map_coords::MapCoords GetNearestEdgeToPos(entt::entity object, const map_coords::MapCoords& pos)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, pos);               // 0x636DB0
	return me + gutils::GetPosFromAngle(angle, Get2DRadius(object));     // 0x636DC0, 0x636DD1, + 0x636DE1
}

map_coords::MapCoords GetNearestEdge(entt::entity object, float angle, float extra)
{
	const float radius = Get2DRadius(object) + extra;                    // 0x636DF9..0x636DFC
	return MapCoordsOf(object) + gutils::GetPosFromAngle(angle, radius); // 0x636E0E, + 0x636E1F
}

map_coords::MapCoords GetWorkingPos(entt::entity object, entt::entity other)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other)); // 0x639564
	const float myRadius = GetRadius(object);                             // 0x639574
	const float radius = GetRadius(other) + myRadius;                     // 0x63957F..0x639582
	return me + gutils::GetPosFromAngle(angle, radius);                   // 0x639594, + 0x605520
}

map_coords::MapCoords TreeGetWorkingPos(entt::entity tree, entt::entity other)
{
	const auto me = MapCoordsOf(tree);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other)); // 0x74C051
	const float radius = Get2DRadius(other) + k_TreeWorkingReach;         // 0x74C061..0x74C064
	return me + gutils::GetPosFromAngle(angle, radius);                   // 0x74C07D, + 0x74C08D
}

map_coords::MapCoords BigForestGetArrivePos(entt::entity bigForest, entt::entity villager)
{
	const auto me = MapCoordsOf(bigForest);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(villager)); // 0x439373
	const float radius = GetRadius(bigForest) * k_Half;                      // 0x439383..0x439386
	return me + gutils::GetPosFromAngle(angle, radius);                      // 0x43939F, + 0x4393AF
}

void detail::SetMeshBoxProviderForTests(MeshBoxProvider provider)
{
	g_MeshBoxProvider = provider;
}

} // namespace openblack::ecs::object
