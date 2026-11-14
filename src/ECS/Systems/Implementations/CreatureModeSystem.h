/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>

#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class CameraModel;
class CreatureCameraModel;
} // namespace openblack

namespace openblack::ecs::systems
{

class CreatureModeSystem final: public CreatureModeSystemInterface
{
public:
	/// A player's own creature, the one C locks onto, or none
	using CreatureLookup = std::function<std::optional<entt::entity>(PlayerNames player)>;

	/// The players' creatures are found through the lookup given; without one no player has a creature
	explicit CreatureModeSystem(CreatureLookup playersCreature = {});
	~CreatureModeSystem() override;
	CreatureModeSystem(const CreatureModeSystem&) = delete;
	CreatureModeSystem& operator=(const CreatureModeSystem&) = delete;
	CreatureModeSystem(CreatureModeSystem&&) = delete;
	CreatureModeSystem& operator=(CreatureModeSystem&&) = delete;

	void Update(std::chrono::microseconds dt, const Frame& frame) override;
	void PressCreatureKey() override;
	bool Enter(entt::entity creature) override;
	bool Restore(entt::entity thing, const creature_follow::View& view, const creature_follow::Style& style) override;
	void Leave() override;
	void CheckFollow() override;
	bool UncoverFromPlayersCamera(uint32_t playersMode) override;
	[[nodiscard]] std::optional<uint32_t> PushedAt() const override
	{
		return _creature.has_value() ? std::optional(_pushed) : std::nullopt;
	}
	void LockOnNext(entt::entity creature) override { _lockOnNext = creature; }
	[[nodiscard]] bool IsActive() const override { return _creature.has_value(); }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return _creature; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override;
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override;
	[[nodiscard]] std::optional<creature_follow::Style> GetStyle() const override;
	void ClearView() override;

private:
	[[nodiscard]] bool OwnsCamera() const;
	/// Hands the player's camera back, if the camera is with this mode's
	void ReleaseCamera();
	/// Whether the creature can still be followed: it is there, and nothing else needs the camera
	[[nodiscard]] bool StillValid() const;
	/// Whether the thing followed is there to be followed (a thing that is not a creature also available)
	[[nodiscard]] bool FollowedThere() const;
	/// Whether the camera may be taken for a follow now
	[[nodiscard]] bool MayTakeCamera() const;
	/// Takes the camera over from the player's for a follow of the thing, a creature or not (`thing` set), starting
	/// from where the camera is heading for, on a thing of that height standing at the position
	bool TakeCamera(entt::entity followed, glm::vec3 position, float height, bool thing);
	/// Follows a thing that is not a creature, as a loaded follow of it does
	bool EnterThing(entt::entity thing);
	/// The follow's place left the camera fence: a new player's camera goes on top, from where the camera is, and the
	/// follow stays under it with its creature (script_camera::PushPlayersModeOver)
	void GoUnderPlayersCamera();
	/// The follow under a new player's mode comes off the camera's stack, and the player's camera under it is what that
	/// mode has under it from now on (script_camera::KeepUnder)
	void DropBuriedFollow();
	/// Reads C, and locks onto the creature a double click was taken on
	void ReadKeys(const Frame& frame);

	CreatureLookup _playersCreature;
	std::optional<entt::entity> _creature;
	/// The creature the player's camera took a double click on, locked onto in the next update
	std::optional<entt::entity> _lockOnNext;
	/// The camera's model while locked on, and the player's own, given back on leaving
	CreatureCameraModel* _model {nullptr};
	std::unique_ptr<CameraModel> _playerModel;
	/// Leaving was asked for while another had the camera; it is given back once this mode has it again
	bool _leaving {false};
	/// The follow under a new player's mode, held here until that mode is popped, and that mode's number (0: none)
	std::unique_ptr<CameraModel> _buried;
	uint32_t _buriedUnder {0};
	/// The follow's number in the order of the camera's other modes' pushes, as it locked on
	uint32_t _pushed {0};
	/// Whether the hand gripped the land last frame, so that only a fresh grip counts
	bool _wasGripping {false};
	/// The thing followed is not a creature: it is read as the follow reads things (script_camera::Thing)
	bool _thing {false};
};

} // namespace openblack::ecs::systems
