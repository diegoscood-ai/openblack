/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <SDL_video.h>
#include <bgfx/platform.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/Clouds.h"
#include "3D/Foliage.h"
#include "3D/DayNightClock.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "3D/SkyWeather.h"
#include "3D/NightLights.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/ScreenFade.h"
#include "3D/SkyInterface.h"
#include "3D/SkyType.h"
#include "Camera/Camera.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Stream.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/WaterRings.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Common/HelpText.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GameFont.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/ModelLight.h"
#include "Graphics/ShadowList.h"
#include "Graphics/Primitive.h"
#include "Graphics/RenderModes.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Game.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h" // milagros2 pieces (pieces_shadows_PLAN.md §1.3 d)
#include "Graphics/ZSorter.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"
#include "PSys/Creators/Mist.h"
#include "PSys/Rules/ExplodeObject.h" // milagros2 pieces (pieces_shadows_PLAN.md §1.3 d)
#include "Profiler.h"
#include "Renderer.h"

#include <unordered_map>
#include <unordered_set>
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::systems;

namespace openblack
{
// clang-format off
constexpr auto k_BgfxDefaultStateInvertedZ = 0 \
                                     | BGFX_STATE_WRITE_RGB \
                                     | BGFX_STATE_WRITE_A \
                                     | BGFX_STATE_WRITE_Z \
                                     | BGFX_STATE_CULL_CW \
                                     | BGFX_STATE_DEPTH_TEST_GREATER \
                                     | BGFX_STATE_MSAA;
// clang-format on

/// Backbuffer reset flags for the graphics mods (MSAA, anisotropy); none by default, like the original.
uint32_t GraphicsOptionResetFlags()
{
	const auto& config = Locator::config::value();
	uint32_t flags = BGFX_RESET_NONE;
	switch (config.msaa)
	{
	case 2:
		flags |= BGFX_RESET_MSAA_X2;
		break;
	case 4:
		flags |= BGFX_RESET_MSAA_X4;
		break;
	case 8:
		flags |= BGFX_RESET_MSAA_X8;
		break;
	case 16:
		flags |= BGFX_RESET_MSAA_X16;
		break;
	default:
		break;
	}
	if (config.anisotropicFiltering)
	{
		flags |= BGFX_RESET_MAXANISOTROPY;
	}
	return flags;
}

struct BgfxCallback: public bgfx::CallbackI
{
	constexpr static std::array<std::string_view, bgfx::Fatal::Count> k_CodeLookup = {
	    "DebugCheck",            //
	    "InvalidShader",         //
	    "UnableToInitialize",    //
	    "UnableToCreateTexture", //
	    "DeviceLost",            //
	};

	~BgfxCallback() override = default;

	void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override
	{
		const auto* codeStr = k_CodeLookup.at(code).data();
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "bgfx: {}:{}: FATAL ({}): {}", filePath, line, codeStr, str);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
		spdlog::get("graphics")
		    ->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::critical, "FATAL ({}): {}", codeStr,
		          str);
#endif

		// Must terminate, continuing will cause crash anyway.
		throw std::runtime_error(std::string("bgfx: ") + filePath + ":" + std::to_string(line) + ": FATAL (" + codeStr +
		                         "): " + str);
	}

	void traceVargs([[maybe_unused]] const char* filePath, [[maybe_unused]] uint16_t line, const char* format,
	                va_list argList) override
	{
		std::array<char, 0x2000> temp;
		char* out = temp.data();
		int32_t len = vsnprintf(out, temp.size(), format, argList);
		if (static_cast<int32_t>(temp.size()) < len)
		{
			out = reinterpret_cast<char*>(alloca(len + 1));
			len = vsnprintf(out, len, format, argList);
		}
		if (len > 0)
		{
			out[len] = '\0';
			if (len > 0 && out[len - 1] == '\n')
			{
				out[len - 1] = '\0';
			}
// TODO(bwrsandman): change level to trace
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
			spdlog::get("graphics")->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::debug, out);
#endif
		}
		else
		{
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
			spdlog::get("graphics")
			    ->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::err,
			          "bgfx: failed to format message: {}", format);
#endif
		}
	}
	void profilerBegin([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t abgr, [[maybe_unused]] const char* filePath,
	                   [[maybe_unused]] uint16_t line) override
	{
	}
	void profilerBeginLiteral([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t abgr,
	                          [[maybe_unused]] const char* filePath, [[maybe_unused]] uint16_t line) override
	{
	}
	void profilerEnd() override {}
	// Reading and writing to shader cache
	uint32_t cacheReadSize([[maybe_unused]] uint64_t id) override { return 0; }
	bool cacheRead([[maybe_unused]] uint64_t id, [[maybe_unused]] void* data, [[maybe_unused]] uint32_t size) override
	{
		return false;
	}
	void cacheWrite([[maybe_unused]] uint64_t id, [[maybe_unused]] const void* data, [[maybe_unused]] uint32_t size) override {}
	// Saving a screenshot
	void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch, const void* data,
	                [[maybe_unused]] uint32_t size, bool yflip) override
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Taking a screenshot...");

		const auto ext = std::filesystem::path(filePath).extension();
		if (std::filesystem::path(filePath).extension() == ".png")
		{
			bx::FileWriter writer;
			bx::Error err;
			if (bx::open(&writer, filePath, false, &err))
			{
				// Strip out alpha for screenshot
				std::vector<uint32_t> noAlpha;
				noAlpha.resize(size / sizeof(noAlpha[0]), 0);
				memcpy(noAlpha.data(), data, size);
				for (uint32_t y = 0; y < height; ++y)
				{
					for (uint32_t x = 0; x < width; ++x)
					{
						noAlpha[x + pitch / sizeof(noAlpha[0]) * y] |= 0xFF000000;
					}
				}

				bimg::imageWritePng(&writer, width, height, pitch, noAlpha.data(), bimg::TextureFormat::BGRA8, yflip, &err);
				bx::close(&writer);
				SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot ({}x{}) saved at {}", width, height, filePath);
			}
			else
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save Screenshot ({}x{}) at {}: {}", width, height,
				                    filePath, std::string(err.getMessage().getCPtr(), err.getMessage().getLength()));
			}
		}
		else
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: {} screenshot ({}x{}) requested at {}", ext.string(),
			                   width, height, filePath);
		}
	}
	// Saving a video
	void captureBegin(uint32_t width, uint32_t height, [[maybe_unused]] uint32_t pitch,
	                  [[maybe_unused]] bgfx::TextureFormat::Enum format, [[maybe_unused]] bool yflip) override
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture Begin ({}x{}) requested", width, height);
	}
	void captureEnd() override { SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture End requested"); }
	void captureFrame([[maybe_unused]] const void* data, [[maybe_unused]] uint32_t size) override
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture Frame requested");
	}
};

} // namespace openblack

std::unique_ptr<RendererInterface> RendererInterface::Create(GraphicsBackend backend, bool vsync) noexcept
{
	bgfx::Init init {};
	switch (backend)
	{
	case GraphicsBackend::Noop:
		init.type = bgfx::RendererType::Noop;
		break;
	case GraphicsBackend::Direct3D12:
		init.type = bgfx::RendererType::Direct3D12;
		break;
	case GraphicsBackend::Metal:
		init.type = bgfx::RendererType::Metal;
		break;
	case GraphicsBackend::Vulkan:
		init.type = bgfx::RendererType::Vulkan;
		break;
	default:
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Got impossible graphics backend.");
		return nullptr;
	}

	// Get render area size
	glm::uvec2 drawableSize;
	if (backend != GraphicsBackend::Noop)
	{
		const auto& window = Locator::windowing::value();

		drawableSize = static_cast<glm::uvec2>(window.GetSize());
		init.resolution.width = static_cast<uint32_t>(drawableSize.x);
		init.resolution.height = static_cast<uint32_t>(drawableSize.y);

		// Get Native Handles from SDL window
		const auto handles = window.GetNativeHandles();
		init.platformData.nwh = handles.nativeWindow;
		init.platformData.ndt = handles.nativeDisplay;
	}

	uint32_t bgfxReset = BGFX_RESET_NONE;
	auto bgfxCallback = std::make_unique<BgfxCallback>();
	if (vsync)
	{
		bgfxReset |= BGFX_RESET_VSYNC;
	}
	init.resolution.reset = bgfxReset | GraphicsOptionResetFlags();
	init.callback = dynamic_cast<bgfx::CallbackI*>(bgfxCallback.get());
	// milagros2 pieces (pieces_shadows_PLAN.md §1.3 b), (openblack guard): the exploded pieces' triangles go up in the
	// frame's transient vertex buffer (world_triangles), a tree is about 0.5 MB of them; bgfx's default is 6 MB for all
	init.limits.transientVbSize = 32 << 20;

	if (!bgfx::init(init))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to initialize bgfx.");
		return nullptr;
	}

	const bgfx::Caps* caps = bgfx::getCaps();
	if ((caps->supported & BGFX_CAPS_TEXTURE_2D_ARRAY) == 0 || caps->limits.maxTextureLayers < 9)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Graphics device must support texture layers.");
		return nullptr;
	}

	return std::make_unique<Renderer>(bgfxReset, std::move(bgfxCallback));
}

Renderer::Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
    , _shadows(std::make_unique<shadow_list::List>())
    , _bgfxCallback(std::move(bgfxCallback))
    , _bgfxReset(bgfxReset)
{
	_shaderManager->LoadShaders();
	_plane = Primitive::CreatePlane();

	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::Main), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::MainBlended), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay), bgfx::ViewMode::Sequential);
	// what is under the sea is painted in order too (GLandscape::Draw 0x5E48AE..0x5E4E6B): the sky, the moon's
	// reflection, the mirrored land, the parts under the water, the hand glow
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::Reflection), bgfx::ViewMode::Sequential);

	// give debug names to views
	// TODO (#749) use std::views::enumerate
	for (bgfx::ViewId i = 0; const auto& name : k_RenderPassNames)
	{
		bgfx::setViewName(i, name.data());
		++i;
	}
}

Renderer::~Renderer() noexcept
{
	_clouds.reset();
	_font.reset(); // its texture before bgfx::shutdown
	_foliage.reset();
	_shadows.reset(); // its textures before bgfx::shutdown
	if (bgfx::isValid(_landLightTexture))
	{
		bgfx::destroy(_landLightTexture);
	}
	if (bgfx::isValid(_landCellsTexture))
	{
		bgfx::destroy(_landCellsTexture);
	}
	if (bgfx::isValid(_videoTexture))
	{
		bgfx::destroy(_videoTexture);
	}
	if (bgfx::isValid(_videoAlphaTexture))
	{
		bgfx::destroy(_videoAlphaTexture);
	}
	if (bgfx::isValid(_fishPlotInstances))
	{
		bgfx::destroy(_fishPlotInstances);
	}
	_plane.reset();
	_shaderManager.reset();
	bgfx::frame();
	bgfx::shutdown();
}

void Renderer::ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept
{
	bgfx::setViewClear(static_cast<bgfx::ViewId>(viewId), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 0.0f, 0);
	bgfx::setViewRect(static_cast<bgfx::ViewId>(viewId), 0, 0, resolution.x, resolution.y);
	if (viewId == graphics::RenderPass::Main)
	{
		const auto blended = static_cast<bgfx::ViewId>(graphics::RenderPass::MainBlended);
		bgfx::setViewClear(blended, BGFX_CLEAR_NONE);
		bgfx::setViewRect(blended, 0, 0, resolution.x, resolution.y);
		const auto overlay = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
		bgfx::setViewClear(overlay, BGFX_CLEAR_NONE);
		bgfx::setViewRect(overlay, 0, 0, resolution.x, resolution.y);
		_resolution = resolution;
	}
}

void Renderer::Reset(glm::u16vec2 resolution) const noexcept
{
	bgfx::reset(resolution.x, resolution.y, _bgfxReset | GraphicsOptionResetFlags());
}

graphics::ShaderManager& Renderer::GetShaderManager() const noexcept
{
	return *_shaderManager;
}

void Renderer::DrawSubMesh(const graphics::L3DMesh& mesh, const graphics::L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
                           bool preserveState) const
{
	assert(&subMesh.GetMesh());
	// meshes without bones use the variant of the program with a single model matrix, meshes with up to 32 bones the one
	// with 32 (see vs_object.sc)
	const auto* program = !mesh.IsBoned()                          ? StaticVariant(desc.program)
	                      : mesh.GetBoneMatrices().size() <= 32 ? BonesVariant32(desc.program)
	                                                             : desc.program;
	// We don't draw physics meshes, we haven't implemented statuses (building and graves) and modern GPUs can handle high lod.
	// Window submeshes have no LOD bits: the original draws them only at night, without the LOD test (Abode::Draw ->
	// fn_00856D40); vs_object hides them on the instances whose windows are not lit.
	const bool window = subMesh.GetFlags().isWindow && desc.instanceDesc != nullptr;
	if (!desc.drawAll &&
	    (subMesh.IsPhysics() || subMesh.GetFlags().status != 0 || ((subMesh.GetFlags().lodMask & 1) != 1 && !window)))
	{
		return;
	}

	const auto& island = Locator::terrainSystem::value();

	auto extent = island.GetExtent();
	auto islandExtent = glm::vec4(extent.minimum, extent.maximum);
	const auto& heightMap = island.GetHeightMap();

	bool lastPreserveState = false;
	// the pass of the opaque models: the normal table and every primitive in its own mode
	const bool modelPass = desc.table == render_modes::Table::Normal && !desc.mode.has_value();
	// MSAA mod: smooth alpha cut-out edges in the multisampled opaque passes (not in blended ones)
	const bool alphaToCoverage = Locator::config::value().msaa != 0 && desc.viewId != RenderPass::Reflection && modelPass;
	const auto& primitives = subMesh.GetPrimitives();
	for (auto it = primitives.begin(); it != primitives.end(); ++it)
	{
		const auto& prim = *it;

		const bool hasNext = std::next(it) != primitives.end();

		const Texture2D* texture = world_triangles::PrimitiveTexture(mesh, prim.skinID);
		const Texture2D* nextTexture = !hasNext ? nullptr : world_triangles::PrimitiveTexture(mesh, std::next(it)->skinID);

		// Material blending of the original (L3D material type): AlphaTextured & co. blend with the texture alpha, e.g.
		// the fading wrist of the hand and the soft edges of buildings. Chroma materials stay alpha tested.
		const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
		// SetMaterial (0x412662..0x4126BD): the mode of the primitive's material through the current table, or the mode
		// every primitive is drawn in
		const auto drawn =
		    render_modes::Select(desc.mode.value_or(static_cast<render_modes::Mode>(prim.materialType)), desc.table);
		// MSAA mod (only in the model pass, so the primitive's own mode)
		const bool a2c = render_modes::AlphaToCoverage(drawn, alphaToCoverage);
		const auto sameMaterial = [&prim](const L3DSubMesh::Primitive& other) {
			return other.materialType == prim.materialType && other.blend == prim.blend &&
			       other.thresholdAlpha == prim.thresholdAlpha &&
			       other.depthWrite == prim.depthWrite && other.alphaCutoutThreshold == prim.alphaCutoutThreshold &&
			       other.wrap == prim.wrap && other.uvOffset == prim.uvOffset;
		};
		const bool primitivePreserveState = texture != nullptr && texture == nextTexture && sameMaterial(*std::next(it)) &&
		                                    (preserveState || hasNext);
		// the main pass draws the opaque primitives; the blended ones go through the back-to-front list (Z-sorter)
		if ((desc.blendFilter == 1 && blended) || (desc.blendFilter == 2 && !blended))
		{
			continue;
		}

		uint32_t skip = Mesh::SkipState::SkipNone;
		if (!lastPreserveState)
		{
			if (desc.modelMatrices != nullptr && desc.matrixCount > 0)
			{
				bgfx::setTransform(desc.modelMatrices, desc.matrixCount);
			}
			if (texture != nullptr)
			{
				// Materials without the tiling bit are clamped (LH3DRender::SetD3DTillingOff; the global
				// g_b_need_tilling is only set for particle meshes)
				const uint32_t samplerFlags =
				    prim.wrap ? UINT32_MAX
				              : (texture->GetSamplerFlags() & ~(BGFX_SAMPLER_U_MASK | BGFX_SAMPLER_V_MASK)) |
				                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
				program->SetTextureSampler("s_diffuse", 0, *texture, samplerFlags);
			}
			if (desc.morphWithTerrain)
			{
				program->SetTextureSampler("s_heightmap", 1, heightMap);   // vs
				program->SetUniformValue("u_islandExtent", &islandExtent); // vs
			}
			if (desc.isSky)
			{
				// vs_object reads u_objectClip in both its branches (sea_plane.sh) and bgfx keeps the last value of a
				// uniform: the sky is never a sea draw, so it must not inherit the last net's or shark's unmirror
				program->SetUniformValue("u_objectClip", &sea_pass::k_NoClip); // vs
			}
			if (!desc.isSky)
			{
				const bool lit = _landLight && _landLight->IsLoaded();
				const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
				const glm::vec4 u_cellMap = {extent.minimum, cellMapSize};
				// x, by desc.sea.light (sea_pass::SeaLight): 0 white, 1 lit like the original (Normal), 2 B's constant
				// colour (Constant: z = obj+0x4C's r 65536 + g 256 + b), 3 the land colour only (LastDraw), 4 C,
				// DrawCutByPlane (Cut: y = the colour's alpha, z = its rgb, w = the specular obj+0x50's rgb)
				// y: the colour boost, w: 1 = no haze + 2 x the land light mode (land_light::ObjectMode), modes 1 and 3
				constexpr uint32_t k_Rgb = 0x00FFFFFFu;
				glm::vec4 u_objectLight = {lit ? 1.0f : 0.0f, desc.lightBoost, -1.0f,
				                           (desc.noHaze ? 1.0f : 0.0f) + 2.0f * static_cast<float>(desc.landLightMode)};
				switch (desc.sea.light)
				{
				case sea_pass::SeaLight::Normal:
					break;
				case sea_pass::SeaLight::Constant:
					// fn_00811010 0x811033..0x81103F: obj+0x4C -> [0xC37D8C], obj+0x50 -> [0xE9FE2C], both read per vertex
					// by fn_00850FC0 (0x851082, 0x85102F). The colour's alpha (the hand's 0x65) only reaches what the
					// stage's alpha takes from the diffuse: no table 0xC387C8 here (fn_00811010 tests Flags1 & 0x80 with
					// vt+0x4C = fn_007F9D80 at 0x8110BF, `test eax, eax / je 0x81114C` 0x8110C2..0x8110C4, and only then
					// stores the table, 0x8110CF; (inferido) the hand's object never gets the bit, see
					// Renderer::DrawUnderWater) and the hand's AlphaTextured takes the texture's alpha, so it is left out
					u_objectLight = {2.0f, desc.lightBoost, static_cast<float>(desc.sea.argb & k_Rgb),
					                 static_cast<float>(desc.sea.specular & k_Rgb)};
					break;
				case sea_pass::SeaLight::LastDraw:
					u_objectLight.x = lit ? 3.0f : 0.0f;
					break;
				case sea_pass::SeaLight::Cut:
					// z = -1: each instance's own colour and specular (sea_pass::CutAtoms, the PSys mesh atoms)
					u_objectLight = {4.0f, static_cast<float>(desc.sea.argb >> 24) / 255.0f,
					                 desc.sea.perInstanceColour ? -1.0f : static_cast<float>(desc.sea.argb & k_Rgb),
					                 static_cast<float>(desc.sea.specular & k_Rgb)};
					break;
				}
				// x: the plane kept (fs), y: 1 = mirrored back in y = 0 (vs) (sea_plane.sh)
				const auto u_objectClip = sea_pass::PackClip(desc.sea);
				program->SetUniformValue("u_objectClip", &u_objectClip); // vs, fs
				program->SetTextureSampler("s_landLightTable", 3, fromBgfx(_landLightTexture)); // vs
				// this frame's cells (land_light), or the loaded ones (the same layout) before the first frame's
				if (bgfx::isValid(_landCellsTexture) && glm::vec2(_landCellsSize) == cellMapSize)
				{
					program->SetTextureSampler("s_landCells", 4, fromBgfx(_landCellsTexture)); // vs
				}
				else
				{
					program->SetTextureSampler("s_landCells", 4, island.GetCellMap()); // vs
				}
				program->SetUniformValue("u_cellMap", &u_cellMap);                    // vs
				program->SetUniformValue("u_objectLight", &u_objectLight);            // vs
				program->SetUniformValue("u_haze", &_hazeUniforms[0]);               // vs
				program->SetUniformValue("u_hazeColour", &_hazeUniforms[1]);         // vs
				// xyz: the one light of LH3DTech [0xEA9E90], w: the ambient [0xC39264] (model_light.sh, fn_0084BA90)
				const auto u_modelLight = model_light::Uniform();
				program->SetUniformValue("u_modelLight", &u_modelLight);              // vs, fs
				// y, z: mod graphics.hd-tweaks on villagers lit like the original (lighting mode, mip bias; fs_object)
				const auto& config = Locator::config::value();
				const bool person = subMesh.IsHdTweaked() && desc.instanceDesc != nullptr && lit &&
				                    desc.sea.light == sea_pass::SeaLight::Normal;
				// w: 1 if the primitive takes the object's texture offset (L3DSubMesh::Primitive::uvOffset)
				const glm::vec4 u_window = {subMesh.GetFlags().isWindow ? 1.0f : 0.0f,
				                            person ? static_cast<float>(config.hdTweaksLighting) : 0.0f,
				                            person ? config.hdTweaksMipBias : 0.0f, prim.uvOffset ? 1.0f : 0.0f};
				program->SetUniformValue("u_window", &u_window);                      // vs
				const glm::vec4 u_materialColour = {glm::vec3(prim.colour), texture == nullptr ? 1.0f : 0.0f};
				program->SetUniformValue("u_materialColour", &u_materialColour);      // fs
				if (desc.dynamicShadow.has_value())
				{
					program->SetTextureSampler("s_dynamicShadow", 5, *desc.dynamicShadow);
					program->SetUniformValue("u_dynamicShadowBox", &desc.dynamicShadowBox);
					program->SetUniformValue("u_dynamicShadowCull", &desc.dynamicShadowCull);
				}
			}
			if (!desc.isSky)
			{
				// y: the drawn mode's ALPHAREF / 255 (-1 without alpha test), w: its stage 0 alpha
				const auto alpha = render_modes::PrimitiveAlpha(
				    drawn, desc.table, static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)), desc.globalAlpha);
				const glm::vec4 u_skyAlphaThreshold = {
				    0.0f, // x: unused (fs_object reads only y, z, w)
				    alpha.ref,
				    a2c ? 1.0f : 0.0f,
				    static_cast<float>(alpha.source),
				};
				program->SetUniformValue("u_skyAlphaThreshold", &u_skyAlphaThreshold);
			}
		}
		else
		{
			skip |= Mesh::SkipState::SkipRenderState;
			skip |= Mesh::SkipState::SkipVertexBuffer;
		}

		{
			if (desc.instanceDesc != nullptr && (skip & Mesh::SkipState::SkipInstanceBuffer) == 0)
			{
				bgfx::setInstanceDataBuffer(toBgfx(desc.instanceDesc->GetRawHandle()), desc.instanceDesc->GetStart(),
				                            desc.instanceDesc->GetCount());
			}
			// (openblack guard) a mesh whose buffers bgfx could not create (out of handles) is not drawn
			if (!subMesh.GetMesh().GetVertexBuffer().IsValid() ||
			    (subMesh.GetMesh().IsIndexed() && !subMesh.GetMesh().GetIndexBuffer().IsValid()))
			{
				bgfx::discard(BGFX_DISCARD_ALL);
				lastPreserveState = false;
				continue;
			}
			if (subMesh.GetMesh().IsIndexed() && (skip & Mesh::SkipState::SkipIndexBuffer) == 0)
			{
				subMesh.GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			if ((skip & Mesh::SkipState::SkipVertexBuffer) == 0)
			{
				subMesh.GetMesh().GetVertexBuffer().Bind();
			}
			auto viewId = desc.viewId;
			// SetMaterial: the culling from the material's +5 bit 0 (D3DCULL_CCW 0x84C34A; the mirrored reflection camera
			// flips it, and a mesh mirrored back in the sea flips it back: sea_pass::SeaPassState::FaceCull)
			auto options = desc.options;
			if (!desc.isSky && options.cull == render_modes::Cull::None)
			{
				options.cull = sea_pass::ForPass(viewId).FaceCull(sea_pass::Surface::Model, prim.twoSided, desc.sea.unmirror);
			}
			// Blended: drawn after every opaque model (MainBlended) so what lies behind is already in the target
			if (blended && modelPass && viewId == RenderPass::Main)
			{
				viewId = RenderPass::MainBlended;
			}
			const auto state = render_modes::PrimitiveState(drawn, options, blended, alphaToCoverage);
			if ((skip & Mesh::SkipState::SkipRenderState) == 0)
			{
				bgfx::setState(state, desc.rgba);
			}

			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()), 0,
			             primitivePreserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
		}
		lastPreserveState = primitivePreserveState;
	}
}

namespace
{
/// The land light mode and haze of a mesh's models (RenderContext::meshLandLight, land_light::ObjectLight)
void ApplyLandLightMode(const RenderContext& context, entt::id_type meshId, RendererInterface::L3DMeshSubmitDesc& desc)
{
	const auto mode = context.meshLandLight.find(meshId);
	desc.landLightMode = 0;
	if (mode != context.meshLandLight.end())
	{
		desc.landLightMode = static_cast<uint8_t>(mode->second.mode);
		desc.noHaze = desc.noHaze || !mode->second.haze;
	}
}
} // namespace

namespace
{
/// Whether a sphere touches the view volume of a view-projection matrix (the planes of its rows, Gribb-Hartmann)
bool SphereInView(const glm::mat4& viewProjection, const glm::vec3& centre, float radius)
{
	const glm::mat4 rows = glm::transpose(viewProjection);
	for (int plane = 0; plane < 6; ++plane)
	{
		const glm::vec4 p = rows[3] + (plane % 2 == 0 ? 1.0f : -1.0f) * rows[plane / 2];
		if (glm::dot(glm::vec3(p), centre) + p.w < -radius * glm::length(glm::vec3(p)))
		{
			return false;
		}
	}
	return true;
}
} // namespace

const graphics::ShaderProgram* Renderer::BonesVariant32(const graphics::ShaderProgram* program) const
{
	if (_bonesVariants32.empty())
	{
		for (const auto* name : {"ObjectInstanced", "ObjectHeightMapInstanced", "ObjectShadowInstanced", "ObjectHeightMapShadowInstanced"})
		{
			_bonesVariants32.emplace(_shaderManager->GetShader(name), _shaderManager->GetShader(std::string(name) + "B32"));
		}
	}
	const auto found = _bonesVariants32.find(program);
	return found != _bonesVariants32.end() ? found->second : program;
}

const graphics::ShaderProgram* Renderer::StaticVariant(const graphics::ShaderProgram* program) const
{
	if (_staticVariants.empty())
	{
		for (const auto* name : {"ObjectInstanced", "ObjectHeightMapInstanced", "ObjectShadowInstanced", "ObjectHeightMapShadowInstanced"})
		{
			_staticVariants.emplace(_shaderManager->GetShader(name), _shaderManager->GetShader(std::string(name) + "Static"));
		}
	}
	const auto found = _staticVariants.find(program);
	return found != _staticVariants.end() ? found->second : program;
}

void Renderer::DrawMesh(const graphics::L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept
{
	if (mesh.GetNumSubMeshes() == 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Mesh {} has no submeshes to draw", mesh.GetDebugName());
		return;
	}

	const auto& subMeshes = mesh.GetSubMeshes();

	if (subMeshIndex != std::numeric_limits<uint8_t>::max())
	{
		if (subMeshIndex >= mesh.GetNumSubMeshes())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "tried to draw submesh out of range ({}/{})", subMeshIndex,
			                   mesh.GetNumSubMeshes());
		}

		DrawSubMesh(mesh, *subMeshes[subMeshIndex], desc, false);
		return;
	}

	for (auto it = subMeshes.begin(); it != subMeshes.end(); ++it)
	{
		const L3DSubMesh& subMesh = **it;
		DrawSubMesh(mesh, subMesh, desc, std::next(it) != subMeshes.end());
	}
}

void Renderer::DrawFootprintPass(const DrawSceneDesc& drawDesc) const
{
	const auto viewId = graphics::RenderPass::Footprint;
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::FootprintPass);
	if (drawDesc.drawIsland)
	{
		const auto& island = Locator::terrainSystem::value();
		island.GetFootprintFramebuffer().Bind(viewId);

		// This dummy draw call is here to make sure that view is cleared if no
		// other draw calls are submitted to view
		bgfx::touch(static_cast<bgfx::ViewId>(viewId));

		// _shaderManager->SetCamera(viewId, *drawDesc.camera); // TODO

		auto view = island.GetOrthoView();
		auto proj = island.GetOrthoProj();
		bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), &view, &proj);

		const auto& meshManager = Locator::resources::value().GetMeshes();
		const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
		const auto* footprintShaderInstanced = _shaderManager->GetShader("FootprintInstanced");
		for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
		{
			auto mesh = meshManager.Handle(meshId);
			if (!mesh->ContainsLandscapeFeature() || mesh->GetFootprints().empty())
			{
				continue;
			}
			const auto& footprint = mesh->GetFootprints()[0];
			footprintShaderInstanced->SetTextureSampler("s_footprint", 0, *footprint.texture);
			footprint.mesh->GetVertexBuffer().Bind();
			bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), placers.offset, placers.count);
			const uint64_t state = 0u                       //
			                       | BGFX_STATE_WRITE_RGB   //
			                       | BGFX_STATE_WRITE_A     //
			                       | BGFX_STATE_BLEND_ALPHA //
			                       | BGFX_STATE_CULL_CW     //
			                       | BGFX_STATE_MSAA;
			bgfx::setState(state);
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(footprintShaderInstanced->GetRawHandle()));
		}
		DrawRiverFootprints(static_cast<bgfx::ViewId>(viewId), false);
		// mod world.foliage, fields = wheat: tilled soil under the crop fields
		const auto& config = Locator::config::value();
		if (_foliage && config.foliageFields && config.foliageDensity > 0.0f)
		{
			_foliage->DrawFieldFootprints(static_cast<bgfx::ViewId>(viewId), *footprintShaderInstanced);
		}
	}
}

void Renderer::DrawRiverFootprints(bgfx::ViewId viewId, bool channel) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = entt::hashed_string(channel ? "river" : "river2");
	if (!meshes.Contains(meshId))
	{
		return;
	}
	const auto mesh = meshes.Handle(meshId);
	if (mesh->GetFootprints().empty())
	{
		return;
	}
	std::vector<glm::mat4> matrices;
	Locator::entitiesRegistry::value().Each<const ecs::components::StreamFootprint, const ecs::components::Transform>(
	    [&matrices, channel](const ecs::components::StreamFootprint& footprint, const ecs::components::Transform& transform) {
		    if (footprint.channel == channel)
		    {
			    matrices.push_back(lh_matrix::Model(transform));
		    }
	    });
	const auto count = static_cast<uint32_t>(matrices.size());
	constexpr uint16_t k_Stride = sizeof(glm::mat4);
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
	std::memcpy(instances.data, matrices.data(), matrices.size() * sizeof(glm::mat4));

	const auto& footprint = mesh->GetFootprints()[0];
	const auto* program = _shaderManager->GetShader(channel ? "LandAlphaInstanced" : "FootprintInstanced");
	program->SetTextureSampler("s_footprint", 0, *footprint.texture);
	if (channel)
	{
		const auto size = footprint.texture->GetResolution();
		const glm::vec4 u_footprintSize(size.x, size.y, 0.0f, 0.0f);
		program->SetUniformValue("u_footprintSize", &u_footprintSize);
	}
	footprint.mesh->GetVertexBuffer().Bind();
	bgfx::setInstanceDataBuffer(&instances);
	// the bed blends its colour like any footprint (fn_008728A0); the channel keeps the lowest alpha (fn_00872AB0)
	const uint64_t state = channel ? BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
	                                     BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MIN)
	                               : BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
	bgfx::setState(state);
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawLandAlphaPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland)
	{
		return;
	}
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::LandAlpha);
	const auto& island = Locator::terrainSystem::value();
	const auto& frameBuffer = island.GetLandAlphaFramebuffer();
	frameBuffer.Bind(graphics::RenderPass::LandAlpha);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0xFFFFFFFF);
	bgfx::setViewRect(viewId, 0, 0, frameBuffer.GetColorAttachment().GetResolution().x,
	                  frameBuffer.GetColorAttachment().GetResolution().y);
	bgfx::touch(viewId);
	const auto view = island.GetOrthoView();
	const auto proj = island.GetOrthoProj();
	bgfx::setViewTransform(viewId, &view, &proj);
	DrawRiverFootprints(viewId, true);
}

void Renderer::UpdateLandLight() const
{
	if (!_landLight)
	{
		_landLight = std::make_unique<LandLightTable>();
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			const auto path = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "palette.raw";
			if (!_landLight->Load(fileSystem.ReadAll(fileSystem.FindPath(path))))
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "palette.raw has an unexpected size");
			}
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No landscape light table (palette.raw): {}", e.what());
		}
		_landLightTexture = bgfx::createTexture2D(LandLightTable::k_Size, 1, false, 1, bgfx::TextureFormat::RGBA8,
		                                          BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
		bgfx::setName(_landLightTexture, "LandLightTable");
	}
	if (!_landLight->IsLoaded())
	{
		return;
	}
	// The overcast at the camera caps the base colour (Clouds::WeatherOvercastAtCamera, [0xFA2754]); the lightning flash
	// at the camera lerps the table to white ([0xFA2768], sky_weather::LightningFlash -> weather::LightningFlashAtCamera)
	// with this frame's sky type: fn_00869850 runs from fn_0086A330 right after fn_0086A2C0 (DrawSky 0x5E2226..0x5E222B), so
	// its Time2SkyType([0xFA26C4]) (0x869859) is [0xFA26BC] = sky_type::Frame(), the value its haze reads (0x869D5F)
	_landLight->Build(sky_type::Frame(), _skyAlignment.Get(),
	                  Clouds::WeatherOvercastAtCamera(), sky_weather::LightningFlash());
	const auto& texels = _landLight->GetTexels();
	bgfx::updateTexture2D(_landLightTexture, 0, 0, 0, 0, LandLightTable::k_Size, 1,
	                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(texels[0]))));
}

void Renderer::DrawStaticShadowPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !drawDesc.drawEntities)
	{
		return;
	}
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::StaticShadow);
	const auto& island = Locator::terrainSystem::value();
	const auto& frameBuffer = island.GetStaticShadowFramebuffer();
	frameBuffer.Bind(graphics::RenderPass::StaticShadow);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::setViewRect(viewId, 0, 0, frameBuffer.GetColorAttachment().GetResolution().x,
	                  frameBuffer.GetColorAttachment().GetResolution().y);
	bgfx::touch(viewId);
	const auto view = island.GetOrthoView();
	const auto proj = island.GetOrthoProj();
	bgfx::setViewTransform(viewId, &view, &proj);

	const auto& meshManager = Locator::resources::value().GetMeshes();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto* boned = _shaderManager->GetShader("StaticShadowInstanced");
	const auto* single = _shaderManager->GetShader("StaticShadowInstancedStatic");
	constexpr uint64_t k_State = BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
	                             BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
	for (const auto& [meshId, placers] : renderCtx.shadowCasterDrawDescs)
	{
		const auto mesh = meshManager.Handle(meshId);
		const auto* program = mesh->IsBoned() ? boned : single;
		const glm::mat4 identity(1.0f);
		const auto* matrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &identity;
		const auto matrixCount = mesh->IsBoned() ? static_cast<uint16_t>(mesh->GetBoneMatrices().size()) : uint16_t {1};
		for (const auto& subMesh : mesh->GetSubMeshes())
		{
			// LOD 0 only, like fn_00806DA0
			if (subMesh->IsPhysics() || subMesh->GetFlags().status != 0 || (subMesh->GetFlags().lodMask & 1) != 1)
			{
				continue;
			}
			for (const auto& prim : subMesh->GetPrimitives())
			{
				const auto* texture = world_triangles::PrimitiveTexture(*mesh, prim.skinID);
				// x: ALPHAREF / 255 of the primitive's mode, -1 without alpha test (inferido: the normal table)
				const auto alpha = render_modes::PrimitiveAlpha(
				    static_cast<render_modes::Mode>(prim.materialType), render_modes::Table::Normal,
				    static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)));
				const glm::vec4 u_shadowParams = {alpha.ref, texture != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f};
				program->SetUniformValue("u_shadowParams", &u_shadowParams);
				if (texture != nullptr)
				{
					program->SetTextureSampler("s_diffuse", 0, *texture);
				}
				bgfx::setTransform(matrices, matrixCount);
				bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), placers.offset, placers.count);
				if (subMesh->GetMesh().IsIndexed())
				{
					subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
				}
				subMesh->GetMesh().GetVertexBuffer().Bind();
				bgfx::setState(k_State);
				bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
			}
		}
	}
}

void Renderer::DrawCelestialMesh(graphics::RenderPass viewId, const L3DMesh& mesh, const glm::mat4& model,
                                 const Texture2D& texture, const glm::vec4& colour, uint64_t state, const glm::vec4& celestial,
                                 const Texture2D* alpha) const
{
	const auto* program = _shaderManager->GetShader("Celestial");
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		for (const auto& prim : subMesh->GetPrimitives())
		{
			bgfx::setTransform(&model);
			program->SetTextureSampler("s_diffuse", 0, texture);
			program->SetUniformValue("u_colour", &colour);
			program->SetUniformValue("u_celestial", &celestial);
			program->SetTextureSampler("s_alpha", 1, alpha != nullptr ? *alpha : texture);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			bgfx::setState(state);
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}

void Renderer::DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare) const
{
	const auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_SunTexture = entt::hashed_string("raw/sun");
	if (!textures.Contains(k_SunTexture))
	{
		return;
	}
	// fn_0086C020: height by game hour T, alpha A [0xC395A8] fading in 3..6 h and out 18..21 h
	const float t = sky.GetTime();
	const float height = 7500.0f * (std::clamp(std::min(t, 24.0f - t), 6.0f, 12.0f) - 6.0f) / 6.0f;
	float alpha = 255.0f;
	if (t < 3.0f || t > 21.0f)
	{
		alpha = 0.0f;
	}
	else if (t < 6.0f)
	{
		alpha = (t - 3.0f) * 85.0f;
	}
	else if (t > 18.0f)
	{
		alpha = 255.0f - (t - 18.0f) * 85.0f;
	}
	if (alpha <= 0.0f)
	{
		return;
	}
	// A vertical quad at (-30000, y, -30000) turned by 3*pi/4 about Y, i.e. facing the island
	const glm::vec3 position(-30000.0f, height, -30000.0f);
	// (inferido) fn_0086C020's turn (+3 pi / 4 in the original's sense = glm's -3 pi / 4) is not read
	auto model = glm::translate(position) * glm::rotate(-3.0f * glm::pi<float>() / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto& texture = *textures.Handle(k_SunTexture);
	// mode 13: additive SRCALPHA / ONE, colour and alpha = texture x diffuse, no Z write, cull none; the glare with
	// ZFUNC ALWAYS (fn_0086BB60)
	const uint64_t additive =
	    render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNz, {.zFunc = render_modes::ZFunc::Always});
	if (!glare)
	{
		const glm::vec4 colour(glm::vec3(0x95, 0x7C, 0x63) / 255.0f, alpha / 255.0f);
		DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour,
		                  render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNz));
		return;
	}

	// Glare (fn_0086BB60 / fn_0086BD00): 5 samples around the sun, each hidden when the landscape is in the way; the
	// visibility eases toward (1 - 0.2 * hidden) * 255 by 1 % per ms
	int hidden = 0;
	const auto origin = camera.GetOrigin();
	const auto& island = Locator::terrainSystem::value();
	const auto right = glm::vec3(model * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	for (const auto& offset : {glm::vec2(0.0f), glm::vec2(500.0f, 500.0f), glm::vec2(-500.0f, 500.0f),
	                           glm::vec2(500.0f, -500.0f), glm::vec2(-500.0f, -500.0f)})
	{
		auto sample = position + right * offset.x + glm::vec3(0.0f, offset.y, 0.0f);
		sample.y = std::max(sample.y, 10.0f);
		const auto direction = sample - origin;
		// march over the island (the landscape is at most a few thousand units across)
		constexpr int k_Steps = 256;
		for (int i = 1; i <= k_Steps; ++i)
		{
			const auto p = origin + direction * (static_cast<float>(i) / k_Steps * 0.25f);
			if (p.y < island.GetHeightAt(glm::vec2(p.x, p.z)))
			{
				++hidden;
				break;
			}
		}
	}
	static auto lastTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float milliseconds = std::chrono::duration<float, std::milli>(now - lastTime).count();
	lastTime = now;
	const float target = (1.0f - 0.2f * static_cast<float>(hidden)) * 255.0f;
	_sunGlare = std::clamp(_sunGlare + (target - _sunGlare) * std::min(1.0f, milliseconds * 0.01f), 0.0f, 255.0f);
	if (_sunGlare <= 0.0f)
	{
		return;
	}
	model = model * glm::scale(glm::vec3(1.8f));
	const glm::vec4 colour(glm::vec3(0xA0, 0x6A, 0x35) / 255.0f, _sunGlare * alpha / 255.0f / 255.0f);
	DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour, additive);
}

void Renderer::DrawMoon(graphics::RenderPass viewId, const Camera& camera) const
{
	const auto pass = sea_pass::ForPass(viewId);
	const bool mirrored = pass.mirrored;
	const auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Weather = entt::hashed_string("raw/weather");
	static const auto k_WeatherAlpha = entt::hashed_string("raw/weathera");
	static const auto k_Atmos = entt::hashed_string("raw/ATMOS");
	static const auto k_AtmosAlpha = entt::hashed_string("raw/ATMOSA");
	if (!textures.Contains(k_Weather) || !textures.Contains(k_WeatherAlpha) || !textures.Contains(k_Atmos) ||
	    !textures.Contains(k_AtmosAlpha))
	{
		return;
	}
	// Position relative to the camera, by game hour T; alpha m = min(200, 0.5 y - 110), so it shows about +-4.7 h
	// around midnight
	const float theta = sky.GetTime() * glm::pi<float>() / 12.0f;
	const glm::vec3 offset(4000.0f, 1100.0f * std::cos(theta) - 150.0f, 800.0f * std::sin(theta));
	const float m = std::min(200.0f, std::floor(0.5f * offset.y - 110.0f));
	if (m <= 0.0f)
	{
		return;
	}
	// (the reflection camera has the main camera's origin)
	const auto centre = camera.GetOrigin() + offset;
	const auto frame = billboard::CameraFrame::From(camera);
	// fn_0086B010 0x86B61D: fn_0086AC60 + fn_0086A930 twice, the second time at (x, -y, z) with [0xFA2774] = 1 (0x86B662..
	// 0x86B69C). That second call only draws its glow (fn_0086A930 0x86AC0F skips the moon object): drawn here with the
	// mirrored camera at (x, y, z), it is that glow, built from the mirrored view (billboard::MoonBasis). The moon in the
	// reflection is the first call's DrawUnderWater (vt+0x118, 0x86AC46), its matrix mirrored in y = 0: drawn with the
	// mirrored camera, that is the main camera's moon matrix itself (the mirrored view x mirror(y) is the main view,
	// ReflectionXZCamera::GetViewMatrix), so the mesh comes out flipped and its winding with it.
	const auto colour = _landLight && _landLight->IsLoaded() ? _landLight->GetMoonColour() : glm::vec3(1.0f);

	// Glow (fn_0086A930): a 4000 x 4000 quad on the fn_0086AC60 basis (billboard::MoonHalo), atmos.raw UV
	// 0.25..0.49375, additive (mode 13), colour (R/6, G/5, B/4, m)
	{
		struct Vertex
		{
			float x, y, z, u, v;
		};
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		if (bgfx::getAvailTransientVertexBuffer(6, layout) == 6)
		{
			bgfx::TransientVertexBuffer buffer;
			bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
			auto* vertices = reinterpret_cast<Vertex*>(buffer.data);
			const auto halo = billboard::MoonHalo(billboard::MoonBasis(frame.view, frame.inverseView, centre), centre);
			for (size_t i = 0; i < billboard::k_MoonHaloTriangles.size(); ++i)
			{
				const auto k = static_cast<size_t>(billboard::k_MoonHaloTriangles.at(i));
				const auto& p = halo.corners.at(k);
				vertices[i] = {p.x, p.y, p.z, halo.uv.at(k).x, halo.uv.at(k).y};
			}
			const auto* program = _shaderManager->GetShader("Celestial");
			const glm::mat4 identity(1.0f);
			const glm::vec4 glowColour(colour.r / 6.0f, colour.g / 5.0f, colour.b / 4.0f, m / 255.0f);
			const glm::vec4 celestial(0.0f, 0.0f, 0.0f, 1.0f);
			bgfx::setTransform(&identity);
			program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Atmos));
			program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AtmosAlpha));
			program->SetUniformValue("u_colour", &glowColour);
			program->SetUniformValue("u_celestial", &celestial);
			bgfx::setVertexBuffer(0, &buffer);
			bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNz));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}

	// The moon (mode 4: SRCALPHA / INVSRCALPHA): the fn_0086AC60 basis x4, fn_0086AFA0's tilt, RotateY(phase + pi), x0.65
	// (billboard::MoonModel). The phase follows the real clock: 2 pi (1 - frac((days since 1970 - 10962) / 29.5306))
	const auto days = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
	                                          std::chrono::system_clock::now().time_since_epoch())
	                                          .count()) /
	                  86400.0;
	const double cycles = (days - 10962.0) / 29.5306;
	const auto phase = static_cast<float>(2.0 * glm::pi<double>() * (1.0 - (cycles - std::floor(cycles))));
	const auto mainView = mirrored ? sea_pass::UnmirrorView(frame.view) : frame.view;
	const auto mainInverseView = mirrored ? glm::inverse(mainView) : frame.inverseView;
	const auto model = billboard::MoonModel(billboard::MoonBasis(mainView, mainInverseView, centre), centre, phase);
	const glm::vec4 moonColour(colour, m / 255.0f);
	const glm::vec4 celestial(std::cos(phase), std::sin(phase), 1.0f, 1.0f);
	// (inferido) without the Z write of mode 4 (0x82DC20): nothing farther is drawn after it in the sky.
	// The moon object gets Flags1 0x80 (vt+0x48(1) = fn_007F9D60) before its Draw (0x86AC05) and before its
	// DrawUnderWater (0x86AC3B), so both draw through the table 0xC387C8: mode 4 -> 5 (0x82DD90, the same blend and Z
	// write, ALPHAOP MODULATE(TEXTURE, DIFFUSE)). fs_celestial always modulates the alpha by u_colour, so the state of
	// mode 4 here already draws as mode 5, in both passes
	DrawCelestialMesh(viewId, sky.GetMoonMesh(), model, *textures.Handle(k_Weather), moonColour,
	                  render_modes::State(render_modes::Mode::AlphaTextured,
	                                      {.cull = pass.FaceCull(sea_pass::Surface::Model, false, false), .zWrite = false}),
	                  celestial, &*textures.Handle(k_WeatherAlpha));
}

void Renderer::UpdateClouds() const
{
	const auto& detail = GetDetailLevel(Locator::config::value().detailLevel);
	// GLandscape::Open -> CloudInSky::Open: a new layout for every land
	if (!_clouds || _cloudsGeneration != Clouds::GetLandscapeGeneration())
	{
		_clouds = std::make_unique<Clouds>();
		_cloudsGeneration = Clouds::GetLandscapeGeneration();
	}
	if (_cloudShadowImage.empty())
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			_cloudShadowImage =
			    fileSystem.ReadAll(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "sclouds.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No cloud shadows (sclouds.raw): {}", e.what());
		}
	}
	// g_game_time_inc [0xEA9EC0] (game_clock::FrameGameMs): the game ms of this frame, whole, 0 while paused, faster
	// or slower with the game speed. The clouds and their animation stop while the game is paused. Its readers here:
	// DrawSky 0x5E2160 (the sky's alignment), fn_005E25C0 0x5E25FD (the clouds), and for the night lights
	// fn_00823460 0x8234B6 (the jitter) and fn_00823570 0x82359F (the flames)
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	const bool running = Game::Instance() != nullptr && !Game::Instance()->IsPaused();
	if (running)
	{
		_clouds->Update(milliseconds);
	}
	// CollectClouds advances the animation counters of the clouds it queues by this step
	_cloudMilliseconds = running ? milliseconds : 0.0f;
	// GLandAlignement::DrawSky 0x5E2160: the sky's alignment moves towards the most influential player's
	_skyAlignment.Update(Clouds::InfluentialPlayerAlignment(), running ? milliseconds : 0.0f);

	// fn_005E1DE0 (called by DrawSky): the colour and the alpha byte from the sky's alignment and light table[255]
	const uint32_t table255 = _landLight && _landLight->IsLoaded() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	const uint32_t colour = Clouds::Colour(_skyAlignment.Get(), table255);
	_cloudRgb = lh3d_colour::ToVec3(colour);
	const auto alignAlpha = static_cast<int>(colour >> 24);
	_cloudAlpha.resize(_clouds->GetClouds().size());
	for (size_t i = 0; i < _cloudAlpha.size(); ++i)
	{
		// fn_005E25C0: alpha = edge * A / 255 in integers (0x80808081), drawn only when it is not 0
		_cloudAlpha[i] =
		    detail.clouds ? static_cast<float>(Clouds::EdgeAlpha(_clouds->GetClouds()[i]) * alignAlpha / 255) : 0.0f;
	}

	// This frame's land cells (land_light, the cell map's layout): the loaded ones back (ClearLight fn_0086D460), then
	// fn_005E5830: the map clouds' stamps (fn_005E25C0 0x5E2800, "CloudShadows"), fn_0086D360 0x5E592F with every
	// stamp of this frame in the list 0xFA2920 (PSys light maps, the storms, the flashes, the fires), then the hand's
	// and the village lights (fn_008229B0)
	if (!Locator::terrainSystem::has_value())
	{
		land_light::ClearStamps();
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto size = island.GetCellMap().GetResolution();
	if (size != _landCellsSize)
	{
		if (bgfx::isValid(_landCellsTexture))
		{
			bgfx::destroy(_landCellsTexture);
		}
		_landCellsTexture = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                          BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
		_landCellsSize = size;
	}
	land_light::BeginFrame(island, Clouds::GetLandscapeGeneration());
	if (detail.clouds)
	{
		_clouds->StampShadows(_cloudShadowImage, _cloudAlpha);
	}
	land_light::ApplyStamps();
	land_light::ClearStamps();
	// Night lights (fn_005E5830): the hand light and the village lights into this frame's luminosities, after the
	// stamps: fn_008229B0 reads the cell's byte +3 and writes it directly (0x822D9D, 0x822DC3), no min with the loaded one
	if (_landLight && _landLight->IsLoaded() && Game::Instance() != nullptr)
	{
		auto luminosity = land_light::Luminosity();
		night_lights::LightCells cells;
		cells.firstCell = land_light::GetCells().firstCell;
		cells.size = glm::ivec2(size);
		cells.cap = &luminosity;
		cells.fullLightGreen = static_cast<uint8_t>((land_light::FullLight(*_landLight) >> 8) & 0xFFu); // [0xEDDD09]
		night_lights::Update(Game::Instance()->IsPaused() ? 0.0f : milliseconds,
		                     Game::Instance()->GetDayNightClock().GetScriptTime(), _landLight->GetBaseColour(), cells);
		land_light::SetLuminosity(luminosity);
	}
	const auto texels = land_light::Texels();
	if (texels.size() == static_cast<size_t>(size.x) * size.y * 4)
	{
		bgfx::updateTexture2D(_landCellsTexture, 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
	}
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectClouds(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	if (!_clouds || !GetDetailLevel(Locator::config::value().detailLevel).clouds)
	{
		return order;
	}
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	if (mesh.GetNumSubMeshes() == 0)
	{
		return order;
	}
	const auto origin = camera.GetOrigin();
	// LH3DMist::AddDrawing 0x7FA7F0 (vt+0x100 of the cloud objects, called by fn_005E25C0): only a cloud whose sphere
	// (the mesh's bounding-box half diagonal x size x 0.55) touches the screen is queued and advances its counter
	const float meshRadius = glm::length(mesh.GetBoundingBox().Size()) * 0.5f;
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	const float milliseconds = _cloudMilliseconds;
	_cloudMilliseconds = 0.0f;
	// mist.l3d is loaded without skins; LH3DMist::Draw (fn_007FA300) uses the smoke material instead
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Smoke = entt::hashed_string("raw/smoke");
	static const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return order;
	}
	order.reserve(_clouds->GetClouds().size());
	for (size_t index = 0; index < _clouds->GetClouds().size(); ++index)
	{
		const auto& cloud = _clouds->GetClouds()[index];
		const auto position = Clouds::WorldPosition(cloud);
		if (_cloudAlpha[index] / 255.0f <= 0.0f || !SphereInView(viewProjection, position, meshRadius * cloud.size * 0.55f))
		{
			continue;
		}
		_clouds->AdvanceAnimation(index, milliseconds);
		// the Z-sorter key: |+0x38 - g_camera|^2, (x^2 + y^2) + z^2 (0x7FA83C..0x7FA86B, NewZObject call 0x7FA87B)
		order.emplace_back(zsorter::Key(position, origin), static_cast<uint32_t>(index));
	}
	return order;
}

void Renderer::DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (!_clouds || index >= _clouds->GetClouds().size())
	{
		return;
	}
	const auto rgb = _cloudRgb;
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto origin = camera.GetOrigin();
	// the same billboard as the map mists (Renderer::DrawMist): 0xEA1C98 after its in-place inverse fn_007FB3F0
	// (0x819AF3), in glm mat3(right, -forward, up): local X = screen right, local Y (the dome's axis) towards the
	// camera, local Z = screen up (billboard::MistBasis)
	const auto cameraFrame = billboard::CameraFrame::From(camera);
	const auto& rotation = billboard::MistBasis(cameraFrame);
	// The clouds are LH3DMist objects too (+0x88 the size, +0x8C the shrink), so their draw is the effect branch of the
	// same fn_007FA300: no specular (+0x50 is never written) and the temporary light straight above.
	const glm::vec4 u_cloudSpecular(0.0f);
	const auto* program = _shaderManager->GetShader("Cloud");
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Smoke = entt::hashed_string("raw/smoke");
	static const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	const auto& cloud = _clouds->GetClouds()[index];
	const auto position = Clouds::WorldPosition(cloud);
	// 0x7FA4DC..0x7FA539: row 0 (local X, the screen width) is scaled by the size and rows 1-2 (local Y = depth,
	// local Z = screen height) by the shrunk one, so a cloud is round only straight overhead and near the horizon
	// it is about k (2.5 to 5, CloudInSky::Open 0x5E23F0) times wider than tall (billboard::MistShrunkSize)
	const float shrunk = billboard::MistShrunkSize(cloud.size, cloud.k, position - origin);
	const auto model = glm::translate(position) * glm::mat4(rotation) * glm::scale(glm::vec3(cloud.size, shrunk, shrunk));
	const glm::vec4 u_cloudColour(rgb, _cloudAlpha[index] / 255.0f);
	// fn_007FA300 0x7FA3F4..0x7FA466: one whole atlas cell, rows 2-3 (the frame after this frame's step;
	// frame_anim::MistCellUv of the effect branch)
	const auto cell = frame_anim::MistCellUv(Clouds::GetFrame(cloud), true);
	// fn_007FA300 0x7FA53C..0x7FA563: LH3DMist::Draw saves [0xEA9E90] and moves the light straight above with
	// fn_0081E1F0 while it draws the clouds, with the ambient at 210 (0x7FA56D); both go back at 0x7FA586 / 0x7FA590.
	// fn_00855340 (model_light::LightInMeshSpace) brings the light into the mesh's own space, normalised
	const model_light::ScopedLight cloudLight(glm::vec3(0.0f, 500000.0f, 0.0f));
	const model_light::ScopedAmbient cloudAmbient(model_light::k_MistAmbient);
	const glm::vec4 u_cloud(cell.x, cell.y, static_cast<float>(model_light::Ambient()) / 256.0f, 0.0f);
	const glm::vec4 u_cloudLight(model_light::LightInMeshSpace(model), 0.0f);
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		for (const auto& prim : subMesh->GetPrimitives())
		{
			bgfx::setTransform(&model);
			program->SetTextureSampler("s_diffuse", 0, smoke);
			program->SetTextureSampler("s_alpha", 1, smokeAlpha);
			program->SetUniformValue("u_cloud", &u_cloud);
			program->SetUniformValue("u_cloudColour", &u_cloudColour);
			program->SetUniformValue("u_cloudLight", &u_cloudLight);
			program->SetUniformValue("u_cloudSpecular", &u_cloudSpecular);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			// the smoke material [0xEA1ABC] (fn_007FA300): mode 6, two-sided
			bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}

void Renderer::DrawObjectReflections(graphics::RenderPass viewId) const
{
	if (!Locator::handSystem::has_value())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& hand = Locator::handSystem::value();
	// DrawUnderWater of the held object (CHand, after the hand, in its own colour) and of the physics objects
	// (fn_00646FE0, while not wholly under water: y > -r, r = the farthest vertex). "Own colour" is obj+0x4C / +0x50 as
	// the last Draw left them: the land light and cell specular of fn_00801C90 (PhysicsObject::DrawAll 0x646F9F)
	// The physics objects are the whole list 0xD47814 (thrown, knocked, the resting proxies); r is the body's radius
	// (PhysOb: the farthest vertex). The hand's thrown objects that are not in the physics keep the bounding box radius.
	struct Reflected
	{
		entt::entity entity;
		float radius; ///< < 0: always drawn (the held object); 0: from the bounding box
		float centreY;
	};
	std::vector<Reflected> objects;
	if (const auto held = hand.GetHeldObject(); held.has_value())
	{
		objects.push_back({*held, -1.0f, 0.0f});
	}
	ecs::physics::PhysicsObjects::ForEach([&objects](const ecs::physics::PhysicsObject& po) {
		if (po.entity != entt::null)
		{
			objects.push_back({po.entity, po.body.Radius(), po.body.Centre().y});
		}
	});
	for (const auto entity : hand.GetThrownObjects())
	{
		if (ecs::physics::PhysicsObjects::Find(entity) == nullptr)
		{
			objects.push_back({entity, 0.0f, 0.0f});
		}
	}
	for (const auto& [entity, bodyRadius, centreY] : objects)
	{
		const auto instance = renderCtx.entityInstances.find(entity);
		if (!registry.Valid(entity) || instance == renderCtx.entityInstances.end() || !meshes.Contains(instance->second.meshId))
		{
			continue;
		}
		if (bodyRadius >= 0.0f)
		{
			const auto mesh = meshes.Handle(instance->second.meshId);
			const auto& transform = registry.Get<ecs::components::Transform>(entity);
			const float radius =
			    bodyRadius > 0.0f ? bodyRadius : 0.5f * glm::length(mesh->GetBoundingBox().Size()) * transform.scale.x;
			if ((bodyRadius > 0.0f ? centreY : transform.position.y) <= -radius)
			{
				continue;
			}
		}
		// vt+0x118 in the colour the last Draw left (0x5E49A2 the held object, 0x6470C6 / 0x64717C the physics objects)
		DrawUnderWater(viewId, entity, sea_pass::UnderWaterLastDraw());
	}
}

void Renderer::DrawFishShoals(graphics::RenderPass viewId) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Texture = entt::hashed_string("raw/misc0");
	static const auto k_Alpha = entt::hashed_string("raw/misc0a");
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	Locator::entitiesRegistry::value().Each<const ecs::components::FishFarm>([&vertices](const ecs::components::FishFarm& farm) {
		if (!farm.shoal.has_value() || !farm.shoal->visible)
		{
			return;
		}
		const uint32_t colour = (static_cast<uint32_t>(farm.shoal->alpha) << 24) | 0x00FFFFFFu;
		for (size_t i = 0; i < std::min(farm.shoal->shown, farm.shoal->fish.size()); ++i)
		{
			const auto& fish = farm.shoal->fish[i];
			// LH3DSprite::Draw 0x840530 with flag 0x40 (fn_00824740 0x8247EF): a flat quad turned about Y, its local x
			// along the heading (billboard::Horizontal); cells 8..23 of the 8 x 8 sheet
			billboard::Sprite sprite;
			// drawn before the sea without a mirror (fn_00824740, 0x5E4B2B): mirrored back in the reflection target (see
			// DrawPass; on the CPU, vs_blob is shared by 6 programs)
			sprite.position = sea_pass::Unmirror(fish.position);
			sprite.size = fish.halfSize;
			sprite.angle = fish.heading;
			sprite.cell = fish.cell; // fn_008248E0 0x824993..0x8249A4, taken before the frame's wrap (frame_anim::FishFrame)
			sprite.horizontal = true;
			const auto quad = billboard::Horizontal(sprite);
			for (const int k : billboard::k_SpriteTriangles)
			{
				const auto& p = quad.corners.at(static_cast<size_t>(k));
				const auto& uv = quad.uv.at(static_cast<size_t>(k));
				vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, colour});
			}
		}
	});
	if (vertices.empty())
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// misc0 [0xEA1AB0] (inferido: reference 0x8247CC, not decoded), mode 6: SRCALPHA / INVSRCALPHA, no Z write;
	// two-sided. ZFUNC ALWAYS (aproximado: the original keeps LESSEQUAL 0x82CCC5; equivalent because the mirrored land
	// under them wrote no Z, 0x5E48C5)
	bgfx::setState(render_modes::State(render_modes::materials::k_Misc0, {.zFunc = render_modes::ZFunc::Always}));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::DrawFoliage(const DrawSceneDesc& desc) const
{
	const auto& config = Locator::config::value();
	if (config.foliageDensity <= 0.0f || !Locator::terrainSystem::has_value())
	{
		return;
	}
	// loaded again when a module of the mod is turned on or off
	if (!Locator::mods::has_value())
	{
		return;
	}
	const auto& mods = Locator::mods::value();
	// and when a module's density option changes (as the mod's own: low 0.5, medium 1, high 2, very high 4)
	std::vector<std::filesystem::path> modules;
	std::vector<float> moduleDensities;
	std::string loadKey = "loaded";
	for (const auto& module : mods.GetModules("world.foliage"))
	{
		const auto found = module.options.find("density");
		const std::string density = found == module.options.end() ? "" : found->second;
		modules.push_back(module.directory);
		moduleDensities.push_back(density == "very low" ? 0.25f
		                          : density == "low"    ? 0.5f
		                          : density == "high"   ? 2.0f
		                          : density == "very high" ? 4.0f
		                                                   : 1.0f);
		loadKey += "|" + module.directory.generic_string() + ":" + density;
	}
	if (loadKey != _foliageLoadKey)
	{
		_foliageLoadKey = loadKey;
		_foliage.reset();
		auto foliage = std::make_unique<Foliage>();
		if (foliage->Load(mods.GetModFilesDirectory("world.foliage"), modules, moduleDensities))
		{
			_foliage = std::move(foliage);
		}
	}
	if (!_foliage)
	{
		return;
	}
	auto& island = Locator::terrainSystem::value();
	_foliage->Update(island, config.foliageDensity, desc.camera->GetOrigin(), config.foliageDistance, config.foliageFields);
	const float seconds = static_cast<float>(SDL_GetTicks()) / 1000.0f;
	_foliage->UpdateFlyers(island, desc.camera->GetOrigin(), config.foliageDistance, seconds);
	Foliage::DrawDesc foliageDesc {};
	foliageDesc.viewId = static_cast<bgfx::ViewId>(desc.viewId);
	foliageDesc.program = _shaderManager->GetShader("Foliage");
	foliageDesc.cameraPosition = desc.camera->GetOrigin();
	foliageDesc.distance = config.foliageDistance;
	foliageDesc.landLight = _landLightTexture;
	foliageDesc.materials = &island.GetAlbedoArray();
	foliageDesc.materialRepeats = config.terrainTextureDensity;
	foliageDesc.haze = _hazeUniforms[0];
	foliageDesc.hazeColour = _hazeUniforms[1];
	foliageDesc.alphaToCoverage = config.msaa != 0;
	foliageDesc.seconds = seconds;
	_foliage->Draw(foliageDesc);
}

void Renderer::DrawWaterRings(graphics::RenderPass viewId) const
{
	const auto& rings = ecs::GetWaterRings();
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Texture = entt::hashed_string("raw/smoke");
	static const auto k_Alpha = entt::hashed_string("raw/smokea");
	if (rings.empty() || !textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	vertices.reserve(rings.size() * 6);
	for (const auto& ring : rings)
	{
		// half size max(age * growth / 700, 0.0001), the z half size x aspect; alpha (255 - 0.364286 age) * A >> 8
		const float half = std::max(static_cast<float>(ring.age) * ring.growth * 0.00142857f, 0.0001f);
		const auto alpha = static_cast<uint32_t>(static_cast<int>((255.0f - static_cast<float>(ring.age % 700) * 0.364286f) *
		                                                          static_cast<float>(ring.argb >> 24)) >> 8) & 0xFFu;
		// +0x34 as the creator left it (the light colour was fixed at creation, ecs::AddWaterRing)
		const uint32_t abgr = lh3d_colour::ToAbgr(ring.argb, alpha);
		// LH3DSprite flag 0x40 (GWater::InitialiseCircles 0x54BA84): a flat quad turned about Y (billboard::Horizontal),
		// the z half size x the aspect (+0x10)
		billboard::Sprite sprite;
		sprite.position = ring.position;
		sprite.size = half;
		sprite.height = ring.aspect;
		sprite.angle = ring.angle;
		sprite.cell = frame_anim::SpriteCell(ring.cell); // fixed at creation (fn_005E5100)
		sprite.horizontal = true;
		const auto quad = billboard::Horizontal(sprite);
		for (const int k : billboard::k_SpriteTriangles)
		{
			const auto& p = quad.corners.at(static_cast<size_t>(k));
			const auto& uv = quad.uv.at(static_cast<size_t>(k));
			vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
		}
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// smoke.raw in mode 13 [0xEA1AC4] (inferido: reference 0x54BA72, not decoded): SRCALPHA / ONE, no Z write
	bgfx::setState(render_modes::State(render_modes::materials::k_SmokeAdditive));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::DrawHumanShadows(graphics::RenderPass viewId) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Texture = entt::hashed_string("raw/human_shadow");
	if (!textures.Contains(k_Texture) || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	// fn_0081FFF0: each point on the ground + [0xEAA3C4] (land_morph, algorithm D)
	const auto ground = land_morph::Altitude(island);
	const auto& meshes = Locator::resources::value().GetMeshes();
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	// fn_0081FAA0 constants: half width U = 0.2 * norm(1, 0, -1), the light offset O = 2 * norm(1, 0, 1)
	const glm::vec3 u(0.14142136f, 0.0f, -0.14142136f);
	const glm::vec3 w = -u;
	const glm::vec3 o(1.41421356f, 0.0f, 1.41421356f);
	const auto addQuad = [&vertices, &u, &w](const glm::vec3& c, const glm::vec3& v) {
		// fn_0081FE50: v0 = C - 0.02V + U, v1 = C - 0.02V + W, v2 = C + V + W, v3 = C + V + U; opaque at the feet
		const std::array<glm::vec3, 4> p = {c - 0.02f * v + u, c - 0.02f * v + w, c + v + w, c + v + u};
		const std::array<glm::vec2, 4> uv = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)};
		const std::array<uint32_t, 4> colour = {0xFFFFFFFFu, 0xFFFFFFFFu, 0x00FFFFFFu, 0x00FFFFFFu};
		for (const int i : {0, 1, 2, 0, 2, 3})
		{
			vertices.push_back({p[i].x, p[i].y, p[i].z, uv[i].x, uv[i].y, colour[i]});
		}
	};
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const ecs::components::Villager, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Villager&, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    // none for villagers in the water (y <= 0.2)
		    if (transform.position.y <= 0.2f || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    // the feet of the drawn pose (ecs/Animations.h) where the villager is drawn (ecs/MobileDrawing.h)
		    const auto* animation = registry.TryGet<const ecs::components::SkeletalAnimation>(entity);
		    const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                            ? animation->pose
		                            : l3d->GetBoneMatrices();
		    if (bones.size() <= 21)
		    {
			    return;
		    }
		    const auto* draw = registry.TryGet<const ecs::components::DrawPosition>(entity);
		    // the two feet: bone matrix slots 21 and 18 (ends of the leg chains), on the ground + 0.2; in the physics, the
		    // drawn pose between its last two turns (ECS/Physics)
		    const auto* flying = registry.TryGet<const ecs::components::PhysicsDrawPose>(entity);
		    auto model = flying != nullptr ? lh_matrix::Model(flying->position, flying->rotation, transform.scale)
		                                   : lh_matrix::Model(draw != nullptr ? draw->position : transform.position,
		                                                      draw != nullptr ? draw->rotation : transform.rotation, transform.scale);
		    const auto foot = [&](size_t bone) {
			    auto p = glm::vec3(model * bones[bone] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			    p.y = land_morph::OnGround(ground, glm::vec2(p.x, p.z), land_morph::k_BlobLift);
			    return p;
		    };
		    const auto a = foot(21);
		    const auto b = foot(18);
		    // the light offset projected onto the land's plane: D = O s - ((O s) . n) n, n = LH3DIsland::GetNormal 0x803630
		    // (fn_00812170 0x8126FD)
		    const auto n = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto os = o * transform.scale.x;
		    const auto d = os - glm::dot(os, n) * n;
		    addQuad(a, d + (b - a) * 0.5f);
		    addQuad(b, d + (a - b) * 0.5f);
	    });
	// Animals (IsHumanShadowed, flag 0x4000000): the points of their mesh's EBone block, 2 or 4 quads. The original passes
	// the first quad of each pair V = D (it builds D + (P1 - P0) / 2 but hands over &D); the second gets D + (P0 - P1) / 2.
	registry.Each<const ecs::components::Animal, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Animal& animal, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    if (!animal.humanShadowed || transform.position.y <= 0.2f || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    const auto& points = l3d->GetBlobPoints();
		    const auto& bones = l3d->GetBoneMatrices();
		    if (points.empty())
		    {
			    return;
		    }
		    // in the physics, the drawn pose between its last two turns (ECS/Physics)
		    const auto* flying = registry.TryGet<const ecs::components::PhysicsDrawPose>(entity);
		    const auto model = flying != nullptr ? lh_matrix::Model(flying->position, flying->rotation, transform.scale)
		                                         : lh_matrix::Model(transform);
		    // LH3DIsland::GetNormal 0x803630 (fn_00812170 0x812859)
		    const auto n = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto os = o * transform.scale.x;
		    const auto d = os - glm::dot(os, n) * n;
		    std::array<glm::vec3, 4> p {};
		    for (size_t k = 0; k < points.size(); ++k)
		    {
			    const auto& [bone, position] = points[k];
			    const auto boneMatrix = bone < bones.size() ? bones[bone] : glm::mat4(1.0f);
			    p[k] = glm::vec3(model * boneMatrix * glm::vec4(position, 1.0f));
			    p[k].y = land_morph::OnGround(ground, glm::vec2(p[k].x, p[k].z), land_morph::k_BlobLift);
		    }
		    for (size_t k = 0; k + 1 < points.size(); k += 2)
		    {
			    addQuad(p[k], d);
			    addQuad(p[k + 1], d + (p[k] - p[k + 1]) * 0.5f);
		    }
	    });
	if (vertices.empty())
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("Blob");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	bgfx::setVertexBuffer(0, &buffer);
	// mode 6 ([0xEB998C], fn_0081FAA0 0x81FD42), no Z write, cull none
	bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaNz));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::DrawScene(const DrawSceneDesc& drawDesc) const noexcept
{
	// Process3dEngine 0x54DD5E..0x54DD7D: with the full screen film, alpha == 1.0 and not the falling spell's film, the
	// 3D world is not drawn (0x54DD7D jumps to 0x54E2A4); the film, the script fade and HelpSystem::Draw3D's bars still
	// are (0x54E2D7..0x54E2ED), in FinishFrame's order: bars, film, fade (DrawFinishFrameOverlays). (aproximado) the main
	// view is cleared to openblack's colour as always (the original does not clear: the film covers the screen).
	// In mode 2 (+0x205A28, the falling spell, Video/FallingSpellVideo.h) case 2 0x54DD9B..0x54DE02 draws no land
	// either: the film with base alpha 0x50 (FallingSpell::Draw's thedraw 0x52689F) and, not ported, the falling
	// creature, its sprites and the liquid particles. (inferido) what lies under that 31 % film in the original (no
	// clear read): here openblack's clear colour
	if (video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld())
	{
		bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
		DrawFinishFrameOverlays();
		return;
	}
	// DrawSky 0x5E21FD..0x5E222B, once a frame from GLandscape::Draw (0x5E48AE): fn_0086A2C0 samples the sky type of
	// the visual time [0xBF3380], then fn_0086A330 rebuilds the land light table (UpdateLandLight below) and advances
	// the dome. Here once per DrawScene, so the reflection pass does not advance the dome a second time.
	if (Game::Instance() != nullptr)
	{
		sky_type::SampleFrame(Game::Instance()->GetDayNightClock().GetVisualTime());
	}
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().UpdateDome();
	}
	UpdateLandLight();
	// fn_005E5830, called by GLandscape::Draw (0x5E488E) before the models: the one light of LH3DTech for this frame.
	// The focus is the player hand's model position (CHand::position, +0x78 of MyInterface()->hand, 0x5E5848), used
	// even while the hand is hidden, and not the point under the cursor of GetPlayerHandPositions. Known difference: with
	// the cursor off the land the original still moves the hand along the mouse ray at its distance from view
	// (ObtainRequiredHandPosition 0x5B5E70, CHand::fn_0046DF60 keeps |camera - position| when the ray misses), while
	// HandSystem::Place leaves the hand where it was last placed, so at night the light stays there.
	bool placed = false;
	if (Game::Instance() != nullptr && Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value() &&
	    drawDesc.camera != nullptr)
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		const auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			// 0x5E58D1..0x5E58DF: Time2SkyType(GetVisualTime()) computed there, not the frame's sample [0xFA26BC]
			// (the same value: DrawSky samples the same visual time right after)
			model_light::UpdateFrameLight(registry.Get<const ecs::components::Transform>(hand).position,
			                              drawDesc.camera->GetOrigin(),
			                              sky_type::At(Game::Instance()->GetDayNightClock().GetVisualTime()));
			placed = true;
		}
	}
	if (!placed)
	{
		// The original runs fn_005E5830 every frame; with no hand or camera to place it here, the light falls back to
		// its day branch, the default sun (0x5E5B70), instead of keeping an old frame's (inferido)
		model_light::SetLight(model_light::k_DefaultSun);
	}
	UpdateShadows(drawDesc);
	if (drawDesc.drawIsland)
	{
		UpdateClouds();
	}
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::FootprintPass);
		DrawStaticShadowPass(drawDesc);
	}
	// TODO(bwrsandman): Footprint framebuffer doesn't need to be updated each frame
	DrawFootprintPass(drawDesc);
	DrawLandAlphaPass(drawDesc);
	// Reflection Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::ReflectionPass);
		if (drawDesc.drawWater)
		{
			UpdateReflectionTarget();
			DrawSceneDesc drawPassDesc = drawDesc;
			// fn_007FF4F0 draws the mirrored land with UseSmallBump ([0xC37210]) = 0 (sea_pass::SeaPassState::smallBump)
			if (!sea_pass::ForPass(graphics::RenderPass::Reflection).smallBump)
			{
				drawPassDesc.smallBumpMapStrength = 0.0f;
			}

			const auto& frameBuffer = Locator::oceanSystem::value().GetReflectionFramebuffer();
			auto reflectionCamera = drawDesc.camera->Reflect();

			drawPassDesc.viewId = graphics::RenderPass::Reflection;
			drawPassDesc.camera = reflectionCamera.get();
			drawPassDesc.frameBuffer = &frameBuffer;
			drawPassDesc.drawWater = false;
			drawPassDesc.drawBoundingBoxes = false;
			// "LandRef" detail key: without it only the sky is mirrored
			drawPassDesc.drawIsland = GetDetailLevel(Locator::config::value().detailLevel).landReflection;
			// GLandscape::Draw (0x5E48B3, "LandRef"): the mirrored scene under the sea is only the sky and the land
			// (fn_007FF4F0); models and sprites are not reflected unless the living water mod is on
			if (!Locator::config::value().livingWater)
			{
				drawPassDesc.drawEntities = false;
				drawPassDesc.drawSprites = false;
			}
			// (the culling of the mirrored camera comes from sea_pass::ForPass(Reflection))

			DrawPass(drawPassDesc);
		}
	}

	// Main Draw Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::MainPass);
		DrawPass(drawDesc);
	}
	DrawHandToolTip(*drawDesc.camera);
	DrawFinishFrameOverlays();
}

void Renderer::DrawFinishFrameOverlays() const
{
	// LH3DRender::FinishFrame 0x82F460, all in the Sequential ScreenOverlay view: (e) the bars (0x82F652..0x82F6DD), then
	// the callbacks with the bit 0x80000000 (0x82F6E5..0x82F718), among them LHVideoPlayer::thedraw 0x844E30
	// (registered with 1 at 0x54B62D, RegisterFinishFrameCallback 0x82F2C0 ORs the bit), so the film is drawn over the
	// bars and fits between them at 100 % (FullScreenRect's letterbox is barH at pct 1); (h) the fade fn_0086FEE0
	// (0x82F753) last, over the film
	if (video::GetFallingSpell().HidesWorld())
	{
		// (milagros2, fallspell) mode 2: FallingSpell::Draw draws the film itself (thedraw(0) 0x52689F, which clears
		// the player's pending flag +0x64 so the 0x8000 callback draws nothing, 0x844E3A..0x844E49), before FinishFrame:
		// then the Z-sorter (its sparks), the callback 0x526480 (its bursts), the bars and the fade
		DrawVideoOverlay();
		DrawFallingSpellOverlay();
		DrawScreenOverlay(false);
		DrawScreenOverlay(true);
		return;
	}
	DrawScreenOverlay(false);
	DrawVideoOverlay();
	DrawScreenOverlay(true);
}

void Renderer::DrawVideoOverlay() const
{
	const auto frame = video::Get().GetFrame();
	if (!frame || frame->width == 0 || frame->height == 0 || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const glm::u16vec2 size(static_cast<uint16_t>(frame->width), static_cast<uint16_t>(frame->height));
	if (!bgfx::isValid(_videoTexture) || _videoTextureSize != size)
	{
		if (bgfx::isValid(_videoTexture))
		{
			bgfx::destroy(_videoTexture);
		}
		// the tiles are clamped: the material's +5 bit 2 (tiling) is cleared at 0x844FD7, SetD3DTillingOff 0x8459B1.
		// (inferido) the driver's bilinear filter
		_videoTexture = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                      BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
		bgfx::setName(_videoTexture, "Video");
		_videoTextureSize = size;
		_videoSerial.reset();
	}
	if (_videoSerial != frame->serial)
	{
		// UploadToTextures fn_00845420 after each picture decoded (0x84514E)
		bgfx::updateTexture2D(_videoTexture, 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(frame->rgba.data(), static_cast<uint32_t>(frame->rgba.size())));
		_videoSerial = frame->serial;
	}
	if (!bgfx::isValid(_videoAlphaTexture))
	{
		const uint8_t white = 0xFF;
		_videoAlphaTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R8,
		                                           BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(&white, 1));
	}

	// DrawToScreen(colour, 0, bars, W + 1, H - 2 bars + 1) 0x54DC56..0x54DC6D with W, H = [0xE839E4] / [0xE839E8]
	const int width = _resolution.x;
	const int height = _resolution.y;
	const auto rect = video::FullScreenRect(width, height);
	// fn_00845740 0x8457B9..0x8457D3: the size of a texel on screen, w / +0x00 and h / +0x04
	const float sx = static_cast<float>(rect.width) / static_cast<float>(frame->width);
	const float sy = static_cast<float>(rect.height) / static_cast<float>(frame->height);
	// the mosaic of Open fn_00844E70: ceil(size / 256) tiles a side (+0x28, +0x2C)
	constexpr uint32_t k_Tile = 0x100;
	const uint32_t tilesX = (frame->width + k_Tile - 1) / k_Tile;
	const uint32_t tilesY = (frame->height + k_Tile - 1) / k_Tile;
	const uint32_t abgr = lh3d_colour::ToAbgr(frame->colour); // the diffuse of the four vertices (0x8458C1..0x8458DB)
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	vertices.reserve(static_cast<size_t>(tilesX) * tilesY * 6);
	const auto toClipX = [width](float px) { return 2.0f * px / static_cast<float>(width) - 1.0f; };
	const auto toClipY = [height](float py) { return 1.0f - 2.0f * py / static_cast<float>(height); };
	for (uint32_t ty = 0; ty < tilesY; ++ty)
	{
		// 0x84583A..0x845854: the last row has height & 0xFF texels (256 if that is 0)
		const uint32_t rows = (ty == tilesY - 1 && (frame->height & 0xFFu) != 0) ? (frame->height & 0xFFu) : k_Tile;
		for (uint32_t tx = 0; tx < tilesX; ++tx)
		{
			// 0x84585C..0x845870: the last column width & 0xFF
			const uint32_t columns = (tx == tilesX - 1 && (frame->width & 0xFFu) != 0) ? (frame->width & 0xFFu) : k_Tile;
			// 0x845878..0x8458F1: x0 = x + tx * 256 * sx, x1 = x0 + columns * sx, likewise y (pre-transformed, FVF 0x1C4)
			const float x0 = static_cast<float>(rect.x) + static_cast<float>(tx * k_Tile) * sx;
			const float x1 = x0 + static_cast<float>(columns) * sx;
			const float y0 = static_cast<float>(rect.y) + static_cast<float>(ty * k_Tile) * sy;
			const float y1 = y0 + static_cast<float>(rows) * sy;
			// 0x845891..0x84596C: u, v from 1/512 to n/256 - 1/512 of the 256x256 tile, half a texel inside each edge;
			// the same texels of the one texture here (clamped, so the filter never reaches the next tile)
			const float u0 = (static_cast<float>(tx * k_Tile) + 0.5f) / static_cast<float>(frame->width);
			const float u1 = (static_cast<float>(tx * k_Tile + columns) - 0.5f) / static_cast<float>(frame->width);
			const float v0 = (static_cast<float>(ty * k_Tile) + 0.5f) / static_cast<float>(frame->height);
			const float v1 = (static_cast<float>(ty * k_Tile + rows) - 0.5f) / static_cast<float>(frame->height);
			const Vertex a {toClipX(x0), toClipY(y0), 0.5f, u0, v0, abgr};
			const Vertex b {toClipX(x1), toClipY(y0), 0.5f, u1, v0, abgr};
			const Vertex c {toClipX(x1), toClipY(y1), 0.5f, u1, v1, abgr};
			const Vertex d {toClipX(x0), toClipY(y1), 0.5f, u0, v1, abgr};
			for (const auto& vertex : {a, b, c, a, c, d})
			{
				vertices.push_back(vertex);
			}
		}
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	// mode 6 (MODULATE colour and alpha): colour = texture x diffuse, alpha = texture alpha (1) x diffuse alpha
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, fromBgfx(_videoTexture));
	program->SetTextureSampler("s_alpha", 1, fromBgfx(_videoAlphaTexture));
	bgfx::setVertexBuffer(0, &buffer);
	// CreateMaterial(mode 6) 0x844FC6 with +5 |= 1 (two-sided, 0x844FE4: CULLMODE NONE 0x8459E4): SRCALPHA /
	// INVSRCALPHA, no Z write; ZFUNC (0x845A16) ALWAYS and ZWRITEENABLE (0x845A49) 0 from DrawToScreen's two false
	// bools (0x54DC56 / 0x54DC58)
	constexpr render_modes::Material k_VideoMaterial {render_modes::Mode::AlphaTexturedAlphaNz, render_modes::k_TwoSided};
	bgfx::setState(render_modes::State(k_VideoMaterial, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawHandToolTip(const Camera& camera) const
{
	if (!Locator::handSystem::has_value() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const auto amount = Locator::handSystem::value().GetAmountInHandToolTip();
	if (!amount)
	{
		return;
	}
	if (!_fontLoadTried)
	{
		_fontLoadTried = true;
		auto font = std::make_unique<GameFont>();
		auto& fileSystem = Locator::filesystem::value();
		const auto base = fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "j0.met").replace_extension();
		if (font->Load(base))
		{
			_font = std::move(font);
		}
	}
	if (!_font)
	{
		return;
	}
	const auto hands = Locator::handSystem::value().GetPlayerHands();
	const auto& registry = Locator::entitiesRegistry::value();
	if (hands.empty() || !registry.Valid(hands[0]))
	{
		return;
	}
	const float width = _resolution.x;
	const float height = _resolution.y;
	glm::vec3 screen;
	if (!camera.ProjectWorldToScreen(registry.Get<ecs::components::Transform>(hands[0]).position,
	                                 glm::vec4(0.0f, 0.0f, width, height), screen))
	{
		return;
	}
	// the anchor is the hand on screen, clamped to [0, W - h] x [0, H - h]; box height h = H / 25, text 2/3 of it,
	// centred vertically; the text goes to the other side of the hand past 2/3 of the screen (and back below 1/3)
	static bool leftSide = false;
	const float h = height / 25.0f;
	const float size = h * 2.0f / 3.0f;
	const auto text = helptext::Format(helptext::k_ToolTipAmountInHand, static_cast<double>(*amount));
	const float textWidth = _font->GetStringWidth(text, size);
	const float boxWidth = textWidth + h;
	float x = screen.x;
	const float y = std::clamp(screen.y, 0.0f, height - h); // ProjectWorldToScreen: y from the top
	if (x > 2.0f * width / 3.0f)
	{
		leftSide = true;
	}
	else if (x < width / 3.0f)
	{
		leftSide = false;
	}
	if (leftSide)
	{
		x -= boxWidth;
	}
	x = std::clamp(x, 0.0f, std::max(0.0f, width - boxWidth));

	const auto toClip = [width, height](float px, float py) {
		return glm::vec2(2.0f * px / width - 1.0f, 1.0f - 2.0f * py / height);
	};
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));

	// no box: the text alone over the scene (transparent background)
	// the text: DrawTextRaw three times, black copies 1 px to each side, then yellow (LH3DColor b0 g255 r255 a255)
	std::vector<GameFont::Vertex> glyphs;
	const float tx = x + h * 0.5f; // just right of the hand
	const float ty = y + (h - size) * 0.5f;
	_font->AddText(glyphs, text, tx - 1.0f, ty, size, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	_font->AddText(glyphs, text, tx + 1.0f, ty, size, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	_font->AddText(glyphs, text, tx, ty, size, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	if (glyphs.empty())
	{
		return;
	}
	struct TextVertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<TextVertex> vertices;
	vertices.reserve(glyphs.size());
	for (const auto& g : glyphs)
	{
		const auto p = toClip(g.x, g.y);
		vertices.push_back({p.x, p.y, 0.5f, g.u, g.v, g.abgr});
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(TextVertex));
	const auto* program = _shaderManager->GetShader("Text");
	program->SetTextureSampler("s_diffuse", 0, _font->GetTexture());
	bgfx::setVertexBuffer(0, &buffer);
	// mode 16 (CachePage::Init 0x830244): SRCALPHA / INVSRCALPHA, no Z write, ZFUNC ALWAYS (depthTest 0); its alpha test
	// (+4 = 5) is fs_text's
	bgfx::setState(render_modes::State(render_modes::Mode::TexturedChromaAlphaNz, {.zFunc = render_modes::ZFunc::Always}));
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawScreenOverlay(bool drawFade) const
{
	if (Game::Instance() == nullptr || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const auto& fade = Game::Instance()->GetScreenFade();
	const uint32_t colour = fade.GetColour();
	const int width = _resolution.x;
	const int height = _resolution.y;
	const int bar = ScreenFade::LetterboxHeight(width, height, fade.GetWideScreenFraction());
	if (drawFade ? (colour >> 24) == 0 : bar == 0)
	{
		return;
	}
	struct Vertex
	{
		float x, y, z;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	// pre-transformed rectangles in pixels (FVF 0x1C4, rhw 1), here straight to clip space
	const auto addRect = [&vertices, width, height](int x0, int y0, int x1, int y1, uint32_t argb) {
		const uint32_t abgr = lh3d_colour::ToAbgr(argb);
		const float l = 2.0f * static_cast<float>(x0) / static_cast<float>(width) - 1.0f;
		const float r = 2.0f * static_cast<float>(x1) / static_cast<float>(width) - 1.0f;
		const float t = 1.0f - 2.0f * static_cast<float>(y0) / static_cast<float>(height);
		const float b = 1.0f - 2.0f * static_cast<float>(y1) / static_cast<float>(height);
		for (const auto& [x, y] : {std::pair {l, t}, {r, t}, {r, b}, {l, t}, {r, b}, {l, b}})
		{
			vertices.push_back({x, y, 0.5f, abgr});
		}
	};
	// (e) the bars, 0xFF000000, at the top and the bottom
	const auto addBars = [&]() {
		if (bar > 0)
		{
			addRect(0, 0, width, bar, 0xFF000000u);
			addRect(0, height - bar, width, height, 0xFF000000u);
		}
	};
	if (drawFade)
	{
		// (h) fn_0086FEE0: x 0..W-1, y h'..H-1-h' with h' = h ? h - 1 : 0, then the bars again so the fade never tints them
		const int inset = bar > 0 ? bar - 1 : 0;
		addRect(0, inset, width - 1, height - 1 - inset, colour);
	}
	addBars();
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::setVertexBuffer(0, &buffer);
	// mode 1 (untextured, SRCALPHA / INVSRCALPHA), ZFUNC ALWAYS and ZWRITEENABLE 0 by hand (fn_0081E590 0x81E64C)
	bgfx::setState(render_modes::State(render_modes::Mode::SmoothAlpha,
	                                   {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	bgfx::submit(viewId, toBgfx(_shaderManager->GetShader("DebugLine")->GetRawHandle()));
}

namespace
{
/// One entry of the frame's transparency queue (zsorter::Queue, LH3DZSorter::NewZObject 0x83F310): what DrawPass needs
/// to draw it when the queue is drained (fn_0082F280), in place of the original's object and callback. One of the
/// indices is set; with none, an instance of the models (a mesh with the flag 0x200, LH3DObject::AddDrawing 0x815F53;
/// a fading one; the hand, CHand::AddDrawing 0x46D203).
/// (aproximado) the arrival order, which breaks ties between equal keys, is not the original's: here clouds, then the
/// models mesh by mesh (a map), fading ones, the PSys sprites, meshes, chains and Queued effects, sprites, mists,
/// smoke, rain, boat; there the order of the frame's AddDrawing calls (tmp_dis original-frame.md steps 4m..22)
struct ZObject
{
	entt::id_type meshId {0};
	uint32_t index {0};
	bool morphWithTerrain {false};
	bool fading {false};
	entt::entity sprite {entt::null};
	/// a sprite of a Sorted effect (manager::SortedFrame::sprites): LH3DSprite::AddDrawing 0x840C70 from 0x67B0D2
	int psysSprite {-1};
	/// a mesh atom of a Sorted effect (RenderContext::psysAtoms), opaque or not: fn_00679F60 from 0x67A246
	int psysMesh {-1};
	/// a chain of a Sorted effect (manager::SortedFrame::chains): fn_0067B380 from 0x6798DF
	int psysChain {-1};
	/// a Queued effect (manager::CollectQueued), all of it: PSysManager::AddDrawing 0x6797D0 -> NewZObject 0x679834
	int queuedEffect {-1};
	int mist {-1};  ///< an index of _frameMists
	int smoke {-1}; ///< an index of _frameSmoke (LH3DSmoke::AddDrawing: one Z object per chimney)
	int cloud {-1}; ///< a cloud of _clouds (LH3DMist::AddDrawing 0x7FA87B from fn_005E25C0)
	int rain {-1};  ///< an index of _frameRain (fn_008341B0 0x83427F, one per raining tile)
	int boat {-1};  ///< an index of _frameBoatSprites (LH3DSprite::AddDrawing 0x840CB3, one per sprite)

	/// a model instance (meshId / index): none of the other kinds is set. The drain draws it with drawInstance, its
	/// shadows inside it (DrawShadowsOnObject); a new kind must be added here too
	[[nodiscard]] bool IsModel() const
	{
		return sprite == entt::null && psysSprite < 0 && psysMesh < 0 && psysChain < 0 && queuedEffect < 0 && mist < 0 &&
		       smoke < 0 && cloud < 0 && rain < 0 && boat < 0;
	}
};

// The renderer draws the PSys effects by their draw path (manager::CollectSorted / CollectQueued / HandEffects and
// RenderContext::psysAtoms): with k_DrawByPath false the mesh atoms would be drawn by the old loops as well
static_assert(psys::manager::k_DrawByPath, "Renderer::DrawPass draws the PSys effects by path");
} // namespace

void Renderer::DrawPass(const DrawSceneDesc& desc) const
{
	const auto& meshManager = Locator::resources::value().GetMeshes();
	auto& profiler = Locator::profiler::value();

	if (desc.frameBuffer != nullptr)
	{
		desc.frameBuffer->Bind(desc.viewId);
	}
	// This dummy draw call is here to make sure that view is cleared if no
	// other draw calls are submitted to view
	bgfx::touch(static_cast<bgfx::ViewId>(desc.viewId));

	_shaderManager->SetCamera(desc.viewId, *desc.camera);

	const auto* skyShader = _shaderManager->GetShader("Sky");
	const auto* terrainShader = _shaderManager->GetShader("Terrain");
	const auto* debugShader = _shaderManager->GetShader("DebugLine");
	const auto* spriteShader = _shaderManager->GetShader("Sprite");
	const auto* debugShaderInstanced = _shaderManager->GetShader("DebugLineInstanced");
	const auto* objectShaderInstanced = _shaderManager->GetShader("ObjectInstanced");

	// Distance haze of this frame (graphics::haze::Frame: fn_007FEAA0 / fn_007FEAD0 and the "Fog" detail key)
	_haze = _landLight && _landLight->IsLoaded() ? haze::Frame() : haze::Params {};
	_hazeUniforms = haze::Uniforms(_haze);
	const glm::vec4 u_haze = _hazeUniforms[0];
	const glm::vec4 u_hazeColour = _hazeUniforms[1];

	// LH3DRender::StartFrame 0x82F1F9 -> fn_0083F3B0: the frame's single queue of everything blended, filled while the
	// main view is drawn and drained once, far to near, after all of it (FinishFrame 0x82F480 -> fn_0082F280)
	zsorter::Queue<ZObject> sorted;
	sorted.Begin();

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSky
		                                                                          : Profiler::Stage::MainPassDrawSky);
		if (desc.drawSky)
		{
			const auto modelMatrix = glm::mat4(1.0f);
			// x unused: the sky type is blended into the dome textures by Sky::UpdateDome (sky_type::DomeBlend)
			const glm::vec4 u_typeAlignment = {0.0f, _skyAlignment.Get() + 1.0f, 0.0f, 0.0f};

			skyShader->SetTextureSampler("s_diffuse", 0, Locator::skySystem::value().GetTexture());
			skyShader->SetUniformValue("u_typeAlignment", &u_typeAlignment);

			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = skyShader;
			// (inferido) the sky meshes' own modes, culled as a whole (flipped by the mirrored camera)
			submitDesc.options = {.cull = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Sky, false, false),
			                      .writeAlpha = true,
			                      .msaa = true};
			submitDesc.modelMatrices = &modelMatrix;
			submitDesc.matrixCount = 1;
			submitDesc.isSky = true;

			DrawMesh(Locator::skySystem::value().GetMesh(), submitDesc, 0);
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawMoon(desc.viewId, *desc.camera);
				DrawSun(desc.viewId, *desc.camera, false);
				// fn_005E25C0 0x5E2813: each cloud's vt+0x100, LH3DMist::AddDrawing 0x7FA7F0, queues it with the blended
				// things of the frame (it used to be sorted on its own and drawn after all of them)
				for (const auto& [key, index] : CollectClouds(*desc.camera))
				{
					sorted.Submit({.cloud = static_cast<int>(index)}, key);
				}
			}
			else if (desc.viewId == graphics::RenderPass::Reflection)
			{
				// the moon is reflected (its halo and DrawUnderWater; mirrored: sea_pass::ForPass), the sun is not
				// (fn_0086C140 is called once)
				DrawMoon(desc.viewId, *desc.camera);
			}
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawWater
		                                                                          : Profiler::Stage::MainPassDrawWater);
		if (desc.drawWater)
		{
			// fn_00879930 / fn_0087A090 (RendererSea.cpp)
			DrawSea(desc);
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawIsland
		                                                                          : Profiler::Stage::MainPassDrawIsland);
		if (desc.drawIsland)
		{
			auto& island = Locator::terrainSystem::value();
			auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);

			// Small bump fade line (fn_007FEE60): the plane perpendicular to the camera forward, 50 units ahead, meets
			// the horizontal plane y = min(camera y, 0.67 * 165); the detail is full up to 20 units before that line and
			// gone 20 units past it.
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto cameraForward = desc.camera->GetForward();
			const glm::vec2 forwardXZ(cameraForward.x, cameraForward.z);
			const float forwardLength = std::max(glm::length(forwardXZ), 1e-4f);
			const float lineDistance =
			    (50.0f + cameraForward.y * std::max(0.0f, cameraOrigin.y - 0.67f * 165.0f)) / forwardLength;
			const glm::vec4 u_smallBumpLine = {cameraOrigin.x, cameraOrigin.z, forwardXZ / forwardLength};
			// x unused: the sky type reaches the land through the land light table and the haze (vs_terrain)
			const glm::vec4 u_skyAndBump = {0.0f, desc.bumpMapStrength, desc.smallBumpMapStrength, lineDistance};

			terrainShader->SetTextureSampler("s0_materials", 0, island.GetAlbedoArray());
			terrainShader->SetTextureSampler("s1_bump", 1, island.GetBump());
			terrainShader->SetTextureSampler("s2_smallBump", 2, island.GetSmallBump());
			terrainShader->SetTextureSampler("s3_footprints", 3, island.GetFootprintFramebuffer().GetColorAttachment());
			terrainShader->SetTextureSampler("s_landLightTable", 4, fromBgfx(_landLightTexture)); // vs
			const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
			if (bgfx::isValid(_landCellsTexture) && glm::vec2(_landCellsSize) == cellMapSize)
			{
				terrainShader->SetTextureSampler("s_landCells", 6, fromBgfx(_landCellsTexture)); // vs
			}
			else
			{
				terrainShader->SetTextureSampler("s_landCells", 6, island.GetCellMap()); // vs
			}
			const glm::vec4 u_cellMap = {island.GetExtent().minimum, cellMapSize};
			terrainShader->SetUniformValue("u_cellMap", &u_cellMap); // vs
			terrainShader->SetTextureSampler("s5_staticShadow", 5, island.GetStaticShadowFramebuffer().GetColorAttachment());
			terrainShader->SetTextureSampler("s8_landAlpha", 8, island.GetLandAlphaFramebuffer().GetColorAttachment());
			// x: 1 = the colour comes from the block texture (the original's, BlockTexture.h); 0 = the per-vertex materials
			// of the terrain mods (terrain-x2 repeats, triplanar cliffs), which the block texture cannot follow
			const auto& terrainConfig = Locator::config::value();
			const bool terrainMod = terrainConfig.terrainTextureDensity != 1.0f || terrainConfig.terrainTriplanar ||
			                        terrainConfig.terrainTexturesX2;
			glm::vec4 u_blockTexture {0.0f};
			if (const auto* blockTexture = island.GetBlockTexture(); blockTexture != nullptr)
			{
				terrainShader->SetTextureSampler("s10_blockTexture", 10, *blockTexture);
				u_blockTexture.x = terrainMod ? 0.0f : 1.0f;
				u_blockTexture.y = 1.0f; // its alpha is the coast alpha
			}
			else
			{
				terrainShader->SetTextureSampler("s10_blockTexture", 10, island.GetLandAlphaFramebuffer().GetColorAttachment());
			}
			terrainShader->SetUniformValue("u_blockTexture", &u_blockTexture);
			terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
			terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
			terrainShader->SetUniformValue("u_haze", &u_haze);
			terrainShader->SetUniformValue("u_hazeColour", &u_hazeColour);
			// Static shadows darken the block texture by up to x0.5 (x0.75 with 128 px textures, fn_008721A0)
			const float staticShadowStrength =
			    GetDetailLevel(Locator::config::value().detailLevel).useHighTexture ? 0.5f : 0.25f;
			// x: the light table >> 1 of the mirrored land (sea_pass::SeaPassState::landLightScale, 0x7FF53F..0x7FF564)
			const glm::vec4 u_terrainPass = {sea_pass::ForPass(desc.viewId).landLightScale,
			                                 Locator::config::value().terrainTextureDensity, staticShadowStrength,
			                                 Locator::config::value().terrainTriplanar ? 1.0f : 0.0f};
			terrainShader->SetUniformValue("u_terrainPass", &u_terrainPass);
			terrainShader->SetUniformValue("u_islandExtent", &islandExtent);

			// clang-format off
			// fs_terrain writes the land and small bump passes premultiplied (the coast alpha does not fade the small
			// bump, as in the original); Z is written even where the land is transparent (render mode 14)
			// (the land blocks' material, fn_007FEDB0 0x7FEDE2: the function of mode 5, 0x82DD90)
			const auto defaultState = render_modes::State(render_modes::Mode::Landscape,
				{.writeAlpha = true, .msaa = true, .premultiplied = true});

			constexpr auto discard = 0u
				| BGFX_DISCARD_INSTANCE_DATA
				| BGFX_DISCARD_INDEX_BUFFER
				| BGFX_DISCARD_TRANSFORM
				| BGFX_DISCARD_VERTEX_STREAMS
				| BGFX_DISCARD_STATE
			;
			// clang-format on

			// The mirrored land (fn_007FF4F0) is drawn with ZWRITEENABLE off (GLandscape::Draw 0x5E48C5..0x5E4900), so it
			// hides nothing drawn after it under the sea (the hand's, the objects' and the boats' reflections, the fish);
			// here it wrote Z, and a mirrored hill hid the reflections behind it.
			const auto seaPass = sea_pass::ForPass(desc.viewId);
			const auto state = seaPass.landWriteZ ? defaultState : defaultState & ~BGFX_STATE_WRITE_Z;
			// openblack's blocks wind the other way round from the models (sea_pass::Surface::Land)
			const uint64_t landCull =
			    seaPass.FaceCull(sea_pass::Surface::Land, false, false) == render_modes::Cull::Ccw ? BGFX_STATE_CULL_CCW
			                                                                                     : BGFX_STATE_CULL_CW;

			// Block order of both land passes: the list LH3DIsland::PreDraw (0x7FF45F..0x7FF4DD) builds, ascending by
			// block+0x9BC = distance from the camera to the block centre (x + 80, 0, z + 80) with LandRef on
			// (fn_00877210 0x87722C..0x877296, 0x877C8A..0x877CCD): nearest first. Without Z, it is what decides
			// which mirrored hill covers which.
			const auto& blocks = island.GetBlocks();
			const auto& viewOrigin = desc.camera->GetOrigin();
			std::vector<std::pair<float, size_t>> blockOrder;
			blockOrder.reserve(blocks.size());
			for (size_t i = 0; i < blocks.size(); ++i)
			{
				const glm::vec2 centre = blocks[i].GetMapPosition() + glm::vec2(80.0f);
				blockOrder.emplace_back(glm::length(glm::vec3(centre.x - viewOrigin.x, viewOrigin.y, centre.y - viewOrigin.z)), i);
			}
			std::stable_sort(blockOrder.begin(), blockOrder.end(),
			                 [](const auto& a, const auto& b) { return a.first < b.first; });

			// fn_00877210: the block's haze class from its box (graphics::haze::BlockClassOf), LandRef [0xE9CD8C] (inferido)
			const auto view = desc.camera->GetViewMatrix(Camera::Interpolation::Current);
			const bool landRef = GetDetailLevel(Locator::config::value().detailLevel).landReflection;
			for (const auto& [distance, blockIndex] : blockOrder)
			{
				const auto& block = blocks[blockIndex];
				// pack uniforms
				const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
				terrainShader->SetUniformValue("u_blockPositionAndSize", &mapPositionAndSize);
				const glm::vec4 u_hazeBlock = {static_cast<float>(haze::BlockClassOf(_haze, view, block, landRef)), 0.0f,
				                               0.0f, 0.0f};
				terrainShader->SetUniformValue("u_hazeBlock", &u_hazeBlock);

				block.GetMesh().GetVertexBuffer().Bind();

				bgfx::setState(state | landCull, 0);
				bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(terrainShader->GetRawHandle()), 0, discard);
				// fn_007FF610 0x7FF6BF..0x7FF749: the shadows over the block just drawn, only in the main land (the mirrored
				// one, fn_007FF4F0, draws none)
				if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities)
				{
					DrawLandShadows(desc.viewId, block, landCull);
				}
			}
			bgfx::discard(BGFX_DISCARD_BINDINGS);

			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawFoliage(desc);
			}
		}
	}

	bool mistsSorted = false; ///< the mists went through the back-to-front list of the blended models
	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawModels
		                                                                          : Profiler::Stage::MainPassDrawModels);
		// The original's "underwater" stage (GLandscape::Draw 0x5E490F): the hand mirrored in the sea, unlit in
		// SetColorSpecular(0x65A0A0A0, 0) (0x5E496C..0x5E4975), only its part above the water (CHand's LH3DObject
		// DrawUnderWater, vt+0x118 0x5E4985 = fn_00813300, which sets [0xC37D9C] = obj+0x80 and goes on to fn_00811010;
		// sea_pass::UnderWater). Around it vt+0x58(0) 0x5E497E (fn_008168C0: Flags1 0x20 cleared, it is only set while
		// [0xC38224] != 0) and vt+0x58(edi) 0x5E4991 puts it back: (no portado) what Flags1 0x20 does in this draw is
		// not identified (fn_00811010 has no test of it)
		if (!desc.drawEntities && desc.viewId == graphics::RenderPass::Reflection)
		{
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
			const auto handDesc = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			const auto* bones = Locator::handSystem::has_value() ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
			if (handDesc != renderCtx.instancedDrawDescs.end() && bones != nullptr)
			{
				const auto mesh = meshManager.Handle(ecs::components::Hand::k_MeshId);
				if (mesh->GetBoneMatrices().size() == bones->size())
				{
					DrawUnderWater(desc.viewId, *mesh,
					               std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer,
					                                                        handDesc->second.offset, handDesc->second.count),
					               bones->data(), static_cast<uint8_t>(bones->size()), false,
					               sea_pass::UnderWater(sea_pass::k_HandColour, sea_pass::k_HandSpecular));
				}
			}
			DrawObjectReflections(desc.viewId);
			// TODO(sea_pass, R10): the creature's DrawUnderWater (GLandscape::Draw 0x5E4A84..0x5E4AE6) goes here, after
			// the held and physics objects: SetColorSpecular(sea_pass::k_CreatureColour, k_CreatureSpecular) (0x5E4ACD /
			// 0x5E4ACF, vt+0x2C 0x5E4AD6), vt+0x58(0) 0x5E4ADF, vt+0x118 0x5E4AE6, only while the tests of
			// sea_pass::k_CreatureMaxBlockDistance / k_CreatureMaxY / k_CreatureMaxA0 pass (0x5E4A88..0x5E4AC9). There is
			// no creature in openblack yet
			DrawBoatReflection(desc.viewId);
		}
		if (desc.viewId == graphics::RenderPass::Reflection)
		{
			// GLandscape::Draw 0x5E4B26..: the parts under the water go into the frame before the sea, over the mirrored
			// land. Here that frame is the reflection target, drawn with the mirrored camera, so they are mirrored too.
			// the sharks (and whatever else is cut by the plane), then the fish (4d-4e)
			// the sharks: fn_00775120 0x5E4B26
			DrawCutBelowWater(desc.viewId);
			// fn_00824B90 0x5E4B2B: each shoal of a bait, then its net (fn_00829BC0) under the net's own plane
			// (sea_pass::k_NetPlane, 0x829C91..0x829CB7, put back 0x829D25..0x829D45)
			DrawFishShoals(desc.viewId);
			DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_NetPlane));
			// TODO(sea_pass, R11): the swimming SuperVillagers go here, after fn_00824B90 (0x5E4B2B: the fish and the
			// nets) and before the hand's glow (0x5E4D89) (GLandscape::Draw 0x5E4B4C..0x5E4D76): each of the list
			// SuperVillager::g_first [0xEB9A08] whose animation is "M_P_Swim2" (0xBF3598, compared at 0x5E4C07), after its
			// Draw vt+0x610 (0x5E4BC2) and shadow fn_00874850 (0x5E4BFB): the plane sea_pass::k_SwimPlane
			// (0x5E4C4A..0x5E4C5E), SetColorSpecular(sea_pass::k_SwimmerColour, k_SwimmerSpecular) (0x5E4C68..0x5E4C70),
			// vt+0x11C 0x5E4C77 = DrawCutByPlane(viewId, entity, KeepBelow, k_SwimmerColour, k_SwimmerSpecular), then the
			// default plane again (0x5E4D76). openblack has no SuperVillager list (ScriptControl.cpp)
			// 0x5E4D89: the hand's glow on the water, the last thing before the sea
			DrawHandWaterGlow(desc.viewId);
		}
		// LH3DSprite::Draw 0x840530 mode A (billboard::Screen) on the GPU: vs_sprite adds u_invView x (model x (x, y, 0,
		// 0)) to the model's translation on the plane -1..1 with v = 0 at the top, so the half width / half height are
		// the Transform's scale x / y. components::Sprite has no angle nor origin (every user, NightLights, FireFlies,
		// Dust, HandEffects, Glow, CameraBookmark, HandDebugHooks, gives an identity rotation), so it is Screen with angle
		// 0 and ox = oy = 0. The near test of mode A (0x840585) is made here on the CPU
		const auto spriteFrame = billboard::CameraFrame::From(*desc.camera);
		const auto drawSprite = [this, &spriteShader, &spriteFrame](const ecs::components::Sprite& sprite,
		                                                            const ecs::components::Transform& transform,
		                                                            RenderPass viewId) {
			if (!billboard::InFrontOfNear(transform.position, spriteFrame))
			{
				return;
			}
			const glm::mat4 modelMatrix = billboard::ScreenSpriteModel(transform.position, glm::vec2(transform.scale), 0.0f);

			glm::vec4 u_sampleRect(sprite.uvExtent, sprite.uvMin);

			bgfx::setTransform(glm::value_ptr(modelMatrix));
			spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
			spriteShader->SetUniformValue("u_tint", glm::value_ptr(sprite.tint));
			spriteShader->SetTextureSampler("s_diffuse", 0, sprite.texture);

			_plane->GetVertexBuffer().Bind();

			// mode 13, or mode 6 with the tint premultiplied (fs_sprite) (inferido: the materials of the sprites' owners)
			bgfx::setState(render_modes::State(sprite.additive ? render_modes::Mode::AlphaTexturedAlphaAdditiveNz
			                                                   : render_modes::Mode::AlphaTexturedAlphaNz,
			                                   {.writeAlpha = true, .premultiplied = true}));

			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(spriteShader->GetRawHandle()));
		};
		// LH3DSprite::Draw also goes to the Z-sorter: in the main pass the sprites are sorted with the blended models
		const bool spritesSorted = desc.drawEntities && desc.drawSprites && desc.viewId == graphics::RenderPass::Main;
		// LH3DMist::AddDrawing 0x7FA7F0 sends every mist on screen to the same Z-sorter (key |pos - camera|^2)
		mistsSorted = desc.drawEntities && desc.drawSky && desc.viewId == graphics::RenderPass::Main;

		if (desc.drawEntities)
		{
			if (desc.viewId == graphics::RenderPass::Main)
			{
				_shaderManager->SetCamera(graphics::RenderPass::MainBlended, *desc.camera);
			}
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = objectShaderInstanced;
			submitDesc.options = render_modes::k_ModelPass;
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
			// the objects under a shadow that falls on objects: each one gets it right after its own draw
			// (RendererShadows.cpp: the tail loop of the objects' Draw, 0x80E457..0x80E4D7)
			CollectShadowReceivers(desc.viewId == graphics::RenderPass::Main);

			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawWaterRings(desc.viewId);
				DrawHumanShadows(desc.viewId);
				// fn_00824D60 (0x5E6296): the nets' part over the water, with the default plane
				DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane));
			}
			const auto setMatrices = [&submitDesc](entt::id_type meshId, const L3DMesh& mesh) {
				const auto* handBones =
				    meshId == ecs::components::Hand::k_MeshId ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
				if (mesh.IsBoned() && handBones != nullptr && handBones->size() == mesh.GetBoneMatrices().size())
				{
					// Player hand: animated pose from hh.HBN
					submitDesc.modelMatrices = handBones->data();
					submitDesc.matrixCount = static_cast<uint8_t>(handBones->size());
				}
				else if (mesh.IsBoned())
				{
					submitDesc.modelMatrices = mesh.GetBoneMatrices().data();
					submitDesc.matrixCount = static_cast<uint8_t>(mesh.GetBoneMatrices().size());
					// TODO(bwrsandman): Get animation frame instead of default
				}
				else
				{
					const static auto identity = glm::mat4(1.0f);
					submitDesc.modelMatrices = &identity;
					submitDesc.matrixCount = 1;
				}
			};
			// LH3DObject::AddDrawing 0x815A70 (vt+0x100 of the static, animated and morphing objects) queues an object only by
			// its +4 bit 0x10 (vt+0x44 fn_007F97C0 at 0x815AC2, tested at 0x815F0B): the whole object goes to the Z-sorter
			// (NewZObject 0x815F53, callback 0x7FA980 -> Draw vt+0x108), else it is drawn at once, blended primitives and
			// all (vt+0x108 0x815F62). The bit is the mesh's flag 0x200 (SetMesh 0x7F9E48..0x7F9E58, L3DMesh::IsZSorted);
			// the one-shot orb forces it (0x72A4AA, after SetMesh 0x72A49D). SetGlobalAlpha's bit 0x80 (vt+0x4C
			// fn_007F9D80) is not looked at here (only fn_00813340, vtable 0x9A3068, queues by it, 0x8133B9), so a fading
			// object whose mesh lacks 0x200 is drawn at once through the table 0xC387C8. (inferido) that every model
			// instance is drawn through 0x815A70 (Game3DObject::AddForDrawing 0x63B5D0 -> vt+0x100) and none is of the
			// class 0x9A3068; in the main view only, the queue's (the reflection draws everything at once)
			const bool sortBlended = desc.viewId == graphics::RenderPass::Main;
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto opaqueOptions = submitDesc.options;

			// the poses of the animated boned meshes (ecs/Animations.h), by instance; the PSys mesh atoms' too
			const auto poses = ecs::PosesByInstance(renderCtx);
			// the sharks (components::CutByPlane::drawAbove): their owner draws them cut by the water instead
			const auto cutAbove = desc.viewId == graphics::RenderPass::Main ? CutAboveInstances() : std::unordered_set<uint32_t>();

			// Instance meshes
			for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
			{
				auto mesh = meshManager.Handle(meshId);

				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset, placers.count);
				setMatrices(meshId, *mesh);
				submitDesc.isSky = false;
				submitDesc.lightBoost = meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, meshId, submitDesc);
				submitDesc.morphWithTerrain = placers.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, submitDesc.morphWithTerrain);
				submitDesc.blendFilter = 0;

				// CHand::AddDrawing 0x46D100 queues the whole hand, always (0x46D1B7..0x46D203, no test); any other model
				// whole by its mesh's flag 0x200 (LH3DObject::AddDrawing 0x815F0B). In the main view a queued one is drawn
				// only from the queue, opaque primitives included; the others at once with their blended primitives
				const bool hand = meshId == ecs::components::Hand::k_MeshId;
				const bool queued = sortBlended && (hand || mesh->IsZSorted());
				// TODO(bwrsandman): choose the correct LOD
				if (!queued && mesh->IsBoned() && ecs::HasPose(poses, placers.offset, placers.count))
				{
					// animated (ecs/Animations.h): each instance on its own, with its pose. Only the ones in the view: every
					// draw copies the bones into the backend's per-frame uniform buffer (see vs_object.sc)
					const auto viewProjection = desc.camera->GetViewProjectionMatrix();
					const auto box = mesh->GetBoundingBox();
					const auto boxCentre = box.Center();
					const float boxRadius = glm::length(box.Size()) * 0.5f;
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						const auto& model = renderCtx.instanceUniforms[placers.offset + i];
						const float scale = std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
						                              glm::length(glm::vec3(model[2]))});
						if (cutAbove.contains(placers.offset + i))
						{
							continue;
						}
						if (!SphereInView(viewProjection, glm::vec3(model * glm::vec4(boxCentre, 1.0f)), boxRadius * scale))
						{
							continue;
						}
						submitDesc.instanceDesc =
						    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset + i, 1);
						setMatrices(meshId, *mesh);
						ecs::UsePose(poses, placers.offset + i, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
						DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
						// drawn at once (0x815F62): the shadows over it right after, the tail of its Draw vt+0x108
						// (fn_0080DB30 0x80E457..0x80E4D7 -> fn_0080B050); a queued one gets them in its Z object
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
					}
				}
				else if (!queued)
				{
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
					}
				}
				if (queued)
				{
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						if (cutAbove.contains(placers.offset + i))
						{
							continue;
						}
						// the key of LH3DObject::AddDrawing fn_00815A70 (0x815F0F..0x815F45: +0x38, (x^2 + z^2) + y^2) or,
						// for the hand, of CHand::AddDrawing: the origin +0x38 of the LH3DObject [CHand+0x482C]
						// (0x46D1B7..0x46D1C0), (x^2 + y^2) + z^2 (0x46D1BD..0x46D1F3). (inferido) that the hand instance's
						// translation is that object's origin
						const auto origin = glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						const auto order = hand ? zsorter::SumOrder::XYZ : zsorter::SumOrder::XZY;
						sorted.Submit({.meshId = meshId,
						               .index = placers.offset + i,
						               .morphWithTerrain = placers.morphWithTerrain},
						              zsorter::Key(origin, cameraOrigin, order));
					}
				}
			}
			// Whale::Draw 0x774E10: the sharks' parts above the water, in the normal object list, and the shadows over
			// them (RendererShadows.cpp)
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawCutAboveWater(desc.viewId);
				DrawShadowsOnCutObjects(desc.viewId, cutAbove);
			}
			// One model instance drawn on its own: from the queue, or a fading one at once (in the main view). The table
			// 0xC387C8 with its alpha byte for a fading object (components::Alpha; LH3DObject Draw 0x80DEED..0x80DF09, obj
			// +0x4C -> [0xC37D8C]), every primitive in its own mode for the others
			const auto drawInstance = [&](const ZObject& instance, RenderPass viewId) {
				auto mesh = meshManager.Handle(instance.meshId);
				submitDesc.viewId = viewId;
				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance.index, 1);
				setMatrices(instance.meshId, *mesh);
				ecs::UsePose(poses, instance.index, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.isSky = false;
				submitDesc.lightBoost = instance.meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = instance.meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, instance.meshId, submitDesc);
				submitDesc.morphWithTerrain = instance.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, instance.morphWithTerrain);
				// the whole object (Draw vt+0x108; the hand's CHand::Draw 0x46D210)
				submitDesc.blendFilter = 0;
				submitDesc.options = instance.fading ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = instance.fading ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[instance.index][0][3]);
				submitDesc.mode = std::nullopt;
				submitDesc.sea = {};
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				// the tail loop of its Draw vt+0x108 (fn_0080DB30 0x80E457..0x80E4D7 -> fn_0080B050): the shadows over it
				// right after it, inside its Z object for a queued one (callback 0x7FA980), in the main view for a fading
				// one drawn at once (0x815F62). (inferido) that the hand's mesh draw (CHand::Draw 0x46D258) runs that tail too,
				// so its shadows come before the held object (0x46D27C) and its effects (0x46D2AE)
				DrawShadowsOnObject(viewId, instance.index, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.viewId = desc.viewId;
			};
			// One PSys mesh atom (RenderContext::psysAtoms): fn_00679F20, the callback of a Sorted atom's Z object
			// (`push 0x679F20` 0x679FBC) and the draw of a Queued / Immediate one inside its effect (0x67A458): vt+0x104,
			// or vt+0x11C cut by the default plane with DrawCutByPlane (`test al, 4` 0x679F29, 0x679F4A;
			// sea_pass::CutAtoms). A translucent one (global alpha or additive) through the table 0xC387C8, an additive
			// one in mode 13 (UseAdditiveAlpha, Creators/Mesh.h); an opaque one in its materials' own modes
			const auto drawAtom = [&](const RenderContext::PSysAtomInstance& atom, RenderPass viewId) {
				// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth
				if (atom.translucent && renderCtx.instanceUniforms[atom.index][0][3] >= 1.0f)
				{
					return;
				}
				auto mesh = meshManager.Handle(atom.meshId);
				submitDesc.viewId = viewId;
				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, atom.index, 1);
				setMatrices(atom.meshId, *mesh);
				// a ParticleAnimCreator atom has its own pose
				ecs::UsePose(poses, atom.index, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.isSky = false;
				submitDesc.lightBoost = 1.0f;
				submitDesc.noHaze = false;
				ApplyLandLightMode(renderCtx, atom.meshId, submitDesc);
				submitDesc.morphWithTerrain = false;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
				submitDesc.blendFilter = 0;
				submitDesc.options = atom.translucent ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = atom.translucent ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[atom.index][0][3]);
				submitDesc.mode = atom.translucent && atom.additive
				                      ? std::optional(render_modes::Mode::AlphaTexturedAlphaAdditiveNz)
				                      : std::nullopt;
				submitDesc.sea = atom.cut ? sea_pass::CutAtoms(viewId) : sea_pass::SeaDraw {};
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				submitDesc.sea = {};
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.mode = std::nullopt;
				submitDesc.viewId = desc.viewId;
			};
			if (sortBlended)
			{
				for (const auto& [meshId, placers] : renderCtx.translucentDrawDescs)
				{
					const bool zSorted = meshManager.Handle(meshId)->IsZSorted();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth
						if (renderCtx.instanceUniforms[placers.offset + i][0][3] >= 1.0f)
						{
							continue;
						}
						const ZObject instance {.meshId = meshId,
						                        .index = placers.offset + i,
						                        .morphWithTerrain = placers.morphWithTerrain,
						                        .fading = true};
						// the sort point of the one-shot orb (OneOffSpellSeed::Draw 0x518E90): only the orbs have one, and
						// their object is always queued (vt+0x40(1) 0x72A4AA, after SetMesh 0x72A49D)
						const auto point = renderCtx.sortPoints.find(placers.offset + i);
						if (!zSorted && point == renderCtx.sortPoints.end())
						{
							// without the bit 0x10 (LH3DObject::AddDrawing 0x815F0B): Draw vt+0x108 at once (0x815F62),
							// through the table 0xC387C8. The physical shield too when its mesh lacks 0x200
							// (PhysicalShield::DrawShield: SetGlobalAlpha 0x72D0CC, AddForDrawing 0x72D0E2)
							drawInstance(instance, desc.viewId);
							continue;
						}
						// the key of LH3DObject::AddDrawing (0x815F0F..0x815F45, (x^2 + z^2) + y^2) at the orb's sort
						// point (which reaches it through Game3DObject::AddForDrawing 0x519027) or the translation
						const auto origin = point != renderCtx.sortPoints.end()
						                        ? point->second
						                        : glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						sorted.Submit(instance, zsorter::Key(origin, cameraOrigin, zsorter::SumOrder::XZY));
					}
				}
			}

			// ---- milagros2 pieces (pieces_shadows_PLAN.md §1.3 d) ----
			// RenderParticleGJMesh::DrawAt 0x67C150 of the exploded pieces (PSysGlobal::DrawLoop 0x68F60C -> fn_006718A0
			// Draw_(1)): at once, no Z object (0x67C150 does not read [0xC0215D]), after the models GGame::Draw 0x54E00A
			// draws at once and before the queue's drain (FinishFrame 0x82F460)
			if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities)
			{
				static world_triangles::Frame s_pieces; // refilled every frame, kept for its capacity
				s_pieces.Clear();
				psys::gj_mesh::Build(s_pieces, psys::DrawPath::Sorted);
				world_triangles::Submit(graphics::RenderPass::Main, s_pieces, *_shaderManager);
			}
			// ---- end milagros2 pieces ----
			// The particle effects by their draw path (psys::DrawPath; tmp_dis\miracles\polish\psys_draw_paths_verdict.md)
			const auto psysSorted = sortBlended ? psys::manager::CollectSorted() : psys::manager::SortedFrame {};
			const auto psysQueued = sortBlended ? psys::manager::CollectQueued() : std::vector<psys::manager::OrderedEffect>();
			const auto psysHand = sortBlended ? psys::manager::HandEffects() : std::vector<psys::manager::OrderedEffect>();
			const auto psysSurfaces = sortBlended ? psys::surf_revol::Collect() : std::vector<psys::surf_revol::Surface>();
			std::unordered_map<const psys::Atom*, size_t> surfaceOf;
			for (size_t i = 0; i < psysSurfaces.size(); ++i)
			{
				surfaceOf.insert_or_assign(psysSurfaces[i].atom, i);
			}
			// The pieces (Kind::GJMesh) of the Queued and Immediate effects, built once for the frame and tagged with their
			// atom: each is drawn by drawOrderedEffect at its atom's place (world_triangles::Submit's `only`)
			static world_triangles::Frame s_orderedPieces; // refilled every frame, kept for its capacity
			s_orderedPieces.Clear();
			if (sortBlended)
			{
				psys::gj_mesh::Build(s_orderedPieces, psys::DrawPath::Queued);
				psys::gj_mesh::Build(s_orderedPieces, psys::DrawPath::Immediate);
			}
			// A Queued or Immediate effect drawn all at once (fn_00679860 with [0xC0215D] = 0, 0x679884): its items in
			// fn_006798B0's order (0x6798B0..0x679912, manager::OrderedEffect). Sprites through LH3DSprite::Draw
			// (0x67B0DF), meshes through fn_00679F20 (0x67A458), mists through LH3DMist vt+0x104 (0x67A78C), surfaces
			// 0x67CBA0, pieces through RenderParticleGJMesh::DrawAt 0x67C150, chains through fn_0067B370 (0x6798DD); the
			// other kinds draw nothing here
			const auto drawOrderedEffect = [&](const psys::manager::OrderedEffect& effect, RenderPass viewId) {
				std::vector<psys::Effect::DrawAtom> sprites;
				const auto flushSprites = [&]() {
					if (!sprites.empty())
					{
						DrawPSysSprites(sprites, *desc.camera, viewId);
						sprites.clear();
					}
				};
				for (const auto& item : effect.items)
				{
					const auto* creator = item.atom.creator;
					if (item.chain < 0 && creator != nullptr && creator->kind == psys::Creator::Kind::Sprite)
					{
						sprites.push_back(item.atom);
						continue;
					}
					flushSprites();
					if (item.chain >= 0)
					{
						if (static_cast<size_t>(item.chain) < effect.chains.size())
						{
							DrawPSysChain(viewId, *desc.camera, effect.chains[static_cast<size_t>(item.chain)]);
						}
						continue;
					}
					if (creator == nullptr)
					{
						continue;
					}
					if (creator->kind == psys::Creator::Kind::GJMesh)
					{
						// RenderParticleGJMesh::DrawAt 0x67C150 of a piece atom: Draw3DWorldTriangle 0x81C090 at once, at
						// the atom's place in fn_006798B0's order (0x67C150 does not read [0xC0215D]). No data uses it
						// today: the pieces' effect, EXPLODE_OBJECT, is drawn Sorted (the block above, in Main)
						world_triangles::Submit(viewId, s_orderedPieces, *_shaderManager, item.atom.atom);
						continue;
					}
					if (creator->kind == psys::Creator::Kind::Mesh)
					{
						const auto found = renderCtx.psysAtomIndex.find(item.atom.atom);
						if (found != renderCtx.psysAtomIndex.end())
						{
							drawAtom(renderCtx.psysAtoms[found->second], viewId);
						}
						continue;
					}
					if (dynamic_cast<const psys::MistCreator*>(creator) != nullptr)
					{
						mists::MistDesc mist {};
						if (psys::mist_atoms::Describe(item.atom, mist))
						{
							DrawEffectMist(viewId, *desc.camera, mist);
						}
						continue;
					}
					if (creator->className == "ZR_SurfRevol")
					{
						const auto found = surfaceOf.find(item.atom.atom);
						if (found != surfaceOf.end())
						{
							DrawPSysSurface(viewId, psysSurfaces[found->second]);
						}
					}
				}
				flushSprites();
			};
			if (sortBlended)
			{
				// Draw_(t, 1) (fn_00679840, +0xAE = 1 at 0x67984E; Spell::Draw 0x720441 and the other Sorted sites): the
				// effect has no Z object, each element goes into the queue with its own key, (x^2 + y^2) + z^2
				// (zsorter::SumOrder::XYZ). Its ZR_SurfRevol surfaces do not read [0xC0215D] (0x67CBA0): drawn at once,
				// here after the models, unsorted (fn_0081C780 from 0x67CAEE, RendererSurfRevol.cpp)
				for (const auto& surface : psysSurfaces)
				{
					if (surface.path == psys::DrawPath::Sorted)
					{
						DrawPSysSurface(desc.viewId, surface);
					}
				}
				// each sprite: LH3DSprite::AddDrawing 0x840C70 from 0x67B0D2, keyed at the LH3DSprite's +0/+4/+8
				// (0x840C95..0x840CA3; manager::SortedFrame::sprites)
				for (size_t i = 0; i < psysSorted.sprites.size(); ++i)
				{
					sorted.Submit({.psysSprite = static_cast<int>(i)}, zsorter::Key(psysSorted.sprites[i].key, cameraOrigin));
				}
				// each mesh atom, opaque ones too: fn_00679F60 from 0x67A246, after CheckRegionOnScreen (0x679F75; here
				// the sphere around the mesh's box, (aproximado) not the box), keyed at the object's +0x38..+0x40
				// (0x679F7E..0x679FB7)
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				for (size_t i = 0; i < renderCtx.psysAtoms.size(); ++i)
				{
					const auto& atom = renderCtx.psysAtoms[i];
					if (atom.path != psys::DrawPath::Sorted)
					{
						continue;
					}
					const auto& model = renderCtx.instanceUniforms[atom.index];
					const auto box = meshManager.Handle(atom.meshId)->GetBoundingBox();
					const float scale = std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
					                              glm::length(glm::vec3(model[2]))});
					if (!SphereInView(viewProjection, glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
					                  glm::length(box.Size()) * 0.5f * scale))
					{
						continue;
					}
					sorted.Submit({.psysMesh = static_cast<int>(i)}, zsorter::Key(atom.key, cameraOrigin));
				}
				// each mist: fn_007FA7F0 from 0x67A782, its own Z object at mist +0x38 (NewZObject 0x7FA87B): they come
				// through mists::Submit (mist_atoms::SubmitFrame) and CollectMists below.
				// each chain: fn_0067B380 from 0x6798DF, keyed at the joint n / 2 (0x67B389..0x67B3D7)
				for (size_t i = 0; i < psysSorted.chains.size(); ++i)
				{
					sorted.Submit({.psysChain = static_cast<int>(i)}, zsorter::Key(psysSorted.chains[i].key, cameraOrigin));
				}
				// AddDrawing (PSysManager::AddDrawing 0x6797D0, +0xAE = 0 at 0x6797DE: the seed graphic on a ball or an
				// icon 0x51A2CA, the containers 0x63E26A; the fire's FireGraphic 0x73261D likewise): one Z object per
				// effect at GetOrigin (0x6797E5..0x679834), drawn all at once from the drain
				for (size_t i = 0; i < psysQueued.size(); ++i)
				{
					sorted.Submit({.queuedEffect = static_cast<int>(i)}, zsorter::Key(psysQueued[i].origin, cameraOrigin));
				}
			}
			if (spritesSorted)
			{
				Locator::entitiesRegistry::value().Each<const ecs::components::Sprite, const ecs::components::Transform>(
				    [&sorted, &cameraOrigin](entt::entity entity, const auto&, const ecs::components::Transform& transform) {
					    // LH3DSprite::AddDrawing 0x840C70: the sprite's +0/+4/+8, (x^2 + y^2) + z^2
					    sorted.Submit({.sprite = entity}, zsorter::Key(transform.position, cameraOrigin));
				    });
			}
			if (mistsSorted)
			{
				for (const auto& [key, index] : CollectMists(*desc.camera))
				{
					sorted.Submit({.mist = static_cast<int>(index)}, key);
				}
			}
			if (spritesSorted)
			{
				for (const auto& [key, index] : CollectChimneySmoke(*desc.camera))
				{
					sorted.Submit({.smoke = static_cast<int>(index)}, key);
				}
			}
			if (sortBlended)
			{
				// LH3DAtmos::Render3D 0x836250: one Z object per raining tile (fn_008341B0, NewZObject call 0x83427F);
				// they used to be drawn as one group after the queue
				for (const auto& [key, index] : CollectRain(*desc.camera))
				{
					sorted.Submit({.rain = static_cast<int>(index)}, key);
				}
				// the boat's LH3DSprites, each one on its own (LH3DSprite::AddDrawing 0x840CB3); they used to be one batch
				// after the queue
				for (const auto& [key, index] : CollectBoatSprites(*desc.camera))
				{
					sorted.Submit({.boat = static_cast<int>(index)}, key);
				}
			}

			// OPENBLACK_ZSORTER_TRACE=1: once a second, what the frame's queue holds (docs/bw1-notes/openblack-internals.md)
			static const bool k_ZSorterTrace = std::getenv("OPENBLACK_ZSORTER_TRACE") != nullptr;
			static uint32_t zsorterTraceFrame = 0;
			if (k_ZSorterTrace && sortBlended && ++zsorterTraceFrame % 60 == 0)
			{
				std::array<int, 14> counts {};
				const auto ordered = sorted.Ordered();
				for (const auto& entry : ordered)
				{
					const auto& z = *entry.item;
					const size_t kind = z.cloud >= 0          ? 1
					                    : z.rain >= 0         ? 2
					                    : z.boat >= 0         ? 3
					                    : z.psysSprite >= 0   ? 4
					                    : z.psysMesh >= 0     ? 5
					                    : z.psysChain >= 0    ? 6
					                    : z.queuedEffect >= 0 ? 7
					                    : z.mist >= 0         ? 8
					                    : z.smoke >= 0        ? 9
					                    : z.sprite != entt::null ? 10
					                    : z.meshId == ecs::components::Hand::k_MeshId && !z.fading ? 11
					                    : z.fading                                                 ? 12
					                    : z.IsModel()                                              ? 0
					                                                                               : 13;
					++counts.at(kind);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
				                   "ZSorter trace: {} entries ({} dropped): models {}, fading {}, clouds {}, rain tiles {}, "
				                   "boat sprites {}, PSys sprites {}, PSys meshes {}, PSys chains {}, queued effects {}, "
				                   "mists {}, smoke {}, sprites {}, hand {}; farthest key {:.1f}, nearest {:.1f}",
				                   ordered.size(), sorted.Dropped(), counts[0], counts[12], counts[1], counts[2], counts[3],
				                   counts[4], counts[5], counts[6], counts[7], counts[8], counts[9], counts[10], counts[11],
				                   ordered.empty() ? 0.0f : ordered.front().key, ordered.empty() ? 0.0f : ordered.back().key);
			}

			// OPENBLACK_ORB_TRACE=1: one line a drawn frame per one-shot orb (docs/bw1-notes/openblack-internals.md), to
			// follow the bubble across the 15 -> 0 wrap of its 4 x 4 sheet (OneOffSpellSeed::UpdateFrame 0x72A570)
			static const bool k_OrbTrace = std::getenv("OPENBLACK_ORB_TRACE") != nullptr;
			if (k_OrbTrace && sortBlended && Locator::entitiesRegistry::has_value())
			{
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				// the queue in draw order; the key printed is the distance (the root of the queue's key) as before
				const auto ordered = sorted.Ordered();
				// the ZR_SurfRevol discs: a Sorted effect's at once, before the whole queue (so before every bubble); a
				// Queued one's inside its effect's entry
				for (const auto& surface : psysSurfaces)
				{
					int at = -1;
					for (size_t k = 0; k < ordered.size() && surface.path != psys::DrawPath::Sorted; ++k)
					{
						const int effect = ordered[k].item->queuedEffect;
						if (effect >= 0 && psysQueued[static_cast<size_t>(effect)].effect == surface.effect)
						{
							at = static_cast<int>(k);
							break;
						}
					}
					SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
					                   "Orb trace: surface ({}) path {} sorted {}/{} origin ({:.1f}, {:.1f}, {:.1f})", surface.texture,
					                   static_cast<int>(surface.path), at, static_cast<int>(ordered.size()), surface.origin.x,
					                   surface.origin.y, surface.origin.z);
				}
				Locator::entitiesRegistry::value().Each<const ecs::components::OneOffSpellSeed, const ecs::components::Mesh>(
				    [&](entt::entity entity, const ecs::components::OneOffSpellSeed& orb, const ecs::components::Mesh& mesh) {
					    const auto found = renderCtx.entityInstances.find(entity);
					    if (found == renderCtx.entityInstances.end())
					    {
						    SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Orb trace: orb {} has no instance this frame",
						                       static_cast<uint32_t>(entity));
						    return;
					    }
					    const auto index = found->second.index;
					    const auto& model = renderCtx.instanceUniforms[index];
					    int at = -1;
					    float key = -1.0f;
					    for (size_t k = 0; k < ordered.size(); ++k)
					    {
						    if (ordered[k].item->fading && ordered[k].item->index == index && ordered[k].item->meshId == mesh.id)
						    {
							    at = static_cast<int>(k);
							    key = std::sqrt(ordered[k].key);
							    break;
						    }
					    }
					    // the gate of the sorted list: a fully faded instance ([0][3] >= 1) is left out; nothing culls the
					    // translucent instances by frustum, so SphereInView is only reported
					    const float alpha = 1.0f - model[0][3];
					    auto l3d = meshManager.Handle(mesh.id);
					    const auto box = l3d->GetBoundingBox();
					    const float radius = glm::length(box.Size()) * 0.5f;
					    const bool inView =
					        SphereInView(viewProjection, glm::vec3(model * glm::vec4(box.Center(), 1.0f)), radius);
					    SPDLOG_LOGGER_INFO(
					        spdlog::get("graphics"),
					        "Orb trace: orb {} phase {:.4f} frame {} packed[1][3] {:.6f} uv ({:.3f}, {:.3f}) alpha {:.3f} "
					        "sorted {}/{} key {:.2f} sortPoint ({:.1f}, {:.1f}, {:.1f}) inView {}",
					        static_cast<uint32_t>(entity), orb.phase, static_cast<int>(orb.phase), model[1][3],
					        static_cast<float>(static_cast<int>(orb.phase) % 4) * 0.25f,
					        static_cast<float>(static_cast<int>(orb.phase) / 4) * 0.25f, alpha, at,
					        static_cast<int>(ordered.size()), key, orb.sortPoint.x, orb.sortPoint.y, orb.sortPoint.z, inView);
				    });
			}

			// The drain, far to near (fn_0082F280; the full queue dropped the entries over 0x800, NewZObject 0x83F31C), in
			// its own view right after the main pass (same target and camera, no clear), so that nothing drawn at once in
			// the main pass is drawn over them
			if (!sorted.Empty())
			{
				constexpr auto k_Blended = graphics::RenderPass::MainBlended;
				auto& spriteRegistry = Locator::entitiesRegistry::value();
				const auto drained = sorted.Drain();
				bool handEffectsDrawn = false;
				std::vector<psys::Effect::DrawAtom> sprites;
				for (size_t e = 0; e < drained.size(); ++e)
				{
					const auto& instance = *drained[e].item;
					if (instance.cloud >= 0)
					{
						DrawCloud(k_Blended, *desc.camera, static_cast<uint32_t>(instance.cloud));
						continue;
					}
					if (instance.rain >= 0)
					{
						DrawRainTile(k_Blended, static_cast<uint32_t>(instance.rain));
						continue;
					}
					if (instance.boat >= 0)
					{
						DrawBoatSprite(k_Blended, static_cast<uint32_t>(instance.boat));
						continue;
					}
					if (instance.psysSprite >= 0)
					{
						// a Sorted effect's sprite, LH3DSprite::Draw from its Z object; the next entries that are sprites too
						// go in the same call (DrawPSysSprites keeps their order and batches only equal materials)
						sprites.clear();
						sprites.push_back(psysSorted.sprites[static_cast<size_t>(instance.psysSprite)].atom);
						while (e + 1 < drained.size() && drained[e + 1].item->psysSprite >= 0)
						{
							++e;
							sprites.push_back(psysSorted.sprites[static_cast<size_t>(drained[e].item->psysSprite)].atom);
						}
						DrawPSysSprites(sprites, *desc.camera, k_Blended);
						continue;
					}
					if (instance.psysMesh >= 0)
					{
						drawAtom(renderCtx.psysAtoms[static_cast<size_t>(instance.psysMesh)], k_Blended);
						continue;
					}
					if (instance.psysChain >= 0)
					{
						// 0x67B3F0, the callback of fn_0067B380's Z object
						DrawPSysChain(k_Blended, *desc.camera, psysSorted.chains[static_cast<size_t>(instance.psysChain)].chain);
						continue;
					}
					if (instance.queuedEffect >= 0)
					{
						// fn_00679860, the callback of PSysManager::AddDrawing's Z object (0x67982D)
						drawOrderedEffect(psysQueued[static_cast<size_t>(instance.queuedEffect)], k_Blended);
						continue;
					}
					if (instance.mist >= 0)
					{
						DrawMist(k_Blended, *desc.camera, static_cast<uint32_t>(instance.mist));
						continue;
					}
					if (instance.smoke >= 0)
					{
						DrawChimneySmoke(k_Blended, *desc.camera, static_cast<uint32_t>(instance.smoke));
						continue;
					}
					if (instance.sprite != entt::null)
					{
						const auto& [sprite, transform] =
						    spriteRegistry.Get<const ecs::components::Sprite, const ecs::components::Transform>(instance.sprite);
						drawSprite(sprite, transform, k_Blended);
						continue;
					}
					if (!instance.IsModel())
					{
						continue;
					}
					drawInstance(instance, k_Blended);
					// CHand::Draw 0x46D210: after the hand's mesh (0x46D258) and the held object (0x46D27C),
					// DrawSpellInHand (0x46D2AE -> 0x46E680) draws the hand's effects with Draw_(1.0, 0) (0x46E76A): at
					// once, in their own order (manager::HandEffects, DrawPath::Immediate). The held object is not drawn
					// from the hand's entry here (inferido: no visible difference, it is opaque and drawn before)
					if (instance.meshId == ecs::components::Hand::k_MeshId && !instance.fading && !handEffectsDrawn)
					{
						handEffectsDrawn = true;
						for (const auto& effect : psysHand)
						{
							drawOrderedEffect(effect, k_Blended);
						}
					}
				}
			}
			// The effects' ribbons (fn_0067B3F0) and surfaces (0x67CBA0), the rain tiles, the boat's sprites and the
			// clouds are no longer groups of their own: they all went at once or through the queue above. The shadows on the objects
			// (fn_0080B050) went with their objects: right after each one drawn at once, inside the Z object of each queued
			// one (RendererShadows.cpp). A receiver not drawn this frame (out of view, faded out, past the queue's 0x800)
			// gets none, as the tail of a Draw that did not run (0x80E457..0x80E4D7)
			ClearShadowReceivers();

			// Debug
			if (desc.viewId == graphics::RenderPass::Main)
			{
				for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
				{
					auto mesh = meshManager.Handle(meshId);
					if (!mesh->ContainsLandscapeFeature() || mesh->GetFootprints().empty())
					{
						continue;
					}
				}
				if (renderCtx.boundingBox)
				{
					const auto boundBoxOffset = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					const auto boundBoxCount = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					renderCtx.boundingBox->GetVertexBuffer().Bind();
					bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), boundBoxOffset, boundBoxCount);
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShaderInstanced->GetRawHandle()));
				}
				if (renderCtx.footpaths)
				{
					renderCtx.footpaths->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShader->GetRawHandle()));
				}
				if (renderCtx.streams)
				{
					renderCtx.streams->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShader->GetRawHandle()));
				}
			}
		}

		{
			auto subSection =
			    profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSprites
			                                                               : Profiler::Stage::MainPassDrawSprites);

			// In the main pass the sprites went through the back-to-front list with the blended models
			if (desc.drawSprites && !spritesSorted)
			{
				using namespace ecs::components;

				auto& registry = Locator::entitiesRegistry::value();
				registry.Each<const Sprite, const Transform>(
				    [&drawSprite, &desc](const Sprite& sprite, const Transform& transform) {
					    drawSprite(sprite, transform, desc.viewId);
				    });
			}
		}
	}

	if (desc.drawSky && desc.viewId == graphics::RenderPass::Main)
	{
		if (!mistsSorted)
		{
			// without the entities the models' section did not drain the queue: the mists join the clouds in it here and
			// it is drained (fn_0082F280); nothing else can be in it
			for (const auto& [key, index] : CollectMists(*desc.camera))
			{
				sorted.Submit({.mist = static_cast<int>(index)}, key);
			}
			for (const auto& entry : sorted.Drain())
			{
				if (entry.item->cloud >= 0)
				{
					DrawCloud(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(entry.item->cloud));
				}
				else if (entry.item->mist >= 0)
				{
					DrawMist(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(entry.item->mist));
				}
			}
		}
		DrawSun(graphics::RenderPass::MainBlended, *desc.camera, true);
	}

	// Enable stats or debug text.
	auto debugMode = BGFX_DEBUG_NONE;
	if (_bgfxDebug)
	{
		debugMode |= BGFX_DEBUG_STATS;
	}
	if (desc.wireframe)
	{
		debugMode |= BGFX_DEBUG_WIREFRAME;
	}
	if (_bgfxProfile)
	{
		debugMode |= BGFX_DEBUG_PROFILER;
	}
	bgfx::setDebug(debugMode);
}

void Renderer::Frame() noexcept
{
	// Advance to next frame. Process submitted rendering primitives.
	bgfx::frame();
}

void Renderer::RequestScreenshot(const std::filesystem::path& filepath) noexcept
{
	const bgfx::FrameBufferHandle mainBackbuffer = BGFX_INVALID_HANDLE;
	bgfx::requestScreenShot(mainBackbuffer, filepath.string().c_str());
}
