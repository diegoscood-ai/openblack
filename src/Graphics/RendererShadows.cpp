/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The projected shadows (graphics::shadow_list, the ShadowInfo list [0xFAA7E0]; wiki: rendering.md, "Sombras
// proyectadas"): their update once a frame, their draw over the land blocks (fn_007FF610 -> fn_00878350) and over the
// objects (the tail loop of the objects' Draw, 0x80E457..0x80E4D7 -> fn_0080B050).
//
// Where they go in the frame, against the Z-sorter (graphics::zsorter, LH3DZSorter NewZObject 0x83F310, drained by
// FinishFrame 0x82F480 after everything drawn at once): none is a Z object of its own (fn_00878350, fn_0080B050 and
// fn_0084E200 are not among the 32 callers of 0x83F310) and both draw at once (LH3DRender::DrawTriangle 0x82F810 ->
// IDirect3DDevice7 vt+0x68 at 0x82F916; fn_0084E200 0x84E8F0 through [0xC386EC]):
// - over the land: right after each block of the main land (fn_007FF610 0x7FF749, GLandscape::Draw 0x5E4E96), so
//   before the queue (Renderer::DrawPass's block loop, view Main);
// - over an object: at the tail of that object's own Draw (vt+0x108: fn_0080DB30 0x80E4C2 for the vtable 0x9A2974,
//   fn_00812170 0x81317C for 0x9A32A0; vt+0x15C: fn_00810720 0x810CD6, fn_00817930 0x8185AB). That Draw runs at once
//   for an opaque object (LH3DObject::AddDrawing 0x815F62) and from the queue for a blended one (its Z object's
//   callback 0x7FA980 = jmp [vt+0x108], 0x815F53), so the shadow goes right after the object either in the main view
//   or inside the object's Z object, and takes no entry of the queue (its 0x800 cap) of its own.

#include <cstdlib>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandMorph.h"
#include "Camera/Camera.h"
#include "ECS/Animations.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowList.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The shadow's texture stage in the land redraw: past the 0..10 of vs_terrain / fs_terrain, whose bindings stay for
/// the next blocks (the original binds it to stage 0, fn_00878350 through the material si+0x460)
constexpr uint8_t k_LandShadowStage = 11;

/// x0, z0, 1 / (x1 - x0), 1 / (z1 - z0) of the box +0x2C (si+0x1C..0x28)
glm::vec4 BoxUniform(const shadow_math::Box& box)
{
	return {box.x0, box.z0, 1.0f / (box.x1 - box.x0), 1.0f / (box.z1 - box.z0)};
}

/// The code 0x400: si+0x450 / 0x458 (d.x, d.z) and si+0x440 (the least k)
glm::vec4 CullUniform(const shadow_list::ShadowInfo& shadow)
{
	return {shadow.projection.dir.x, shadow.projection.dir.z, shadow.box.kMin, 0.0f};
}
} // namespace

void Renderer::UpdateShadows(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !drawDesc.drawEntities)
	{
		_shadows->Clear();
		return;
	}
	shadow_list::FrameInputs inputs;
	inputs.camera = drawDesc.camera->GetOrigin();                    // g_camera [0xEA1DB8]
	inputs.worldToClip = drawDesc.camera->GetViewProjectionMatrix(); // g_world_to_clipping [0xEA9E40]
	inputs.nearW = billboard::CameraFrame::From(*drawDesc.camera).nearZ;                  // [0xE839E0]
	inputs.landRef = GetDetailLevel(Locator::config::value().detailLevel).landReflection; // [0xE9CD8C]
	_shadows->Frame(inputs);
}

void Renderer::DrawLandShadows(RenderPass viewId, const LandBlock& block, uint64_t cull) const
{
	const auto* program = _shaderManager->GetShader("LandShadow");
	if (program == nullptr)
	{
		return;
	}
	const glm::vec2 corner = block.GetMapPosition();
	// the block's (bx, bz) of fn_007FF610's box test (0x7FF6BF..0x7FF744): its corner / 160
	const int blockX = static_cast<int>(corner.x / shadow_math::k_BlockSize);
	const int blockZ = static_cast<int>(corner.y / shadow_math::k_BlockSize);
	const glm::vec4 u_blockPositionAndSize(corner, shadow_math::k_BlockSize, shadow_math::k_BlockSize);
	// The shadow material: CreateMaterial(6, texture) with +5 = 0 (fn_0087FD50 0x87FE12): mode 6, SRCALPHA /
	// INVSRCALPHA, no Z write, Z LESSEQUAL over the block just drawn, the block's own culling. openblack's depth is
	// reversed and State's LessEqual is GREATER, which would drop the equal depth of the redraw: LessEqualInclusive
	// (GEQUAL; vs_land_shadow computes the depth as vs_terrain does, land_position.sh). The colour's alpha is not written.
	const uint64_t state =
	    render_modes::State(render_modes::Mode::AlphaTexturedAlphaNz,
	                        {.zFunc = render_modes::ZFunc::LessEqualInclusive, .msaa = true}) |
	    cull;
	// the vertex streams, the state and the transform go after each draw; the terrain's bindings stay
	constexpr auto k_Discard = BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_TRANSFORM |
	                           BGFX_DISCARD_VERTEX_STREAMS | BGFX_DISCARD_STATE;
	// every active shadow (fn_00881030) whose box touches the block, newest first; all of them, si+0xC is not read here
	_shadows->ForEachActive([&](const shadow_list::ShadowInfo& shadow) {
		if (!shadow_math::TouchesBlock(shadow.box, blockX, blockZ))
		{
			return;
		}
		// fn_00878350: the light si+0x444 and this shadow's t' (one H per shadow, 0x878394..0x87842F)
		const glm::vec4 u_shadowLight(shadow.projection.light, shadow.landT);
		const auto u_shadowBox = BoxUniform(shadow.box);
		const auto u_shadowCull = CullUniform(shadow);
		program->SetUniformValue("u_blockPositionAndSize", &u_blockPositionAndSize);
		program->SetUniformValue("u_shadowLight", &u_shadowLight);
		program->SetUniformValue("u_shadowBox", &u_shadowBox);
		program->SetUniformValue("u_shadowCull", &u_shadowCull);
		program->SetTextureSampler("s_shadow", k_LandShadowStage, fromBgfx(shadow.texture));
		block.GetMesh().GetVertexBuffer().Bind();
		bgfx::setState(state, 0);
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()), 0, k_Discard);
	});
}

void Renderer::CollectShadowReceivers(bool mainView) const
{
	_shadowReceivers.clear();
	// "ShadowsOnObjects" detail key; only the main view (the reflection draws no shadows)
	if (!mainView || !GetDetailLevel(Locator::config::value().detailLevel).shadowsOnObjects)
	{
		return;
	}
	// the entries with si+0xC == 0: the hand and the launched boat (the creature too, not here yet), in the list's order
	// (the loop walks [0xFAA7E0] by +4, 0x80E462..0x80E4CC: newest first)
	std::vector<const shadow_list::ShadowInfo*> shadows;
	_shadows->ForEachActive([&shadows](const shadow_list::ShadowInfo& shadow) {
		if (shadow.onObjects)
		{
			shadows.push_back(&shadow);
		}
	});
	if (shadows.empty())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	// SetHeldObject clears the held object's flag of receiving (vt+0x78(0), 0x816842)
	const auto held = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHeldObject() : std::nullopt;
	for (const auto& [entity, instance] : renderCtx.entityInstances)
	{
		// the receiver: vt+0x7C (Flags1 0x40, RenderingSystem's ReceivesDynamicShadow), 0x80E457..0x80E460
		if (!instance.receivesDynamicShadow || (held.has_value() && *held == entity) || !meshes.Contains(instance.meshId))
		{
			continue;
		}
		const auto mesh = meshes.Handle(instance.meshId);
		// ContainsThisBoundingBox (vt+0x1BC, fn_007F9E80, 0x80E497): the mesh box centre +- half its size, moved to the
		// object, x and z only; the morphable Draw has its own test instead (fn_0080E550, shadow_math::ReachesMorphable)
		const auto& meshBox = mesh->GetBoundingBox();
		const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
		if (transform == nullptr)
		{
			continue;
		}
		// obj+0x14, the drawn matrix (x and z read only). (inferido) valid while no MorphWithTerrain entity gets
		// RenderingSystem's per-instance edits of that matrix (the fields' and trees' wind sway, the trees' bend, the
		// burning trees' shrink): those edits are the original's draw matrix, not obj+0x14; none carries the component
		const auto& model = renderCtx.instanceUniforms[instance.index];
		const float scale = transform->scale.x;                          // obj+0x44
		const float halfDiagonal = ecs::object::MeshHalfDiagonal(instance.meshId); // mesh+0x30
		const glm::vec2 centre =
		    glm::vec2(meshBox.Center().x, meshBox.Center().z) + glm::vec2(transform->position.x, transform->position.z);
		const glm::vec2 half = glm::vec2(meshBox.Size().x, meshBox.Size().z) * 0.5f;
		for (const auto* shadow : shadows)
		{
			// si+0x464 != obj (the caster of a generic shadow, 0x80E47C), and not the complex object's own si (vt+0x1A8 /
			// vt+0x1B8, 0x80E4A5..0x80E4BB: the hand's body)
			if (shadow->caster == entity)
			{
				continue;
			}
			const auto& box = shadow->box;
			// (inferido) MorphWithTerrain stands for the morphable class here (vtable 0x9A2E34, Get3DType 1, Draw
			// fn_0080E550). The CITADEL class (Get3DType 8, CitadelHeart 0x464B40; vtable 0x9A2BFC) draws through
			// fn_00882A40 -> the static Draw fn_0080DB30 (0x882AB5): ContainsThisBoundingBox and ZFUNC EQUAL. No type 8
			// entity carries the component today (CitadelArchetype has none; CitadelPart is type 1, 0x4694B0)
			if (instance.morphWithTerrain)
			{
				// fn_0080E550 0x80E78E..0x80E857 (no vt+0x1A8 / vt+0x1B8 test there, only si+0x464 0x80E782)
				if (!shadow_math::ReachesMorphable(box, meshBox.Center(), model, scale, halfDiagonal))
				{
					continue;
				}
			}
			else if (centre.x + half.x < box.x0 || centre.x - half.x > box.x1 || centre.y + half.y < box.z0 ||
			         centre.y - half.y > box.z1)
			{
				continue;
			}
			auto& receiver = _shadowReceivers[instance.index];
			receiver.meshId = instance.meshId;
			receiver.morphWithTerrain = instance.morphWithTerrain;
			receiver.shadows.push_back(shadow);
		}
	}
	// OPENBLACK_SHADOW_TRACE=1: the receivers of this frame, once a second
	static const bool k_Trace = std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr;
	static uint32_t traceFrame = 0;
	if (k_Trace && ++traceFrame % 60 == 0)
	{
		for (const auto& [instance, receiver] : _shadowReceivers)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "shadow receiver: instance {} mesh {} shadows {}", instance,
			                   receiver.meshId, receiver.shadows.size());
		}
	}
}

void Renderer::DrawShadowsOnObject(RenderPass viewId, uint32_t instance, const glm::mat4* matrices,
                                   uint8_t matrixCount) const
{
	const auto found = _shadowReceivers.find(instance);
	if (found == _shadowReceivers.end())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& receiver = found->second;
	const auto mesh = meshes.Handle(receiver.meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// fn_0080B050 0x80B06A..0x80B08B: SetMaterial of the shadow material [si+0x460] (CreateMaterial(6) fn_0087FD50
	// 0x87FE12) through the current table, mode 6 (SRCALPHA / INVSRCALPHA, no Z write); the current table is the normal
	// one again by then, also for a fading object (fn_0080DB30 0x80E197 puts 0xC38728 back after its 0xC387C8 of
	// 0x80DF09). fn_0084E200 draws each primitive with no state of its own (the draw-triangle pointer [0xC386EC],
	// 0x84E8F0, at once). The shadow material's culling: +5 = 0 (CreateMaterial 0x87FE12), CULLMODE CCW (fn_0080B050
	// 0x80B0AD..0x80B0E6), two-sided primitives too.
	// The Z test over the object as it was just drawn: the static Draw fn_0080DB30 sets ZFUNC EQUAL before each shadow
	// (0x80E484) and LESSEQUAL after the loop (0x80E4CE), and so does fn_00810720 (vt+0x15C, 0x810C8F / 0x810CF2); the
	// animated one fn_00812170 (vt+0x108 of 0x9A32A0, the loop 0x81311A..0x81317C) sets nothing, so the frame's
	// LESSEQUAL holds (0x82CCC5): LessEqualInclusive (GEQUAL in openblack's reversed depth), which lets the redraw's
	// equal depth pass (as DrawLandShadows). The morphable Draw fn_0080E550 (vt+0x108 of 0x9A2E34) sets nothing either
	// around its loop (0x80E768..0x80E874 -> fn_0080AE40): LESSEQUAL too. (inferido) that a boned mesh is one of the
	// animated class
	const bool lessEqual = mesh->IsBoned() || receiver.morphWithTerrain;
	submitDesc.mode = render_modes::Mode::AlphaTexturedAlphaNz;
	submitDesc.options = {.zFunc = lessEqual ? render_modes::ZFunc::LessEqualInclusive : render_modes::ZFunc::Equal,
	                      .cull = render_modes::Cull::Ccw,
	                      .msaa = true};
	submitDesc.morphWithTerrain = receiver.morphWithTerrain;
	submitDesc.program =
	    land_morph::ObjectProgram(*_shaderManager, receiver.morphWithTerrain, land_morph::ObjectPass::Shadow);
	static const auto k_Identity = glm::mat4(1.0f);
	submitDesc.modelMatrices = matrices != nullptr ? matrices : &k_Identity;
	submitDesc.matrixCount = matrices != nullptr ? matrixCount : 1;
	for (const auto* shadow : receiver.shadows)
	{
		submitDesc.dynamicShadow = fromBgfx(shadow->texture);
		submitDesc.dynamicShadowBox = BoxUniform(shadow->box);
		submitDesc.dynamicShadowCull = CullUniform(*shadow);
		submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance, 1);
		DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
	static const bool k_Trace = std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr;
	static uint32_t traceCount = 0;
	if (k_Trace && ++traceCount % 60 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "shadow on object: instance {} mesh {} view {} matrices {}", instance,
		                   receiver.meshId, static_cast<int>(viewId), matrixCount);
	}
	_shadowReceivers.erase(found);
}

void Renderer::DrawShadowsOnCutObjects(RenderPass viewId, const std::unordered_set<uint32_t>& cut) const
{
	if (_shadowReceivers.empty() || cut.empty())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto poses = ecs::PosesByInstance(renderCtx);
	// (inferido) after all of them instead of right after each one: they are opaque and drawn with the Z test of the
	// receivers, so nothing drawn in between can take their shadow. Only the instances DrawCutAboveWater drew
	std::vector<uint32_t> drawn;
	for (const auto& [instance, receiver] : _shadowReceivers)
	{
		if (cut.contains(instance))
		{
			drawn.push_back(instance);
		}
	}
	for (const auto instance : drawn)
	{
		const auto mesh = meshes.Handle(_shadowReceivers.at(instance).meshId);
		const glm::mat4* matrices = nullptr;
		uint8_t count = 0;
		if (mesh->IsBoned())
		{
			matrices = mesh->GetBoneMatrices().data();
			count = static_cast<uint8_t>(std::min<size_t>(255, mesh->GetBoneMatrices().size()));
			ecs::UsePose(poses, instance, *mesh, matrices, count);
		}
		DrawShadowsOnObject(viewId, instance, matrices, count);
	}
}
