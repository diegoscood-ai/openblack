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
/// PSysInterface::Create: an effect from a spell file (e.g. "SF_Smoke"); 0 if the file is missing
uint32_t Start(const std::string& file, glm::vec3 origin, float magnitude);
void CloseDown(uint32_t id);
void SetOrigin(uint32_t id, glm::vec3 origin);

/// PSysInterface::Create 0x68E910 for a Spell: the spell owns the effect and steps it itself once per turn with its
/// PSysProcessInfo (Spell::CoreProcess 0x720660), so ProcessTurn leaves it alone. 0 if the file is missing.
uint32_t StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude, SpellSink* sink);
/// The spell's PSys vt 0x100: one step of dt with that info; false (5) once it is finished, and then it is gone
bool ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt);
/// delete psys (Spell::ToBeDeleted, CoreProcess on 5)
void Delete(uint32_t id);
/// An effect stepped every frame (Process_ with g_game_time_inc: the in-hand and the utility effects): drawn where its
/// last step left it, not interpolated over the game turn
void SetPerFrame(uint32_t id);
/// nullptr when gone
[[nodiscard]] Effect* Find(uint32_t id);
/// The id of a running effect (0 if it is not one)
[[nodiscard]] uint32_t IdOf(const Effect* effect);

/// GParticleContainer::CreateSpotVisualWithSpecifiedDuration 0x63E580: SPOT_VISUAL index (GSpotVisualInfo 0xD44470),
/// seconds (< 0: forever; 0: the entry's own life), an owner object it follows and whose loss ends it. Returns the
/// container object for the script (deleting it closes the effect), or entt::null. `magnitude` is the float argument
/// (0x63E4B0 -> fn_0063E410 passes it to PSysInterface::Create as the effect's magnitude; UR_Explosion's smoke gives 8).
entt::entity CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner, float magnitude = 1.0f);

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
};
/// kind: the sprites (with the town belief sprites), or the mesh atoms (Creators/Mesh.h)
std::vector<Drawable> Collect(Creator::Kind kind = Creator::Kind::Sprite);
/// Every chain collection of every running effect, for the ribbon pass (Creators/Chain.h, Graphics/RendererChain.cpp)
std::vector<Effect::DrawChain> CollectChains();
/// Other drawers of PSys-style sprites (the fire's FireGraphic, ECS/Fire): Collect appends what they give
using DrawableSource = void (*)(std::vector<Drawable>& out);
void AddDrawableSource(DrawableSource source);
} // namespace manager

} // namespace openblack::psys
