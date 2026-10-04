/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SuperVillagerEyes.h"

#include <cmath>
#include <cstring>

#include <exception>
#include <string>

#include <L3DFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ModelLight.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::super_villager::eyes
{
using namespace components;

namespace
{
// fn_00883560's constants
constexpr int32_t k_CloseMs = 50;       ///< 0x8835B2 / 0x8835D0: closing and opening take 50 ms each
constexpr int32_t k_EndMs = 100;        ///< 0x883583: the blink ends after hold + 100 ms
constexpr float k_LidSpeed = 0.02f;     ///< [0x8CB930], closure per ms
constexpr float k_GlanceSpeed = 0.7f;   ///< [0xC3A214], rad/s
constexpr float k_MsToSeconds = 0.001f; ///< [0x8AC418]
constexpr float k_GlanceRange = 0.25f;  ///< 0x8836AF / 0x8836B6
constexpr uint32_t k_SquintMs = 300;    ///< [0xC3A218]
constexpr float k_SquintMax = 0.3f;     ///< 0x8836E2 (0x3E99999A)
constexpr float k_UpperLid = 0.47f;     ///< [0x8C7A4C]
constexpr float k_LowerLid = -0.35f;    ///< [0x9A3D80]
constexpr double k_Pi = 3.1415927410125732; ///< [0x8D45D0], the double of float pi
/// 0xC3A1D8: per type, eye 0 (yaw, pitch) and eye 1 (yaw, pitch) of the lighting normal
constexpr std::array<std::array<float, 4>, 3> k_ShadeAngles = {{
    {0.0f, 0.0f, 0.195f, 0.262f},             // man
    {-0.15625f, -0.045f, 0.34f, -0.253333f},  // woman
    {-0.28125f, -0.165f, 0.705f, -0.333333f}, // boy
}};
/// 0xC3A208
constexpr std::array<int32_t, 3> k_IrisCell = {4, 3, 5};
/// 0xC3A21C..0xC3A340, slots 0..4
constexpr std::array<const char*, 5> k_Files = {"r_paupe_up", "r_paupe_down", "l_paupe_up", "l_paupe_down", "eye_ball"};
constexpr size_t k_SkinBytes = sizeof(l3d::L3DTexture); ///< an embedded skin: its id and 256 x 256 texels
constexpr size_t k_EyeballRight = 4;
constexpr size_t k_EyeballLeft = 5;

bool ReadU32(const std::vector<uint8_t>& file, size_t offset, uint32_t& value)
{
	if (offset + 4 > file.size())
	{
		return false;
	}
	std::memcpy(&value, file.data() + offset, 4);
	return true;
}

void WriteU32(std::vector<uint8_t>& file, size_t offset, uint32_t value)
{
	std::memcpy(file.data() + offset, &value, 4);
}

/// every primitive header's offset, submesh by submesh
std::vector<uint32_t> PrimitiveHeaders(const std::vector<uint8_t>& file)
{
	std::vector<uint32_t> headers;
	uint32_t submeshCount = 0;
	uint32_t submeshOffsets = 0;
	if (!ReadU32(file, 3 * 4, submeshCount) || !ReadU32(file, 4 * 4, submeshOffsets))
	{
		return headers;
	}
	for (uint32_t i = 0; i < submeshCount; ++i)
	{
		uint32_t submesh = 0;
		uint32_t primitiveCount = 0;
		uint32_t primitiveOffsets = 0;
		if (!ReadU32(file, submeshOffsets + 4 * i, submesh) || !ReadU32(file, submesh + 4, primitiveCount) ||
		    !ReadU32(file, submesh + 8, primitiveOffsets))
		{
			return {};
		}
		for (uint32_t j = 0; j < primitiveCount; ++j)
		{
			uint32_t primitive = 0;
			if (!ReadU32(file, primitiveOffsets + 4 * j, primitive) || primitive + 48 > file.size())
			{
				return {};
			}
			headers.push_back(primitive);
		}
	}
	return headers;
}

/// LH3DMesh::CreateFromHD(Data\MISC\Eyes\<file>.l3d, 1) (0x884431) with the ctor's material and uv edits, once per
/// file (and host skin for the lids) into the mesh manager. (inferred) the original's per-Eyes copies are identical.
/// (pending, Motor M3: engine::gpu::Submit or preload) it makes the meshes' bgfx buffers from op 290 / 349 during the
/// turn (game logic), not before the game or through the GPU queue
entt::id_type LoadEyeMesh(int32_t type, int slot, const HostModel& host)
{
	const auto file = File(type, slot);
	const bool eyeball = slot == 4;
	const auto name = eyeball ? "misc/Eyes/" + file : "misc/Eyes/" + file + "@" + host.name;
	const auto id = resources::HashIdentifier(name);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		auto bytes = fileSystem.ReadAll(fileSystem.GetPath<filesystem::Path::Misc>() / "Eyes" / (file + ".l3d"));
		if (eyeball)
		{
			// 0x884456..0x884466: the first material's texture = [0xEA1A90], .\data\Textures\misc0.raw ("raw/misc0")
			l3d_patch::SetFirstSkinId(bytes, entt::hashed_string("raw/misc0").value());
		}
		else
		{
			// 0x884491..0x88449F: the host mesh's first material texture; openblack's mesh takes the skin with it
			l3d_patch::SetFirstSkinId(bytes, host.skinId);
			if (!host.skin.empty())
			{
				l3d_patch::AppendSkin(bytes, host.skin);
			}
			// 0x8844B8..0x884533: fn_008840E0(type) (0 <= type < 3): every uv doubled
			if (type >= 0 && type < 3)
			{
				l3d_patch::DoubleUvs(bytes);
			}
		}
		// (pending, Motor M3: engine::gpu::Submit or preload) bgfx buffers created during the turn
		meshes.Load(id, resources::L3DLoader::FromBufferTag {}, name, bytes);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "SuperVillager eyes: cannot load {}: {}", file, e.what());
		return 0;
	}
	return id;
}

void Place(Registry& registry, entt::entity object, const glm::mat4& world, uint32_t colour, uint32_t specular)
{
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	auto& eye = registry.Get<SuperVillagerEye>(object);
	if (!registry.AllOf<Mesh>(object) && eye.mesh != 0)
	{
		registry.Assign<Mesh>(object, eye.mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	auto& transform = registry.Get<Transform>(object);
	// the whole drawn matrix in the rotation (the mirror, the host's scale and shear), so T R S rebuilds it exactly
	transform.position = glm::vec3(world[3]);
	transform.rotation = glm::mat3(world);
	transform.scale = glm::vec3(1.0f);
	// the object's +0x4C / +0x50 (0x883A3D..0x883A82): drawn instead of the land light, x the model light per vertex
	registry.AssignOrReplace<ObjectColour>(
	    object, ObjectColour {{static_cast<uint8_t>(colour >> 16), static_cast<uint8_t>(colour >> 8), static_cast<uint8_t>(colour)},
	                          specular});
}

/// The upper 3x3 of an eye matrix (glm's columns = the LHMatrix rows) turned in place by an lh_matrix pair turn
template <typename Turn>
glm::mat4 TurnedRows(const glm::mat4& eye, Turn&& turn)
{
	glm::mat3 rows(eye);
	turn(rows);
	glm::mat4 m = eye;
	for (int i = 0; i < 3; ++i)
	{
		m[i] = glm::vec4(rows[i], 0.0f);
	}
	return m;
}
} // namespace

void Hide(const Eyes& eyes)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : eyes.objects)
	{
		if (object != entt::null && registry.Valid(object) && registry.AllOf<Mesh>(object))
		{
			registry.Remove<Mesh>(object);
		}
	}
}

void Step(Timers& timers, Shared& shared, int32_t milliseconds, const RandomFn& random)
{
	timers.closure = 0.0f; // 0x883574
	if (timers.blinking)
	{
		const int32_t t = timers.blinkMs;
		if (t > timers.holdMs + k_EndMs)
		{
			// 0x88358D..0x8835AD: the blink is over, the next one holds Random(100, 200) ms (ftol)
			timers.blinking = false;
			timers.holdMs = static_cast<int32_t>(random(100.0f, 200.0f));
			timers.blinkMs = 0;
			timers.field28 = 0;
		}
		else
		{
			if (t < k_CloseMs)
			{
				timers.closure = static_cast<float>(t) * k_LidSpeed;
			}
			else if (t < timers.holdMs + k_CloseMs)
			{
				timers.closure = 1.0f;
			}
			else
			{
				timers.closure = 1.0f - static_cast<float>(t - timers.holdMs - k_CloseMs) * k_LidSpeed;
			}
			timers.blinkMs = t + milliseconds;
		}
	}
	else
	{
		// 0x883614..0x88363F: the countdown; below 0 a blink starts and the next one is Random(1000, 5000) ms away
		timers.untilBlinkMs -= milliseconds;
		if (timers.untilBlinkMs < 0)
		{
			timers.blinking = true;
			timers.untilBlinkMs = static_cast<int32_t>(random(1000.0f, 5000.0f));
		}
	}
	// 0x883642..0x8836C0: the glance
	const float step = (static_cast<float>(milliseconds) * k_GlanceSpeed) * k_MsToSeconds;
	if (shared.glanceTarget == timers.glance)
	{
		shared.glanceTarget = random(-k_GlanceRange, k_GlanceRange);
	}
	else if (timers.glance > shared.glanceTarget)
	{
		timers.glance = timers.glance - step;
		if (timers.glance < shared.glanceTarget)
		{
			timers.glance = shared.glanceTarget;
		}
	}
	else
	{
		timers.glance = step + timers.glance;
		if (timers.glance > shared.glanceTarget)
		{
			timers.glance = shared.glanceTarget;
		}
	}
	// 0x8836C6..0x883712: the squint, reset to 0 (not - 300) past 300 ms
	shared.squintMs += static_cast<uint32_t>(milliseconds);
	if (shared.squintMs > k_SquintMs)
	{
		shared.squintMs = 0;
		shared.squint = random(0.0f, k_SquintMax);
	}
	if (timers.closure < shared.squint)
	{
		timers.closure = shared.squint;
	}
}

glm::mat4 Mirror()
{
	const double c = std::cos(k_Pi);
	const auto s = static_cast<float>(std::sin(k_Pi)); // fst [ebp-0x18]
	glm::mat4 m(0.0f);
	// columns = the LHMatrix rows, stored at 0x8837B3 (m0), 0x8837C1 (m2), 0x88378B (m4), 0x8837E1 (m6), 0x8837E9 (m8)
	m[0] = glm::vec4(static_cast<float>(-c - 0.0), 0.0f, 0.0f + -s, 0.0f);
	m[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	m[2] = glm::vec4(0.0f - s, 0.0f, static_cast<float>(c), 0.0f);
	m[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	return m;
}

glm::mat4 EBoneMatrix(const std::array<float, 12>& cells)
{
	glm::mat4 m(1.0f);
	for (int row = 0; row < 4; ++row)
	{
		m[row] = glm::vec4(cells[static_cast<size_t>(row * 3)], cells[static_cast<size_t>(row * 3 + 1)],
		                   cells[static_cast<size_t>(row * 3 + 2)], row == 3 ? 1.0f : 0.0f);
	}
	return m;
}

glm::mat4 EyeMatrix(const glm::mat4& boneWorld, const std::array<float, 12>& eBoneCells)
{
	// (approximate) composed in the world, not in camera space and back through [0xEA9DE0]: a few ulp apart
	return boneWorld * EBoneMatrix(eBoneCells) * Mirror();
}

glm::mat4 Glance(const glm::mat4& eye, float glance)
{
	if (glance == 0.0f) // 0x883A98..0x883AA6
	{
		return eye;
	}
	// r0' = c r0 + s r2, r2' = c r2 - s r0 (0x883AB8..0x883B1A): c stored as a float ([ebp-4] 0x883AB5; eye 1 0x883DF6),
	// s kept on the FPU stack
	const auto c = static_cast<float>(std::cos(static_cast<double>(glance)));
	const double s = std::sin(static_cast<double>(glance));
	return TurnedRows(eye, [c, s](glm::mat3& rows) { lh_matrix::RotateY(rows, c, s); });
}

glm::mat4 Lid(const glm::mat4& eye, float closure, bool upper)
{
	const float angle = closure * (upper ? k_UpperLid : k_LowerLid);
	// r1' = c r1 - s r2, r2' = c r2 + s r1 (0x883ED5..0x883F42; lh_matrix::RotateX): c stored as a float ([esp+0x18]
	// 0x883ECF upper, [esp+0x1C] 0x883F72 lower), s kept on the FPU stack
	const auto c = static_cast<float>(std::cos(static_cast<double>(angle)));
	const double s = std::sin(static_cast<double>(angle));
	return TurnedRows(eye, [c, s](glm::mat3& rows) { lh_matrix::RotateX(rows, c, s); });
}

int ShadeIntensity(int type, int eye, const glm::mat3& eyeRows)
{
	const auto& angles = k_ShadeAngles.at(static_cast<size_t>(type));
	const float a = angles.at(static_cast<size_t>(eye * 2));
	const float b = angles.at(static_cast<size_t>(eye * 2 + 1));
	// One float operation per statement: the FPU runs at 24 bits (each fadd / fmul rounded to a float), but fsin / fcos
	// give extended values, so it matters which of them are stored. cos a, -sin a and cos b are stored as floats
	// (0x88386F, 0x88387C, 0x883889; eye 1 0x883C1B, 0x883C28, 0x883C35); sin b is rounded by `0 + sin b` (fadd,
	// 0x8838C9 / 0x883C71) before its store
	const auto ca = static_cast<float>(std::cos(static_cast<double>(a)));
	const auto minusSa = static_cast<float>(-std::sin(static_cast<double>(a)));
	const auto cb = static_cast<float>(std::cos(static_cast<double>(b)));
	const auto sb = static_cast<float>(std::sin(static_cast<double>(b)));
	const float x = -(minusSa * cb); // x -1 (0x8838EE)
	const float y = -sb;             // 0x8838FA
	const float z = -(ca * cb);      // 0x883903
	// n M with the LHMatrix rows r0, r1, r2 (glm's columns), in the exe's sum orders: n0 = (y r1.x + z r2.x) + x r0.x and
	// n1 the same (0x88390F..0x883950; eye 1 0x883CB9..0x883CE4); n2 = (x r0.z + z r2.z) + y r1.z for eye 0
	// (0x883950..0x88396C) but (z r2.z + y r1.z) + x r0.z for eye 1 (0x883CE7..0x883CF8)
	const glm::vec3 r0 = eyeRows[0];
	const glm::vec3 r1 = eyeRows[1];
	const glm::vec3 r2 = eyeRows[2];
	const auto sum3 = [](float first, float second, float third) {
		const float pair = first + second;
		return pair + third;
	};
	glm::vec3 n;
	n.x = sum3(y * r1.x, z * r2.x, x * r0.x);
	n.y = sum3(y * r1.y, z * r2.y, x * r0.y);
	n.z = eye == 0 ? sum3(x * r0.z, z * r2.z, y * r1.z) : sum3(z * r2.z, y * r1.z, x * r0.z);
	// |n|^2: (ny^2 + nz^2) + nx^2 for eye 0 (0x88396F..0x883982), (nx^2 + ny^2) + nz^2 for eye 1 (0x883CFB..0x883D0F);
	// InverseSquareRoot 0x841170 (0x88398B / 0x883D18), then each component times it, stored
	const float length2 = eye == 0 ? sum3(n.y * n.y, n.z * n.z, n.x * n.x) : sum3(n.x * n.x, n.y * n.y, n.z * n.z);
	const float inverse = lh_matrix::InverseSquareRoot(length2);
	n = glm::vec3(n.x * inverse, n.y * inverse, n.z * inverse);
	// the default sun [0xEA1C88] normalised: (y^2 + z^2) + x^2 (0x88381B..0x883838), InverseSquareRoot 0x883841, each
	// component times it, stored (0x883846..0x883860)
	const glm::vec3 sun0 = model_light::k_DefaultSun;
	const float sunInverse = lh_matrix::InverseSquareRoot(sum3(sun0.y * sun0.y, sun0.z * sun0.z, sun0.x * sun0.x));
	const glm::vec3 sun(sun0.x * sunInverse, sun0.y * sunInverse, sun0.z * sunInverse);
	// (nz sz + ny sy) + nx sx (0x8839A9..0x8839BD / 0x883D36..0x883D4A), x 255 stored, fistp
	const float dot = sum3(n.z * sun.z, n.y * sun.y, n.x * sun.x);
	return model_light::Intensity(dot);
}

float IrisU(int type)
{
	return static_cast<float>(k_IrisCell.at(static_cast<size_t>(type))) * 0.125f; // fild, fmul [0x8AB620]
}

std::string File(int type, int slot)
{
	return std::string(k_Files.at(static_cast<size_t>(slot))) + (type == 2 ? "2" : "");
}

bool l3d_patch::FirstSkinId(const std::vector<uint8_t>& file, uint32_t& skinId)
{
	const auto headers = PrimitiveHeaders(file);
	return !headers.empty() && ReadU32(file, headers.front() + 8, skinId);
}

bool l3d_patch::SetFirstSkinId(std::vector<uint8_t>& file, uint32_t skinId)
{
	const auto headers = PrimitiveHeaders(file);
	if (headers.empty())
	{
		return false;
	}
	WriteU32(file, headers.front() + 8, skinId);
	return true;
}

bool l3d_patch::DoubleUvs(std::vector<uint8_t>& file)
{
	const auto headers = PrimitiveHeaders(file);
	for (const auto header : headers)
	{
		uint32_t count = 0;
		uint32_t vertices = 0;
		if (!ReadU32(file, header + 16, count) || !ReadU32(file, header + 20, vertices) ||
		    static_cast<size_t>(vertices) + static_cast<size_t>(count) * 32 > file.size())
		{
			return false;
		}
		for (uint32_t i = 0; i < count; ++i)
		{
			for (size_t k = 0; k < 2; ++k)
			{
				const size_t at = vertices + i * 32 + 12 + k * 4;
				float uv = 0.0f;
				std::memcpy(&uv, file.data() + at, 4);
				uv = uv + uv;
				std::memcpy(file.data() + at, &uv, 4);
			}
		}
	}
	return !headers.empty();
}

std::vector<uint8_t> l3d_patch::FindSkin(const std::vector<uint8_t>& file, uint32_t skinId)
{
	uint32_t count = 0;
	uint32_t offsets = 0;
	if (!ReadU32(file, 14 * 4, count) || !ReadU32(file, 15 * 4, offsets))
	{
		return {};
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		uint32_t skin = 0;
		uint32_t id = 0;
		if (!ReadU32(file, offsets + 4 * i, skin) || !ReadU32(file, skin, id) || skin + k_SkinBytes > file.size())
		{
			return {};
		}
		if (id == skinId)
		{
			return {file.begin() + skin, file.begin() + skin + k_SkinBytes};
		}
	}
	return {};
}

bool l3d_patch::AppendSkin(std::vector<uint8_t>& file, std::span<const uint8_t> skin)
{
	if (file.size() < 19 * 4 || skin.size() != k_SkinBytes)
	{
		return false;
	}
	const auto table = static_cast<uint32_t>(file.size());
	file.resize(file.size() + 4);
	WriteU32(file, table, table + 4);
	file.insert(file.end(), skin.begin(), skin.end());
	WriteU32(file, 14 * 4, 1);
	WriteU32(file, 15 * 4, table);
	return true;
}

Eyes Create(entt::entity host, int32_t type, const HostModel& model)
{
	std::array<entt::id_type, 5> meshes {};
	for (size_t slot = 0; slot < meshes.size(); ++slot)
	{
		meshes.at(slot) = LoadEyeMesh(type, static_cast<int>(slot), model);
	}
	return Create(host, type, model, meshes);
}

Eyes Create(entt::entity host, int32_t type, const HostModel& model, const std::array<entt::id_type, 5>& meshes)
{
	auto& registry = Locator::entitiesRegistry::value();
	Eyes eyes;
	eyes.type = type;
	eyes.matrices = model.matrices;
	eyes.bones = model.bones;
	// 0x8843E3..0x884553, slots 0..4: LH3DObject::Create(0), vt+0x98(0) and vt+0x58(0) (obj+4 bits 8 and 0x20 off;
	// meaning pending), the mesh, and for the eyeball SetAnimatedUV(k / 8, 0.75); for the lids SetAnimatedUV of
	// fn_00884000 zeroed by fn_008840E0 for these types, so none
	for (size_t i = 0; i < eyes.objects.size(); ++i)
	{
		const int slot = i >= k_EyeballRight ? 4 : static_cast<int>(i);
		const auto mesh = meshes.at(static_cast<size_t>(slot));
		const auto object = registry.Create();
		registry.Assign<SuperVillagerEye>(object, host, static_cast<uint8_t>(i), mesh);
		registry.Assign<Transform>(object, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		if (slot == 4)
		{
			registry.Assign<UvScroll>(object, UvScroll {.v = 0.75f, .u = IrisU(type)});
		}
		eyes.objects.at(i) = object;
	}
	return eyes;
}

void Destroy(Objects& objects)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		objects.fill(entt::null);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& object : objects)
	{
		if (object != entt::null && registry.Valid(object))
		{
			registry.Destroy(object);
		}
		object = entt::null;
	}
}

void Update(Eyes& eyes, Shared& shared, entt::entity host, int32_t milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	Step(eyes.timers, shared, milliseconds, game_random::crt::Random);
	const auto* animation = registry.TryGet<const SkeletalAnimation>(host);
	// the SuperVillager bone buffer [0xC37D9C] (0x883560 reads it after the body): the cross-faded pose while the fade
	// is drawn (ecs::DrawnPose)
	const auto* bones = animation != nullptr ? &DrawnPose(*animation) : nullptr;
	const auto fits = [&](int32_t bone) {
		return bone >= 0 && static_cast<size_t>(bone) < bones->size();
	};
	if (!registry.AllOf<Mesh, Transform>(host) || animation == nullptr || !fits(eyes.bones[0]) || !fits(eyes.bones[1]))
	{
		Hide(eyes);
		return;
	}
	// ECS/MobileDrawing.h, as RenderingSystem draws the body: the root of [0xC37D9C] is fn_00825530's turned local copy
	// (0x8257CD..0x8259AC)
	const auto model = DrawnBodyModel(registry, host);
	// fn_00825530 0x82555D: fn_00801C90 at the drawn position into the host's +0x4C / +0x50 (both written on every
	// path, 0x8020E4 / 0x802107..0x802109); fn_00825530 calls no fn_007FEB30, so no haze in them (the haze Villager::Draw
	// 0x5E4BC2 put there is overwritten)
	const auto light = land_light::At(LandLightTable::Current(), glm::vec2(model[3].x, model[3].z));
	const int ambient = model_light::Ambient(); // [0xC39264]
	for (size_t k = 0; k < 2; ++k)
	{
		const auto bone = static_cast<size_t>(eyes.bones.at(k));
		const auto eye = EyeMatrix(model * (*bones)[bone], eyes.matrices.at(k));
		// 0x8839D0..0x883A3B: the host's colour x this eye's factor, alpha kept; the host's specular to all five
		const auto colour =
		    model_light::Apply(light.diffuse, ShadeIntensity(eyes.type, static_cast<int>(k), glm::mat3(eye)), ambient);
		Place(registry, eyes.objects.at(k == 0 ? k_EyeballRight : k_EyeballLeft), Glance(eye, eyes.timers.glance), colour,
		      light.specular);
		// fn_00883EA0(k): the upper lid (slot 2k) and the lower one (2k + 1)
		Place(registry, eyes.objects.at(2 * k), Lid(eye, eyes.timers.closure, true), colour, light.specular);
		Place(registry, eyes.objects.at(2 * k + 1), Lid(eye, eyes.timers.closure, false), colour, light.specular);
	}
	registry.SetDirty();
}

} // namespace openblack::ecs::super_villager::eyes
