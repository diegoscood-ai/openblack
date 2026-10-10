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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFace.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
}
namespace openblack::creature_mind_tables
{
struct Tables;
}

namespace openblack::ecs::systems
{

/// The creatures' minds: once a game turn their desires grow and fade, and each decides what its body does while it
/// has nothing better to do, and what it looks at (see components::CreatureMindState). Creatures can also be told to
/// do things, which the debug tools use.
class CreatureMindSystemInterface
{
public:
	virtual ~CreatureMindSystemInterface() = default;

	virtual void ProcessTurn() = 0;
	/// Once a game turn after ProcessTurn: each creature plans two of its desires and changes what it does for a plan
	/// pressing enough
	virtual void PlanTurn() = 0;
	/// Once a game turn after PlanTurn: what the leash taught, and copying the player, step on
	virtual void LearnTurn() = 0;

	/// Takes up a mind file's learning, desires and stage of growing up at the next turn
	virtual void LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) = 0;
	/// The creature's mind as a mind file of the current version, over the file it was loaded from, if it has a mind
	[[nodiscard]] virtual std::optional<creaturemind::MindFileData> SaveMind(entt::entity creature) const = 0;
	/// Forgets everything learnt: the desires start again as the species' do, with no opinions or examples
	virtual void ClearLearning(entt::entity creature) = 0;
	/// Creatures that can see the point watch an ordinary skill practised (by its row in the game's table), or a miracle
	/// cast
	virtual void SeeSkill(const glm::vec3& point, size_t skill) = 0;
	virtual void SeeMiracle(const glm::vec3& point, size_t miracle) = 0;
	/// The player did one of the deeds creatures copy (by its row in the game's table) at a point, maybe to something.
	/// Only that player's own creatures watch it.
	virtual void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) = 0;
	/// The player was seen to want one of the creature's desires, or one of a town's, at a point: the player's own
	/// creature, if it has one and can see the point, thinks the player wants it more by the weight
	virtual void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point) = 0;
	virtual void EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight, const glm::vec3& point) = 0;
	/// CREATURE_SET_KNOWS_ACTION: the creature knows an ordinary skill (kind 0, by its row in the game's table) or a
	/// miracle (kind 1, by its magic type), or no longer does (see creature_watching::SetKnows). A mind not yet set up,
	/// or with a file still to take up, keeps it for when it has, so that it stays over the file's. False for anything
	/// but a creature (nothing is changed)
	virtual bool SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows) = 0;
	/// A nasty miracle struck near the creature, at a point: it grows afraid, then runs from it, or goes to look at it
	/// when it is on the learning leash or not afraid at all, and learns the miracle (by its magic type) by seeing it when
	/// one is given
	virtual void ReactToNastyMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// It goes to look at a nice miracle, and learns it by seeing it when one is given
	virtual void ReactToNiceMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// The game's tables the minds use, once the game's data is loaded
	[[nodiscard]] virtual const creature_mind_tables::Tables* GetTables() = 0;
	/// The action of the game's table the creature's mind carries out now, which its body goes by; none while it does
	/// nothing that counts as one, or for anything but a creature with a mind
	[[nodiscard]] virtual std::optional<uint32_t> CurrentActionOf(entt::entity /*creature*/) { return std::nullopt; }

	/// Plays an action once, unless the creature's body already plays one: mirrored or not as given, else by a coin the
	/// game's random numbers toss (so a caller outside the turn passes it)
	virtual bool PlayAction(entt::entity creature, size_t animation, std::optional<bool> mirrored = std::nullopt) = 0;
	/// Plays a gesture on top of the body, unless one already plays
	virtual bool PlayGesture(entt::entity creature, size_t animation) = 0;
	/// Pulls a face for a few seconds, as told to by hand
	virtual void PullFace(entt::entity creature, size_t animation) = 0;
	/// Pulls the face a feeling or what it is doing calls for, as the creature's mind would; returns the face pulled
	virtual std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) = 0;
	/// Sits down for a while, unless the body plays an action
	virtual bool SitDown(entt::entity creature) = 0;
	/// Gets up from sitting
	virtual void StandUp(entt::entity creature) = 0;
	/// The player's hand let go of the creature having stroked or slapped it, from -1 (slapped) to 1 (stroked): it
	/// stops what it was doing if slapped, warms or cools to the player, and shows its pleasure or sorrow next. Feedback
	/// too slight to count only has it look at the player.
	virtual void ReceiveFeedback(entt::entity creature, float feedback) = 0;
	/// Plays an action at once, as stroking and slapping make it, unless the body is less than a share of the way through
	/// what it plays; a face can be pulled with it. Returns whether it played.
	virtual bool ForceAction(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	                         float interruptsAfter) = 0;

	/// Sees to a need now, in place of whatever it was doing: sleeps until rested, eats something (the nearest food when
	/// none is given), goes and drinks at the nearest water, has a poo, is sick, or faints. Returns whether it could.
	virtual bool Sleep(entt::entity creature) = 0;
	virtual bool Eat(entt::entity creature, std::optional<entt::entity> food) = 0;
	virtual bool Drink(entt::entity creature) = 0;
	virtual bool Poo(entt::entity creature) = 0;
	virtual bool Puke(entt::entity creature) = 0;
	virtual bool Faint(entt::entity creature) = 0;
	/// Wakes it, or ends whatever it is sitting, lying or squatting through
	virtual void Wake(entt::entity creature) = 0;

	/// A fight the creature was in ended, won or lost: fighting satisfies the desire it is for, and its body pays for it
	virtual void FoughtFight(entt::entity creature, bool won) = 0;
	/// The creature tries a miracle at an object, as its casting pose's loop begins or in a fight: in a fight it pays
	/// stamina first; a try short of the sightings it needs fizzles; a miracle it can't cast frustrates it. Whether it
	/// was cast.
	virtual bool TryMiracle(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
	/// For the debug tools: the creature knows a miracle (by its magic type) as if it had learnt it, and is told to cast
	/// one at a thing, going about it as it would by itself
	virtual void KnowMiracle(entt::entity /*creature*/, size_t /*miracle*/) {}
	virtual bool TellCast(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
};

} // namespace openblack::ecs::systems
