/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeedGraphic.h"

#include <cmath>

#include <optional>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "PSys/Rules/SurfRevol.h"
#include "PSys/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// CreatePUBand 0x727080: GJUtils::GetSharedMesh(".\data\spells\meshes\Power_Up_Band.L3d") (loaded here on first use:
/// Game.cpp loads only O_Bibble_up of that folder)
constexpr auto k_BandMesh = entt::hashed_string("Power_Up_Band");

constexpr float k_TwoPi = 6.2831854820251465f; // the fmod divisor, double [0x8D45D8]
constexpr float k_SpinRate = 2.0f;             // [0x8D8700], rad/s of +0x3C (DrawSpellGraphic 0x519B20)
constexpr float k_BandSpinRate = 10.3f;        // [0xBE8E94], rad/s of +0x44 (0x51A2EA)
constexpr float k_BandSpin2Rate = 1.0f;        // [0xBE8E90], rad/s of +0x40 (0x51A305)
constexpr float k_BandScale = 0.2f;            // [0xBE8E9C] x +0x58 x +0x54 (0x51A70C)
/// +0x70, the bands' alpha before the caller's (fn_00726F10 0x726F4E, the only writer; read at 0x51A399)
constexpr uint32_t k_BandAlpha = 0x3C;
/// Each band level is drawn twice with the same matrix and colour (0x51A780 vt+0x104, then 0x51A7A3 vt+0x104 or, for
/// the last level when arg 1 is 0, 0x51A796 vt+0x100): both are draws of LH3DStaticObject (vtable 0x9A2974: +0x104
/// fn_00815980 = on-screen and distance test, then vt+0x108; +0x100 fn_00815A70 = the same draw that also records
/// g_last_distance / g_last_selected_box for the picking). Additive, so every band adds its light twice.
constexpr size_t k_DrawsPerBand = 2;

float g_Phase = 0.0f;

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
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "Meshes" /
		                                "Power_Up_Band.L3d"));
		// CreatePUBand 0x727097..0x7270B8 (and PHandFX fn_0068CC70 0x68CC7D..0x68CC99, the same mesh): GJUtils::GetSharedMesh
		// with MaterialProperties {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so SetMaterialProperties 0x57E120
		// turns the band into mode 13 (SRCALPHA / ONE, no Z write, fn_0082ECD0)
		meshes.Handle(k_BandMesh)->SetMaterialProperties(
		    {.additive = true, .zWrite = false, .doubleSided = true, .change = true, .alpha = true});
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Worship: cannot load Power_Up_Band.L3d: {}", e.what());
		return false;
	}
	return true;
}

entt::entity NewBand(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto band = registry.Create();
	registry.Assign<Transform>(band, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(band, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<Alpha>(band, 0.0f);
	registry.Assign<ObjectColour>(band);
	return band;
}

/// SetColour 0x51A3BE (fn_007F9770): +0x4C = GetPlayerColour 0x64D800 of the owner (vt 0x1C), or of the local player
/// (g_game +0x205A59) when the owner is the neutral one (g_game +0x205A5B, 0x51A322..0x51A36D), its rgb with the alpha
/// (+0x70 x the caller's alpha) >> 8 (0x51A397..0x51A3B9); +0x50 (the specular) = 0x141414, the byte [0xBE8EA0] = 20
/// in r, g and b (0x51A370..0x51A38D; refs.py: nothing writes the byte). (inferido) openblack's local player is
/// PLAYER_ONE.
void SetBandColour(entt::entity band, PlayerNames owner, uint8_t alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (band == entt::null || !registry.Valid(band))
	{
		return;
	}
	const auto player = owner == PlayerNames::NEUTRAL ? PlayerNames::PLAYER_ONE : owner;
	const uint32_t rgb = psys::surf_revol::PlayerColour(static_cast<int>(player)); // 0xBFF0B8 (identity remap)
	constexpr uint32_t k_BandSpecular = 0x141414u; // [0xBE8EA0] = 20 in each channel
	registry.AssignOrReplace<ObjectColour>(
	    band, ObjectColour {{static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)},
	                        k_BandSpecular});
	const uint32_t a = (k_BandAlpha * alpha) >> 8; // imul, and 0xFFFFFF00, shl 0x10: the byte above bit 8
	registry.AssignOrReplace<Alpha>(band, static_cast<float>(a & 0xFF) / 255.0f);
}

/// CreatePUBand 0x727080 (one Game3DObject, +0x30); the loop 0x51A3D4 draws it pu + 1 times, each twice
/// (k_DrawsPerBand; openblack: one entity per drawing, extraBands the 2 (pu + 1) - 1 after the first)
void UpdateBands(entt::entity graphicEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& graphic = registry.Get<SpellSeedGraphic>(graphicEntity);
	if (graphic.powerUp != -1 && graphic.band == entt::null && LoadBandMesh())
	{
		graphic.band = NewBand(graphic.point);
	}
	// DrawSpellGraphic 0x51A2D0: no band drawn while +0x60 is -1 (the band object stays: SetPowerUpType 0x727060)
	const size_t extra = graphic.band == entt::null || graphic.powerUp < 0
	                         ? 0
	                         : (static_cast<size_t>(graphic.powerUp) + 1) * k_DrawsPerBand - 1;
	while (graphic.extraBands.size() > extra)
	{
		registry.Destroy(graphic.extraBands.back());
		graphic.extraBands.pop_back();
	}
	while (graphic.extraBands.size() < extra)
	{
		graphic.extraBands.push_back(NewBand(graphic.point));
	}
	if (graphic.band != entt::null)
	{
		const bool shown = graphic.powerUp != -1;
		if (shown && !registry.AllOf<Mesh>(graphic.band))
		{
			registry.Assign<Mesh>(graphic.band, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
		}
		else if (!shown && registry.AllOf<Mesh>(graphic.band))
		{
			registry.Remove<Mesh>(graphic.band);
		}
	}
	registry.SetDirty();
}

/// SpellSeed::ReplaceMeshGivenSeedType 0x728450 (table 0x72854C by seed - 3): FLYING_FLOCK (10) takes mesh 1
/// (AnimalBat1) when the player's alignment (GPlayer +0x60 -> +8; 0 with no player) is below the flock's
/// alignmentSwitch (fn_00723140, GMagicFlockFlyingInfo +0x5C), else 11 (AnimalSpellDove). FOOD (3) and the creature
/// phials (12..27) get the environment map 0 (fn_0080B440(0): .\data\textures\envmap.raw) and BEAM_EXPLOSION (29)
/// material properties {1, 0, 1, 1, 0} with vt 0x40(1): neither is ported (openblack has no per-object envmap).
std::optional<MeshId> ReplacedMesh(SpellSeedType seed, PlayerNames player)
{
	if (seed != SpellSeedType::FlockFlying)
	{
		return std::nullopt;
	}
	const auto* flying = magic::GetMagicInfoAs<GMagicFlockFlyingInfo>(Locator::infoConstants::value(), MagicType::FlockFlying);
	if (flying == nullptr)
	{
		return std::nullopt;
	}
	const float alignment = player == PlayerNames::NEUTRAL ? 0.0f : ecs::effects::alignment::Get(player);
	return alignment < flying->alignmentSwitch ? MeshId::AnimalBat1 : MeshId::AnimalSpellDove;
}

/// fn_007270E0: +0x64 = the point, the mesh (+0x14) at + unknown0x150 x scale, the effect at + unknown0x154 x scale
void SetPositions(SpellSeedGraphic& graphic, const glm::vec3& point)
{
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), graphic.seedType);
	graphic.point = point;
	graphic.meshPosition = point + glm::vec3(0.0f, info.unknown0x150 * graphic.scale, 0.0f);
	graphic.effectPosition = point + glm::vec3(0.0f, info.unknown0x154 * graphic.scale, 0.0f);
}

/// PSys vt 0xFC Process_ with the zeroed PSysProcessInfo of fn_007274D0 / UpdateOnly 0x727590 (power +0x30 = 1,
/// enabled +0x38 = 1) and g_game_time_inc
void StepEffect(uint32_t id, float milliseconds)
{
	if (id == 0 || psys::manager::Find(id) == nullptr)
	{
		return;
	}
	psys::ProcessInfo info;
	info.power = 1.0f;
	info.enabled = true;
	psys::manager::ProcessForSpell(id, info, milliseconds * 0.001f);
}

/// The band's matrix, DrawSpellGraphic 0x51A3EE..0x51A70C, rows of the LHMatrix (world = sum local_i row_i): identity,
/// rows 1 and 2 swapped with the old row 1 negated, then turned in (x, z) by base + the band angle (+0x44), in (x, y)
/// by 0.3 [0xBE8E98], in (x, z) by k and in (x, y) by 0.2 [0xBE8EAC]; base, k = 0, -1 for the first band, 0.5 [0xBE8EA8],
/// 1 [0xBE8EA4] for the others
glm::mat3 BandRotation(float bandAngle, size_t index)
{
	glm::vec3 rows[3] = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
	const auto turnXZ = [&rows](float angle) {
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		for (auto& row : rows)
		{
			const float x = row.x;
			row.x = c * x - s * row.z;
			row.z = c * row.z + s * x;
		}
	};
	const auto turnXY = [&rows](float angle) {
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		for (auto& row : rows)
		{
			const float x = row.x;
			row.x = c * x + s * row.y;
			row.y = c * row.y - s * x;
		}
	};
	const float base = index == 0 ? 0.0f : 0.5f;
	const float k = index == 0 ? -1.0f : 1.0f;
	turnXZ(base + bandAngle);
	turnXY(0.3f);
	turnXZ(k);
	turnXY(0.2f);
	return {rows[0], rows[1], rows[2]};
}
} // namespace

entt::entity seed_graphic::Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale,
                                  int powerUp)
{
	const auto index = static_cast<int>(seed);
	if (index < 0 || index >= static_cast<int>(magic::k_SpellSeedCount))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	// fn_00726E70 (ctor, not an Object: no creation index; fn_00726F10 zeroes the angles, +0x58 = +0x54 = 1), then
	// fn_00727190
	const auto entity = registry.Create();
	auto& graphic = registry.Assign<SpellSeedGraphic>(entity);
	graphic.seedType = seed;
	graphic.player = player;
	graphic.scale = scale;
	graphic.powerUp = powerUp;
	SetPositions(graphic, worldPosition);
	// Game3DObject::Create(pos, Get3DType(mesh), mesh +0x130, 0, 1), then the matrix = scale (+0x54) and the position;
	// DrawSpellGraphic 0x519B13 draws it at GSpellSeedInfo.scale (+0x134) x +0x54, and only when useMesh (+0x168,
	// fn_00727690) is set: STORM, FIRE, LIGHTNING_BOLT, WATER and TELEPORT have 0, only their holder effect shows
	const auto mesh = ReplacedMesh(seed, player).value_or(info.mesh);
	registry.Assign<Transform>(entity, graphic.meshPosition, glm::mat3(1.0f), glm::vec3(info.scale * scale));
	if (info.useMesh != 0)
	{
		registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	// fn_007276E0: the holder particle type (+0x164); PSysInterface::Create(NULL, pt, point + unknown0x154 x scale, 0,
	// scale, NET 0), AddTarget(this), SetPlayer. The graphic owns it and steps it itself (DrawUpdateAtPos / UpdateOnly
	// every drawn frame, fn_007273A0 every turn when auto-updated), drawn as last stepped (time multiplier 1, 0x51A27E)
	if (info.holderParticle != ParticleType::None)
	{
		const auto file = psys::ParticleTypeFile(info.holderParticle);
		if (!file.empty())
		{
			auto& component = registry.Get<SpellSeedGraphic>(entity);
			component.psys = psys::manager::StartForSpell(std::string(file), component.effectPosition, glm::vec3(0.0f),
			                                              scale, nullptr);
			if (component.psys != 0)
			{
				psys::manager::SetPerFrame(component.psys);
			}
		}
	}
	if (powerUp != -1)
	{
		UpdateBands(entity); // CreatePUBand 0x727080
	}
	return entity;
}

void seed_graphic::Delete(entt::entity graphic)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	if (auto* component = registry.TryGet<SpellSeedGraphic>(graphic); component != nullptr)
	{
		if (component->psys != 0)
		{
			psys::manager::Delete(component->psys);
		}
		if (component->band != entt::null && registry.Valid(component->band))
		{
			registry.Destroy(component->band);
		}
		for (const auto band : component->extraBands)
		{
			if (registry.Valid(band))
			{
				registry.Destroy(band);
			}
		}
	}
	registry.Destroy(graphic);
	registry.SetDirty();
}

void seed_graphic::SetPowerUpType(entt::entity graphic, int powerUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	registry.Get<SpellSeedGraphic>(graphic).powerUp = powerUp;
	UpdateBands(graphic);
}

void seed_graphic::SetAutoUpdate(entt::entity graphic, bool autoUpdate)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic != entt::null && registry.Valid(graphic))
	{
		registry.Get<SpellSeedGraphic>(graphic).autoUpdate = autoUpdate;
	}
}

void seed_graphic::SetBandScale(entt::entity graphic, float bandScale)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic != entt::null && registry.Valid(graphic))
	{
		registry.Get<SpellSeedGraphic>(graphic).bandScale = bandScale;
	}
}

void seed_graphic::DrawUpdateAtPos(entt::entity graphic, const glm::vec3& point, float scale, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic) || !registry.AllOf<SpellSeedGraphic>(graphic))
	{
		return;
	}
	auto& component = registry.Get<SpellSeedGraphic>(graphic);
	// 0x72763A: +0x54 = scale; MapCoords(matrix translation) -> fn_007270E0
	component.scale = scale;
	SetPositions(component, point);
	// fn_007274D0: the PSys at the effect point (vt 0x124), magnitude = scale (vt 0x11C), Process_(info, ms) (vt 0xFC)
	if (auto* effect = component.psys != 0 ? psys::manager::Find(component.psys) : nullptr; effect != nullptr)
	{
		effect->SetOrigin(component.effectPosition);
		effect->SetMagnitude(scale);
		StepEffect(component.psys, milliseconds);
	}
}

void seed_graphic::UpdateOnly(entt::entity graphic, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic) || !registry.AllOf<SpellSeedGraphic>(graphic))
	{
		return;
	}
	StepEffect(registry.Get<const SpellSeedGraphic>(graphic).psys, milliseconds); // 0x727590: Process_(info, ms)
}

void seed_graphic::DrawSpellGraphic(entt::entity graphicEntity, uint8_t alpha, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphicEntity == entt::null || !registry.Valid(graphicEntity) ||
	    !registry.AllOf<SpellSeedGraphic, Transform>(graphicEntity))
	{
		return;
	}
	auto& graphic = registry.Get<SpellSeedGraphic>(graphicEntity);
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), graphic.seedType);
	const float seconds = milliseconds * 0.001f; // g_game_time_inc x 0.001 [0x8AA3B0]

	// 0x519B04: only with useMesh (fn_00727690 = +0x168). Size = GSpellSeedInfo.scale (+0x134) x +0x54; the y angle
	// +0x3C += 2 x dt, fmod 2 pi (negative + 2 pi)
	// (0x519B0D jumps past the whole mesh part, the angle too, when useMesh is 0)
	if (info.useMesh != 0)
	{
		graphic.spin = std::fmod(graphic.spin + k_SpinRate * seconds, k_TwoPi);
		if (graphic.spin < 0.0f)
		{
			graphic.spin += k_TwoPi;
		}
		// A creature spell phial (GMagicInfo::AsMagicCreatureSpellInfo (vt 0x38) of the seed's magic is not NULL:
		// 0x519B5C..0x519B73): SetAnimatedUV_2(1) (0x519B83), then its 8 x 4 sheet at -15 frames a second
		// (0x519B89..0x519C1B, frame_anim::SpellIconFrame) through the object's UV offset. The magic is magicTypes[0]:
		// fn_00727700 gives the GSpellSeedInfo (0xD9D678 + type x 0x190, the table load_variables fills, 0x42C28E),
		// fn_0072AF50(0) the GMagicInfo [0xD37D10 + 4 x fn_0072AF10(0)], and fn_0072AF10(0) reads +0x124 (0x72AF1C),
		// the field GetMagicTypeFromPULevel(-1) reads (0x72AFC9): magicTypes[0], file offset 0x114
		const auto magicType = info.magicTypes[0];
		if (static_cast<size_t>(magicType) < magic::k_MagicTypeCount &&
		    magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(Locator::infoConstants::value(), magicType) != nullptr)
		{
			const auto uv = graphics::frame_anim::SpellIconFrame(graphic.uvPhase, seconds);
			auto* scroll = registry.TryGet<UvScroll>(graphicEntity);
			if (scroll == nullptr)
			{
				scroll = &registry.Assign<UvScroll>(graphicEntity);
			}
			scroll->u = uv.x;
			scroll->v = uv.y;
		}
	}
	// A player seed (GMagicInfo::AsMagicCreatureSpellInfo (vt 0x38) of its base magic is NULL: 0x519B73 -> 0x51A0B3):
	// diffuse alpha = the owner's alpha, SetGlobalAlpha (vt 0x48)(alpha != 0xFF), then with arg 2 = 0 (every caller in
	// the world) LH3DIsland::GetAltitudeAndSetColorSpecular (the land's light on it, not ported) and
	// LH3DObject::SetPosition 0x423140(point, +0x3C, size): rows X = (cos, 0, sin), Z = (-sin, 0, cos), and
	// AddForDrawing. No bob and no pulse: those (+0x38 and the 0.7 / 0.8 / 1.5 squashes of the switch 0x519D76 by
	// GMagicCreatureSpellInfo +0x58) are only for the creature spell phials (12..27); of that branch only the UV frames
	// (+0x34, above) are ported.
	auto& transform = registry.Get<Transform>(graphicEntity);
	const float c = std::cos(graphic.spin);
	const float s = std::sin(graphic.spin);
	transform.position = graphic.meshPosition;
	transform.rotation = glm::mat3(glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c));
	transform.scale = glm::vec3(info.scale * graphic.scale);
	if (alpha != 0xFF)
	{
		registry.AssignOrReplace<Alpha>(graphicEntity, static_cast<float>(alpha) / 255.0f);
	}
	else if (registry.AllOf<Alpha>(graphicEntity))
	{
		registry.Remove<Alpha>(graphicEntity);
	}
	// 0x51A24B: the holder PSys gets the alpha (vt 0x12C, not ported) and is drawn

	// 0x51A2D0: the bands when +0x60 != -1 and the band object exists: +0x44 += 10.3 dt, +0x40 += dt (fmod 2 pi), one
	// drawing per level 0..pu at +0x64 with size 0.2 x +0x58 x +0x54, turned to the camera by fn_0051A830
	// (billboard::BandToEye), each drawn twice (k_DrawsPerBand) in the owner's colour (SetBandColour, 0x51A3BE)
	if (graphic.powerUp != -1 && graphic.band != entt::null)
	{
		// the angles only move inside this branch (0x51A2D0 / 0x51A2E1 jump past 0x51A2EA..0x51A318)
		graphic.bandSpin = std::fmod(graphic.bandSpin + k_BandSpinRate * seconds, k_TwoPi);
		graphic.bandSpin2 = std::fmod(graphic.bandSpin2 + k_BandSpin2Rate * seconds, k_TwoPi);
		const float size = k_BandScale * graphic.bandScale * graphic.scale;
		// fn_0051A830 0x51A848..0x51A8BE: the camera of this frame (g_camera); the band's translation is the point
		const auto toEye = Locator::camera::has_value()
		                       ? graphics::billboard::BandToEye(graphic.point, Locator::camera::value().GetOrigin())
		                       : glm::mat3(1.0f);
		for (size_t i = 0; i <= graphic.extraBands.size(); ++i)
		{
			const auto band = i == 0 ? graphic.band : graphic.extraBands[i - 1];
			if (registry.Valid(band))
			{
				auto& bandTransform = registry.Get<Transform>(band);
				bandTransform.position = graphic.point;
				bandTransform.rotation = toEye * BandRotation(graphic.bandSpin, i / k_DrawsPerBand);
				bandTransform.scale = glm::vec3(size);
				SetBandColour(band, graphic.player, alpha);
			}
		}
	}
	registry.SetDirty();
}

void seed_graphic::UpdateIconGraphics(float milliseconds)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> graphics;
	registry.Each<const SpellIcon>([&graphics](const SpellIcon& icon) {
		if (icon.graphic != entt::null)
		{
			graphics.push_back(icon.graphic);
		}
	});
	// SpellIcon::Draw 0x5198D2 / TownCentre::Draw 0x5164D4 -> SpellIcon::DrawSpellSeedGraphic 0x726D30: UpdateOnly(ms)
	// then DrawSpellGraphic(icon, 0, 1, the icon's alpha); both icons are drawn with diffuse 0xFFFFFFFF (fn_0080BEC0,
	// edx = -1), so 0xFF. (inferido): every frame, the original only when the icon was on screen
	for (const auto graphic : graphics)
	{
		UpdateOnly(graphic, milliseconds);
		DrawSpellGraphic(graphic, 0xFF, milliseconds);
	}
}

void seed_graphic::ProcessTurn()
{
	const auto turnMilliseconds = static_cast<float>(magic::k_TurnMs); // the turn length *(u32*)0xD01A38
	// fn_00727350 -> fn_007273A0: the auto-updated graphics step their holder PSys with the zeroed info (vt 0x100);
	// fn_00727440 every 30 turns (g_game +0x205A40 % 0x1E) redoes the FLYING_FLOCK mesh for the player's alignment
	auto& registry = Locator::entitiesRegistry::value();
	// g_game +0x205A40, the game's turn (fn_00727350 0x72736E)
	const uint32_t turn = game_clock::Turn();
	const bool refreshMesh = (turn % 0x1E) == 0;
	registry.Each<SpellSeedGraphic>([&registry, turnMilliseconds, refreshMesh](entt::entity entity, SpellSeedGraphic& graphic) {
		if (graphic.autoUpdate)
		{
			StepEffect(graphic.psys, turnMilliseconds);
		}
		if (refreshMesh && registry.AllOf<Mesh>(entity))
		{
			if (const auto mesh = ReplacedMesh(graphic.seedType, graphic.player); mesh.has_value())
			{
				registry.Get<Mesh>(entity).id = resources::HashIdentifier(*mesh);
			}
		}
	});
}

void seed_graphic::UpdatePhase(float milliseconds)
{
	g_Phase += milliseconds * 0.001f / 3.33f;
	while (g_Phase > 1.0f)
	{
		g_Phase -= 1.0f;
	}
}

float seed_graphic::Phase()
{
	return g_Phase;
}
