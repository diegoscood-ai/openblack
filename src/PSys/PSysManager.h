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

#include <memory>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "PSys.h"

namespace openblack::psys
{

/// The running effects (PSysGlobal) and the script's spot visuals (GParticleContainer, CHL SPECIAL_EFFECT_*)
namespace manager
{
/// The draw path of the effects (psys::DrawPath, PSys.h)
using DrawPath = psys::DrawPath;

/// While false, the renderer still draws every effect as one Z object at its origin (Collect, CollectChains,
/// surf_revol::Collect with their origin key), Mist.cpp hands every mist to mists::Submit and RenderingSystem routes the
/// PSys mesh atoms as before (opaque / translucent / cut ranges). Set to true together with the Renderer side that reads
/// CollectSorted / CollectQueued / HandEffects and RenderContext::psysAtoms: then only the Sorted mists go to
/// mists::Submit and RenderingSystem puts every PSys mesh atom in RenderContext::psysAtomDrawDescs, out of the old
/// ranges. Interface: dev\tmp_dis\miracles\polish\drawpath_fix.md
inline constexpr bool k_DrawByPath = false;

/// PSysInterface::Create: an effect from a spell file (e.g. "SF_Smoke"); 0 if the file is missing. Its draw path is
/// DrawPath::Sorted, Spell::Draw's (0x720441), until SetDrawPath changes it. `type` is Create's NET_GAME_TYPE (its 6th
/// argument, 0x68F3AE): Synced for a spell's own effect and the spot visuals, Local for the others (Effect)
uint32_t Start(const std::string& file, glm::vec3 origin, float magnitude,
               game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
/// The same with a file already parsed (File::Parse, the tests); 0 for nullptr
uint32_t Start(std::shared_ptr<const File> file, glm::vec3 origin, float magnitude,
               game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
void CloseDown(uint32_t id);
void SetOrigin(uint32_t id, glm::vec3 origin);

/// PSysInterface::Create 0x68E910 for a Spell: the spell owns the effect and steps it itself once per turn with its
/// PSysProcessInfo (Spell::CoreProcess 0x720660), so ProcessTurn leaves it alone. 0 if the file is missing.
uint32_t StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude, SpellSink* sink,
                       game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
/// The spell's PSys vt 0x100: one step of dt with that info; false (5) once it is finished, and then it is gone
bool ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt);
/// delete psys (Spell::ToBeDeleted, CoreProcess on 5)
void Delete(uint32_t id);
/// An effect stepped every frame (Process_ with g_game_time_inc: the in-hand and the utility effects): drawn where its
/// last step left it, not interpolated over the game turn
void SetPerFrame(uint32_t id);
/// How the effect's owner draws it (GJPSysInterface Draw_(t, 1) / AddDrawing / Draw_(t, 0)): set once where the effect
/// is started. Sorted for a missing id
void SetDrawPath(uint32_t id, DrawPath path);
[[nodiscard]] DrawPath GetDrawPath(uint32_t id);
/// fn_0063E0F0 0x63E190..0x63E198: a container's +0x38 = (GSpotVisualInfo +0x4C SingleZSort == 1), which fn_0063E240
/// reads to draw its effect with AddDrawing (0x63E26A) or Draw_(1) (0x63E277)
[[nodiscard]] DrawPath SpotVisualDrawPath(uint32_t singleZSort);
/// nullptr when gone
[[nodiscard]] Effect* Find(uint32_t id);
/// The id of a running effect (0 if it is not one)
[[nodiscard]] uint32_t IdOf(const Effect* effect);

/// GParticleContainer::CreateSpotVisualWithSpecifiedDuration 0x63E580: SPOT_VISUAL index (GSpotVisualInfo 0xD44470),
/// seconds (< 0: forever; 0: the entry's own life), an owner object it follows and whose loss ends it. Returns the
/// container object for the script (deleting it closes the effect), or entt::null. `magnitude` is the float argument
/// (0x63E4B0 -> fn_0063E410 passes it to PSysInterface::Create as the effect's magnitude; GJPSysInterface::Create
/// 0x68F3A1 SetScale; 1 for the scripts, 8 for the smoke of UR_Explosion).
entt::entity CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner, float magnitude = 1.0f);
/// The same with the duration in game turns, as CreateSpotVisualWithSpecifiedDuration 0x63E580 takes it (its int argument;
/// < 0: forever, else that many Process turns, 0 closing it at the first one: 0x63E2A0..0x63E2B1): UR_Explosion's 60
/// (0x67EE92) and TicksForSeconds(4) & 0xFFFF (0x67EF2C)
entt::entity CreateSpotVisualTurns(int spotVisual, glm::vec3 position, int turns, entt::entity owner, float magnitude = 1.0f);
/// GParticleContainer::CloseDown 0x63E370 of a container made by CreateSpotVisual (nothing for entt::null)
void CloseSpotVisual(entt::entity object);

/// One game turn (GParticleContainer::Process 0x63E280, Process_ with the turn length)
void ProcessTurn(float turnSeconds);
/// OPENBLACK_TEST_PSYS: a test effect once the map is loaded
void RunDebugHooks();
void Clear();

/// For the renderer: every effect with atoms to draw, interpolated since the last turn
struct Drawable
{
	glm::vec3 origin;
	std::vector<Effect::DrawAtom> atoms;
	/// The draw fraction of the step this effect is drawn with: the float PSysManager::AddDrawing keeps at its +0xB0
	/// (0x6797D4) and hands down to every atom's DrawAt as DrawData +0x14 (1 for the effects that step every frame)
	float t {1.0f};
	/// The effect's draw path (SetDrawPath). A DrawableSource's drawable is Queued unless it says otherwise: one Z object
	/// at its origin (the fire's FireGraphic, fn_007325D0 0x73261D); the town belief is Sorted (TownCentre::DrawPSys
	/// 0x69BF19 Draw_(1))
	DrawPath path {DrawPath::Queued};
	/// The effect's id (0 for a DrawableSource's)
	uint32_t effect {0};
};
/// kind: the sprites (with the town belief sprites), or the mesh atoms (Creators/Mesh.h)
std::vector<Drawable> Collect(Creator::Kind kind = Creator::Kind::Sprite);
/// Every chain collection of every running effect, for the ribbon pass (Creators/Chain.h, Graphics/RendererChain.cpp)
std::vector<Effect::DrawChain> CollectChains();
/// Other drawers of PSys-style sprites (the fire's FireGraphic, ECS/Fire): Collect appends what they give
using DrawableSource = void (*)(std::vector<Drawable>& out);
void AddDrawableSource(DrawableSource source);

/// One atom of a Sorted effect with the point of its own Z object (LH3DZSorter::NewZObject 0x83F310): the renderer sorts
/// it by zsorter::Key(key, camera), (x^2 + y^2) + z^2 of key - camera (LH3DSprite::AddDrawing 0x840C95..0x840CA3,
/// fn_00679F60 0x679FA9..0x679FB7, fn_0067B380 0x67B3C9..0x67B3D7)
struct SortedAtom
{
	glm::vec3 key;
	Effect::DrawAtom atom;
	uint32_t effect; ///< the effect's id (0: the town belief)
	float t;         ///< the effect's draw fraction (Drawable::t)
};
/// One chain of a Sorted effect: its own Z object at the joint n / 2 (fn_0067B380 0x67B389..0x67B3A1: joints +0x44,
/// 0x1C each, index (n - (n >> 31)) >> 1; only when g_started_frame and n != 0), drawn by 0x67B3F0 at the drain
struct SortedChain
{
	glm::vec3 key;
	Effect::DrawChain chain;
	uint32_t effect;
	float t;
};
/// This frame's atoms of every Sorted effect (Draw_(t, 1)), each list in effect order then fn_006798B0's order
struct SortedFrame
{
	/// Kind::Sprite atoms (ParticleSpriteCreator, the town belief's symbols): key = the LH3DSprite's +0/+4/+8 that
	/// Particle3DSprite::DrawAt fills, the atom's position raised by height x size x 0.5 with CentreAtBase
	/// (0x67AFAB..0x67AFD6), then LH3DSprite::AddDrawing 0x840C70 (0x67B0D2). The town belief's ParticlePlayerSymbol:
	/// PlayerSymbolSprite::AddDrawing 0x69D790 at its position
	std::vector<SortedAtom> sprites;
	/// Kind::Mesh atoms (Particle3DObj / AnimTextured / Particle3DAnim, opaque or not, cut or not): key = the object's
	/// +0x38..+0x40 (fn_00679F60 0x679F7E..0x679F9F), its translation = the atom's position. Only after
	/// CheckRegionOnScreen (0x679F75): the renderer's test. Drawn as instances: RenderContext::psysAtoms
	std::vector<SortedAtom> meshes;
	/// MistCreator atoms: key = the LH3DMist's +0x38 (fn_007FA7F0, NewZObject 0x7FA87B), its SetPos = the atom's
	/// position (mist_atoms::Describe gives the MistDesc)
	std::vector<SortedAtom> mists;
	/// ZR_SurfRevol atoms: no Z object, drawn at once when the effect is drawn (0x67CBA0 does not read [0xC0215D]); the
	/// key is the atom's position, unused. Their vertices: surf_revol::Collect, matched by Surface::atom
	std::vector<SortedAtom> surfaces;
	/// The chain of each collection with two joints or more (Effect::DrawChain)
	std::vector<SortedChain> chains;
	/// The other drawn atoms (Kind::Point, light maps, the storm's carried objects...), in case a renderer wants them
	std::vector<SortedAtom> others;
};
[[nodiscard]] SortedFrame CollectSorted();

/// A Queued or Immediate effect: all of it drawn at once, its items in fn_006798B0's order (Effect::CollectOrdered:
/// sprites, meshes, mists, surfaces... of a collection, then its chain, then the child collections)
struct OrderedEffect
{
	uint32_t effect; ///< the effect's id (0 for a DrawableSource's, the fire)
	DrawPath path;
	/// GetOrigin (vt+0x130): the key of the effect's single Z object (PSysManager::AddDrawing 0x6797E5..0x679834,
	/// (x^2 + y^2) + z^2), Queued only
	glm::vec3 origin;
	float t;
	/// item.chain >= 0: chains[item.chain], whose item.atom is its joint n / 2; else item.atom is the atom to draw (by
	/// its creator: Kind::Sprite, Kind::Mesh -> RenderContext::psysAtomIndex[item.atom.atom], MistCreator ->
	/// mist_atoms::Describe, SurfRevolCreator -> surf_revol::Surface::atom, the others not drawn)
	std::vector<Effect::OrderedItem> items;
	std::vector<Effect::DrawChain> chains;
};
/// Every Queued effect (the seed graphic on a ball / icon 0x51A2CA, the containers 0x63E26A) and the Queued
/// DrawableSources (the fire), one Z object each at its origin
[[nodiscard]] std::vector<OrderedEffect> CollectQueued();
/// Every Immediate effect: the hand's (CHand::DrawSpellInHand 0x46E76A, Draw_(1.0, 0)), drawn inside the hand's Z
/// object after the hand and what it holds (CHand::Draw 0x46D2AE)
[[nodiscard]] std::vector<OrderedEffect> HandEffects();
} // namespace manager

} // namespace openblack::psys
