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

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <SDL_video.h>
#include <bgfx/platform.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/Clouds.h"
#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "3D/OceanInterface.h"
#include "3D/Rain.h"
#include "3D/SkyDome.h"
#include "3D/SkyFrame.h"
#include "3D/SkyType.h"
#include "3D/SkyWeather.h"
#include "3D/SnowCover.h"
#include "3D/Snowfall.h"
#include "Audio/Services/SpookyVoices.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Common/CrashHandler.h"
#include "Common/CrashReport.h"
#include "Common/TruncateToInt.h"
#include "Debug/TraceLog.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/DrawList/FrameInputs.h"
#include "ECS/DrawList/TextureDetail.h"
#include "ECS/FloatingNumber.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SunDraw.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/BoneBudget.h"
#include "Graphics/BonePalette.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GameFont.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/InputPromptFrame.h"
#include "Graphics/InstanceRuns.h"
#include "Graphics/KeyOrMouseLayout.h"
#include "Graphics/MeshStatus.h"
#include "Graphics/ModelLight.h"
#include "Graphics/Moon.h"
#include "Graphics/ObjectListFrame.h"
#include "Graphics/ObjectOrder.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/PartialBuildCap.h"
#include "Graphics/Primitive.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ResetFlags.h"
#include "Graphics/ScreenshotCapture.h"
#include "Graphics/SeaPass.h"
#include "Graphics/SeaReflection.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowList.h"
#include "Graphics/SharedPrimitiveState.h"
#include "Graphics/SpriteTextures.h"
#include "Graphics/Sun.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Graphics/TribalPower.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h" // the exploded pieces
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"
#include "Help/ClickCue.h"
#include "Help/HelpTextDisplay.h"
#include "Locator.h"
#include "Magic/TribalPowerSpin.h"
#include "Particles/Creators/Mist.h"
#include "Particles/ParticleManager.h"
#include "Particles/Rules/ExplodeObject.h" // the exploded pieces
#include "Physics/DamageMesh.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"
#include "Temple/HeartShell.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/CitadelLanterns.h"

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

		// This can come from bgfx's render thread, where nothing would catch the exception: it is reported here
		if (crash_handler::IsInstalled())
		{
			crash_handler::ReportFatal(crash_report::CrashKind::GraphicsFatal, str, filePath, line,
			                           std::string("bgfx ") + codeStr);
		}

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
			// bgfx only says so in its notes when it had no screen to read for a picture, which is then never written
			if (const auto uncaptured = graphics::UncapturedScreenshotPath(out); uncaptured.has_value())
			{
				failures.Record(std::string(*uncaptured),
				                "the renderer had no screen to read (a Vulkan window has none while it is minimised)");
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
	void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch, const void* data, uint32_t size,
	                bool yflip) override
	{
		if (graphics::ScreenPictures::InMemory(filePath))
		{
			// Make the picture now from the pixels of bgfx, and do not copy the full frame
			pictures.Make(filePath, {.width = width,
			                         .height = height,
			                         .pitch = pitch,
			                         .yflip = yflip,
			                         .bgra = std::span(static_cast<const uint8_t*>(data), size)});
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Taking a screenshot...");

		const auto path = std::filesystem::path(filePath);
		if (path.extension() != ".png")
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: {} screenshot ({}x{}) requested at {}",
			                   path.extension().string(), width, height, filePath);
			failures.Record(filePath, "only .png pictures can be written");
			return;
		}
		const auto pixels = std::span(static_cast<const uint8_t*>(data), size);
		if (const auto why = graphics::WriteScreenshotPng(path, width, height, pitch, pixels, yflip); !why.empty())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save Screenshot ({}x{}) at {}: {}", width, height, filePath,
			                    why);
			failures.Record(filePath, "couldn't be written: " + why);
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot ({}x{}) saved at {}", width, height, filePath);
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

	/// The pictures bgfx gave up or couldn't write, told from its render thread
	graphics::ScreenshotFailures failures;
	/// The pictures that the game asked for in memory
	graphics::ScreenPictures pictures;
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

	const uint32_t bgfxReset = graphics::ResetFlagsWithVsync(BGFX_RESET_NONE, vsync);
	auto bgfxCallback = std::make_unique<BgfxCallback>();
	init.resolution.reset = bgfxReset;
	init.callback = dynamic_cast<bgfx::CallbackI*>(bgfxCallback.get());
	// (openblack guard) the exploded pieces' triangles go up in the
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

	const glm::u16vec2 backBufferSize(init.resolution.width, init.resolution.height);
	return std::make_unique<Renderer>(bgfxReset, backBufferSize, std::move(bgfxCallback));
}

Renderer::Renderer(uint32_t bgfxReset, glm::u16vec2 backBufferSize, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
    , _shadows(std::make_unique<shadow_list::List>())
    , _bgfxCallback(std::move(bgfxCallback))
    , _bgfxReset(bgfxReset)
    , _backBufferSize(backBufferSize)
    , _offscreenShots(std::make_unique<OffscreenScreenshots>())
{
	_shaderManager->LoadShaders();
	_plane = Primitive::CreatePlane();

	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::Main), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::MainBlended), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrame3D), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameIcons), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameQueue), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay), bgfx::ViewMode::Sequential);
	// what is under the sea is painted in order too: the sky, the moon's
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
	_offscreenShots.reset();
	for (auto& font : _fonts)
	{
		font.reset(); // its texture before bgfx::shutdown
	}
	_shadows.reset(); // its textures before bgfx::shutdown
	// the handles before bgfx::shutdown too: the members themselves only go after it
	_landLightTexture.Reset();
	_landCellsTexture.Reset();
	_videoTexture.Reset();
	_videoAlphaTexture.Reset();
	_fishPlotInstances.Reset();
	_spiritInstances.Reset();
	_templeMapFrameBuffer.reset();
	// made on the first draw that needs it, such as the temple's: its texture before bgfx::shutdown as well
	_revolvedSurfaceWhite.reset();
	// the glowing shells' buffers too
	_glowShells.clear();
	_snowDepth.reset();
	_bonePalette.reset();
	// the creatures' painted skins and the morph streams' layouts, before bgfx::shutdown too
	_creatureSkins.Clear();
	_creatureStatic.clear();
	for (auto& layout : _morphStreamLayouts)
	{
		layout.Reset();
	}
	_plane.reset();
	// the glare's questions to the drawing too
	for (auto& query : _glareQueries)
	{
		if (bgfx::isValid(query))
		{
			bgfx::destroy(query);
			query = BGFX_INVALID_HANDLE;
		}
	}
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
		// the end of the frame: the Z reset quad (pre-transformed, z = 1 over the whole screen, ZFUNC ALWAYS)
		// before the "after" callbacks: their 3D view starts with the depth at the far value of the main clear
		const auto finishFrame3D = static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrame3D);
		bgfx::setViewClear(finishFrame3D, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		bgfx::setViewRect(finishFrame3D, 0, 0, resolution.x, resolution.y);
		const auto overlay = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
		bgfx::setViewClear(overlay, BGFX_CLEAR_NONE);
		bgfx::setViewRect(overlay, 0, 0, resolution.x, resolution.y);
		// the input prompt icons' pass 0, set up as ScreenOverlay (no clear, the whole screen; identity transforms at submit)
		const auto icons = static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameIcons);
		bgfx::setViewClear(icons, BGFX_CLEAR_NONE);
		bgfx::setViewRect(icons, 0, 0, resolution.x, resolution.y);
		// the falling film's end of frame queue: no clear, over the depth the main views left
		const auto queue = static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameQueue);
		bgfx::setViewClear(queue, BGFX_CLEAR_NONE);
		bgfx::setViewRect(queue, 0, 0, resolution.x, resolution.y);
		_resolution = resolution;
	}
}

void Renderer::Reset(glm::u16vec2 resolution) const noexcept
{
	_backBufferSize = resolution;
	bgfx::reset(resolution.x, resolution.y, _bgfxReset);
}

void Renderer::SetVsync(bool vsync) noexcept
{
	_bgfxReset = graphics::ResetFlagsWithVsync(_bgfxReset, vsync);
	// the back buffer keeps the size it was last given, even while the window is minimised: only the wait for the
	// screen's refresh changes
	bgfx::reset(_backBufferSize.x, _backBufferSize.y, _bgfxReset);
}

void Renderer::SetMultisampling(bool multisampling) noexcept
{
	_bgfxReset = graphics::ResetFlagsWithMultisampling(_bgfxReset, multisampling);
	bgfx::reset(_backBufferSize.x, _backBufferSize.y, _bgfxReset);
}

void Renderer::ResetForNewGame() noexcept
{
	// the main view's size, until the next game configures its views
	_resolution = {0, 0};
	// the sea's scroll and frame count
	_seaDrift = {};
	_seaFrame = 0;
	// the smoothed sun glare, sky alignment and cloud colour, and the clouds' alpha and shadow image
	_sunGlare = 0.0f;
	_skyAlignment = {};
	_cloudMilliseconds = 0.0f;
	_cloudRgb = glm::vec3(1.0f);
	_cloudAlpha.clear();
	_cloudShadowImage.clear();
	// the land light's table, palette and texture, made again on the first frame as a new renderer makes them
	_landLight.reset();
	_landLightPalette.reset();
	_landLightTexture.Reset();
	_landLightBuilt.reset();
	_landCellsTexture.Reset();
	_landCellsSize = {0, 0};
	// the painted skins are keyed by entity and revision, which a new game gives out again from the start
	_creatureSkins.Clear();
	_creatureStatic.clear();
	// the projected shadows and the objects under them
	_shadowReceivers.clear();
	_shadows = std::make_unique<shadow_list::List>();
	// the snow's depths, uploaded again from the next game's snow
	_snowDepth.reset();
	_snowRevision.reset();
	// the influence border's scroll
	_influenceScrollMs = 0;
	_preInfluenceScroll.reset();
	// the input prompts' place, time and side
	_keyOrMouseHand.reset();
	_keyOrMouseView = graphics::RenderPass::ScreenOverlay;
	_keyOrMouseTicks = 0;
	_keyOrMouseSide = 0;
	// the temple's map, drawn again on the next visit
	_templeMapFrameBuffer.reset();
	_templeMapVisit.reset();
	_templeMapFresh = false;
	// the film's last frame, uploaded again
	_videoSerial.reset();
	// the instances made on first use
	_fishPlotInstances.Reset();
	_fishPlotCapacity = 0;
	_spiritInstances.Reset();
}

graphics::ShaderManager& Renderer::GetShaderManager() const noexcept
{
	return *_shaderManager;
}

const Texture2D* Renderer::SnowDepthTexture() const
{
	if (!Locator::snowSystem::has_value())
	{
		return nullptr;
	}
	const auto& snow = Locator::snowSystem::value();
	if (!_snowDepth)
	{
		_snowDepth = std::make_unique<Texture2D>("SnowDepth");
		_snowDepth->Create(snow_cover::k_GridSize, snow_cover::k_GridSize, 1, TextureFormat::R32F, Wrapping::ClampEdge,
		                   Filter::Nearest, nullptr);
		_snowRevision.reset();
	}
	if (_snowRevision != snow.GetRevision())
	{
		const auto depths = snow.GetDepths();
		_snowDepth->Update(depths.data(), static_cast<uint32_t>(depths.size_bytes()));
		_snowRevision = snow.GetRevision();
	}
	return _snowDepth.get();
}

Renderer::MeshDrawValues Renderer::GetMeshDrawValues() const
{
	const auto& island = Locator::terrainSystem::value();
	const auto extent = island.GetExtent();
	const auto& cellMap = island.GetCellMap();
	const auto cellMapSize = glm::vec2(cellMap.GetResolution());
	const auto& textures = Locator::resources::value().GetTextures();
	const auto texture = [&textures](entt::id_type id) -> const Texture2D* {
		return textures.Contains(id) ? &*textures.Handle(id) : nullptr;
	};
	const auto* snowDepth = SnowDepthTexture();
	return {
	    .heightMap = island.GetHeightMap(),
	    .cellMap = cellMap,
	    .islandExtent = glm::vec4(extent.minimum, extent.maximum),
	    .cellMapInfo = {extent.minimum, cellMapSize},
	    .modelLight = model_light::Uniform(),
	    .lit = IsLandLit(),
	    .frameCells = _landCellsTexture.IsValid() && glm::vec2(_landCellsSize) == cellMapSize,
	    .snowDepth = snowDepth,
	    .snowTexture = texture(snow_cover::k_TextureId.value()),
	    .snowAlpha = texture(snow_cover::k_AlphaTextureId.value()),
	    .snow = snowDepth != nullptr && Locator::snowSystem::value().HasSnow(),
	};
}

const mesh_uniforms::Handles& Renderer::MeshUniformsOf(const ShaderProgram& program) const
{
	const auto [found, inserted] = _meshUniforms.try_emplace(&program);
	if (inserted)
	{
		found->second = mesh_uniforms::Resolve([&program](std::string_view name) { return program.FindUniform(name); });
	}
	return found->second;
}

void Renderer::DrawSubMesh(const graphics::L3DMesh& mesh, const graphics::L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
                           bool preserveState, std::optional<MeshDrawValues>& values,
                           std::optional<uint32_t>& transformCache) const
{
	assert(&subMesh.GetMesh());
	// each sub-mesh by its own bones flag: a rigid one uses the variant of the program with a single model matrix, a
	// skinned one of a mesh with up to bone_budget::k_Few bones the one with that many (see vs_object.sc). A mesh
	// without the bones flag is given no bone matrices, so its sub-meshes stay rigid.
	const bool skinned = subMesh.GetFlags().hasBones && mesh.IsBoned();
	const auto budget = bone_budget::ForSubMesh(skinned, mesh.GetBoneMatrices().size());
	// a rigid sub-mesh of a boned mesh is drawn with the object's matrix alone, which its instance carries, not with the
	// bone matrices its mesh's draw was given
	const bool rigidInBonedMesh = !skinned && mesh.IsBoned();
	// the posed instances of a mesh drawn together: a skinned sub-mesh reads every instance's bones from the frame's bone
	// palette, with the bone index clamped as the budget's own program clamps it (vs_object.sc, USE_BONE_PALETTE)
	const bool fromPalette = desc.bonePalette != nullptr && skinned;
	const auto* program = fromPalette                             ? PaletteVariant()
	                      : budget == bone_budget::Budget::Single ? StaticVariant(desc.program)
	                      : budget == bone_budget::Budget::Few    ? FewBonesVariant(desc.program)
	                                                              : desc.program;
	// We don't draw physics meshes, and modern GPUs can handle high lod. The common draw takes the status 0 submeshes; the
	// parts an object's status adds (a graveyard's graves) are drawn over it with statusParts (graphics::mesh_status).
	// Window submeshes have no LOD bits: the original draws them only at night, without the LOD test (the abode's
	// draw); vs_object hides them on the instances whose windows are not lit.
	const bool window = subMesh.GetFlags().isWindow && desc.instanceDesc != nullptr;
	if (!desc.drawAll && (subMesh.IsPhysics() || !graphics::mesh_status::Drawn(subMesh.GetFlags().status, desc.statusParts) ||
	                      ((subMesh.GetFlags().lodMask & 1) != 1 && !window)))
	{
		return;
	}
	// A damage mesh's triangles: the main view gets them lit and drawn on the CPU (damage::Mesh::AppendDraw, in DrawPass); its
	// sub-meshes stay for the other views. Also out of the projected shadows over the object (DrawShadowsOnObject, the
	// same view): the fragments are not drawn as an object, so the objects' shadow loop never runs over them (inferred:
	// that damage mesh receives none)
	if (subMesh.IsCpuDrawn() && (desc.viewId == RenderPass::Main || desc.viewId == RenderPass::MainBlended))
	{
		return;
	}

	if (!values.has_value())
	{
		values.emplace(GetMeshDrawValues());
	}
	const auto& drawValues = *values;

	// The program's uniforms by handle, looked up once per program when the draw first sets one. One the program doesn't
	// have is set by name, which warns of it once.
	using mesh_uniforms::Uniform;
	const mesh_uniforms::Handles* uniforms = nullptr;
	const auto handleOf = [this, &uniforms, program](Uniform uniform) -> const std::optional<UniformHandle>& {
		if (uniforms == nullptr)
		{
			uniforms = &MeshUniformsOf(*program);
		}
		return uniforms->at(static_cast<size_t>(uniform));
	};
	const auto setUniform = [&handleOf, program](Uniform uniform, const void* value) {
		if (const auto& handle = handleOf(uniform))
		{
			program->SetUniformValue(*handle, value);
		}
		else
		{
			program->SetUniformValue(mesh_uniforms::NameOf(uniform).data(), value);
		}
	};
	// flags: none, or the sampler flags of a Texture2D
	const auto setSampler = [&handleOf, program](Uniform sampler, uint8_t stage, const auto& texture, auto... flags) {
		if (const auto& handle = handleOf(sampler))
		{
			program->SetTextureSampler(*handle, stage, texture, flags...);
		}
		else
		{
			program->SetTextureSampler(mesh_uniforms::NameOf(sampler).data(), stage, texture, flags...);
		}
	};

	bool lastPreserveState = false;
	// the pass of the opaque models: the normal table and every primitive in its own mode
	const bool modelPass = desc.table == render_modes::Table::Normal && !desc.mode.has_value();
	const auto& primitives = subMesh.GetPrimitives();
	for (auto it = primitives.begin(); it != primitives.end(); ++it)
	{
		const auto& prim = *it;

		const bool hasNext = std::next(it) != primitives.end();

		// a creature's painted skins in place of its mesh's (RendererCreature.cpp); the mesh's own for everything else
		const Texture2D* texture =
		    desc.textureOverride != nullptr ? desc.textureOverride : SkinTexture(mesh, prim.skinID, desc.paintedSkins);
		const Texture2D* nextTexture = !hasNext ? nullptr
		                               : desc.textureOverride != nullptr
		                                   ? desc.textureOverride
		                                   : SkinTexture(mesh, std::next(it)->skinID, desc.paintedSkins);

		// Material blending of the original (L3D material type): AlphaTextured & co. blend with the texture alpha, e.g.
		// the fading wrist of the hand and the soft edges of buildings. Chroma materials stay alpha tested.
		const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
		// Setting the material: the mode of the primitive's material through the current table, or the mode
		// every primitive is drawn in
		const auto drawn =
		    render_modes::Select(desc.mode.value_or(static_cast<render_modes::Mode>(prim.materialType)), desc.table);
		// the next primitive keeps this one's state only if it would set the same (its culling included)
		const bool primitivePreserveState = texture != nullptr && texture == nextTexture &&
		                                    shared_primitive_state::SharesState(prim, *std::next(it)) &&
		                                    (preserveState || hasNext);
		// the main pass draws the opaque primitives; the blended ones go through the back-to-front list (Z-sorter)
		if ((desc.blendFilter == 1 && blended) || (desc.blendFilter == 2 && !blended))
		{
			continue;
		}

		uint32_t skip = Mesh::k_SkipNone;
		if (!lastPreserveState)
		{
			if (rigidInBonedMesh)
			{
				static const glm::mat4 k_ObjectOnly(1.0f);
				bgfx::setTransform(&k_ObjectOnly);
			}
			else if (desc.modelMatrices != nullptr && desc.matrixCount > 0)
			{
				// the matrices are copied into the frame's matrix cache once per mesh draw; every later primitive
				// reuses that copy, which holds the same matrices
				if (transformCache.has_value())
				{
					bgfx::setTransform(*transformCache, desc.matrixCount);
				}
				else
				{
					transformCache = bgfx::setTransform(desc.modelMatrices, desc.matrixCount);
				}
			}
			if (texture != nullptr)
			{
				// Materials without the tiling bit are clamped (the original's tiling flag is only set for particle
				// meshes)
				const uint32_t samplerFlags =
				    prim.wrap ? UINT32_MAX
				              : (texture->GetSamplerFlags() & ~(BGFX_SAMPLER_U_MASK | BGFX_SAMPLER_V_MASK)) |
				                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
				setSampler(Uniform::Diffuse, 0, *texture, samplerFlags);
			}
			if (desc.morphWithTerrain)
			{
				setSampler(Uniform::Heightmap, 1, drawValues.heightMap);     // vs
				setUniform(Uniform::IslandExtent, &drawValues.islandExtent); // vs
			}
			if (desc.isSky)
			{
				// vs_object reads u_objectClip in both its branches (sea_plane.sh) and bgfx keeps the last value of a
				// uniform: the sky is never a sea draw, so it must not inherit the last net's or shark's unmirror
				setUniform(Uniform::ObjectClip, &sea_pass::k_NoClip); // vs
			}
			if (!desc.isSky)
			{
				const bool lit = drawValues.lit;
				// x, by desc.sea.light (sea_pass::SeaLight): 0 white, 1 lit like the original (Normal), 2 B's constant
				// colour (Constant: z = the object colour's r 65536 + g 256 + b), 3 the land colour only (LastDraw), 4 C,
				// DrawCutByPlane (Cut: y = the colour's alpha, z = its rgb, w = the object specular's rgb)
				// y: the colour boost, w: the draw's land light (land_light::DrawLightField), modes 1 and 3
				constexpr uint32_t k_Rgb = 0x00FFFFFFu;
				glm::vec4 u_objectLight = {
				    lit ? 1.0f : 0.0f, desc.lightBoost, -1.0f,
				    land_light::DrawLightField(desc.noHaze,
				                               {static_cast<land_light::ObjectMode>(desc.landLightMode), desc.landLightHaze})};
				switch (desc.sea.light)
				{
				case sea_pass::SeaLight::Normal:
					break;
				case sea_pass::SeaLight::Constant:
					// the object's colour and specular, both read per vertex. The colour's alpha (the hand's 0x65) only
					// reaches what the stage's alpha takes from the diffuse: no alternate mode table here (the
					// under-water draw only uses it for objects with the global alpha flag; (inferred) the hand's object
					// never gets it, see Renderer::DrawUnderWater) and the hand's AlphaTextured takes the texture's alpha, so
					// it is left out
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
				setUniform(Uniform::ObjectClip, &u_objectClip);                            // vs, fs
				setSampler(Uniform::LandLightTable, 3, fromBgfx(_landLightTexture.Get())); // vs
				// this frame's cells (land_light), or the loaded ones (the same layout) before the first frame's
				if (drawValues.frameCells)
				{
					setSampler(Uniform::LandCells, 4, fromBgfx(_landCellsTexture.Get())); // vs
				}
				else
				{
					setSampler(Uniform::LandCells, 4, drawValues.cellMap); // vs
				}
				setUniform(Uniform::CellMap, &drawValues.cellMapInfo); // vs
				setUniform(Uniform::ObjectLight, &u_objectLight);      // vs
				setUniform(Uniform::Haze, &_hazeUniforms[0]);          // vs
				setUniform(Uniform::HazeColour, &_hazeUniforms[1]);    // vs
				// xyz: the engine's one light, w: the ambient (model_light.sh)
				setUniform(Uniform::ModelLight, &drawValues.modelLight); // vs
				// x: a window sub-mesh, y: 1 for the cap of a partly built building (L3DSubMesh::Primitive::partialBuildCap),
				// w: 1 if the primitive takes the object's texture offset (L3DSubMesh::Primitive::uvOffset)
				const glm::vec4 u_window = {subMesh.GetFlags().isWindow ? 1.0f : 0.0f, prim.partialBuildCap ? 1.0f : 0.0f, 0.0f,
				                            prim.uvOffset ? 1.0f : 0.0f};
				setUniform(Uniform::Window, &u_window); // vs
				const glm::vec4 u_materialColour = {glm::vec3(prim.colour), texture == nullptr ? 1.0f : 0.0f};
				setUniform(Uniform::MaterialColour, &u_materialColour); // fs
				// x: 1 where snow lies and shows on this primitive, as the game draws it over the object where its depth is
				// written (snow.sh); its textures are bound whenever they exist, so that nothing is read unbound
				if (drawValues.snowDepth != nullptr)
				{
					setSampler(Uniform::SnowDepth, 11, *drawValues.snowDepth); // vs
				}
				const bool snowTextured = drawValues.snowTexture != nullptr && drawValues.snowAlpha != nullptr;
				if (snowTextured)
				{
					setSampler(Uniform::SnowTexture, 12, *drawValues.snowTexture);
					setSampler(Uniform::SnowAlpha, 13, *drawValues.snowAlpha);
				}
				const bool takesSnow = partial_build_cap::TakesSnow(desc.snow, prim.depthWrite, prim.partialBuild) &&
				                       drawValues.snow && snowTextured;
				const glm::vec4 u_snow {takesSnow ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
				setUniform(Uniform::Snow, &u_snow); // vs
				// The advisors' pupils, on the program that moves them; its smaller bone budgets' variants have none
				if (desc.advisorPupils.has_value())
				{
					for (size_t i = 0; i < desc.advisorPupils->size(); ++i)
					{
						const auto uniform = static_cast<Uniform>(static_cast<size_t>(Uniform::AdvisorPupil0) + i);
						if (const auto& handle = handleOf(uniform))
						{
							program->SetUniformValue(*handle, &desc.advisorPupils->at(i)); // vs
						}
					}
				}
				if (desc.dynamicShadow.has_value())
				{
					setSampler(Uniform::DynamicShadow, 5, *desc.dynamicShadow);
					setUniform(Uniform::DynamicShadowBox, &desc.dynamicShadowBox);
					setUniform(Uniform::DynamicShadowCull, &desc.dynamicShadowCull);
				}
			}
			if (!desc.isSky)
			{
				// y: the drawn mode's ALPHAREF / 255 (-1 without alpha test), w: its stage 0 alpha
				const auto alpha = render_modes::PrimitiveAlpha(
				    drawn, desc.table, static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)), desc.globalAlpha,
				    desc.forcedAlphaRef);
				const glm::vec4 u_skyAlphaThreshold = {
				    0.0f, // x: unused (fs_object reads only y, z, w)
				    alpha.ref,
				    0.0f, // z: unused
				    static_cast<float>(alpha.source),
				};
				setUniform(Uniform::SkyAlphaThreshold, &u_skyAlphaThreshold);
			}
			if (fromPalette)
			{
				const glm::vec4 u_bonePalette {static_cast<float>(bone_budget::Bones(budget)), 0.0f, 0.0f, 0.0f};
				setUniform(Uniform::BonePaletteBones, &u_bonePalette);  // vs
				setSampler(Uniform::BonePalette, 2, *desc.bonePalette); // vs
			}
			if (desc.morphTargets != nullptr)
			{
				// how far a creature's body is pulled towards each of its meshes (vs_object_morph_instanced)
				const glm::vec4 u_morphWeights {desc.morphTargets->weights, 0.0f};
				setUniform(Uniform::MorphWeights, &u_morphWeights); // vs
				BindBlendSources(mesh, subMesh, *desc.morphTargets, *program);
			}
			if (desc.fizz.has_value())
			{
				// A creature fizzing out of sight (creature_draw::FizzPasses). Its depth: x, y how far the static has slid,
				// z 1, w the alpha the static must reach; its body: z 2, w its alpha
				setUniform(Uniform::CreatureSpellLook, &desc.fizz->spellLook); // fs
				if (desc.fizz->staticAlpha != nullptr)
				{
					setSampler(Uniform::StaticAlpha, 15, *desc.fizz->staticAlpha); // fs, wrapped as the texture is
				}
			}
		}
		else
		{
			skip |= Mesh::k_SkipRenderState;
			skip |= Mesh::k_SkipVertexBuffer;
		}

		{
			if (desc.transientInstance != nullptr && (skip & Mesh::k_SkipInstanceBuffer) == 0)
			{
				bgfx::setInstanceDataBuffer(desc.transientInstance);
			}
			else if (desc.instanceDesc != nullptr && (skip & Mesh::k_SkipInstanceBuffer) == 0)
			{
				bgfx::setInstanceDataBuffer(toBgfx(desc.instanceDesc->GetRawHandle()), desc.instanceDesc->GetStart(),
				                            desc.instanceDesc->GetCount());
			}
			// (openblack guard) a mesh whose buffers bgfx could not create (out of handles) is not drawn
			if (!subMesh.GetMesh().GetVertexBuffer().IsValid() ||
			    (subMesh.GetMesh().IsIndexed() && !subMesh.GetMesh().GetIndexBuffer().IsValid()))
			{
				_shaderManager->DiscardBindings();
				bgfx::discard(BGFX_DISCARD_ALL);
				lastPreserveState = false;
				continue;
			}
			if (subMesh.GetMesh().IsIndexed() && (skip & Mesh::k_SkipIndexBuffer) == 0)
			{
				subMesh.GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			if ((skip & Mesh::k_SkipVertexBuffer) == 0)
			{
				subMesh.GetMesh().GetVertexBuffer().Bind();
				if (desc.morphTargets != nullptr)
				{
					BindMorphTargets(mesh, subMesh, *desc.morphTargets);
				}
			}
			auto viewId = desc.viewId;
			// Setting the material: the culling from the material's two-sided bit (D3DCULL_CCW; the mirrored reflection camera
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
			const auto state = render_modes::PrimitiveState(drawn, options, blended);
			if ((skip & Mesh::k_SkipRenderState) == 0)
			{
				bgfx::setState(state, desc.rgba);
			}

			program->Submit(static_cast<bgfx::ViewId>(viewId), 0,
			                primitivePreserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
		}
		lastPreserveState = primitivePreserveState;
	}
}

namespace
{
/// Whether every primitive of the mesh, drawn as DrawSubMesh draws it with this description, leaves the same pixels in
/// whatever order its instances are drawn (bone_palette::DrawsInAnyOrder)
bool DrawnInAnyOrder(const L3DMesh& mesh, const RendererInterface::L3DMeshSubmitDesc& desc)
{
	return std::ranges::all_of(mesh.GetSubMeshes(), [&desc](const auto& subMesh) {
		return std::ranges::all_of(subMesh->GetPrimitives(), [&desc](const L3DSubMesh::Primitive& prim) {
			const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
			const auto drawn =
			    render_modes::Select(desc.mode.value_or(static_cast<render_modes::Mode>(prim.materialType)), desc.table);
			return bone_palette::DrawsInAnyOrder(render_modes::PrimitiveState(drawn, desc.options, blended));
		});
	});
}

/// The land light mode and haze of a mesh's models (RenderContext::meshLandLight, land_light::ObjectLight), for its
/// rows that carry no light of their own
void ApplyLandLightMode(const RenderContext& context, entt::id_type meshId, RendererInterface::L3DMeshSubmitDesc& desc)
{
	const auto mode = context.meshLandLight.find(meshId);
	desc.landLightMode = 0;
	desc.landLightHaze = true;
	if (mode != context.meshLandLight.end())
	{
		desc.landLightMode = static_cast<uint8_t>(mode->second.mode);
		desc.landLightHaze = mode->second.haze;
	}
}
} // namespace

namespace
{
/// The on-screen region tests, shared (Graphics/RegionOnScreen.h)
using openblack::graphics::region_on_screen::BoxInView;
using openblack::graphics::region_on_screen::SphereInView;

/// OPENBLACK_ZSORTER_TRACE's frame count, in the debug hooks' store
/// (Locator::debugHooks)
struct RendererDebugHooksState
{
	uint32_t zsorterTraceFrame {0};
};

RendererDebugHooksState& RendererDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("renderer: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<RendererDebugHooksState>();
}

/// An object program and its variants for smaller bone budgets (bone_budget::Budget)
struct BoneBudgetVariants
{
	const char* program;
	const char* single;
	const char* few;
};
constexpr std::array k_BoneBudgetVariants {
    BoneBudgetVariants {"ObjectInstanced", "ObjectStaticInstanced", "ObjectFewBonesInstanced"},
    BoneBudgetVariants {"ObjectHeightMapInstanced", "ObjectHeightMapStaticInstanced", "ObjectHeightMapFewBonesInstanced"},
    BoneBudgetVariants {"ObjectShadowInstanced", "ObjectShadowInstancedStatic", "ObjectShadowInstancedB32"},
    BoneBudgetVariants {"ObjectHeightMapShadowInstanced", "ObjectHeightMapShadowInstancedStatic",
                        "ObjectHeightMapShadowInstancedB32"},
    BoneBudgetVariants {"ObjectInstancedSpellLook", "ObjectStaticInstancedSpellLook", "ObjectFewBonesInstancedSpellLook"},
    // The advisors' bodies: their rigid and few-bone parts draw as any object's, without the pupils
    BoneBudgetVariants {"ObjectAdvisorInstanced", "ObjectStaticInstanced", "ObjectFewBonesInstanced"},
};
} // namespace

const graphics::ShaderProgram* Renderer::FewBonesVariant(const graphics::ShaderProgram* program) const
{
	if (_fewBonesVariants.empty())
	{
		for (const auto& variants : k_BoneBudgetVariants)
		{
			_fewBonesVariants.emplace(_shaderManager->GetShader(variants.program), _shaderManager->GetShader(variants.few));
		}
	}
	const auto found = _fewBonesVariants.find(program);
	return found != _fewBonesVariants.end() ? found->second : program;
}

const graphics::ShaderProgram* Renderer::PaletteVariant() const
{
	if (_paletteVariant == nullptr)
	{
		_paletteVariant = _shaderManager->GetShader("ObjectPaletteInstanced");
	}
	return _paletteVariant;
}

const graphics::ShaderProgram* Renderer::StaticVariant(const graphics::ShaderProgram* program) const
{
	if (_staticVariants.empty())
	{
		for (const auto& variants : k_BoneBudgetVariants)
		{
			_staticVariants.emplace(_shaderManager->GetShader(variants.program), _shaderManager->GetShader(variants.single));
		}
	}
	const auto found = _staticVariants.find(program);
	return found != _staticVariants.end() ? found->second : program;
}

void Renderer::DrawMesh(const graphics::L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept
{
	// Some of the game's meshes hold no geometry at all, like the singing stones' centre: there is nothing to draw
	if (mesh.GetNumSubMeshes() == 0)
	{
		return;
	}

	const auto& subMeshes = mesh.GetSubMeshes();
	// worked out once for every sub-mesh and primitive of this draw: nothing they read changes during it
	std::optional<MeshDrawValues> values;
	std::optional<uint32_t> transformCache;

	if (subMeshIndex != std::numeric_limits<uint8_t>::max())
	{
		if (subMeshIndex >= mesh.GetNumSubMeshes())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "tried to draw submesh out of range ({}/{})", subMeshIndex,
			                   mesh.GetNumSubMeshes());
		}

		DrawSubMesh(mesh, *subMeshes[subMeshIndex], desc, false, values, transformCache);
		return;
	}

	for (auto it = subMeshes.begin(); it != subMeshes.end(); ++it)
	{
		const L3DSubMesh& subMesh = **it;
		DrawSubMesh(mesh, subMesh, desc, std::next(it) != subMeshes.end(), values, transformCache);
	}
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
			auto& palettes = Locator::resources::value().GetLandLightPalettes();
			if (!palettes.Contains(LandLightPalette::k_Id.value()))
			{
				palettes.Load(LandLightPalette::k_Id.value(), resources::LandLightPaletteLoader::FromDiskTag {},
				              fileSystem.FindPath(path));
			}
			_landLightPalette = palettes.Handle(LandLightPalette::k_Id.value()).handle();
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No landscape light table (palette.raw): {}", e.what());
		}
		_landLightTexture.Reset(bgfx::createTexture2D(LandLightTable::k_Size, 1, false, 1, bgfx::TextureFormat::RGBA8,
		                                              BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(_landLightTexture.Get(), "LandLightTable");
		_landLightBuilt.reset();
	}
	if (!IsLandLit())
	{
		return;
	}
	// The overcast at the camera caps the base colour (Clouds::WeatherOvercastAtCamera); the lightning flash at the
	// camera lerps the table to white (sky_weather::LightningFlash -> weather::LightningFlashAtCamera) with this frame's
	// sky type: the table is built right after the sky type is updated, so its sky type is sky_type::Frame(), the value
	// the haze reads.
	// The inputs are read every frame (the overcast's lookup refreshes the weather cells it reads, which the others do not
	// read). The table is a function of them and of the palette, which never changes once loaded, and the texture keeps
	// what was last uploaded: with the same inputs as the last build, the table, the copy of it the game reads
	// (RenderFrameSystemInterface::GetLandLightTable) and the texture already hold what a new build would give
	const float skyType = sky_type::Frame();
	const float alignment = _skyAlignment.Get();
	const float overcast = Clouds::WeatherOvercastAtCamera();
	const uint8_t flash = sky_weather::LightningFlash();
	const LandLightInputs inputs {
	    .skyType = std::bit_cast<uint32_t>(skyType),
	    .alignment = std::bit_cast<uint32_t>(alignment),
	    .overcast = std::bit_cast<uint32_t>(overcast),
	    .flash = flash,
	};
	if (_landLightBuilt == inputs)
	{
		return;
	}
	_landLightBuilt = inputs;
	_landLight->Build(*_landLightPalette, skyType, alignment, overcast, flash);
	// the game's copy, read when something is coloured by the land's light outside the renderer
	Locator::renderFrameSystem::value().SetLandLightTable(*_landLight);
	const auto& texels = _landLight->GetTexels();
	bgfx::updateTexture2D(_landLightTexture.Get(), 0, 0, 0, 0, LandLightTable::k_Size, 1,
	                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(texels[0]))));
}

void Renderer::UpdateSkyTint() const
{
	_skyTint = {};
	_overcast = 0.0f;
	_skyFog = Locator::config::has_value() && detail_level::Fog(Locator::config::value().detailLevel);
	if (!IsLandLit() || !_landLightBuilt)
	{
		return;
	}
	// The light table's inputs of this frame. Its alignment is the sky's of the frame before, as the sky moves it later
	// in the frame (UpdateClouds), and the darkness follows that one too
	const auto& inputs = *_landLightBuilt;
	const auto overcast = std::bit_cast<float>(inputs.overcast);
	// The table's build holds an overcast above 0 to 1 at most, for everything drawn after it
	_overcast = overcast > 0.0f ? std::min(overcast, 1.0f) : overcast;
	const bool weather = Locator::config::has_value() && detail_level::Weather(Locator::config::value().detailLevel);
	// At the two lowest detail levels the dome is the day pictures unblended, starting from the land's colour tinted
	// by the frame's sky type; elsewhere from white
	const bool noBlend = Locator::config::has_value() && detail_level::SkyNoBlend(Locator::config::value().detailLevel);
	_skyTint = sky_dome::TintOf({
	    .colour = noBlend ? sky_dome::NoBlendColour(_landLight->GetLandColour(), sky_type::Frame()) : glm::u8vec3 {255},
	    .hazeColour = _landLight->GetHaze().colour,
	    .overcast = _overcast,
	    .flash = inputs.flash,
	    .darkness = sky_dome::Darkness(1.0f - std::bit_cast<float>(inputs.alignment)),
	    .fog = _skyFog,
	    .weather = weather,
	});
}

void Renderer::SetSkyUniforms(const ShaderProgram& skyShader) const
{
	// The sky's alignment as the game keeps it, 0 good to 2 evil. The dome texture's layers run the other way, from evil
	// to good
	const auto alignments = sky_dome::AlignmentPair(1.0f - _skyAlignment.Get());
	const glm::vec4 u_skyAlignment {2.0f - static_cast<float>(alignments.lower), 2.0f - static_cast<float>(alignments.upper),
	                                static_cast<float>(alignments.weight), 0.0f};
	const glm::vec4 u_skyModulate {glm::vec3(_skyTint.modulate) / 255.0f, 1.0f};
	const glm::vec4 u_skyAdd {glm::vec3(_skyTint.add) / 255.0f, 0.0f};
	skyShader.SetTextureSampler("s_diffuse", 0, Locator::skySystem::value().GetTexture());
	skyShader.SetUniformValue("u_skyAlignment", &u_skyAlignment);
	skyShader.SetUniformValue("u_skyModulate", &u_skyModulate);
	skyShader.SetUniformValue("u_skyAdd", &u_skyAdd);
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
			program->Submit(static_cast<bgfx::ViewId>(viewId));
		}
	}
}

void Renderer::DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare, uint32_t frameGameMs) const
{
	const auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const ecs::components::CelestialBody>(sky.GetSun());
	if (body == nullptr || !textures.Contains(body->textureId))
	{
		return;
	}
	// A script that hides the sun hides its glare too, and the glare's easing waits until it shows again
	if (ecs::sun_draw::IsHidden(registry, sky.GetSun()))
	{
		return;
	}
	// height by game hour, alpha fading in 3..6 h and out 18..21 h (sun::Place)
	const auto placement = sun::Place(sky.GetTime());
	if (!placement)
	{
		return;
	}
	const float alpha = placement->alpha;
	// A vertical quad at (-30000, y, -30000) turned by 3*pi/4 about Y, i.e. facing the island
	const glm::vec3 position = placement->position;
	// (inferred) the turn (+3 pi / 4 in the original's sense = glm's -3 pi / 4) is not read
	auto model = glm::translate(position) * glm::rotate(-3.0f * glm::pi<float>() / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto& texture = *textures.Handle(body->textureId);
	// mode 13: additive SRCALPHA / ONE, colour and alpha = texture x diffuse, no Z write, cull none; the glare with
	// ZFUNC ALWAYS
	const uint64_t additive =
	    render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite, {.zFunc = render_modes::ZFunc::Always});
	if (!glare)
	{
		// It shows less through an overcast
		const float shown = sky_dome::ThroughOvercast(alpha, _overcast, _skyFog);
		const glm::vec4 colour(glm::vec3(0x95, 0x7C, 0x63) / 255.0f, shown / 255.0f);
		DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour,
		                  render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite));
		return;
	}

	// Glare: 5 points around the sun, each hidden when the land's ray cast from the camera meets the land or the sea
	// near the camera, or else, when it is on the screen, when what is drawn at its pixel is nearer the camera than
	// the depth the original's depth buffer counts as hidden. The original reads that frame's depth buffer; bgfx has
	// no read back of it within the frame, so the drawing is asked with an occlusion query, answered a frame or two
	// later. The visibility eases toward (1 - 0.2 * hidden) * 255 by 1 % per ms
	const auto origin = camera.GetOrigin();
	const float nearClip = camera.GetDrawnNearClip(Locator::config::value().cameraNearClip);
	const auto points = sun::GlareSamplePoints(position, origin, nearClip);
	const bool asksDrawing =
	    detail_level::GlareDepthTest(ecs::draw_list::DetailIndexOf(Locator::config::value().detailLevel)) &&
	    (bgfx::getCaps()->supported & BGFX_CAPS_OCCLUSION_QUERY) != 0;
	const auto toClip = camera.GetViewProjectionMatrix();
	const int hidden = sun::HiddenSamples(points, [&](size_t i, const glm::vec3& point) {
		bool isHidden = Locator::pickingSystem::value().LandOrSeaAlong(origin, point, origin).has_value();
		const auto pixel = sun::SamplePixel(toClip * glm::vec4(point, 1.0f), nearClip, glm::vec2(_resolution));
		if (asksDrawing && pixel.has_value())
		{
			// The answer to the question asked of this sample's pixel the frames before, while it was on the screen. A
			// question not asked the frame before may still hold an answer from long ago, so it is made afresh: until
			// its first answer, only the land hides the sample
			auto& query = _glareQueries.at(i);
			auto& askedFrame = _glareAskedFrame.at(i);
			if (bgfx::isValid(query) && askedFrame + 1 != _framesDrawn)
			{
				bgfx::destroy(query);
				query = BGFX_INVALID_HANDLE;
			}
			if (!isHidden && bgfx::isValid(query) && bgfx::getResult(query) == bgfx::OcclusionQueryResult::Invisible)
			{
				isHidden = true;
			}
			if (!bgfx::isValid(query))
			{
				query = bgfx::createOcclusionQuery();
			}
			if (bgfx::isValid(query))
			{
				AskGlareSampleDrawn(viewId, camera, query, *pixel, sun::GlareHidingDepth(nearClip));
				askedFrame = _framesDrawn;
			}
		}
		return isHidden;
	});
	// (target - glare) x (frame game ms x 0.01) + glare, clamped to 0..255, with the frame's game ms (DrawClock); the
	// glare stays put in pause (0 ms)
	_sunGlare = sun::EaseGlare(_sunGlare, hidden, frameGameMs);
	if (_sunGlare <= 0.0f)
	{
		return;
	}
	model = model * glm::scale(glm::vec3(sun::k_GlareScale));
	// Cut to a whole number and weakened through an overcast, as the sun is
	const int shown = sun::GlareAlpha(_sunGlare, alpha, _overcast, _skyFog);
	const glm::vec4 colour(glm::vec3(0xA0, 0x6A, 0x35) / 255.0f, static_cast<float>(shown) / 255.0f);
	DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour, additive);
}

void Renderer::AskGlareSampleDrawn(graphics::RenderPass viewId, const Camera& camera, bgfx::OcclusionQueryHandle query,
                                   glm::ivec2 pixel, float depth) const
{
	// A square over the pixel at that depth from the camera, drawn after the scene against its depth and writing
	// nothing: none of it passes where the scene there is nearer
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	constexpr uint32_t k_Corners = 6;
	if (bgfx::getAvailTransientVertexBuffer(k_Corners, layout) < k_Corners)
	{
		return;
	}
	// Each corner on the line from the eye through it, as far along the view's axis as the depth
	const glm::vec2 screen(_resolution);
	const auto axis = camera.RayFromEye({0.5f, 0.5f}).direction;
	const auto at = [&](glm::vec2 corner) {
		const auto [eye, direction] = camera.RayFromEye(corner / screen);
		const auto point = eye + direction * (depth / glm::dot(direction, axis));
		return ScreenRectVertex {point.x, point.y, point.z, 0xFFFFFFFFu};
	};
	const glm::vec2 topLeft(pixel);
	const auto a = at(topLeft);
	const auto b = at(topLeft + glm::vec2(1.0f, 0.0f));
	const auto c = at(topLeft + glm::vec2(1.0f, 1.0f));
	const auto d = at(topLeft + glm::vec2(0.0f, 1.0f));
	const std::array<ScreenRectVertex, k_Corners> corners {a, b, c, a, c, d};
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, k_Corners, layout);
	std::memcpy(buffer.data, corners.data(), sizeof(corners));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA);
	const auto* program = _shaderManager->GetShader("DebugLine");
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()), query);
}

void Renderer::DrawMoon(graphics::RenderPass viewId, const Camera& camera) const
{
	const auto pass = sea_pass::ForPass(viewId);
	const bool mirrored = pass.mirrored;
	auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	auto& registry = Locator::entitiesRegistry::value();
	const auto moonEntity = sky.GetMoon();
	const auto* body = registry.TryGet<const ecs::components::CelestialBody>(moonEntity);
	const auto* glowTextures = registry.TryGet<const ecs::components::CelestialGlow>(moonEntity);
	auto* shown = registry.TryGet<ecs::components::Moon>(moonEntity);
	if (body == nullptr || glowTextures == nullptr || shown == nullptr)
	{
		return;
	}
	if (!textures.Contains(body->textureId) || !textures.Contains(body->alphaTextureId) ||
	    !textures.Contains(glowTextures->textureId) || !textures.Contains(glowTextures->alphaTextureId))
	{
		return;
	}
	// Position relative to the camera, by game hour; alpha m = min(200, 0.5 y - 110), so it shows about +-4.7 h
	// around midnight (moon::Place)
	const auto placement = moon::Place(sky.GetTime());
	if (!placement)
	{
		return;
	}
	// It shows less through an overcast, the glow too, and not at all once that leaves nothing
	const float m = sky_dome::ThroughOvercast(placement->alpha, _overcast, _skyFog);
	if (m <= 0.0f)
	{
		return;
	}
	// Only a moon that shows takes up the phase of the date, which follows the real clock (moon::Phase, whole days),
	// read at most once every two seconds; otherwise it keeps the phase it last showed with, which scripts are told of
	sky_frame::TakeMoonPhase(*shown, audio::spooky::UnixTime(), game_clock::TickCount());
	// (the reflection camera has the main camera's origin)
	const auto centre = camera.GetOrigin() + placement->offset;
	const auto frame = billboard::CameraFrame::From(camera);
	// The moon is drawn twice for the reflection, the second time at (x, -y, z) under water. That second call only
	// draws its glow (it skips the moon object): drawn here with the mirrored camera at (x, y, z), it is that glow,
	// built from the mirrored view (moon::Basis). The moon in the reflection is the first call's
	// DrawUnderWater, its matrix mirrored in y = 0: drawn with the mirrored camera, that is the main camera's moon
	// matrix itself (the mirrored view x mirror(y) is the main view, ReflectionXZCamera::GetViewMatrix), so the mesh
	// comes out flipped and its winding with it.
	const auto colour = IsLandLit() ? LandLightTable::ToColour(_landLight->GetMoonColour()) : glm::vec3(1.0f);

	// Glow: a 4000 x 4000 quad on the moon's basis (moon::MakeGlow), atmos.raw UV
	// 0.25..0.49375, additive (mode 13), colour (R/6, G/5, B/4, m)
	const auto drawGlow = [&]() {
		struct Vertex
		{
			float x, y, z, u, v;
		};
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .end();
		if (bgfx::getAvailTransientVertexBuffer(6, layout) == 6)
		{
			bgfx::TransientVertexBuffer buffer;
			bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
			auto* vertices = reinterpret_cast<Vertex*>(buffer.data);
			const auto glow = moon::MakeGlow(moon::Basis(frame.view, frame.inverseView, centre), centre);
			for (size_t i = 0; i < moon::k_GlowIndices.size(); ++i)
			{
				const auto k = static_cast<size_t>(moon::k_GlowIndices.at(i));
				const auto& p = glow.corners.at(k);
				vertices[i] = {p.x, p.y, p.z, glow.uvs.at(k).x, glow.uvs.at(k).y};
			}
			const auto* program = _shaderManager->GetShader("Celestial");
			const glm::mat4 identity(1.0f);
			const glm::vec4 glowColour(moon::GlowColour(colour), m / 255.0f);
			const glm::vec4 celestial(0.0f, 0.0f, 0.0f, 1.0f);
			bgfx::setTransform(&identity);
			program->SetTextureSampler("s_diffuse", 0, *textures.Handle(glowTextures->textureId));
			program->SetTextureSampler("s_alpha", 1, *textures.Handle(glowTextures->alphaTextureId));
			program->SetUniformValue("u_colour", &glowColour);
			program->SetUniformValue("u_celestial", &celestial);
			bgfx::setVertexBuffer(0, &buffer);
			bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite));
			program->Submit(static_cast<bgfx::ViewId>(viewId));
		}
	};

	// The moon (mode 4: SRCALPHA / INVSRCALPHA): the moon's basis x4, its tilt, RotateY(phase + pi), x0.65
	// (moon::Model), by the phase it shows with
	const auto drawMoon = [&]() {
		const auto phase = shown->phase;
		const auto mainView = mirrored ? sea_pass::UnmirrorView(frame.view) : frame.view;
		const auto mainInverseView = mirrored ? glm::inverse(mainView) : frame.inverseView;
		const auto model = moon::Model(moon::Basis(mainView, mainInverseView, centre), centre, phase);
		const glm::vec4 moonColour(colour, m / 255.0f);
		const glm::vec4 celestial(std::cos(phase), std::sin(phase), 1.0f, 1.0f);
		// The moon object is flagged to use the alternate mode table before its Draw and its DrawUnderWater, so both draw
		// mode 4 as 5 (the same blend, the Z written, ALPHAOP MODULATE(TEXTURE, DIFFUSE)), with the depth test less or
		// equal set before the sun and the moon. fs_celestial always modulates the alpha by u_colour, so the state of
		// mode 4 here already draws as mode 5, in both passes
		DrawCelestialMesh(viewId, sky.GetMoonMesh(), model, *textures.Handle(body->textureId), moonColour,
		                  render_modes::State(render_modes::Mode::AlphaTextured,
		                                      {.zFunc = render_modes::ZFunc::LessEqualInclusive,
		                                       .cull = pass.FaceCull(sea_pass::Surface::Model, false, false)}),
		                  celestial, &*textures.Handle(body->alphaTextureId));
	};

	// In the sky the glow is drawn first and the moon over it. Under the sea the moon's reflection, the first call's
	// DrawUnderWater, comes before the second call's glow, so there the mirrored moon is drawn first and its glow over it
	if (mirrored)
	{
		drawMoon();
		drawGlow();
	}
	else
	{
		drawGlow();
		drawMoon();
	}
}

namespace
{
/// The sky's clouds (Locator::cloudSystem)
ecs::systems::CloudSystemInterface& SkyClouds()
{
	if (!Locator::cloudSystem::has_value())
	{
		std::fputs("renderer: no cloud system in the locator (Locator::cloudSystem)\n", stderr);
		std::abort();
	}
	return Locator::cloudSystem::value();
}

/// The night lights (Locator::villageLightSystem)
ecs::systems::VillageLightSystemInterface& VillageLights()
{
	if (!Locator::villageLightSystem::has_value())
	{
		std::fputs("renderer: no village light system in the locator (Locator::villageLightSystem)\n", stderr);
		std::abort();
	}
	return Locator::villageLightSystem::value();
}
} // namespace

void Renderer::UpdateClouds() const
{
	const auto& detail = detail_level::Get(Locator::config::value().detailLevel);
	auto& cloudSystem = SkyClouds();
	// the layout of the last land opened (Clouds::OnLandscapeOpened): a frame makes none, and none before a land
	auto* clouds = cloudSystem.GetLayout();
	if (_cloudShadowImage.empty())
	{
		// the file system resolves the path when it reads: a missing file is reported by LoadOptionalBlob, not thrown
		_cloudShadowImage = resources::LoadOptionalBlob(
		    Locator::resources::value().GetBlobs(),
		    Locator::filesystem::value().GetPath<filesystem::Path::Textures>() / "sclouds.raw", "cloud shadows");
	}
	// game_clock::FrameGameMs: the game ms of this frame, whole, 0 while paused, faster or slower with the game speed.
	// The clouds and their animation stop while the game is paused. Its readers here: the sky's alignment, the clouds,
	// and for the night lights the jitter and the flames
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	const bool running = Locator::time::has_value() && !game_clock::IsPaused();
	if (running)
	{
		cloudSystem.Update(std::chrono::duration<float, std::milli>(milliseconds));
	}
	// CollectClouds advances the animation counters of the clouds it queues by this step
	_cloudMilliseconds = running ? milliseconds : 0.0f;
	// the sky's alignment moves towards the most influential player's
	_skyAlignment.Update(Clouds::InfluentialPlayerAlignment(), running ? milliseconds : 0.0f);

	// the colour and the alpha byte from the sky's alignment and light table[255]
	const uint32_t table255 = IsLandLit() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	const uint32_t colour = Clouds::Colour(_skyAlignment.Get(), table255);
	_cloudRgb = argb_colour::ToVec3(colour);
	const auto alignAlpha = static_cast<int>(colour >> 24);
	_cloudAlpha.resize(clouds != nullptr ? clouds->GetClouds().size() : 0);
	for (size_t i = 0; i < _cloudAlpha.size(); ++i)
	{
		// alpha = edge * A / 255 in integers, drawn only when it is not 0
		_cloudAlpha[i] =
		    detail.clouds ? static_cast<float>(Clouds::EdgeAlpha(clouds->GetClouds()[i]) * alignAlpha / 255) : 0.0f;
	}

	// This frame's land cells (land_light, the cell map's layout): the loaded ones back, then the map clouds' stamps
	// ("CloudShadows"), every stamp of this frame in the stamp list (PSys light maps, the storms, the flashes, the
	// fires), then the hand's and the village lights
	if (!Locator::terrainSystem::has_value())
	{
		land_light::ClearStamps();
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto size = island.GetCellMap().GetResolution();
	if (size != _landCellsSize)
	{
		_landCellsTexture.Reset(); // the old one before the new one is made
		_landCellsTexture.Reset(bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                              BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		_landCellsSize = size;
	}
	land_light::BeginFrame(island, Clouds::GetLandscapeGeneration());
	if (detail.clouds && clouds != nullptr)
	{
		clouds->StampShadows(_cloudShadowImage, _cloudAlpha);
	}
	// only into the blocks this frame's cull drew (the object draw list ran before this, in the frame's logic)
	land_light::ApplyStamps(Locator::objectDrawListSystem::has_value() ? Locator::objectDrawListSystem::value().DrawnBlocks()
	                                                                   : std::span<const uint8_t> {});
	land_light::ClearStamps();
	// Night lights: the hand light and the village lights into this frame's luminosities, after the stamps
	// (Locator::villageLightSystem, over the table UpdateLandLight built this frame)
	if (IsLandLit() && Locator::time::has_value() && Locator::dayNightClock::has_value())
	{
		VillageLights().Update(std::chrono::duration<float, std::milli>(game_clock::IsPaused() ? 0.0f : milliseconds));
	}
	const auto texels = land_light::Texels();
	if (texels.size() == static_cast<size_t>(size.x) * size.y * 4)
	{
		bgfx::updateTexture2D(_landCellsTexture.Get(), 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
	}
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectClouds(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	auto* clouds = SkyClouds().GetLayout();
	if (clouds == nullptr || !detail_level::Clouds(Locator::config::value().detailLevel))
	{
		return order;
	}
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	if (mesh.GetNumSubMeshes() == 0)
	{
		return order;
	}
	const auto origin = camera.GetOrigin();
	// as for the mists: only a cloud whose sphere
	// (the mesh's bounding-box half diagonal x size x 0.55) touches the screen is queued and advances its counter
	const float meshRadius = glm::length(mesh.GetBoundingBox().Size()) * 0.5f;
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	const float milliseconds = _cloudMilliseconds;
	_cloudMilliseconds = 0.0f;
	// mist.l3d is loaded without skins; the mist draw uses the smoke material instead
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return order;
	}
	order.reserve(clouds->GetClouds().size());
	for (size_t index = 0; index < clouds->GetClouds().size(); ++index)
	{
		const auto& cloud = clouds->GetClouds()[index];
		const auto position = Clouds::WorldPosition(cloud);
		if (_cloudAlpha[index] / 255.0f <= 0.0f || !SphereInView(viewProjection, position, meshRadius * cloud.size * 0.55f))
		{
			continue;
		}
		// the frame's whole game milliseconds, held in a float
		clouds->AdvanceAnimation(index, static_cast<uint32_t>(milliseconds));
		// the Z-sorter key: |position - camera|^2, (x^2 + y^2) + z^2
		order.emplace_back(zsort::Key(position, origin), static_cast<uint32_t>(index));
	}
	return order;
}

void Renderer::DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	const auto* clouds = SkyClouds().GetLayout();
	if (clouds == nullptr || index >= clouds->GetClouds().size())
	{
		return;
	}
	const auto rgb = _cloudRgb;
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto origin = camera.GetOrigin();
	// the same billboard as the map mists (Renderer::DrawMist): the inverse camera rotation, in glm
	// mat3(right, -forward, up): local X = screen right, local Y (the dome's axis) towards the camera, local Z =
	// screen up (billboard::MistBasis)
	const auto cameraFrame = billboard::CameraFrame::From(camera);
	const auto& rotation = billboard::MistBasis(cameraFrame);
	// The clouds are mists too (with a size and a shrink), so their draw is the effect branch of the same mist draw:
	// no specular (it is never written) and the temporary light straight above.
	const glm::vec4 u_cloudSpecular(0.0f);
	const auto* program = _shaderManager->GetShader("Cloud");
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	const auto& cloud = clouds->GetClouds()[index];
	const auto position = Clouds::WorldPosition(cloud);
	// row 0 (local X, the screen width) is scaled by the size and rows 1-2 (local Y = depth, local Z = screen
	// height) by the shrunk one, so a cloud is round only straight overhead and near the horizon it is about k (2.5
	// to 5, set when the clouds open) times wider than tall (billboard::MistShrunkSize)
	const float shrunk = billboard::MistShrunkSize(cloud.size, cloud.k, position - origin);
	const auto model = glm::translate(position) * glm::mat4(rotation) * glm::scale(glm::vec3(cloud.size, shrunk, shrunk));
	const glm::vec4 u_cloudColour(rgb, _cloudAlpha[index] / 255.0f);
	// one whole atlas cell, rows 2-3 (the frame after this frame's step;
	// frame_anim::MistCellUv of the effect branch)
	const auto cell = frame_anim::MistCellUv(Clouds::GetFrame(cloud), true);
	// the mist draw saves the engine light and moves it straight above while it draws the clouds, with the ambient at
	// 210; both go back afterwards. model_light::LightInMeshSpace brings the light into the mesh's own space, normalised
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
			// the smoke material: mode 6, two-sided
			bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
			program->Submit(static_cast<bgfx::ViewId>(viewId));
		}
	}
}

void Renderer::DrawObjectReflections(graphics::RenderPass viewId, bool handReflected) const
{
	if (!Locator::handSystem::has_value())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& hand = Locator::handSystem::value();
	// DrawUnderWater of the held object (by the hand, after the hand, in its own colour) and of the physics objects
	// (while not wholly under water: y > -r, r = the farthest vertex). "Own colour" is the object colour and specular
	// as the last Draw left them: the land light and cell specular of the physics objects' draw.
	// The physics objects are the awake entries of the physics list (the asleep ones are skipped: no resting proxy, so
	// no broken house); r is the body's radius (the farthest vertex). (pending) a fragment's own 3D object is a rock in
	// the default colour (white, no specular): the original mirrors that rock, openblack the piece. The hand's thrown
	// objects that are not in the physics keep the bounding box radius.
	struct Reflected
	{
		entt::entity entity;
		float radius; ///< < 0: always drawn (the held object); 0: from the bounding box
		float centreY;
	};
	std::vector<Reflected> objects;
	// the render hand's object, held from the press: it casts no dynamic shadow while held, restored when the hand
	// throws it. It is mirrored only with the hand, so not while the hand is hidden
	if (const auto held = hand.GetRenderHandObject();
	    held.has_value() && sea_reflection::ReflectsVillager({.inHand = true, .handReflected = handReflected}))
	{
		objects.push_back({*held, -1.0f, 0.0f});
	}
	Locator::dynamicsSystem::value().ForEachEntry([&objects](const ecs::PhysicsEntry& po) {
		if (po.entity != entt::null && !po.body.resting)
		{
			objects.push_back({po.entity, po.body.Radius(), po.body.Centre().y});
		}
	});
	for (const auto entity : hand.GetThrownObjects())
	{
		if (Locator::dynamicsSystem::value().Find(entity) == nullptr)
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
			// the scale it is drawn at (a creature is drawn smaller in its pen)
			const float radius = bodyRadius > 0.0f ? bodyRadius
			                                       : 0.5f * glm::length(mesh->GetBoundingBox().Size()) *
			                                             ecs::creature_pose::DrawnScale(registry, entity).x;
			if ((bodyRadius > 0.0f ? centreY : transform.position.y) <= -radius)
			{
				continue;
			}
		}
		// DrawUnderWater in the colour the last Draw left (the held object, the physics objects)
		DrawUnderWater(viewId, entity, sea_pass::UnderWaterLastDraw());
	}
}

void Renderer::DrawFishShoals(graphics::RenderPass viewId) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/misc0").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/misc0a").value();
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
	Locator::entitiesRegistry::value().Each<const ecs::components::FishFarm>(
	    [&vertices](const ecs::components::FishFarm& farm) {
		    if (!farm.shoal.has_value() || !farm.shoal->visible)
		    {
			    return;
		    }
		    const uint32_t colour = (static_cast<uint32_t>(farm.shoal->alpha) << 24) | 0x00FFFFFFu;
		    for (size_t i = 0; i < std::min(farm.shoal->shown, farm.shoal->fish.size()); ++i)
		    {
			    const auto& fish = farm.shoal->fish[i];
			    // a flat sprite: a quad turned about Y, its local x along the heading (billboard::Horizontal); cells 8..23 of
			    // the 8 x 8 sheet
			    billboard::Sprite sprite {
			        // drawn before the sea without a mirror: mirrored back in the reflection target (see
			        // DrawPass; on the CPU, vs_blob is shared by 6 programs)
			        .position = sea_pass::Unmirror(fish.position),
			        .size = fish.halfSize,
			        .angle = fish.heading,
			        .cell = fish.cell, // taken before the frame's wrap (frame_anim::FishFrame)
			        .horizontal = true,
			    };
			    const auto quad = billboard::Horizontal(sprite);
			    for (const int k : billboard::k_SpriteTriangles)
			    {
				    const auto& p = quad.corners.at(static_cast<size_t>(k));
				    const auto& uv = quad.uv.at(static_cast<size_t>(k));
				    vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, colour});
			    }
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
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
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// the misc0 material (inferred), mode 6: SRCALPHA / INVSRCALPHA, no Z write; two-sided. ZFUNC ALWAYS (approximate:
	// the original keeps LESSEQUAL; equivalent because the mirrored land under them wrote no Z)
	bgfx::setState(render_modes::State(render_modes::materials::k_Misc0, {.zFunc = render_modes::ZFunc::Always}));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}

void Renderer::PreloadForLand() const noexcept
{
	// (openblack engine) what the draw would load lazily, loaded with the land: the chimney smoke's alpha texture
	LoadChimneySmokeAlpha();
	// the help text's three fonts, which the original loads when it makes its help text, not on the first text drawn
	for (const auto font : {help::TextFont::J0, help::TextFont::F1, help::TextFont::F3})
	{
		[[maybe_unused]] const auto* loaded = GameFontAt(font);
	}
	// Textures and meshes no longer call bgfx::frame() when they are made: two frames here hand everything the land
	// made to the render thread and wait for its upload, so the upload stays in the load and not in the first frames
	bgfx::frame();
	bgfx::frame();
}

void Renderer::DrawWaterRings(graphics::RenderPass viewId) const
{
	const auto rings = Locator::waterRingSystem::value().GetRings();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = water_rings::k_TextureId.value();
	constexpr entt::id_type k_Alpha = water_rings::k_AlphaTextureId.value();
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
		                                                          static_cast<float>(ring.argb >> 24)) >>
		                                         8) &
		                   0xFFu;
		// the colour as the creator left it (the light colour was fixed when it was added)
		const uint32_t abgr = argb_colour::ToAbgr(ring.argb, alpha);
		// a flat sprite: a quad turned about Y (billboard::Horizontal), the z half size x the aspect
		billboard::Sprite sprite {
		    .position = ring.position,
		    .size = half,
		    .height = ring.aspect,
		    .angle = ring.angle,
		    .cell = frame_anim::SpriteCell(ring.cell), // fixed at creation
		    .horizontal = true,
		};
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
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// smoke.raw in mode 13 (inferred): SRCALPHA / ONE, no Z write
	bgfx::setState(render_modes::State(render_modes::materials::k_SmokeAdditive));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}

namespace
{
/// A shadow blob's point: the local point `local` of bone `bone` of the drawn pose, through the drawn model matrix
/// (the blobs read the frame's skinned bone buffer). Shared by the villagers' feet and the animals' EBone points.
/// (approximate) The glm chain: the original's, BlobBones + BlobPointExact below, needs the frame's locals
glm::vec3 BlobPointApprox(const glm::mat4& model, const glm::mat4& bone, const glm::vec3& local)
{
	return glm::vec3(model * bone * glm::vec4(local, 1.0f));
}

/// The frame's skinned bone buffer, in clipping space: SkinBones of the frame's locals
/// (SkeletalAnimation::locals, SampleLocal's) under the root BoneRootToClip(object matrix, W); (approximate)
/// the object matrix is FromModel(DrawnModel), openblack's cells. Without a clip, the mesh's rest locals, as the
/// original skins them without an animation. (approximate) a clip of another skeleton also falls back to the rest
/// locals: the original skins any clip. Empty only for a mesh without bones
std::vector<affine::AffineMatrix> BlobBones(const glm::mat4& model, const ecs::components::SkeletalAnimation* animation,
                                            const L3DMesh& mesh, const affine::CameraMatrices& lh)
{
	std::vector<affine::AffineMatrix> skinned;
	const auto& parents = mesh.GetBoneParents();
	const bool clip = animation != nullptr && !animation->locals.empty() && animation->locals.size() == parents.size();
	const auto& source = clip ? animation->locals : mesh.GetBoneLocals();
	if (source.empty() || source.size() != parents.size())
	{
		return skinned;
	}
	std::vector<affine::AffineMatrix> locals;
	locals.reserve(source.size());
	for (const auto& local : source)
	{
		locals.push_back(affine::FromModel(local));
	}
	const auto root = affine::BoneRootToClip(affine::FromModel(model), lh.worldToClipping);
	affine::SkinBones(locals, parents, root, skinned);
	return skinned;
}

/// A blob's point of a skinned bone: M = Mul(bone, clippingToWorld) (the feet and the EBone points), for an EBone
/// point then MultiplyReversed(M, E), whose translation reads only E's position; the point is the translation row (its y then
/// goes onto the ground)
glm::vec3 BlobPointExact(const affine::AffineMatrix& bone, const affine::AffineMatrix& clippingToWorld,
                         const std::optional<glm::vec3>& eBone)
{
	auto m = affine::Mul(bone, clippingToWorld);
	if (eBone.has_value())
	{
		affine::AffineMatrix e;
		e.m[9] = eBone->x;
		e.m[10] = eBone->y;
		e.m[11] = eBone->z;
		m = affine::MultiplyReversed(m, e);
	}
	return {m.m[9], m.m[10], m.m[11]};
}
} // namespace

void Renderer::DrawGroundBlobs(graphics::RenderPass viewId, const graphics::SuperVillagerFrame& superVillagers,
                               const graphics::ObjectListFrame& objectList, const affine::CameraMatrices& lh) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/human_shadow").value();
	if (!textures.Contains(k_Texture) || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	// each point on the ground + the blob lift (land_morph, algorithm D)
	const auto ground = land_morph::Altitude(island);
	const auto& meshes = Locator::resources::value().GetMeshes();
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	// the blobs' quads (ground_blobs): opaque at the feet, white
	const auto addQuad = [&vertices](const ground_blobs::Quad& quad) {
		for (const auto i : ground_blobs::k_Indices)
		{
			const auto& p = quad.corners.at(i);
			const auto& uv = ground_blobs::k_Uvs.at(i);
			const auto alpha = static_cast<uint32_t>(ground_blobs::k_Opacity.at(i) * 255.0f);
			vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, alpha << 24u | 0x00FFFFFFu});
		}
	};
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const ecs::components::Villager, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Villager& villager, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    // none for villagers in the water (y <= 0.2), nor for one no longer shadowed (a dead one), nor for a
		    // SuperVillager (its temporary shadow turns the human shadow off), nor for one the object draw list leaves out
		    // (the villager's own Draw draws the blobs)
		    if (transform.position.y <= ground_blobs::k_LowestHeight || !villager.humanShadowed || !meshes.Contains(mesh.id) ||
		        std::ranges::find(superVillagers.notHumanShadowed, entity) != superVillagers.notHumanShadowed.end() ||
		        objectList.LeftOut(entity))
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    // the feet of the drawn pose (ecs/Animations.h) where the villager is drawn (ecs/MobileDrawing.h)
		    const auto* animation = registry.TryGet<const ecs::components::SkeletalAnimation>(entity);
		    const auto bones = ground_blobs::FootBones(animation != nullptr ? std::span<const glm::mat4>(animation->pose)
		                                                                    : std::span<const glm::mat4> {},
		                                               l3d->GetBoneMatrices());
		    if (bones.size() <= ground_blobs::k_FootBones[0])
		    {
			    return;
		    }
		    // the two feet: bone matrix slots 21 and 18 (ends of the leg chains), on the ground + 0.2, of the drawn matrix
		    // (ecs::DrawnModel: in the physics the drawn pose between its last two turns) WITH the slope shear: the villager's
		    // draw applies MultiplyReversed(object matrix, S) with S = identity + the shear cells m1 / m7 before the 3D draw:
		    // the skin's root, and so the feet, are sheared
		    const auto model = ecs::DrawnModel(registry, entity, true, ecs::PoseOf::Drawing);
		    // BlobPointExact on the skinned bones (the clip's locals, or the rest ones without a clip); BlobPointApprox, the
		    // glm chain (approximate), only for a mesh without bones
		    const auto skinned = BlobBones(model, animation, *l3d, lh);
		    const auto foot = [&](size_t bone) {
			    auto p = skinned.empty() ? BlobPointApprox(model, bones[bone], glm::vec3(0.0f))
			                             : BlobPointExact(skinned[bone], lh.clippingToWorld, std::nullopt);
			    p.y = land_morph::OnGround(ground, glm::vec2(p.x, p.z), ground_blobs::k_Lift);
			    return p;
		    };
		    const auto a = foot(ground_blobs::k_FootBones[0]);
		    const auto b = foot(ground_blobs::k_FootBones[1]);
		    // the light offset projected onto the land's plane, n = the land normal there
		    const auto n = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto feet = ground_blobs::Feet(a, b, ground_blobs::Fall(n, transform.scale.x));
		    addQuad(feet[0]);
		    addQuad(feet[1]);
	    },
	    entt::exclude<ecs::components::Unavailable>);
	// Animals (with the human shadow flag): the points of their mesh's EBone block, 2 or 4 quads. The original passes
	// the first quad of each pair V = D (it builds D + (P1 - P0) / 2 but hands over &D); the second gets D + (P0 - P1) / 2.
	registry.Each<const ecs::components::Animal, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Animal& animal, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    // the animal's Draw draws them, so none for one the object draw list leaves out
		    if (!animal.humanShadowed || transform.position.y <= ground_blobs::k_LowestHeight || !meshes.Contains(mesh.id) ||
		        objectList.LeftOut(entity))
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    const auto& points = l3d->GetBlobPoints();
		    // the blobs read the bone buffer filled with the frame's skinning: the animated pose, as the villagers above;
		    // the bind pose only without a matching SkeletalAnimation
		    const auto* animation = registry.TryGet<const ecs::components::SkeletalAnimation>(entity);
		    const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                            ? animation->pose
		                            : l3d->GetBoneMatrices();
		    if (points.empty())
		    {
			    return;
		    }
		    // the matrix the mesh is drawn with this frame (the root bone x object matrix x world-to-clip):
		    // ecs::DrawnModel (HandDrawPose in the render hand, then PhysicsDrawPose, DrawPosition, Transform) WITH the slope
		    // shear: the animal's draw applies MultiplyReversed(object matrix, S), which shears the matrix the skin's root and
		    // so the blobs come from
		    const auto model = ecs::DrawnModel(registry, entity, true, ecs::PoseOf::Drawing);
		    // the land normal at the drawn matrix's translation x, z
		    const auto n = island.GetNormalAt(glm::vec2(model[3].x, model[3].z));
		    const auto d = ground_blobs::Fall(n, transform.scale.x);
		    // BlobPointExact on the skinned bones (the clip's locals, or the rest ones without a clip); BlobPointApprox, the
		    // glm chain (approximate), only for a point whose bone the mesh does not have
		    const auto skinned = BlobBones(model, animation, *l3d, lh);
		    std::array<glm::vec3, 4> p {};
		    for (size_t k = 0; k < points.size(); ++k)
		    {
			    const auto& [bone, position] = points[k];
			    const auto boneMatrix = bone < bones.size() ? bones[bone] : glm::mat4(1.0f);
			    p[k] = bone < skinned.size() ? BlobPointExact(skinned[bone], lh.clippingToWorld, position)
			                                 : BlobPointApprox(model, boneMatrix, position);
			    p[k].y = land_morph::OnGround(ground, glm::vec2(p[k].x, p[k].z), ground_blobs::k_Lift);
		    }
		    for (size_t k = 0; k + 1 < points.size(); k += 2)
		    {
			    addQuad(ground_blobs::MakeQuad(p[k], d));
			    addQuad(ground_blobs::MakeQuad(p[k + 1], d + (p[k] - p[k + 1]) * 0.5f));
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
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
	bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}

void Renderer::BuildObjectListRows(const DrawSceneDesc& drawDesc) const
{
	_objectListRows.clear();
	const auto& frame = drawDesc.objectList;
	if (frame.ran && frame.applied && Locator::rendereringSystem::has_value())
	{
		const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
		for (const auto& row : renderCtx.entityRows)
		{
			// a held object's row is never left out: the hand draws it, outside the list
			if (!frame.LeftOut(row.entity) || renderCtx.sortedInstances.contains(row.index))
			{
				continue;
			}
			if (_objectListRows.empty())
			{
				_objectListRows.assign(renderCtx.instanceUniforms.size(), 1);
			}
			if (row.index < _objectListRows.size())
			{
				_objectListRows[row.index] = 0;
			}
		}
	}
}

void Renderer::PreDraw(const DrawSceneDesc& drawDesc) const noexcept
{
	// the object rows the draw list leaves out of this frame, which the object draw skips
	BuildObjectListRows(drawDesc);
	// (openblack engine) what DrawScene wrote before it drew, in the same order: the sky type of the frame, the dome,
	// the land light table, the frame's light, the dynamic shadows and the clouds. Kept on the logic side so that the
	// draw only reads (docs/wiki/engine/loop.md#threads-plan). Not with the full screen film (DrawScene's first test), as
	// before
	if (video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld())
	{
		// no PSys list of an earlier frame (its pointers into the effects) survives a frame
		// without PreDraw
		_particleFrame.Clear();
		return;
	}
	// The sky, once a frame from the landscape draw: it samples the sky type of the visual time, then rebuilds the land
	// light table (UpdateLandLight below) and advances the dome. Here once per DrawScene, so the reflection pass does not
	// advance the dome a second time. The dome moves on with every scene drawn, whether or not the sky is drawn.
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().UpdateFrame(true);
		// A frame drawn inside the temple shows the sun again, whatever a script had set
		if (InTemple() && Locator::entitiesRegistry::has_value())
		{
			ecs::sun_draw::ShowAgain(Locator::entitiesRegistry::value(), Locator::skySystem::value().GetSun());
		}
	}
	UpdateLandLight();
	UpdateSkyTint();
	// Before the models, the landscape draw sets the engine's one light for this frame. The focus is the player hand's
	// model position, used even while the hand is hidden, and not the point under the cursor of GetPlayerHandPositions.
	// Known difference: with the cursor off the land the original still moves the hand along the mouse ray at its
	// distance from view (it keeps |camera - position| when the ray misses), while HandSystem::Place leaves the hand
	// where it was last placed, so at night the light stays there.
	bool placed = false;
	// Inside the temple the hand carries no light, and the light stays the default sun
	if (!InTemple() && Locator::dayNightClock::has_value() && Locator::handSystem::has_value() &&
	    Locator::entitiesRegistry::has_value() && drawDesc.camera != nullptr)
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		const auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			// the sky type of the visual time computed there, not the frame's sample (the same value: the sky samples the
			// same visual time right after)
			model_light::UpdateFrameLight(registry.Get<const ecs::components::Transform>(hand).position,
			                              drawDesc.camera->GetOrigin(),
			                              sky_type::At(Locator::dayNightClock::value().Clock().GetVisualTime()));
			placed = true;
		}
	}
	if (!placed)
	{
		// The original sets the light every frame; with no hand or camera to place it here, the light falls back to
		// its day branch, the default sun, instead of keeping an old frame's (inferred)
		model_light::SetLight(model_light::k_DefaultSun);
	}
	UpdateShadows(drawDesc);
	if (drawDesc.drawIsland)
	{
		UpdateClouds();
	}
	// Distance haze of this frame (graphics::haze::Frame and the "Fog" detail key), from the
	// land light table just built: the same for the reflection and the main pass
	_haze = IsLandLit() ? haze::Frame() : haze::Params {};
	_hazeUniforms = haze::Uniforms(_haze);
	// the clouds the main view draws (each is queued with the blended things); only the
	// main pass with the sky collects them (the reflection does not)
	_preClouds.clear();
	if (drawDesc.drawSky)
	{
		_preClouds = CollectClouds(*drawDesc.camera);
	}
	// the temples' lanterns, every frame of the world view whatever is on screen, after the night lights' stamps and
	// before the objects: made with their lights' draws on the temple's first lit frame, then drawn while it is lit
	if (!InTemple() && Locator::entitiesRegistry::has_value())
	{
		auto* lights = Locator::villageLightSystem::has_value() ? &Locator::villageLightSystem::value() : nullptr;
		worship::citadel_lanterns::DrawAll(Locator::entitiesRegistry::value(), lights);
	}
	// The main pass's writes that the logic shares, in the order and under the tests the main pass made them (the
	// reflection pass makes none of them): with the entities, the creators of the surfaces of revolution that are
	// gone, then the mists (with the sky, with or without the entities: Mist.counter, mists::Submit's list), the
	// chimney smoke (with the sprites: after the clouds' and the night lights' CRT draws, as before), the rain
	// (RainSystemInterface::TakeTiles) and the influence border's alpha
	if (drawDesc.drawEntities)
	{
		particles::surf_revol::PruneCreators();
	}
	_preMists.clear();
	if (drawDesc.drawSky)
	{
		_preMists = CollectMists(*drawDesc.camera);
	}
	_preSmoke.clear();
	if (drawDesc.drawEntities && drawDesc.drawSprites)
	{
		_preSmoke = CollectChimneySmoke(*drawDesc.camera);
	}
	_preRain.clear();
	_preSnow.clear();
	_preInfluenceScroll.reset();
	if (rain::FrameTakesTiles(drawDesc.drawEntities, InTemple()))
	{
		_preRain = CollectRain(*drawDesc.camera);
	}
	if (snowfall::FrameTakesTiles(drawDesc.drawEntities, InTemple()))
	{
		_preSnow = CollectSnow(*drawDesc.camera);
	}
	if (drawDesc.drawEntities)
	{
		UpdateInfluenceCurtain(*drawDesc.camera);
	}
	// the temples' leash posts, every frame of the world view (the landscape draw moves their spin on, whatever is on
	// screen); not inside the temple, where the world is not drawn
	_frameLeashPosts.clear();
	_frameLightColumns.clear();
	if (!InTemple())
	{
		CollectLeashPosts();
		CollectLightColumns(drawDesc);
	}
	// the golden showers the effects' atoms draw over their objects join the sparkles' queue, from the world's effects
	// drawn in the main view with the entities, as CollectFrameParticles collects them
	if (drawDesc.viewId == graphics::RenderPass::Main && drawDesc.camera != nullptr && drawDesc.drawEntities)
	{
		auto inputs = particles::manager::LiveInputs();
		inputs.turnFraction = drawDesc.clock.turnFraction;
		particles::manager::QueueShowers(inputs);
	}
	// the gold scrolls' sparkles the object draw list queued: they step when the blended things are drawn, at the
	// frame's end, after everything else of the frame's logic; then only they are collected
	ecs::script_highlight::DrainSparkles(drawDesc.clock.frameGameMs);
	// the main pass's PSys collects, made here once with what the draw used to pass them (the desc's turn fraction,
	// camera origin and hand) and under the same tests (the main view; the Sorted pieces with
	// the entities). After PruneCreators above, as the draw's surf_revol::Collect was
	CollectFrameParticles(drawDesc);
}

namespace
{
/// The particle service (Locator::particleSystem), which keeps the sheets of light
ecs::systems::ParticleSystemInterface& ParticleService()
{
	if (!Locator::particleSystem::has_value())
	{
		std::fputs("renderer: no particle service in the locator (Locator::particleSystem)\n", stderr);
		std::abort();
	}
	return Locator::particleSystem::value();
}
} // namespace

void Renderer::CollectFrameParticles(const DrawSceneDesc& drawDesc) const
{
	auto& frame = _particleFrame;
	frame.Clear();
	// the draw collected them in the main view and with the entities only (DrawPass's `if (desc.drawEntities)`)
	if (drawDesc.viewId != graphics::RenderPass::Main || drawDesc.camera == nullptr || !drawDesc.drawEntities)
	{
		return;
	}
	// the effects, the surfaces, the exploded pieces and the sheets of light (drawn from their own Z objects in the main
	// view only, never in the reflection), at the frame's share of the turn, by the particle service
	// the world's: the temple's rooms draw no particles of the world's frame (their pass never reads it), and no effect is
	// one of theirs yet
	ParticleService().CollectDrawFrame(drawDesc.clock.turnFraction, frame, false);
	// the landscape vortices' effects: the ground ones' surfaces drawn before the land, none while a vortex does not show
	// (RendererVortex.cpp)
	SplitVortexParticles();
}

void Renderer::DrawScene(const DrawSceneDesc& drawDesc) const noexcept
{
	// With the full screen film, alpha == 1.0 and not the falling spell's film, the 3D world is not drawn; the film, the
	// script fade and the help system's bars still are, in the end of frame's order: bars, film, fade
	// (DrawFinishFrameOverlays). (approximate) the main view is cleared to openblack's colour as always (the original
	// does not clear: the film covers the screen).
	// In mode 2 (the falling spell, Video/FallingSpellVideo.h) no land is drawn either: the falling spell draws the film
	// with base alpha 0x50 first, then the copy of the creature over it, before the end of the frame (not ported: the
	// liquid particles). (inferred) what lies under that 31 % film in the original (no clear read): here openblack's
	// clear colour
	if (video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld())
	{
		bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
		if (video::GetFallingSpell().HidesWorld())
		{
			DrawVideoOverlay(graphics::RenderPass::Main);
			if (drawDesc.camera != nullptr)
			{
				DrawFallingCreature(*drawDesc.camera);
			}
		}
		DrawFinishFrameOverlays(drawDesc.overlay, drawDesc.camera, drawDesc.interfaceLayers);
		return;
	}
	// the creatures drawn this frame, their shapes and their painted skins; none on a land without one
	CollectCreatures();
	// (Renderer::PreDraw: the sky type, the dome, the land light, the frame's light, the shadows and the clouds of this
	// frame were made just before this, in this order, then the haze, the clouds, the foliage, the surfaces' creators,
	// the mists, the chimney smoke, the rain and the influence border of the main view)
	// The land from above for the temple's map, once a visit; nothing outside the temple
	DrawTempleMapPass(drawDesc);
	// Reflection Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::ReflectionPass);
		if (drawDesc.drawWater)
		{
			UpdateReflectionTarget();
			DrawSceneDesc drawPassDesc = drawDesc;
			// the mirrored land is drawn without the small bump map (sea_pass::SeaPassState::smallBump)
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
			drawPassDesc.drawIsland = detail_level::Get(Locator::config::value().detailLevel).landReflection;
			// "LandRef": the mirrored scene under the sea is only the sky and the land; models and sprites are not
			// reflected
			drawPassDesc.drawEntities = false;
			drawPassDesc.drawSprites = false;
			// (the culling of the mirrored camera comes from sea_pass::ForPass(Reflection))

			DrawPass(drawPassDesc);
		}
		else if (InTemple())
		{
			// The temple has no sea: the reflection is its main room's, under its floor
			DrawTempleReflection(drawDesc);
		}
	}

	// Main Draw Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::MainPass);
		DrawPass(drawDesc);
	}
	// The end of the frame: the "before" callbacks after the Z-sorter's flush, the spirits' trails (priority 100); the
	// help system's 3D draw: the spirits in the world (its did-you-know bubble went into the Z-sorter, drained with
	// the main pass); then the overlay spirits (priority 100) after the Z reset, before
	// the 2D rectangles (10000) and the texts (20000)
	DrawSpiritTrails(*drawDesc.camera, drawDesc.overlay);
	DrawSpirits(*drawDesc.camera, drawDesc.overlay, false);
	DrawSpirits(*drawDesc.camera, drawDesc.overlay, true);
	DrawFinishFrameOverlays(drawDesc.overlay, drawDesc.camera, drawDesc.interfaceLayers);
}

void Renderer::DrawFinishFrameOverlays(const OverlayFrame& overlay, const Camera* camera, const InterfaceLayers& layers) const
{
	// The interface's panels, drawn by HelpText's callback too: the fight's after the dialogue's texts, the creature's
	// after the input prompt icons' second draw
	const auto fightPanel = [&layers]() {
		if (layers.fightPanel)
		{
			layers.fightPanel();
		}
	};
	const auto creaturePanel = [&layers]() {
		if (layers.creaturePanel)
		{
			layers.creaturePanel();
		}
	};
	// The end of the frame, all in the Sequential ScreenOverlay view: the bars, then the last callbacks, among them the
	// video player's draw, so the film is drawn over the bars and fits between them at 100 % (FullScreenRect's letterbox
	// is barH at pct 1); the fade last, over the film
	// The control pictures over the temple's scroll seen close up, drawn by the room's draw, before the help system's
	DrawTempleScrollIcons(overlay);
	// The input prompt icons' first draw, the end of the help system's 3D draw: in the 3D frame, before the end of the frame,
	// so under the bars and the overlay spirits: its own view FinishFrameIcons (before FinishFrame3D and ScreenOverlay)
	DrawInputPrompts(overlay, 0);
	if (video::GetFallingSpell().HidesWorld())
	{
		// mode 2: the falling spell drew the film itself (clearing the player's pending flag so its end-of-frame callback
		// draws nothing) and its creature, before the end of the frame (DrawScene): then the Z-sorter (its sparks), its
		// callback (its bursts), the bars and the fade
		DrawFallingSpellOverlay(camera);
		DrawHandTricons(overlay);
		DrawScreenOverlay(overlay, false);
		DrawHelpText(overlay); // HelpText's callback, as in the other branch
		DrawSpeedIndicator(overlay, camera);
		fightPanel();
		DrawInputPrompts(overlay, 1); // the input prompt icons' second draw, at the end of HelpText's
		creaturePanel();
		DrawScreenOverlay(overlay, true);
		return;
	}
	// the floating numbers, which the world's draw sends to the transparent queue: drawn when it is flushed at the start
	// of the end of the frame, so over the icons' first draw and under the bars
	DrawFloatingNumbers(overlay);
	// the camera's helper icons by the hand: a finish-frame callback after the hand and the spirits, before the bars
	DrawHandTricons(overlay);
	DrawScreenOverlay(overlay, false);
	// HelpText's callback (priority 20000) runs after the bars and before the film (one of the last callbacks)
	DrawHelpText(overlay);
	// the game speed's words, the end of HelpText's draw
	DrawSpeedIndicator(overlay, camera);
	fightPanel();
	// the input prompt icons' second draw, the end of HelpText's (also on its early exit): every InputPromptIcon again
	DrawInputPrompts(overlay, 1);
	creaturePanel();
	DrawVideoOverlay();
	DrawScreenOverlay(overlay, true);
}

void Renderer::DrawVideoOverlay(graphics::RenderPass pass) const
{
	const auto frame = video::Get().GetFrame();
	if (!frame || frame->width == 0 || frame->height == 0 || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const glm::u16vec2 size(static_cast<uint16_t>(frame->width), static_cast<uint16_t>(frame->height));
	if (!_videoTexture.IsValid() || _videoTextureSize != size)
	{
		_videoTexture.Reset(); // the old one before the new one is made
		// the tiles are clamped: the material's tiling bit is cleared.
		// (inferred) the driver's bilinear filter
		_videoTexture.Reset(bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                          BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(_videoTexture.Get(), "Video");
		_videoTextureSize = size;
		_videoSerial.reset();
	}
	if (_videoSerial != frame->serial)
	{
		// uploaded after each picture decoded
		bgfx::updateTexture2D(_videoTexture.Get(), 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(frame->rgba.data(), static_cast<uint32_t>(frame->rgba.size())));
		_videoSerial = frame->serial;
	}
	if (!_videoAlphaTexture.IsValid())
	{
		const uint8_t white = 0xFF;
		_videoAlphaTexture.Reset(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R8,
		                                               BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(&white, 1)));
	}

	// drawn to the screen at (colour, 0, bars, W + 1, H - 2 bars + 1) with W, H the screen size
	const int width = _resolution.x;
	const int height = _resolution.y;
	const auto rect = video::FullScreenRect(width, height);
	// the size of a texel on screen, w / film width and h / film height
	const float sx = static_cast<float>(rect.width) / static_cast<float>(frame->width);
	const float sy = static_cast<float>(rect.height) / static_cast<float>(frame->height);
	// the film's mosaic: ceil(size / 256) tiles a side
	constexpr uint32_t k_Tile = 0x100;
	const uint32_t tilesX = (frame->width + k_Tile - 1) / k_Tile;
	const uint32_t tilesY = (frame->height + k_Tile - 1) / k_Tile;
	const uint32_t abgr = argb_colour::ToAbgr(frame->colour); // the diffuse of the four vertices
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
		// the last row has height & 0xFF texels (256 if that is 0)
		const uint32_t rows = (ty == tilesY - 1 && (frame->height & 0xFFu) != 0) ? (frame->height & 0xFFu) : k_Tile;
		for (uint32_t tx = 0; tx < tilesX; ++tx)
		{
			// the last column width & 0xFF
			const uint32_t columns = (tx == tilesX - 1 && (frame->width & 0xFFu) != 0) ? (frame->width & 0xFFu) : k_Tile;
			// x0 = x + tx * 256 * sx, x1 = x0 + columns * sx, likewise y (pre-transformed)
			const float x0 = static_cast<float>(rect.x) + static_cast<float>(tx * k_Tile) * sx;
			const float x1 = x0 + static_cast<float>(columns) * sx;
			const float y0 = static_cast<float>(rect.y) + static_cast<float>(ty * k_Tile) * sy;
			const float y1 = y0 + static_cast<float>(rows) * sy;
			// u, v from 1/512 to n/256 - 1/512 of the 256x256 tile, half a texel inside each edge;
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
	const auto viewId = static_cast<bgfx::ViewId>(pass);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	// mode 6 (MODULATE colour and alpha): colour = texture x diffuse, alpha = texture alpha (1) x diffuse alpha
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, fromBgfx(_videoTexture.Get()));
	program->SetTextureSampler("s_alpha", 1, fromBgfx(_videoAlphaTexture.Get()));
	bgfx::setVertexBuffer(0, &buffer);
	// a mode 6 material, two-sided (CULLMODE NONE): SRCALPHA / INVSRCALPHA, no Z write; ZFUNC ALWAYS and
	// ZWRITEENABLE 0 from the screen draw's two false flags
	constexpr render_modes::Material k_VideoMaterial {render_modes::Mode::AlphaTexturedAlphaNoZWrite, render_modes::k_TwoSided};
	bgfx::setState(render_modes::State(k_VideoMaterial, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	program->Submit(viewId);
}

namespace
{
// atmos.raw / atmosa.raw (the atmosphere's) and mousehelp.raw / mousehelpa.raw (the mouse picture's), loaded by
// Game.cpp as raw/<file stem>
constexpr entt::id_type k_KeyOrMouseAtmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_KeyOrMouseAtmosAlpha = entt::hashed_string("raw/ATMOSA").value();
constexpr entt::id_type k_KeyOrMouseMouse = entt::hashed_string("raw/mousehelp").value();
constexpr entt::id_type k_KeyOrMouseMouseAlpha = entt::hashed_string("raw/mousehelpa").value();

/// A text colour (argb) as GameFont's rgba
glm::vec4 TextRgba(uint32_t argb)
{
	return glm::vec4(static_cast<float>((argb >> 16) & 0xFFu), static_cast<float>((argb >> 8) & 0xFFu),
	                 static_cast<float>(argb & 0xFFu), static_cast<float>(argb >> 24)) /
	       255.0f;
}
} // namespace

void Renderer::DrawInputPrompts(const OverlayFrame& overlay, size_t pass) const
{
	// The input prompt icons' draw half, twice a frame as the original: pass 0 from the help system's 3D draw, pass 1 from
	// HelpText's end-of-frame callback, each icon with the alpha its own update left (input_prompt::Frame twice with the same
	// dt). Only the frame's copy is read (OverlayFrame::inputPrompts, newest first as the original's list); the hand's point
	// and the tick count are the copy's too. Under an active box the copy is empty (input_prompt::Snapshot)
	const auto& frame = overlay.inputPrompts;
	_keyOrMouseView = pass == 0 ? graphics::RenderPass::FinishFrameIcons : graphics::RenderPass::ScreenOverlay;
	_keyOrMouseHand = frame.hand;
	_keyOrMouseTicks = frame.tickCount;
	for (const auto& icon : frame.icons)
	{
		// c1.a = c2.a = int(a x 255); the panels at int(alpha8 x a)
		const float alpha = icon.alpha.at(pass);
		const auto byte = static_cast<uint32_t>(TruncateToInt(alpha * 255.0f)) & 0xFFu;
		const uint32_t c1 = (icon.c1 & 0x00FFFFFFu) | (byte << 24);
		const uint32_t c2 = (icon.c2 & 0x00FFFFFFu) | (byte << 24);
		// the row: a key's name for a key with no codes (animType -1, clickType 0), an int otherwise
		const KeyOrMouseRow row = icon.animType == -1 && icon.clickType == 0 ? KeyOrMouseRow(std::u16string_view(icon.keyName))
		                                                                     : KeyOrMouseRow(icon.row);
		DrawKeyOrMouse(icon.animType, icon.clickType, row, std::u16string_view(icon.text), icon.x, icon.y, icon.size,
		               icon.align, &c1, &c2, TruncateToInt(static_cast<float>(icon.alpha8) * alpha));
	}
}

void Renderer::AddScreenQuad(std::vector<SpiritQuadVertex>& out, float x0, float y0, float x1, float y1, float u0, float v0,
                             float u1, float v1, uint32_t argb) const
{
	// a pre-transformed textured rectangle in pixels, here straight to clip space. (approximate) the original box
	// draw's clip (+-0xA000) and inv_w 100.0 are left out
	const float width = _resolution.x;
	const float height = _resolution.y;
	const uint32_t abgr = argb_colour::ToAbgr(argb);
	const float l = 2.0f * x0 / width - 1.0f;
	const float r = 2.0f * x1 / width - 1.0f;
	const float t = 1.0f - 2.0f * y0 / height;
	const float b = 1.0f - 2.0f * y1 / height;
	const SpiritQuadVertex a {l, t, 0.5f, u0, v0, abgr};
	const SpiritQuadVertex bq {r, t, 0.5f, u1, v0, abgr};
	const SpiritQuadVertex c {r, b, 0.5f, u1, v1, abgr};
	const SpiritQuadVertex d {l, b, 0.5f, u0, v1, abgr};
	for (const auto& vertex : {a, bq, c, a, c, d})
	{
		out.push_back(vertex);
	}
}

void Renderer::AddKeyOrMousePanel(std::vector<SpiritQuadVertex>& out, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                  uint32_t argb) const
{
	// A 9-slice of the soft blob (0, 0.25)-(0.24609375, 0.49609375) of atmos.raw, the draw alpha = argb >> 24 and the
	// rgb of argb. b = (y1 - y0) / 4; each corner is a 2b square centred on the rect's corner, the edges and the centre
	// between them
	const int32_t b = (y1 - y0) / 4;
	const std::array<float, 4> xs {static_cast<float>(x0 - b), static_cast<float>(x0 + b), static_cast<float>(x1 - b),
	                               static_cast<float>(x1 + b)};
	const std::array<float, 4> ys {static_cast<float>(y0 - b), static_cast<float>(y0 + b), static_cast<float>(y1 - b),
	                               static_cast<float>(y1 + b)};
	// the breaks: u 0, 0.078125, 0.16796875, 0.24609375; v 0.25, 0.328125, 0.41796875, 0.49609375
	constexpr std::array<float, 4> k_U {0.0f, 0.078125f, 0.16796875f, 0.24609375f};
	constexpr std::array<float, 4> k_V {0.25f, 0.328125f, 0.41796875f, 0.49609375f};
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			AddScreenQuad(out, xs.at(column), ys.at(row), xs.at(column + 1), ys.at(row + 1), k_U.at(column), k_V.at(row),
			              k_U.at(column + 1), k_V.at(row + 1), argb);
		}
	}
}

void Renderer::DrawKeyOrMouseIn(graphics::RenderPass view, const InputPromptFrame& frame, int32_t animType, int32_t clickType,
                                KeyOrMouseRow row, std::optional<std::u16string_view> text, int32_t x, int32_t y, int32_t s,
                                uint32_t align, const uint32_t* c1, const uint32_t* c2, int32_t alpha8) const
{
	const auto savedView = _keyOrMouseView;
	const auto savedHand = _keyOrMouseHand;
	const auto savedTicks = _keyOrMouseTicks;
	_keyOrMouseView = view;
	_keyOrMouseHand = frame.hand;
	_keyOrMouseTicks = frame.tickCount;
	DrawKeyOrMouse(animType, clickType, row, text, x, y, s, align, c1, c2, alpha8);
	_keyOrMouseView = savedView;
	_keyOrMouseHand = savedHand;
	_keyOrMouseTicks = savedTicks;
}

void Renderer::DrawKeyOrMouse(int32_t animType, int32_t clickType, KeyOrMouseRow row, std::optional<std::u16string_view> text,
                              int32_t x, int32_t y, int32_t s, uint32_t align, const uint32_t* c1In, const uint32_t* c2In,
                              int32_t alpha8) const
{
	// A key or mouse button icon with its text, panels and arrows
	const GameFont* font = GameFontAt(help::TextFont::J0);
	// (openblack) no font or no view yet: nothing
	if (font == nullptr || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const int32_t width = _resolution.x;
	const int32_t height = _resolution.y;
	int32_t boxX = x;
	int32_t boxY = y;
	if ((align & 6) == 6)
	{
		// the hand's point (projected, the frame's copy) plus (x, y); Y clamped to [0, H - S], then X to [0, W - S]
		if (!_keyOrMouseHand)
		{
			return;
		}
		const auto box = key_or_mouse::BoxAtTheHand(*_keyOrMouseHand, x, y, s, width, height);
		boxX = box.x;
		boxY = box.y;
	}
	else
	{
		boxY = y - s / 2; // y is the box's vertical centre
	}

	// a null c1 is white, a null c2 is c1; nothing when both alphas are under 4
	const uint32_t c1 = c1In != nullptr ? *c1In : 0xFFFFFFFFu;
	const uint32_t c2 = c2In != nullptr ? *c2In : c1;
	if ((c1 >> 24) < 4 && (c2 >> 24) < 4)
	{
		return;
	}

	// the icon's width. A key (animType -1): its text is the row's string when clickType is 0; with key codes the key
	// names (GetKeyNameText-style, inferred), "%s + %s" of the row's and clickType's when the row is not 0. Its width is
	// max(1.0, GetStringWidth(text, 0.4 S) + int(2S / 3))
	std::u16string keyText;
	float iconWidth = 0.0f;
	if (animType == -1)
	{
		if (clickType == 0)
		{
			if (const auto* name = std::get_if<std::u16string_view>(&row); name != nullptr)
			{
				keyText = *name;
			}
		}
		// (not ported) the key codes' names: no caller of openblack passes codes (input_prompt::ResolveAction
		// gives a name)
		iconWidth = std::max(1.0f, font->GetStringWidth(keyText, static_cast<float>(s) * 0.4f) + static_cast<float>(2 * s / 3));
	}
	else
	{
		// a mouse button: S wide, none for the row 3
		const auto* mouseRow = std::get_if<int32_t>(&row);
		iconWidth = (mouseRow != nullptr && *mouseRow == 3) ? 0.0f : static_cast<float>(s);
	}
	// keyW': + S / 2 for each stub 0x400 / 0x800
	float keyWidth = iconWidth;
	if ((align & 0x400) != 0)
	{
		keyWidth += static_cast<float>(s / 2);
	}
	if ((align & 0x800) != 0)
	{
		keyWidth += static_cast<float>(s / 2);
	}
	// textSize int(2S / 3); (pending) int(4S / 5) for languages that need bigger text: openblack has no language
	const float textSize = static_cast<float>(2 * s / 3);
	// total = textW + keyW' + 2.0; a null text gives textW 0 and total keyW'
	const std::u16string textString = text ? std::u16string(*text) : std::u16string();
	const float textWidth = text ? font->GetStringWidth(textString, textSize) : 0.0f;
	const float total = text ? textWidth + keyWidth + 2.0f : keyWidth;

	// the horizontal layout. align & 1 right-aligns; align & 2 picks the side, with the hysteresis _keyOrMouseSide: 1
	// past 2W / 3, 0 below W / 3; side 1 ends S / 2 right of X, side 0 starts S / 2 left of it
	const auto horizontal = key_or_mouse::Layout(boxX, total, s, align, width, _keyOrMouseSide);
	boxX = horizontal.x;
	const bool rightAligned = horizontal.rightAligned;
	// the order: align & 0x10 puts the text first; with 0x20, the side / right-align flag instead
	bool textFirst = (align & 0x10) != 0;
	if ((align & 0x20) != 0)
	{
		textFirst = rightAligned;
	}
	int32_t iconX = textFirst ? TruncateToInt(static_cast<float>(boxX) + textWidth + 2.0f) : boxX;
	const int32_t textX = textFirst ? boxX : TruncateToInt(static_cast<float>(boxX) + keyWidth + 2.0f);
	if ((align & 0x400) != 0)
	{
		iconX += s / 2;
	}

	// every part is its own submit in the pass's Sequential view (FinishFrameIcons / ScreenOverlay), in the original's
	// order
	const auto viewId = _keyOrMouseView;
	// in the world (a $m of the did-you-know bubble) the view keeps its camera and the pieces are placed in the world
	const auto& inWorld = _keyOrMouseInWorld;
	if (!inWorld.has_value())
	{
		const glm::mat4 identity(1.0f);
		bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), glm::value_ptr(identity), glm::value_ptr(identity));
	}
	// the box draw has no depth test (ZFUNC ALWAYS), the Atmos materials have no Z write (modes 6 and 13)
	const uint64_t atmosState = render_modes::State(render_modes::materials::k_Atmos, {.zFunc = render_modes::ZFunc::Always});
	const uint64_t additiveState =
	    render_modes::State(render_modes::materials::k_AtmosAdditive, {.zFunc = render_modes::ZFunc::Always});

	// the panels, only with alpha8 != 0: AddKeyOrMousePanel in the additive material
	if (alpha8 != 0)
	{
		std::vector<SpiritQuadVertex> panels;
		// behind the icon, when its width is not 0: c2's rgb at alpha8
		if (iconWidth != 0.0f)
		{
			AddKeyOrMousePanel(panels, iconX, boxY, TruncateToInt(static_cast<float>(iconX) + iconWidth), boxY + s,
			                   (c2 & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha8) << 24));
		}
		// the rest in c1's rgb at alpha8 / 3
		const uint32_t dim = (c1 & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha8 / 3) << 24);
		// behind the text, when it is not empty: o = (S - textSize) * 0.5
		if (!textString.empty())
		{
			const float o = (static_cast<float>(s) - textSize) * 0.5f;
			AddKeyOrMousePanel(panels, textX, TruncateToInt(static_cast<float>(boxY) + o),
			                   TruncateToInt(static_cast<float>(textX) + textWidth),
			                   TruncateToInt(static_cast<float>(boxY + s) - o), dim);
		}
		// the arrow stubs: 0x400 left, 0x800 right, 0x100 up, 0x200 down
		if ((align & 0x400) != 0)
		{
			AddKeyOrMousePanel(panels, iconX - s / 2, boxY + s / 4, iconX, boxY + 3 * s / 4, dim);
		}
		if ((align & 0x800) != 0)
		{
			AddKeyOrMousePanel(panels, TruncateToInt(static_cast<float>(iconX) + iconWidth), boxY + s / 4,
			                   TruncateToInt(static_cast<float>(s / 2) + static_cast<float>(iconX) + iconWidth),
			                   boxY + 3 * s / 4, dim);
		}
		if ((align & 0x100) != 0)
		{
			AddKeyOrMousePanel(panels, iconX + s / 4, boxY - s / 2, iconX + 3 * s / 4, boxY, dim);
		}
		if ((align & 0x200) != 0)
		{
			AddKeyOrMousePanel(panels, iconX + s / 4, boxY + s, iconX + 3 * s / 4, boxY + s / 2 + s, dim);
		}
		SubmitSpiritQuads(viewId, PlaceInWorld(std::move(panels), inWorld), k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha,
		                  additiveState);
	}

	if (animType == -1)
	{
		// the key cap from iconX to int(iconX + iconW), Y to Y + S: a 3-slice in the atmos material, b = (y1 - y0) / 4
		// wide ends, white with the draw alpha = c2.a, v 0.001953125..0.248046875, u 0.501953125 / 0.564453125 /
		// 0.685546875 / 0.748046875
		const int32_t x0 = iconX;
		const int32_t y0 = boxY;
		const int32_t x1 = TruncateToInt(static_cast<float>(iconX) + iconWidth);
		const int32_t y1 = boxY + s;
		const int32_t b = (y1 - y0) / 4;
		const uint32_t capAlpha = c2 >> 24;
		const uint32_t white = 0x00FFFFFFu | (capAlpha << 24);
		constexpr float k_V0 = 0.001953125f;
		constexpr float k_V1 = 0.248046875f;
		std::vector<SpiritQuadVertex> cap;
		AddScreenQuad(cap, static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x0 + b), static_cast<float>(y1),
		              0.501953125f, k_V0, 0.564453125f, k_V1, white);
		AddScreenQuad(cap, static_cast<float>(x0 + b), static_cast<float>(y0), static_cast<float>(x1 - b),
		              static_cast<float>(y1), 0.564453125f, k_V0, 0.685546875f, k_V1, white);
		AddScreenQuad(cap, static_cast<float>(x1 - b), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1),
		              0.685546875f, k_V0, 0.748046875f, k_V1, white);
		SubmitSpiritQuads(viewId, PlaceInWorld(std::move(cap), inWorld), k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha, atmosState);
		// its text at (x0 + h / 5, y0 + h / 7), h = y1 - y0, size h x 1.0 x 0.4, black with the cap's alpha. (inferred,
		// static) checked on the original's code, not at run time
		std::vector<GameFont::Vertex> glyphs;
		const int32_t h = y1 - y0;
		font->AddText(glyphs, keyText, static_cast<float>(x0 + h / 5), static_cast<float>(y0 + h / 7),
		              static_cast<float>(h) * 1.0f * 0.4f, TextRgba(capAlpha << 24));
		SubmitScreenText(*font, glyphs, _keyOrMouseView, inWorld);
	}
	else if (const auto* mouseRow = std::get_if<int32_t>(&row); mouseRow != nullptr && *mouseRow != 3)
	{
		DrawKeyOrMouseMouse(clickType, *mouseRow, animType, iconX, boxY, s, c2);
	}

	// the arrows: the atmos material, colour c2 with the draw alpha = c2.a, grown by g = S x 0.1111111 (S / 9) on
	// each side
	if ((align & 0xF00) != 0)
	{
		const float g = static_cast<float>(s) * 0.1111111f;
		const float fx = static_cast<float>(iconX);
		const float fy = static_cast<float>(boxY);
		std::vector<SpiritQuadVertex> arrows;
		const auto addArrow = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
			AddScreenQuad(arrows, static_cast<float>(TruncateToInt(x0)), static_cast<float>(TruncateToInt(y0)),
			              static_cast<float>(TruncateToInt(x1)), static_cast<float>(TruncateToInt(y1)), u0, v0, u1, v1, c2);
		};
		const float quarter = static_cast<float>(s / 4);
		const float threeQuarters = static_cast<float>(3 * s / 4);
		const float half = static_cast<float>(s / 2);
		if ((align & 0x400) != 0)
		{
			// left
			addArrow(fx - half - g, fy + quarter - g, fx + g, fy + threeQuarters + g, 0.75390625f, 0.12890625f, 0.87109375f,
			         0.24609375f);
		}
		if ((align & 0x800) != 0)
		{
			// right
			addArrow(fx - g + iconWidth, fy + quarter - g, fx + g + half + iconWidth, fy + threeQuarters + g, 0.87890625f,
			         0.00390625f, 0.99609375f, 0.12109375f);
		}
		if ((align & 0x100) != 0)
		{
			// up
			addArrow(fx + quarter - g, fy - half - g, fx + threeQuarters + g, fy + g, 0.75390625f, 0.00390625f, 0.87109375f,
			         0.12109375f);
		}
		if ((align & 0x200) != 0)
		{
			// down
			addArrow(fx + quarter - g, fy + static_cast<float>(s) - g, fx + threeQuarters + g,
			         fy + half + static_cast<float>(s) + g, 0.87890625f, 0.12890625f, 0.99609375f, 0.24609375f);
		}
		SubmitSpiritQuads(viewId, PlaceInWorld(std::move(arrows), inWorld), k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha,
		                  atmosState);
	}

	// the text, only when not null: drawn three times with j0 at textSize, y = Y + o with o a float (not floored);
	// black (c1's alpha) at (textX - 1, Y + o - 1) and (textX + 1, Y + o + 1), then c1 at (textX, Y + o). (not ported)
	// the text draw's fifth argument, 1.1 x the near plane
	if (text)
	{
		const float o = (static_cast<float>(s) - textSize) * 0.5f;
		const float textY = static_cast<float>(boxY) + o;
		const auto shadow = TextRgba(c1 & 0xFF000000u);
		std::vector<GameFont::Vertex> glyphs;
		font->AddText(glyphs, textString, static_cast<float>(textX - 1), textY - 1.0f, textSize, shadow);
		font->AddText(glyphs, textString, static_cast<float>(textX + 1), textY + 1.0f, textSize, shadow);
		font->AddText(glyphs, textString, static_cast<float>(textX), textY, textSize, TextRgba(c1));
		SubmitScreenText(*font, glyphs, _keyOrMouseView, inWorld);
	}
}

void Renderer::DrawKeyOrMouseMouse(int32_t clickType, int32_t row, int32_t animType, int32_t x, int32_t y, int32_t s,
                                   uint32_t argb) const
{
	// An S x S quad of mousehelp.raw (256 x 256, 4 x 4 cells of 64 px) with mousehelpa.raw, material mode 5 (made on
	// the first draw)
	if (row == 3)
	{
		return;
	}
	// the lit state from the tick count: animType 1 (T / 100) % 5 < 2; any other always (the game asks for no other
	// blink). The frame's game_clock::TickCount, read before the draw: both passes
	// use the same one (the original's two calls read it twice, the same but on a tick boundary)
	const uint32_t ticks = _keyOrMouseTicks;
	const int32_t d = static_cast<int32_t>((ticks / 100) % 5);
	bool lit = true;
	if (animType == 1)
	{
		lit = help::click_cue::ButtonLit(ticks);
	}
	// the cell by clickType, r the row; 1 and anything else
	// is the right button's cell mirrored, the left button
	int32_t cell = 0;
	bool mirrored = false;
	switch (clickType)
	{
	case 0:
		cell = 4 * row;
		break;
	case 2:
		cell = 4 * row + (lit ? 1 : 0);
		break;
	case 3:
		cell = 4 * row + (lit ? 2 : 0);
		break;
	case 4:
		cell = 4 * row + (lit ? 3 : 0);
		break;
	case 8:
		cell = d <= 3 ? 15 - d : 11; // the wheel down frames
		break;
	case 0x10:
		cell = d <= 3 ? 12 + d : 11; // the wheel up frames
		break;
	default:
		mirrored = true;
		cell = 4 * row + (lit ? 1 : 0);
		break;
	}
	// nothing for a negative cell or a colour of 0
	if (cell < 0 || argb == 0)
	{
		return;
	}
	// u = (c & 3) x 0.25, v = (c >> 2) x 0.25, mirrored: u + 0.25 to u; the vertices (x, y), (x + S, y), (x + S, y + S),
	// (x, y + S)
	const float u0 = static_cast<float>(cell & 3) * 0.25f + (mirrored ? 0.25f : 0.0f);
	const float u1 = u0 + (mirrored ? -0.25f : 0.25f);
	const float v0 = static_cast<float>(cell / 4) * 0.25f;
	const float v1 = v0 + 0.25f;
	std::vector<SpiritQuadVertex> quad;
	AddScreenQuad(quad, static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + s), static_cast<float>(y + s), u0,
	              v0, u1, v1, argb);
	// mode 5, tiling off, ZFUNC ALWAYS and ZWRITEENABLE 0 around the 2D draw
	constexpr render_modes::Material k_MouseHelp {render_modes::Mode::AlphaTexturedAlpha, 0};
	SubmitSpiritQuads(_keyOrMouseView, PlaceInWorld(std::move(quad), _keyOrMouseInWorld), k_KeyOrMouseMouse,
	                  k_KeyOrMouseMouseAlpha,
	                  render_modes::State(k_MouseHelp, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
}

const GameFont* Renderer::GameFontAt(help::TextFont font) const
{
	// as HelpText loads them: j0, f1 if loaded, else j0, f3 if loaded, else j0
	const auto index = static_cast<size_t>(font);
	if (!_fontLoadTried.at(index))
	{
		_fontLoadTried.at(index) = true;
		static constexpr std::array<std::string_view, 3> k_Names {"j0.met", "f1.met", "f3.met"};
		auto base = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / k_Names.at(index);
		_fonts.at(index) = resources::LoadGameFont(Locator::resources::value().GetFonts(), base.replace_extension());
	}
	if (_fonts.at(index))
	{
		return _fonts.at(index).get();
	}
	return font == help::TextFont::J0 ? nullptr : GameFontAt(help::TextFont::J0);
}

float Renderer::MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept
{
	// The string width in the font HelpText would draw it with. Logic side, read-only, no bgfx calls; GameFontAt's
	// first-use font load must move before the game or through gpu::Submit (pending)
	const GameFont* gameFont = GameFontAt(font);
	return gameFont != nullptr ? gameFont->GetStringWidth(text, size) : 0.0f;
}

void Renderer::SubmitScreenText(const GameFont& font, const std::vector<GameFont::Vertex>& glyphs, graphics::RenderPass view,
                                const std::optional<ScreenInWorld>& inWorld) const
{
	if (glyphs.empty() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const float width = _resolution.x;
	const float height = _resolution.y;
	const auto toClip = [width, height](float px, float py) {
		return glm::vec2(2.0f * px / width - 1.0f, 1.0f - 2.0f * py / height);
	};
	const auto viewId = static_cast<bgfx::ViewId>(view);
	// in a world view the view keeps its camera, the glyphs placed in the world
	if (!inWorld.has_value())
	{
		const glm::mat4 identity(1.0f);
		bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
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
		if (inWorld.has_value())
		{
			const auto world = screen_point::WorldAtDepth(*inWorld->toWorld, p, inWorld->depth);
			vertices.push_back({world.x, world.y, world.z, g.u, g.v, g.abgr});
			continue;
		}
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
	program->SetTextureSampler("s_diffuse", 0, font.GetTexture());
	bgfx::setVertexBuffer(0, &buffer);
	// mode 16 (the font pages' material): SRCALPHA / INVSRCALPHA, no Z write, ZFUNC ALWAYS (no depth test) or, in the
	// world, the given one; its alpha test is fs_text's
	bgfx::setState(render_modes::State(render_modes::Mode::TexturedChromaAlphaNoZWrite,
	                                   {.zFunc = inWorld.has_value() ? inWorld->zFunc : render_modes::ZFunc::Always}));
	program->Submit(viewId);
}

void Renderer::DrawScreenOverlay(const OverlayFrame& overlay, bool drawFade) const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// ScriptFade's colour and its bar (CinemaBars::BarHeight), read before the draw (FillOverlayFrame)
	const uint32_t colour = overlay.fade.colour;
	const int width = _resolution.x;
	const int height = _resolution.y;
	const int bar = overlay.fade.barPixels;
	if (drawFade ? (colour >> 24) == 0 : bar == 0)
	{
		return;
	}
	std::vector<ScreenRectVertex> vertices;
	const auto addRect = [this, &vertices](int x0, int y0, int x1, int y1, uint32_t argb) {
		AddScreenRect(vertices, x0, y0, x1, y1, argb);
	};
	// the bars, 0xFF000000, at the top and the bottom
	const auto addBars = [&]() {
		if (bar > 0)
		{
			addRect(0, 0, width, bar, 0xFF000000u);
			addRect(0, height - bar, width, height, 0xFF000000u);
		}
	};
	if (drawFade)
	{
		// the fade: x 0..W-1, y h'..H-1-h' with h' = h ? h - 1 : 0, then the bars again so the fade never tints them
		const int inset = bar > 0 ? bar - 1 : 0;
		addRect(0, inset, width - 1, height - 1 - inset, colour);
	}
	addBars();
	SubmitScreenRects(vertices);
}

void Renderer::SubmitScreenRects(const std::vector<ScreenRectVertex>& vertices) const
{
	if (vertices.empty())
	{
		return;
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
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(ScreenRectVertex));
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::setVertexBuffer(0, &buffer);
	// mode 1 (untextured, SRCALPHA / INVSRCALPHA), ZFUNC ALWAYS and ZWRITEENABLE 0 by hand (the 2D rectangle queue the
	// same)
	bgfx::setState(
	    render_modes::State(render_modes::Mode::SmoothAlpha, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	_shaderManager->GetShader("DebugLine")->Submit(viewId);
}

void Renderer::AddScreenRect(std::vector<ScreenRectVertex>& out, int x0, int y0, int x1, int y1, uint32_t argb) const
{
	// pre-transformed rectangles in pixels (rhw 1), here straight to clip space
	const float width = _resolution.x;
	const float height = _resolution.y;
	const uint32_t abgr = argb_colour::ToAbgr(argb);
	const float l = 2.0f * static_cast<float>(x0) / width - 1.0f;
	const float r = 2.0f * static_cast<float>(x1) / width - 1.0f;
	const float t = 1.0f - 2.0f * static_cast<float>(y0) / height;
	const float b = 1.0f - 2.0f * static_cast<float>(y1) / height;
	for (const auto& [x, y] : {std::pair {l, t}, {r, t}, {r, b}, {l, t}, {r, b}, {l, b}})
	{
		out.push_back({x, y, 0.5f, abgr});
	}
}

std::vector<SpiritQuadVertex> Renderer::PlaceInWorld(std::vector<SpiritQuadVertex> vertices,
                                                     const std::optional<ScreenInWorld>& inWorld)
{
	if (inWorld.has_value())
	{
		for (auto& v : vertices)
		{
			const auto world = screen_point::WorldAtDepth(*inWorld->toWorld, {v.x, v.y}, inWorld->depth);
			v.x = world.x;
			v.y = world.y;
			v.z = world.z;
		}
	}
	return vertices;
}

void Renderer::DrawDidYouKnowBubble(graphics::RenderPass view, const Camera& camera, const OverlayFrame& overlay) const
{
	if (!overlay.didYouKnow.active || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const auto& bubble = overlay.didYouKnow;
	// the pixels go back into the world through the camera the view draws with, so that they land where they were laid
	// out, at their depth along the view
	const auto toWorld =
	    screen_point::ClipToWorldOf(camera.GetViewMatrix(Camera::Interpolation::Current), camera.GetProjectionMatrix());
	const auto toClip = [this](float x, float y) {
		return glm::vec2(2.0f * x / _resolution.x - 1.0f, 1.0f - 2.0f * y / _resolution.y);
	};
	// pixel vertices whose z is their depth along the view
	const auto placed = [&toWorld, &toClip](std::vector<SpiritQuadVertex> vertices) {
		for (auto& v : vertices)
		{
			const auto world = screen_point::WorldAtDepth(toWorld, toClip(v.x, v.y), v.z);
			v.x = world.x;
			v.y = world.y;
			v.z = world.z;
		}
		return vertices;
	};
	// gatheringtext.raw in a mode 6 material, with its alpha gatheringtexta.raw: no Z write, tested against the world's
	// depth as the frame's
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/gatheringtext").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/gatheringtexta").value();
	const uint64_t tested = render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite, {.writeAlpha = true});
	SubmitSpiritQuads(view, placed(bubble.shape), k_Texture, k_Alpha, tested);
	// the scroll arrows: boxes in the atmos material, yellow at the draw alpha, with no depth test (the box draw's)
	std::vector<SpiritQuadVertex> arrows;
	for (const auto& arrow : bubble.arrows)
	{
		AddScreenQuad(arrows, arrow.x0, arrow.y0, arrow.x1, arrow.y1, arrow.u0, arrow.v0, arrow.u1, arrow.v1,
		              (static_cast<uint32_t>(bubble.arrowAlpha) << 24) | 0x00FFFF00u);
	}
	const ScreenInWorld atBubble {.toWorld = &toWorld, .depth = bubble.depth, .zFunc = render_modes::ZFunc::Always};
	const uint64_t untested = render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
	                                              {.zFunc = render_modes::ZFunc::Always, .writeAlpha = true});
	constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
	constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();
	SubmitSpiritQuads(view, PlaceInWorld(std::move(arrows), atBubble), k_Atmos, k_AtmosAlpha, untested);
	// the $g gestures: a mode 6 material of S_Gesture<page>.raw with its alpha, tested against the world's depth
	constexpr std::array<std::pair<entt::id_type, entt::id_type>, 2> k_Gestures = {{
	    {entt::hashed_string("raw/S_Gesture0").value(), entt::hashed_string("raw/S_Gesture0a").value()},
	    {entt::hashed_string("raw/S_Gesture1").value(), entt::hashed_string("raw/S_Gesture1a").value()},
	}};
	for (size_t page = 0; page < k_Gestures.size(); ++page)
	{
		SubmitSpiritQuads(view, placed(bubble.gestures.at(page)), k_Gestures.at(page).first, k_Gestures.at(page).second,
		                  tested);
	}
	// the $m icons: DrawKeyOrMouse through DrawKeyOrMouseIn (the frame's hand and ticks), its 2D draws with no depth
	// test, at the bubble's depth in the world view
	_keyOrMouseInWorld = atBubble;
	for (const auto& icon : bubble.keyIcons)
	{
		const uint32_t c1 = (static_cast<uint32_t>(icon.alpha) << 24) | 0x00FFFFFFu;
		const KeyOrMouseRow row = icon.animType == -1 && icon.clickType == 0 ? KeyOrMouseRow(std::u16string_view(icon.keyName))
		                                                                     : KeyOrMouseRow(icon.row);
		DrawKeyOrMouseIn(view, overlay.inputPrompts, icon.animType, icon.clickType, row, std::nullopt, icon.x, icon.y,
		                 icon.size, 1, &c1, nullptr, icon.alpha);
	}
	_keyOrMouseInWorld.reset();
	// the lines (their shadow and text), each at its depth and tested against the world's, in their order
	if (const GameFont* font = GameFontAt(help::TextFont::J0); font != nullptr)
	{
		for (const auto& line : bubble.lines)
		{
			const auto& run = line.run;
			std::vector<GameFont::Vertex> glyphs;
			font->AddText(glyphs, run.text, run.x, run.y, run.size, glm::vec4(run.r, run.g, run.b, run.a) / 255.0f, run.clipTop,
			              run.clipBottom);
			// the text draw's two colours: the alpha from the line's top edge to its bottom edge
			for (auto& glyph : glyphs)
			{
				const float t = std::clamp((glyph.y - run.y) / run.size, 0.0f, 1.0f);
				const auto alphaByte = static_cast<uint32_t>(
				    static_cast<float>(run.a) + (static_cast<float>(line.alphaBottom) - static_cast<float>(run.a)) * t);
				glyph.abgr = (glyph.abgr & 0x00FFFFFFu) | (std::min(alphaByte, 255u) << 24);
			}
			SubmitScreenText(*font, glyphs, view,
			                 ScreenInWorld {.toWorld = &toWorld, .depth = line.depth, .zFunc = render_modes::ZFunc::LessEqual});
		}
	}
}

void Renderer::DrawHandTricons(const OverlayFrame& overlay) const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	std::vector<SpiritQuadVertex> quads;
	for (const auto& sprite : overlay.handTricons)
	{
		if (!sprite.has_value())
		{
			continue;
		}
		// white at the icon's alpha; the corners in pixels from the top left, to clip space
		const uint32_t argb = static_cast<uint32_t>(std::lround(sprite->alpha * 255.0f)) << 24 | 0xFFFFFFu;
		const uint32_t abgr = argb_colour::ToAbgr(argb);
		std::array<SpiritQuadVertex, 4> corners {};
		for (size_t i = 0; i < corners.size(); ++i)
		{
			const auto& at = sprite->corners.at(i);
			const auto& uv = sprite->uvs.at(i);
			corners.at(i) = {2.0f * at.x / _resolution.x - 1.0f, 1.0f - 2.0f * at.y / _resolution.y, 0.5f, uv.x, uv.y, abgr};
		}
		for (const size_t i : {0, 1, 2, 0, 2, 3})
		{
			quads.push_back(corners.at(i));
		}
	}
	if (quads.empty())
	{
		DrawHandDemoMouse(overlay);
		return;
	}
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay), glm::value_ptr(identity),
	                       glm::value_ptr(identity));
	SubmitSpiritQuads(graphics::RenderPass::ScreenOverlay, quads, k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha,
	                  render_modes::State(render_modes::materials::k_Atmos, {.zFunc = render_modes::ZFunc::Always}));
	DrawHandDemoMouse(overlay);
}

void Renderer::DrawTempleScrollIcons(const OverlayFrame& overlay) const
{
	// The game's key and mouse draw with an empty text, aligned as its first kind, white at full alpha and no panels
	constexpr uint32_t k_White = 0xFFFFFFFFu;
	for (const auto& icon : overlay.templeScrollIcons)
	{
		const KeyOrMouseRow row = icon.animType == -1 && icon.clickType == 0 ? KeyOrMouseRow(std::u16string_view(icon.keyName))
		                                                                     : KeyOrMouseRow(icon.row);
		DrawKeyOrMouseIn(graphics::RenderPass::FinishFrameIcons, overlay.inputPrompts, icon.animType, icon.clickType, row,
		                 std::u16string_view(u""), icon.x, icon.y, icon.size, 1, &k_White, nullptr, 0);
	}
}

void Renderer::DrawHandDemoMouse(const OverlayFrame& overlay) const
{
	if (!overlay.handDemoMouse.has_value())
	{
		return;
	}
	// the mouse and the word in red, its glow white, at full alpha
	constexpr uint32_t k_Red = 0xFFFF0000u;
	constexpr uint32_t k_White = 0xFFFFFFFFu;
	const auto& mouse = *overlay.handDemoMouse;
	DrawKeyOrMouseIn(graphics::RenderPass::ScreenOverlay, overlay.inputPrompts, mouse.animType, mouse.clickType, mouse.row,
	                 std::u16string_view(mouse.text), mouse.at.x, mouse.at.y, hand_tricons::k_DemoMouseSize, mouse.align,
	                 &k_Red, &k_White, 0xFF);
}

void Renderer::DrawFloatingNumbers(const OverlayFrame& overlay) const
{
	const GameFont* font = GameFontAt(help::TextFont::J0);
	if (font == nullptr || overlay.floatingNumbers.empty())
	{
		return;
	}
	// the text draw's height clip: from the top of the screen down to 4096
	constexpr float k_ClipTop = 0.0f;
	constexpr float k_ClipBottom = 4096.0f;
	constexpr float k_Size = ecs::floating_number::k_TextSize;
	constexpr float k_Shadow = ecs::floating_number::k_ShadowOffset;
	std::vector<GameFont::Vertex> glyphs;
	for (const auto& number : overlay.floatingNumbers)
	{
		const auto at = ecs::floating_number::TextPlace(number.screen, font->GetStringWidth(number.text, k_Size));
		const float alpha = static_cast<float>(number.alpha) / 255.0f;
		font->AddText(glyphs, number.text, at.x - k_Shadow, at.y + k_Shadow, k_Size, glm::vec4(0.0f, 0.0f, 0.0f, alpha),
		              k_ClipTop, k_ClipBottom);
		const glm::vec4 colour {static_cast<float>((number.colour >> 16u) & 0xFFu) / 255.0f,
		                        static_cast<float>((number.colour >> 8u) & 0xFFu) / 255.0f,
		                        static_cast<float>(number.colour & 0xFFu) / 255.0f, alpha};
		font->AddText(glyphs, number.text, at.x, at.y, k_Size, colour, k_ClipTop, k_ClipBottom);
	}
	SubmitScreenText(*font, glyphs, graphics::RenderPass::FinishFrameIcons);
}

void Renderer::DrawHelpText(const OverlayFrame& overlay) const
{
	// HelpText's draw (not while a film is open: FillOverlayFrame's gate) with HelpTextDisplay::Layout's frame, laid out before
	// the draw at the same resolution (FillOverlayFrame in Game.cpp)
	const auto& helpText = overlay.helpText;
	if (!helpText.active || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const auto& frame = helpText.text;

	// the box, {b, g, r of the box colour, a 0x80} in the 2D rectangle queue (flushed before this callback runs)
	if (frame.boxShown)
	{
		std::vector<ScreenRectVertex> box;
		AddScreenRect(box, frame.box.left, frame.box.top, frame.box.right, frame.box.bottom,
		              static_cast<uint32_t>(frame.boxAlpha) << 24);
		SubmitScreenRects(box);
	}
	// the dialogue texts: one text draw per word, here one submit per font for the words between two boxes
	const auto drawRuns = [this, &frame](size_t first, size_t last) {
		for (const auto font : {help::TextFont::J0, help::TextFont::F1, help::TextFont::F3})
		{
			const GameFont* gameFont = GameFontAt(font);
			if (gameFont == nullptr)
			{
				continue;
			}
			std::vector<GameFont::Vertex> glyphs;
			for (size_t i = first; i < last; ++i)
			{
				const auto& run = frame.runs[i];
				if (run.font == font)
				{
					gameFont->AddText(glyphs, run.text, run.x, run.y, run.size, glm::vec4(run.r, run.g, run.b, run.a) / 255.0f,
					                  run.clipTop, run.clipBottom);
				}
			}
			SubmitScreenText(*gameFont, glyphs);
		}
	};
	// the song's box (in the word's colour at half its alpha), the help's glow panel, right before the sung word: the words
	// before it are under it, the sung word and those after over it
	const auto sung = std::ranges::find(frame.runs, true, &help::TextRun::sung);
	if (!helpText.songBox.has_value() || sung == frame.runs.end())
	{
		drawRuns(0, frame.runs.size());
		return;
	}
	const auto at = static_cast<size_t>(sung - frame.runs.begin());
	drawRuns(0, at);
	const auto& box = *helpText.songBox;
	const uint32_t argb = static_cast<uint32_t>(box.rgba[3]) << 24 | static_cast<uint32_t>(box.rgba[0]) << 16 |
	                      static_cast<uint32_t>(box.rgba[1]) << 8 | static_cast<uint32_t>(box.rgba[2]);
	std::vector<SpiritQuadVertex> panel;
	AddKeyOrMousePanel(panel, box.left, box.top, box.right, box.bottom, argb);
	SubmitSpiritQuads(graphics::RenderPass::ScreenOverlay, panel, k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha,
	                  render_modes::State(render_modes::materials::k_AtmosAdditive, {.zFunc = render_modes::ZFunc::Always}));
	drawRuns(at, frame.runs.size());
}

namespace
{
// The renderer draws the PSys effects by their draw path (manager::CollectSorted / CollectQueued / HandEffects and
// RenderContext::particleAtoms): with k_DrawByPath false the mesh atoms would be drawn by the old loops as well
static_assert(particles::manager::k_DrawByPath, "Renderer::DrawPass draws the PSys effects by path");
} // namespace

void Renderer::DrawPass(const DrawSceneDesc& desc) const
{
	// Inside the temple the pass draws the temple in place of the world (RendererTemple.cpp)
	if (InTemple())
	{
		DrawTemplePass(desc);
		return;
	}
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

	// Distance haze of this frame (PreDraw)
	const glm::vec4 u_haze = _hazeUniforms[0];
	const glm::vec4 u_hazeColour = _hazeUniforms[1];

	// the frame's single queue of everything blended, begun at the start of the frame, filled while the main view is
	// drawn and drained once, far to near, after all of it (at the end of the frame)
	zsort::Queue<ZObject> sorted;
	sorted.Begin();

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSky
		                                                                          : Profiler::Stage::MainPassDrawSky);
		if (desc.drawSky)
		{
			const auto modelMatrix = glm::mat4(1.0f);
			// the sky type is blended into the dome textures by SkySystem::UpdateDome (sky_type::DomeBlend)
			SetSkyUniforms(*skyShader);

			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = skyShader;
			// (inferred) the sky meshes' own modes, culled as a whole (flipped by the mirrored camera)
			submitDesc.options = {.cull = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Sky, false, false),
			                      .writeAlpha = true,
			                      .msaa = true};
			submitDesc.modelMatrices = &modelMatrix;
			submitDesc.matrixCount = 1;
			submitDesc.isSky = true;

			DrawMesh(Locator::skySystem::value().GetMesh(), submitDesc, 0);
			if (desc.viewId == graphics::RenderPass::Main)
			{
				// after the dome, the sun and then the moon, as the original draws its sky
				DrawSun(desc.viewId, *desc.camera, false, desc.clock.frameGameMs);
				DrawMoon(desc.viewId, *desc.camera);
				// each cloud is queued with the blended things of the frame (it used to be sorted on its own and drawn after
				// all of them)
				for (const auto& [key, index] : _preClouds) // collected by PreDraw
				{
					sorted.Submit({.cloud = static_cast<int>(index)}, key);
				}
			}
			else if (desc.viewId == graphics::RenderPass::Reflection)
			{
				// the moon is reflected (its halo and DrawUnderWater; mirrored: sea_pass::ForPass), the sun is not
				// (it is drawn once)
				DrawMoon(desc.viewId, *desc.camera);
			}
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawWater
		                                                                          : Profiler::Stage::MainPassDrawWater);
		if (desc.drawWater)
		{
			// RendererSea.cpp
			DrawSea(desc);
		}
	}

	// the landscape vortices' depth boxes, then their ground effects' surfaces, before the land of the main view
	// (RendererVortex.cpp)
	if (desc.drawIsland && desc.viewId == graphics::RenderPass::Main)
	{
		DrawVortexBoxes(desc.viewId);
		if (desc.drawEntities)
		{
			DrawVortexGroundEffects(desc.viewId);
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawIsland
		                                                                          : Profiler::Stage::MainPassDrawIsland);
		if (desc.drawIsland)
		{
			auto& island = Locator::terrainSystem::value();
			auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);

			// Small bump fade line: the plane perpendicular to the camera forward, 50 units ahead, meets
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
			terrainShader->SetTextureSampler("s_landLightTable", 4, fromBgfx(_landLightTexture.Get())); // vs
			const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
			if (_landCellsTexture.IsValid() && glm::vec2(_landCellsSize) == cellMapSize)
			{
				terrainShader->SetTextureSampler("s_landCells", 6, fromBgfx(_landCellsTexture.Get())); // vs
			}
			else
			{
				terrainShader->SetTextureSampler("s_landCells", 6, island.GetCellMap()); // vs
			}
			const glm::vec4 u_cellMap = {island.GetExtent().minimum, cellMapSize};
			terrainShader->SetUniformValue("u_cellMap", &u_cellMap); // vs
			// x: 1 = the colour comes from the block texture (the original's, BlockTexture.h); 0 = the per-vertex materials
			// (an island without a block texture)
			glm::vec4 u_blockTexture {0.0f};
			if (const auto* blockTexture = island.GetBlockTexture(); blockTexture != nullptr)
			{
				terrainShader->SetTextureSampler("s10_blockTexture", 10, *blockTexture);
				u_blockTexture.x = 1.0f;
				u_blockTexture.y = 1.0f; // its alpha is the coast alpha
			}
			terrainShader->SetUniformValue("u_blockTexture", &u_blockTexture);
			// The texture detail line (the draw list's, rebuilt this frame): beyond it a block shows its low-resolution
			// tile, in its band its texture with the tile over it by the vertices' alpha
			const auto* lowResTexture = island.GetLowResTexture();
			const auto textureLine = Locator::objectDrawListSystem::has_value()
			                             ? Locator::objectDrawListSystem::value().TextureLine()
			                             : ecs::draw_list::k_StartTextureLine;
			if (lowResTexture != nullptr)
			{
				terrainShader->SetTextureSampler("s9_lowResPages", 9, *lowResTexture);
			}
			const glm::vec4 u_textureLine = {textureLine.point.x, textureLine.point.z, textureLine.along.x,
			                                 textureLine.along.z};
			terrainShader->SetUniformValue("u_textureLine", &u_textureLine); // vs
			const glm::vec4 u_textureLineBand = {textureLine.halfWidth, ecs::draw_list::TileAlphaScale(textureLine), 0.0f,
			                                     0.0f};
			terrainShader->SetUniformValue("u_textureLineBand", &u_textureLineBand); // vs
			terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
			terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
			terrainShader->SetUniformValue("u_haze", &u_haze);
			terrainShader->SetUniformValue("u_hazeColour", &u_hazeColour);
			// Static shadows darken the block texture by up to x0.5 (x0.75 with 128 px textures)
			const float staticShadowStrength =
			    detail_level::Get(Locator::config::value().detailLevel).useHighTexture ? 0.5f : 0.25f;
			// x: the light table >> 1 of the mirrored land (sea_pass::SeaPassState::landLightScale)
			const glm::vec4 u_terrainPass = {sea_pass::ForPass(desc.viewId).landLightScale, 0.0f, staticShadowStrength, 0.0f};
			terrainShader->SetUniformValue("u_terrainPass", &u_terrainPass);
			terrainShader->SetUniformValue("u_islandExtent", &islandExtent);

			// clang-format off
			// fs_terrain writes the land and small bump passes premultiplied (the coast alpha does not fade the small
			// bump, as in the original); Z is written even where the land is transparent (render mode 14)
			// (the land blocks' material: the function of mode 5)
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

			// The mirrored land is drawn with ZWRITEENABLE off, so it
			// hides nothing drawn after it under the sea (the hand's, the objects' and the boats' reflections, the fish);
			// here it wrote Z, and a mirrored hill hid the reflections behind it.
			const auto seaPass = sea_pass::ForPass(desc.viewId);
			const auto state = seaPass.LandState(defaultState);
			// openblack's blocks wind the other way round from the models (sea_pass::Surface::Land)
			const uint64_t landCull = seaPass.FaceCull(sea_pass::Surface::Land, false, false) == render_modes::Cull::Ccw
			                              ? BGFX_STATE_CULL_CCW
			                              : BGFX_STATE_CULL_CW;
			// the vortices' decals on their blocks, in the main land (RendererVortex.cpp): such a block is drawn with an
			// equal depth test over its decal's depth mask
			const auto vortexDecals =
			    desc.viewId == graphics::RenderPass::Main ? CollectVortexDecals() : std::vector<VortexDecalDraw> {};
			const auto decalState = (state & ~BGFX_STATE_DEPTH_TEST_MASK) | BGFX_STATE_DEPTH_TEST_EQUAL;

			// Block order of both land passes: the list the island builds before drawing, ascending by the distance from the
			// camera to the block centre (x + 80, 0, z + 80) with LandRef on: nearest first. Without Z, it is what decides
			// which mirrored hill covers which.
			const auto& blocks = island.GetBlocks();
			const auto& viewOrigin = desc.camera->GetOrigin();
			std::vector<std::pair<float, size_t>> blockOrder;
			blockOrder.reserve(blocks.size());
			for (size_t i = 0; i < blocks.size(); ++i)
			{
				const glm::vec2 centre = blocks[i].GetMapPosition() + glm::vec2(80.0f);
				blockOrder.emplace_back(glm::length(glm::vec3(centre.x - viewOrigin.x, viewOrigin.y, centre.y - viewOrigin.z)),
				                        i);
			}
			std::stable_sort(blockOrder.begin(), blockOrder.end(),
			                 [](const auto& a, const auto& b) { return a.first < b.first; });

			// the block's haze class from its box (graphics::haze::BlockClassOf), with the LandRef detail (inferred)
			const auto view = desc.camera->GetViewMatrix(Camera::Interpolation::Current);
			const bool landRef = detail_level::Get(Locator::config::value().detailLevel).landReflection;
			for (const auto& [distance, blockIndex] : blockOrder)
			{
				const auto& block = blocks[blockIndex];
				// pack uniforms
				const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
				terrainShader->SetUniformValue("u_blockPositionAndSize", &mapPositionAndSize);
				const glm::vec4 u_hazeBlock = {static_cast<float>(haze::BlockClassOf(_haze, view, block, landRef)), 0.0f, 0.0f,
				                               0.0f};
				terrainShader->SetUniformValue("u_hazeBlock", &u_hazeBlock);
				// x: what the block shows (draw_list::TextureLevel: 0 its texture, 1 its tile only, 2 both), y: its tile's
				// page, zw: the tile's first texel plus 1/255 (a page UV), as the original moves it in by one texel
				glm::vec4 u_blockTile {0.0f};
				if (const auto tile = island.GetBlockTile(blockIndex); tile.has_value() && lowResTexture != nullptr)
				{
					const auto level = island.TilesOnly()
					                       ? ecs::draw_list::TextureLevel::TileOnly
					                       : ecs::draw_list::BlockTextureLevel(block.GetMapPosition(), textureLine, false);
					constexpr float k_OneTexel = 1.0f / 255.0f;
					u_blockTile = {static_cast<float>(level), static_cast<float>(tile->layer), tile->origin.x + k_OneTexel,
					               tile->origin.y + k_OneTexel};
				}
				terrainShader->SetUniformValue("u_blockTile", &u_blockTile);
				const glm::vec4 u_blockTileDebug {island.ShowTextureLevels() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
				terrainShader->SetUniformValue("u_blockTileDebug", &u_blockTileDebug);

				const auto decal = std::ranges::find(vortexDecals, block.GetBlockPosition(), &VortexDecalDraw::block);
				const bool decalHere = decal != vortexDecals.end();
				if (decalHere)
				{
					DrawVortexDecalMask(desc.viewId, *decal, block, landCull);
				}

				block.GetMesh().GetVertexBuffer().Bind();

				bgfx::setState((decalHere ? decalState : state) | landCull, 0);
				terrainShader->Submit(static_cast<bgfx::ViewId>(desc.viewId), 0, discard);
				if (decalHere)
				{
					DrawVortexDecalRing(desc.viewId, *decal, block, landCull);
				}
				// the shadows over the block just drawn, only in the main land (the mirrored one draws none)
				if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities)
				{
					DrawLandShadows(desc.viewId, block, landCull);
				}
			}
			_shaderManager->DiscardBindings();
		}
	}

	bool mistsSorted = false; ///< the mists went through the back-to-front list of the blended models
	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawModels
		                                                                          : Profiler::Stage::MainPassDrawModels);
		// The original's "underwater" stage: the hand mirrored in the sea, unlit in colour 0x65A0A0A0 with no specular, only
		// its part above the water (the hand's DrawUnderWater with its own bones; sea_pass::UnderWater). Around it one of
		// the hand object's flags is cleared and put back afterwards: (not ported) what that flag does in this draw is not
		// identified (the under-water draw has no test of it)
		if (!desc.drawEntities && desc.viewId == graphics::RenderPass::Reflection)
		{
			// the missionaries' boat first, as its own draw mirrors its hull before the land draws anything under the sea
			DrawBoatReflection(desc.viewId);
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
			const auto handDesc = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			const auto* bones = Locator::handSystem::has_value() ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
			// A hidden hand is not drawn, so it has no instances
			bool handReflected = false;
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
					handReflected = handDesc->second.count > 0;
				}
			}
			DrawObjectReflections(desc.viewId, handReflected);
			// then the local player's creature
			if (Locator::config::value().drawCreatureReflection)
			{
				DrawCreatureReflection(desc.viewId);
			}
		}
		if (desc.viewId == graphics::RenderPass::Reflection)
		{
			// The parts under the water go into the frame before the sea, over the mirrored
			// land. Here that frame is the reflection target, drawn with the mirrored camera, so they are mirrored too.
			// the sharks (and whatever else is cut by the plane), then the fish (4d-4e)
			// the sharks
			DrawCutBelowWater(desc.viewId);
			// each shoal of a bait, then its net under the net's own plane (sea_pass::k_NetPlane, put back afterwards)
			DrawFishShoals(desc.viewId);
			DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_NetPlane));
			// the swimming SuperVillagers, after the fish and the nets and before the hand's glow: each SuperVillager whose
			// animation is "M_P_Swim2" (the frame's SuperVillagerFrame::swimmers, filled before the draw), after its own draw
			// and shadow: the plane sea_pass::k_SwimPlane, colour k_SwimmerColour / k_SwimmerSpecular, DrawCutByPlane, then the
			// default plane again. The swim rings every 1000 ms are made by ecs::super_villager::Update (the water ring system)
			for (const auto swimmer : desc.superVillagers.swimmers)
			{
				DrawCutByPlane(desc.viewId, swimmer, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_SwimPlane),
				               sea_pass::k_SwimmerColour, sea_pass::k_SwimmerSpecular);
			}
			// the hand's glow on the water, the last thing before the sea
			DrawHandWaterGlow(desc.viewId);
		}
		// The screen-facing sprite (billboard::Screen) on the GPU: vs_sprite adds u_invView x (model x (x, y, 0,
		// 0)) to the model's translation on the plane -1..1 with v = 0 at the top, so the half width / half height are
		// the Transform's scale x / y. components::Sprite has no angle nor origin (every user, NightLights, FireFlies,
		// Dust, HandEffects, Glow, CameraBookmark, HandDebugHooks, gives an identity rotation), so it is Screen with angle
		// 0 and ox = oy = 0. The sprite's near test is made here on the CPU
		const auto spriteFrame = billboard::CameraFrame::From(*desc.camera);
		const auto drawSpriteWith = [this, &spriteShader, &spriteFrame](const ecs::components::Sprite& sprite,
		                                                                TextureHandle texture, TextureHandle alpha,
		                                                                const ecs::components::Transform& transform,
		                                                                RenderPass viewId, bool premultipliedAdditive) {
			if (!billboard::InFrontOfNear(transform.position, spriteFrame))
			{
				return;
			}
			const glm::mat4 modelMatrix = billboard::ScreenSpriteModel(transform.position, glm::vec2(transform.scale), 0.0f);

			glm::vec4 u_sampleRect(sprite.uvExtent, sprite.uvMin);

			bgfx::setTransform(glm::value_ptr(modelMatrix));
			spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
			spriteShader->SetUniformValue("u_tint", glm::value_ptr(sprite.tint));
			spriteShader->SetTextureSampler("s_diffuse", 0, texture);
			// a texture of colours with its alpha file (the fireflies'), or a single channel of alpha
			const glm::vec4 u_spriteAlpha(sprite.alpha.has_value() ? 1.0f : 0.0f, sprite.additive ? 1.0f : 0.0f, 0.0f, 0.0f);
			spriteShader->SetUniformValue("u_spriteAlpha", glm::value_ptr(u_spriteAlpha));
			spriteShader->SetTextureSampler("s_alpha", 1, alpha);

			_plane->GetVertexBuffer().Bind();

			// mode 13, or mode 6 with the tint premultiplied (fs_sprite) (inferred: the materials of the sprites' owners)
			auto state = render_modes::State(sprite.additive ? render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite
			                                                 : render_modes::Mode::AlphaTexturedAlphaNoZWrite,
			                                 {.writeAlpha = true, .premultiplied = true});
			// an added sprite whose tint is premultiplied (a mote on its white sheet) is added as it is: the colour times
			// its alpha and the sheet's alpha once, as mode 13 adds it. render_modes::State is left as it is: its additive
			// blend ignores `premultiplied` (it always adds by the source alpha), and the other additive sprites go
			// through it, so this override belongs to the dust alone
			if (sprite.additive && premultipliedAdditive)
			{
				state = (state & ~BGFX_STATE_BLEND_MASK) | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE);
			}
			bgfx::setState(state);

			spriteShader->Submit(static_cast<bgfx::ViewId>(viewId));
		};
		// The sprites name their textures by their ids in the cache; one the cache does not have is not drawn
		const auto& spriteTextures = Locator::resources::value().GetTextures();
		const auto& markRegistry = Locator::entitiesRegistry::value();
		const auto drawSprite = [&drawSpriteWith, &spriteTextures,
		                         &markRegistry](entt::entity entity, const ecs::components::Sprite& sprite,
		                                        const ecs::components::Transform& transform, RenderPass viewId) {
			const auto alphaId = sprite_textures::AlphaId(sprite);
			if (!spriteTextures.Contains(sprite.texture) || !spriteTextures.Contains(alphaId))
			{
				return;
			}
			// a disciple's mark at its villager's drawing pose (smooth between turns); its Transform keeps the game's
			auto drawn = transform;
			if (const auto* mark = markRegistry.TryGet<const ecs::components::DiscipleMark>(entity);
			    mark != nullptr && mark->drawPosition.has_value())
			{
				drawn.position = *mark->drawPosition;
			}
			drawSpriteWith(sprite, spriteTextures.Handle(sprite.texture)->GetNativeHandle(),
			               spriteTextures.Handle(alphaId)->GetNativeHandle(), drawn, viewId, false);
		};
		// the physics' dust puffs: a list taken from the game (DynamicsSystemInterface::SnapshotDust), each drawn as the
		// components::Sprite + Transform it was (identity rotation, the half size in x / y, normal blending; a creature's
		// motes added)
		Locator::dynamicsSystem::value().SnapshotDust(_frameDust);
		const auto drawDust = [&drawSpriteWith](const ecs::physics::DustParticleDraw& puff, RenderPass viewId) {
			// the puff keeps its own texture's handle, which is also its alpha
			const ecs::components::Sprite sprite {
			    .uvMin = puff.uvMin, .uvExtent = puff.uvExtent, .tint = puff.tint, .additive = puff.additive};
			const ecs::components::Transform transform {puff.position, glm::mat3(1.0f), glm::vec3(puff.halfSize)};
			drawSpriteWith(sprite, puff.texture, puff.texture, transform, viewId, true);
		};
		// the sprites also go to the Z-sorter: in the main pass they are sorted with the blended models
		const bool spritesSorted = desc.drawEntities && desc.drawSprites && desc.viewId == graphics::RenderPass::Main;
		// every mist on screen goes to the same Z-sorter (key |pos - camera|^2)
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
			// (RendererShadows.cpp: the tail loop of the objects' Draw)
			CollectShadowReceivers(desc.viewId == graphics::RenderPass::Main);
			// the knocked town's read-out: each building's row is drawn at once at the end of the building's own draw,
			// whether the building is drawn at once or queued (OverlayFrame::knockRows), by the instance it is drawn with;
			// the main view only
			std::unordered_map<uint32_t, size_t> knockRowOfInstance;
			const bool knockRows = desc.viewId == graphics::RenderPass::Main;
			std::vector<bool> knockRowDrawn(knockRows ? desc.overlay.knockRows.size() : 0, false);
			if (knockRows)
			{
				for (size_t k = 0; k < desc.overlay.knockRows.size(); ++k)
				{
					const auto found = renderCtx.entityInstances.find(desc.overlay.knockRows[k].building);
					if (found != renderCtx.entityInstances.end())
					{
						knockRowOfInstance.emplace(found->second.index, k);
					}
				}
			}
			const auto drawKnockRowAt = [&](size_t k) {
				if (knockRowDrawn[k])
				{
					return;
				}
				knockRowDrawn[k] = true;
				const auto& row = desc.overlay.knockRows[k];
				DrawKnockRow(desc.viewId, std::span(desc.overlay.knockReadout).subspan(row.first, row.count));
			};
			// the shells over the citadel hearts being destroyed, by their hearts' instances; the main view only
			_frameHeartShells.clear();
			if (desc.viewId == graphics::RenderPass::Main)
			{
				CollectHeartShells(desc);
			}
			const auto drawKnockRow = [&](uint32_t instance) {
				if (const auto found = knockRowOfInstance.find(instance); found != knockRowOfInstance.end())
				{
					drawKnockRowAt(found->second);
				}
			};

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
			// An object (static, animated or morphing) is queued only by its Z-sort flag: the whole object goes to the
			// Z-sorter, else it is drawn at once, blended primitives and all. The flag comes from the mesh's flag 0x200
			// (L3DMesh::IsZSorted); the one-shot orb forces it. The global alpha's flag is not looked at here (only one other
			// object class queues by it), so a fading object whose mesh lacks 0x200 is drawn at once through the alternate
			// mode table. (inferred) that every model instance is drawn this way and none is of that other class; in the main
			// view only, the queue's (the reflection draws everything at once)
			const bool sortBlended = desc.viewId == graphics::RenderPass::Main;
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto opaqueOptions = submitDesc.options;

			// the poses of the animated boned meshes (ecs/Animations.h), by instance; the PSys mesh atoms' too
			const auto poses = ecs::PosesByInstance(renderCtx);
			// the sharks (components::CutByPlane::ownerDraws): their owner draws them cut by the water instead
			const auto cutAbove =
			    desc.viewId == graphics::RenderPass::Main ? CutAboveInstances() : std::unordered_set<uint32_t>();
			// of those, the ones whose part above the water is drawn this frame (a shark walking a path)
			const auto cutDrawn =
			    desc.viewId == graphics::RenderPass::Main ? CutAboveInstances(true) : std::unordered_set<uint32_t>();

			// The rows the object draw list leaves out of this frame are skipped (BuildObjectListRows): 0 in the mask, and
			// every row past its end drawn. While the opaque draw is cut into segments (object_order, below), a segment's
			std::span<const uint8_t> rowMask {_objectListRows};
			const auto rowDrawn = [&rowMask](uint32_t row) { return row >= rowMask.size() || rowMask[row] != 0; };
			// The citadel hearts are drawn in a pass of their own, before the water rings and the models (below): each
			// heart with a shell shown over it or a row the object draw list draws. Their rows leave the models' draw
			struct FrameHeart
			{
				const HeartShellDraw* shell {nullptr};
				std::optional<uint32_t> row;
			};
			std::vector<FrameHeart> frameHearts;
			std::vector<heart_shell::PassHeart> passHearts;
			std::vector<uint8_t> withoutHearts;
			if (desc.viewId == graphics::RenderPass::Main && Locator::entitiesRegistry::has_value())
			{
				std::as_const(Locator::entitiesRegistry::value())
				    .Each<const ecs::components::Temple>([&](entt::entity heart, const ecs::components::Temple& temple) {
					    FrameHeart frameHeart;
					    if (const auto shell = std::ranges::find(_frameHeartShells, heart, &HeartShellDraw::heart);
					        shell != _frameHeartShells.end())
					    {
						    frameHeart.shell = &*shell;
					    }
					    if (const auto placed = renderCtx.entityInstances.find(heart);
					        placed != renderCtx.entityInstances.end() &&
					        placed->second.index < renderCtx.instanceUniforms.size() && rowDrawn(placed->second.index))
					    {
						    frameHeart.row = placed->second.index;
					    }
					    if (frameHeart.shell == nullptr && !frameHeart.row.has_value())
					    {
						    return;
					    }
					    frameHearts.push_back(frameHeart);
					    passHearts.push_back({.creationStamp = temple.creationStamp,
					                          .shell = frameHeart.shell != nullptr,
					                          .model = frameHeart.row.has_value()});
				    });
				if (std::ranges::any_of(frameHearts, [](const FrameHeart& heart) { return heart.row.has_value(); }))
				{
					withoutHearts.resize(renderCtx.instanceUniforms.size());
					for (uint32_t row = 0; row < withoutHearts.size(); ++row)
					{
						withoutHearts[row] = rowDrawn(row) ? 1 : 0;
					}
					for (const auto& heart : frameHearts)
					{
						if (heart.row.has_value())
						{
							withoutHearts[*heart.row] = 0;
						}
					}
					rowMask = withoutHearts;
				}
			}
			// a row's culling box: its own (std) mesh's where a finer one is drawn (components::DetailMesh), as the
			// original culls by the object's own mesh
			const auto ownBox = [&renderCtx, &meshManager](uint32_t row, entt::id_type drawn,
			                                               const AxisAlignedBoundingBox& box) {
				const auto own = row < renderCtx.rowOwnMesh.size() ? renderCtx.rowOwnMesh[row] : entt::id_type {0};
				return own == 0 || own == drawn || !meshManager.Contains(own) ? box : meshManager.Handle(own)->GetBoundingBox();
			};

			// The posed villagers and animals of a mesh are drawn in one instanced draw, each instance's bones read from the
			// frame's bone palette (graphics::bone_palette), wherever that leaves the same pixels as drawing them one by
			// one below: a mesh drawn at once (not queued), neither the hand nor a creature, not morphing with the land,
			// every primitive of which draws in any order (bone_palette::DrawsInAnyOrder: tested against and writing the
			// depth, not blended), and none of whose instances in view gets a shadow over it or a read-out row, which go
			// right after their object's own draw. They are the same instances, culled the same way, with the same bones
			// (their pose, else the mesh's), and the same rows but for the first bone in a w no shader reads otherwise.
			// The main view only, the one view that draws the objects: the palette is uploaded once a frame. A mesh with
			// fewer than two instances in view is drawn as before, and everything is without RGBA32F vertex fetch.
			const auto planPaletteDraws = [&]() {
				_paletteDraws.clear();
				_paletteTexels.clear();
				static const bool s_vertexFetch =
				    (bgfx::getCaps()->formats[bgfx::TextureFormat::RGBA32F] & BGFX_CAPS_FORMAT_TEXTURE_VERTEX) != 0;
				if (!s_vertexFetch || desc.viewId != graphics::RenderPass::Main)
				{
					return;
				}
				constexpr uint16_t k_Stride = sizeof(glm::mat4) + sizeof(glm::vec4);
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				std::vector<uint32_t> rows;
				for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
				{
					if (meshId == ecs::components::Hand::k_MeshId || placers.morphWithTerrain || !meshManager.Contains(meshId))
					{
						continue;
					}
					const auto mesh = meshManager.Handle(meshId);
					if (!mesh->IsBoned() || (sortBlended && mesh->IsZSorted()) ||
					    !ecs::HasPose(poses, placers.offset, placers.count) || HasCreature(placers.offset, placers.count) ||
					    !DrawnInAnyOrder(*mesh, submitDesc))
					{
						continue;
					}
					const auto box = mesh->GetBoundingBox();
					rows.clear();
					bool oneByOne = false;
					for (uint32_t i = 0; i < placers.count && !oneByOne; ++i)
					{
						const auto row = placers.offset + i;
						if (cutAbove.contains(row) || !rowDrawn(row) ||
						    !BoxInView(viewProjection, ownBox(row, meshId, box), renderCtx.instanceUniforms[row]))
						{
							continue;
						}
						oneByOne = _shadowReceivers.contains(row) || knockRowOfInstance.contains(row);
						rows.push_back(row);
					}
					const auto count = static_cast<uint32_t>(rows.size());
					if (oneByOne || count < 2 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
					{
						continue;
					}
					bgfx::InstanceDataBuffer instances;
					bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
					for (uint32_t i = 0; i < count; ++i)
					{
						const auto row = rows[i];
						const glm::mat4* bones = mesh->GetBoneMatrices().data();
						auto boneCount = static_cast<uint8_t>(mesh->GetBoneMatrices().size());
						ecs::UsePose(poses, row, *mesh, bones, boneCount);
						auto model = renderCtx.instanceUniforms[row];
						bone_palette::SetFirstBone(model, bone_palette::Append(_paletteTexels, std::span(bones, boneCount)));
						std::memcpy(instances.data + (i * k_Stride), &model, sizeof(glm::mat4));
						std::memcpy(instances.data + (i * k_Stride) + sizeof(glm::mat4), &renderCtx.instanceColours[row],
						            sizeof(glm::vec4));
					}
					_paletteDraws.emplace(meshId, instances);
				}
				if (_paletteDraws.empty())
				{
					return;
				}
				// a texture as tall as the bones need, grown by doubling; every mesh drawn one by one if it can't be
				const auto rowsNeeded = bone_palette::RowsFor(_paletteTexels.size());
				const uint32_t maxRows = bgfx::getCaps()->limits.maxTextureSize;
				if (rowsNeeded > maxRows)
				{
					_paletteDraws.clear();
					return;
				}
				if (!_bonePalette || _bonePalette->GetResolution().y < rowsNeeded)
				{
					uint32_t height = 1;
					while (height < rowsNeeded)
					{
						height *= 2;
					}
					_bonePalette = std::make_unique<Texture2D>("BonePalette");
					_bonePalette->Create(bone_palette::k_Width, static_cast<uint16_t>(std::min(height, maxRows)), 1,
					                     TextureFormat::RGBA32F, Wrapping::ClampEdge, Filter::Nearest, nullptr);
				}
				_paletteTexels.resize(static_cast<size_t>(rowsNeeded) * bone_palette::k_Width, glm::vec4(0.0f));
				_bonePalette->UpdateRegion({0, 0}, {bone_palette::k_Width, rowsNeeded}, _paletteTexels.data(),
				                           static_cast<uint32_t>(_paletteTexels.size() * sizeof(glm::vec4)));
			};

			// Instance meshes, one range of a mesh at a time: forceQueued for the held objects' (sortedOpaqueDrawDescs)
			const auto drawRange = [&](entt::id_type meshId, const auto& placers, bool forceQueued) {
				if (!meshManager.Contains(meshId)) // (openblack guard) a mesh that did not load; never in the original
				{
					return;
				}
				auto mesh = meshManager.Handle(meshId);
				// a SuperVillager's HD body and its eyes are drawn with the light at the default sun
				// (SuperVillagerFrame::litByDefaultSun), not the frame's light
				std::optional<model_light::ScopedLight> superVillagerSun;
				if (desc.superVillagers.litByDefaultSun.contains(meshId))
				{
					superVillagerSun.emplace(model_light::k_DefaultSun);
				}

				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset, placers.count);
				setMatrices(meshId, *mesh);
				submitDesc.isSky = false;
				submitDesc.lightBoost = meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, meshId, submitDesc);
				// the snow lying under the world's objects shows on them in the main view, not on the hand
				submitDesc.snow = desc.viewId == graphics::RenderPass::Main && meshId != ecs::components::Hand::k_MeshId;
				submitDesc.morphWithTerrain = placers.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, submitDesc.morphWithTerrain);
				submitDesc.blendFilter = 0;

				// the hand always queues the whole hand (no test); any other model whole by its mesh's flag 0x200. In the main
				// view a queued one is drawn only from the queue, opaque primitives included; the others at once with their
				// blended primitives
				const bool hand = meshId == ecs::components::Hand::k_MeshId;
				// and a held object by its Z-sort flag (components::NeedsSorting, the range drawn with forceQueued)
				const bool queued = sortBlended && (forceQueued || hand || mesh->IsZSorted());
				// TODO(bwrsandman): choose the correct LOD
				// a creature too, posed or not: its body is drawn with its own shape and skins
				const auto palette = forceQueued ? _paletteDraws.end() : _paletteDraws.find(meshId);
				if (!queued && palette != _paletteDraws.end())
				{
					// the posed instances in view all at once, each with its own bones from the bone palette: the same
					// pixels as one draw each (planPaletteDraws)
					static const auto k_Identity = glm::mat4(1.0f);
					submitDesc.modelMatrices = &k_Identity;
					submitDesc.matrixCount = 1;
					submitDesc.transientInstance = &palette->second;
					submitDesc.bonePalette = _bonePalette.get();
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
					submitDesc.transientInstance = nullptr;
					submitDesc.bonePalette = nullptr;
				}
				else if (!queued && mesh->IsBoned() &&
				         (ecs::HasPose(poses, placers.offset, placers.count) || HasCreature(placers.offset, placers.count)))
				{
					// animated (ecs/Animations.h): each instance on its own, with its pose. Only the ones in the view: every
					// draw copies the bones into the backend's per-frame uniform buffer (see vs_object.sc)
					const auto viewProjection = desc.camera->GetViewProjectionMatrix();
					const auto box = mesh->GetBoundingBox();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						const auto& model = renderCtx.instanceUniforms[placers.offset + i];
						if (cutAbove.contains(placers.offset + i) || !rowDrawn(placers.offset + i))
						{
							continue;
						}
						if (!BoxInView(viewProjection, ownBox(placers.offset + i, meshId, box), model))
						{
							continue;
						}
						submitDesc.instanceDesc =
						    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset + i, 1);
						setMatrices(meshId, *mesh);
						ecs::UsePose(poses, placers.offset + i, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
						const auto* creature = creature_draw::Find(_frameCreatures, placers.offset + i);
						const auto* bodyProgram = submitDesc.program;
						if (creature != nullptr)
						{
							UseCreatureBody(*creature, submitDesc);
							// as its fizz leaves it: through the static while it fizzes out of sight or back in
							DrawCreatureBody(*mesh, submitDesc, *creature, desc.clock.frameGameMs);
						}
						else
						{
							DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
						}
						if (creature != nullptr)
						{
							submitDesc.program = bodyProgram;
							submitDesc.morphTargets = nullptr;
							submitDesc.paintedSkins = entt::null;
						}
						// drawn at once: the shadows over it right after, the tail of its Draw; a queued one gets them in its
						// Z object
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
						if (creature != nullptr)
						{
							DrawCreatureParts(desc.viewId, *desc.camera, renderCtx, *creature);
						}
						drawKnockRow(placers.offset + i);
					}
				}
				else if (!queued)
				{
					// one batch per run of the rows the object draw list draws; a whole range is drawn with the instance
					// rows set above
					for (const auto& run : graphics::instance_runs::Runs(rowMask, placers.offset, placers.count))
					{
						// a burning tree's own ALPHAREF (RenderContext::forcedAlphaRefs) is drawn on its own
						for (const auto& part : graphics::instance_runs::Split(run, renderCtx.forcedAlphaRefs))
						{
							if (part.offset != placers.offset || part.count != placers.count)
							{
								submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(
								    renderCtx.instanceUniformBuffer, part.offset, part.count);
							}
							submitDesc.forcedAlphaRef = part.forcedAlphaRef;
							DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
						}
						submitDesc.forcedAlphaRef = std::nullopt;
					}
					// the shadows over an object are the tail of its Draw: none over one the list leaves out
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						if (!rowDrawn(placers.offset + i))
						{
							continue;
						}
						// the parts of its model its status adds (a graveyard's graves), drawn as part of its own draw
						if (const auto status = renderCtx.objectStatuses.find(placers.offset + i);
						    status != renderCtx.objectStatuses.end() && Locator::config::value().drawGraves)
						{
							submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer,
							                                                                   placers.offset + i, 1);
							submitDesc.statusParts = status->second;
							DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
							submitDesc.statusParts.reset();
						}
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
						// then the end of its Draw: the read-out's row over it
						drawKnockRow(placers.offset + i);
					}
				}
				if (queued)
				{
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						if (cutAbove.contains(placers.offset + i) || !rowDrawn(placers.offset + i))
						{
							continue;
						}
						// the object's key (its origin, (x^2 + z^2) + y^2) or, for the hand, the origin of the hand's 3D
						// object, (x^2 + y^2) + z^2. (inferred) that the hand instance's translation is that object's origin
						const auto origin = glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						const auto order = hand ? zsort::SumOrder::XYZ : zsort::SumOrder::XZY;
						sorted.Submit(
						    {.meshId = meshId, .index = placers.offset + i, .morphWithTerrain = placers.morphWithTerrain},
						    zsort::Key(origin, cameraOrigin, order));
						// its Draw goes on at once after it queues the object: the read-out's row over it is not queued
						drawKnockRow(placers.offset + i);
					}
				}
			};
			// One model instance drawn on its own: from the queue, or a fading one at once (in the main view). The alternate
			// mode table with its alpha byte for a fading object (components::Alpha, in the object colour's alpha), every
			// primitive in its own mode for the others
			const auto drawInstance = [&](const ZObject& instance, RenderPass viewId) {
				auto mesh = meshManager.Handle(instance.meshId);
				// the SuperVillager's default sun, as in the instance loop above
				std::optional<model_light::ScopedLight> superVillagerSun;
				if (desc.superVillagers.litByDefaultSun.contains(instance.meshId))
				{
					superVillagerSun.emplace(model_light::k_DefaultSun);
				}
				submitDesc.viewId = viewId;
				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance.index, 1);
				setMatrices(instance.meshId, *mesh);
				ecs::UsePose(poses, instance.index, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.isSky = false;
				submitDesc.lightBoost = instance.meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = instance.meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, instance.meshId, submitDesc);
				submitDesc.snow =
				    desc.viewId == graphics::RenderPass::Main && instance.meshId != ecs::components::Hand::k_MeshId;
				submitDesc.morphWithTerrain = instance.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, instance.morphWithTerrain);
				// the whole object (its Draw; the hand's own draw)
				submitDesc.blendFilter = 0;
				submitDesc.options = instance.fading ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = instance.fading ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[instance.index][0][3]);
				submitDesc.mode = std::nullopt;
				submitDesc.sea = {};
				// a creature's body, with its own shape and skins, then its hair and eyes
				const auto* creature = creature_draw::Find(_frameCreatures, instance.index);
				if (creature != nullptr)
				{
					UseCreatureBody(*creature, submitDesc);
					DrawCreatureBody(*mesh, submitDesc, *creature, desc.clock.frameGameMs);
				}
				else
				{
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				}
				// the tail loop of its Draw: the shadows over it right after it, inside its Z object for a queued one, in the
				// main view for a fading one drawn at once. (inferred) that the hand's mesh draw runs that tail too, so its
				// shadows come before the held object and its effects
				DrawShadowsOnObject(viewId, instance.index, submitDesc.modelMatrices, submitDesc.matrixCount);
				// the hand's glow after those shadows, inside the hand's own Z object, in the world view only. (inferred)
				// where the original draws it in the frame is not traced. Nothing hands one in, so no bgfx call
				if (desc.viewId == graphics::RenderPass::Main && instance.meshId == ecs::components::Hand::k_MeshId)
				{
					DrawHandGlow(viewId, std::nullopt);
				}
				// a creature's hair and eyes after those shadows, as when it is drawn at once. (approximate) the original
				// draws a queued creature's eyes at once, outside its Z object; a creature's body is never queued today
				if (creature != nullptr)
				{
					submitDesc.morphTargets = nullptr;
					submitDesc.paintedSkins = entt::null;
					DrawCreatureParts(viewId, *desc.camera, renderCtx, *creature);
				}
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.viewId = desc.viewId;
			};
			// One PSys mesh atom (RenderContext::particleAtoms): the callback of a Sorted atom's Z object and the draw of a
			// Queued / Immediate one inside its effect: drawn as is, or cut by the default plane with DrawCutByPlane
			// (sea_pass::CutAtoms). A translucent one (global alpha or additive) through the alternate mode table, an
			// additive one in mode 13 (UseAdditiveAlpha, Creators/Mesh.h); an opaque one in its materials' own modes
			const auto drawAtom = [&](const RenderContext::ParticleInstance& atom, RenderPass viewId) {
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
				submitDesc.snow = false;
				submitDesc.morphWithTerrain = false;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
				submitDesc.blendFilter = 0;
				submitDesc.options = atom.translucent ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = atom.translucent ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[atom.index][0][3]);
				// a mode of its own (an object's ghost, the second draw) for every material, else mode 13 for an additive
				// atom, else the materials' own
				submitDesc.mode = atom.mode ? atom.mode
				                  : atom.translucent && atom.additive
				                      ? std::optional(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite)
				                      : std::nullopt;
				submitDesc.sea = atom.cut ? sea_pass::CutAtoms(viewId) : sea_pass::SeaDraw {};
				// an object's ghost: its passes' own ALPHAREF, depth test and mask texture
				submitDesc.forcedAlphaRef = atom.forcedAlphaRef;
				if (atom.depthEqual)
				{
					submitDesc.options.zFunc = render_modes::ZFunc::Equal;
				}
				const auto& textures = Locator::resources::value().GetTextures();
				submitDesc.textureOverride = atom.texture.has_value() && textures.Contains(*atom.texture)
				                                 ? textures.Handle(*atom.texture).operator->()
				                                 : nullptr;
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				submitDesc.sea = {};
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.mode = std::nullopt;
				submitDesc.forcedAlphaRef = std::nullopt;
				submitDesc.textureOverride = nullptr;
				submitDesc.viewId = desc.viewId;
			};
			// The blended models drawn at once (neither their mesh nor their object depth sorted) are drawn among the opaque
			// ones as the object list visits them: each after the opaque models the list draws before it and before those
			// it draws after it, which its depth then hides behind it (graphics::object_order). The opaque models are cut
			// into one segment of their ranges per blended model; with none on screen, one segment, the draw as it was
			std::vector<std::pair<object_order::Key, ZObject>> atOnce;
			std::vector<object_order::Key> keys;
			if (sortBlended)
			{
				for (const auto& [meshId, placers] : renderCtx.translucentDrawDescs)
				{
					if (!meshManager.Contains(meshId))
					{
						continue;
					}
					const bool zSorted = meshManager.Handle(meshId)->IsZSorted();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						const auto row = placers.offset + i;
						// fully faded out (alpha 0, kept in [0][3] as 1 - alpha) or left out by the list: not drawn
						if (renderCtx.instanceUniforms[row][0][3] >= 1.0f || !rowDrawn(row) ||
						    !object_order::DrawnAtOnce(zSorted, renderCtx.sortPoints.contains(row),
						                               renderCtx.sortedInstances.contains(row)))
						{
							continue;
						}
						atOnce.push_back({{},
						                  ZObject {.meshId = meshId,
						                           .index = row,
						                           .morphWithTerrain = placers.morphWithTerrain,
						                           .fading = true}});
					}
				}
				if (!atOnce.empty())
				{
					std::vector<object_order::Row> rows;
					rows.reserve(renderCtx.entityRows.size());
					for (const auto& row : renderCtx.entityRows)
					{
						rows.push_back({.index = row.index, .object = row.drawnBy, .step = row.step});
					}
					keys = object_order::RowKeys(
					    {.visited = desc.objectList.visited, .tail = desc.objectList.tail, .physics = desc.objectList.physics},
					    rows, renderCtx.instanceUniforms.size());
					for (auto& [key, instance] : atOnce)
					{
						key = keys[instance.index];
					}
					std::ranges::stable_sort(atOnce, {}, &std::pair<object_order::Key, ZObject>::first);
				}
			}
			// The physics objects are drawn after the leashes, as the original's physics list is (object_order::Band::
			// AfterLeashes): their rows leave the opaque draw here, for their own pass below. Their depth-sorted blended
			// models stay in the queue (a thrown one-shot orb's bubble), as their draw queues them
			object_order::PhysicsSplit physicsSplit;
			const auto& physicsRows = physicsSplit.physics;
			std::span<const uint8_t> queuedMask;
			if (desc.viewId == graphics::RenderPass::Main &&
			    (!desc.objectList.physics.empty() || !desc.objectList.sunk.empty()))
			{
				std::vector<object_order::Row> rows;
				rows.reserve(renderCtx.entityRows.size());
				for (const auto& row : renderCtx.entityRows)
				{
					rows.push_back({.index = row.index, .object = row.drawnBy, .step = row.step});
				}
				physicsSplit = object_order::SplitPhysics(rows, rowMask, renderCtx.instanceUniforms.size(),
				                                          desc.objectList.physics, desc.objectList.sunk);
				rowMask = physicsSplit.list;
				// (debug) the View menu's switch off: left out of the queue too, as before
				if (!Locator::config::has_value() || Locator::config::value().queuePhysicsBlended)
				{
					queuedMask = physicsSplit.queued;
				}
			}
			// a depth-sorted blended model's row: drawn by the list or by the physics objects' draw
			const auto queuedDrawn = [&rowDrawn, &queuedMask](uint32_t row) {
				return queuedMask.empty() ? rowDrawn(row) : row >= queuedMask.size() || queuedMask[row] != 0;
			};
			const auto drawOpaque = [&]() {
				for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
				{
					drawRange(meshId, placers, false);
				}
				for (const auto& [meshId, placers] : renderCtx.sortedOpaqueDrawDescs)
				{
					drawRange(meshId, placers, true);
				}
			};
			// One heart's model in the hearts' pass, drawn as the models' draw would draw it: in its range's way, with the
			// shadows over it and its read-out row after it; a fading one through the alternate mode table, or queued
			const auto drawHeartModel = [&](uint32_t row) {
				const auto within = [row](const auto& descs) {
					return std::ranges::find_if(descs, [row](const auto& range) {
						return row >= range.second.offset && row < range.second.offset + range.second.count;
					});
				};
				if (const auto opaque = within(renderCtx.instancedDrawDescs); opaque != renderCtx.instancedDrawDescs.end())
				{
					drawRange(opaque->first, RenderContext::InstancedDrawDesc(row, 1, opaque->second.morphWithTerrain), false);
					return;
				}
				if (const auto held = within(renderCtx.sortedOpaqueDrawDescs); held != renderCtx.sortedOpaqueDrawDescs.end())
				{
					drawRange(held->first, RenderContext::InstancedDrawDesc(row, 1, held->second.morphWithTerrain), true);
					return;
				}
				const auto fading = within(renderCtx.translucentDrawDescs);
				// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, as among the models
				if (fading == renderCtx.translucentDrawDescs.end() || !meshManager.Contains(fading->first) ||
				    renderCtx.instanceUniforms[row][0][3] >= 1.0f)
				{
					return;
				}
				const ZObject instance {
				    .meshId = fading->first, .index = row, .morphWithTerrain = fading->second.morphWithTerrain, .fading = true};
				if (object_order::DrawnAtOnce(meshManager.Handle(fading->first)->IsZSorted(),
				                              renderCtx.sortPoints.contains(row), renderCtx.sortedInstances.contains(row)))
				{
					const auto forced = renderCtx.forcedAlphaRefs.find(row);
					submitDesc.forcedAlphaRef =
					    forced != renderCtx.forcedAlphaRefs.end() ? std::optional<uint8_t>(forced->second) : std::nullopt;
					drawInstance(instance, desc.viewId);
					submitDesc.forcedAlphaRef = std::nullopt;
				}
				else
				{
					const auto point = renderCtx.sortPoints.find(row);
					const auto origin =
					    point != renderCtx.sortPoints.end() ? point->second : glm::vec3(renderCtx.instanceUniforms[row][3]);
					sorted.Submit(instance, zsort::Key(origin, cameraOrigin, zsort::SumOrder::XZY));
				}
				drawKnockRow(row);
			};
			if (desc.viewId == graphics::RenderPass::Main)
			{
				// the citadel hearts' own pass comes before the water rings and the models: the hearts newest first, each
				// one's shell over it as it is destroyed (whether the object draw list reaches the heart, it is off the
				// screen or faded out; RendererParticles.cpp), then its model, so each heart right after its own shell
				// (heart_shell::Pass). The models' own draw leaves the hearts out (withoutHearts, above)
				const auto withoutHeartsMask = rowMask;
				rowMask = std::span<const uint8_t> {_objectListRows};
				// (the palette's draws are planned below, for the models' draw; a heart's model is never posed)
				_paletteDraws.clear();
				_paletteTexels.clear();
				for (const auto& draw : heart_shell::Pass(passHearts))
				{
					const auto& heart = frameHearts[draw.heart];
					if (draw.step == heart_shell::PassStep::Shell)
					{
						DrawHeartShell(desc.viewId, *desc.camera, *heart.shell, desc.clock.frameGameMs);
					}
					else
					{
						drawHeartModel(*heart.row);
					}
				}
				rowMask = withoutHeartsMask;
				DrawWaterRings(desc.viewId);
				DrawGroundBlobs(desc.viewId, desc.superVillagers, desc.objectList,
				                billboard::CameraFrame::From(*desc.camera).clipMatrices);
				// the nets' part over the water, with the default plane
				DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane));
			}
			if (atOnce.empty())
			{
				planPaletteDraws();
				drawOpaque();
			}
			else
			{
				// cut into segments, the posed instances are drawn one by one, each in its own segment: the palette's one
				// draw of a mesh's instances would draw every segment's rows (the same pixels either way)
				_paletteDraws.clear();
				_paletteTexels.clear();
				std::vector<object_order::Key> blendedKeys;
				blendedKeys.reserve(atOnce.size());
				for (const auto& [key, instance] : atOnce)
				{
					blendedKeys.push_back(key);
				}
				const std::vector<uint8_t> listMask(rowMask.begin(), rowMask.end());
				std::vector<uint8_t> segmentMask(renderCtx.instanceUniforms.size());
				for (size_t segment = 0; segment <= atOnce.size(); ++segment)
				{
					for (uint32_t row = 0; row < segmentMask.size(); ++row)
					{
						const bool listed = row >= listMask.size() || listMask[row] != 0;
						segmentMask[row] = listed && object_order::SegmentOf(keys[row], blendedKeys) == segment ? 1 : 0;
					}
					const auto before = rowMask;
					rowMask = segmentMask;
					drawOpaque();
					rowMask = before;
					// the sharks' parts above the water that fall in this segment, each with the shadows over it, as the
					// original's walkers are drawn at their place in the list's tail
					if (desc.viewId == graphics::RenderPass::Main && !cutDrawn.empty())
					{
						std::unordered_set<uint32_t> cutHere;
						for (const auto row : cutDrawn)
						{
							if (row < keys.size() && object_order::SegmentOf(keys[row], blendedKeys) == segment)
							{
								cutHere.insert(row);
							}
						}
						if (!cutHere.empty())
						{
							DrawCutAboveWater(desc.viewId, cutHere);
							DrawShadowsOnCutObjects(desc.viewId, cutHere);
						}
					}
					if (segment < atOnce.size())
					{
						// through the alternate mode table, then the end of its draw: the read-out's row over it
						// a burning tree with its forced ALPHAREF, which only a draw at once sees (a queued one is drawn after
						// the tree has put the override back)
						const auto forced = renderCtx.forcedAlphaRefs.find(atOnce[segment].second.index);
						submitDesc.forcedAlphaRef =
						    forced != renderCtx.forcedAlphaRefs.end() ? std::optional<uint8_t>(forced->second) : std::nullopt;
						drawInstance(atOnce[segment].second, desc.viewId);
						submitDesc.forcedAlphaRef = std::nullopt;
						drawKnockRow(atOnce[segment].second.index);
					}
				}
			}
			// the sharks' parts above the water, in the normal object list, and the shadows over them (RendererShadows.cpp):
			// with no blended model drawn at once, after the opaque ones; otherwise in their segment, above
			if (desc.viewId == graphics::RenderPass::Main && atOnce.empty())
			{
				DrawCutAboveWater(desc.viewId);
				DrawShadowsOnCutObjects(desc.viewId, cutDrawn);
			}
			if (sortBlended)
			{
				for (const auto& [meshId, placers] : renderCtx.translucentDrawDescs)
				{
					if (!meshManager.Contains(meshId)) // (openblack guard) a mesh that did not load; never in the original
					{
						continue;
					}
					const bool zSorted = meshManager.Handle(meshId)->IsZSorted();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth. Nor
						// one the object draw list leaves out (a flying physics object's is drawn: queued by its own draw)
						if (renderCtx.instanceUniforms[placers.offset + i][0][3] >= 1.0f || !queuedDrawn(placers.offset + i))
						{
							continue;
						}
						const ZObject instance {.meshId = meshId,
						                        .index = placers.offset + i,
						                        .morphWithTerrain = placers.morphWithTerrain,
						                        .fading = true};
						// the sort point of the one-shot orb: only the orbs have one, and their object is always queued
						const auto point = renderCtx.sortPoints.find(placers.offset + i);
						// a fading held object too (components::NeedsSorting, the Z-sort flag)
						if (!zSorted && point == renderCtx.sortPoints.end() &&
						    !renderCtx.sortedInstances.contains(placers.offset + i))
						{
							// without the Z-sort flag: drawn at once, among the opaque models above. The physical shield too
							// when its mesh lacks 0x200 (it sets its global alpha, then is drawn as any object)
							continue;
						}
						// the object's key ((x^2 + z^2) + y^2) at the orb's sort point or the translation
						const auto origin = point != renderCtx.sortPoints.end()
						                        ? point->second
						                        : glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						sorted.Submit(instance, zsort::Key(origin, cameraOrigin, zsort::SumOrder::XZY));
						drawKnockRow(placers.offset + i);
					}
				}
				// the domes the shield service hands out, after the models (none today: the domes are map objects, queued
				// with the models above)
				DrawShieldDomes(desc, sorted);
			}

			// The broken buildings and their fragments (damage mesh): lit per face on the CPU and drawn at once in the object
			// loop, no Z object. (approximate) here after every model instead of in the loop's order
			if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities && desc.camera != nullptr)
			{
				_frameDamageMeshes.Clear();
				openblack::physics::damage::Mesh::FrameLight frameLight {
				    .lit = IsLandLit(),
				    .light = model_light::Light(),
				    .ambient = model_light::Ambient(),
				    .haze = _haze,
				    .view = desc.camera->GetViewMatrix(Camera::Interpolation::Current),
				};
				// the fire glow's clock: the frame's turn and its fraction (DrawClock)
				const float turnTime = static_cast<float>(desc.clock.turn) + desc.clock.turnFraction;
				Locator::buildingDamageSystem::value().AppendDamageMeshes(_frameDamageMeshes, frameLight, turnTime);
				// (approximate) every primitive, blended ones too, goes in Main
				// (docs/wiki/rendering/model-lighting.md#model-lighting)
				world_triangles::Submit(graphics::RenderPass::Main, _frameDamageMeshes, *_shaderManager);
			}
			// the rows of the buildings drawn here with their damage mesh, or not drawn as an instance at all: after
			// their draw too
			for (size_t k = 0; k < knockRowDrawn.size(); ++k)
			{
				drawKnockRowAt(k);
			}
			// The leashes, drawn at once right after the land and its models, as the original does: before the exploded
			// pieces, the hand and the particle effects drawn at once
			if (sortBlended)
			{
				DrawLeashes(desc.viewId, *desc.camera);
			}
			// then the physics objects, by mesh as the opaque draw (the same pixels as the physics list's order but where two
			// are at the same depth); the palette's one draw of a mesh's instances is for the opaque draw above
			if (!physicsRows.empty())
			{
				_paletteDraws.clear();
				_paletteTexels.clear();
				rowMask = physicsRows;
				drawOpaque();
				rowMask = _objectListRows;
			}
			// ---- the exploded pieces ----
			// The exploded pieces are drawn at once, no Z object (they ignore the queue flag), after the models drawn at once
			// and before the queue's drain (at the end of the frame); drawn below with the Sorted surfaces, in the walk's order
			// (SortedFrame::atOnce); built by PreDraw
			// ---- end of the exploded pieces ----
			// The particle effects by their draw path (particles::DrawPath)
			// collected by PreDraw (CollectFrameParticles); the other views draw none of them, as before
			static const particles::draw::Frame k_NoParticles {};
			const auto& frameParticles = sortBlended ? _particleFrame : k_NoParticles;
			const auto& particleSorted = frameParticles.sorted;
			const auto& particleQueued = frameParticles.queued;
			const auto& particleHand = frameParticles.hand;
			const auto& particleSurfaces = frameParticles.surfaces;
			std::unordered_map<const particles::Atom*, size_t> surfaceOf;
			for (size_t i = 0; i < particleSurfaces.size(); ++i)
			{
				surfaceOf.insert_or_assign(particleSurfaces[i].atom, i);
			}
			// The glowing shells (an artefact's aura): a Sorted effect's each take their own place, the others' are drawn
			// with their effect, found by their atom
			const auto& particleShells = frameParticles.shells;
			std::unordered_map<const particles::Atom*, size_t> shellOf;
			for (size_t i = 0; i < particleShells.size(); ++i)
			{
				if (particleShells[i].path != particles::DrawPath::Sorted && particleShells[i].atom != nullptr)
				{
					shellOf.insert_or_assign(particleShells[i].atom, i);
				}
			}
			// The pieces (Kind::MeshPiece) of the Queued and Immediate effects, built once for the frame and tagged with their
			// atom: each is drawn by drawOrderedEffect at its atom's place (world_triangles::Submit's `only`)
			const auto& orderedPieces = frameParticles.orderedPieces; // built by PreDraw (CollectFrameParticles)
			// A Queued or Immediate effect drawn all at once (the queue flag off): its items in the effect's order
			// (manager::OrderedEffect). Sprites through DrawParticleSprites, meshes through drawAtom, mists through
			// DrawEffectMist, surfaces through DrawParticleSurface, pieces through DrawParticleFragment, chains
			// through DrawParticleChain; the other kinds draw nothing here
			const auto drawOrderedEffect = [&](const particles::manager::OrderedEffect& effect, RenderPass viewId) {
				std::vector<particles::Effect::DrawAtom> sprites;
				const auto flushSprites = [&]() {
					if (!sprites.empty())
					{
						DrawParticleSprites(sprites, *desc.camera, viewId);
						sprites.clear();
					}
				};
				for (const auto& item : effect.items)
				{
					const auto* creator = item.atom.creator;
					if (item.chain < 0 && item.atom.objectGlow.has_value())
					{
						// a glowing shell over an object's model, whatever its atom's creator
						flushSprites();
						if (const auto found = shellOf.find(item.atom.atom); found != shellOf.end())
						{
							DrawParticleShell(viewId, *desc.camera, particleShells[found->second]);
						}
						continue;
					}
					if (item.chain < 0 && creator != nullptr && creator->kind == particles::Creator::Kind::Sprite)
					{
						sprites.push_back(item.atom);
						continue;
					}
					flushSprites();
					if (item.chain >= 0)
					{
						if (static_cast<size_t>(item.chain) < effect.chains.size())
						{
							DrawParticleChain(viewId, *desc.camera, effect.chains[static_cast<size_t>(item.chain)]);
						}
						continue;
					}
					if (creator == nullptr)
					{
						continue;
					}
					if (creator->kind == particles::Creator::Kind::MeshPiece)
					{
						// a piece atom: its world triangles at once, at the atom's place in the effect's order (it ignores the
						// queue flag). No data uses it today: the pieces' effect, EXPLODE_OBJECT, is drawn Sorted (the block
						// above, in Main)
						DrawParticleFragment(viewId, orderedPieces, item.atom.atom);
						continue;
					}
					if (creator->kind == particles::Creator::Kind::Mesh)
					{
						const auto found = renderCtx.particleAtomIndex.find(item.atom.atom);
						if (found != renderCtx.particleAtomIndex.end())
						{
							drawAtom(renderCtx.particleAtoms[found->second], viewId);
						}
						continue;
					}
					if (dynamic_cast<const particles::MistCreator*>(creator) != nullptr)
					{
						mists::MistDesc mist {};
						if (particles::mist_atoms::Describe(item.atom, mist))
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
							DrawParticleSurface(viewId, particleSurfaces[found->second]);
						}
					}
				}
				flushSprites();
			};
			if (sortBlended)
			{
				// A Sorted effect (the spells and the other Sorted sites): the effect has no Z object, each element goes into
				// the queue with its own key, (x^2 + y^2) + z^2 (zsort::SumOrder::XYZ). Its MeshPiece pieces (the exploded
				// pieces above) and ZR_SurfRevol surfaces ignore the queue flag: drawn at once, here after the models, in the
				// walk's order, effect by effect (RendererRevolvedSurface.cpp)
				if (desc.drawEntities)
				{
					for (const auto* atom : particleSorted.atOnce)
					{
						if (const auto found = surfaceOf.find(atom); found != surfaceOf.end())
						{
							if (particleSurfaces[found->second].path == particles::DrawPath::Sorted)
							{
								DrawParticleSurface(desc.viewId, particleSurfaces[found->second]);
							}
							continue;
						}
						// one Submit per piece (its own transient buffer): the same draws, but the (openblack) guard of a full
						// transient buffer now drops whole later pieces instead of cutting the last one
						DrawParticleFragment(graphics::RenderPass::Main, _particleFrame.sortedPieces, atom);
					}
				}
				// each sprite, keyed at the sprite's position (manager::SortedFrame::sprites)
				for (size_t i = 0; i < particleSorted.sprites.size(); ++i)
				{
					const particles::draw::Item item {.kind = particles::draw::ItemKind::Sprite,
					                                  .index = static_cast<uint32_t>(i),
					                                  .sortPoint = particleSorted.sprites[i].key};
					sorted.Submit({.particleItem = item}, zsort::Key(item.sortPoint, cameraOrigin));
				}
				// each mesh atom, opaque ones too, after the on-screen test (here the sphere around the mesh's box,
				// (approximate) not the box), keyed at the object's origin
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				for (size_t i = 0; i < renderCtx.particleAtoms.size(); ++i)
				{
					const auto& atom = renderCtx.particleAtoms[i];
					if (atom.path != particles::DrawPath::Sorted)
					{
						continue;
					}
					const auto& model = renderCtx.instanceUniforms[atom.index];
					if (!BoxInView(viewProjection, meshManager.Handle(atom.meshId)->GetBoundingBox(), model))
					{
						continue;
					}
					const particles::draw::Item item {
					    .kind = particles::draw::ItemKind::Mesh, .index = static_cast<uint32_t>(i), .sortPoint = atom.key};
					sorted.Submit({.particleItem = item}, zsort::Key(item.sortPoint, cameraOrigin));
				}
				// each mist, its own Z object at the mist's position: they come through mists::Submit
				// (mist_atoms::SubmitFrame) and CollectMists (PreDraw).
				// each chain, keyed at the joint n / 2
				for (size_t i = 0; i < particleSorted.chains.size(); ++i)
				{
					const particles::draw::Item item {.kind = particles::draw::ItemKind::Chain,
					                                  .index = static_cast<uint32_t>(i),
					                                  .sortPoint = particleSorted.chains[i].key};
					sorted.Submit({.particleItem = item}, zsort::Key(item.sortPoint, cameraOrigin));
				}
				// the Queued effects (the seed graphic on a ball or an icon, the containers; the fire's graphic likewise):
				// one Z object per effect at its origin, drawn all at once from the drain
				for (size_t i = 0; i < particleQueued.size(); ++i)
				{
					sorted.Submit({.queuedEffect = static_cast<int>(i)}, zsort::Key(particleQueued[i].origin, cameraOrigin));
				}
				// each sheet of light, one Z object at its origin (its middle before this frame's build), the newest first
				// as CollectFrameParticles lists them, so that equal keys keep the game's order; the key's squares are
				// summed x, then y, then z, as the game sums them for the sheet
				const auto& lightSheets = frameParticles.lightSheets;
				for (size_t i = 0; i < lightSheets.size(); ++i)
				{
					const particles::draw::Item item {.kind = particles::draw::ItemKind::Surface,
					                                  .index = static_cast<uint32_t>(i),
					                                  .sortPoint = lightSheets[i].origin};
					sorted.Submit({.particleItem = item}, zsort::Key(item.sortPoint, cameraOrigin, zsort::SumOrder::XYZ));
				}
				// each glowing shell of a Sorted effect, one Z object where its object stands
				for (size_t i = 0; i < particleShells.size(); ++i)
				{
					if (particleShells[i].path != particles::DrawPath::Sorted)
					{
						continue;
					}
					const particles::draw::Item item {.kind = particles::draw::ItemKind::Shell,
					                                  .index = static_cast<uint32_t>(i),
					                                  .sortPoint = particleShells[i].sortPoint};
					sorted.Submit({.particleItem = item}, zsort::Key(item.sortPoint, cameraOrigin));
				}
			}
			if (spritesSorted)
			{
				Locator::entitiesRegistry::value().Each<const ecs::components::Sprite, const ecs::components::Transform>(
				    [&sorted, &cameraOrigin](entt::entity entity, const auto&, const ecs::components::Transform& transform) {
					    // the sprite's position, (x^2 + y^2) + z^2
					    sorted.Submit({.sprite = entity}, zsort::Key(transform.position, cameraOrigin));
				    },
				    entt::exclude<ecs::components::Unavailable>);
				// the dust, the same key (it came in the loop above while its puffs were entities)
				for (size_t i = 0; i < _frameDust.size(); ++i)
				{
					sorted.Submit({.dust = static_cast<int>(i)}, zsort::Key(_frameDust[i].position, cameraOrigin));
				}
			}
			if (mistsSorted)
			{
				for (const auto& [key, index] : _preMists) // collected by PreDraw
				{
					sorted.Submit({.mist = static_cast<int>(index)}, key);
				}
			}
			if (spritesSorted)
			{
				for (const auto& [key, index] : _preSmoke) // collected by PreDraw
				{
					sorted.Submit({.smoke = static_cast<int>(index)}, key);
				}
			}
			// the temples' leash posts (RendererLeashPosts.cpp, made by PreDraw), each post two Z objects when on screen: its
			// smoke at its point ((x^2 + y^2) + z^2, a sprite's), then its collar at the collar's origin ((x^2 + z^2) + y^2, a
			// model's). (inferred) the smoke's screen test, its sphere of its half width. The Citadel debug window's switch
			// hides them
			if (sortBlended && (!Locator::config::has_value() || Locator::config::value().drawLeashPosts))
			{
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				constexpr auto k_CollarMesh = resources::shared_assets::k_LeashCollarMesh.value();
				for (size_t i = 0; i < _frameLeashPosts.size(); ++i)
				{
					const auto& post = _frameLeashPosts[i];
					if (spritesSorted && SphereInView(viewProjection, post.smoke.position, post.smoke.size))
					{
						sorted.Submit({.leashSmoke = static_cast<int>(i)}, zsort::Key(post.smoke.position, cameraOrigin));
					}
					if (meshManager.Contains(k_CollarMesh) &&
					    BoxInView(viewProjection, meshManager.Handle(k_CollarMesh)->GetBoundingBox(), post.collar))
					{
						sorted.Submit({.leashCollar = static_cast<int>(i)},
						              zsort::Key(glm::vec3(post.collar[3]), cameraOrigin, zsort::SumOrder::XZY));
					}
				}
			}
			// the columns of light over the village centres and the temples (RendererLightColumns.cpp, made by PreDraw
			// with those on screen), one Z object each at the point it stands on, (inferred) keyed as a model's
			// ((x^2 + z^2) + y^2). The world view only; the Citadel debug window's switch hides them
			if (sortBlended && desc.viewId == RenderPass::Main &&
			    (!Locator::config::has_value() || Locator::config::value().drawLightColumns))
			{
				for (size_t i = 0; i < _frameLightColumns.size(); ++i)
				{
					sorted.Submit({.lightColumn = static_cast<int>(i)},
					              zsort::Key(glm::vec3(_frameLightColumns[i].model[3]), cameraOrigin, zsort::SumOrder::XZY));
				}
			}
			// the creatures' creed glows (RendererParticles.cpp), one Z object each at the glow's point, keyed as a glowing
			// shell's ((x^2 + y^2) + z^2). The world view only: (inferred) not in the water's reflection
			if (sortBlended && desc.viewId == RenderPass::Main)
			{
				CollectCreedGlows(*desc.camera);
				for (size_t i = 0; i < _frameCreedGlows.size(); ++i)
				{
					sorted.Submit({.creedGlow = static_cast<int>(i)},
					              zsort::Key(glm::vec3(_frameCreedGlows[i].model[3]), cameraOrigin, zsort::SumOrder::XYZ));
				}
			}
			if (sortBlended)
			{
				// one Z object per raining tile; they used to be drawn as one group after the queue
				for (const auto& [key, index] : _preRain) // collected by PreDraw
				{
					sorted.Submit({.rain = static_cast<int>(index)}, key);
				}
				// and one per snowing tile, in the same queue
				for (const auto& [key, index] : _preSnow) // collected by PreDraw
				{
					sorted.Submit({.snow = static_cast<int>(index)}, key);
				}
				// the boat's sprites, each one on its own; they used to be one batch after the queue
				for (const auto& [key, index] : CollectBoatSprites(*desc.camera))
				{
					sorted.Submit({.boat = static_cast<int>(index)}, key);
				}
				// the hand's ripples on the influence border, one Z object each (RendererInfluence.cpp)
				for (const auto& [key, index] : CollectInfluenceRipples(*desc.camera))
				{
					sorted.Submit({.ripple = static_cast<int>(index)}, key);
				}
				// the intro light, one Z object, keyed at its head ((x^2 + y^2) + z^2); RendererIntroLight.cpp
				if (desc.overlay.introLight.active)
				{
					const auto& introLight = desc.overlay.introLight;
					sorted.Submit({.introLight = 0}, zsort::Key(introLight.keyPoint, desc.camera->GetOrigin()));
				}
				// the gesture icons, one Z object each at key 0, so after everything farther, in the list's order
				// (RendererGestureIcons.cpp). The world view only
				if (desc.viewId == RenderPass::Main)
				{
					for (size_t i = 0; i < desc.overlay.gestureIcons.size(); ++i)
					{
						sorted.Submit({.gestureIcon = static_cast<int>(i)}, 0.0f);
					}
				}
				// the did-you-know bubble, one Z object of the help system's 3D draw, keyed at the point over its sign
				// ((x^2 + y^2) + z^2), so that the dialogue texts are drawn over it
				if (desc.overlay.didYouKnow.active)
				{
					sorted.Submit({.didYouKnow = 0}, zsort::Key(desc.overlay.didYouKnow.keyPoint, desc.camera->GetOrigin()));
				}
			}
			// The tribes' names: each ring or column is a Z object of its own, keyed at its centre ((x^2 + y^2) + z^2),
			// queued before the influence circles as the original does. None in the vanilla game
			const auto tribalPower = sortBlended ? tribal_power::Submit(tribal_power::Source(), sorted, cameraOrigin,
			                                                            [](int i) { return ZObject {.tribalPower = i}; })
			                                     : std::vector<const magic::tribal_spin::Runner*> {};
			// the one-shot globes' draws, after the tribes' names (none today: the globes are entities, queued with the models
			// above)
			if (sortBlended)
			{
				DrawGlobes(desc, sorted);
			}

			// OPENBLACK_ZSORTER_TRACE=1: once a second, what the frame's queue holds
			// (docs/wiki/project/debugging.md#debug-environment-variables)
			static const bool k_ZSorterTrace = std::getenv("OPENBLACK_ZSORTER_TRACE") != nullptr;
			if (k_ZSorterTrace && sortBlended && ++RendererDebugHooksData().zsorterTraceFrame % 60 == 0)
			{
				std::array<int, 14> counts {};
				const auto ordered = sorted.Ordered();
				for (const auto& entry : ordered)
				{
					const auto& z = *entry.item;
					const auto particleKind = [&z](particles::draw::ItemKind kind) {
						return z.particleItem.has_value() && z.particleItem->kind == kind;
					};
					const size_t kind = z.cloud >= 0                                               ? 1
					                    : z.rain >= 0 || z.snow >= 0                               ? 2
					                    : z.boat >= 0                                              ? 3
					                    : particleKind(particles::draw::ItemKind::Sprite)          ? 4
					                    : particleKind(particles::draw::ItemKind::Mesh)            ? 5
					                    : particleKind(particles::draw::ItemKind::Chain)           ? 6
					                    : z.queuedEffect >= 0                                      ? 7
					                    : z.mist >= 0                                              ? 8
					                    : z.smoke >= 0                                             ? 9
					                    : z.sprite != entt::null || z.dust >= 0                    ? 10
					                    : z.meshId == ecs::components::Hand::k_MeshId && !z.fading ? 11
					                    : z.fading                                                 ? 12
					                    : z.IsModel()                                              ? 0
					                                                                               : 13;
					++counts.at(kind);
				}
				if (auto logger = spdlog::get("graphics"); logger != nullptr)
				{
					SPDLOG_LOGGER_INFO(
					    logger,
					    "ZSorter trace: {} entries ({} dropped): models {}, fading {}, clouds {}, rain tiles {}, "
					    "boat sprites {}, PSys sprites {}, PSys meshes {}, PSys chains {}, queued effects {}, "
					    "mists {}, smoke {}, sprites {}, hand {}; farthest key {:.1f}, nearest {:.1f}",
					    ordered.size(), sorted.Dropped(), counts[0], counts[12], counts[1], counts[2], counts[3], counts[4],
					    counts[5], counts[6], counts[7], counts[8], counts[9], counts[10], counts[11],
					    ordered.empty() ? 0.0f : ordered.front().key, ordered.empty() ? 0.0f : ordered.back().key);
				}
			}

			// OPENBLACK_ORB_TRACE=1: one line a drawn frame per one-shot orb
			// (docs/wiki/project/debugging.md#debug-environment-variables), to follow the bubble across the 15 -> 0 wrap of its
			// 4 x 4 sheet
			static const bool k_OrbTrace = std::getenv("OPENBLACK_ORB_TRACE") != nullptr;
			if (debug_env::TraceLogs(k_OrbTrace, "graphics") && sortBlended && Locator::entitiesRegistry::has_value())
			{
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				// the queue in draw order; the key printed is the distance (the root of the queue's key) as before
				const auto ordered = sorted.Ordered();
				// the ZR_SurfRevol discs: a Sorted effect's at once, before the whole queue (so before every bubble); a
				// Queued one's inside its effect's entry
				for (const auto& surface : particleSurfaces)
				{
					int at = -1;
					for (size_t k = 0; k < ordered.size() && surface.path != particles::DrawPath::Sorted; ++k)
					{
						const int effect = ordered[k].item->queuedEffect;
						if (effect >= 0 && particleQueued[static_cast<size_t>(effect)].effect == surface.effect)
						{
							at = static_cast<int>(k);
							break;
						}
					}
					SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
					                   "Orb trace: surface ({}) path {} sorted {}/{} origin ({:.1f}, {:.1f}, {:.1f})",
					                   surface.texture, static_cast<int>(surface.path), at, static_cast<int>(ordered.size()),
					                   surface.origin.x, surface.origin.y, surface.origin.z);
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
						    if (ordered[k].item->fading && ordered[k].item->index == index &&
						        ordered[k].item->meshId == mesh.id)
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

			// The influence circles: after the power spins, so after everything drawn at once, and before the drain at the
			// end of the frame, every frame of the world view. (inferred) not in the reflection: they are drawn once, after
			// the whole world draw (RendererInfluence.cpp; docs/wiki/rendering/frame.md#draw-stages-in-order)
			if (sortBlended)
			{
				DrawInfluenceCircles(desc.viewId); // its camera gate, scroll and alpha: PreDraw
			}

			// The drain, far to near (a full queue dropped the entries over 2048), in
			// its own view right after the main pass (same target and camera, no clear), so that nothing drawn at once in
			// the main pass is drawn over them
			if (!sorted.Empty())
			{
				constexpr auto k_Blended = graphics::RenderPass::MainBlended;
				auto& spriteRegistry = Locator::entitiesRegistry::value();
				const auto drained = sorted.Drain();
				bool handEffectsDrawn = false;
				std::vector<particles::Effect::DrawAtom> sprites;
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
					if (instance.snow >= 0)
					{
						DrawSnowTile(k_Blended, static_cast<uint32_t>(instance.snow));
						continue;
					}
					if (instance.boat >= 0)
					{
						DrawBoatSprite(k_Blended, static_cast<uint32_t>(instance.boat));
						continue;
					}
					if (instance.ripple >= 0)
					{
						DrawInfluenceRipple(k_Blended, static_cast<uint32_t>(instance.ripple), desc.clock.frameGameMs);
						continue;
					}
					if (instance.introLight >= 0)
					{
						DrawIntroLight(k_Blended, *desc.camera, desc.overlay.introLight);
						continue;
					}
					if (instance.gestureIcon >= 0)
					{
						DrawGestureIcon(k_Blended, *desc.camera,
						                desc.overlay.gestureIcons[static_cast<size_t>(instance.gestureIcon)]);
						continue;
					}
					if (instance.didYouKnow >= 0)
					{
						DrawDidYouKnowBubble(k_Blended, *desc.camera, desc.overlay);
						continue;
					}
					if (instance.tribalPower >= 0)
					{
						DrawTribalPower(k_Blended, *tribalPower[static_cast<size_t>(instance.tribalPower)]);
						continue;
					}
					if (instance.globe >= 0)
					{
						// a globe's draw from DrawGlobes: none is queued today, and its draw comes with the list's first feed
						continue;
					}
					if (instance.particleItem.has_value())
					{
						const auto& item = *instance.particleItem;
						switch (item.kind)
						{
						case particles::draw::ItemKind::Sprite:
						{
							// a Sorted effect's sprite, drawn from its Z object; the next entries that are sprites too go in
							// the same call (DrawParticleSprites keeps their order and batches only equal materials)
							sprites.clear();
							sprites.push_back(particleSorted.sprites[item.index].atom);
							while (e + 1 < drained.size())
							{
								const auto& next = drained[e + 1].item->particleItem;
								if (!next.has_value() || next->kind != particles::draw::ItemKind::Sprite)
								{
									break;
								}
								++e;
								sprites.push_back(particleSorted.sprites[next->index].atom);
							}
							DrawParticleSprites(sprites, *desc.camera, k_Blended);
							break;
						}
						case particles::draw::ItemKind::Mesh:
							drawAtom(renderCtx.particleAtoms[item.index], k_Blended);
							break;
						case particles::draw::ItemKind::Chain:
							// the chain's Z object callback
							DrawParticleChain(k_Blended, *desc.camera, particleSorted.chains[item.index].chain);
							break;
						case particles::draw::ItemKind::Surface:
							// a sheet of light's Z object, the only surface queued on its own
							DrawParticleSurface(k_Blended, frameParticles.lightSheets[item.index]);
							break;
						case particles::draw::ItemKind::Shell:
							// a glowing shell's Z object, over its object as it is drawn this frame
							DrawParticleShell(k_Blended, *desc.camera, particleShells[item.index]);
							break;
						case particles::draw::ItemKind::Mist:
						case particles::draw::ItemKind::Fragment:
							// not queued on their own: the mists come through mists::Submit, the pieces are drawn at once
							// (DrawParticleFragment), as the effects' surfaces are (DrawParticleSurface)
							break;
						}
						continue;
					}
					if (instance.queuedEffect >= 0)
					{
						// the Queued effect's Z object callback
						drawOrderedEffect(particleQueued[static_cast<size_t>(instance.queuedEffect)], k_Blended);
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
					if (instance.leashSmoke >= 0)
					{
						DrawLeashPostSmoke(k_Blended, *desc.camera, static_cast<uint32_t>(instance.leashSmoke));
						continue;
					}
					if (instance.leashCollar >= 0)
					{
						DrawLeashPostCollar(k_Blended, static_cast<uint32_t>(instance.leashCollar));
						continue;
					}
					if (instance.lightColumn >= 0)
					{
						DrawLightColumn(k_Blended, static_cast<uint32_t>(instance.lightColumn));
						continue;
					}
					if (instance.creedGlow >= 0)
					{
						DrawCreedGlow(k_Blended, *desc.camera, static_cast<uint32_t>(instance.creedGlow));
						continue;
					}
					if (instance.sprite != entt::null)
					{
						const auto& [sprite, transform] =
						    spriteRegistry.Get<const ecs::components::Sprite, const ecs::components::Transform>(
						        instance.sprite);
						drawSprite(instance.sprite, sprite, transform, k_Blended);
						continue;
					}
					if (instance.dust >= 0)
					{
						drawDust(_frameDust[static_cast<size_t>(instance.dust)], k_Blended);
						continue;
					}
					if (!instance.IsModel())
					{
						continue;
					}
					drawInstance(instance, k_Blended);
					// the hand's draw: after the hand's mesh and the held object, the spell in the hand draws the hand's
					// effects at once, in their own order (manager::HandEffects, DrawPath::Immediate). The held object is not
					// drawn from the hand's entry here (inferred: no visible difference, it is opaque and drawn before)
					if (instance.meshId == ecs::components::Hand::k_MeshId && !instance.fading && !handEffectsDrawn)
					{
						handEffectsDrawn = true;
						for (const auto& effect : particleHand)
						{
							drawOrderedEffect(effect, k_Blended);
						}
					}
				}
			}
			// The effects' ribbons and surfaces, the rain tiles, the boat's sprites and the
			// clouds are no longer groups of their own: they all went at once or through the queue above. The shadows on the
			// objects went with their objects: right after each one drawn at once, inside the Z object of each queued one
			// (RendererShadows.cpp). A receiver not drawn this frame (out of view, faded out, past the queue's 2048) gets none,
			// as the tail of a Draw that did not run
			ClearShadowReceivers();

			// Debug
			if (desc.viewId == graphics::RenderPass::Main)
			{
				if (renderCtx.boundingBox)
				{
					const auto boundBoxOffset = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					const auto boundBoxCount = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					renderCtx.boundingBox->GetVertexBuffer().Bind();
					bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), boundBoxOffset, boundBoxCount);
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShaderInstanced->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
				if (renderCtx.footpaths)
				{
					renderCtx.footpaths->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
				if (renderCtx.streams)
				{
					renderCtx.streams->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
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
				    [&drawSprite, &desc](entt::entity entity, const Sprite& sprite, const Transform& transform) {
					    drawSprite(entity, sprite, transform, desc.viewId);
				    },
				    entt::exclude<ecs::components::Unavailable>);
				for (const auto& puff : _frameDust)
				{
					drawDust(puff, desc.viewId);
				}
			}
		}
	}

	if (desc.drawSky && desc.viewId == graphics::RenderPass::Main)
	{
		if (!mistsSorted)
		{
			// without the entities the models' section did not drain the queue: the mists join the clouds in it here and
			// it is drained; nothing else can be in it
			for (const auto& [key, index] : _preMists) // collected by PreDraw
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
		DrawSun(graphics::RenderPass::MainBlended, *desc.camera, true, desc.clock.frameGameMs);
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
	SubmitOpaqueAlpha();
	// Advance to next frame. Process submitted rendering primitives.
	const auto frame = bgfx::frame();
	++_framesDrawn;
	_shaderManager->FrameEnded();
	_offscreenShots->FrameEnded(frame, _bgfxCallback->failures, _bgfxCallback->pictures);
}

void Renderer::SubmitOpaqueAlpha() const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// (openblack engine) every view before this one may leave the backbuffer's alpha below 1 (the blended models,
	// sprites, ImGui), and bgfx's Vulkan swapchain is composited with it when the driver offers it
	// (VK_COMPOSITE_ALPHA_INHERIT / PRE_MULTIPLIED, renderer_vk.cpp:7305): the desktop then shows through. A full-screen
	// quad writing only the alpha (no RGB, no depth test or write, no blending) makes it 1 without changing the image
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::OpaqueAlpha);
	bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
	bgfx::setViewRect(viewId, 0, 0, _resolution.x, _resolution.y);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	constexpr uint32_t k_OpaqueWhite = 0xFFFFFFFFu;
	const std::array<ScreenRectVertex, 6> quad {{{-1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, -1.0f, 0.5f, k_OpaqueWhite},
	                                             {-1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, -1.0f, 0.5f, k_OpaqueWhite},
	                                             {-1.0f, -1.0f, 0.5f, k_OpaqueWhite}}};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(quad.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, quad.data(), sizeof(quad));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_WRITE_A);
	_shaderManager->GetShader("DebugLine")->Submit(viewId);
}

void Renderer::RequestScreenshot(const std::filesystem::path& filepath) noexcept
{
	const bool minimised = Locator::windowing::has_value() && Locator::windowing::value().IsMinimised();
	const auto size = Locator::windowing::has_value() ? glm::u16vec2(Locator::windowing::value().GetSize()) : glm::u16vec2(0);
	constexpr uint64_t k_ReadBackCaps = BGFX_CAPS_TEXTURE_READ_BACK | BGFX_CAPS_TEXTURE_BLIT;
	const bool canReadBack = (bgfx::getCaps()->supported & k_ReadBackCaps) == k_ReadBackCaps;
	// Vulkan lets go of the window's buffers while it is minimised, where the other renderers keep drawing into them
	const bool screenDropped = bgfx::getRendererType() == bgfx::RendererType::Vulkan;
	switch (const auto plan = PlanScreenshot(screenDropped, minimised, canReadBack, size); plan.route)
	{
	case ScreenshotRoute::Screen:
	{
		const bgfx::FrameBufferHandle mainBackbuffer = BGFX_INVALID_HANDLE;
		bgfx::requestScreenShot(mainBackbuffer, filepath.string().c_str());
		break;
	}
	case ScreenshotRoute::Offscreen:
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "The window is minimised: drawing the frame aside for {}",
		                   filepath.string());
		_offscreenShots->Begin(filepath, size);
		break;
	case ScreenshotRoute::Refused:
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No screenshot at {}: {}", filepath.string(), plan.why);
		_bgfxCallback->failures.Record(filepath.string(), plan.why);
		break;
	}
}

std::optional<std::string> Renderer::TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept
{
	auto failure = _bgfxCallback->failures.Take(filepath.string());
	if (failure.has_value())
	{
		_bgfxCallback->pictures.Forget(filepath.string());
	}
	return failure;
}

void Renderer::RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept
{
	_bgfxCallback->pictures.Ask(name, std::move(picturer));
	RequestScreenshot(name);
}

std::optional<std::vector<uint8_t>> Renderer::TakeScreenPicture(const std::string& name) noexcept
{
	return _bgfxCallback->pictures.Take(name);
}
