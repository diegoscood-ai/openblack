/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHighlight.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <string>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "GameClock.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "Resources/ResourceManager.h"

namespace openblack::ecs::script_highlight
{
using components::Mesh;
using components::Transform;
using HighlightComponent = components::ScriptHighlight;

namespace
{
/// [0x942AF4] = 0.00314159: the turning highlight's radians per game ms (pi / 1000)
constexpr float k_SpinPerMs = 0.00314159f;
/// [0x8C6CAC] = 0.159155 (1 / 2 pi) and [0x8AB210] = 6.28319
constexpr float k_InvTwoPi = 0.159155f;
constexpr float k_TwoPi = 6.28319f;
/// [0x942244] = 5 and [0x8AA3B0] = 0.001: the pulse's radians per second of turn
constexpr float k_PulsePerSecond = 5.0f;
/// Draw: the distance bounds 0xA / 0x1E and [0x8CF3F8] = 0.0333333
constexpr uint32_t k_NearDistance = 10;
constexpr uint32_t k_FarDistance = 30;
constexpr float k_InvFarDistance = 0.0333333f;
/// Draw 0x70A36C..0x70A3D3: the sprite stands 1.4 [0x8C7E18] radii towards the eye, 2 [0x94222C] radii wide (at least
/// 0.0001 [0x8BF518]); the click sphere is r + 0.5 [0x942230]
constexpr float k_SpriteTowardsEye = 1.4f;
constexpr float k_SpriteSizePerRadius = 2.0f;
constexpr float k_MinSpriteSize = 0.0001f;
constexpr float k_ClickMargin = 0.5f;
/// Draw 0x70A08B..0x70A0B6: the glow's Y row x 3 [0x8C2C50], its +0x44 = 3.0
constexpr float k_GlowStretch = 3.0f;
/// fn_0078CC10: 48 entries a category (`cmp edx, 0x30`)
constexpr uint32_t k_DykListSize = 0x30;
constexpr size_t k_DykCategories = 5;

Pulse g_Pulse;
/// g_game+0x205C94 / +0x205C98: the head of the list (next at +0x88) and its count
std::vector<entt::entity> g_List;
std::array<std::vector<uint32_t>, k_DykCategories> g_DykRead;

ecs::Registry& Reg()
{
	return Locator::entitiesRegistry::value();
}

const GScriptHighlightInfo* InfoRow(uint32_t index)
{
	if (!Locator::infoConstants::has_value() || index >= k_InfoCount)
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().scriptHighlight.at(index);
}

HighlightComponent* Get(entt::entity thing)
{
	auto& registry = Reg();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<HighlightComponent>(thing) : nullptr;
}

float GroundAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

glm::vec3 WorldOf(const HighlightComponent& highlight)
{
	return {highlight.x, GroundAt(highlight.x, highlight.z) + highlight.altitude, highlight.z};
}

/// SingleMapFixed::GetMesh (vt +0x608, the info's +0x100 normal) or GetActiveMesh 0x70A750 (+0x104 active), as
/// SetActivated 0x70A656..0x70A6BE and CallVirtualFunctionsForCreation 0x709B8A..0x709BA4 choose; MeshPack[0] when out
/// of range (0x709BAC)
void SetMeshOf(entt::entity thing, const HighlightComponent& highlight)
{
	const auto* info = InfoRow(highlight.infoIndex);
	if (info == nullptr)
	{
		return;
	}
	const MeshId mesh = highlight.active ? info->active : info->normal;
	auto& registry = Reg();
	const auto id = resources::HashIdentifier(mesh);
	if (auto* current = registry.TryGet<Mesh>(thing))
	{
		current->id = id;
	}
	else
	{
		registry.Assign<Mesh>(thing, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	registry.SetDirty();
}

/// GJPSysInterface::Create(nullptr, type, GetLHPoint, (0, 0, 0), 1.0, 0) 0x68E910 (0x709BDA..0x709C0E, 0x70A6D2..
/// 0x70A707): 0 when the row has no particle type or the type no file
uint32_t StartEffect(ParticleType type, const glm::vec3& point)
{
	if (type == ParticleType::None)
	{
		return 0; // `test edi, edi` / `test ebx, ebx`: PARTICLE_TYPE 0, none
	}
	const auto file = psys::ParticleTypeFile(type);
	if (file.empty())
	{
		return 0;
	}
	return psys::manager::Start(std::string(file), point, 1.0f, game_random::psys::NetGameType::Local);
}

void DeleteEffect(uint32_t& id)
{
	if (id != 0)
	{
		psys::manager::Delete(id); // vt +4 (1): the scalar deleting destructor, at once
		id = 0;
	}
}

/// Process 0x70A580
void Process(entt::entity thing, HighlightComponent& highlight)
{
	// 0x70A583..0x70A59D: Pos.altitude = the draw height, or fn_006022C0(Pos, this, 1): the top of what it stands on
	if (highlight.hasDrawHeight)
	{
		highlight.altitude = highlight.drawHeight;
	}
	else
	{
		const auto coords = map_coords::FromMetres(glm::vec2(highlight.x, highlight.z));
		highlight.altitude =
		    map_cells::TallestOverlapping(coords, thing, coords, object::Get2DRadius(thing), true);
	}
	// 0x70A5A0..0x70A5E7: a did-you-know not lit yet, every 8 turns by (turn + its unique id) & 7 (fn_00436A80
	// UniqueKeyHeap::GetUniqueIdFromAddress), is lit once its text has been read. (approximate) the unique id is the
	// creation index (openblack has no object heap)
	if (!highlight.active && IsDidYouKnowInfo(highlight.infoIndex))
	{
		const auto unique = static_cast<uint32_t>(object_index::Of(thing));
		if (((game_clock::Turn() + unique) & 7u) == 0 && dyk_read::IsRead(highlight.scriptId, highlight.category))
		{
			SetActivated(thing, true);
		}
	}
}
} // namespace

// ---- Pure rules -----------------------------------------------------------------------------------------------------

uint8_t SpriteAlpha(uint32_t index)
{
	switch (index)
	{
	case 1:
		return 0x32;
	case 2:
		return 0x96;
	case 3:
		return 0x64;
	default:
		return 0;
	}
}

float DistanceScale(float scale, float distanceToCamera, bool didYouKnow)
{
	auto distance = static_cast<uint32_t>(static_cast<int64_t>(distanceToCamera)); // __ftol, then unsigned compares
	if (distance > k_FarDistance)
	{
		distance = k_FarDistance;
	}
	else if (distance < k_NearDistance)
	{
		distance = k_NearDistance;
	}
	if (didYouKnow)
	{
		return scale;
	}
	// fild qword (the distance, high dword 0) x scale x (1 / 30)
	return static_cast<float>(distance) * scale * k_InvFarDistance;
}

float SpinAngle(float angle, uint32_t frameMs)
{
	const float turned = angle + static_cast<float>(frameMs) * k_SpinPerMs;
	const auto turns = static_cast<int32_t>(turned * k_InvTwoPi); // __ftol
	return turned - static_cast<float>(turns) * k_TwoPi;
}

bool SavesGameWhenClicked(uint32_t scriptId)
{
	return scriptId == 0x38 || scriptId == 0x3B || scriptId == 0x3C || scriptId == 0x3D;
}

bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId)
{
	if (didYouKnow)
	{
		return scriptId != 0; // 0x70ADDC..0x70ADE3
	}
	return active && scriptId != 0; // 0x70ADEA..0x70ADF8
}

uint32_t OverwriteTapToolTip(ObjectType infoType)
{
	return static_cast<int>(infoType) == 1 ? 0xEF2u : 0u; // setne; dec; and 0xEF2
}

void StepPulse(Pulse& pulse, uint32_t msPerTurn)
{
	pulse.phase += static_cast<float>(msPerTurn) * k_PulsePerSecond * 0.001f;
	if (pulse.phase > k_TwoPi) // fcomp; test ah, 0x41; jne: only when above
	{
		pulse.phase -= k_TwoPi;
	}
	pulse.previous = pulse.value; // 0x70A4C4: [0xD967DC] = the old [0xD967D8]
	pulse.value = (1.0f - std::cos(pulse.phase)) * 0.5f;
}

float ActivePulse(const Pulse& pulse, float turnFraction)
{
	float t = (pulse.value - pulse.previous) * turnFraction + pulse.previous;
	if (!(t > 0.0f)) // test ah, 0x41 against 0: not above 0
	{
		t = 0.0f;
	}
	else if (!(t < 1.0f)) // test ah, 1 against 1: not below 1
	{
		t = 1.0f;
	}
	return t * 0.6f + 0.4f; // [0x942AF8], [0x8C7A44]
}

uint32_t ActiveGlowArgb(uint32_t index, float activePulse)
{
	if (index == static_cast<uint32_t>(Info::Silver))
	{
		return 0x14B4DCFFu;
	}
	const auto whole = static_cast<uint8_t>(static_cast<int32_t>(activePulse)); // __ftol, the low byte
	const auto alpha = static_cast<uint8_t>(whole * 0x50);                     // imul cl (8 bits)
	return (static_cast<uint32_t>(alpha) << 24) | 0xFFFF00u;
}

// ---- The read did-you-knows -----------------------------------------------------------------------------------------

bool dyk_read::IsRead(uint32_t text, DykCategory category)
{
	const auto c = static_cast<size_t>(category);
	if (c >= k_DykCategories) // `cmp eax, 4; ja`: 0
	{
		return false;
	}
	const auto& list = g_DykRead.at(c);
	return std::find(list.begin(), list.end(), text) != list.end();
}

void dyk_read::MarkRead(uint32_t text, DykCategory category)
{
	const auto c = static_cast<size_t>(category);
	if (c >= k_DykCategories || IsRead(text, category) || g_DykRead.at(c).size() >= k_DykListSize)
	{
		return; // (inferred) fn_0078CD20 is not read
	}
	g_DykRead.at(c).push_back(text);
}

uint32_t dyk_read::Total()
{
	uint32_t total = 0;
	for (const auto& list : g_DykRead)
	{
		total += static_cast<uint32_t>(list.size());
	}
	return total;
}

void dyk_read::Clear()
{
	for (auto& list : g_DykRead)
	{
		list.clear();
	}
}

// ---- The highlights -------------------------------------------------------------------------------------------------

entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale)
{
	const auto* info = InfoRow(infoIndex);
	if (infoIndex >= k_InfoCount)
	{
		return entt::null;
	}
	auto& registry = Reg();
	const auto thing = registry.Create();
	// Object::Object (through FixedObject 0x52DDC0): the next creation index
	object_index::Assign(thing);
	auto& highlight = registry.Assign<HighlightComponent>(thing);
	highlight.infoIndex = infoIndex;
	highlight.x = position.x;
	highlight.z = position.z;
	highlight.altitude = 0.0f; // MapCoords(LHPoint) 0x603160 of the script's point: y - ground (inferred: 0 for these)
	if (Locator::terrainSystem::has_value())
	{
		highlight.altitude = position.y - GroundAt(position.x, position.z);
	}
	highlight.scale = scale;
	highlight.drawAngle = yAngle;
	highlight.scriptId = scriptId; // ctor 0x7098FB
	g_List.insert(g_List.begin(), thing); // ctor 0x7098D6..0x7098F4: the head
	registry.Assign<Transform>(thing, WorldOf(highlight), lh_matrix::AngleY(yAngle), glm::vec3(scale));

	// CallVirtualFunctionsForCreation 0x709AA0: SingleMapFixed's (0x52E880, the 3D object and InsertMapObject 0x52E620:
	// the head of its cell's fixed list)
	SetMeshOf(thing, highlight);
	map_cells::InsertMapObject(thing);
	// (pending, renderer) 0x709AB7 fn_0080B440(0) on the 3D object; 0x709ACE vt +0x98(0) on it for a scroll; the sprite
	// +0x64 (0x709ADE) and the glow object +0x68 (0x709AF7: mesh k_GlowMesh, SetMaterialProperties {1, 0, 0, 1, 1}
	// 0x709B4B) are ExtrasOf's; 0x709B5A vt +0x80 on the 3D object (UseFootpathIfNecessary in its vtable, not read)
	// 0x709B60..0x709B85: a did-you-know already read is lit at once
	if (IsDidYouKnowInfo(infoIndex) && dyk_read::IsRead(highlight.scriptId, highlight.category))
	{
		SetActivated(thing, true);
	}
	// 0x709BC7: unless +0xA & 1 (inferred clear for a new object), the glints (info +0x108) at GetLHPoint, with this as
	// their target (vt +0x114 AddTarget_), then the +0x74 particle (fn_006CA7A0, a 0x1C-byte PSysRenderParticle)
	auto& made = registry.Get<HighlightComponent>(thing);
	if (info != nullptr)
	{
		made.glintsEffect = StartEffect(info->particleTypeGlints, WorldOf(made));
		if (made.glintsEffect != 0)
		{
			// Draw steps and draws it each frame with g_game_time_inc (vt +0xFC, vt +0x104 (1.0, 1): Draw_(t, 1))
			psys::manager::SetPerFrame(made.glintsEffect);
			psys::manager::SetDrawPath(made.glintsEffect, psys::DrawPath::Sorted);
			if (auto* effect = psys::manager::Find(made.glintsEffect); effect != nullptr)
			{
				effect->AddTarget(thing);
			}
		}
	}
	return thing;
}

bool IsHighlight(entt::entity thing)
{
	return Get(thing) != nullptr; // vt +0x48C: ScriptHighlight 0x709740 = 1, GameThingWithPos 0x4023A0 = 0
}

uint32_t InfoIndexOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->infoIndex : script_type::k_NoSubtype;
}

bool IsDidYouKnow(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && IsDidYouKnowInfo(highlight->infoIndex);
}

bool IsActive(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && highlight->active;
}

uint32_t ScriptIdOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->scriptId : 0;
}

void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category)
{
	if (auto* highlight = Get(thing))
	{
		highlight->scriptId = scriptId;
		highlight->category = category;
	}
}

void SetYPos(entt::entity thing, float height)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->hasDrawHeight = true; // SetDrawHeight 0x709C44
	highlight->drawHeight = height;
	highlight->altitude = height; // 0x70EFBF: +0x1C
	// 0x70EFC2..: IsObject with a 3D object: LH3DObject::SetPosition(GetAltitude + altitude, angle, scale)
	if (auto* transform = Reg().TryGet<Transform>(thing))
	{
		transform->position = WorldOf(*highlight);
		Reg().SetDirty();
	}
}

float GetYPos(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->altitude : 0.0f;
}

void SetActivated(entt::entity thing, bool on)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->active = on;                   // 0x70A643
	DeleteEffect(highlight->activeEffect);    // 0x70A63D..0x70A64F
	if (on)
	{
		if (!IsDidYouKnowInfo(highlight->infoIndex))
		{
			// 0x70A67A..0x70A6B6: GAudio::PlaySoundEffect(LH_SamplePlayOptions: bank GGlobal.audio +0x3AC, owner this,
			// sample 0x86). (pending, audio) the options' mapping to audio::PlaySoundEffect(PlayOptions) is not checked
		}
		if (const auto* info = InfoRow(highlight->infoIndex); info != nullptr)
		{
			highlight->activeEffect = StartEffect(info->particleTypeActive, WorldOf(*highlight)); // 0x70A6C3..0x70A70F
		}
	}
	SetMeshOf(thing, *highlight); // 0x70A712..0x70A732: vt +0xF4 on the 3D object with the mesh
}

void ProcessHighlights()
{
	StepPulse(g_Pulse, game_clock::MsPerTurn());
	// 0x70A4DC..0x70A506: from the head, next read before Process (vt +0x5FC); only IsAvailable (vt +0x2C) ones.
	// (inferred) every highlight in the list is available: openblack has no other state for them
	const auto list = g_List; // Process may light one (SetActivated), never delete one
	for (const auto thing : list)
	{
		if (auto* highlight = Get(thing))
		{
			Process(thing, *highlight);
		}
	}
}

void UpdateFrame(uint32_t frameMs, const glm::vec3& eye)
{
	auto& registry = Reg();
	bool moved = false;
	for (const auto thing : g_List)
	{
		auto* highlight = Get(thing);
		auto* transform = highlight != nullptr ? registry.TryGet<Transform>(thing) : nullptr;
		if (transform == nullptr)
		{
			continue;
		}
		const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
		// 0x709C7C..0x709CB9: +0x5C set, or the scripts' SET_DRAW_HIGHLIGHT 0 (GScript +0x80) for a scroll: no Draw at
		// all, the 3D object stays where it was (inferred: openblack hides the mesh)
		const bool drawn = highlight->hidden == 0 &&
		                   (help::script_control::GetCameraControl().drawHighlight != 0 || didYouKnow);
		if (!drawn)
		{
			if (registry.AllOf<Mesh>(thing))
			{
				registry.Remove<Mesh>(thing);
				moved = true;
			}
			continue;
		}
		if (!registry.AllOf<Mesh>(thing))
		{
			SetMeshOf(thing, *highlight);
		}
		const auto point = WorldOf(*highlight);
		// 0x709D13..0x709DBD: a lit did-you-know faces the eye, the others turn
		if (highlight->active && didYouKnow)
		{
			highlight->drawAngle = graphics::billboard::YawToEyeAngle(point, eye);
			const auto turns = static_cast<int32_t>(highlight->drawAngle * k_InvTwoPi);
			highlight->drawAngle -= static_cast<float>(turns) * k_TwoPi;
		}
		else
		{
			highlight->drawAngle = SpinAngle(highlight->drawAngle, frameMs);
		}
		// 0x709DBF..0x709EA6: the scale (GetScale) by the camera's distance (GCamera +0x118 / +0x148 / +0x178)
		const float scale = DistanceScale(highlight->scale, gutils::GetDistance(eye, point), didYouKnow);
		transform->position = point; // 0x709EB7..0x709ED2 LH3DObject::SetPosition(point, angle, scale)
		transform->rotation = lh_matrix::AngleY(highlight->drawAngle);
		transform->scale = glm::vec3(scale);
		moved = true;
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye)
{
	DrawExtras extras;
	const auto* highlight = Get(thing);
	const auto* transform = highlight != nullptr ? Reg().TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr || !Reg().AllOf<Mesh>(thing))
	{
		return extras;
	}
	extras.drawn = true;
	const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
	// 0x709ED7..0x709FC4: the mesh's +0x18 point through the 3D object's matrix and |M x (mesh +0x24 half extents)|.
	// (approximate) ObjectMetrics exports no box centre: the centre is taken at half the height over the position
	const auto half = object::ObjectHalfExtents(thing).value_or(glm::vec3(0.0f));
	const glm::mat3 matrix = transform->rotation * glm::mat3(transform->scale.x);
	extras.radius = glm::length(matrix * half);
	extras.centre = transform->position + glm::vec3(0.0f, half.y * transform->scale.y, 0.0f);
	// 0x709FCF..0x70A10A: the glow of an active scroll: YawToEye's rows, the Y row x 3, at the centre
	if (highlight->active && !didYouKnow)
	{
		glm::mat3 axes = graphics::billboard::YawToEye(extras.centre, eye);
		axes[1] *= k_GlowStretch;
		glm::mat4 glow(axes);
		glow[3] = glm::vec4(extras.centre, 1.0f);
		extras.glow = glow;
		extras.glowArgb = ActiveGlowArgb(highlight->infoIndex, ActivePulse(g_Pulse, game_clock::TurnFraction()));
	}
	// 0x70A2BA..0x70A2DC: a scroll is clicked through an invisible sphere (GInterface::SendInvisibleDrawCollision)
	if (!didYouKnow)
	{
		extras.clickRadius = extras.radius + k_ClickMargin;
	}
	// 0x70A2DF..0x70A447: the sprite, not for a lit did-you-know
	if (!(didYouKnow && highlight->active))
	{
		const glm::vec3 towards = eye - extras.centre;
		const float length = glm::length(towards);
		const glm::vec3 direction = length > 0.0f ? towards / length : glm::vec3(0.0f); // InverseSquareRoot 0x841170
		graphics::billboard::Sprite sprite;
		sprite.position = extras.centre + direction * (extras.radius * k_SpriteTowardsEye);
		sprite.size = std::max(k_SpriteSizePerRadius * extras.radius, k_MinSpriteSize);
		sprite.argb = (static_cast<uint32_t>(SpriteAlpha(highlight->infoIndex)) << 24) | 0xFFFFFFu;
		sprite.cell = 0;
		extras.sprite = sprite;
	}
	return extras;
}

bool InterfaceTap(entt::entity thing, bool byLocalPlayer)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return true;
	}
	// 0x70AC83..0x70ACCF: (pending, help profile) HelpProfile::Trigger(+0x78 ? 0x22 : 0x23) for the local player's tap
	if (highlight->scriptId == 0)
	{
		return true; // 0x70ACD9
	}
	SetActivated(thing, true); // 0x70ACE3
	if (IsDidYouKnowInfo(highlight->infoIndex))
	{
		// 0x70ACF9..0x70AD4A: (pending, audio) sample 0xAD for the local status; then fn_0070AC00 ->
		// HelpSystem::SetBubbleProperties(this, +0x78, +0x84) 0x5C8330: the bubble's text, "FirstDYKExplained" for the
		// first one ever, the text marked read (fn_0078CD20). (pending, Intro: the DYK bubble of HelpSystem +0x18) only
		// the read list is kept here
		static_cast<void>(byLocalPlayer);
		dyk_read::MarkRead(highlight->scriptId, highlight->category);
		return true;
	}
	// 0x70AD68..0x70ADAD: (pending) a help task with the dialogue (HelpSystem +0x45CC, ScriptDLL::GetScriptType == 2)
	// -> GScript::StopHelpScripts 0x6EC780; no dialogue owner -> Temple::StartScript(+0x78) 0x794A20 ->
	// ChallengeRoom::StartScript 0x784C10
	return true;
}

void OnToBeDeleted(entt::entity thing)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	g_List.erase(std::remove(g_List.begin(), g_List.end(), thing), g_List.end()); // 0x709980..0x7099CF
	DeleteEffect(highlight->glintsEffect);                                          // 0x7099D5..0x7099E3
	DeleteEffect(highlight->activeEffect);                                          // 0x7099E6..0x7099F4
}

void OnClearMap()
{
	g_Pulse = Pulse {}; // 0x7096E0..0x7096FE
	g_List.clear();     // openblack: ClearMap's GameLists.ClearMap deleted them
}

const std::vector<entt::entity>& All()
{
	return g_List;
}

} // namespace openblack::ecs::script_highlight
