/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game object's writer: its head and tail with the values openblack keeps, its players and its global lists. See
// docs/wiki/engine/saved-games.md#writing-the-world-file.

#include <utility>
#include <variant>

#include <fmt/format.h>

#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack::ecs;
using namespace openblack::ecs::saved_world_write;

namespace
{
/// The loaded file's first object is the game object
constexpr openblack::gamesave::ObjectNumber k_GameObject = 1;
} // namespace

void openblack::ecs::saved_world_write::RegisterGameObjectWriter(ClassWriters& writers,
                                                                 std::function<saved_game_save::GameValues()> values)
{
	writers.Register(
	    gamesave::k_GameType, {},
	    [values = std::move(values)](WriteContext& context, entt::entity /*unused*/) -> std::optional<gamesave::SavedRecord> {
		    const auto* kept = context.KeptWorldRecord(k_GameObject);
		    const auto* keptHead = kept != nullptr ? std::get_if<gamesave::GameHead>(kept) : nullptr;
		    // every value openblack does not keep stays as the loaded game's record has it
		    auto head = saved_game_save::HeadOf(values ? values() : saved_game_save::GameValues {},
		                                        keptHead != nullptr ? *keptHead : gamesave::GameHead {});
		    // the eight players: a loaded game's are the objects its record names (each written by the players' writer
		    // when the load listed it); a new game's are the players' writer's eight, else the players' entities, by
		    // their numbers
		    if (keptHead == nullptr && context.Writers().HasSource(gamesave::k_PlayerType))
		    {
			    for (size_t index = 0; index < head.players.size(); ++index)
			    {
				    head.players.at(index) = context.Reference(SourceKey {.type = gamesave::k_PlayerType, .id = index});
			    }
		    }
		    else if (keptHead == nullptr)
		    {
			    context.GetRegistry().Each<const components::Player>(
			        [&context, &head](entt::entity entity, const components::Player& player) {
				        const auto index = static_cast<size_t>(std::to_underlying(player.name));
				        if (index < head.players.size())
				        {
					        head.players.at(index) = context.Reference(entity, gamesave::k_PlayerType);
				        }
			        });
		    }
		    // the global lists, in the game's order: each by its writer, else as the load kept it
		    for (size_t list = 0; list < head.lists.size(); ++list)
		    {
			    if (const auto* write = context.Writers().ListWriterOf(static_cast<GlobalList>(list)); write != nullptr)
			    {
				    head.lists.at(list).numbers = (*write)(context);
			    }
			    else if (keptHead == nullptr)
			    {
				    context.Fail(
				        fmt::format("the game object's global list {} has no writer and no list kept from a load", list));
				    return std::nullopt;
			    }
		    }
		    return head;
	    });
}
