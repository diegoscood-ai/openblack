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
#include <cstdlib>

#include <algorithm>
#include <array>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Debug/TraceLog.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerSeen.h"
#include "ECS/CreatureMimic.h"
#include "ECS/CreaturePose.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HandActionPoint.h"
#include "ECS/HeldApply.h"
#include "ECS/Influence/Influence.h"
#include "ECS/LastDropped.h"
#include "ECS/Life.h"
#include "ECS/LivingFootpath.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptTypes.h"
#include "ECS/SkirmishGame.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/ComputerPlayerGameWorld.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/WindResistance.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCastData.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/MagicTables.h"
#include "Particles/Utility.h"
#include "Resources/CreatureFileWriter.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// OPENBLACK_COMPUTER_MIND_TRACE=1: every computer god's mind traced, as the debug window's switch traces one: each
/// action it runs, each chosen action openblack lacks and each turn of its mind (docs/wiki/project/debugging.md)
[[nodiscard]] bool ComputerMindTraceOn()
{
	static const bool k_Trace = std::getenv("OPENBLACK_COMPUTER_MIND_TRACE") != nullptr;
	return debug_env::TraceLogs(k_Trace);
}

/// Whether a mind's actions and turns are logged: the debug window's switch for it, or the trace of every mind
[[nodiscard]] bool IsTraced(const ComputerMind& mind)
{
	return mind.traced || ComputerMindTraceOn();
}

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

	[[nodiscard]] bool ScriptControls(entt::entity creature) const override
	{
		return ecs::script_held::IsControlledByScript(creature);
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

	[[nodiscard]] std::optional<glm::vec3> TemplePlace(PlayerNames player) const override
	{
		std::optional<glm::vec3> place;
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		registry.Each<const Temple, const Transform>([&place, player](const Temple& building, const Transform& at) {
			if (!place.has_value() && building.owner == player)
			{
				place = at.position;
			}
		});
		// on the land there
		if (place.has_value() && Locator::terrainSystem::has_value())
		{
			place->y = Locator::terrainSystem::value().GetHeightAt({place->x, place->z});
		}
		return place;
	}

	void PlaceHand(PlayerNames player, const glm::vec3& point) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = magic::players::EntityOf(player);
		if (entity == entt::null)
		{
			return;
		}
		if (auto* mind = registry.TryGet<ComputerPlayer>(entity))
		{
			mind->handStart = point;
		}
		if (auto* hand = registry.TryGet<ComputerHand>(entity))
		{
			hand->point = point;
		}
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

	[[nodiscard]] bool Skirmish() const override { return ecs::IsSkirmishGame(); }

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
		// A readout for the log (the game channel at debug): each step of a computer god's hand as it begins
		if (const auto logger = spdlog::get("game");
		    logger != nullptr && stage == 0 && logger->should_log(spdlog::level::debug))
		{
			logger->debug("Computer hand: step {} begins, the hand at ({:.1f}, {:.1f}), turn {}, player {}, on {}",
			              static_cast<int>(step.type), hand.point.x, hand.point.z, game_clock::Turn(),
			              static_cast<int>(_player), DescribeThing(FirstThing(step)));
		}
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
		case HandSubAction::MoveHandNearPos:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, FirstPoint(step));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::MoveNearTurn(hand, _ground) : StageResult::Done;
		case HandSubAction::MoveHandNearObject:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, PointOfThing(FirstThing(step)));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::MoveNearThingTurn(hand, _ground) : StageResult::Done;
		case HandSubAction::GiveObjectInHandToCreature:
			return computer_hand::GiveToCreatureStage(hand, step, stage, *this);
		case HandSubAction::ThrowObjectInHandAtPos:
			return computer_hand::ThrowAtPosStage(hand, step, stage, *this);
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
		case HandSubAction::DropObjectInHandOnObject:
			return stage == 1 ? computer_hand::GiveHeldTurn(hand, FirstThing(step), *this) : StageResult::Done;
		case HandSubAction::CreateFoodFromContainer:
		case HandSubAction::CreateWoodFromContainer:
			// the container and the amount asked for; only the stage that goes on does anything
			if (stage == 1)
			{
				const auto type = step.type == HandSubAction::CreateWoodFromContainer ? ResourceType::Wood : ResourceType::Food;
				return computer_hand::TakeFromContainerTurn(hand, FirstThing(step), SecondInt(step), type, *this);
			}
			return StageResult::Done;
		case HandSubAction::Wait:
			if (stage == 0)
			{
				computer_hand::StartWait(hand, FirstFloat(step));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::WaitTurn(hand) : StageResult::Done;
		case HandSubAction::MakeGesture:
			return computer_hand::GestureStage(hand, step, stage, *this);
		case HandSubAction::MakeWorshippersChantForSpell:
			return computer_hand::ChantStage(hand, step, stage, *this);
		case HandSubAction::MakeWorshippersChantForSpellNoWait:
			return computer_hand::ChantNoWaitStage(hand, step, stage, *this);
		case HandSubAction::CastSpellAtObject:
			return computer_hand::CastAtObjectStage(hand, step, stage, *this, _ground);
		case HandSubAction::CastSpellAtPos:
		case HandSubAction::CastSpellTowardPos:
			// the cast towards a point has the cast at a point's own stages
			return computer_hand::CastAtPosStage(hand, step, stage, *this);
		case HandSubAction::MoveHandToPosStopWhenOutOfInfluence:
			if (stage == 0)
			{
				computer_hand::StartTowards(hand, FirstPoint(step));
				return StageResult::Done;
			}
			return stage == 1 ? computer_hand::MoveUntilOutOfInfluence(hand, _ground, *this) : StageResult::Done;
		case HandSubAction::SetLeash:
			return computer_hand::SetLeashStage(step, stage, *this);
		case HandSubAction::AttachLeashToObject:
			return computer_hand::AttachLeashStage(hand, step, stage, *this, _ground);
		case HandSubAction::ForceCreatureDominantDesire:
			return computer_hand::ForceDominantDesireStage(hand, step, stage, *this);
		case HandSubAction::ForceCreatureToPerformAction:
			return computer_hand::ForceActionStage(hand, step, stage, *this, _ground);
		case HandSubAction::SetCreatureSpeedForJourney:
			return computer_hand::JourneySpeedStage(step, stage, *this);
		case HandSubAction::RaiseTotem:
			return computer_hand::RaiseTotemStage(step, stage, *this);
		case HandSubAction::CastShieldAtPos:
			return computer_hand::CastShieldAtPosStage(hand, step, stage, *this);
		case HandSubAction::CloseDownCurrentSpell:
			// a step that does nothing: each of its three stages is over in its turn
			return StageResult::Done;
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

	void LetGo(ComputerHand& hand) override { Release(hand, glm::vec3(0.0f)); }

	void ThrowHeld(ComputerHand& hand, const glm::vec3& velocity) override { Release(hand, velocity); }

	[[nodiscard]] glm::vec3 InteractPoint(entt::entity thing) const override { return PointOfThing(thing); }

	[[nodiscard]] bool IsAngryMiracle(int32_t miracle) const override
	{
		const auto& effect = magic::GetMagicEffectInfo(Locator::infoConstants::value(), static_cast<MagicType>(miracle));
		return effect.perceivedPlayerDesire.at(0) == CreatureDesires::Anger;
	}

	[[nodiscard]] float InfluenceAt(const map_coords::MapCoords& place) const override
	{
		return influence::CalculatePlayerInfluence(_player, map_coords::ToWorld(place), influence::CalcType::Default, true);
	}

	/// The interface's release of what the hand holds, at the speed (none for a let-go)
	void Release(ComputerHand& hand, const glm::vec3& velocity)
	{
		const auto object = std::exchange(hand.heldObject, entt::null);
		if (object == entt::null || !_registry.Valid(object))
		{
			return;
		}
		// The thing leaves the hand where the hand is, at the speed (none for a gentle drop), as from the player's hand,
		// credited to the hand's own player
		if (auto* transform = _registry.TryGet<Transform>(object); transform != nullptr)
		{
			transform->position = hand.point;
		}
		ecs::physics::from_hand::Throw(object, velocity, false, computer_hand::LetGoCreditedTo(_player));
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
			// out of the physics, its flying-object reactions ended when it was in flight, as any hand's take
			ecs::physics::from_hand::LeavePhysicsForHand(thing);
		}
		ecs::villager_physics::InterfaceSetInMagicHand(thing, _player);
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

	void GiveHeldTo(ComputerHand& hand, entt::entity thing) override
	{
		// As the player's hand gives what it holds to the object under it: both must be there to act on, and the held
		// thing must be one that can be given to it; what is not taken stays in the hand
		const auto held = hand.heldObject;
		if (held == entt::null || !Interactable(thing) || !Interactable(held))
		{
			return;
		}
		if (ecs::held_apply::KindOfHeld(held) == ecs::held_apply::HeldKind::Other)
		{
			// (openblack) only the things a store takes are given by a computer player's hand yet
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Computer hand: giving this held thing to an object is not ported");
			return;
		}
		if (!ecs::held_apply::ValidToApplyThisToObject(held, thing))
		{
			return;
		}
		static_cast<void>(
		    ecs::held_apply::ApplyThisToObject(held, thing, ecs::pot_resource::DropperOf(_player), [&hand, held]() {
			    hand.heldObject = entt::null;
			    ecs::fire::SetOutMagicHand(held);
		    }));
	}

	[[nodiscard]] int32_t ResourceOf(entt::entity thing, ResourceType type) const override
	{
		return _registry.Valid(thing) ? static_cast<int32_t>(ecs::object_resources::GetResource(thing, type)) : 0;
	}

	[[nodiscard]] bool IsPotOf(entt::entity thing, ResourceType type) const override
	{
		return _registry.Valid(thing) && _registry.AllOf<Pot>(thing) && ecs::object_resources::GetResourceType(thing) == type;
	}

	void RemoveResource(entt::entity thing, ResourceType type, int32_t amount) override
	{
		static_cast<void>(ecs::object_resources::RemoveResource(thing, type, static_cast<uint32_t>(amount)));
	}

	void AddToPot(entt::entity pot, ResourceType type, int32_t amount) override
	{
		static_cast<void>(ecs::object_resources::AddResource(pot, type, static_cast<uint32_t>(amount)));
	}

	entt::entity MakeHandPot(entt::entity at, ResourceType type, int32_t amount) override
	{
		// the pot is made where the container is, and the hand takes it at once
		const auto kind = type == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
		return ecs::archetypes::PotArchetype::Create(PointOfThing(at), 0.0f, kind, amount);
	}

	[[nodiscard]] uint8_t GestureOf(int32_t miracle, int32_t which) const override
	{
		return Locator::infoConstants::has_value()
		           ? computer_hand::GestureOfMiracle(Locator::infoConstants::value(), miracle, which)
		           : uint8_t {0};
	}

	[[nodiscard]] const std::vector<glm::vec3>& GestureShape(uint8_t gesture) const override
	{
		return magic::gestures::ShapeOf(gesture).points;
	}

	[[nodiscard]] float GestureLengthRatio(uint8_t gesture) const override
	{
		return magic::gestures::ShapeOf(gesture).length / magic::gestures::ShapeOf(magic::gestures::k_Circle).length;
	}

	void LayGestureOnLand(uint8_t gesture, const glm::vec3& corner) override
	{
		const auto local =
		    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
		particles::utility::ShowGestureOnLand(static_cast<GestureType>(gesture), corner, computer_hand::k_GestureOnLandSize,
		                                      _player, _player == local);
	}

	[[nodiscard]] bool HasWorshipSite(entt::entity icon) const override
	{
		// a site's icon its site, a town or village centre's icon its town's site, any other icon none
		if (icon == entt::null || !_registry.Valid(icon) || !_registry.AllOf<SpellIcon>(icon))
		{
			return false;
		}
		if (const auto* site = _registry.TryGet<const WorshipSpellIcon>(icon); site != nullptr)
		{
			return site->site != entt::null;
		}
		if (const auto* townIcon = _registry.TryGet<const TownCentreSpellIcon>(icon); townIcon != nullptr)
		{
			const auto* townMagic = townIcon->town != entt::null && _registry.Valid(townIcon->town)
			                            ? _registry.TryGet<const TownMagic>(townIcon->town)
			                            : nullptr;
			return townMagic != nullptr && townMagic->worshipSite != entt::null;
		}
		return false;
	}

	[[nodiscard]] bool IsWorshipIcon(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<WorshipSpellIcon, SpellIcon>(thing);
	}

	[[nodiscard]] bool IsIconCharging(entt::entity icon) const override
	{
		return worship::icon::IsCharging(icon, PlayerNames::NEUTRAL, true);
	}

	[[nodiscard]] bool ValidForStartCharge(entt::entity icon, int32_t powerUp) const override
	{
		return worship::icon::ValidForStartCharge(icon, _player, powerUp, true);
	}

	void StartCharge(entt::entity icon, int32_t powerUp) override
	{
		static_cast<void>(worship::icon::StartCharge(icon, _player, powerUp, true));
	}

	[[nodiscard]] float ChantNeeded(entt::entity icon) const override { return worship::icon::GetChantNeeded(icon); }

	[[nodiscard]] int32_t PowerUpOf(int32_t miracle) const override
	{
		if (miracle < 0 || miracle >= static_cast<int32_t>(ecs::components::PlayerMagic::k_MagicTypes) ||
		    !Locator::infoConstants::has_value())
		{
			return -1;
		}
		// the level the game gives the miracle at load: a power-up's level in its first seed, -1 for a base miracle
		return magic::GetPowerUpLevelOfMagicInfo(Locator::infoConstants::value(), static_cast<MagicType>(miracle));
	}

	[[nodiscard]] float SecondsWithTheThingAfterACast(int32_t miracle) const override
	{
		return ValidMiracle(miracle)
		           ? magic::GetMagicEffectInfo(Locator::infoConstants::value(), static_cast<MagicType>(miracle))
		                 .computerCastDuration
		           : 0.0f;
	}

	[[nodiscard]] std::optional<computer_hand::FollowedPlace> PlaceToFollow(entt::entity thing) const override
	{
		// the thing's own place on the map, and its height
		const auto* transform =
		    thing != entt::null && _registry.Valid(thing) ? _registry.TryGet<const Transform>(thing) : nullptr;
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		return computer_hand::FollowedPlace {.place = map_coords::FromWorld(transform->position),
		                                     .height = ecs::object::GetHeight(thing)};
	}

	entt::entity CastOnObject(entt::entity thing, int32_t miracle, const glm::vec3& from, entt::entity icon) override
	{
		if (!ValidMiracle(miracle) || thing == entt::null || !_registry.Valid(thing) || !ecs::script_type::IsObject(thing))
		{
			return entt::null;
		}
		// a worship site's icon casts only once charged
		const bool worshipIcon =
		    icon != entt::null && _registry.Valid(icon) && _registry.AllOf<WorshipSpellIcon, SpellIcon>(icon);
		if (worshipIcon && worship::icon::GetChantNeeded(icon) > 0.0f)
		{
			return entt::null;
		}
		const auto type = static_cast<MagicType>(miracle);
		if (!magic::cast_rules::CanCastOn(type, thing))
		{
			return entt::null;
		}
		auto castData = magic::ComputerPlayerCastData(Locator::infoConstants::value(), type, 1.0f);
		// the point it is cast from and the thing's place, both through the map's coordinates
		const auto hand = magic::ToWorld(magic::ToMap(from));
		const auto* transform = _registry.TryGet<const Transform>(thing);
		const auto at = magic::ToWorld(magic::ToMap(transform != nullptr ? transform->position : glm::vec3(0.0f)));
		const particles::ProcessInfo info {
		    .handPos = hand,
		    .cameraForward = at - hand,
		    .direction = glm::vec3(0.0f),
		    .power = 1.0f,
		    .curl = 0.0f,
		    .enabled = true,
		};
		ecs::components::SpellCreator creator {};
		if (icon != entt::null && _registry.Valid(icon))
		{
			creator = {.kind = worshipIcon ? ecs::components::SpellCreator::Kind::WorshipSpellIcon
			                               : ecs::components::SpellCreator::Kind::Thing,
			           .player = worshipIcon ? worship::icon::PlayerOf(icon) : PlayerNames::NEUTRAL,
			           .entity = icon};
		}
		else
		{
			creator = magic::creator::NeutralPlayer();
		}
		entt::entity spell = entt::null;
		magic::CastAtObject(type, creator, thing, &spell, &castData, info);
		if (spell != entt::null && worshipIcon)
		{
			static_cast<void>(worship::icon::RemoveFromChantStore(icon, worship::icon::GetChantRequired(icon)));
		}
		return spell;
	}

	entt::entity CastAtPlace(int32_t miracle, const map_coords::MapCoords& to, const map_coords::MapCoords& from,
	                         entt::entity icon, float radius) override
	{
		if (!ValidMiracle(miracle))
		{
			return entt::null;
		}
		// a worship site's icon casts only once charged
		const bool worshipIcon =
		    icon != entt::null && _registry.Valid(icon) && _registry.AllOf<WorshipSpellIcon, SpellIcon>(icon);
		if (worshipIcon && worship::icon::GetChantNeeded(icon) > 0.0f)
		{
			return entt::null;
		}
		const auto type = static_cast<MagicType>(miracle);
		// the place on the land, as a miracle's place is kept: x and z on the map's grid, the height above the land
		const glm::vec3 mapPosition {map_coords::ToMetres(to.x), to.altitude, map_coords::ToMetres(to.z)};
		if (!magic::cast_rules::CanCastAt(type, mapPosition))
		{
			return entt::null;
		}
		auto castData = magic::ComputerPlayerCastData(Locator::infoConstants::value(), type, radius);
		const auto hand = map_coords::ToWorld(from);
		const auto at = map_coords::ToWorld(to);
		const particles::ProcessInfo info {
		    .handPos = hand,
		    .cameraForward = at - hand,
		    .direction = glm::vec3(0.0f),
		    .power = 1.0f,
		    .curl = 0.0f,
		    .enabled = true,
		};
		ecs::components::SpellCreator creator {};
		if (icon != entt::null && _registry.Valid(icon))
		{
			creator = {.kind = worshipIcon ? ecs::components::SpellCreator::Kind::WorshipSpellIcon
			                               : ecs::components::SpellCreator::Kind::Thing,
			           .player = worshipIcon ? worship::icon::PlayerOf(icon) : PlayerNames::NEUTRAL,
			           .entity = icon};
		}
		else
		{
			creator = magic::creator::NeutralPlayer();
		}
		entt::entity spell = entt::null;
		magic::CastAtPos(type, creator, mapPosition, &spell, &castData, info);
		if (spell != entt::null && worshipIcon)
		{
			static_cast<void>(worship::icon::RemoveFromChantStore(icon, worship::icon::GetChantRequired(icon)));
		}
		return spell;
	}

	void MakeDisciple(entt::entity villager, int32_t disciple) override
	{
		if (villager == entt::null || !_registry.Valid(villager))
		{
			return;
		}
		// the kind of disciple as the villagers' rule takes it, which does nothing outside its table
		static_cast<void>(ecs::villager::SetVillagerDisciple(villager, entt::null, static_cast<VillagerDisciple>(disciple), 1));
	}

	[[nodiscard]] bool IsCreatureWithPlayer(entt::entity thing) const override
	{
		if (thing == entt::null || !_registry.Valid(thing))
		{
			return false;
		}
		const auto* creature = _registry.TryGet<const Creature>(thing);
		return creature != nullptr && creature->owner != PlayerNames::NEUTRAL;
	}

	[[nodiscard]] bool IsLeashableObject(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && ecs::script_type::IsObject(thing);
	}

	void SetLeashType(entt::entity creature, int32_t type) override
	{
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().SetType(creature, static_cast<LeashType>(type));
		}
	}

	[[nodiscard]] bool IsLeashTied(entt::entity creature) const override
	{
		return Locator::leashSystem::has_value() && Locator::leashSystem::value().TiedTo(creature).has_value();
	}

	void TakeLeashOff(entt::entity creature) override
	{
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().TakeOff(creature);
		}
	}

	void TieLeash(entt::entity creature, entt::entity thing) override
	{
		if (Locator::leashSystem::has_value())
		{
			static_cast<void>(Locator::leashSystem::value().TieTo(creature, thing));
		}
	}

	void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) override
	{
		if (!Locator::creatureMindSystem::has_value() || creature == entt::null || !_registry.Valid(creature) || desire < 0 ||
		    static_cast<size_t>(desire) >= creature_desires::k_DesireCount)
		{
			return;
		}
		auto& minds = Locator::creatureMindSystem::value();
		minds.AbandonAction(creature);
		minds.MakeDesireDominant(creature, static_cast<creature_desires::Desire>(desire), seconds, false);
	}

	void OrderCreature(entt::entity creature, uint32_t action, entt::entity thing) override
	{
		if (Locator::creatureMindSystem::has_value() && creature != entt::null && _registry.Valid(creature) &&
		    _registry.TryGet<const Creature>(creature) != nullptr)
		{
			static_cast<void>(Locator::creatureMindSystem::value().ObeyGodOrder(creature, action, thing));
		}
	}

	void TeachCreature(entt::entity creature, uint32_t kind, uint32_t action) override
	{
		if (Locator::creatureMindSystem::has_value() && creature != entt::null && _registry.Valid(creature))
		{
			Locator::creatureMindSystem::value().LearnAction(creature, kind, action);
		}
	}

	[[nodiscard]] std::optional<computer_hand::JourneyBody> JourneyBodyOf(entt::entity thing) const override
	{
		const auto* body = thing != entt::null && _registry.Valid(thing) ? _registry.TryGet<const Creature>(thing) : nullptr;
		const auto* transform = body != nullptr ? _registry.TryGet<const Transform>(thing) : nullptr;
		if (transform == nullptr || !Locator::infoConstants::has_value())
		{
			return std::nullopt;
		}
		const auto& infos = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(body->species);
		if (row >= infos.size())
		{
			return std::nullopt;
		}
		return computer_hand::JourneyBody {.point = map_coords::ToWorld(map_coords::FromWorld(transform->position)),
		                                   .walk = infos.at(row).walkSpeed,
		                                   .run = infos.at(row).runSpeed};
	}

	void DrawThrownAway(float most) override { static_cast<void>(game_random::GameFloatRand(most)); }

	void SetJourneySpeed(entt::entity creature, float fraction) override
	{
		if (Locator::creatureLocomotionSystem::has_value() && creature != entt::null && _registry.Valid(creature))
		{
			Locator::creatureLocomotionSystem::value().SetKeptFraction(creature, fraction);
		}
	}

	void RaiseTotem(entt::entity totem, float share) override
	{
		const auto* statue =
		    totem != entt::null && _registry.Valid(totem) ? _registry.TryGet<const VillageTotem>(totem) : nullptr;
		if (statue == nullptr || !Locator::villageTotemSystem::has_value())
		{
			return;
		}
		auto& totems = Locator::villageTotemSystem::value();
		// the hand's player takes hold of it, as a player's hand does
		totems.FriendlyStartLockedSelect(totem, _player);
		const auto town = statue->townCentre != entt::null && _registry.Valid(statue->townCentre)
		                      ? ecs::abode_villagers::TownOf(statue->townCentre)
		                      : entt::entity {entt::null};
		if (town == entt::null)
		{
			// a totem of no town takes the share itself
			static_cast<void>(totems.SetShareFromScript(totem, share));
			return;
		}
		const auto* townData = _registry.TryGet<const Town>(town);
		const auto* transform = _registry.TryGet<const Transform>(totem);
		if (townData != nullptr && transform != nullptr && Locator::creatureMindSystem::has_value())
		{
			// the town's player's creature sees its player wants to impress it there
			constexpr float k_ImpressWeight = 0.5f;
			Locator::creatureMindSystem::value().EmpathiseWithPlayer(townData->owner, CreatureDesires::ToImpress,
			                                                         k_ImpressWeight, transform->position);
		}
		totems.SetTownShare(town, share);
	}

	[[nodiscard]] bool IsCommanded(entt::entity creature) const override
	{
		return Locator::creatureMindSystem::has_value() && creature != entt::null && _registry.Valid(creature) &&
		       Locator::creatureMindSystem::value().IsCommanded(creature);
	}

	void KeepCastAt(entt::entity spell, const map_coords::MapCoords& place) override
	{
		// at the head of the mind's list, with the place
		const auto entity = magic::players::EntityOf(_player);
		auto* mind = entity != entt::null ? _registry.TryGet<ComputerMind>(entity) : nullptr;
		if (mind != nullptr)
		{
			mind->spellsCast.insert(mind->spellsCast.begin(), ComputerMindCast {.spell = spell, .place = place});
		}
	}

	void KeepCast(entt::entity spell, entt::entity thing) override
	{
		// at the head of the mind's list, with the thing's place
		const auto* transform = _registry.Valid(thing) ? _registry.TryGet<const Transform>(thing) : nullptr;
		const auto place = transform != nullptr ? map_coords::FromWorld(transform->position) : map_coords::MapCoords {};
		const auto entity = magic::players::EntityOf(_player);
		auto* mind = entity != entt::null ? _registry.TryGet<ComputerMind>(entity) : nullptr;
		if (mind != nullptr)
		{
			mind->spellsCast.insert(mind->spellsCast.begin(), ComputerMindCast {.spell = spell, .place = place});
		}
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
	/// What a step acts on, for the readout: no thing, a creature and its player, or another thing
	[[nodiscard]] std::string DescribeThing(entt::entity thing) const
	{
		if (thing == entt::null || !_registry.Valid(thing))
		{
			return "no thing";
		}
		if (const auto* creature = _registry.TryGet<ecs::components::Creature>(thing); creature != nullptr)
		{
			return fmt::format("thing {} (the creature of player {})", static_cast<uint32_t>(thing),
			                   static_cast<int>(creature->owner));
		}
		return fmt::format("thing {} (not a creature)", static_cast<uint32_t>(thing));
	}
	[[nodiscard]] static entt::entity FirstThing(const HandStep& step)
	{
		return step.arguments.empty() ? entt::null : step.arguments.front().thing;
	}
	[[nodiscard]] static float FirstFloat(const HandStep& step)
	{
		return step.arguments.empty() ? 0.0f : step.arguments.front().floatValue;
	}
	[[nodiscard]] static bool ValidMiracle(int32_t miracle)
	{
		return miracle >= 0 && miracle < static_cast<int32_t>(ecs::components::PlayerMagic::k_MagicTypes) &&
		       Locator::infoConstants::has_value();
	}
	[[nodiscard]] static int32_t SecondInt(const HandStep& step)
	{
		return step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
	}
	/// The thing can be acted on by a hand: it is there, and an abode is partly built
	[[nodiscard]] bool Interactable(entt::entity thing) const
	{
		const bool available = thing != entt::null && _registry.Valid(thing) && ecs::IsAvailable(thing);
		const bool abode = available && _registry.AllOf<Abode>(thing);
		return ecs::hand_action_point::Interactable(available, abode, abode ? ecs::abodes::GetPercentBuilt(thing) : 1.0f);
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

	[[nodiscard]] bool IsVillager(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<Villager>(thing);
	}

	[[nodiscard]] VillagerHome HomeOf(entt::entity villager) const override
	{
		const auto* data =
		    villager != entt::null && _registry.Valid(villager) ? _registry.TryGet<const Villager>(villager) : nullptr;
		if (data == nullptr)
		{
			return {};
		}
		const auto valid = [this](entt::entity e) {
			return e != entt::null && _registry.Valid(e) ? e : entt::entity {entt::null};
		};
		return {.town = valid(data->town), .abode = valid(data->abode)};
	}

	[[nodiscard]] map_coords::MapCoords ArrivePlace(entt::entity thing) const override
	{
		return ecs::living_footpath::ArrivePosOf(thing);
	}

	[[nodiscard]] bool IsAvailableBuildingSite(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<BuildingSite>(thing) && ecs::IsAvailable(thing);
	}

	[[nodiscard]] bool IsScaffold(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && ecs::scaffolds::IsScaffold(thing);
	}

	[[nodiscard]] bool IsCreature(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<Creature>(thing);
	}

	[[nodiscard]] bool IsWorshipSite(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<WorshipSite>(thing);
	}

	[[nodiscard]] std::optional<glm::vec3> GatePoint(entt::entity site) const override
	{
		// the gate is kept as a place on the map, and given back as a point from it
		const auto gate = IsWorshipSite(site) ? worship::site::GetSpecialPos(site, worship::site::Point::Arrive) : std::nullopt;
		return gate.has_value() ? std::optional<glm::vec3>(map_coords::ToWorld(map_coords::FromWorld(*gate))) : std::nullopt;
	}

	[[nodiscard]] glm::vec3 MapPoint(entt::entity thing) const override
	{
		// the thing's place is kept on the map grid, and given back as a point from it
		const auto* transform =
		    thing != entt::null && _registry.Valid(thing) ? _registry.TryGet<const Transform>(thing) : nullptr;
		return transform != nullptr ? map_coords::ToWorld(map_coords::FromWorld(transform->position)) : glm::vec3(0.0f);
	}

	[[nodiscard]] entt::entity SiteBuilding(entt::entity thing) const override
	{
		const bool site = thing != entt::null && _registry.Valid(thing) && _registry.AllOf<BuildingSite>(thing);
		return site ? ecs::building_sites::GetBuilding(thing) : entt::entity {entt::null};
	}

	[[nodiscard]] uint32_t ResourceOf(entt::entity thing, ResourceType type) const override
	{
		return thing != entt::null && _registry.Valid(thing) ? ecs::object_resources::GetResource(thing, type) : 0;
	}

	[[nodiscard]] bool IsTown(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) && _registry.AllOf<Town>(thing);
	}

	[[nodiscard]] ResourceType ResourceTypeOf(entt::entity thing) const override
	{
		return thing != entt::null && _registry.Valid(thing) ? ecs::object_resources::GetResourceType(thing)
		                                                     : ResourceType::None;
	}

	[[nodiscard]] entt::entity ResourceDropObject(entt::entity town, ResourceType type) const override
	{
		// the town's store, else its temporary pot, made from the town's place when it has none
		if (const auto pit = ecs::town_queries::GetStoragePit(town); pit != entt::null)
		{
			return pit;
		}
		return ecs::town_stores::GetTemporaryResourceStorePotOrPos(town, ecs::object::MapCoordsOf(town), type).pot;
	}

	[[nodiscard]] std::optional<MagicType> IconMagicType(entt::entity thing) const override
	{
		// a worship site's icon gives the miracle of its seed at its base level
		if (thing == entt::null || !_registry.Valid(thing) || !_registry.AllOf<WorshipSpellIcon, SpellIcon>(thing))
		{
			return std::nullopt;
		}
		const auto seed = _registry.Get<const SpellIcon>(thing).seedType;
		return magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed), -1);
	}

	[[nodiscard]] map_coords::MapCoords MapPlace(entt::entity thing) const override
	{
		const auto* transform =
		    thing != entt::null && _registry.Valid(thing) ? _registry.TryGet<const Transform>(thing) : nullptr;
		return transform != nullptr ? map_coords::FromWorld(transform->position) : map_coords::MapCoords {};
	}

	[[nodiscard]] map_coords::MapCoords NearestEdgeTo(entt::entity thing, const map_coords::MapCoords& place) const override
	{
		return ecs::object::GetNearestEdgeToPos(thing, place);
	}

	[[nodiscard]] glm::vec3 LandPoint(const map_coords::MapCoords& place) const override { return map_coords::ToWorld(place); }

	[[nodiscard]] std::vector<Worshipper> SiteWorshippers(entt::entity site) const override
	{
		std::vector<Worshipper> worshippers;
		const auto* data = IsWorshipSite(site) ? _registry.TryGet<const WorshipSite>(site) : nullptr;
		if (data == nullptr)
		{
			return worshippers;
		}
		for (const auto villager : data->villagers)
		{
			if (villager == entt::null || !_registry.Valid(villager) || !_registry.AllOf<Villager>(villager))
			{
				continue;
			}
			worshippers.push_back({.villager = villager,
			                       .hiding = ecs::villager::GetFinalState(villager) == VillagerStates::HidingAtWorshipSite,
			                       .life = ecs::life::LifeOf(villager)});
		}
		return worshippers;
	}

	[[nodiscard]] std::optional<map_coords::MapCoords> WorshipFoodPlace(entt::entity site) const override
	{
		if (site == entt::null || !_registry.Valid(site))
		{
			return std::nullopt;
		}
		// the point through the map's coordinates, its height above the land kept
		const auto point = worship::site::GetFoodPos(site);
		return point.has_value() ? std::optional(map_coords::FromWorld(*point)) : std::nullopt;
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

/// The hand as the mind's turn drives it: its steps, a script's hold, and its queue, forced through the player's mind
class GameTurnHand final: public computer_player::TurnHand
{
public:
	using ForceQueued = std::function<bool(const QueuedHandAction&)>;

	GameTurnHand(ecs::Registry& registry, ComputerHand& hand, GameHandRunner& runner, ForceQueued force)
	    : _registry(registry)
	    , _hand(hand)
	    , _runner(runner)
	    , _force(std::move(force))
	{
	}

	[[nodiscard]] bool Busy() const override { return computer_hand::TakesItsTurn(_hand); }
	[[nodiscard]] bool HeldByScript() const override { return _hand.held; }
	void Step() override
	{
		_hand.lastStepTurn = game_clock::Turn();
		computer_hand::Step(_hand, _runner);
		CarryHeld(_registry, _hand);
	}
	bool ForceNext() override
	{
		auto next = computer_hand::TakeNext(_hand);
		if (!next.has_value())
		{
			return false;
		}
		const bool done = _force(*next);
		_forced = std::make_pair(next->name, done);
		return done;
	}

	/// The queued action this turn forced, and whether it gave the hand something to do
	[[nodiscard]] const std::optional<std::pair<std::string, bool>>& Forced() const { return _forced; }

private:
	ecs::Registry& _registry;
	ComputerHand& _hand;
	GameHandRunner& _runner;
	ForceQueued _force;
	std::optional<std::pair<std::string, bool>> _forced;
};

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
	// The minds in the order of the players, as each takes its turn in its player's turn
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::NEUTRAL); ++p)
	{
		PlayerTurn(static_cast<PlayerNames>(p));
	}
}

void ComputerPlayerSystem::PlayerTurn(PlayerNames name)
{
	// The neutral player has no computer mind of its own
	if (name == PlayerNames::NEUTRAL)
	{
		return;
	}
	const auto wideScreenOn =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> minds;
	std::as_const(registry).Each<const Player, const ComputerPlayer>(
	    [&minds, name](entt::entity entity, const Player& player, const ComputerPlayer& /*mind*/) {
		    if (player.name == name)
		    {
			    minds.push_back(entity);
		    }
	    });
	if (minds.empty())
	{
		return;
	}
	// A start can make a creature, so the mind takes its turn after the player was found
	GameHandRunner runner(registry);
	const auto seen = computer_world::MakeSeenWorld(registry);
	const auto& seenWorld = *seen;
	for (const auto entity : minds)
	{
		runner.SetPlayer(name);
		// Whatever the mind does, the miracles it cast and what it saw go when they are gone, and so do its queued actions
		// and the hand's steps when a thing they act on is gone
		if (auto* known = registry.TryGet<ComputerMind>(entity); known != nullptr)
		{
			std::erase_if(known->spellsCast, [](const ComputerMindCast& cast) { return !ecs::IsAvailable(cast.spell); });
			computer_player::ForgetSeenGone(*known, seenWorld);
		}
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
		auto* moving = registry.TryGet<ComputerHand>(entity);
		if (moving == nullptr)
		{
			continue;
		}
		auto& computerMind = ComputerMindOf(name);
		const auto turn = game_clock::Turn();
		// The mind looks round its hand, then takes its turn: the hand's steps, or the queue, or its own choice
		computer_player::ScanAroundHand(computerMind, seenWorld, map_coords::MapCoords {.x = moving->mapX, .z = moving->mapZ},
		                                turn);
		GameTurnHand turnHand(registry, *moving, runner, [this, name](const QueuedHandAction& next) {
			auto* hand = HandOf(name);
			return hand != nullptr && Force(name, *hand, next.name, next.first, next.second);
		});
		if (computerMind.heldByDebug)
		{
			// the debug window holds the mind: its hand goes on with its steps and its queue, and it weighs nothing
			if (turnHand.Busy())
			{
				turnHand.Step();
			}
			else
			{
				static_cast<void>(turnHand.ForceNext());
			}
			continue;
		}
		const auto scores = computer_world::MakeScoreWorld(registry, name);
		const GameThingFacts facts(registry);
		computer_hand::HandActions actions(
		    *moving, runner, facts, scores.get(),
		    [name, this](computer_player::ActionKind /*kind*/, uint16_t node) { NoteMissingAction(name, node); },
		    [name, this, moving](computer_player::ActionKind /*kind*/, uint16_t node, bool done) {
			    TraceAction(name, node, done, false, *moving);
		    });
		const auto done = computer_player::TakeTurn(computer_player::GameTree(), computerMind, turnHand, turn, actions);
		if (IsTraced(computerMind))
		{
			TraceTurn(name, computerMind, *moving, done, turnHand.Forced(), turn);
		}
	}
}

void ComputerPlayerSystem::NoteMissingAction(PlayerNames player, uint16_t node) const
{
	// (openblack) while the mind's actions are being ported, a chosen one openblack lacks is only told, once each time:
	// as information while the mind is traced, so that a run's log lists them, else for debugging
	auto log = spdlog::get("game");
	if (log == nullptr)
	{
		return;
	}
	const auto name = _rowNames.Name(computer_player::MindNodes().at(node).row);
	if (const auto* mind = ReadComputerMindOf(player); mind != nullptr && IsTraced(*mind))
	{
		SPDLOG_LOGGER_INFO(log, "Computer player {} chose {}, whose action openblack lacks", static_cast<int>(player), name);
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(log, "Computer player {} chose {}, whose action openblack lacks", static_cast<int>(player), name);
	}
}

void ComputerPlayerSystem::TraceAction(PlayerNames player, uint16_t node, bool done, bool forced,
                                       const ComputerHand& hand) const
{
	const auto* mind = ReadComputerMindOf(player);
	auto log = spdlog::get("game");
	if (log == nullptr || mind == nullptr || !IsTraced(*mind))
	{
		return;
	}
	// the action by its row's name, and the hand's steps by their numbers in order
	std::string steps;
	for (const auto& step : hand.steps)
	{
		steps += fmt::format("{}{}", steps.empty() ? "" : " ", static_cast<int>(step.type));
	}
	SPDLOG_LOGGER_INFO(log, "Computer player {} ran {} (node {}, {}): {}; hand: {} steps [{}]", static_cast<int>(player),
	                   _rowNames.Name(computer_player::MindNodes().at(node).row), node, forced ? "forced" : "chosen",
	                   done ? "done" : "failed", hand.steps.size(), steps);
}

void ComputerPlayerSystem::TraceTurn(PlayerNames player, const ComputerMind& mind, const ComputerHand& hand,
                                     computer_player::TurnDone done, const std::optional<std::pair<std::string, bool>>& forced,
                                     uint32_t turn) const
{
	auto log = spdlog::get("game");
	if (log == nullptr)
	{
		return;
	}
	const auto forcedText = forced.has_value()
	                            ? fmt::format("forced {} ({})", forced->first, forced->second ? "done" : "not done")
	                            : "nothing forced";
	const auto chosen = mind.nodes.empty() ? std::nullopt : mind.nodes.front().chosen;
	const auto chosenText =
	    chosen.has_value() ? std::string(_rowNames.Name(computer_player::MindNodes().at(*chosen).row)) : std::string("none");
	// the chosen node by its place in the tree and the row of its parent, as a row is met under several parents
	std::string nodeText = "no node";
	if (chosen.has_value())
	{
		const auto parent = computer_player::MindNodes().at(*chosen).parent;
		nodeText = fmt::format("node {} under {}", *chosen,
		                       parent.has_value() ? _rowNames.Name(computer_player::MindNodes().at(*parent).row)
		                                          : std::string_view("nothing"));
	}
	constexpr std::array<const char*, 4> k_Done {"steps", "forced", "rested", "weighed"};
	// the hand as the turn left it: its first step by the step's number and the stage it is at, how many steps it has,
	// what it holds, and whether a script holds it
	const auto stepText = hand.steps.empty() ? std::string("no step")
	                                         : fmt::format("step {} stage {} of {}", static_cast<int>(hand.steps.front().type),
	                                                       static_cast<int>(hand.stage), hand.steps.size());
	SPDLOG_LOGGER_INFO(
	    log, "Computer mind {} turn {}: {}; {}; root worth {:.4f}, chosen {}, plan worth {:.4f}; hand: {}, holds {}{}; {}",
	    static_cast<int>(player), turn, k_Done.at(static_cast<size_t>(done)), forcedText,
	    mind.nodes.empty() ? 0.0f : mind.nodes.front().score, chosenText, mind.planScore, stepText,
	    hand.heldObject == entt::null ? std::string("nothing") : std::to_string(static_cast<uint32_t>(hand.heldObject)),
	    hand.held ? ", held by a script" : "", nodeText);
}

ComputerHand* ComputerPlayerSystem::HandOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = magic::players::EntityOf(player);
	if (entity == entt::null)
	{
		return nullptr;
	}
	if (auto* hand = registry.TryGet<ComputerHand>(entity); hand != nullptr)
	{
		return hand;
	}
	// a hand made after its player started is where the start put it
	auto& made = registry.AssignState<ComputerHand>(entity);
	if (const auto* mind = registry.TryGet<const ComputerPlayer>(entity); mind != nullptr && mind->handStart.has_value())
	{
		made.point = *mind->handStart;
	}
	return &made;
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
	// its weighings score as the mind's own do
	const auto scores = computer_world::MakeScoreWorld(registry, player);
	computer_hand::HandActions actions(
	    hand, runner, facts, scores.get(),
	    [this, player](computer_player::ActionKind /*kind*/, uint16_t node) { NoteMissingAction(player, node); },
	    [this, player, &hand](computer_player::ActionKind /*kind*/, uint16_t node, bool done) {
		    TraceAction(player, node, done, true, hand);
	    });
	return computer_hand::ForceAction(hand, mind, name, valid(first) ? first : entt::null, valid(second) ? second : entt::null,
	                                  game_clock::Turn(), facts, actions, _rowNames);
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

void ComputerPlayerSystem::SetTraced(PlayerNames player, bool on)
{
	ComputerMindOf(player).traced = on;
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

const ComputerMind* ComputerPlayerSystem::SavedMind(PlayerNames player) const
{
	return ReadComputerMindOf(player);
}

void ComputerPlayerSystem::Restore(PlayerNames player, const ComputerPlayer& state, const ComputerMind& mind)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		_keptMinds.at(static_cast<size_t>(player)) = mind;
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = magic::players::EntityOf(player);
	if (auto* own = entity != entt::null ? registry.TryGet<ComputerMind>(entity) : nullptr; own != nullptr)
	{
		*own = mind;
	}
	else
	{
		_keptMinds.at(static_cast<size_t>(player)) = mind;
	}
	// A player whose switches are a new one's needs no computer player of its own: one is made when first set
	const bool asNew = !state.switchedOn && !state.started && !state.paused && !state.creatureLike.has_value();
	if (!asNew)
	{
		MindOf(player) = state;
	}
	else if (auto* own = entity != entt::null ? registry.TryGet<ComputerPlayer>(entity) : nullptr; own != nullptr)
	{
		*own = state;
	}
}
