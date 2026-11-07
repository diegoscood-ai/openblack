/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownBeliefSymbols.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/PlayerSymbol.h"
#include "Particles/TownBeliefMaths.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// The effect each town centre has (TOWN_BELIEF)
constexpr const char* k_TownBeliefFile = "SF_TownBelief";
/// A symbol's scale as it is made, before the rule gives it its own
constexpr float k_SymbolBaseScale = 1.5f;
/// The collection flag the rule sets on a fight sparkle collection while its symbol fights hard enough
/// (CollectionShouldBeEmitting)
constexpr uint8_t k_EmittingFlag = 0x04;

using Centre = ecs::components::TownBeliefSymbols;

bool IsTownCentre(const ecs::components::Abode& abode)
{
	// the town centre types, from the info constants on first use (kept in the psys state)
	auto& centres = Locator::particleSystem::value().GetState().townCentreTypes;
	if (!centres.has_value())
	{
		centres.emplace();
		for (const auto& info : Locator::infoConstants::value().abode)
		{
			if (info.abodeType == AbodeType::TownCentre)
			{
				centres->insert(info.abodeNumber);
			}
		}
	}
	return centres->contains(abode.type);
}

/// The centre's town's beliefs, none without a town
std::optional<std::array<float, town_belief::k_PlayerCount>> BeliefsOf(entt::entity centre)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.Valid(centre) ? registry.TryGet<const Abode>(centre) : nullptr;
	if (abode == nullptr)
	{
		return std::nullopt;
	}
	const auto town = ecs::town_queries::TownByKey(abode->townId);
	if (town == entt::null || !registry.Valid(town))
	{
		return std::nullopt;
	}
	return registry.Get<const Town>(town).belief.belief;
}

/// TownCentreBelief's data on its collection: whether it still has to take its centre, and the centre
struct CentreData
{
	bool take {true};
	entt::entity centre {entt::null};
};

/// Its data on each symbol: the player and its motion (TownBeliefMaths.h)
struct SymbolData
{
	int player {0};
	town_belief::Symbol motion;
};

/// TownCentreBelief (the rule of each town centre's belief effect): the first step that has a target takes the
/// centre and makes one symbol per active player (a new atom with the rule's NextGroups, base scale 1.5, a player's
/// symbol; its tilt and way round random in 0..2 pi, in that order). Every step each symbol moves over the centre's
/// totem by its belief and rank (town_belief::Step), its size by its belief, and its fight sparkle collections
/// (GroupFightSparkle) take the fight's alpha and emit while it sparks. Nothing moves while the centre has no town or no
/// totem
class TownCentreBelief final: public Modifier
{
public:
	explicit TownCentreBelief(const Object& object)
	    : groupFightSparkle(object.Int("GroupFightSparkle", -1))
	    , nextGroups(object.IntArray("NextGroups"))
	{
		const town_belief::Params d;
		params = {
		    .radiusAt0 = object.Float("RadiusAt0", d.radiusAt0),
		    .radiusAt1 = object.Float("RadiusAt1", d.radiusAt1),
		    .speedAt0 = object.Float("SpeedAt0", d.speedAt0),
		    .speedAt1 = object.Float("SpeedAt1", d.speedAt1),
		    .scaleAt0 = object.Float("ScaleAt0", d.scaleAt0),
		    .scaleAt1 = object.Float("ScaleAt1", d.scaleAt1),
		    .heightAt1 = object.Float("HeightAt1", d.heightAt1),
		    .heightPerLevel = object.Float("HeightPerLevel", d.heightPerLevel),
		    .useFixedScale = object.Bool("UseFixedScale", d.useFixedScale),
		    .fixedScale = object.Float("FixedScale", d.fixedScale),
		    .timeBetweenFightsAt0 = object.Float("TimeBetweenFightsAt0", d.timeBetweenFightsAt0),
		    .timeBetweenFightsAt1 = object.Float("TimeBetweenFightsAt1", d.timeBetweenFightsAt1),
		    .fightDurationAt0 = object.Float("FightDurationAt0", d.fightDurationAt0),
		    .fightDurationAt1 = object.Float("FightDurationAt1", d.fightDurationAt1),
		    .speedUpDuringFight = object.Float("SpeedUpDuringFight", d.speedUpDuringFight),
		    .fightFracWhenEmitting = object.Float("FightFracWhenEmitting", d.fightFracWhenEmitting),
		    .angleSpeedPhaseSpeed = object.Float("AngleSpeedPhaseSpeed", d.angleSpeedPhaseSpeed),
		};
	}
	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<CentreData>(collection, this);
		if (data.take && !effect.GetTargets().empty())
		{
			data.take = false;
			data.centre = effect.TakeTarget();
			const auto typeOf = [](int slot) { return magic::players::ReadMagicOf(static_cast<PlayerNames>(slot)).playerType; };
			for (const int player : town_belief::SymbolPlayers(typeOf))
			{
				auto& atom = effect.NewAtom(collection, nullptr, nextGroups);
				atom.baseScale = k_SymbolBaseScale;
				auto& symbol = AtomDataOf<SymbolData>(atom, this);
				symbol.player = player;
				atom.playerSymbol =
				    std::make_shared<player_symbol::PlayerSymbol>(player_symbol::PlayerSymbol {.player = player});
				symbol.motion.tilt = effect.Random(glm::two_pi<float>());
				symbol.motion.around = effect.Random(glm::two_pi<float>());
			}
		}
		if (data.centre == entt::null)
		{
			return true;
		}
		const auto beliefs = BeliefsOf(data.centre);
		const auto base = beliefs.has_value() ? town_belief::SymbolBase(data.centre, params.heightAt1) : std::nullopt;
		if (!base.has_value())
		{
			return true; // no town or no totem: no symbol moves
		}
		// the camera's distance from the point over the totem, as the step sees the camera
		const glm::vec3 eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : *base;
		const float s = town_belief::DistanceScale(params, glm::distance(eye, *base));
		const auto random = [&effect](float a, float b) { return effect.Random(a, b); };
		for (auto& atom : collection.atoms)
		{
			auto& symbol = AtomDataOf<SymbolData>(*atom, this);
			const auto player = static_cast<size_t>(symbol.player);
			const auto look = town_belief::Step(symbol.motion, params, beliefs->at(player),
			                                    town_belief::Rank(*beliefs, symbol.player), effect.GetDt(), s, random);
			atom->position = *base + look.offset;
			atom->ruleScale = look.scale;
			// the fight sparkles: their collection's alpha and emitting flag
			for (auto& sub : atom->subCollections)
			{
				if (sub->group != groupFightSparkle)
				{
					continue;
				}
				sub->alpha = static_cast<float>(town_belief::SparkAlpha(look.fight));
				const auto emitting = town_belief::Sparking(params, look.fight) ? k_EmittingFlag : uint8_t {0};
				sub->flags = static_cast<uint8_t>((sub->flags & ~k_EmittingFlag) | emitting);
			}
		}
		return true;
	}

	town_belief::Params params;
	int groupFightSparkle;
	std::vector<int> nextGroups;
};

/// CollectionShouldBeEmitting: the collection's emitting flag, which TownCentreBelief sets on its fight sparkles
bool CollectionShouldBeEmitting(const Effect& /*effect*/, const Object& /*object*/, const Atom* /*atom*/,
                                const Collection& collection)
{
	return (collection.flags & k_EmittingFlag) != 0;
}

/// The town centres (an Abode whose number is a TownCentre's, with a Mesh), in the order the original draws them: its
/// list of centres gets each new one at the head, so the newest first (here by the object creation index);
/// unavailable ones (components::Unavailable) left out. A centre with no town still steps its effect, whose symbols do
/// not move then
template <typename Fn>
void ForEachCentre(Fn&& fn)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<int64_t, entt::entity>> centres;
	registry.Each<const Abode, const Transform, const Mesh>(
	    [&centres](entt::entity entity, const Abode& abode, const Transform&, const Mesh&) {
		    // a centre leaves the list when it is deleted
		    if (IsTownCentre(abode) && ecs::IsAvailable(entity))
		    {
			    centres.emplace_back(ecs::object_index::Of(entity), entity);
		    }
	    },
	    entt::exclude<Unavailable>);
	std::stable_sort(centres.begin(), centres.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& [index, entity] : centres)
	{
		fn(entity);
	}
}

/// Each symbol atom of the centre's effect
template <typename Fn>
void ForEachSymbol(const Effect& effect, Fn&& fn)
{
	for (const auto& root : effect.GetRoots())
	{
		for (const auto& atom : root->atoms)
		{
			if (atom->playerSymbol != nullptr)
			{
				fn(*atom);
			}
		}
	}
}

/// An artefact's owner's symbol floats this far above its model (in its model's half heights), or this far above the
/// top of a thing without a model
constexpr float k_ArtefactSymbolHalfHeights = 1.2f;
constexpr float k_ArtefactSymbolOverTop = 2.0f;

/// Whether an artefact shows its owner's symbol: only among a town's artefacts, once worth enough
bool ShowsArtefactSymbol(const ecs::components::TownArtefact& artefact)
{
	return artefact.inTown && artefact.symbol && artefact.player.has_value() &&
	       ecs::artefact_rules::ShowsSymbol(artefact.value);
}

/// Each artefact's symbol's glows turn by the frame's game milliseconds, only while it is drawn
void StepArtefactSymbols(float milliseconds)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> shown;
	registry.Each<const TownArtefact>([&shown](entt::entity object, const TownArtefact& artefact) {
		if (ShowsArtefactSymbol(artefact))
		{
			shown.push_back(object);
		}
	});
	for (const auto object : shown)
	{
		auto* existing = registry.TryGet<ArtefactSymbol>(object);
		auto& symbol = existing != nullptr ? *existing : registry.AssignState<ArtefactSymbol>(object);
		symbol.spin = graphics::frame_anim::PlayerSymbolSpin(symbol.glowSpin, milliseconds);
		symbol.cellA = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowA, milliseconds, 0));
		symbol.cellB = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowB, milliseconds, 1));
	}
}

/// The owners' symbols over the artefacts that show them, at half size: over the middle of the model, by more than its
/// half height; over the top of a thing without one
void CollectArtefactSymbols(std::vector<manager::Drawable>& out)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<const TownArtefact, const ArtefactSymbol, const Transform>(
	    [&](entt::entity object, const TownArtefact& artefact, const ArtefactSymbol& symbol, const Transform& transform) {
		    if (!ShowsArtefactSymbol(artefact))
		    {
			    return;
		    }
		    glm::vec3 at = transform.position + glm::vec3(0.0f, ecs::object::GetHeight(object) + k_ArtefactSymbolOverTop, 0.0f);
		    if (const auto* mesh = registry.TryGet<const Mesh>(object); mesh != nullptr && meshes.Contains(mesh->id))
		    {
			    const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
			    at = transform.position + transform.rotation * (box.Center() * transform.scale);
			    at.y += box.Size().y * 0.5f * transform.scale.y * k_ArtefactSymbolHalfHeights;
		    }
		    const int player = static_cast<int>(*artefact.player);
		    const float size = 1.5f * ecs::artefact_rules::k_SymbolScale;
		    const uint32_t rgb = player_symbol::ColourOf(player);
		    const std::array<uint8_t, 3> colour = {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8),
		                                           static_cast<uint8_t>(rgb)};
		    const glm::mat3 still(1.0f);
		    const glm::mat3 spun = affine::AngleY(symbol.spin);
		    manager::Drawable drawable {at, {}, 1.0f, manager::DrawPath::Sorted};
		    drawable.atoms.push_back(
		        {&player_symbol::GlowCreator(), at, still, 1.5f * size, 1.0f, 99.0f, symbol.cellA, colour});
		    drawable.atoms.push_back(
		        {&player_symbol::GlowCreator(), at, spun, 1.5f * size, 1.0f, 99.0f, symbol.cellB, {255, 255, 255}});
		    drawable.atoms.push_back({&player_symbol::SymbolCreator(player), at, still, size, 1.0f, 255.0f, 0.0f, colour});
		    out.push_back(std::move(drawable));
	    });
}
} // namespace

void openblack::particles::RegisterTownBeliefRules()
{
	RegisterModifier("UR_TownCentreBelief", &MakeModifierOf<TownCentreBelief>);
	RegisterCondition("EC_CollectionShouldBeEmitting", &CollectionShouldBeEmitting);
}

void town_belief::Clear()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> centres;
	registry.Each<const Centre>([&centres](entt::entity entity, const Centre&) { centres.push_back(entity); });
	for (const auto entity : centres)
	{
		registry.RemoveState<Centre>(entity);
	}
	std::vector<entt::entity> artefacts;
	registry.Each<const ecs::components::ArtefactSymbol>(
	    [&artefacts](entt::entity entity, const ecs::components::ArtefactSymbol&) { artefacts.push_back(entity); });
	for (const auto entity : artefacts)
	{
		registry.RemoveState<ecs::components::ArtefactSymbol>(entity);
	}
}

void town_belief::RemoveCentre(entt::entity townCentre)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(townCentre))
	{
		return;
	}
	// the centre's effect is deleted with it
	if (const auto* centre = registry.TryGet<const Centre>(townCentre); centre != nullptr && centre->effect != 0)
	{
		manager::Delete(centre->effect);
	}
	registry.RemoveState<Centre>(townCentre);
}

std::optional<glm::vec3> town_belief::SymbolBase(entt::entity townCentre, float heightAt1)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> base;
	registry.Each<const VillageTotem, const Transform>(
	    [&](entt::entity plinth, const VillageTotem& statue, const Transform& at) {
		    if (base.has_value() || statue.townCentre != townCentre || !statue.onCentre)
		    {
			    return;
		    }
		    // the icon's drawn place, raised by the totem's height (its icon's mesh at the totem's scale) and HeightAt1;
		    // a totem with no icon has no height, and the plinth's place stands for its own
		    const auto* icon = registry.Valid(statue.icon) ? registry.TryGet<const Transform>(statue.icon) : nullptr;
		    if (icon == nullptr)
		    {
			    base = at.position + glm::vec3(0.0f, heightAt1, 0.0f);
			    return;
		    }
		    const auto half = ecs::object::ObjectHalfExtents(statue.icon);
		    const float height = half.has_value() ? ecs::object::Height(*half, ecs::object::GetScaleField(plinth)) : 0.0f;
		    base = icon->position + glm::vec3(0.0f, height + heightAt1, 0.0f);
	    });
	return base;
}

town_belief::Counts town_belief::Count()
{
	Counts counts;
	if (!Locator::entitiesRegistry::has_value())
	{
		return counts;
	}
	Locator::entitiesRegistry::value().Each<const Centre>([&counts](const Centre& centre) {
		const auto* effect = manager::Find(centre.effect);
		if (effect == nullptr)
		{
			return;
		}
		++counts.centres;
		ForEachSymbol(*effect, [&counts](const Atom& atom) {
			++counts.symbols;
			bool sparking = false;
			for (const auto& sub : atom.subCollections)
			{
				sparking = sparking || (sub->flags & k_EmittingFlag) != 0;
				counts.sparkles += sub->atoms.size();
			}
			counts.sparking += sparking ? 1 : 0;
		});
	});
	return counts;
}

void town_belief::MakeEffect(entt::entity townCentre)
{
	// (openblack guard) without a particle system or the game's files (the unit tests) there is no effect
	if (!Locator::entitiesRegistry::has_value() || !Locator::particleSystem::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(townCentre))
	{
		return;
	}
	// a centre made functional again (repaired) loses its effect for a new one
	auto* existing = registry.TryGet<Centre>(townCentre);
	auto& centre = existing != nullptr ? *existing : registry.AssignState<Centre>(townCentre);
	if (centre.effect != 0)
	{
		manager::Delete(centre.effect);
	}
	// the centre's own effect (a local one) at the origin, the centre its target, processed at once: an effect's first
	// process steps twice, the first step making the symbols and their random numbers here
	centre.effect = manager::Start(k_TownBeliefFile, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Local);
	auto* effect = manager::Find(centre.effect);
	if (effect == nullptr)
	{
		centre.effect = 0;
		return;
	}
	effect->AddTarget(townCentre);
	const float dt = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	for (int i = 0; i < 2 && centre.effect != 0; ++i)
	{
		if (!manager::ProcessByFrame(centre.effect, dt))
		{
			centre.effect = 0;
		}
	}
}

void town_belief::Step()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// the centre's effect steps with the ms of a turn, not the frame's: dt = ms x 0.001
	const float dt = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	// the symbol draw's game time step: the frame's whole game ms, 0 paused
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	// each centre's effect made when the centre was made functional (MakeEffect): one step of it
	ForEachCentre([&](entt::entity entity) {
		auto* centre = Locator::entitiesRegistry::value().TryGet<Centre>(entity);
		if (centre != nullptr && centre->effect != 0 && !manager::ProcessByFrame(centre->effect, dt))
		{
			centre->effect = 0; // finished: it has no symbols, and is not made again
		}
	});
	// each symbol's draw, once a frame: its glows' frame (the centres' and every other effect's)
	manager::StepPlayerSymbols(milliseconds);
	// and the owners' symbols over their towns' worthiest artefacts
	StepArtefactSymbols(milliseconds);
}

void town_belief::Collect(const glm::vec3& /*camera*/, std::vector<manager::Drawable>& out)
{
	// the centres' symbols are their effects' atoms, drawn with the other effects
	CollectArtefactSymbols(out);
}
