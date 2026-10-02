/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesignedScenery.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <unordered_set>

#include <entt/entity/entity.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Device/Sound.h"
#include "Audio/Services/SoundTags.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// 0xBF34E4: the land the scenery was made for, 74 at start (no land)
int32_t g_LastLand = 74;
// 0xD1A31C / 0xD1A324: the objects (the waterfall or the ark; the dinosaur). Their meshes (0xD1A320 / 0xD1A328) stay
// loaded in the mesh manager instead of LH3DMesh::Release
entt::entity g_Object = entt::null;
entt::entity g_Object2 = entt::null;
// 0xD1A330: the SoundTag on the ScriptMarker 0xD1A32C (the marker is only its point here)
audio::sound_tags::TagId g_Tag = audio::sound_tags::k_NoTag;
// 0xD1A334: the waterfall's texture V offset, 0xD1A338: its ring timer (seconds of game time)
float g_V = 0.0f;
float g_RingTimer = 0.0f;

// Land 3 (0xD1A2C0, 0xBF34E8 angle, 0xBF34EC scale)
constexpr glm::vec3 k_WaterfallPos {3059.23f, 0.0f, 3145.33f};
constexpr float k_WaterfallAngle = 4.7f;
constexpr float k_WaterfallScale = 1.0f;
// 0xBF34F0: one ring per 0.7 s; 0x5E3A32..: its point, growth (0xBF34F4), rate (0xBF34F8), alpha (0xBF34FC)
constexpr float k_RingPeriod = 0.7f;
constexpr glm::vec3 k_RingPos {3018.8f, 0.2f, 3130.15f};
constexpr float k_RingGrowth = 30.0f;
constexpr float k_RingRate = 0.3f;
constexpr uint32_t k_RingAlpha = 0x80;
// Land 4 (0xD1A2B0 with 0xBF3500 / 0xBF3504; 0xD1A2A0 with 0xBF3508 / 0xBF350C)
constexpr glm::vec2 k_ArkPos {3538.0f, 2129.0f};
constexpr float k_ArkAngle = 8.9728f;
constexpr float k_ArkScale = 1.1f;
constexpr glm::vec2 k_DinosaurPos {2690.0f, 2590.0f};
constexpr float k_DinosaurAngle = 1.57f;
constexpr float k_DinosaurScale = 1.0f;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SCENERY_TRACE") != nullptr;
	return k_Trace;
}

/// LH3DMesh::CreateFromHD("Data\\MISC\\<file>.l3d"): loaded once into the mesh manager as "misc/<file>"
entt::id_type LoadMiscMesh(const char* file)
{
	const auto id = resources::HashIdentifier(fmt::format("misc/{}", file));
	auto& meshes = Locator::resources::value().GetMeshes();
	// a file that failed is not tried again every frame
	static std::unordered_set<entt::id_type> s_Failed;
	if (s_Failed.contains(id))
	{
		return 0;
	}
	if (!meshes.Contains(id))
	{
		try
		{
			const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / fmt::format("{}.l3d", file);
			meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Designed scenery: cannot load {}.l3d: {}", file, e.what());
			s_Failed.insert(id);
			return 0;
		}
	}
	return id;
}

/// LH3DObject::Create + SetMesh + UseDynamicLighting(1) + SetPosition(pos, angle, scale). The objects' +0x10 (float 1
/// for the waterfall, 10 for the ark and the dinosaur) is the LOD distance factor k of fn_00815A70 (vt+0x100):
/// D = min((k + 1) [0xC37EA0] [0xC3813C], 100000), LOD 1 / 2 / 3 / 4 below 23.33 D / 66.67 D / 86.67 D / beyond
/// (g_last_distance 0xEA1AF4), and a submesh is drawn when bits 29-31 of its flags hold the LOD. Every submesh of the
/// three meshes has 0xE0000800 (all the LODs), so k changes nothing on screen and is not kept.
entt::entity MakeObject(const char* file, const glm::vec3& position, float angle, float scale)
{
	const auto mesh = LoadMiscMesh(file);
	if (mesh == 0)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-angle)), glm::vec3(scale));
	registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: {} at ({:.2f}, {:.2f}, {:.2f}) angle {} scale {}", file,
		                   position.x, position.y, position.z, angle, scale);
	}
	return entity;
}

float Altitude(const glm::vec2& at)
{
	return Locator::terrainSystem::value().GetHeightAt(at);
}

/// ScriptMarker + SoundTag::Create(marker, 12 G_WaterFlow, false, mode 2, loops -1, 0, 3D, InGame, 0) (0x5E3921,
/// 0x5E3BD5). The marker is made with MapCoords(LHPoint) (y kept as y - altitude), so it sounds at y = 0.
audio::sound_tags::TagId MakeWaterFlowTag(const glm::vec3& point)
{
	audio::sound_tags::TagDesc desc;
	desc.sample = static_cast<entt::id_type>(audio::SoundId::G_WaterFlow);
	desc.point = point;
	return audio::sound_tags::Create(desc);
}

void DestroyObject(entt::entity& entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity != entt::null && registry.Valid(entity))
	{
		registry.Destroy(entity);
	}
	entity = entt::null;
}

void ProcessWaterfall(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (g_Object == entt::null)
	{
		// LH3DObject::Create(0): a static object
		g_Object = MakeObject("waterfall3", k_WaterfallPos, k_WaterfallAngle, k_WaterfallScale);
		if (g_Object == entt::null)
		{
			return;
		}
		registry.Assign<UvScroll>(g_Object);
		g_Tag = MakeWaterFlowTag(k_WaterfallPos);
	}
	// 0x5E392E: V = frac(V - 0.5 dt) (__ftol truncates, so it stays in -1..0), then SetUVOffset(0, V) (vt+0xE8
	// fn_007F9B70: +0x68 / +0x6C and bit 0x400 of +4, which nothing reads). The static Draw 0x80DB30 reads the offsets
	// through vt+0x8 / +0xC (0x80DE86) into 0xECA62C / 0xECA630 (0xECA628 = 1 if not both 0), and the triangle submit
	// LH3DRender::DrawTriangle 0x82F8BE adds them to the UVs unless the material's byte +5 has bit 0x10: the rock of
	// waterfall3.l3d (submesh 0, Textured, byte +5 = 0x14) stays still and only the water (submesh 1, TexturedChroma,
	// 0x04) flows (L3DSubMesh::Primitive::uvOffset).
	// (frame_anim::WaterfallScroll)
	const float v = graphics::frame_anim::WaterfallScroll(g_V, seconds);
	if (auto* scroll = registry.TryGet<UvScroll>(g_Object); scroll != nullptr)
	{
		scroll->v = v;
	}
	// 0x5E39BA: a ring at the foot once the timer passes 0.7 s (the timer restarts even when the pool is full)
	g_RingTimer += seconds;
	if (g_RingTimer > k_RingPeriod)
	{
		g_RingTimer = 0.0f;
		ecs::WaterRing ring;
		ring.position = k_RingPos;
		ring.age = 0;
		ring.growth = k_RingGrowth;
		ring.angle = 0.0f;
		ring.aspect = 1.0f;
		ring.rate = k_RingRate;
		ring.cell = 0x30;
		// (0x80 << 24) | ([0xEDDD08] & 0xFFFFFF): the rgb of the light table[255]. The drift field +0x1C is not written
		// (it keeps the slot's last value); openblack's rings have no slots nor drift, so none.
		ring.argb = (k_RingAlpha << 24u) | 0x00FFFFFFu;
		ring.seaLight = true;
		ecs::AddWaterRing(ring);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: waterfall ring, V {:.4f}, {} rings", g_V,
			                   ecs::GetWaterRings().size());
		}
	}
}

void ProcessLand4()
{
	if (g_Object == entt::null)
	{
		// LH3DObject::Create(1)
		g_Object = MakeObject("arche", glm::vec3(k_ArkPos.x, Altitude(k_ArkPos), k_ArkPos.y), k_ArkAngle, k_ArkScale);
		if (g_Object != entt::null)
		{
			// a morphable object (Create(ecx = 1) 0x5E3B08): UpdateMelting at 0x5E3C55
			Locator::entitiesRegistry::value().Assign<MorphWithTerrain>(g_Object);
			g_Tag = MakeWaterFlowTag(glm::vec3(k_ArkPos.x, 0.0f, k_ArkPos.y));
		}
	}
	if (g_Object2 == entt::null)
	{
		// fn_0081E9E0 adds its landscape footprint: dinosaur.l3d has ContainsLandscapeFeature, so the footprint pass
		// draws it like any placed mesh's (Renderer::DrawFootprintPass)
		g_Object2 = MakeObject("dinosaur", glm::vec3(k_DinosaurPos.x, Altitude(k_DinosaurPos), k_DinosaurPos.y),
		                       k_DinosaurAngle, k_DinosaurScale);
		if (g_Object2 != entt::null)
		{
			// a morphable object too (Create(ecx = 1) 0x5E3CF8): UpdateMelting at 0x5E3DBE
			Locator::entitiesRegistry::value().Assign<MorphWithTerrain>(g_Object2);
		}
	}
}
} // namespace

void ecs::designed_scenery::Update(float gameMilliseconds)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value() || !Locator::terrainSystem::has_value() ||
	    Game::Instance() == nullptr)
	{
		return;
	}
	const int32_t land = Game::Instance()->GetMapScriptGlobals().landNumber;
	if (land != g_LastLand)
	{
		// 0x5E37AB: a new land deletes what the last one had
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: land {} -> {}", g_LastLand, land);
		}
		g_LastLand = land;
		DestroyObject(g_Object);
		DestroyObject(g_Object2);
		audio::sound_tags::Delete(g_Tag);
		g_Tag = audio::sound_tags::k_NoTag;
	}
	if (land == 3)
	{
		// g_game_time_inc * 0.001
		ProcessWaterfall(gameMilliseconds * 0.001f);
	}
	else if (land == 4)
	{
		ProcessLand4();
	}
}

void ecs::designed_scenery::OnLoadMap()
{
	g_Object = entt::null;
	g_Object2 = entt::null;
	g_Tag = audio::sound_tags::k_NoTag;
}
