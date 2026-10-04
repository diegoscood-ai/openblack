/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SuperVillager.h"

#include <algorithm>
#include <bit>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>

#include <L3DFile.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "Camera/FieldOfView.h"
#include "Common/GameRandom.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/IntroSpecial.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::super_villager
{
using namespace components;

namespace
{
constexpr int32_t k_FadeMs = 300; ///< [0xC383D8]
/// [0x9A392C] = 0x407B53D2, rad/s (fn_00825530 0x8255E1)
constexpr float k_TurnRate = std::bit_cast<float>(0x407B53D2u);

/// One of g_first [0xEB9A08]: the thing (+0x18) and the ids of its Eyes' six objects (+0x1C), kept here so that
/// Release can delete them even when the thing (and its SuperVillager component) is gone
struct Entry
{
	entt::entity thing {entt::null};
	eyes::Objects eyes {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};
/// g_first [0xEB9A08]: fn_00825E40 links a new one at the head
std::vector<Entry> s_List;
/// [0xFAA7E8] / [0xFAA7EC] / [0xFAA7F0]: never reset by the original
eyes::Shared s_Shared;
/// (pending) fn_005DFCE0's point while [0xD19C34] != 0
std::optional<glm::vec3> s_HandGrip;
/// [0xD20188]: the swim rings' clock, one for all the swimmers (bss, read and written only at 0x5E4C7D..0x5E4CA2)
int32_t s_SwimRingMs = 0;
constexpr int32_t k_SwimRingEveryMs = 1000;                  ///< [0xBF3584]
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu); ///< 0x5E4CB4
/// [0xC37D9C] +0x114 / +0x11C: bone 5's x and z (5 x 0x30 + 0x24 / + 0x2C)
constexpr size_t k_SwimRingBone = 5;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

} // namespace

const HdModel* LoadHdModel(const std::string& relative, const std::string& name)
{
	static std::map<std::string, std::optional<HdModel>> s_Models;
	if (const auto it = s_Models.find(relative); it != s_Models.end())
	{
		return it->second.has_value() ? &*it->second : nullptr;
	}
	auto& slot = s_Models[relative];
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto bytes = fileSystem.ReadAll(fileSystem.GetPath<filesystem::Path::Misc>() / (relative + ".l3d"));
		l3d::L3DFile file;
		if (file.Open(bytes) != l3d::L3DResult::Success)
		{
			throw std::runtime_error("not an L3D");
		}
		HdModel model;
		model.mesh = resources::HashIdentifier("misc/" + relative);
		auto& meshes = Locator::resources::value().GetMeshes();
		if (!meshes.Contains(model.mesh))
		{
			// (pending, Motor M3: engine::gpu::Submit or preload) the mesh's bgfx buffers are made here, from op 290
			// during the turn (game logic), not before the game or through the GPU queue
			meshes.Load(model.mesh, resources::L3DLoader::FromBufferTag {}, name, bytes);
		}
		const bool hasEBone = (static_cast<uint32_t>(file.GetHeader().flags) &
		                       static_cast<uint32_t>(l3d::L3DMeshFlags::ContainsEBone)) != 0;
		if (const auto& eBone = file.GetEBone(); hasEBone && eBone.has_value())
		{
			eyes::HostModel host;
			host.name = name;
			host.matrices = {eBone->matrices[0], eBone->matrices[1]};
			host.bones = {eBone->bones[0], eBone->bones[1]};
			if (eyes::l3d_patch::FirstSkinId(bytes, host.skinId))
			{
				host.skin = eyes::l3d_patch::FindSkin(bytes, host.skinId);
			}
			model.host = std::move(host);
		}
		slot = std::move(model);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "SuperVillager: cannot load {}: {}", relative, e.what());
		return nullptr;
	}
	return &*slot;
}

namespace
{
/// The LH3DObject's +0x98 (the high detail slot) the op compares: the info.dat high mesh, the child one for a child
/// (Villager::SetAge 0x7528C0, GetDetailMesh(2, 1, 0)). (approximate) only villagers: other things keep their mesh
std::optional<MeshId> HighMesh(entt::entity thing)
{
	const bool isVillager = Entities().AllOf<Villager>(thing);
	const auto* info = isVillager ? VillagerInfoOf(thing) : nullptr;
	if (info == nullptr)
	{
		return std::nullopt;
	}
	return villager::IsChild(thing) ? info->childMeshHigh : info->highDetail;
}

/// openblack's one drawn mesh, or the one kept while the villager is hidden (SkeletalAnimation::hiddenMesh)
entt::id_type* DrawnMesh(entt::entity thing)
{
	auto& registry = Entities();
	if (auto* mesh = registry.TryGet<Mesh>(thing); mesh != nullptr)
	{
		return &mesh->id;
	}
	if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr && animation->hiddenMesh != 0)
	{
		return &animation->hiddenMesh;
	}
	return nullptr;
}

/// fn_00825E70's part on the LH3DObject: obj+0x88..+0x94 cleared, and the smooth-drawing parameters off
void ClearSmoothing(entt::entity thing)
{
	auto& registry = Entities();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr)
	{
		animation->crossFadeMs = 0;
		animation->crossFadeFrozen = false;
		animation->crossFade = {};
		animation->drawnPose.clear();
	}
	if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
	{
		draw->followRate = 0.0f;
		draw->followSnap = false;
		draw->followTurn = true;
		draw->followFrozen = false;
		draw->hasFollowYaw = false;
		draw->followDrawnTurn = 0.0f;
	}
}

/// fn_00825F20: nothing when the thing has one already (0x825F49; its HD mesh stays loaded, as the original leaks it)
void Create(entt::entity thing, int32_t eyeType, const HdModel* model, std::optional<MeshId> high)
{
	auto& registry = Entities();
	if (registry.AllOf<SuperVillager>(thing))
	{
		return;
	}
	auto& super = registry.Assign<SuperVillager>(thing);
	s_List.insert(s_List.begin(), Entry {thing}); // fn_00825E40
	super.eyeType = eyeType;
	// 0x825FBC: +0x14 = obj+0x48, the yaw the object was drawn with last (Villager::Draw's +0x108 in
	// LH3DObject::SetPosition's convention: ECS/MobileDrawing's drawn yaw + 90 degrees). (openblack) not drawn yet (no
	// drawn yaw): UpdateMobileDrawing takes it on its first followed frame
	if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr && draw->hasYaw)
	{
		draw->followYaw = draw->yaw + glm::half_pi<float>();
		draw->hasFollowYaw = true;
	}
	// 0x825FC5..0x825FF8: +0x20..+0x28 = the three meshes, then vt+0xF4(hd, 0, 0) when there is an HD mesh. The
	// SuperVillager draws the high slot (0x825C8A): the HD mesh, or the thing's own high mesh
	super.hdMesh = model != nullptr ? model->mesh : 0;
	const entt::id_type drawn = super.hdMesh != 0 ? super.hdMesh : high.has_value() ? resources::HashIdentifier(*high) : 0;
	if (auto* mesh = DrawnMesh(thing); mesh != nullptr && drawn != 0)
	{
		super.savedMesh = *mesh;
		*mesh = drawn;
	}
	// 0x825FFB..0x82615D: an Eyes when the high slot's mesh has an EBone block and the type is not -1
	if (model != nullptr && model->host.has_value() && eyeType != -1)
	{
		super.eyes = eyes::Create(thing, eyeType, *model->host);
		s_List.front().eyes = super.eyes->objects;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "SuperVillager: {} (eye type {}, {})", static_cast<uint32_t>(thing), eyeType,
	                   model != nullptr ? "HD mesh" : "its high mesh");
}

/// Release 0x826180 + fn_00825E70 on a thing still alive (the eyes and the list entry are the caller's)
void Restore(entt::entity thing)
{
	auto& registry = Entities();
	auto* super = registry.TryGet<SuperVillager>(thing);
	if (super == nullptr)
	{
		return;
	}
	// 0x826183..0x8261A1: the three meshes back
	if (auto* mesh = DrawnMesh(thing); mesh != nullptr && super->savedMesh != 0)
	{
		*mesh = super->savedMesh;
	}
	ClearSmoothing(thing);
	registry.Remove<SuperVillager>(thing);
}

/// Release 0x826180 on one entry already out of the list: fn_00825E70 0x825EDF..0x825EF6 deletes the Eyes whatever the
/// thing's state; the rest only on a thing still alive
void ReleaseEntry(Entry& entry)
{
	eyes::Destroy(entry.eyes);
	if (Locator::entitiesRegistry::has_value() && Entities().Valid(entry.thing))
	{
		Restore(entry.thing);
	}
}

/// fn_00825400 0x825403..0x82541D: LH3DBoundingBox::CheckRegionOnScreen 0x868C80 (path [0xEA9EB4] == 0) of the resolved
/// mesh's box +0x14 under the drawing object: graphics::region_on_screen::SphereOnScreen, the faithful port of 0x868C80.
/// centre = the box's centre +4 (LH3DMesh::ComputeBoundingBox 0x8081B0: (min + max) / 2 of the rest-posed vertices of
/// every submesh = AxisAlignedBoundingBox::Center) through the object's matrix (0x868CBC..0x868D04); radius = obj +0x44
/// (the scale Villager::Draw gives SetPosition) x box +0x1C (|max - min| / 2, 0x80835E..0x808379); origin = the
/// matrix's translation +0x38 (0x868DA1). (approximate) the drawn matrix of the frame before (Update runs before
/// ECS/MobileDrawing; the original tests the matrix Villager::Draw set this frame); 0x868C80's side effects
/// (g_b_last_on_screen, g_last_selected_box, g_last_distance, vt+0xA0) and the fn_007ACC60 path are not ported
bool RegionOnScreen(entt::entity thing)
{
	auto& registry = Entities();
	const auto* mesh = DrawnMesh(thing);
	field_of_view::View view;
	if (mesh == nullptr || !registry.AllOf<Transform>(thing) || !Locator::resources::has_value() ||
	    !field_of_view::CurrentView(view))
	{
		return true;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*mesh))
	{
		return true;
	}
	const auto box = meshes.Handle(*mesh)->GetBoundingBox();
	const auto model = DrawnModel(registry, thing);
	const auto& scale = registry.Get<const Transform>(thing).scale;
	const glm::vec3 centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
	const float radius = std::max({scale.x, scale.y, scale.z}) * (glm::length(box.Size()) * 0.5f);
	return graphics::region_on_screen::SphereOnScreen(view, centre, radius, glm::vec3(model[3]));
}

/// GLandscape::Draw 0x5E4C7D..0x5E4D4B, for each swimmer after its under-water draw (off screen too: fn_00825400 comes
/// later): the shared clock += g_game_time_inc; past 1000 ms (signed jle) it restarts at 0 (not -1000) and a ring is
/// made at (x, 0.05, z) of bone 5 in the bone buffer [0xC37D9C], turned by Random(0, 2 pi) (the CRT stream, drawn at
/// 0x5E4CC2 before the pool is searched: a full pool still takes the draw). Ring = age 0, +0x18 growth 1, +0x20 the
/// angle, +0x24 1, +0x28 aspect 1, +0x2C rate 1, +0x30 cell 0x30, +0x34 0xFFFFFFFF (white, not the land light); +0x1C
/// left as the slot had it. (approximate) the buffer holds the last pose made before this point of the frame (the body
/// is posed later, fn_008254A0 0x5E6255): here the swimmer's own pose, drawn last frame
void SwimRing(const Registry& registry, entt::entity thing, const SkeletalAnimation& animation)
{
	s_SwimRingMs += static_cast<int32_t>(game_clock::FrameGameMs()); // [0xEA9EC0]
	if (s_SwimRingMs <= k_SwimRingEveryMs)
	{
		return;
	}
	s_SwimRingMs = 0;
	const auto model = DrawnModel(registry, thing);
	const glm::vec3 bone = animation.pose.size() > k_SwimRingBone ? glm::vec3(model * animation.pose[k_SwimRingBone][3])
	                                                               : glm::vec3(model[3]);
	WaterRing ring;
	ring.angle = game_random::crt::Random(0.0f, k_TwoPi);
	ring.position = glm::vec3(bone.x, 0.05f, bone.z); // 0x3D4CCCCD
	ring.age = 0;
	ring.growth = 1.0f;
	ring.aspect = 1.0f;
	ring.rate = 1.0f;
	ring.cell = 0x30;
	ring.argb = 0xFFFFFFFFu;
	AddWaterRing(ring); // 1024 slots; full: no ring, the clock and the draw already spent
}

/// MobileWallHug::SetYAngle (vt+0x524, 0x639260): villager::SetYAngle, the one copy (ECS/Villager/VillagerScript.h)
void SetYAngle(entt::entity thing, float angle)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(thing);
	auto* transform = registry.TryGet<Transform>(thing);
	if (wallHug != nullptr && transform != nullptr)
	{
		villager::SetYAngle(*transform, *wallHug, angle);
	}
}

/// Object::GetYAngle (vt+0x508): +0x4C, WallHug::yAngle here
float GetYAngle(entt::entity thing)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(thing);
	return wallHug != nullptr ? wallHug->yAngle : 0.0f;
}
} // namespace

void SetHighGraphicsDetail(entt::entity thing, bool on)
{
	if (!on)
	{
		// 0x708E02..0x708E1A: vt+0x70(0) (the normal draw back), DrawListRebuildCount = 1, fn_00825440
		Release(thing);
		return;
	}
	// 0x708D2D: vt+0x70(1) (obj+8 |= 0x10: LH3DObject::AddDrawing 0x815A7C skips it; here the entity is the body itself)
	// 0x708D40..0x708DC5: the high slot against MeshPack[501], [498], [439], [420] in that order
	const auto high = HighMesh(thing);
	int32_t eyeType = -1;
	const HdModel* model = nullptr;
	if (high == MeshId::PersonNorseMaleA1)
	{
		eyeType = 0;
		model = LoadHdModel("Intro/nors_man", "nors_man"); // 0xC20574
	}
	else if (high == MeshId::PersonNorseFemaleA1)
	{
		eyeType = 1;
		model = LoadHdModel("Intro/nors_woman", "nors_woman"); // 0xC20554
	}
	else if (high == MeshId::PersonBoyWhite1)
	{
		eyeType = 2;
		model = LoadHdModel("Intro/nors_boy", "nors_boy"); // 0xC20534
	}
	else if (high == MeshId::PersonAnimalTrainer)
	{
		eyeType = 1;
		model = LoadHdModel("sable", "sable"); // 0xC20520 (no EBone block: no eyes)
	}
	Create(thing, eyeType, model, high);
	// 0x708DE7..0x708DEC: +0x18 = the thing, its shadow object's +0x10 = 0; DrawListRebuildCount = 1 (openblack has no
	// landscape draw list)
}

void ThingJcSpecial(entt::entity thing, int32_t feature, bool on)
{
	auto& registry = Entities();
	// 0x70905B..0x709085: the thing must be one of the list, except for feature 19
	auto* super = registry.TryGet<SuperVillager>(thing);
	if (super == nullptr && feature != 19)
	{
		return;
	}
	switch (feature) // jump table 0x709130 (feature - 7, 0..12)
	{
	case 7: // 0x70909E: drawn at the intro hand's grip (GLandscape::Draw 0x5E4BC8)
		super->flags |= k_FollowHand;
		break;
	case 8: // 0x7090AE: on, the smoothing and the fade back; off, none
		super->flags = on ? super->flags & ~k_Snap : super->flags | k_Snap;
		break;
	case 9: // 0x7090C2: SetYAngle(-GetYAngle()) (fchs: the heading mirrored, not turned round)
		SetYAngle(thing, -GetYAngle(thing));
		super->flags |= k_Snap;
		break;
	case 16: // 0x7090D0: + pi/2 [0x8C78D8]
		SetYAngle(thing, GetYAngle(thing) + glm::half_pi<float>());
		super->flags |= k_Snap;
		break;
	case 17: // 0x7090E2: - pi/2
		SetYAngle(thing, GetYAngle(thing) - glm::half_pi<float>());
		super->flags |= k_Snap;
		break;
	case 18: // 0x70910E: Intro::ReleaseAll 0x5DFC40 (ecs/IntroSpecial.h; [0xD19C34] stays, so does the grip)
		intro_special::ReleaseAll();
		break;
	case 19: // 0x70911A: the thing's LH3DObject vt+0x98(0), obj+4 bit 8 off (meaning pending)
		break;
	default: // 10..15 and outside 7..19: nothing
		break;
	}
}

bool Release(entt::entity thing)
{
	// fn_00825440: the one whose LH3DObject it is, if any
	const auto it = std::ranges::find(s_List, thing, &Entry::thing);
	if (it == s_List.end())
	{
		return false;
	}
	auto entry = *it;
	s_List.erase(it);
	ReleaseEntry(entry);
	return true;
}

void ReleaseAll()
{
	auto all = std::move(s_List);
	s_List.clear();
	for (auto& entry : all)
	{
		ReleaseEntry(entry);
	}
}

std::vector<entt::entity> List()
{
	std::vector<entt::entity> things;
	things.reserve(s_List.size());
	for (const auto& entry : s_List)
	{
		things.push_back(entry.thing);
	}
	return things;
}

void Update()
{
	if (s_List.empty())
	{
		return;
	}
	// 0x5E4B3A..0x5E4B93: without a script's wide screen (HelpSystem +0x45E8 && +0x45EC) every one goes, every frame
	const auto* helpSystem = help::Get();
	if (helpSystem == nullptr || !helpSystem->IsScriptWideScreen())
	{
		ReleaseAll();
		return;
	}
	auto& registry = Entities();
	for (const auto thing : List())
	{
		// 0x5E4BA5..0x5E4BB7 / 0x5E4E3C: the thing gone (IsAvailable vt+0x2C; (inferred) a destroyed entity here):
		// Release, its eyes deleted with it
		if (!registry.Valid(thing) || !registry.AllOf<SuperVillager>(thing))
		{
			Release(thing);
			continue;
		}
		auto& super = registry.Get<SuperVillager>(thing);
		const bool snap = (super.flags & k_Snap) != 0;
		// GLandscape::Draw 0x5E4C07: the clip "M_P_Swim2" -> the cut draw (renderer) and the swim ring
		if (const auto* animation = registry.TryGet<const SkeletalAnimation>(thing);
		    animation != nullptr && animation->hasClip && animation->clipIndex == static_cast<int32_t>(AnimId::PSwim2))
		{
			SwimRing(registry, thing, *animation);
		}
		// fn_00825400 0x82541D: off screen fn_00825530 does not run at all this frame
		super.onScreen = RegionOnScreen(thing);
		// fn_00825530: the 300 ms fade (0x8256C8..0x825755), not drawn with bit 2 (0x8257C4: DrawPosition::followSnap,
		// the one field of the bit for both stages)
		if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr)
		{
			animation->crossFadeMs = k_FadeMs;
			animation->crossFadeFrozen = !super.onScreen;
		}
		// 0x8255C1..0x8256BF: the yaw over the villager's drawn one, not with bit 2 (0x8255B8); the swim branch
		// (0x825A13) poses with the object's own matrix, unturned. (approximate) the swim test on the clip set last
		// frame, and a fade while swimming also turns it in the original (0x8257C4 comes first)
		if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
		{
			const auto* animation = registry.TryGet<const SkeletalAnimation>(thing);
			draw->followRate = k_TurnRate;
			draw->followSnap = snap;
			draw->followFrozen = !super.onScreen;
			draw->followTurn = animation == nullptr || animation->clipIndex != static_cast<int32_t>(AnimId::PSwim2);
		}
	}
}

void FollowHand()
{
	if (s_List.empty() || !s_HandGrip.has_value())
	{
		return;
	}
	auto& registry = Entities();
	for (const auto thing : List())
	{
		const auto* super = registry.Valid(thing) ? registry.TryGet<const SuperVillager>(thing) : nullptr;
		if (super == nullptr || (super->flags & k_FollowHand) == 0)
		{
			continue;
		}
		// GLandscape::Draw 0x5E4BC8..0x5E4BEE: bit 1 and [0xD19C34]: SetPosition(grip, obj+0x48, obj+0x44), the drawing
		// object at the grip with its yaw and scale and no slope shear. (pending) the grip comes from the intro hand.
		// fn_008254A0's own SetPosition(&[0xEA9F30] = (0, 0, 0), obj+0x48, obj+0x44) under [0xEB9A0C] != 0
		// (0x8254DD..0x825504) is dead code: 0xEB9A0C is bss with one reference in the image (the read 0x8254E3)
		// fn_005DFCE0 / GLandscape::Draw 0x5E4BC8: overwrites the DrawPosition written by UpdateMobileDrawing (Personas).
		// Only the DrawPosition (the drawing object's matrix): no Transform, no game state (SetDirty only re-reads the
		// draw instances)
		if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
		{
			draw->position = *s_HandGrip;
			draw->shearX = 0.0f;
			draw->shearZ = 0.0f;
			registry.SetDirty();
		}
	}
}

void Draw(int32_t milliseconds)
{
	auto& registry = Entities();
	for (const auto thing : List())
	{
		// a thing destroyed since Update: released now, so its eyes are not drawn this frame
		if (!registry.Valid(thing) || !registry.AllOf<SuperVillager>(thing))
		{
			Release(thing);
			continue;
		}
		auto& super = registry.Get<SuperVillager>(thing);
		if (!super.eyes.has_value())
		{
			continue;
		}
		// fn_00825400: off screen no fn_00825530, so no fn_00883560 (no draws of the CRT stream, no timers moved)
		if (!super.onScreen)
		{
			eyes::Hide(*super.eyes);
			continue;
		}
		// fn_00825530 0x825E1F..0x825E28: the eyes after the body
		eyes::Update(*super.eyes, s_Shared, thing, milliseconds);
	}
}

void FillFrame(graphics::SuperVillagerFrame& out)
{
	out.litByDefaultSun.clear();
	out.swimmers.clear();
	if (s_List.empty() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	for (const auto& entry : s_List)
	{
		const bool valid = registry.Valid(entry.thing);
		if (const auto* super = valid ? registry.TryGet<const SuperVillager>(entry.thing) : nullptr;
		    super != nullptr && super->hdMesh != 0)
		{
			out.litByDefaultSun.insert(super->hdMesh);
		}
		for (const auto object : entry.eyes)
		{
			if (const auto* eye = object != entt::null && registry.Valid(object)
			                          ? registry.TryGet<const SuperVillagerEye>(object)
			                          : nullptr;
			    eye != nullptr && eye->mesh != 0)
			{
				out.litByDefaultSun.insert(eye->mesh);
			}
		}
		// GLandscape::Draw 0x5E4C07: the clip "M_P_Swim2" (0xBF3598)
		const auto* animation = valid ? registry.TryGet<const SkeletalAnimation>(entry.thing) : nullptr;
		if (animation != nullptr && animation->hasClip && animation->clipIndex == static_cast<int32_t>(AnimId::PSwim2))
		{
			out.swimmers.push_back(entry.thing);
		}
	}
}

void SetIntroHandGrip(std::optional<glm::vec3> grip)
{
	s_HandGrip = grip;
}

void testing::Adopt(entt::entity thing, int32_t eyeType)
{
	auto& registry = Entities();
	if (registry.AllOf<SuperVillager>(thing))
	{
		return;
	}
	auto& super = registry.Assign<SuperVillager>(thing);
	super.eyeType = eyeType;
	super.eyes = eyes::Create(thing, eyeType, eyes::HostModel {}, std::array<entt::id_type, 5> {});
	s_List.insert(s_List.begin(), Entry {thing, super.eyes->objects}); // fn_00825E40
}

} // namespace openblack::ecs::super_villager
