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

#include <array>
#include <optional>
#include <utility>
#include <variant>

#include <InterfaceRecords.h>
#include <WorldReader.h>
#include <fmt/format.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Registry.h"
#include "ECS/SavedScriptObjects.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadCamera.h"
#include "ECS/SavedWorld/LoadPlayerInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"
#include "Magic/Hand/HandMagicFX.h"

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
		    const auto current = values ? values() : saved_game_save::GameValues {};
		    auto head = saved_game_save::HeadOf(current, keptHead != nullptr ? *keptHead : gamesave::GameHead {});
		    // the help bubble's object and each bookmark's object, as kept, none once gone since: the original's help
		    // and bookmarks let go of a deleted object at their next turn (a bookmark stays set at its last place)
		    head.tail.helpObjects.at(1).number = context.UnlessGone(head.tail.helpObjects.at(1).number);
		    for (auto& bookmark : head.tail.bookmarks)
		    {
			    bookmark.object.number = context.UnlessGone(bookmark.object.number);
		    }
		    // the scripts' record of the last thing hit and of what hit it, in the script's part of the tail: as the
		    // physics keeps it now (0 for none, or a thing gone since); without it, as kept, none once gone since, as
		    // the scripts' turn lets go of a thing no longer available
		    if (current.hits.has_value())
		    {
			    const auto numberOf = [&context](std::optional<uint32_t> id) -> gamesave::ObjectNumber {
				    const auto entity = id.has_value() ? static_cast<entt::entity>(*id) : entt::entity {entt::null};
				    return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
			    };
			    head.tail.scriptRefs.at(0).number = numberOf(current.hits->hit);
			    head.tail.scriptRefs.at(1).number = numberOf(current.hits->hitter);
		    }
		    else
		    {
			    for (auto& object : head.tail.scriptRefs)
			    {
				    object.number = context.UnlessGone(object.number);
			    }
		    }
		    // the town the music last played for, in the audio's part of the tail (0 for none, or a town gone since)
		    if (current.music.has_value())
		    {
			    const auto town = current.music->town.has_value() ? static_cast<entt::entity>(*current.music->town)
			                                                      : entt::entity {entt::null};
			    head.tail.audioObject.number =
			        town != entt::null && context.GetRegistry().Valid(town) ? context.Reference(town) : 0;
		    }
		    // the drawn hand from the live hand: its object and the spell seed whose effect plays in it (0 for none, or a
		    // thing gone since), and with a drawn hand its state, hold, place and effect's bone (saved_hand::WriteDrawnHand)
		    if (Locator::handSystem::has_value())
		    {
			    const auto& hand = Locator::handSystem::value();
			    const auto numberOf = [&context](std::optional<entt::entity> thing) -> gamesave::ObjectNumber {
				    return thing.has_value() && context.GetRegistry().Valid(*thing) ? context.Reference(*thing) : 0;
			    };
			    const auto held = numberOf(hand.GetRenderHandObject());
			    const auto spell = numberOf(hand.GetSpellInHand());
			    if (const auto drawn = hand.GetDrawnHandNow(); drawn.has_value())
			    {
				    saved_hand::WriteDrawnHand(head.tail, *drawn, held, spell);
			    }
			    else
			    {
				    head.tail.handObject.number = held;
				    head.tail.handObjects.at(1).number = spell;
			    }
		    }
		    // the particles of the spell in the hand, as the hand's save writes them: the live in-hand effect, none
		    // without one (the original's hand clears them as the seed leaves the hand or the effect ends)
		    if (context.Writers().HasSource(gamesave::ParticleInterfaceRecord::k_Type))
		    {
			    const auto inHand = magic::hand_fx::InHandEffectOf();
			    head.tail.handObjects.at(0).number =
			        inHand.has_value()
			            ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = inHand->effect})
			            : 0;
		    }
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
		    // a new game's camera, the first of the two objects its tail names, when its writer can write it
		    if (keptHead == nullptr && context.Writers().HasSource(gamesave::CameraRecord::k_Type) &&
		        saved_camera::NewCameraWritable())
		    {
			    head.tail.objects.at(0) = {
			        context.Reference(SourceKey {.type = gamesave::CameraRecord::k_Type, .id = saved_camera::k_CameraId})};
		    }
		    // the audio's music attached to things, head first, when their writer can write them
		    if (context.Writers().HasSource(gamesave::ThingMusicInfoRecord::k_Type))
		    {
			    head.tail.sounds.numbers.clear();
			    for (const auto key : context.Keys(gamesave::ThingMusicInfoRecord::k_Type))
			    {
				    head.tail.sounds.numbers.push_back(
				        context.Reference(SourceKey {.type = gamesave::ThingMusicInfoRecord::k_Type, .id = key}));
			    }
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
		    // the scripts' object table, from the objects the scripts name now (the machine's file holds their places):
		    // each place its object, whether a script made it and how many variables hold it; an object gone since
		    // keeps its place with none. With no scripts' machine, the table stays as the loaded game's record has it
		    if (!saved_script_objects::HasMachine())
		    {
			    return head;
		    }
		    const auto places = saved_script_objects::Current();
		    if (!places.has_value())
		    {
			    context.Fail(fmt::format("the scripts name more objects than the {} places of their table",
			                             saved_script_objects::k_Places));
			    return std::nullopt;
		    }
		    const auto& registry = context.GetRegistry();
		    for (size_t place = 0; place < head.tail.scriptObjects.size(); ++place)
		    {
			    auto& entry = head.tail.scriptObjects.at(place);
			    entry = {};
			    if (place >= places->size())
			    {
				    continue;
			    }
			    const auto entity = static_cast<entt::entity>(places->at(place));
			    if (!registry.Valid(entity))
			    {
				    continue;
			    }
			    entry.object.number = ScriptTableObject(context, entity);
			    if (const auto* slot = registry.TryGet<const components::ScriptHeld>(entity); slot != nullptr)
			    {
				    entry.word = slot->createdByScript ? 1U : 0U;
				    entry.byte = slot->references;
			    }
		    }
		    return head;
	    });
}

std::array<entt::entity, 2> openblack::ecs::saved_world_write::LoadedScriptHits(const Registry& registry,
                                                                                const gamesave::WorldRead& world,
                                                                                const LoadedObjects& loaded)
{
	std::array<entt::entity, 2> hits {entt::null, entt::null};
	const auto* head = !world.objects.empty() && world.objects.front().has_value()
	                       ? std::get_if<gamesave::GameHead>(&*world.objects.front())
	                       : nullptr;
	if (head == nullptr)
	{
		return hits;
	}
	for (size_t i = 0; i < hits.size(); ++i)
	{
		const auto number = head->tail.scriptRefs.at(i).number;
		const auto found = number != 0 ? loaded.find(number) : loaded.end();
		if (found != loaded.end() && found->second.entity != entt::null && registry.Valid(found->second.entity))
		{
			hits.at(i) = found->second.entity;
		}
	}
	return hits;
}
