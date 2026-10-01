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
#include "3D/SkyWeather.h"
#include "3D/NightLights.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/ScreenFade.h"
#include "3D/SkyInterface.h"
#include "Camera/Camera.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/DrawPosition.h"
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
#include "Graphics/PhysicsShadows.h"
#include "Graphics/Primitive.h"
#include "Graphics/ShaderManager.h"
#include "Game.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"
#include "Profiler.h"
#include "Renderer.h"

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
    , _physicsShadows(std::make_unique<PhysicsShadows>())
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
	_handShadowFrameBuffer.reset(); // before bgfx::shutdown
	_physicsShadows.reset();
	if (bgfx::isValid(_landLightTexture))
	{
		bgfx::destroy(_landLightTexture);
	}
	if (bgfx::isValid(_landCellsTexture))
	{
		bgfx::destroy(_landCellsTexture);
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

const Texture2D* GetTexture(uint32_t skinID, const std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>>& meshSkins)
{
	const auto& textureManager = Locator::resources::value().GetTextures();

	const Texture2D* texture = nullptr;

	if (skinID != 0xFFFFFFFF)
	{
		if (meshSkins.find(skinID) != meshSkins.end())
		{
			texture = meshSkins.at(skinID).get();
		}
		else if (textureManager.Contains(skinID))
		{
			texture = &*textureManager.Handle(skinID);
		}
		else if (!meshSkins.empty())
		{
			// Some modded packs embed a mesh's skin under a placeholder id (0x1001) while its material still names a
			// pack texture that the pack does not have: use the mesh's own skin.
			texture = meshSkins.begin()->second.get();
		}
		else
		{
			static std::unordered_set<uint32_t> reported;
			if (reported.insert(skinID).second)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Could not find the texture {:#x}", skinID);
			}
		}
	}

	return texture;
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

	auto const& skins = mesh.GetSkins();
	bool lastPreserveState = false;
	// MSAA mod: smooth alpha cut-out edges in the multisampled opaque passes (not in blended ones)
	const bool alphaToCoverage = Locator::config::value().msaa != 0 && desc.viewId != RenderPass::Reflection &&
	                             (desc.state & BGFX_STATE_BLEND_MASK) == 0;
	const auto& primitives = subMesh.GetPrimitives();
	for (auto it = primitives.begin(); it != primitives.end(); ++it)
	{
		const auto& prim = *it;

		const bool hasNext = std::next(it) != primitives.end();

		const Texture2D* texture = GetTexture(prim.skinID, skins);
		const Texture2D* nextTexture = !hasNext ? nullptr : GetTexture(std::next(it)->skinID, skins);

		// Material blending of the original (L3D material type): AlphaTextured & co. blend with the texture alpha, e.g.
		// the fading wrist of the hand and the soft edges of buildings. Chroma materials stay alpha tested.
		const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
		const auto sameMaterial = [&prim](const L3DSubMesh::Primitive& other) {
			return other.blend == prim.blend && other.thresholdAlpha == prim.thresholdAlpha &&
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
			if (!desc.isSky)
			{
				const bool lit = _landLight && _landLight->IsLoaded();
				const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
				const glm::vec4 u_cellMap = {extent.minimum, cellMapSize};
				// x: 0 white, 1 lit like the original, 2 unlit constant colour z (the hand's reflection), 3 the land colour
				// only, 4 DrawCutByPlane (y: colour alpha, z: colour r 65536 + g 256 + b)
				// w: 1 = no haze + 2 x the land light mode (land_light::ObjectMode)
				glm::vec4 u_objectLight = {desc.unlitColour >= 0.0f ? 2.0f : (lit ? (desc.landColourOnly ? 3.0f : 1.0f) : 0.0f),
				                           desc.lightBoost, desc.unlitColour,
				                           (desc.noHaze ? 1.0f : 0.0f) + 2.0f * static_cast<float>(desc.landLightMode)};
				if (desc.cutByPlane != 0)
				{
					u_objectLight = {4.0f, static_cast<float>(desc.cutColour >> 24) / 255.0f,
					                 static_cast<float>(desc.cutColour & 0x00FFFFFFu), 1.0f};
				}
				// x: 1 discard y < 0, -1 discard y > 0 (fs); y > 0: mirrored in y = 0 (vs)
				const glm::vec4 u_objectClip = {desc.cutByPlane != 0 ? static_cast<float>(desc.cutByPlane)
				                                                     : (desc.clipBelowSea ? 1.0f : 0.0f),
				                                desc.mirrorInSea ? 1.0f : 0.0f, 0.0f, 0.0f};
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
				// y, z: mod graphics.hd-tweaks on villagers lit like the original (lighting mode, mip bias; fs_object)
				const auto& config = Locator::config::value();
				const bool person = subMesh.IsHdTweaked() && desc.instanceDesc != nullptr && lit && !desc.landColourOnly &&
				                    desc.unlitColour < 0.0f && desc.cutByPlane == 0;
				// w: 1 if the primitive takes the object's texture offset (L3DSubMesh::Primitive::uvOffset)
				const glm::vec4 u_window = {subMesh.GetFlags().isWindow ? 1.0f : 0.0f,
				                            person ? static_cast<float>(config.hdTweaksLighting) : 0.0f,
				                            person ? config.hdTweaksMipBias : 0.0f, prim.uvOffset ? 1.0f : 0.0f};
				program->SetUniformValue("u_window", &u_window);                      // vs
				const glm::vec4 u_materialColour = {glm::vec3(prim.colour), texture == nullptr ? 1.0f : 0.0f};
				program->SetUniformValue("u_materialColour", &u_materialColour);      // fs
				if (desc.dynamicShadow != nullptr)
				{
					program->SetTextureSampler("s_dynamicShadow", 5, *desc.dynamicShadow);
					program->SetUniformValue("u_dynamicShadowBox", &desc.dynamicShadowBox);
					program->SetUniformValue("u_dynamicShadow", &desc.dynamicShadowParams);
				}
			}
			if (!desc.isSky)
			{
				const glm::vec4 u_skyAlphaThreshold = {
				    Locator::skySystem::value().GetCurrentSkyType(),
				    prim.thresholdAlpha ? prim.alphaCutoutThreshold : 0.0f,
				    alphaToCoverage && prim.thresholdAlpha ? 1.0f : 0.0f,
				    blended ? 1.0f : 0.0f,
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
			if (subMesh.GetMesh().IsIndexed() && (skip & Mesh::SkipState::SkipIndexBuffer) == 0)
			{
				subMesh.GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			if ((skip & Mesh::SkipState::SkipVertexBuffer) == 0)
			{
				subMesh.GetMesh().GetVertexBuffer().Bind();
			}
			auto viewId = desc.viewId;
			auto state = desc.state;
			if (!desc.isSky && (state & BGFX_STATE_CULL_MASK) == 0 && !prim.twoSided)
			{
				// D3DCULL_CCW of the original (0x84C34A) is bgfx's CCW here; the mirrored reflection camera flips it (and a
				// mesh mirrored in the sea flips it back)
				state |= viewId == RenderPass::Reflection && !desc.mirrorInSea ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW;
			}
			if (blended && (state & BGFX_STATE_BLEND_MASK) == 0)
			{
				// Drawn after every opaque model (MainBlended) so what lies behind is already in the target
				state &= ~(BGFX_STATE_WRITE_A | (prim.depthWrite ? 0 : BGFX_STATE_WRITE_Z));
				state |= prim.blend == L3DSubMesh::Primitive::BlendMode::Additive
				             ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
				             : BGFX_STATE_BLEND_ALPHA;
				if (viewId == RenderPass::Main)
				{
					viewId = RenderPass::MainBlended;
				}
			}
			else if (blended && prim.blend == L3DSubMesh::Primitive::BlendMode::Additive &&
			         (state & BGFX_STATE_BLEND_MASK) == BGFX_STATE_BLEND_ALPHA)
			{
				// An object drawn with its own alpha (components::Alpha: SetGlobalAlpha, mode table 0xC387C8) keeps the
				// additive modes 10..13 of its additive primitives (SRCALPHA / ONE), 11 and 13 without Z write: the one-shot
				// orb's bubble (mode 12 by GJUtils::SetMaterialProperties, Game.cpp)
				state = (state & ~BGFX_STATE_BLEND_MASK) | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
				if (!prim.depthWrite)
				{
					state &= ~BGFX_STATE_WRITE_Z;
				}
			}
			if (prim.thresholdAlpha && !alphaToCoverage && (state & BGFX_STATE_BLEND_MASK) == 0)
			{
				// Chroma materials (fn_0082E080 & co.): alpha test and SRCALPHA / INVSRCALPHA blending, drawn in the
				// normal (unsorted) model order like the original
				state |= BGFX_STATE_BLEND_ALPHA;
			}
			if ((skip & Mesh::SkipState::SkipRenderState) == 0)
			{
				const auto a2c = alphaToCoverage && prim.thresholdAlpha ? BGFX_STATE_BLEND_ALPHA_TO_COVERAGE : 0;
				bgfx::setState(state | a2c, desc.rgba);
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
			    matrices.push_back(glm::translate(transform.position) * glm::mat4(transform.rotation) *
			                       glm::scale(transform.scale));
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
	_landLight->Build(Locator::skySystem::value().GetCurrentSkyType(), _skyAlignment.Get(),
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
		const auto& skins = mesh->GetSkins();
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
				const auto* texture = GetTexture(prim.skinID, skins);
				const glm::vec4 u_shadowParams = {prim.thresholdAlpha ? prim.alphaCutoutThreshold : 0.0f,
				                                  texture != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f};
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
	auto model = glm::translate(position) * glm::rotate(-3.0f * glm::pi<float>() / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto& texture = *textures.Handle(k_SunTexture);
	// mode 13: additive SRCALPHA / ONE, colour and alpha = texture x diffuse, no Z write, cull none
	const uint64_t additive = BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
	if (!glare)
	{
		const glm::vec4 colour(glm::vec3(0x95, 0x7C, 0x63) / 255.0f, alpha / 255.0f);
		DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour, additive | BGFX_STATE_DEPTH_TEST_GREATER);
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

void Renderer::DrawMoon(graphics::RenderPass viewId, const Camera& camera, bool mirrored) const
{
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
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER |
			               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
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
	const auto mainView = mirrored ? frame.view * glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) : frame.view;
	const auto mainInverseView = mirrored ? glm::inverse(mainView) : frame.inverseView;
	const auto model = billboard::MoonModel(billboard::MoonBasis(mainView, mainInverseView, centre), centre, phase);
	const glm::vec4 moonColour(colour, m / 255.0f);
	const glm::vec4 celestial(std::cos(phase), std::sin(phase), 1.0f, 1.0f);
	DrawCelestialMesh(viewId, sky.GetMoonMesh(), model, *textures.Handle(k_Weather), moonColour,
	                  BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA |
	                      (mirrored ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW),
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
	// game time: the clouds and their animation stop while the game is paused
	static auto lastTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	// g_game_time_inc: game time, faster or slower with the game speed
	const float speed = Game::Instance() != nullptr ? Game::Instance()->GetGameSpeed() : 1.0f;
	const float milliseconds = std::min(100.0f, std::chrono::duration<float, std::milli>(now - lastTime).count() / speed);
	lastTime = now;
	const bool running = Game::Instance() != nullptr && !Game::Instance()->IsPaused();
	if (running)
	{
		_clouds->Update(milliseconds);
	}
	// DrawClouds advances the animation counters of the clouds it draws by this step
	_cloudMilliseconds = running ? milliseconds : 0.0f;
	// GLandAlignement::DrawSky 0x5E2160: the sky's alignment moves towards the most influential player's
	_skyAlignment.Update(Clouds::InfluentialPlayerAlignment(), running ? milliseconds : 0.0f);

	// fn_005E1DE0 (called by DrawSky): the colour and the alpha byte from the sky's alignment and light table[255]
	const uint32_t table255 = _landLight && _landLight->IsLoaded() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	const uint32_t colour = Clouds::Colour(_skyAlignment.Get(), table255);
	_cloudRgb = glm::vec3((colour >> 16) & 0xFFu, (colour >> 8) & 0xFFu, colour & 0xFFu) / 255.0f;
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

void Renderer::DrawClouds(graphics::RenderPass viewId, const Camera& camera) const
{
	if (!_clouds || !GetDetailLevel(Locator::config::value().detailLevel).clouds)
	{
		return;
	}
	const auto rgb = _cloudRgb;

	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	if (mesh.GetNumSubMeshes() == 0)
	{
		return;
	}
	const auto origin = camera.GetOrigin();
	// the same billboard as the map mists (Renderer::DrawMist): 0xEA1C98 after its in-place inverse fn_007FB3F0
	// (0x819AF3), in glm mat3(right, -forward, up): local X = screen right, local Y (the dome's axis) towards the
	// camera, local Z = screen up (billboard::MistBasis)
	const auto cameraFrame = billboard::CameraFrame::From(camera);
	const auto& rotation = billboard::MistBasis(cameraFrame);
	std::vector<std::pair<float, size_t>> order;
	order.reserve(_clouds->GetClouds().size());
	for (size_t i = 0; i < _clouds->GetClouds().size(); ++i)
	{
		order.emplace_back(glm::distance(Clouds::WorldPosition(_clouds->GetClouds()[i]), origin), i);
	}
	std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

	// LH3DMist::AddDrawing 0x7FA7F0 (vt+0x100 of the cloud objects, called by fn_005E25C0): only a cloud whose sphere
	// (the mesh's bounding-box half diagonal x size x 0.55) touches the screen is drawn and advances its counter
	const float meshRadius = glm::length(mesh.GetBoundingBox().Size()) * 0.5f;
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	const float milliseconds = _cloudMilliseconds;
	_cloudMilliseconds = 0.0f;
	// The clouds are LH3DMist objects too (+0x88 the size, +0x8C the shrink), so their draw is the effect branch of the
	// same fn_007FA300: no specular (+0x50 is never written) and the temporary light straight above.
	const glm::vec4 u_cloudSpecular(0.0f);
	const auto* program = _shaderManager->GetShader("Cloud");
	// mist.l3d is loaded without skins; LH3DMist::Draw (fn_007FA300) uses the smoke material instead
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Smoke = entt::hashed_string("raw/smoke");
	static const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	for (const auto& [distance, index] : order)
	{
		const auto& cloud = _clouds->GetClouds()[index];
		const auto position = Clouds::WorldPosition(cloud);
		// 0x7FA4DC..0x7FA539: row 0 (local X, the screen width) is scaled by the size and rows 1-2 (local Y = depth,
		// local Z = screen height) by the shrunk one, so a cloud is round only straight overhead and near the horizon
		// it is about k (2.5 to 5, CloudInSky::Open 0x5E23F0) times wider than tall (billboard::MistShrunkSize)
		const float shrunk = billboard::MistShrunkSize(cloud.size, cloud.k, position - origin);
		const auto model =
		    glm::translate(position) * glm::mat4(rotation) * glm::scale(glm::vec3(cloud.size, shrunk, shrunk));
		const glm::vec4 u_cloudColour(rgb, _cloudAlpha[index] / 255.0f);
		if (u_cloudColour.a <= 0.0f || !SphereInView(viewProjection, position, meshRadius * cloud.size * 0.55f))
		{
			continue;
		}
		_clouds->AdvanceAnimation(index, milliseconds);
		// fn_007FA300 0x7FA3F4..0x7FA466: one whole atlas cell, rows 2-3 (the frame after this frame's step;
		// frame_anim::MistCellUv of the effect branch)
		const auto cell = frame_anim::MistCellUv(Clouds::GetFrame(_clouds->GetClouds()[index]), true);
		const glm::vec4 u_cloud(cell.x, cell.y, 210.0f / 256.0f, 0.0f);
		// fn_00855340: the light's position brought into the mesh's own space, normalised (the light is at (0, 500000, 0))
		const glm::vec4 u_cloudLight(glm::normalize(glm::inverse(glm::mat3(model)) * (glm::vec3(0.0f, 500000.0f, 0.0f) - position)),
		                             0.0f);
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
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
				bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
			}
		}
	}
}

void Renderer::DrawHandShadowPass(const DrawSceneDesc& drawDesc) const
{
	_handShadowParams = glm::vec4(0.0f);
	if (!drawDesc.drawIsland || !drawDesc.drawEntities || !Locator::handSystem::has_value())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto desc = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
	const auto* bones = Locator::handSystem::value().GetBoneMatrices();
	if (desc == renderCtx.instancedDrawDescs.end() || desc->second.count == 0 || bones == nullptr ||
	    desc->second.offset >= renderCtx.instanceUniforms.size())
	{
		return;
	}
	const auto mesh = Locator::resources::value().GetMeshes().Handle(ecs::components::Hand::k_MeshId);
	if (mesh->GetBoneMatrices().size() != bones->size())
	{
		return;
	}
	if (!_handShadowFrameBuffer)
	{
		// the original's silhouette is 32 x 32 with 4 x 2 subsamples per texel; 64 x 64 sampled bilinearly is as soft
		_handShadowFrameBuffer = std::make_unique<FrameBuffer>("HandShadow", 64, 64, TextureFormat::R8);
	}
	// the hand's transform (its pose is in the bone matrices, the instance only carries its scale and origin)
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto& handTransform = Locator::entitiesRegistry::value().Get<ecs::components::Transform>(hand);
	const glm::vec3 position = handTransform.position;
	const float scale = handTransform.scale.x;
	const float radius = 0.5f * glm::length(mesh->GetBoundingBox().Size()) * scale;
	const auto& island = Locator::terrainSystem::value();
	const float ground = island.GetHeightAt(glm::vec2(position.x, position.z));

	// fn_00874600: full up to 50 radii from the camera, gone at 80
	const float q = glm::distance(drawDesc.camera->GetOrigin(), glm::vec3(position.x, ground, position.z)) /
	                std::max(radius, 0.001f);
	const float fade = q < 50.0f ? 1.0f : std::max(0.0f, 1.0f - (q - 50.0f) / 30.0f);
	if (fade <= 0.0f)
	{
		return;
	}
	// the hand's light is straight above it (+200, CHand::PrepareForDrawing sets [obj+0xBC])
	const glm::vec4 light(position + glm::vec3(0.0f, 200.0f, 0.0f), ground);
	const float extent = radius * 2.0f;
	_handShadowBox = glm::vec4(position.x - extent, position.z - extent, 1.0f / (2.0f * extent), 1.0f / (2.0f * extent));
	_handShadowParams = glm::vec4(8.0f / 15.0f * fade, ground, 0.0f, 0.0f);

	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::DynamicShadow);
	_handShadowFrameBuffer->Bind(graphics::RenderPass::DynamicShadow);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::setViewRect(viewId, 0, 0, 64, 64);
	bgfx::touch(viewId);
	const auto* program = _shaderManager->GetShader("DynamicShadowInstanced");
	constexpr uint64_t k_State = BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
	                             BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
	const glm::vec4 u_shadowParams(0.0f);
	const glm::vec4 u_shadowSlot(0.0f, 0.0f, 1.0f, 0.0f);
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
		{
			continue;
		}
		for (const auto& prim : subMesh->GetPrimitives())
		{
			program->SetUniformValue("u_shadowSlot", &u_shadowSlot);
			program->SetUniformValue("u_shadowLight", &light);
			program->SetUniformValue("u_shadowBox", &_handShadowBox);
			program->SetUniformValue("u_shadowParams", &u_shadowParams);
			bgfx::setTransform(bones->data(), static_cast<uint16_t>(bones->size()));
			// both player hands share the mesh; the one outside the box leaves nothing
			bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), desc->second.offset, desc->second.count);
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
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	submitDesc.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
	submitDesc.landColourOnly = true;
	submitDesc.clipBelowSea = true;
	for (const auto& [entity, bodyRadius, centreY] : objects)
	{
		const auto instance = renderCtx.entityInstances.find(entity);
		if (!registry.Valid(entity) || instance == renderCtx.entityInstances.end() || !meshes.Contains(instance->second.meshId))
		{
			continue;
		}
		const auto mesh = meshes.Handle(instance->second.meshId);
		if (bodyRadius >= 0.0f)
		{
			const auto& transform = registry.Get<ecs::components::Transform>(entity);
			const float radius =
			    bodyRadius > 0.0f ? bodyRadius : 0.5f * glm::length(mesh->GetBoundingBox().Size()) * transform.scale.x;
			if ((bodyRadius > 0.0f ? centreY : transform.position.y) <= -radius)
			{
				continue;
			}
		}
		submitDesc.instanceDesc =
		    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1);
		static const auto k_Identity = glm::mat4(1.0f);
		submitDesc.modelMatrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity;
		submitDesc.matrixCount = mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1;
		submitDesc.morphWithTerrain = instance->second.morphWithTerrain;
		submitDesc.program = land_morph::ObjectProgram(*_shaderManager, submitDesc.morphWithTerrain);
		DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
}

void Renderer::DrawHandShadowOnObjects() const
{
	// "ShadowsOnObjects" detail key; only the hand's (and the creature's) shadow holder falls on objects (si+0xC == 0)
	if (_handShadowParams.x <= 0.0f || !_handShadowFrameBuffer ||
	    !GetDetailLevel(Locator::config::value().detailLevel).shadowsOnObjects)
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto held = Locator::handSystem::value().GetHeldObject();
	// si+0x2C: the shadow box {x0, z0, x1, z1}
	const glm::vec2 boxMin(_handShadowBox.x, _handShadowBox.y);
	const glm::vec2 boxMax = boxMin + 1.0f / glm::vec2(_handShadowBox.z, _handShadowBox.w);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = graphics::RenderPass::MainBlended;
	// fn_0080B050: mode 6 (no Z write) with ZFUNC EQUAL over the object as it was drawn
	submitDesc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_EQUAL | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
	submitDesc.dynamicShadow = &_handShadowFrameBuffer->GetColorAttachment();
	submitDesc.dynamicShadowBox = _handShadowBox;
	submitDesc.dynamicShadowParams = _handShadowParams;
	for (const auto& [entity, instance] : renderCtx.entityInstances)
	{
		if (!instance.receivesDynamicShadow || (held.has_value() && *held == entity) || !meshes.Contains(instance.meshId))
		{
			continue;
		}
		const auto mesh = meshes.Handle(instance.meshId);
		// ContainsThisBoundingBox (fn_007F9E80): the mesh box centre +- half its size, moved to the object, x and z only
		const auto& box = mesh->GetBoundingBox();
		const auto& transform = registry.Get<ecs::components::Transform>(entity);
		const glm::vec2 centre =
		    glm::vec2(box.Center().x, box.Center().z) + glm::vec2(transform.position.x, transform.position.z);
		const glm::vec2 half = glm::vec2(box.Size().x, box.Size().z) * 0.5f;
		if (centre.x + half.x < boxMin.x || centre.x - half.x > boxMax.x || centre.y + half.y < boxMin.y ||
		    centre.y - half.y > boxMax.y)
		{
			continue;
		}
		submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance.index, 1);
		if (mesh->IsBoned())
		{
			submitDesc.modelMatrices = mesh->GetBoneMatrices().data();
			submitDesc.matrixCount = static_cast<uint8_t>(mesh->GetBoneMatrices().size());
		}
		else
		{
			static const auto k_Identity = glm::mat4(1.0f);
			submitDesc.modelMatrices = &k_Identity;
			submitDesc.matrixCount = 1;
		}
		submitDesc.morphWithTerrain = instance.morphWithTerrain;
		submitDesc.program = land_morph::ObjectProgram(*_shaderManager, instance.morphWithTerrain, land_morph::ObjectPass::Shadow);
		DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
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
			// mirrored in y = 0 for the reflection target (see DrawPass)
			sprite.position = glm::vec3(fish.position.x, -fish.position.y, fish.position.z);
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
	// mode 6: SRCALPHA / INVSRCALPHA, no Z write; two-sided. The mirrored land under them wrote no Z in the original.
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
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
		const uint32_t r = (ring.argb >> 16) & 0xFFu;
		const uint32_t g = (ring.argb >> 8) & 0xFFu;
		const uint32_t b = ring.argb & 0xFFu;
		const uint32_t abgr = (alpha << 24) | (b << 16) | (g << 8) | r;
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
	// mode 13: SRCALPHA / ONE, no Z write
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
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
		    // the two feet: bone matrix slots 21 and 18 (ends of the leg chains), on the ground + 0.2
		    auto model = glm::translate(draw != nullptr ? draw->position : transform.position) *
		                 glm::mat4(draw != nullptr ? draw->rotation : transform.rotation) * glm::scale(transform.scale);
		    const auto foot = [&](size_t bone) {
			    auto p = glm::vec3(model * bones[bone] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			    p.y = land_morph::OnGround(ground, glm::vec2(p.x, p.z), land_morph::k_BlobLift);
			    return p;
		    };
		    const auto a = foot(21);
		    const auto b = foot(18);
		    // the light offset projected onto the land's plane: D = O s - ((O s) . n) n
		    const auto n = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto os = o * transform.scale.x;
		    const auto d = os - glm::dot(os, n) * n;
		    addQuad(a, d + (b - a) * 0.5f);
		    addQuad(b, d + (a - b) * 0.5f);
	    });
	// Animals (IsHumanShadowed, flag 0x4000000): the points of their mesh's EBone block, 2 or 4 quads. The original passes
	// the first quad of each pair V = D (it builds D + (P1 - P0) / 2 but hands over &D); the second gets D + (P0 - P1) / 2.
	registry.Each<const ecs::components::Animal, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](const ecs::components::Animal& animal, const ecs::components::Transform& transform, const ecs::components::Mesh& mesh) {
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
		    const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
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
	// mode 6, no Z write, cull none
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::DrawScene(const DrawSceneDesc& drawDesc) const noexcept
{
	UpdateLandLight();
	DrawHandShadowPass(drawDesc);
	if (drawDesc.drawIsland && drawDesc.drawEntities)
	{
		_physicsShadows->Update(*drawDesc.camera);
		_physicsShadows->Draw(*_shaderManager);
	}
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
			// fn_007FF4F0 draws the mirrored land with UseSmallBump ([0xC37210]) = 0
			drawPassDesc.smallBumpMapStrength = 0.0f;

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
			drawPassDesc.cullBack = true;

			DrawPass(drawPassDesc);
		}
	}

	// Main Draw Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::MainPass);
		DrawPass(drawDesc);
	}
	DrawHandToolTip(*drawDesc.camera);
	DrawScreenOverlay();
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
	// mode 16: SRCALPHA / INVSRCALPHA, no Z write, ZFUNC ALWAYS (depthTest 0)
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawScreenOverlay() const
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
	if ((colour >> 24) == 0 && bar == 0)
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
		const uint32_t abgr = (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
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
	addBars();
	if ((colour >> 24) != 0)
	{
		// (h) fn_0086FEE0: x 0..W-1, y h'..H-1-h' with h' = h ? h - 1 : 0, then the bars again so the fade never tints them
		const int inset = bar > 0 ? bar - 1 : 0;
		addRect(0, inset, width - 1, height - 1 - inset, colour);
		addBars();
	}
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
	// mode 1 (untextured, SRCALPHA / INVSRCALPHA), ZFUNC ALWAYS, no Z write
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
	bgfx::submit(viewId, toBgfx(_shaderManager->GetShader("DebugLine")->GetRawHandle()));
}

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

	const auto skyType = Locator::skySystem::value().GetCurrentSkyType();

	// Distance haze of this frame (graphics::haze::Frame: fn_007FEAA0 / fn_007FEAD0 and the "Fog" detail key)
	_haze = _landLight && _landLight->IsLoaded() ? haze::Frame() : haze::Params {};
	_hazeUniforms = haze::Uniforms(_haze);
	const glm::vec4 u_haze = _hazeUniforms[0];
	const glm::vec4 u_hazeColour = _hazeUniforms[1];

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSky
		                                                                          : Profiler::Stage::MainPassDrawSky);
		if (desc.drawSky)
		{
			const auto modelMatrix = glm::mat4(1.0f);
			const glm::vec4 u_typeAlignment = {skyType, _skyAlignment.Get() + 1.0f, 0.0f, 0.0f};

			skyShader->SetTextureSampler("s_diffuse", 0, Locator::skySystem::value().GetTexture());
			skyShader->SetUniformValue("u_typeAlignment", &u_typeAlignment);

			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = skyShader;
			submitDesc.state = k_BgfxDefaultStateInvertedZ;
			if (!desc.cullBack)
			{
				submitDesc.state &= ~BGFX_STATE_CULL_MASK;
				submitDesc.state |= BGFX_STATE_CULL_CCW;
			}
			submitDesc.modelMatrices = &modelMatrix;
			submitDesc.matrixCount = 1;
			submitDesc.isSky = true;

			DrawMesh(Locator::skySystem::value().GetMesh(), submitDesc, 0);
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawMoon(desc.viewId, *desc.camera);
				DrawSun(desc.viewId, *desc.camera, false);
			}
			else if (desc.viewId == graphics::RenderPass::Reflection)
			{
				// the moon is reflected (its halo and DrawUnderWater), the sun is not (fn_0086C140 is called once)
				DrawMoon(desc.viewId, *desc.camera, true);
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
			const glm::vec4 u_skyAndBump = {skyType, desc.bumpMapStrength, desc.smallBumpMapStrength, lineDistance};

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
			if (_handShadowFrameBuffer)
			{
				terrainShader->SetTextureSampler("s7_dynamicShadow", 7, _handShadowFrameBuffer->GetColorAttachment());
			}
			const auto dynamicParams = desc.viewId == graphics::RenderPass::Main ? _handShadowParams : glm::vec4(0.0f);
			terrainShader->SetUniformValue("u_dynamicShadowBox", &_handShadowBox);
			terrainShader->SetUniformValue("u_dynamicShadow", &dynamicParams);
			_physicsShadows->BindTerrain(*terrainShader,
			                             desc.viewId == graphics::RenderPass::Main && desc.drawEntities);

			terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
			terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
			terrainShader->SetUniformValue("u_haze", &u_haze);
			terrainShader->SetUniformValue("u_hazeColour", &u_hazeColour);
			// Static shadows darken the block texture by up to x0.5 (x0.75 with 128 px textures, fn_008721A0)
			const float staticShadowStrength =
			    GetDetailLevel(Locator::config::value().detailLevel).useHighTexture ? 0.5f : 0.25f;
			const glm::vec4 u_terrainPass = {desc.viewId == graphics::RenderPass::Reflection ? 0.5f : 1.0f,
			                                 Locator::config::value().terrainTextureDensity, staticShadowStrength,
			                                 Locator::config::value().terrainTriplanar ? 1.0f : 0.0f};
			terrainShader->SetUniformValue("u_terrainPass", &u_terrainPass);
			terrainShader->SetUniformValue("u_islandExtent", &islandExtent);

			// clang-format off
			// fs_terrain writes the land and small bump passes premultiplied (the coast alpha does not fade the small
			// bump, as in the original); Z is written even where the land is transparent (render mode 14)
			constexpr auto defaultState = 0u
				| BGFX_STATE_WRITE_MASK
				| BGFX_STATE_DEPTH_TEST_GREATER
				| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
				| BGFX_STATE_MSAA
			;

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
			const auto state = desc.viewId == graphics::RenderPass::Reflection ? defaultState & ~BGFX_STATE_WRITE_Z : defaultState;

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

				bgfx::setState(state | (desc.cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW), 0);
				bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(terrainShader->GetRawHandle()), 0, discard);
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
		// The original's "underwater" stage (GLandscape::Draw 0x5E490F): the hand mirrored in the sea, unlit grey
		// 0xA0A0A0, only its part above the water (CHand DrawUnderWater, vt+0x118)
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
					L3DMeshSubmitDesc handSubmit = {};
					handSubmit.viewId = desc.viewId;
					handSubmit.program = objectShaderInstanced;
					handSubmit.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
					handSubmit.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer,
					                                                                   handDesc->second.offset, handDesc->second.count);
					handSubmit.modelMatrices = bones->data();
					handSubmit.matrixCount = static_cast<uint8_t>(bones->size());
					handSubmit.unlitColour = 160.0f / 255.0f;
					handSubmit.clipBelowSea = true;
					DrawMesh(*mesh, handSubmit, std::numeric_limits<uint8_t>::max());
				}
			}
			DrawObjectReflections(desc.viewId);
			DrawBoatReflection(desc.viewId);
		}
		if (desc.viewId == graphics::RenderPass::Reflection)
		{
			// GLandscape::Draw 0x5E4B26..: the parts under the water go into the frame before the sea, over the mirrored
			// land. Here that frame is the reflection target, drawn with the mirrored camera, so they are mirrored too.
			// the sharks (and whatever else is cut by the plane), then the fish (4d-4e)
			DrawCutBelowWater(desc.viewId);
			DrawFishShoals(desc.viewId);
			DrawFishPlots(desc.viewId, -1); // fn_00824B90: each shoal of a bait, then its net (fn_00829BC0)
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

			const auto blend = sprite.additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
			                                   : BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
			bgfx::setState(0 | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | blend |
			               BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD));

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
			submitDesc.state = 0u                              //
			                   | BGFX_STATE_WRITE_MASK         //
			                   | BGFX_STATE_DEPTH_TEST_GREATER //
			                   | BGFX_STATE_MSAA               //
			    ;
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();

			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawWaterRings(desc.viewId);
				DrawHumanShadows(desc.viewId);
				DrawFishPlots(desc.viewId, 1); // fn_00824D60 (0x5E6296): the nets' part over the water
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
			// The original draws opaque meshes at once and sends meshes with alpha to the Z-sorter, drawn back to front
			// at the end of the frame (fn_0082F280); here every instance with blended primitives, and every fading
			// one, is drawn on its own in that order in the blended view
			struct SortedInstance
			{
				float distance;
				entt::id_type meshId;
				uint32_t index;
				bool morphWithTerrain;
				bool fading;
				entt::entity sprite {entt::null};
				int effect {-1}; ///< a particle effect (PSysManager::AddDrawing: one Z object per effect)
				int mist {-1}; ///< an index of _frameMists
				int smoke {-1}; ///< an index of _frameSmoke (LH3DSmoke::AddDrawing: one Z object per chimney)
				/// an index of _frameSurfaces: a ZR_SurfRevol atom of an effect, drawn inside that effect's single Z object
				/// (PSysManager::AddDrawing 0x6797D0), so with the effect's key and right after its sprites
				int surface {-1};
				int chain {-1}; ///< an index of _frameChains, likewise inside its effect's Z object (fn_0067B370)
			};
			std::vector<SortedInstance> sorted;
			const bool sortBlended = desc.viewId == graphics::RenderPass::Main;
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto hasBlended = [](const L3DMesh& mesh) {
				for (const auto& subMesh : mesh.GetSubMeshes())
				{
					for (const auto& prim : subMesh->GetPrimitives())
					{
						if (prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha)
						{
							return true;
						}
					}
				}
				return false;
			};

			// the poses of the animated boned meshes (ecs/Animations.h), by instance
			const auto poses = ecs::PosesByInstance(renderCtx.entityInstances);
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
				submitDesc.blendFilter = sortBlended ? 1 : 0;

				// TODO(bwrsandman): choose the correct LOD
				if (mesh->IsBoned() && ecs::HasPose(poses, placers.offset, placers.count))
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
					}
				}
				else
				{
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				}
				if (sortBlended && hasBlended(*mesh))
				{
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						if (cutAbove.contains(placers.offset + i))
						{
							continue;
						}
						const auto origin = glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						sorted.push_back({glm::distance(origin, cameraOrigin), meshId, placers.offset + i, placers.morphWithTerrain, false});
					}
				}
			}
			// Whale::Draw 0x774E10: the sharks' parts above the water, in the normal object list
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawCutAboveWater(desc.viewId);
			}
			if (sortBlended)
			{
				for (const auto& [meshId, placers] : renderCtx.translucentDrawDescs)
				{
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth
						if (renderCtx.instanceUniforms[placers.offset + i][0][3] >= 1.0f)
						{
							continue;
						}
						// the sort point of the one-shot orb (OneOffSpellSeed::Draw 0x518E90), else the matrix's translation
						const auto point = renderCtx.sortPoints.find(placers.offset + i);
						const auto origin = point != renderCtx.sortPoints.end()
						                        ? point->second
						                        : glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						sorted.push_back(
						    {glm::distance(origin, cameraOrigin), meshId, placers.offset + i, placers.morphWithTerrain, true});
					}
				}
			}
			const auto effects = sortBlended ? psys::manager::Collect() : std::vector<psys::manager::Drawable>();
			for (size_t i = 0; i < effects.size(); ++i)
			{
				sorted.push_back({glm::distance(effects[i].origin, cameraOrigin), 0, 0, false, false, entt::null, static_cast<int>(i)});
			}
			if (sortBlended)
			{
				// The atoms of an effect that are not sprites share the Z object of its sprites, with the same key:
				// fn_00679860 -> fn_006798B0 draws a whole collection at once (its atoms through fn_00679920, which calls
				// vt+0xFC DrawAt 0x67CBA0 for a ZR_SurfRevol one, then its chain through fn_0067B370, "draw now"). Pushed
				// after the effects and ordered with a stable sort, so with an equal key they follow their effect's sprites,
				// as the original's stable insertion does (NewZObject 0x83F36A..0x83F376).
				// (aproximado) the original interleaves them atom by atom inside the collection; here an effect's sprites come
				// first, then its surfaces, then its ribbons.
				for (const auto& [distance, index] : CollectPSysSurfaces(*desc.camera))
				{
					sorted.push_back({distance, 0, 0, false, false, entt::null, -1, -1, -1, static_cast<int>(index)});
				}
				for (const auto& [distance, index] : CollectPSysChains(*desc.camera))
				{
					sorted.push_back({distance, 0, 0, false, false, entt::null, -1, -1, -1, -1, static_cast<int>(index)});
				}
			}
			if (spritesSorted)
			{
				Locator::entitiesRegistry::value().Each<const ecs::components::Sprite, const ecs::components::Transform>(
				    [&sorted, &cameraOrigin](entt::entity entity, const auto&, const ecs::components::Transform& transform) {
					    sorted.push_back({glm::distance(transform.position, cameraOrigin), 0, 0, false, false, entity});
				    });
			}
			if (mistsSorted)
			{
				for (const auto& [distance, index] : CollectMists(*desc.camera))
				{
					sorted.push_back({distance, 0, 0, false, false, entt::null, -1, static_cast<int>(index)});
				}
			}
			if (spritesSorted)
			{
				for (const auto& [distance, index] : CollectChimneySmoke(*desc.camera))
				{
					sorted.push_back({distance, 0, 0, false, false, entt::null, -1, -1, static_cast<int>(index)});
				}
			}
			// stable: NewZObject 0x83F310 puts a new entry before the first one with a strictly smaller key
			// (0x83F36A..0x83F376), so entries with an equal key keep the order they arrived in
			std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.distance > b.distance; });

			// OPENBLACK_ORB_TRACE=1: one line a drawn frame per one-shot orb (docs/bw1-notes/openblack-internals.md), to
			// follow the bubble across the 15 -> 0 wrap of its 4 x 4 sheet (OneOffSpellSeed::UpdateFrame 0x72A570)
			static const bool k_OrbTrace = std::getenv("OPENBLACK_ORB_TRACE") != nullptr;
			if (k_OrbTrace && sortBlended && Locator::entitiesRegistry::has_value())
			{
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				// the ZR_SurfRevol discs in the same list, to see that each one is drawn before the bubble of its dispenser
				for (size_t k = 0; k < sorted.size(); ++k)
				{
					if (sorted[k].surface >= 0)
					{
						const auto& surface = _frameSurfaces[static_cast<size_t>(sorted[k].surface)];
						SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
						                   "Orb trace: surface {} ({}) sorted {}/{} key {:.2f} origin ({:.1f}, {:.1f}, {:.1f})",
						                   sorted[k].surface, surface.texture, static_cast<int>(k),
						                   static_cast<int>(sorted.size()), sorted[k].distance, surface.origin.x, surface.origin.y,
						                   surface.origin.z);
					}
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
					    for (size_t k = 0; k < sorted.size(); ++k)
					    {
						    if (sorted[k].fading && sorted[k].index == index && sorted[k].meshId == mesh.id)
						    {
							    at = static_cast<int>(k);
							    key = sorted[k].distance;
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
					        static_cast<int>(sorted.size()), key, orb.sortPoint.x, orb.sortPoint.y, orb.sortPoint.z, inView);
				    });
			}

			// Back to front: blended primitives and fading meshes (components::Alpha), in their own view right after the
			// main pass (same target and camera, no clear), so that nothing drawn in the main pass is sorted over them
			if (!sorted.empty())
			{
				const auto opaqueState = submitDesc.state;
				submitDesc.viewId = graphics::RenderPass::MainBlended;
				auto& spriteRegistry = Locator::entitiesRegistry::value();
				for (const auto& instance : sorted)
				{
					if (instance.effect >= 0)
					{
						DrawPSysEffect(effects[static_cast<size_t>(instance.effect)], *desc.camera, graphics::RenderPass::MainBlended);
						continue;
					}
					if (instance.mist >= 0)
					{
						DrawMist(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(instance.mist));
						continue;
					}
					if (instance.smoke >= 0)
					{
						DrawChimneySmoke(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(instance.smoke));
						continue;
					}
					if (instance.surface >= 0)
					{
						DrawPSysSurface(graphics::RenderPass::MainBlended, static_cast<uint32_t>(instance.surface));
						continue;
					}
					if (instance.chain >= 0)
					{
						DrawPSysChain(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(instance.chain));
						continue;
					}
					if (instance.sprite != entt::null)
					{
						const auto& [sprite, transform] =
						    spriteRegistry.Get<const ecs::components::Sprite, const ecs::components::Transform>(instance.sprite);
						drawSprite(sprite, transform, graphics::RenderPass::MainBlended);
						continue;
					}
					auto mesh = meshManager.Handle(instance.meshId);
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
					submitDesc.blendFilter = instance.fading ? 0 : 2;
					submitDesc.state = instance.fading ? (0u | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z |
					                                      BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA |
					                                      BGFX_STATE_MSAA)
					                                   : opaqueState;
					// a PSys mesh atom with UseAdditiveAlpha (Creators/Mesh.h): mode 13, SRCALPHA / ONE without Z write
					if (instance.fading && renderCtx.additiveInstances.contains(instance.index))
					{
						submitDesc.state = 0u | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER |
						                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) | BGFX_STATE_MSAA;
					}
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				}
				submitDesc.state = opaqueState;
				submitDesc.viewId = desc.viewId;
				submitDesc.blendFilter = 0;
			}
			if (desc.viewId == graphics::RenderPass::Main)
			{
				// The effects' ribbons (fn_0067B3F0) and surfaces (0x67CBA0) are no longer a group of their own: they go
				// through the back-to-front list above, inside their effect's Z object, as the original draws them.
				// The rain keeps its group: LH3DAtmos::Render3D queues one Z object per raining tile of its own
				// (fn_008341B0 0x83427F, key = dist2 to (x, GetAltitude(x, z), z), the tile and the alpha packed in K), not
				// through PSysManager::AddDrawing, so giving it an effect's key would be wrong (hole H3).
				// (inferido: draw order) the rain as a group after the sorted sprites; its order against them is not read.
				// LH3DAtmos::Render3D 0x836250: the rain streaks (RendererRain.cpp)
				DrawRain(graphics::RenderPass::MainBlended, *desc.camera);
				DrawHandShadowOnObjects();
				// the boat's LH3DSprites (wake, dust, spray), after the blended models (the original Z-sorts them)
				DrawBoatSprites(graphics::RenderPass::MainBlended, *desc.camera);
			}

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
		DrawClouds(graphics::RenderPass::MainBlended, *desc.camera);
		if (!mistsSorted)
		{
			DrawMists(graphics::RenderPass::MainBlended, *desc.camera);
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
