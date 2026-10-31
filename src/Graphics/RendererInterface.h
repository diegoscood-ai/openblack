/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "GraphicsHandle.h"
#include "InstanceDesc.h"
#include "InterfaceCreatures.h"
#include "RenderModes.h"
#include "RenderPass.h"
#include "ScreenshotCapture.h"
#include "SeaPass.h"

#include "../EngineConfig.h"

namespace openblack
{
class Camera;
class Profiler;
class Ocean;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
}

namespace openblack::help
{
enum class TextFont : uint8_t;
}

namespace bgfx
{
struct InstanceDataBuffer;
}

namespace openblack::graphics::creature_draw
{
struct MorphTargets;
}

namespace openblack::graphics
{
class L3DMesh;
class FrameBuffer;
class Texture2D;
class ShaderManager;
class ShaderProgram;
class Texture2D;
struct ObjectListFrame;
struct OverlayFrame;
struct SuperVillagerFrame;

/// The colour the game's screen is cleared to (RGBA): opaque black, as the original's background colour, which nothing
/// changes. (openblack) it is cleared every frame, where the original clears only on some (docs/wiki/rendering/frame.md)
inline constexpr uint32_t k_BackgroundColour = 0x000000ff;

class RendererInterface
{
public:
	/// The game clocks of the frame the draw reads (openblack engine), taken with the camera's copy where the desc is
	/// made (Game::Run, at the end of the frame's logic). (pending) the real-time reads (the sea's
	/// game_clock::EngineMs, DrawMoon's date and tick reads for sky_frame::TakeMoonPhase) are still made live in the
	/// draw, as before
	struct DrawClock
	{
		uint32_t frameGameMs; ///< game_clock::FrameGameMs: the game time step, whole ms, 0 paused
		uint32_t turn;        ///< game_clock::Turn
		float turnFraction;   ///< game_clock::TurnFraction
		bool paused;          ///< game_clock::IsPaused (no draw reader yet: the tooltip's)
	};

	/// The hand of the frame the draw reads, taken with the camera's copy and DrawClock
	struct DrawHand
	{
		std::optional<glm::vec3> position; ///< HandSystem::GetHandMatrix's translation (none: no hand system)
	};

	/// What the game's interface draws inside the end of the frame, in the original's order there: its fight panel after
	/// the help text, its creature status panel after the input prompt icons, both before the film and the screen fade.
	/// Each runs on the thread that draws the scene, as the interface's own draw does; none draws nothing
	struct InterfaceLayers
	{
		std::function<void()> fightPanel;
		/// The scripts' words on the screen, after the fight's panel
		std::function<void()> scriptTexts;
		std::function<void()> creaturePanel;
	};

	struct DrawSceneDesc
	{
		/// The draw's copy of the camera (Camera::CopyViewTo), not Locator::camera
		const Camera* camera;
		const graphics::FrameBuffer* frameBuffer;
		const ecs::Registry& entities;
		/// What the end of the frame draws over the scene (Graphics/OverlayFrame.h), filled before the draw
		const graphics::OverlayFrame& overlay;
		/// What the SuperVillagers' draw reads (Graphics/SuperVillagerFrame.h), filled before the draw
		const graphics::SuperVillagerFrame& superVillagers;
		/// What the object draw list leaves out of this frame (Graphics/ObjectListFrame.h), filled before the draw
		const graphics::ObjectListFrame& objectList;
		uint32_t time;
		float timeOfDay;
		DrawClock clock;
		DrawHand hand;
		float bumpMapStrength;
		float smallBumpMapStrength;
		graphics::RenderPass viewId;
		bool drawSky;
		bool drawWater;
		bool drawIsland;
		bool drawEntities;
		bool drawSprites;
		bool drawBoundingBoxes;
		bool wireframe;
		InterfaceLayers interfaceLayers {};
	};

	struct L3DMeshSubmitDesc
	{
		graphics::RenderPass viewId;
		const graphics::ShaderProgram* program;
		/// What the draw adds to every primitive's mode (render_modes::State): Z func, alpha written, MSAA, a cull for the
		/// whole mesh (the sky, the hand's shadow); without one, each primitive's material culls
		render_modes::StateOptions options;
		/// The render mode table: GlobalAlpha for an object drawn with its own alpha
		render_modes::Table table {render_modes::Table::Normal};
		/// with the GlobalAlpha table: the object's alpha byte (the diffuse colour >> 24), the ALPHAREF of modes 9 / 15
		uint8_t globalAlpha {255};
		/// the ALPHAREF every alpha tested primitive takes instead of its material's (render_modes::AlphaRef `forced`): a
		/// burning tree's draw
		std::optional<uint8_t> forcedAlphaRef;
		/// A texture every primitive is drawn with instead of its own skin (an object's ghost's mask, cool_effect)
		const Texture2D* textureOverride {nullptr};
		/// every primitive drawn in this mode instead of its own, alpha test included (the particle additive atoms: 13;
		/// the hand's shadow on the objects: 6)
		std::optional<render_modes::Mode> mode;
		uint32_t rgba;
		const glm::mat4* modelMatrices;
		uint8_t matrixCount;
		std::unique_ptr<const graphics::InstanceDesc> instanceDesc;
		uint32_t instanceStart;
		uint32_t instanceCount;
		bool isSky;
		bool drawAll; ///< For use in the mesh viewer
		bool morphWithTerrain;
		float lightBoost {1.0f};   ///< model colour multiplier (the hand: 1.5)
		bool noHaze {false};       ///< no distance haze (the hand's draw never applies it)
		uint8_t landLightMode {0}; ///< land_light::ObjectMode: how the model takes the land light (vs_object)
		bool landLightHaze {true}; ///< the haze after that mode; both are for the rows with no light of their own
		uint8_t blendFilter {0};   ///< 0: every primitive, 1: the opaque ones only, 2: the blended ones only
		/// The draw of the parts an object's status adds to its model (graphics::mesh_status::Drawn): the submeshes with
		/// a status from 1 to this one; without it, the common draw of the status 0 submeshes
		std::optional<uint8_t> statusParts;
		/// The snow lying where the object stands shows on it (snow_cover): the world's objects in the main view, not the hand
		bool snow {false};
		/// A draw of the pass under the sea (graphics::sea_pass): DrawUnderWater in a constant or the last Draw's
		/// colour, DrawCutByPlane, the plane kept and whether it is mirrored back
		sea_pass::SeaDraw sea {};
		/// A projected shadow drawn on the object (programs *ShadowInstanced, shadow_list): its texture, its box
		/// (x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)) and its cull values (d.x, d.z, the least k)
		std::optional<graphics::TextureHandle> dynamicShadow;
		glm::vec4 dynamicShadowBox {0.0f};
		glm::vec4 dynamicShadowCull {0.0f};

		// The temple's (Graphics/RendererTemple.cpp): the world's draws leave them as they are and never read them
		/// The table of joints the submeshes with a joint turn by about their pivots, as the temple's doors do
		std::span<const glm::mat4> joints {};
		/// The temple's light: what every colour is multiplied by, and what is added after
		glm::vec3 lightMultiply {1.0f};
		glm::vec3 lightAdd {0.0f};
		/// How far back the mesh is pushed towards the far plane, as a fraction of its depth
		float depthBias {0.0f};
		/// The program the submeshes with a lightmap are drawn with, through it
		const graphics::ShaderProgram* lightmapProgram {nullptr};

		// A creature's (Graphics/CreatureDraw.h, RendererCreature.cpp): the world's other draws leave them as they are
		/// The meshes the body's shape is blended towards, read as the second to fourth vertex streams by the program
		/// "ObjectMorphInstanced"
		const graphics::creature_draw::MorphTargets* morphTargets {nullptr};
		/// The creature whose painted skins take the place of the mesh's own
		entt::entity paintedSkins {entt::null};
		/// A row of the instances made for this draw alone, in place of instanceDesc's (a creature's eyelid, tinted)
		const bgfx::InstanceDataBuffer* transientInstance {nullptr};
		/// A creature fizzing out of sight is drawn twice through the static (creature_draw::FizzPasses), with a program
		/// that has the creature spells' looks: first its depth alone, where the static over its skin reaches the level;
		/// then its body blended at what the level leaves, over just that depth
		struct Fizz
		{
			/// u_creatureSpellLook of the pass (creature_draw::FizzPass::spellLook)
			glm::vec4 spellLook {0.0f};
			/// The static's alpha, which the depth is tested against
			const Texture2D* staticAlpha {nullptr};
		};
		std::optional<Fizz> fizz;

		// The advisors' (RendererSpirits.cpp): the world's other draws leave it as it is
		/// u_advisorPupil0, u_advisorPupil1 and u_advisorPupilCentre, set on every primitive of a program that has them
		/// (vs_object's USE_ADVISOR_PUPILS)
		std::optional<std::array<glm::vec4, 3>> advisorPupils;
		/// The frame's bone palette (graphics::bone_palette): the skinned sub-meshes take each instance's bones from it,
		/// where its instance says they start, rather than from modelMatrices ("ObjectPaletteInstanced")
		const graphics::Texture2D* bonePalette {nullptr};
	};

	static std::unique_ptr<RendererInterface> Create(GraphicsBackend backend, bool vsync) noexcept;

	virtual ~RendererInterface() noexcept = default;

	virtual void ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept = 0;
	virtual void Reset(glm::u16vec2 resolution) const noexcept = 0;
	/// Switches the wait for the screen's refresh on or off at once, the back buffer keeping its size and other flags
	virtual void SetVsync(bool vsync) noexcept = 0;
	/// (openblack debug) Switches four-sample multisampling of the back buffer on or off at once, the back buffer keeping
	/// its size and other flags; off, as the original draws
	virtual void SetMultisampling(bool multisampling) noexcept = 0;
	/// A new game in the same process: what the draw keeps from one frame to the next goes back to how a new renderer
	/// has it (the sea's drift, the sun's glare, the sky's alignment, the clouds' alpha, the painted skins, the shadows,
	/// the snow's and the land light's uploads, the input prompts, the temple's map and the film's frame); the shaders,
	/// the fonts and the caches of what never changes stay
	virtual void ResetForNewGame() noexcept = 0;
	/// (openblack engine) a new land: what the draw would load lazily (the chimney smoke's alpha)
	virtual void PreloadForLand() const noexcept = 0;
	/// (openblack engine) the frame's writes of the draw, on the logic side, just before DrawScene, which then only
	/// reads: the sky type, the dome, the land light table, the frame's light, the dynamic shadows, the clouds (and
	/// their collection), the haze, the foliage, the PSys surface creators, the mists, the chimney smoke, the rain and
	/// the influence border
	virtual void PreDraw(const DrawSceneDesc& drawDesc) const noexcept = 0;
	virtual void DrawScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
	/// The size of the Main view (ConfigureView), what the overlays are laid out for; 0 x 0 before it.
	/// Called from the logic side (the OverlayFrame filled before the draw): read-only, no bgfx calls
	[[nodiscard]] virtual glm::u16vec2 GetResolution() const noexcept = 0;
	/// The string width of a HelpText font (f1 / f3 fall back to j0): the width
	/// of the text at that size, 0 without the font. For the layouts made before the draw (Graphics/OverlayFrame.h).
	/// Called from the logic side: read-only, no bgfx calls; the font's first-use load (GameFontAt) must move before
	/// the game or go through gpu::Submit (pending)
	[[nodiscard]] virtual float MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept = 0;
	virtual void Frame() noexcept = 0;
	/// Asks for the frame being made to be written to a PNG once drawn; the file appears whole once written
	virtual void RequestScreenshot(const std::filesystem::path& filepath) noexcept = 0;
	/// Why a picture asked for was given up, once; none while it may still come
	[[nodiscard]] virtual std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept = 0;
	/// Asks for a picture of the frame that the game makes now, kept in memory under a name. The name must start with
	/// graphics::ScreenPictures::k_Prefix. The renderer calls picturer on its own thread when it reads the frame.
	virtual void RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept = 0;
	/// The picture made for a name, one time; none while it is not made
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& name) noexcept = 0;
	/// Keeps the frame that the game makes now aside once it is drawn (graphics::ShownFrameTargets), so that a picture can
	/// be of it while it is on the screen: its screen passes draw into a target, which is copied to the window pixel for
	/// pixel before the debug windows. Nothing is read back unless a picture is asked. Without the read-backs, or with
	/// the window multisampled, the frame is drawn to the window as before and not kept.
	virtual void KeepShownFrame() noexcept = 0;
	/// Asks for a picture (made by picturer) of the frame on the screen, the last one handed over, when it was kept: the
	/// name it comes back under through TakeScreenPicture (pictures asked of one frame share it); none if it was not
	[[nodiscard]] virtual std::optional<std::string> AskShownFramePicture(graphics::ScreenPicturer picturer) noexcept = 0;
	/// Whether the frame on the screen is kept, and the last frame kept (a debug readout)
	[[nodiscard]] virtual bool ShownFrameKept() const noexcept = 0;
	[[nodiscard]] virtual std::optional<uint32_t> LastShownFrameKept() const noexcept = 0;
	[[nodiscard]] virtual bool GetDebug() const noexcept = 0;
	virtual void SetDebug(bool value) noexcept = 0;
	[[nodiscard]] virtual bool GetProfile() const noexcept = 0;
	virtual void SetProfile(bool value) noexcept = 0;

	// TODO: Remove this function. All renderables should be drawn through RenderingSystem with Components
	virtual void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept = 0;
	// TODO: Should shader manager be available through Locator as a service?
	[[nodiscard]] virtual graphics::ShaderManager& GetShaderManager() const noexcept = 0;

	/// The creatures that the interface shows over itself. The interface asks for them each frame that it shows them,
	/// as it draws itself, and the next frame of the scene draws them.
	virtual void ShowInterfaceCreatures([[maybe_unused]] interface_creatures::Request request) noexcept {}
};

} // namespace openblack::graphics
