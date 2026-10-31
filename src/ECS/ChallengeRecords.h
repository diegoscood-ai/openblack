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
#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptChallengeSnapshots.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct ChallengeRecord;
}

/// The player's records of their challenges (components::ChallengeRecord), kept by the scripts' snapshot commands
namespace openblack::ecs::challenge_records
{

/// What keeps a thing alive while a record holds it as an argument of its reminder script
struct References
{
	std::function<void(entt::entity)> add;
	std::function<void(entt::entity)> remove;
};

/// What a snapshot did
struct Outcome
{
	entt::entity record;
	/// A snapshot of a challenge already recorded moves the player's alignment by its alignment; the first one doesn't
	std::optional<float> alignment;
	/// The challenge's picture is to be taken: always for a new record, then for a start, or a picture being taken
	bool takesPicture {false};
};

/// A script's snapshot of a challenge. The first starts its record. Later ones replace the record's reminder script,
/// arguments, title, alignment and how far it has got (once done it stays done); a start or a picture being taken also
/// replaces where the camera was, an update keeps it. The record's things are kept alive while it holds them.
Outcome Record(Registry& registry, const script::challenge_snapshots::Snapshot& snapshot,
               script::challenge_snapshots::Call call, uint32_t turn, const References& references);

/// The scripts restart: every record lets go of the things it holds, in the order the challenges started, and goes
void Clear(Registry& registry, const References& references);

/// The record of a challenge, none before it has started
[[nodiscard]] std::optional<entt::entity> Find(const Registry& registry, uint32_t challenge);

/// The records in the order their challenges started, which is also the order of their pictures in the challenge room
[[nodiscard]] std::vector<entt::entity> InOrder(const Registry& registry);

/// A challenge is done once its success is exactly 1
[[nodiscard]] bool IsDone(const components::ChallengeRecord& record);

/// What the challenge room's scroll lists: how many challenges have started, how many are done, and their titles (help
/// text numbers) in the order they started
struct Listing
{
	int32_t discovered {0};
	int32_t completed {0};
	std::vector<uint32_t> titles;
};
[[nodiscard]] Listing List(const Registry& registry);

/// The record's reminder script arguments in the order the script is to be given them, the first given first
[[nodiscard]] std::vector<script::challenge_snapshots::Value> ReminderArguments(const components::ChallengeRecord& record);

/// Where the camera goes as the temple's challenge room replays a challenge: where it was when the challenge's picture
/// was taken, looking where it looked. Only for a replay that moves the camera, while no script has taken the player's
/// camera and interface keys (both switches on), and for a record that has both.
struct View
{
	glm::vec3 position;
	glm::vec3 focus;
};
[[nodiscard]] std::optional<View> ReplayView(const components::ChallengeRecord& record, bool moveCamera,
                                             bool keyShortcutsEnabled);

/// What starting a challenge's reminder does with the script machine and the camera
struct ReminderCalls
{
	/// One of the reminder's arguments onto the script's stack
	std::function<void(const script::challenge_snapshots::Value&)> push;
	/// The camera to where the challenge's picture was taken
	std::function<void(const View&)> moveCamera;
	/// The reminder script started
	std::function<void(std::string_view script)> start;
};
/// A challenge's reminder starts from its record: none before the challenge has one. Its arguments go on the stack, the
/// first given first, then the camera goes to the replay's view (ReplayView), then the script starts (true). A record
/// whose reminder was emptied (a picture update gives none) starts nothing (false), its arguments not pushed
bool StartReminder(const Registry& registry, uint32_t challenge, bool moveCamera, bool keyShortcutsEnabled,
                   const ReminderCalls& calls);

/// How far a snapshot's alignment moves the player's: scaled by the game's most a script may move it, then damped by
/// the player's own alignment (a change the way they already lean counts for less, the other way for more)
[[nodiscard]] float AlignmentChange(float playerAlignment, float snapshotAlignment, float scriptScale);

} // namespace openblack::ecs::challenge_records
