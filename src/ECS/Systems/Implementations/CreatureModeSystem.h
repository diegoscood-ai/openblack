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

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

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
	void ReadCreatureKey() override;
	bool Enter(entt::entity creature) override;
	bool Restore(entt::entity thing, const creature_follow::View& view, const creature_follow::Style& style) override;
	void Leave() override;
	void LeaveAll() override;
	void LeaveAllBehindTemple(Camera& world) override;
	bool RemoveInvalid() override;
	bool TakeBackFromUnderPlayersCamera(uint32_t playersMode) override;
	bool KeepUnderPlayersMode(uint32_t playersMode) override;
	bool GoUnderNewPlayersMode() override;
	void DeleteFollow(uint64_t follow) override;
	void LockOnNext(entt::entity creature) override { _lockOnNext = creature; }
	[[nodiscard]] bool IsActive() const override { return Newest() != nullptr; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<entt::entity> GetCreatureInView() const override;
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override;
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override;
	[[nodiscard]] std::optional<creature_follow::Style> GetStyle() const override;
	[[nodiscard]] std::optional<CurrentFollow> GetCurrentFollow() const override;
	void ClearView() override;

private:
	/// One follow: a mode of its own on the camera's stack of modes, by its key
	struct Follow
	{
		/// Its entry's key on the camera's stack (camera_stack::StackedMode::follow)
		uint64_t key {0};
		entt::entity followed {entt::null};
		/// The thing followed is not a creature: it is read as the follow reads things (script_camera::Thing)
		bool thing {false};
		/// Its camera's model, the camera's while the camera has it
		CreatureCameraModel* model {nullptr};
		/// Its model while a mode with a camera of its own is over it (a newer follow, a new player's mode), held here
		std::unique_ptr<CameraModel> held;
		/// The player's camera it took the camera over from, given back as it goes; none when it took the camera over from
		/// the follow under it, which holds its own model meanwhile
		std::unique_ptr<CameraModel> wentOver;
		/// The number of the new player's mode put over it, 0 while none is
		uint32_t under {0};
		/// Off the stack while a camera that cannot be left (the temple's, the editor's, the creature interaction's) has
		/// its model: the camera goes back once that camera hands it on
		bool leaving {false};
	};
	/// What the end of a follow did
	struct Dropped
	{
		/// Its entry was the current mode
		bool current {false};
		/// The camera had its model, and has the one under it now
		bool hadCamera {false};
	};

	/// The newest follow still on the stack; none while there is none
	[[nodiscard]] const Follow* Newest() const;
	[[nodiscard]] Follow* Newest();
	[[nodiscard]] std::optional<std::size_t> NewestIndex() const;
	/// The camera's model is this follow's
	[[nodiscard]] bool HasCameraModel(const Follow& follow) const;
	/// The camera's model is the newest follow's
	[[nodiscard]] bool OwnsCamera() const;
	/// The newest follow is the current mode of the camera's stack and the camera has its model: its update runs
	[[nodiscard]] bool HasTheView() const;
	/// A camera that cannot be left has the follow's model: neither the camera's nor held here
	[[nodiscard]] bool TakenByAnother(const Follow& follow) const;
	/// The follow at `index` ends: its entry comes off the camera's stack, and what was under it is under the mode over
	/// it from then on: the camera's model goes back to the one under it when the camera has its model; under a new
	/// player's mode, the model under it is kept for that mode; under a newer follow, that follow takes it over. With its
	/// model taken by a camera that cannot be left, it waits off the stack (Follow::leaving)
	Dropped Drop(std::size_t index);
	/// The newest follow ends as Leave ends it; true when it was the current mode and the mode now current under it is
	/// neither a follow (started again here) nor a new player's mode started from the camera as it is
	bool PopNewest();
	/// The player's mode current again after a follow over it was popped starts again, as the pop of a mode restarts the
	/// one under it: it flies back to the view it had
	void RestartPlayersModeUnder();
	/// C over a script's camera mode that follows the player's creature `yours` pops it, as it lets go of a follow: true
	/// when it did
	bool LetGoOfScriptFollow(entt::entity yours);
	/// The model under the follow at `index`, taken: the player's camera it went over, or the model of the follow under
	/// it that it took the camera from
	std::unique_ptr<CameraModel> TakeModelUnder(std::size_t index);
	/// Hands the camera's model on to the one under the leaving follows once the camera has their model again
	void ReleaseLeaving();
	/// Whether the thing a follow follows is there to be followed (a thing that is not a creature also available)
	[[nodiscard]] bool FollowedThere(const Follow& follow) const;
	/// Whether the camera may be taken for a follow now
	[[nodiscard]] bool MayTakeCamera() const;
	/// A new follow of the thing, a creature or not (`thing` set), on top of the current mode, the follow before it staying
	/// under it; starting from where the camera is heading for, on a thing of that height standing at the position
	bool TakeCamera(entt::entity followed, glm::vec3 position, float height, bool thing);
	/// Follows a thing that is not a creature, as a loaded follow of it does
	bool EnterThing(entt::entity thing);
	/// A new player's camera from the camera as it is goes on top of the newest follow, the camera's model, which is held
	/// under it, with its creature (script_camera::PushPlayersModeOver): its place left the camera fence, the cursor keys
	/// alone or a grip of the land asked for the player's camera, or C let go of a follow over it
	void GoUnderPlayersCamera();
	/// Reads C, and locks onto the creature a double click was taken on
	void ReadKeys(const Frame& frame);

	CreatureLookup _playersCreature;
	/// The follows, in the order they were pushed, the newest last
	std::vector<Follow> _follows;
	/// The key the next follow is pushed with
	uint64_t _nextKey {1};
	/// The creature the player's camera took a double click on, locked onto in the next update
	std::optional<entt::entity> _lockOnNext;
};

} // namespace openblack::ecs::systems
