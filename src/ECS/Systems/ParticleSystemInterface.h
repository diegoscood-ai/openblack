/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/registry.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Particles/GestureTrail.h"

namespace openblack::psys
{
class Effect;
class SpellSink;
struct ProcessInfo;
enum class DrawPath : uint8_t;
} // namespace openblack::psys

namespace openblack::psys::manager
{
struct State;
} // namespace openblack::psys::manager

namespace openblack::psys::shields
{
struct DefensiveSphere;
} // namespace openblack::psys::shields

namespace openblack::ecs::town_belief
{
struct BeliefSprite;
} // namespace openblack::ecs::town_belief

namespace openblack::particles
{
class LightSheet;
/// A symbol of belief as the particle system takes it: where it rises, how much and in whose colour (ECS/Town/TownBelief.h)
using BeliefSprite = ecs::town_belief::BeliefSprite;
/// A live shield as the particle system reports it: its sphere, and the effect that owns it (Particles/Rules/Shield.h)
using ShieldSphere = psys::shields::DefensiveSphere;
} // namespace openblack::particles

namespace openblack::ecs::systems
{
/// The running particle effects: those miracles own and step themselves, the spot visuals scripts and the game place for
/// a time, and any other effect, stepped once a game turn. It also holds the particle engine's state: the running
/// effects, the spot visual containers, the next effect id, the drawable sources and the shields' defensive spheres,
/// which psys::manager's functions and the shield rules work on (Locator::particleSystem)
class ParticleSystemInterface
{
public:
	/// A running effect, by a number that is never reused
	using EffectId = uint32_t;
	static constexpr EffectId k_NoEffect = 0;

	/// How many things the last collected frame drew, for the debug window
	struct DrawStats
	{
		size_t sprites {};
		size_t chains {};
		size_t meshes {};
		size_t mists {};
		size_t lightStamps {};
		size_t effects {};
	};

	/// What the debug window shows of an effect
	struct EffectInfo
	{
		EffectId id {};
		std::string file;
		glm::vec3 origin {0.0f};
		float age {};
		size_t atoms {};
		size_t collections {};
		bool closing {};
		bool ownedBySpell {};
		psys::DrawPath path {};
		/// Objects and points given to it that its rules haven't taken yet
		size_t targets {};
		/// Seconds left of a spot visual's life, none for one that lasts until it is closed and for any other effect
		std::optional<float> secondsLeft;
		/// The modifier classes its file names that the game does not run yet
		std::vector<std::string> unportedClasses;
	};

	virtual ~ParticleSystemInterface() = default;

	/// An effect from a particle file by its name (such as "SF_Smoke"), at a point and a magnitude; k_NoEffect if there
	/// is no such file. Its random numbers are the shared ones when synced. It is drawn sorted until SetDrawPath changes
	/// it
	virtual EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced = false) = 0;
	/// The effect of a particle type, as Start with the type's file; k_NoEffect for a type without a file
	virtual EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced = false) = 0;
	/// An effect a miracle (or the object showing one) owns and steps itself with ProcessForSpell, so ProcessTurn leaves
	/// it alone; the sink, when given, hears what it does. Its random numbers are the shared ones when synced
	virtual EffectId StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                               psys::SpellSink* sink, bool synced) = 0;
	/// One step of a miracle's effect; false once it has ended, and then it is gone
	virtual bool ProcessForSpell(EffectId id, const psys::ProcessInfo& info, float seconds) = 0;
	/// One step of an effect stepped as it is drawn, by the frame's game time, rather than each turn: from then on its
	/// owner steps it, so ProcessTurn leaves it alone, and it is drawn where its last step left it. False once it has
	/// gone
	virtual bool ProcessByFrame(EffectId /*id*/, float /*seconds*/) { return false; }
	/// A spot visual: the info table's particle effect at a point for some turns (its own life when not given, for
	/// ever when negative, closing at its first turn when 0), staying where it was made, and ending when its owner
	/// object (if any) goes. The object a script holds it by, whose deletion closes it; entt::null when the visual has
	/// no particle file
	virtual entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                                     float magnitude = 1.0f) = 0;

	virtual void SetOrigin(EffectId id, glm::vec3 origin) = 0;
	/// The player the effect is shown for, such as the colour its rules give it; nothing for an effect not running
	virtual void SetPlayer(EffectId id, int player) = 0;
	/// How the effect is drawn: sorted with everything else that blends unless told otherwise
	virtual void SetDrawPath(EffectId id, psys::DrawPath path) = 0;
	/// What the effect draws is moved by this from where its last step left it, as the miracle in the hand follows the
	/// hand between turns, each atom by the offset times its draw weight (all of it unless a rule says otherwise): its
	/// sprites, meshes, mists, other atoms and chain joints. Not moved: its mesh pieces and surface-of-revolution atoms,
	/// drawn from the atom itself, a queued effect's sort key and the town belief. No offset until told; nothing for an
	/// effect not running
	virtual void SetDrawOffset(EffectId id, glm::vec3 offset) = 0;
	/// The live shield holding a point, its sphere grown by a margin, if any: the newest first; nullptr when none does.
	/// The pointer is valid until the next sphere is added or removed: do not keep it across a step
	[[nodiscard]] virtual const particles::ShieldSphere* FindShield(glm::vec3 point, float margin) const = 0;
	/// How many particle sounds are playing or dying away, for the debug window; 0 without the audio
	[[nodiscard]] virtual size_t GetSoundCount() const = 0;
	/// Whether the effect's owner draws it this frame: one that is not drawn still steps but is in no draw list. Every
	/// effect is drawn until told otherwise
	virtual void SetDrawn(EffectId id, bool drawn) = 0;
	/// An object for the effect's rules to act on, such as a person for the heal miracle's chakra
	virtual void AddTarget(EffectId id, entt::entity target) = 0;
	/// A point of the world for the effect's rules to act on, such as where a beam ends; nothing for an effect not running
	virtual void AddTargetPosition(EffectId /*id*/, glm::vec3 /*position*/) {}
	/// A symbol of belief to rise from something that gained it, in the effect every symbol rises in; no more than a few
	/// hundred wait, and one more is dropped
	virtual void AddBeliefSprite(const particles::BeliefSprite& /*sprite*/) {}

	/// This computer's hand in a frame, for the trail that follows it while it gestures
	struct HandFrame
	{
		glm::vec3 position {0.0f};
		/// How big the hand is drawn
		float size {1.0f};
		glm::vec3 cameraPosition {0.0f};
	};
	/// A recognised gesture's trail to show on the land, in the effect every trail shows in, which takes the trails in
	/// the order they came; nothing for nullptr. The trail's contents are taken: moved out when the caller hands over its
	/// only reference, else copied, so that other holders still see it whole
	virtual void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) = 0;
	/// Once a frame, by the frame's game time (0 while the game is paused): every sheet of light still standing moves
	/// on by it, then the trail behind the hand and the open selection step (they do nothing while paused)
	virtual void UpdateFrame(float gameSeconds, const HandFrame& hand) = 0;
	/// A sheet of light to move on every frame for as long as something else holds it
	virtual void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) = 0;
	/// The sheets of light still standing, oldest first; those gone are forgotten
	[[nodiscard]] virtual std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() = 0;
	/// The effect stops making particles and fades out as its file has it
	virtual void CloseDown(EffectId id) = 0;
	/// The effect goes at once
	virtual void Delete(EffectId id) = 0;
	/// Whether the effect is still running: false once it has gone, and for k_NoEffect
	[[nodiscard]] virtual bool IsRunning(EffectId id) const = 0;
	/// nullptr once it has gone
	[[nodiscard]] virtual psys::Effect* Find(EffectId id) = 0;

	/// Once a game turn: the spot visuals count down and step, then every other effect not owned by a miracle steps,
	/// newest first
	virtual void ProcessTurn() = 0;
	/// A new land: every effect goes
	virtual void Reset() = 0;

	/// Every running effect, newest first, for the debug window
	[[nodiscard]] virtual std::vector<EffectInfo> GetEffects() const = 0;
	/// The particle files' names, for the debug window
	[[nodiscard]] virtual std::vector<std::string> GetFileNames() const = 0;
	/// Stops stepping effects, for looking at them in the debug window. While paused, ProcessTurn does nothing, and
	/// ProcessForSpell and ProcessByFrame step nothing but give true for an effect still running
	virtual void SetPaused(bool paused) = 0;
	[[nodiscard]] virtual bool IsPaused() const = 0;

	[[nodiscard]] virtual psys::manager::State& GetState() noexcept = 0;

	/// A rule or creator file's own state of type T (each file keeps its struct type), value-initialised on first
	/// use; it lives as long as the service and goes after the effects. Only the main thread runs the effects
	template <typename T>
	[[nodiscard]] T& Module()
	{
		auto& context = ModuleStore().ctx();
		if (!context.contains<T>())
		{
			context.emplace<T>();
		}
		return context.get<T>();
	}

protected:
	[[nodiscard]] virtual entt::registry& ModuleStore() noexcept = 0;
};
} // namespace openblack::ecs::systems
