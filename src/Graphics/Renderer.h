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
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "3D/Clouds.h"
#include "3D/Rain.h"
#include "3D/SkyDome.h"
#include "3D/Snowfall.h"
#include "3D/TempleLight.h"
#include "3D/TempleMap.h"
#include "Creature/CreatureHandGlow.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/GameFont.h"
#include "Graphics/Globes.h"
#include "Graphics/HandGlow.h"
#include "Graphics/Haze.h"
#include "Graphics/LeashPostDraw.h"
#include "Graphics/LightBeams.h"
#include "Graphics/LightColumn.h"
#include "Graphics/MeshUniforms.h"
#include "Graphics/Mists.h"
#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SeaRows.h"
#include "Graphics/UniqueHandle.h"
#include "Graphics/VolumeGlowShell.h"
#include "Graphics/VortexDraw.h"
#include "Graphics/WorldTriangles.h"
#include "Graphics/ZSort.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleManager.h"
#include "Particles/Rules/SurfRevol.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
struct BgfxCallback;
class LandBlock;
class LandLightPalette;
class LandLightTable;
class Clouds;
class Game;

namespace help
{
enum class TextFont : uint8_t;
}

namespace ecs
{
class Registry;
}

namespace magic::tribal_spin
{
class Runner;
}

namespace screen_point
{
struct ClipToWorld; // Camera/ScreenPoint.h
}

namespace graphics
{
struct BeamMesh;
class FrameBuffer;
class L3DSubMesh;
class OffscreenScreenshots;
namespace shadow_list
{
class List;
struct ShadowInfo;
} // namespace shadow_list
class Mesh;
struct SpiritQuadVertex;  // Graphics/OverlayFrame.h
struct IntroLightOverlay; // Graphics/OverlayFrame.h
struct GestureIconOverlay;      // Graphics/OverlayFrame.h
struct GestureIconModelOverlay; // Graphics/OverlayFrame.h
struct GestureIconEffect;       // Graphics/GestureIconEffect.h
struct InputPromptFrame;  // Graphics/InputPromptFrame.h
struct ZObject;           // Graphics/ZObject.h

class Renderer final: public RendererInterface
{
	/// Rebuilds the landscape light table for this frame and uploads it (256x1 RGBA8, point sampled)
	void UpdateLandLight() const;
	/// The sky dome's colours of this frame and the overcast the sun and the moon show through, from the inputs of the
	/// land light table just built
	void UpdateSkyTint() const;
	/// The sky shader's alignments, colours and dome texture of this frame
	void SetSkyUniforms(const ShaderProgram& skyShader) const;
	/// The projected shadows of this frame (shadow_list::List::Frame; RendererShadows.cpp)
	void UpdateShadows(const DrawSceneDesc& drawDesc) const;
	/// The projected shadows over one land block just drawn (RendererShadows.cpp)
	void DrawLandShadows(RenderPass viewId, const LandBlock& block, uint64_t cull) const;
	/// The projected shadows that fall on objects too (the hand, the boat): which objects of the main view are in which
	/// shadow's box this frame (_shadowReceivers; RendererShadows.cpp). Cleared, and left empty, for the other views
	void CollectShadowReceivers(bool mainView) const;
	/// The shadows over one object, right after its own draw, as the tail loop of the objects' Draw does: in the main
	/// view for an object drawn at once, in the queue's view inside its Z object for one drawn from the Z-sorter.
	/// `matrices` are the ones the object was drawn with
	void DrawShadowsOnObject(RenderPass viewId, uint32_t instance, const glm::mat4* matrices, uint8_t matrixCount) const;
	/// The shadows over the sharks' parts above the water (`cut`, the instances DrawCutAboveWater has just drawn), which
	/// the per-mesh loop leaves out. The PSys mesh atoms never receive (RenderingSystem's ReceivesDynamicShadow)
	void DrawShadowsOnCutObjects(RenderPass viewId, const std::unordered_set<uint32_t>& cut) const;
	/// The receivers left at the end of the frame's objects were not drawn: none of them gets a shadow
	void ClearShadowReceivers() const { _shadowReceivers.clear(); }
	/// Particle sprites in the order given, consecutive ones with the same material in one call (RendererParticles.cpp): a
	/// Sorted effect's sprite (or a run of them, each its own Z object), or the sprites of a Queued / Immediate effect
	/// between its other items
	void DrawParticleSprites(std::span<const particles::Effect::DrawAtom> atoms, const Camera& camera, RenderPass viewId) const;
	/// The same facing a camera of their own, every corner put through `place` before it is drawn (the gesture icons'
	/// holder effects, drawn with the models' camera and moved on the screen)
	void DrawParticleSprites(std::span<const particles::Effect::DrawAtom> atoms, const billboard::CameraFrame& cameraFrame,
	                         RenderPass viewId, const std::function<glm::vec3(const glm::vec3&)>& place) const;
	/// (openblack) the ids of "raw/<texture>" and "raw/<texture>a" (entt::hashed_string), made once per name: the PSys
	/// draws asked them per run, building two strings each time
	[[nodiscard]] std::pair<entt::id_type, entt::id_type> RawTextureIds(const std::string& texture) const;
	/// (openblack) the position / uv / colour layout of the PSys world quads, built once
	[[nodiscard]] static const bgfx::VertexLayout& ParticleQuadLayout();
	/// One chain ribbon (RendererChain.cpp): from its own Z object for a Sorted effect (keyed by the joint n / 2), else
	/// inside its effect
	void DrawParticleChain(RenderPass viewId, const Camera& camera, const particles::Effect::DrawChain& chain) const;
	/// One surface of revolution (the teleport pool, the dispensers' discs; RendererRevolvedSurface.cpp), never queued on its
	/// own: at once for a Sorted effect, inside its effect otherwise. A sheet of light is a surface too, drawn from its own
	/// Z object
	void DrawParticleSurface(RenderPass viewId, const particles::surf_revol::Surface& surface) const;
	/// The exploded pieces of a model (Kind::MeshPiece) tagged with `atom`, or all of `pieces` when it is null: their world
	/// triangles drawn at once, with no Z object of their own (RendererParticles.cpp)
	void DrawParticleFragment(RenderPass viewId, const world_triangles::Frame& pieces, const particles::Atom* atom) const;
	/// A glowing shell over an object's model (an artefact's aura), by the object as it is drawn this frame: nothing while
	/// it isn't (RendererParticles.cpp)
	void DrawParticleShell(RenderPass viewId, const Camera& camera, const particles::draw::ShellDraw& shell) const;
	/// The band of a glowing shell over a part of a model, made when first drawn; nullptr for a part without triangles
	[[nodiscard]] const VolumeGlowShell* GlowShellOf(entt::id_type mesh, uint32_t subMesh) const;
	/// A glowing shell over a model of the game's own at a matrix (a creature's creed glow, the citadel heart's shell),
	/// reaching `reach` out along the normals in the model's units, in the look's colour and texture, with `addedRgb`
	/// (0xRRGGBB) added after the texture at the inner edge (RendererParticles.cpp)
	void DrawGlowShellAt(RenderPass viewId, const Camera& camera, entt::id_type meshId, const glm::mat4& model, float reach,
	                     const particles::draw::ShellDraw& look, uint32_t addedRgb = 0) const;
	/// One creature's creed glow from its Z object (_frameCreedGlows), its shell over the creed model (RendererParticles.cpp)
	void DrawCreedGlow(RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// A creed glow where its body's draw placed it: its shell over the creed model (RendererParticles.cpp)
	void DrawCreedGlowAt(RenderPass viewId, const Camera& camera, const creature_hand_glow::Placed& glow) const;
	/// The falling film's copy's hand glows as its draw queues them, in their places' order: those on the screen and near
	/// enough to the fall's eye, each with its key ((x^2 + y^2) + z^2 from the eye) (RendererParticles.cpp)
	[[nodiscard]] std::vector<std::pair<float, creature_hand_glow::Placed>>
	FallingCreatureGlows(const Camera& camera, entt::entity copy, glm::vec3 eye) const;
	/// The falling film's end of frame queue, in the FinishFrameQueue view with the fall's camera: the copy's hand glows
	/// and the sparks together, far to near (RendererFallingSpell.cpp)
	void DrawFallingFrameQueue(const Camera& camera) const;
	/// The end of PreDraw: _particleFrame for the main pass, the sheets of light built in it
	void CollectFrameParticles(const DrawSceneDesc& drawDesc) const;
	/// The start of PreDraw: _objectListRows from the frame's DrawSceneDesc::objectList, through the entity rows
	void BuildObjectListRows(const DrawSceneDesc& drawDesc) const;
	/// The instance rows (RenderContext::instanceUniforms) the object draw list leaves out of this frame: 0 left out,
	/// 1 drawn, and every row past the end drawn (empty: all of them). DrawPass skips the rows left out
	mutable std::vector<uint8_t> _objectListRows;
	/// The raining tiles (RainSystemInterface::TakeTiles), each one Z object of its own with its Z-sorter key and its index
	/// in _frameRain (RendererRain.cpp)
	std::vector<std::pair<float, uint32_t>> CollectRain(const Camera& camera) const;
	/// One tile of _frameRain, as the Z-sorter's callback: its streaks
	void DrawRainTile(RenderPass viewId, uint32_t index) const;
	/// The tiles of this frame, filled by CollectRain
	mutable std::vector<rain::Tile> _frameRain;
	/// The snowing land blocks (snowfallSystem::TakeTiles), each one Z object of its own with its Z-sorter key and its
	/// index in _frameSnow (RendererSnowfall.cpp); none where it does not snow
	std::vector<std::pair<float, uint32_t>> CollectSnow(const Camera& camera) const;
	/// One tile of _frameSnow, as the Z-sorter's callback: its flakes
	void DrawSnowTile(RenderPass viewId, uint32_t index) const;
	/// The snow's tiles of this frame, filled by CollectSnow
	mutable std::vector<snowfall::Tile> _frameSnow;
	/// The sun (right after the sky dome) and its glare (at the end of the frame), eased by the frame's game time step
	/// (DrawClock::frameGameMs)
	void DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare, uint32_t frameGameMs) const;
	/// Asks the drawing of a glare sample's pixel, in a view after the scene, whether anything is drawn there nearer the
	/// camera than a depth; the answer comes a frame or two later
	void AskGlareSampleDrawn(graphics::RenderPass viewId, const Camera& camera, bgfx::OcclusionQueryHandle query,
	                         glm::ivec2 pixel, float depth) const;
	/// The moon and its glow
	/// In the reflection pass (sea_pass::ForPass(viewId).mirrored): the mirrored glow and the moon's DrawUnderWater
	void DrawMoon(graphics::RenderPass viewId, const Camera& camera) const;
	/// The sea: its screen rows, or the level-0 quad (RendererSea.cpp)
	void DrawSea(const DrawSceneDesc& desc) const;
	/// The reflection target follows the main view's size (RendererSea.cpp)
	void UpdateReflectionTarget() const;
	/// The hand's glow on the water at night, into the reflection target (RendererSea.cpp)
	void DrawHandWaterGlow(graphics::RenderPass viewId) const;
	/// The sky clouds are mists: the ones on screen are queued on the Z-sorter. Their Z-sorter keys and indices, and the
	/// counters of those advanced
	std::vector<std::pair<float, uint32_t>> CollectClouds(const Camera& camera) const;
	/// One cloud, as the Z-sorter's callback (the mist draw, effect branch)
	void DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The mists on screen (the map's and the ones of mists::Submit) with their Z-sorter key and their index in
	/// _frameMists; the map mists' counters advanced (once per frame)
	std::vector<std::pair<float, uint32_t>> CollectMists(const Camera& camera) const;
	/// One mist of _frameMists, as the Z-sorter's callback
	void DrawMist(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// One mist
	void DrawMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const;
	/// The mist of a Queued or Immediate effect, drawn at once inside its effect (the screen test, then the draw); not
	/// kept in _frameMists
	void DrawEffectMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const;
	/// The mists of this frame, filled by CollectMists
	mutable std::vector<mists::MistDesc> _frameMists;
	/// Every Abode with a chimney on screen: the smoke's state and puffs advanced (the smoke simulates while it draws),
	/// its Z-sorter key and its index in _frameSmoke (RendererSmoke.cpp)
	std::vector<std::pair<float, uint32_t>> CollectChimneySmoke(const Camera& camera) const;
	/// One smoke of _frameSmoke: its visible puffs in their order 0..9 (smoke material)
	void DrawChimneySmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// One puff of a smoke drawn this frame, as the sprite draw takes it
	struct DrawnPuff
	{
		glm::vec3 position;
		/// The sprite's half width, in world units at the puff's depth
		float halfWidth;
		/// Its turn in the plane of the screen
		float angle;
		/// Its cell of the smoke texture, 8 to a row
		uint32_t cell;
		uint32_t argb;
	};
	/// The puffs of every smoke of this frame, filled by CollectChimneySmoke
	mutable std::vector<std::vector<DrawnPuff>> _frameSmoke;
	/// The mirrored held object and thrown objects in the reflection (DrawUnderWater)
	void DrawObjectReflections(graphics::RenderPass viewId, bool handReflected) const;
	/// The local player's creature mirrored in the sea, in its drawn pose, shape and skins, unlit in colour 0x65A0A0D0
	/// (sea_pass::ReflectsCreature), with nothing it carries, no eyes, hair or fizz (RendererCut.cpp)
	void DrawCreatureReflection(graphics::RenderPass viewId) const;
	/// The missionaries' boat hull in the reflection (DrawUnderWater in colour 0xFF303070), and the boat's sprites: the
	/// wake and the DisappearSmoke puffs, smoke material mode 6 (RendererBoat.cpp)
	void DrawBoatReflection(graphics::RenderPass viewId) const;
	/// Each of the boat's sprites (the wake and the puffs) goes to the Z-sorter on its own: their keys and indices in
	/// _frameBoatSprites
	std::vector<std::pair<float, uint32_t>> CollectBoatSprites(const Camera& camera) const;
	/// One sprite of _frameBoatSprites, as the Z-sorter's callback
	void DrawBoatSprite(graphics::RenderPass viewId, uint32_t index) const;
	/// The landscape vortices' depth boxes, after the sea and before the land (RendererVortex.cpp)
	void DrawVortexBoxes(graphics::RenderPass viewId) const;
	/// At the end of the frame's PSys collects: the surfaces of the shown vortices' ground effects leave the at-once
	/// draw for _particleFrame.groundSurfaces, and the vortices' effects that do not show leave the frame
	void SplitVortexParticles() const;
	/// The shown vortices' ground effects' surfaces, right after the depth boxes and before the land
	void DrawVortexGroundEffects(graphics::RenderPass viewId) const;
	/// A vortex's decal on its land block this frame (RendererVortex.cpp): the block, the decal's texture coordinates,
	/// the hole's alpha reference and the textures (the mask's alpha, the ring and its alpha)
	struct VortexDecalDraw
	{
		glm::ivec2 block;
		graphics::vortex_draw::DecalUv uv;
		uint8_t alphaRef;
		const Texture2D* mask;
		const Texture2D* ring;
		const Texture2D* ringAlpha;
	};
	/// The decals of the vortices whose decal is made, one per block (the first vortex found holds it)
	[[nodiscard]] std::vector<VortexDecalDraw> CollectVortexDecals() const;
	/// Before the block's own draw: its depth, written only where the decal's mask reaches the alpha reference. The
	/// block is then drawn with an equal depth test, so it shows only there: a hole elsewhere
	void DrawVortexDecalMask(graphics::RenderPass viewId, const VortexDecalDraw& decal, const LandBlock& block,
	                         uint64_t cull) const;
	/// After the block's own draw: the ring over it with an equal depth test, in the land's light
	void DrawVortexDecalRing(graphics::RenderPass viewId, const VortexDecalDraw& decal, const LandBlock& block,
	                         uint64_t cull) const;
	/// The intro light's Z object (RendererIntroLight.cpp): the sprites its callback draws
	void DrawIntroLight(graphics::RenderPass viewId, const Camera& camera, const IntroLightOverlay& light) const;
	/// One gesture icon (RendererGestureIcons.cpp)
	void DrawGestureIcon(graphics::RenderPass viewId, const Camera& camera, const GestureIconOverlay& icon) const;
	/// Before a gesture icon's model, the depth of the box above it reset (RendererGestureIcons.cpp); the texture is
	/// only bound, mode 18 writes no colour
	void DrawGestureIconDepthBox(graphics::RenderPass viewId, const Camera& camera, const GestureIconOverlay& icon,
	                             const Texture2D& anyTexture) const;
	/// One draw of a gesture icon's model, with the models' camera, moved on the screen by the offset
	/// (RendererGestureIcons.cpp)
	void DrawGestureIconModel(graphics::RenderPass viewId, const Camera& camera, const GestureIconModelOverlay& model,
	                          glm::vec2 screenOffset) const;
	/// A seed graphic's holder effect inside a gesture icon: its sprites facing the models' camera, moved on the screen
	/// by the offset (RendererGestureIcons.cpp)
	void DrawGestureIconEffect(graphics::RenderPass viewId, const Camera& camera, const GestureIconEffect& effect,
	                           glm::vec2 screenOffset) const;
	/// A boat sprite of this frame: its quad (none at or before the near plane) and its colour
	struct BoatSpriteDraw
	{
		std::optional<billboard::Quad> quad;
		uint32_t argb {0};
	};
	mutable std::vector<BoatSpriteDraw> _frameBoatSprites;
	/// The physics' dust puffs of this pass (DynamicsSystemInterface::SnapshotDust), refilled every pass
	mutable std::vector<ecs::physics::DustParticleDraw> _frameDust;
	/// The shield domes of this frame (MagicShieldSystemInterface::GetDomes), refilled every main pass; empty today, the
	/// domes being map objects
	mutable std::vector<ecs::systems::MagicShieldSystemInterface::DomeDraw> _frameShieldDomes;
	/// The one-shot globes' draws of this frame (globes::GlobeDraw), read by DrawGlobes. Nothing collects one: the globes
	/// are entities, drawn and queued with the models
	mutable std::vector<globes::GlobeDraw> _frameGlobes;
	/// The broken buildings' fragments of this frame (world_triangles), refilled every frame
	mutable world_triangles::Frame _frameDamageMeshes;
	/// The sea's scroll offsets and its frame counter (0..15), kept between frames (RendererSea.cpp)
	mutable sea::Drift _seaDrift;
	mutable uint32_t _seaFrame {0};
	/// A 1 x 1 white texture for the surfaces of revolution's specular pass, made on first use
	[[nodiscard]] const Texture2D& RevolvedSurfaceWhiteTexture() const;
	mutable std::unique_ptr<Texture2D> _revolvedSurfaceWhite;
	/// How deep the snow lies over the island (snow_cover), as a texture the shaders read point by point: made on first
	/// use and refreshed only when the snow changes; null without the snow system
	[[nodiscard]] const Texture2D* SnowDepthTexture() const;
	mutable std::unique_ptr<Texture2D> _snowDepth;
	mutable std::optional<uint32_t> _snowRevision;
	/// The creatures drawn this frame and what they are drawn with (RendererCreature.cpp, Graphics/CreatureDraw.h):
	/// collected at the start of DrawScene, empty on a land without a creature
	void CollectCreatures() const;
	/// The creatures' painted skins brought up to date (CollectCreatures, and the falling spell's film)
	void UpdateCreatureSkins(const ecs::Registry& registry) const;
	mutable std::vector<creature_draw::Body> _frameCreatures;
	/// Whether a creature is drawn with any row of [offset, offset + count)
	[[nodiscard]] bool HasCreature(uint32_t offset, uint32_t count) const;
	/// A creature's body: its shape (the morphing program and its targets) and its painted skins
	void UseCreatureBody(const creature_draw::Body& body, L3DMeshSubmitDesc& desc) const;
	/// A creature's body as its fizz leaves it: as it is in full sight, nothing of it right out of sight, and in
	/// between through the static (DrawFizzed), whose slide across its skin moves on at each such draw in the world's
	/// view by the frame's game time
	void DrawCreatureBody(const L3DMesh& mesh, L3DMeshSubmitDesc& desc, const creature_draw::Body& body,
	                      uint32_t frameGameMs) const;
	/// A mesh fizzing out of sight at a level, as a byte: its depth alone where the static slid by `scroll` reaches the
	/// level, back faces culled, then its body blended at what the level leaves of its alpha, through the global alpha
	/// table, only where that depth lies. Drawn as it is when its program has no fizzing variant or the static is not
	/// loaded
	void DrawFizzed(const L3DMesh& mesh, L3DMeshSubmitDesc& desc, uint8_t level, glm::vec2 scroll) const;
	/// The variant of a creature's program that draws it through the static, if it has one
	[[nodiscard]] const ShaderProgram* SpellLookVariant(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _spellLookVariants;
	/// The static's alpha (raw/S_Statica), which the fizzed depth is tested against; null when it is not loaded
	[[nodiscard]] const Texture2D* StaticAlphaTexture() const;
	/// How far each creature's static has slid across its skin, from where it was made; only moved while it is drawn
	/// fizzing. A creature gone takes its entry with it
	mutable std::unordered_map<entt::entity, glm::vec2> _creatureStatic;
	/// How far the one static the creatures' eyes fizz through has slid, as CollectCreatures read it this frame
	mutable glm::vec2 _frameEyeStatic {0.0f};
	/// The second to fourth vertex streams of a creature's submesh: its matches in the meshes the body is blended
	/// towards, the submesh's own where a match differs
	void BindMorphTargets(const L3DMesh& mesh, const L3DSubMesh& subMesh, const creature_draw::MorphTargets& targets) const;
	/// The blend sources of a creature's submesh and of its matches in the meshes the body is blended towards, for the
	/// vertex shader to blend the seams by (see vertex_blend); the submesh's own where a match differs. Off where the
	/// submesh has none
	void BindBlendSources(const L3DMesh& mesh, const L3DSubMesh& subMesh, const creature_draw::MorphTargets& targets,
	                      const ShaderProgram& program) const;
	/// The texture of a primitive's skin: the creature's painted one, else the mesh's (world_triangles::PrimitiveTexture)
	[[nodiscard]] const Texture2D* SkinTexture(const L3DMesh& mesh, uint32_t skinId, entt::entity paintedSkins) const;
	/// A creature's hair and then its eyes, right after its body and the shadows over it
	void DrawCreatureParts(graphics::RenderPass viewId, const Camera& camera, const ecs::systems::RenderContext& context,
	                       const creature_draw::Body& body) const;
	/// Each creature's painted skins, in textures of their own
	mutable creature_draw::PaintedSkins<Texture2D> _creatureSkins;
	/// The layouts a creature's variant meshes are read with as the second to fourth vertex streams, made on first use
	mutable std::array<UniqueHandle<bgfx::VertexLayoutHandle>, 3> _morphStreamLayouts;
	/// The creatures' leashes (RendererLeash.cpp): each worn rope's shadow on the land, then the rope, at once in the world
	/// view, while the scripts show the leashes and outside the temple; nothing without a worn leash
	void DrawLeashes(graphics::RenderPass viewId, const Camera& camera) const;
	/// The temples' leash posts (RendererLeashPosts.cpp), once a frame from PreDraw outside the temple: every shown
	/// post's spin moves on by the frame's game time (in its components::LeashPost, which no state hash reads), then
	/// what the world view draws of it goes to _frameLeashPosts
	void CollectLeashPosts() const;
	/// The posts of this frame (CollectLeashPosts)
	mutable std::vector<leash_post_draw::Post> _frameLeashPosts;
	/// One post's smoke from its Z object: a sprite facing the screen in the smoke material, or the additive smoke for
	/// the picked one
	void DrawLeashPostSmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// One post's collar from its Z object: the collar mesh, lit as the world's objects, with its band's texture offset
	void DrawLeashPostCollar(graphics::RenderPass viewId, uint32_t index) const;
	/// The columns of light (RendererLightColumns.cpp, graphics::light_column), once a frame from PreDraw outside the
	/// temple: one over every village centre and every temple whose owner's influence border shows, into
	/// _frameLightColumns
	void CollectLightColumns(const DrawSceneDesc& drawDesc) const;
	/// The columns of this frame (CollectLightColumns)
	mutable std::vector<graphics::light_column::Draw> _frameLightColumns;
	/// One column from its Z object: the column mesh added over the scene in its owner's colour, at its strength
	void DrawLightColumn(graphics::RenderPass viewId, uint32_t index) const;
	/// The creed glows of the creatures drawn this frame (_frameCreatures), each where its creature's pose put it
	/// (components::CreatureHandGlows::placed), into _frameCreedGlows: those on screen and not too far away
	/// (glow_shell::TooFar) (RendererParticles.cpp)
	void CollectCreedGlows(const Camera& camera) const;
	/// The creed glows of the world view this frame (CollectCreedGlows), one Z object each
	mutable std::vector<creature_hand_glow::Placed> _frameCreedGlows;
	/// A citadel heart's shell as it is destroyed (heart_shell), for the heart's instance this frame
	struct HeartShellDraw
	{
		entt::entity heart {entt::null};
		glm::mat4 model {1.0f};
		/// How far the shell reaches out, in parts of the model's half diagonal
		float reach {0.0f};
		/// The heart's owner's colour, 0xAARRGGBB
		uint32_t argb {0};
		/// Its heart's creation stamp (components::Temple::creationStamp), which orders the hearts' draw
		uint32_t creationStamp {0};
	};
	/// The shells over the citadel hearts being destroyed (components::Temple::shellShown), at their hearts' rows, into
	/// _frameHeartShells, every heart's, drawn or not by the object draw list, and at the heart's drawn point once the
	/// heart itself is no longer drawn; the world view only (RendererParticles.cpp)
	void CollectHeartShells(const DrawSceneDesc& desc) const;
	/// One heart's shell, in the hearts' draw before the models: its texture slid on by the frame's game time first
	/// (RendererParticles.cpp)
	void DrawHeartShell(RenderPass viewId, const Camera& camera, const HeartShellDraw& shell, uint32_t frameGameMs) const;
	mutable std::vector<HeartShellDraw> _frameHeartShells;
	/// How far each heart's shell texture has slid, from 0 when its shell is first shown
	mutable std::unordered_map<entt::entity, glm::vec2> _heartShellSlides;
	/// One tribe's name spinning round the hand or rising from where a miracle was cast (magic::hand_fx), from its Z
	/// object in the world view, written as the temple's text is (RendererTemple.cpp)
	void DrawTribalPower(graphics::RenderPass viewId, const magic::tribal_spin::Runner& runner) const;
	/// The domes the shield service hands out (MagicShieldSystemInterface::GetDomes), each queued as a Z object of its
	/// own, keyed where it stands as the models are; in the world view only, into _frameShieldDomes. No bgfx call:
	/// nothing hands out a dome today, the domes are map objects queued with the models (RendererShields.cpp)
	void DrawShieldDomes(const DrawSceneDesc& desc, zsort::Queue<ZObject>& queue) const;
	/// The one-shot globes' draws of the frame (_frameGlobes), each queued as a Z object of its own, keyed at its sort
	/// point as the models are; in the world view only, after the tribes' names. No bgfx call: the list is empty, the
	/// globes being entities queued with the models (RendererMiracles.cpp)
	void DrawGlobes(const DrawSceneDesc& desc, zsort::Queue<ZObject>& queue) const;
	/// The hand's glow, drawn inside the hand's own Z object right after the shadows over it, in the world view only.
	/// Nothing hands it a glow (std::nullopt), so it returns before any bgfx call; the pass itself waits for a program
	/// with the flowing texture (RendererMiracles.cpp)
	void DrawHandGlow(graphics::RenderPass viewId, const std::optional<HandGlowDraw>& glow) const;
	/// Every frame of the world view, no option: every circle of influence::Circles() at once, after everything else
	/// drawn at once and before the Z-sorter's drain (RendererInfluence.cpp)
	void DrawInfluenceCircles(graphics::RenderPass viewId) const;
	/// The circles' writes before they draw (PreDraw): the camera gate, the scroll clock and the middle rows' alpha
	/// (influence::SetCurtainAlpha); the scroll goes to _preInfluenceScroll, none when the gate fails
	void UpdateInfluenceCurtain(const Camera& camera) const;
	/// The border's scroll clock (frame_anim::InfluenceScroll), a global of the original
	mutable int32_t _influenceScrollMs {0};
	/// One Z object for every ripple of influence::Ripples(): their Z-sorter keys and indices (RendererInfluence.cpp)
	std::vector<std::pair<float, uint32_t>> CollectInfluenceRipples(const Camera& camera) const;
	/// One ripple, as the Z-sorter's callback: its 7 sprites
	/// (`frameGameMs`: the frame's game time step, DrawClock)
	void DrawInfluenceRipple(graphics::RenderPass viewId, uint32_t index, uint32_t frameGameMs) const;
	/// A mesh drawn under the water: mirrored by the pass's camera, the part kept by sea.plane, in sea's light
	/// (sea_pass::UnderWater / UnderWaterLastDraw) (RendererCut.cpp)
	void DrawUnderWater(graphics::RenderPass viewId, const L3DMesh& mesh, std::unique_ptr<const InstanceDesc> instances,
	                    const glm::mat4* matrices, uint8_t matrixCount, bool morphWithTerrain,
	                    const sea_pass::SeaDraw& sea) const;
	/// DrawUnderWater of an entity's instance (its mesh's bones, or the identity)
	void DrawUnderWater(graphics::RenderPass viewId, entt::entity entity, const sea_pass::SeaDraw& sea) const;
	/// An entity's model cut by the plane (animated or static): the side of y = 0 the plane keeps, lit 90 + N.L in argb
	/// (0xAARRGGBB) + specular; mirrored back in the reflection target (sea_pass::Cut) (RendererCut.cpp)
	void DrawCutByPlane(graphics::RenderPass viewId, entt::entity entity, sea_pass::SeaPlane plane, uint32_t argb,
	                    uint32_t specular) const;
	/// The parts under the water of the objects with components::CutByPlane, before the sea
	void DrawCutBelowWater(graphics::RenderPass viewId) const;
	/// The parts above the water of the objects whose owner draws them cut (components::CutByPlane::drawAbove: the
	/// sharks), in the colour of the land light table[255]; the normal pass skips those instances
	void DrawCutAboveWater(graphics::RenderPass viewId) const;
	/// The same for the instances in `only`
	void DrawCutAboveWater(graphics::RenderPass viewId, const std::unordered_set<uint32_t>& only) const;
	/// the instance indices (RenderContext::entityInstances) whose owner draws them instead of the normal pass; with
	/// drawnAboveOnly, only those whose part above the water DrawCutAboveWater draws this frame
	[[nodiscard]] std::unordered_set<uint32_t> CutAboveInstances(bool drawnAboveOnly = false) const;
	/// The fish farm shoals (before the sea): misc0.raw sprites lying on the water, mode 6; drawn
	/// mirrored into the reflection target, which is what shows through the sea here
	void DrawFishShoals(graphics::RenderPass viewId) const;
	/// The fish puzzle's nets of floats (FishPlot), cut by the plane: KeepBelow the part under the water (into the
	/// reflection target, mirrored back), KeepAbove the part over it (RendererFishPlot.cpp)
	void DrawFishPlots(graphics::RenderPass viewId, sea_pass::SeaPlane plane) const;
	/// Data/Textures/smokea.raw for the chimney smoke (RendererSmoke.cpp)
	bool LoadChimneySmokeAlpha() const;
	/// The water rings (after the landscape): flat smoke.raw sprites, mode 13
	void DrawWaterRings(graphics::RenderPass viewId) const;
	/// The villagers' ground blobs ("human shadow")
	/// `lh`: the draw camera's W and its inverse (billboard::CameraFrame::clipMatrices), for the blobs' exact points;
	/// none under an owner `objectList` leaves out
	void DrawGroundBlobs(graphics::RenderPass viewId, const graphics::SuperVillagerFrame& superVillagers,
	                     const graphics::ObjectListFrame& objectList, const affine::CameraMatrices& lh) const;
	/// The end of the frame: `drawFade` false, the cinema bars when they are on, before the last finish-frame callbacks
	/// (the film); `drawFade` true, the screen fade after them, which draws the bars again over its colour.
	/// The colour and the bar of the frame's OverlayFrame
	void DrawScreenOverlay(const OverlayFrame& overlay, bool drawFade) const;
	/// DrawKeyOrMouse's `row`: the mouse row (0, 1, 2; 3 no icon) or a key code, or a key's name
	/// (the `wchar*` of a key with clickType 0)
	using KeyOrMouseRow = std::variant<int32_t, std::u16string_view>;
	/// A box of S pixels with the mouse button or the key cap, the text in j0 at 2S / 3, the panels (with alpha8 != 0)
	/// and the arrows (align 0x100..0x800), in the ScreenOverlay view. (x, y) is the
	/// box's left and vertical centre, or an offset from the hand with (align & 6) == 6. A null c1 is white, a null c2
	/// c1; a null text has no width and draws nothing
	void DrawKeyOrMouse(int32_t animType, int32_t clickType, KeyOrMouseRow row, std::optional<std::u16string_view> text,
	                    int32_t x, int32_t y, int32_t s, uint32_t align, const uint32_t* c1, const uint32_t* c2,
	                    int32_t alpha8) const;
	/// DrawKeyOrMouse for a caller outside the input prompt passes (the $m in the «Did you know?» bubble): in `view`, with the
	/// hand's point and the tick count of the frame's copy (OverlayFrame::inputPrompts.hand / tickCount); the input prompt
	/// passes' own state is put back afterwards. The side hysteresis (_keyOrMouseSide) is shared, as the original's one global
	void DrawKeyOrMouseIn(graphics::RenderPass view, const InputPromptFrame& frame, int32_t animType, int32_t clickType,
	                      KeyOrMouseRow row, std::optional<std::u16string_view> text, int32_t x, int32_t y, int32_t s,
	                      uint32_t align, const uint32_t* c1, const uint32_t* c2, int32_t alpha8) const;
	/// While a hand demo plays, its mouse and word by the camera's icons (OverlayFrame::handDemoMouse)
	void DrawHandDemoMouse(const OverlayFrame& overlay) const;
	/// The control pictures over the temple's scroll seen close up (OverlayFrame::templeScrollIcons)
	void DrawTempleScrollIcons(const OverlayFrame& overlay) const;
	/// The mouse picture of DrawKeyOrMouse, mousehelp.raw
	void DrawKeyOrMouseMouse(int32_t clickType, int32_t row, int32_t animType, int32_t x, int32_t y, int32_t s,
	                         uint32_t argb) const;
	/// One panel, the 9-slice of atmos.raw (drawn in the additive material)
	void AddKeyOrMousePanel(std::vector<SpiritQuadVertex>& out, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
	                        uint32_t argb) const;
	/// The draw half (DrawKeyOrMouse) for every InputPromptIcon of the frame's copy (OverlayFrame::inputPrompts), with its
	/// alpha of `pass`: 0 the call from the help system's 3D draw, 1 the one from HelpText's callback
	void DrawInputPrompts(const OverlayFrame& overlay, size_t pass) const;
	/// The draw's copies of OverlayFrame::inputPrompts' hand point (the hand projected on screen, pixels, y from the top; none
	/// when not projected) and tick count, set by DrawInputPrompts
	mutable std::optional<glm::vec2> _keyOrMouseHand;
	/// The view of the input prompt pass being drawn (pass 0 FinishFrameIcons, pass 1 ScreenOverlay), set by DrawInputPrompts
	mutable graphics::RenderPass _keyOrMouseView {graphics::RenderPass::ScreenOverlay};
	/// Screen pieces drawn in a world view (the did-you-know bubble, among the world's blended objects): their
	/// clip-space points become world points at a depth along the view, through the view's own camera, tested against
	/// the world's depth by zFunc
	struct ScreenInWorld
	{
		const screen_point::ClipToWorld* toWorld;
		float depth;
		render_modes::ZFunc zFunc;
	};
	/// Set while DrawKeyOrMouse draws a $m icon of the did-you-know bubble: its pieces go into the world view
	mutable std::optional<ScreenInWorld> _keyOrMouseInWorld;
	/// Clip-space vertices into world points at the depth (screen_point::WorldAtDepth), or as they are without one
	[[nodiscard]] static std::vector<SpiritQuadVertex> PlaceInWorld(std::vector<SpiritQuadVertex> vertices,
	                                                                const std::optional<ScreenInWorld>& inWorld);
	mutable uint32_t _keyOrMouseTicks {0};
	/// The side of DrawKeyOrMouse's align & 2 (1 past 2W / 3, 0 below W / 3)
	mutable int32_t _keyOrMouseSide {0};
	/// The fonts of HelpText: Data\j0, f1 and f3, loaded on first use (f1 / f3 fall back
	/// to j0 when they are missing, as the HelpText ctor does); nullptr when not even j0 loads
	[[nodiscard]] const GameFont* GameFontAt(help::TextFont font) const;
	mutable std::array<std::shared_ptr<const GameFont>, 3> _fonts; ///< from the font cache (Resources::GetFonts)
	mutable std::array<bool, 3> _fontLoadTried {};
	/// Glyph quads (pixels from the top left) in the ScreenOverlay view: the Text program, mode 16, ZFUNC ALWAYS. With
	/// `inWorld`, in a world view instead: the glyphs at its depth, tested by its zFunc, the view's camera left as it is
	void SubmitScreenText(const GameFont& font, const std::vector<GameFont::Vertex>& glyphs,
	                      graphics::RenderPass view = graphics::RenderPass::ScreenOverlay,
	                      const std::optional<ScreenInWorld>& inWorld = std::nullopt) const;
	/// A pre-transformed rectangle vertex (rhw 1), already in clip space
	struct ScreenRectVertex
	{
		float x, y, z;
		uint32_t abgr;
	};
	/// Two triangles of the pixel rectangle [x0, x1] x [y0, y1] in colour argb
	void AddScreenRect(std::vector<ScreenRectVertex>& out, int x0, int y0, int x1, int y1, uint32_t argb) const;
	/// Two textured triangles of the pixel rectangle [x0, x1] x [y0, y1] (uv u0, v0 .. u1, v1, colour argb), in clip
	/// space for SubmitSpiritQuads in the ScreenOverlay view
	void AddScreenQuad(std::vector<SpiritQuadVertex>& out, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
	                   float v1, uint32_t argb) const;
	/// Untextured rectangles in the ScreenOverlay view, mode 1 (SmoothAlpha), ZFUNC ALWAYS, no Z write
	void SubmitScreenRects(const std::vector<ScreenRectVertex>& vertices) const;
	/// HelpText's draw callback (finish-frame priority 20000: after the bars, before the film and the fade): the box, the
	/// dialogue texts and the click cue (InputPromptIcon), as the OverlayFrame laid them out
	void DrawHelpText(const OverlayFrame& overlay) const;
	/// The game speed's words from OverlayFrame::speedIndicator, the end of HelpText's draw: in the ScreenOverlay view
	/// over the dialogue texts, written in the world as the temple's text is and tested against the depth the Z reset
	/// left (RendererTemple.cpp)
	void DrawSpeedIndicator(const OverlayFrame& overlay, const Camera* camera) const;
	/// The did-you-know bubble from OverlayFrame::didYouKnow, drawn when the world's blended objects are drained, at its
	/// place among them (one Z object), in `view` with `camera`: its pixels turned into world points at their depths,
	/// the box, the shadows and the text tested against the world's depth, the scroll arrows and the $m icons not
	void DrawDidYouKnowBubble(graphics::RenderPass view, const Camera& camera, const OverlayFrame& overlay) const;
	/// The floating numbers from OverlayFrame::floatingNumbers, in the FinishFrameIcons view after the icons: each its
	/// black copy then its text, in font j0 without a depth test
	void DrawFloatingNumbers(const OverlayFrame& overlay) const;
	/// The camera's helper icons by the hand from OverlayFrame::handTricons, in the ScreenOverlay view before the cinema
	/// bars: each a white quad of atmos.raw at its alpha, in the atmos material with no depth test
	void DrawHandTricons(const OverlayFrame& overlay) const;
	/// The full screen film (Video/VideoPlayer.h): one quad per 256x256 tile of the mosaic in material mode 6, in
	/// `pass` (the end of the frame's, or in mode 2 the main view, under the falling copy of the creature)
	void DrawVideoOverlay(graphics::RenderPass pass = graphics::RenderPass::ScreenOverlay) const;
	/// The falling spell's copy of the creature over its film in mode 2 (RendererFallingSpell.cpp), in MainBlended: its
	/// body and its eyes in the colours the spell gave it this frame instead of the land's light, none while it is hidden
	void DrawFallingCreature(const Camera& camera) const;
	/// The falling spell's end of frame queue (its sparks, with the copy's hand glows: DrawFallingFrameQueue) and its light
	/// bursts over its film (RendererFallingSpell.cpp); `camera`: the draw's, for the near plane (none: 1, and the sparks
	/// alone in the overlay)
	void DrawFallingSpellOverlay(const Camera* camera) const;
	/// The advisor spirits (Help/SpiritsRuntime.h, RendererSpirits.cpp): `overlay` false, the ones with the in-world
	/// blend >= 0.5, in the frame, after the Z-sorter's drain; true, the others from the finish-frame callback in the
	/// FinishFrame3D view (after the Z reset). The model, its halo, its puff, as the frame's OverlayFrame holds them
	void DrawSpirits(const Camera& camera, const OverlayFrame& frame, bool overlay) const;
	/// Their rainbow trails, the "before" finish-frame callback (after the Z-sorter's flush, before the Z
	/// reset): at the end of MainBlended
	void DrawSpiritTrails(const Camera& camera, const OverlayFrame& frame) const;
	/// One building's row of the knocked town's population read-out (OverlayFrame::knockRows), drawn at once in the
	/// view, right after the building
	void DrawKnockRow(graphics::RenderPass viewId, std::span<const SpiritQuadVertex> vertices) const;
	/// Triangles (three vertices each) of the WorldTextured program with a .raw pair and a bgfx state; `alphaTest`, the
	/// alpha test of an alpha tested material (render_modes::PrimitiveAlpha), none to leave the test off
	void SubmitSpiritQuads(graphics::RenderPass viewId, std::span<const SpiritQuadVertex> vertices, uint32_t texture,
	                       uint32_t alpha, uint64_t state,
	                       std::optional<render_modes::ShaderAlpha> alphaTest = std::nullopt) const;
	/// The end of the frame: the bars, the film, the fade, in that order; in mode 2 (the falling spell) the sparks, the
	/// bursts, the bars, the fade (its film and its creature were drawn before, DrawScene)
	/// (`camera`: the draw's, DrawFallingSpellOverlay's near plane)
	void DrawFinishFrameOverlays(const OverlayFrame& overlay, const Camera* camera, const InterfaceLayers& layers) const;
	/// The film's picture, (re)made when its size changes and updated when its serial changes
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _videoTexture;
	mutable glm::u16vec2 _videoTextureSize {0, 0};
	mutable std::optional<uint32_t> _videoSerial;
	/// 1x1 white R8: the s_alpha of the WorldTextured program, the alpha 1 of the X1R5G5B5 tiles (inferred)
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _videoAlphaTexture;
	/// A mesh with the celestial shader: model matrix, texture, colour, render state
	void DrawCelestialMesh(graphics::RenderPass viewId, const L3DMesh& mesh, const glm::mat4& model, const Texture2D& texture,
	                       const glm::vec4& colour, uint64_t state, const glm::vec4& celestial = glm::vec4(0.0f),
	                       const Texture2D* alpha = nullptr) const;

public:
	Renderer(uint32_t bgfxReset, glm::u16vec2 backBufferSize, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept;
	~Renderer() noexcept final;

	[[nodiscard]] ShaderManager& GetShaderManager() const noexcept final;

	void ConfigureView(RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept final;

	void PreloadForLand() const noexcept final;
	void PreDraw(const DrawSceneDesc& drawDesc) const noexcept final;
	void DrawScene(const DrawSceneDesc& drawDesc) const noexcept final;
	[[nodiscard]] glm::u16vec2 GetResolution() const noexcept final { return _resolution; }
	[[nodiscard]] float MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept final;
	void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept final;
	void Frame() noexcept final;
	/// The OpaqueAlpha view's quad: alpha = 1 over the backbuffer, colour and depth untouched (RenderPass.h)
	void SubmitOpaqueAlpha() const;
	void RequestScreenshot(const std::filesystem::path& filepath) noexcept final;
	[[nodiscard]] std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept final;
	void RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept final;
	[[nodiscard]] std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& name) noexcept final;
	[[nodiscard]] bool GetDebug() const noexcept final { return _bgfxDebug; }
	void SetDebug(bool value) noexcept final { _bgfxDebug = value; }
	[[nodiscard]] bool GetProfile() const noexcept final { return _bgfxProfile; }
	void SetProfile(bool value) noexcept final { _bgfxProfile = value; }

	void Reset(glm::u16vec2 resolution) const noexcept final;
	void SetVsync(bool vsync) noexcept final;
	void SetMultisampling(bool multisampling) noexcept final;
	void ResetForNewGame() noexcept final;

private:
	/// What every primitive of one mesh draw reads that nothing changes during that draw: worked out once, by the first
	/// sub-mesh that draws
	struct MeshDrawValues
	{
		const Texture2D& heightMap;
		const Texture2D& cellMap;
		glm::vec4 islandExtent; ///< u_islandExtent: the land's minimum and maximum
		glm::vec4 cellMapInfo;  ///< u_cellMap: the land's minimum and the cell map's size
		glm::vec4 modelLight;   ///< u_modelLight (model_light::Uniform)
		bool lit;               ///< the land light table is loaded
		bool frameCells;        ///< this frame's cells texture has the cell map's layout
		/// The snow's (snow.sh, snow_object.sh): how deep it lies, its texture and its alpha, null when missing, and
		/// whether any lies anywhere
		const Texture2D* snowDepth;
		const Texture2D* snowTexture;
		const Texture2D* snowAlpha;
		bool snow;
	};
	[[nodiscard]] MeshDrawValues GetMeshDrawValues() const;
	/// A program's handles of the uniforms the mesh draws set, looked up the first time the program draws a mesh
	[[nodiscard]] const mesh_uniforms::Handles& MeshUniformsOf(const ShaderProgram& program) const;
	mutable std::unordered_map<const ShaderProgram*, mesh_uniforms::Handles> _meshUniforms;
	/// `transformCache`: where bgfx's matrix cache holds the draw's model matrices once the first primitive of the mesh has
	/// copied them there; the later primitives point at that copy instead of copying them again
	void DrawSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc, bool preserveState,
	                 std::optional<MeshDrawValues>& values, std::optional<uint32_t>& transformCache) const;
	/// The variant of an object program with one model matrix (bone_budget::Budget::Single), for the rigid sub-meshes
	[[nodiscard]] const ShaderProgram* StaticVariant(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _staticVariants;
	/// The variant of an object program with bone_budget::k_Few bones, for the posed villagers and animals (drawn one
	/// by one)
	[[nodiscard]] const ShaderProgram* FewBonesVariant(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _fewBonesVariants;
	/// The object program reading its bones from the bone palette, for the skinned sub-meshes of the posed instances of a
	/// mesh drawn together (L3DMeshSubmitDesc::bonePalette)
	[[nodiscard]] const ShaderProgram* PaletteVariant() const;
	mutable const ShaderProgram* _paletteVariant {nullptr};
	/// The posed villagers and animals of the main view drawn from the bone palette, a mesh at a time, each in one
	/// instanced draw: the rows they are drawn with, every instance's first bone in its matrix
	/// (bone_palette::SetFirstBone), in an instance buffer of this frame. Made in DrawPass before the objects are drawn
	mutable std::unordered_map<entt::id_type, bgfx::InstanceDataBuffer> _paletteDraws;
	/// The bones of those instances, four texels a bone (bone_palette::Append), and the texture they are uploaded into
	mutable std::vector<glm::vec4> _paletteTexels;
	mutable std::unique_ptr<Texture2D> _bonePalette;
	void DrawPass(const DrawSceneDesc& desc) const;

	// The temple (RendererTemple.cpp): while the player is inside it, the passes draw it in place of the world
	/// Whether the player is inside the temple
	[[nodiscard]] static bool InTemple();
	/// A pass of the temple: its sky, then its rooms and what is drawn in them, in the order they are submitted
	void DrawTemplePass(const DrawSceneDesc& desc) const;
	/// The main room mirrored through the plane of its origin into the reflection's target, which its floor shows
	void DrawTempleReflection(const DrawSceneDesc& drawDesc) const;
	/// The land seen from above, without the haze, which the map over the main room's pool is textured with, drawn
	/// once a visit
	void DrawTempleMapPass(const DrawSceneDesc& drawDesc) const;
	/// How a mesh of the temple is drawn, beyond its submit desc
	struct TempleLook
	{
		/// How far the textures have slid across the mesh
		glm::vec2 uvOffset {0.0f};
		/// Textures some of the submeshes are drawn with in place of their skins, colours added to some, and those
		/// left undrawn, by submesh (RenderContext::InstancedDrawDesc)
		std::span<const std::pair<uint32_t, TextureHandle>> subMeshTextures;
		std::span<const std::pair<uint32_t, glm::vec3>> subMeshGlows;
		std::span<const uint32_t> hiddenSubMeshes;
		bool onlyJoints {false};
		bool hideShutJoints {false};
		/// In its own colour alone, not shaded by the light, and its alpha multiplied by opacity
		bool unshaded {false};
		float opacity {1.0f};
		/// The reflection's target, which the floor's and the pool's programs read
		const Texture2D* reflection {nullptr};
		/// What the floor's alpha over the reflection is multiplied by
		float floorOpacity {1.0f};
		/// One-off instances of a mesh the renderer places itself, in place of the desc's
		const bgfx::InstanceDataBuffer* instances {nullptr};
		/// Every primitive drawn in this state, culled by its material, in place of its material's mode
		std::optional<uint64_t> state;
		/// The temple's depth fog over the mesh (temple_fade), seen from the pass's camera: its lightmapped submeshes are
		/// fogged at each vertex and the others once, at `at` (a belt or medal where it stands) or, without it, at each
		/// one's first vertex as the file has it, placed from the temple's origin. None for the pool, the map and its
		/// markers, which are drawn as they were
		struct Fog
		{
			TempleFade fade;
			glm::vec3 endColour {0.0f};
			glm::mat4 view {1.0f};
			glm::vec3 origin {0.0f};
			std::optional<glm::vec3> at;
			/// The pass is the main room's reflection, whose camera is mirrored through the temple's floor: the points
			/// fogged once are mirrored too, so that their depth is the one the room's own camera sees
			bool mirrored {false};
		};
		std::optional<Fog> fog;
	};
	/// The programs a pass of the temple draws with, looked up once at the start of the pass
	struct TemplePrograms
	{
		const ShaderProgram* temple {nullptr};
		const ShaderProgram* lightmap {nullptr};
		const ShaderProgram* reflective {nullptr};
		/// A floor's submeshes without a lightmap over the reflection
		const ShaderProgram* reflectiveUnlit {nullptr};
		const ShaderProgram* reflection {nullptr};
		const ShaderProgram* textured {nullptr};
		const ShaderProgram* beam {nullptr};
	};
	/// The temple's depth fog a pass draws its rooms in, from its camera, in the frame's fade and the temple's light;
	/// none outside the temple or with the debug window's switch off
	[[nodiscard]] static std::optional<TempleLook::Fog> TempleFogOf(const DrawSceneDesc& desc);
	void DrawTempleMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, const TempleLook& look,
	                    const TemplePrograms& programs) const;
	void DrawTempleSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
	                       const TempleLook& look, const TemplePrograms& programs, const TextureHandle* subMeshTexture,
	                       glm::vec3 glow) const;
	/// A black floor under the whole temple, which the cracks the rooms are modelled with show instead of the sky
	void DrawTempleUnderside(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The main room's pool: its water twice, turned an eighth apart, each shimmering as the other fades
	void DrawTemplePool(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The temple's map: the island in relief over the main room's pool
	void DrawTempleMap(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The markers on the temple's map, on soft glows that show through anything
	void DrawTempleMapMarkers(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The creature's room's belts and medals, in the temple's light, while the room is drawn
	void DrawTempleCaveTrophies(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The mists of the rooms drawn whole (components::MistDome), by the island's mist draw; none in the reflection
	void DrawTempleMists(const DrawSceneDesc& desc) const;
	/// The text the temple's rooms write in the world this frame: the signs' labels and the scroll the camera is close to
	void DrawTempleText(const DrawSceneDesc& desc) const;
	/// The beams of the temple's spot lights and the light its windows shed, which the game draws in its rooms but not
	/// in the reflection of the main room
	void DrawTempleLightBeams(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The glows of the lights of the rooms drawn whole; the main room reflects its own glows alone in its floor
	void DrawTempleGlows(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// Shafts of light and glows (Graphics/LightBeams.h) with the beam program: the texture by the vertices' colours,
	/// with the alpha of `alpha` when given
	void SubmitBeamMesh(RenderPass viewId, const ShaderProgram* shader, const BeamMesh& mesh, const glm::mat4& model,
	                    const TextureHandle& texture, std::optional<TextureHandle> alpha, uint64_t state) const;
	/// The spot lights' cones of a pass, built again each time in storage kept from one pass to the next
	mutable BeamMesh _templeCones;
	/// The land seen from above for the temple's map, and the visit to the temple it was drawn for
	mutable std::unique_ptr<FrameBuffer> _templeMapFrameBuffer;
	mutable std::optional<uint32_t> _templeMapVisit;
	/// The map was drawn this frame, after the main pass that would read it: the map shows from the next frame
	mutable bool _templeMapFresh {false};

	std::unique_ptr<ShaderManager> _shaderManager;
	std::unique_ptr<BgfxCallback> _bgfxCallback;
	mutable std::unique_ptr<LandLightTable> _landLight;
	/// The land light's palette (resources' LandLightPalette::k_Id), null when it could not be loaded
	mutable std::shared_ptr<const LandLightPalette> _landLightPalette;
	/// The land is lit once its palette is loaded: until then the land, the sea and the models are drawn unlit
	[[nodiscard]] bool IsLandLit() const noexcept { return _landLightPalette != nullptr; }
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _landLightTexture;
	/// The inputs of the land light table's last build and upload, as their exact bits: the table is built and uploaded
	/// again only when one of them changes, since the same inputs build the same table
	struct LandLightInputs
	{
		uint32_t skyType;
		uint32_t alignment;
		uint32_t overcast;
		uint8_t flash;
		bool operator==(const LandLightInputs&) const = default;
	};
	mutable std::optional<LandLightInputs> _landLightBuilt;
	mutable sky_dome::Tint _skyTint; ///< the sky dome's colours of the frame (UpdateSkyTint)
	mutable float _overcast {0.0f};  ///< the overcast at the camera of the frame, held to 1 at most (UpdateSkyTint)
	mutable bool _skyFog {false};    ///< the "Fog" detail key of the frame (UpdateSkyTint)
	mutable std::array<glm::vec4, 2> _hazeUniforms {}; ///< u_haze and u_hazeColour of the frame (PreDraw)
	mutable haze::Params _haze;                        ///< the haze of the frame (graphics::haze::Frame, PreDraw)
	/// The clouds of the main view this frame, collected by PreDraw (CollectClouds: their animation counters advance)
	mutable std::vector<std::pair<float, uint32_t>> _preClouds;
	/// What the main pass draws of the PSys this frame, collected at the end of PreDraw with the
	/// desc's FrameInputs: the effects by their draw path, the surfaces of revolution and the exploded pieces (the
	/// Sorted ones with the entities; the Queued and Immediate ones tagged with their atom). Empty outside the main view
	mutable particles::draw::Frame _particleFrame;
	/// The main view's mists, chimney smoke and raining tiles this frame (their Z-sorter keys and indices in _frameMists,
	/// _frameSmoke and _frameRain), collected by PreDraw: CollectMists, CollectChimneySmoke and CollectRain advance
	/// what they collect
	mutable std::vector<std::pair<float, uint32_t>> _preMists;
	mutable std::vector<std::pair<float, uint32_t>> _preSmoke;
	mutable std::vector<std::pair<float, uint32_t>> _preRain;
	/// The snowing tiles, collected by PreDraw with the rain (CollectSnow)
	mutable std::vector<std::pair<float, uint32_t>> _preSnow;
	/// The influence border's UV offset this frame (UpdateInfluenceCurtain), none when its camera gate failed
	mutable std::optional<glm::vec2> _preInfluenceScroll;
	/// Inside the temple, where the circles stand over the main room's map this frame; none in the world
	mutable std::optional<TempleMapInfluence> _preInfluencePlacement;
	/// RawTextureIds' cache (draw-owned)
	mutable std::unordered_map<std::string, std::pair<entt::id_type, entt::id_type>> _rawTextureIds;
	/// The glowing shells' bands made so far, by model and part (GlowShellOf); none for a part without triangles
	mutable std::unordered_map<uint64_t, std::unique_ptr<VolumeGlowShell>> _glowShells;
	mutable float _sunGlare {0.0f};          ///< Sun glare visibility 0..255, smoothed
	mutable glm::u16vec2 _resolution {0, 0}; ///< of the main view
	/// The question asked of each glare sample's pixel as the scene is drawn: whether anything nearer than the depth
	/// that hides it is there. Made the first time it is asked, and made afresh when it was not asked the frame before
	mutable std::array<bgfx::OcclusionQueryHandle, 5> _glareQueries {
	    {BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	/// The frame (_framesDrawn) each glare sample's question was last asked in
	mutable std::array<uint32_t, 5> _glareAskedFrame {};
	/// The frames handed to bgfx by Frame
	uint32_t _framesDrawn {0};
	/// The projected shadows, the ShadowInfo list (shadow_list)
	std::unique_ptr<shadow_list::List> _shadows;
	/// The objects of this frame's main view under a shadow that falls on objects (one not cast by the object itself,
	/// whose box contains the object's bounding box): instance index -> the shadows, newest first
	struct ShadowReceiver
	{
		entt::id_type meshId {0};
		bool morphWithTerrain {false};
		std::vector<const shadow_list::ShadowInfo*> shadows;
	};
	mutable std::unordered_map<uint32_t, ShadowReceiver> _shadowReceivers;
	mutable std::vector<float> _cloudAlpha;         ///< per cloud 0..255 this frame
	mutable std::vector<uint8_t> _cloudShadowImage; ///< sclouds.raw
	/// This frame's land cells (land_light::Texels: the stamps and the night lights in them), the cell map's layout
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _landCellsTexture;
	/// The instances of the fish puzzle nets' floats (RendererFishPlot.cpp), made on first use
	mutable graphics::UniqueHandle<bgfx::DynamicVertexBufferHandle> _fishPlotInstances;
	mutable uint32_t _fishPlotCapacity {0};
	/// The advisor spirits' instances (RendererSpirits.cpp): two in the frame, two in the overlay, made on first use
	mutable graphics::UniqueHandle<bgfx::DynamicVertexBufferHandle> _spiritInstances;
	mutable glm::u16vec2 _landCellsSize {0, 0};
	/// Moves the clouds and computes their colour / alpha; then this frame's land cells (land_light): the frame's
	/// light stamps with the clouds' shadows among them, the night lights, uploaded to _landCellsTexture
	void UpdateClouds() const;
	mutable glm::vec3 _cloudRgb {1.0f};
	mutable SkyAlignment _skyAlignment;      ///< Moved towards the target every frame
	mutable float _cloudMilliseconds {0.0f}; ///< this frame's game time step for the clouds' animation counters
	uint32_t _bgfxReset;
	/// The back buffer's size bgfx was last started or reset with, kept for a reset that only changes the flags
	mutable glm::u16vec2 _backBufferSize;
	/// Pictures of frames drawn aside, while the window has no buffers to read
	std::unique_ptr<OffscreenScreenshots> _offscreenShots;
	bool _bgfxDebug = false;
	bool _bgfxProfile = false;
	std::unique_ptr<Mesh> _plane;
};
} // namespace graphics
} // namespace openblack
