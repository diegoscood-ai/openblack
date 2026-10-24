/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerPlayerSystem.h"

#include <cstdint>

#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/CreatureMimic.h"
#include "ECS/CreaturePose.h"
#include "ECS/LastDropped.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/WindResistance.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/CreatureFileWriter.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// The game's own world, through the registry and the systems in the locator
class GameWorld final: public computer_player::World
{
public:
	[[nodiscard]] PlayerNames LocalPlayer() const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	}

	[[nodiscard]] bool IsComputerPlayer(PlayerNames player) const override
	{
		return magic::players::ReadMagicOf(player).playerType == PlayerMagic::k_ComputerPlayerType;
	}

	[[nodiscard]] std::optional<entt::entity> Creature(PlayerNames player) const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	}

	[[nodiscard]] float DrawnSize(entt::entity creature) const override
	{
		return ecs::creature_pose::DrawnSize(std::as_const(Locator::entitiesRegistry::value()), creature);
	}

	void SetSize(entt::entity creature, float size) override
	{
		// It grows or shrinks to the size at once, as a script that sets its scale does
		ecs::player_creature::SetCreatureScale(Locator::entitiesRegistry::value(), creature, size);
	}

	[[nodiscard]] bool HasTemple(PlayerNames player) const override
	{
		bool temple = false;
		std::as_const(Locator::entitiesRegistry::value()).Each<const Temple>([&temple, player](const Temple& building) {
			temple = temple || building.owner == player;
		});
		return temple;
	}

	[[nodiscard]] std::optional<glm::vec3> Pen(PlayerNames player) const override
	{
		const auto pen = ecs::player_creature::TemplePenOf(player);
		if (!pen.has_value())
		{
			return std::nullopt;
		}
		// The creature stands on the land at the pen, as a creature's home at its temple does
		auto point = ecs::player_creature::HomeOnGround(*pen, 0.0f);
		if (Locator::terrainSystem::has_value())
		{
			point.y = Locator::terrainSystem::value().GetHeightAt({point.x, point.z});
		}
		return point;
	}

	[[nodiscard]] bool Skirmish() const override
	{
		// openblack starts no skirmish yet, as the temple destruction's query of a playground game also says
		return false;
	}

	[[nodiscard]] computer_player::CanCast Miracles(PlayerNames player) const override
	{
		computer_player::CanCast canCast {};
		for (size_t index = 1; index < canCast.size(); ++index)
		{
			canCast.at(index) = magic::players::IsMagicTypeEnabled(player, static_cast<MagicType>(index));
		}
		return canCast;
	}

	void MakeCreature(PlayerNames player, CreatureType species, const glm::vec3& position) override
	{
		// the same step as a script's LOAD_CREATURE after its checks
		ecs::player_creature::MakeFromMindFile(
		    static_cast<int32_t>(openblack::script::property_rules::ScriptCreatureType(species)),
		    computer_player::k_CreatureMindFile, player, glm::vec2(position.x, position.z));
	}

	void Teach(entt::entity creature, MagicType miracle) override
	{
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().SetKnowsAction(
			    creature, static_cast<uint32_t>(CreatureActionLearningType::Magic), static_cast<uint32_t>(miracle), true);
		}
	}

	uint32_t GameRand(uint32_t range) override { return game_random::GameRand(range); }

	float GameFloatRand(float most) override { return game_random::GameFloatRand(most); }
};

/// The height of the game's land, none without a land
class GameGround final: public computer_hand::Ground
{
public:
	[[nodiscard]] float AltitudeAt(int32_t mapX, int32_t mapZ) const override
	{
		if (!Locator::terrainSystem::has_value())
		{
			return 0.0f;
		}
		return Locator::terrainSystem::value().GetHeightAt({map_coords::ToMetres(mapX), map_coords::ToMetres(mapZ)});
	}
};

/// The stages of the hand's steps in the game
class GameHandRunner final: public computer_hand::StageRunner
{
public:
	explicit GameHandRunner(ecs::Registry& registry)
	    : _registry(registry)
	{
	}

	computer_hand::StageResult Run(ComputerHand& hand, HandStep& step, uint8_t stage) override
	{
		using computer_hand::StageResult;
		switch (step.type)
		{
		case HandSubAction::MoveHandToPos:
		case HandSubAction::MoveHandToPosDirectYInterpolation:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, FirstPoint(step));
				return StageResult::Done;
			}
			if (stage == 1)
			{
				return step.type == HandSubAction::MoveHandToPos ? computer_hand::MoveOverGround(hand, _ground)
				                                                 : computer_hand::MoveStraight(hand);
			}
			return StageResult::Done;
		case HandSubAction::MoveHandToObject:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, PointOfThing(FirstThing(step)));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::MoveToThing(hand, _ground) : StageResult::Done;
		case HandSubAction::PickUpObject:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, PointOfThing(FirstThing(step)));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::PickUpTurn(hand, FirstThing(step), *this) : StageResult::Done;
		case HandSubAction::DropObjectInHandAtPos:
			return stage == 1 ? computer_hand::DropTurn(hand, *this) : StageResult::Done;
		case HandSubAction::SetHandToPos:
		case HandSubAction::SetHandToPosDirectY:
			// Only the stage that goes on does anything: the hand is put on the point
			if (stage == 1)
			{
				computer_hand::PutAt(hand, FirstPoint(step), step.type == HandSubAction::SetHandToPos ? &_ground : nullptr);
			}
			return StageResult::Done;
		default:
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Computer hand: the step {} is not ported", static_cast<int>(step.type));
			return StageResult::Failed;
		}
	}

	void LetGo(ComputerHand& hand) override
	{
		const auto object = std::exchange(hand.heldObject, entt::null);
		if (object == entt::null || !_registry.Valid(object))
		{
			return;
		}
		// The thing is let go where the hand is, with no speed, as a gentle drop from the player's hand, credited to the
		// hand's own player
		if (auto* transform = _registry.TryGet<Transform>(object); transform != nullptr)
		{
			transform->position = hand.point;
		}
		ecs::physics::from_hand::Throw(object, glm::vec3(0.0f), false, computer_hand::LetGoCreditedTo(_player));
		ecs::wind_resistance::AfterThrowFromHand(object, _player);
		// Then a scaffold's own part of leaving the hand, as for the player's hand: what it leaves over goes back into
		// this hand (a computer player's leaves nothing)
		if (ecs::scaffolds::IsScaffold(object))
		{
			computer_hand::TakeLeftOver(hand, ecs::scaffolds::OnLeftHand(object, _player), *this);
		}
		// and the player's interface keeps it as the last thing it let go of; the creature of whoever let go of it last
		// may copy them for where it is
		ecs::last_dropped::RecordDrop(_registry, _player, object);
		ecs::creature_mimic::ConsiderObjectLands(object);
	}

	[[nodiscard]] bool IsGone(entt::entity thing) const override { return !_registry.Valid(thing); }

	[[nodiscard]] bool CanBePickedUp(entt::entity thing) const override
	{
		if (!_registry.Valid(thing))
		{
			return false;
		}
		// A thing already in a hand is refused: the player's hand's or any computer player's
		const auto playersHand =
		    Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().GetHeld() : std::nullopt;
		std::vector<entt::entity> computerHands;
		std::as_const(_registry).Each<const ComputerHand>([&computerHands](const ComputerHand& hand) {
			if (hand.heldObject != entt::null)
			{
				computerHands.push_back(hand.heldObject);
			}
		});
		return computer_hand::MayBePickedUp(thing, playersHand, computerHands);
	}

	bool TakeIntoHand(ComputerHand& /*hand*/, entt::entity thing) override
	{
		// The same steps as the player's hand takes a thing, for the computer player
		if (!_registry.Valid(thing) || !_registry.AllOf<Transform>(thing))
		{
			return false;
		}
		if (Locator::dynamicsSystem::has_value())
		{
			Locator::dynamicsSystem::value().RemoveObject(thing);
		}
		ecs::villager_physics::InterfaceSetInMagicHand(thing);
		if (ecs::scaffolds::IsScaffold(thing))
		{
			ecs::scaffolds::OnPickedUp(thing, _player);
		}
		ecs::object_physics::ArtefactTaken(thing, _player);
		if (!ecs::physics::from_hand::TakeOutOfPhysicsForHand(thing))
		{
			return false;
		}
		if (ecs::map_cells::IsObjectInMap(thing))
		{
			ecs::map_cells::RemoveMapObject(thing);
		}
		return true;
	}

	/// The player whose hand takes things
	void SetPlayer(PlayerNames player) { _player = player; }

	/// Where the hand takes the thing: its place on the map grid, at its height above the land there
	[[nodiscard]] glm::vec3 PointOfThing(entt::entity thing) const
	{
		const auto* transform = _registry.Valid(thing) ? _registry.TryGet<const Transform>(thing) : nullptr;
		if (transform == nullptr)
		{
			return glm::vec3(0.0f);
		}
		const auto mapX = map_coords::ToFixed(transform->position.x);
		const auto mapZ = map_coords::ToFixed(transform->position.z);
		// (approximate) openblack keeps a thing's height in the world, so its height above the land is taken from it
		const auto aboveGround = transform->position.y - _ground.AltitudeAt(mapX, mapZ);
		return computer_hand::PointFromMap(mapX, mapZ, aboveGround, _ground);
	}

private:
	[[nodiscard]] static glm::vec3 FirstPoint(const HandStep& step)
	{
		return step.arguments.empty() ? glm::vec3(0.0f) : step.arguments.front().point;
	}
	[[nodiscard]] static entt::entity FirstThing(const HandStep& step)
	{
		return step.arguments.empty() ? entt::null : step.arguments.front().thing;
	}

	ecs::Registry& _registry;
	GameGround _ground;
	PlayerNames _player {PlayerNames::NEUTRAL};
};

/// What the game knows of the things a forced action acts on
class GameThingFacts final: public computer_hand::ThingFacts
{
public:
	explicit GameThingFacts(const ecs::Registry& registry)
	    : _registry(registry)
	{
	}

	[[nodiscard]] bool IsObject(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && ecs::script_type::IsObject(thing);
	}

	[[nodiscard]] bool HasPlace(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<Transform>(thing);
	}

	[[nodiscard]] std::optional<glm::vec3> MarkerPoint(entt::entity thing) const override
	{
		if (thing == entt::null || !_registry.Valid(thing) || !_registry.AllOf<ScriptMarker>(thing))
		{
			return std::nullopt;
		}
		const auto* transform = _registry.TryGet<const Transform>(thing);
		return transform != nullptr ? std::optional<glm::vec3>(transform->position) : std::nullopt;
	}

private:
	const ecs::Registry& _registry;
};

/// The thing held by the hand is carried where the hand is; a thing that is gone is no longer held
void CarryHeld(ecs::Registry& registry, ComputerHand& hand)
{
	if (hand.heldObject == entt::null)
	{
		return;
	}
	if (!registry.Valid(hand.heldObject))
	{
		hand.heldObject = entt::null;
		return;
	}
	if (auto* transform = registry.TryGet<Transform>(hand.heldObject); transform != nullptr)
	{
		transform->position = hand.point;
	}
}

} // namespace

ComputerPlayerSystem::ComputerPlayerSystem()
    : _world(std::make_unique<GameWorld>())
{
}

ComputerPlayerSystem::ComputerPlayerSystem(std::unique_ptr<computer_player::World> world)
    : _world(std::move(world))
{
}

ComputerPlayerSystem::~ComputerPlayerSystem() = default;

entt::entity ComputerPlayerSystem::PlayerEntity(PlayerNames player)
{
	auto entity = magic::players::EntityOf(player);
	// The player made here is known to the players at once, so that the creature made for it next is its creature
	if (entity == entt::null)
	{
		entity = ecs::archetypes::PlayerArchetype::Create(player);
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().AddPlayer(entity);
		}
	}
	return entity;
}

ComputerPlayer& ComputerPlayerSystem::MindOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = PlayerEntity(player);
	auto* mind = registry.TryGet<ComputerPlayer>(entity);
	return mind != nullptr ? *mind : registry.AssignState<ComputerPlayer>(entity);
}

void ComputerPlayerSystem::SwitchOn(PlayerNames player, bool on)
{
	computer_player::SwitchOn(MindOf(player), on);
}

void ComputerPlayerSystem::Pause(PlayerNames player, bool paused)
{
	computer_player::Pause(MindOf(player), paused);
	// The pause holds the hand too, and its steps take the player's speed again
	if (auto* hand = HandOf(player); hand != nullptr)
	{
		computer_hand::Pause(*hand, paused);
	}
}

void ComputerPlayerSystem::SetCreatureLike(PlayerNames player, PlayerNames like)
{
	MindOf(player).creatureLike = like;
}

void ComputerPlayerSystem::ProcessTurn()
{
	const auto wideScreenOn =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<PlayerNames, entt::entity>> minds;
	std::as_const(registry).Each<const Player, const ComputerPlayer>(
	    [&minds](entt::entity entity, const Player& player, const ComputerPlayer& /*mind*/) {
		    // The neutral player has no computer mind of its own
		    if (player.name != PlayerNames::NEUTRAL)
		    {
			    minds.emplace_back(player.name, entity);
		    }
	    });
	// A start can make a creature, so each mind takes its turn after the players were found
	GameHandRunner runner(registry);
	for (const auto& [name, entity] : minds)
	{
		runner.SetPlayer(name);
		// Whatever the mind does, its queued actions and the hand's steps go when a thing they act on is gone
		if (auto* hand = registry.TryGet<ComputerHand>(entity); hand != nullptr)
		{
			computer_hand::SweepQueue(*hand, runner);
			computer_hand::SweepGone(*hand, runner);
		}
		const auto* mind = registry.TryGet<ComputerPlayer>(entity);
		const auto* hand = registry.TryGet<ComputerHand>(entity);
		const bool movedByScript = hand != nullptr && computer_hand::MovedByScript(*hand);
		if (mind == nullptr || !mind->switchedOn || mind->paused ||
		    computer_player::HeldByWideScreen(wideScreenOn, movedByScript))
		{
			continue;
		}
		if (computer_player::DueToStart(*mind))
		{
			computer_player::Start(*_world, name, registry.Get<ComputerPlayer>(entity));
		}
		// The hand goes through its steps while it has some or a script holds it
		if (auto* moving = registry.TryGet<ComputerHand>(entity); moving != nullptr && computer_hand::TakesItsTurn(*moving))
		{
			computer_hand::Step(*moving, runner);
			CarryHeld(registry, *moving);
		}
		// A free hand takes the next queued action
		else if (moving != nullptr)
		{
			if (auto next = computer_hand::TakeNext(*moving); next.has_value())
			{
				Force(name, *moving, next->name, next->first, next->second);
			}
		}
	}
}

ComputerHand* ComputerPlayerSystem::HandOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = magic::players::EntityOf(player);
	if (entity == entt::null)
	{
		return nullptr;
	}
	auto* hand = registry.TryGet<ComputerHand>(entity);
	return hand != nullptr ? hand : &registry.AssignState<ComputerHand>(entity);
}

const ComputerHand* ComputerPlayerSystem::ReadHandOf(PlayerNames player) const
{
	const auto entity = magic::players::EntityOf(player);
	return entity == entt::null ? nullptr
	                            : std::as_const(Locator::entitiesRegistry::value()).TryGet<const ComputerHand>(entity);
}

void ComputerPlayerSystem::TakeHand(PlayerNames player, ComputerHand& hand, HandStep step, float speed)
{
	GameHandRunner runner(Locator::entitiesRegistry::value());
	runner.SetPlayer(player);
	computer_hand::TakeForScript(hand, std::move(step), speed, runner);
}

void ComputerPlayerSystem::MoveHand(PlayerNames player, const glm::vec3& point, float speed, bool overGround)
{
	if (magic::players::EntityOf(player) == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_COMPUTER_PLAYER_POSITION: no computer player");
		return;
	}
	// The move switches the mind on, and lets a paused mind go on
	auto& mind = MindOf(player);
	computer_player::SwitchOn(mind, true);
	if (mind.paused)
	{
		Pause(player, false);
	}
	if (auto* hand = HandOf(player); hand != nullptr)
	{
		const auto type = overGround ? HandSubAction::MoveHandToPos : HandSubAction::MoveHandToPosDirectYInterpolation;
		TakeHand(player, *hand, computer_hand::StepToPoint(type, point), speed);
	}
}

void ComputerPlayerSystem::PutHand(PlayerNames player, const glm::vec3& point, bool overGround)
{
	auto* hand = HandOf(player);
	if (hand == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_COMPUTER_PLAYER_POSITION: no computer player");
		return;
	}
	const auto type = overGround ? HandSubAction::SetHandToPos : HandSubAction::SetHandToPosDirectY;
	TakeHand(player, *hand, computer_hand::StepToPoint(type, point), hand->playerSpeed);
}

std::optional<glm::vec3> ComputerPlayerSystem::HandOnGround(PlayerNames player) const
{
	if (magic::players::EntityOf(player) == entt::null)
	{
		return std::nullopt;
	}
	// A hand that nothing moved yet is at its first place on the map grid
	const auto* hand = ReadHandOf(player);
	return computer_hand::OnGround(hand != nullptr ? *hand : ComputerHand {}, GameGround());
}

bool ComputerPlayerSystem::IsHandReady(PlayerNames player) const
{
	if (magic::players::EntityOf(player) == entt::null)
	{
		return false;
	}
	const auto* hand = ReadHandOf(player);
	return hand == nullptr || computer_hand::IsReady(*hand);
}

void ComputerPlayerSystem::SetHandSpeed(PlayerNames player, float speed)
{
	if (auto* hand = HandOf(player); hand != nullptr)
	{
		computer_hand::SetSpeed(*hand, speed);
	}
}

void ComputerPlayerSystem::Release(PlayerNames player)
{
	if (magic::players::EntityOf(player) != entt::null)
	{
		Pause(player, false);
	}
}

bool ComputerPlayerSystem::Force(PlayerNames player, ComputerHand& hand, std::string_view name, entt::entity first,
                                 entt::entity second)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto valid = [&registry](entt::entity thing) { return thing != entt::null && registry.Valid(thing); };
	if (computer_hand::FindMindAction(name, _rowNames) == computer_hand::MindAction::NotPorted)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Computer player action {} is not ported", name);
	}
	// The force goes through the player's mind, as the original's does: its nodes keep what they are given
	auto& mind = ComputerMindOf(player);
	GameHandRunner runner(registry);
	runner.SetPlayer(player);
	const GameThingFacts facts(registry);
	return computer_hand::ForceAction(hand, mind, name, valid(first) ? first : entt::null, valid(second) ? second : entt::null,
	                                  game_clock::Turn(), runner, facts, _rowNames);
}

bool ComputerPlayerSystem::ForceAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second)
{
	auto* hand = HandOf(player);
	return hand != nullptr && Force(player, *hand, name, first, second);
}

void ComputerPlayerSystem::QueueAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second)
{
	if (auto* hand = HandOf(player); hand != nullptr)
	{
		computer_hand::QueueAction(*hand, std::string(name), first, second);
	}
}

void ComputerPlayerSystem::ClearActions(PlayerNames player)
{
	if (auto* hand = HandOf(player); hand != nullptr)
	{
		GameHandRunner runner(Locator::entitiesRegistry::value());
		runner.SetPlayer(player);
		computer_hand::Clear(*hand, runner);
		hand->queue.clear();
	}
}

ComputerMind& ComputerPlayerSystem::ComputerMindOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = PlayerEntity(player);
	if (auto* mind = registry.TryGet<ComputerMind>(entity); mind != nullptr)
	{
		return *mind;
	}
	// Every player has a mind from the start of the game; openblack makes it when a script first sets it, or takes up the
	// one the player had on the land before
	return registry.AssignState<ComputerMind>(entity, computer_player::TakeUp(_keptMinds.at(static_cast<size_t>(player))));
}

const ComputerMind* ComputerPlayerSystem::ReadComputerMindOf(PlayerNames player) const
{
	if (const auto entity = magic::players::EntityOf(player); entity != entt::null)
	{
		if (const auto* mind = std::as_const(Locator::entitiesRegistry::value()).TryGet<const ComputerMind>(entity);
		    mind != nullptr)
		{
			return mind;
		}
	}
	const auto& kept = _keptMinds.at(static_cast<size_t>(player));
	return kept.has_value() ? &*kept : nullptr;
}

void ComputerPlayerSystem::SetPersonality(PlayerNames player, std::string_view name, float value)
{
	computer_player::SetPersonality(ComputerMindOf(player), name, value, _rowNames);
}

void ComputerPlayerSystem::SetSuppression(PlayerNames player, std::string_view name, float value)
{
	computer_player::SetSuppression(ComputerMindOf(player), name, value, _rowNames);
}

void ComputerPlayerSystem::SetAttitude(PlayerNames player, PlayerNames toward, float value)
{
	computer_player::SetAttitude(ComputerMindOf(player), toward, value);
}

float ComputerPlayerSystem::Attitude(PlayerNames player, PlayerNames toward) const
{
	// A mind no script has set yet feels as every mind starts
	const auto* mind = ReadComputerMindOf(player);
	return mind != nullptr ? computer_player::Attitude(*mind, toward) : computer_player::Attitude(ComputerMind {}, toward);
}

void ComputerPlayerSystem::LoadPersonality(PlayerNames player, std::string_view path)
{
	// The file is read afresh each time, as a script may just have written it. The original reads nothing when it cannot
	// open the file, and checks nothing of what it reads
	if (path.empty() || !Locator::filesystem::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto fixedPath = filesystem::FileSystemInterface::FixPath(std::string(path));
	if (!fileSystem.Exists(fixedPath))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "Computer player personality file {} not found", path);
		return;
	}
	std::vector<uint8_t> file;
	try
	{
		file = fileSystem.ReadAll(fixedPath);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Could not read computer player personality file {}: {}", path, e.what());
		return;
	}
	computer_player::ReadPersonality(file, ComputerMindOf(player), _rowNames);
}

void ComputerPlayerSystem::SavePersonality(PlayerNames player, std::string_view path)
{
	if (path.empty() || !Locator::filesystem::has_value())
	{
		return;
	}
	// A mind no script has set yet is written as every mind starts
	const auto* mind = ReadComputerMindOf(player);
	const auto file = computer_player::WritePersonality(mind != nullptr ? *mind : computer_player::MakeMind(), _rowNames);
	resources::WriteFile(Locator::filesystem::value(), filesystem::FileSystemInterface::FixPath(std::string(path)), file);
}

void ComputerPlayerSystem::KeepForNextLand()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<PlayerNames, entt::entity>> minds;
	std::as_const(registry).Each<const Player, const ComputerMind>(
	    [&minds](entt::entity entity, const Player& player, const ComputerMind& /*mind*/) {
		    minds.emplace_back(player.name, entity);
	    });
	// A player that had no entity on this land keeps the mind it kept before; the land's clear takes the entities
	for (const auto& [name, entity] : minds)
	{
		auto& kept = _keptMinds.at(static_cast<size_t>(name));
		kept = std::move(registry.Get<ComputerMind>(entity));
		computer_player::StartNewLand(*kept);
	}
}
