/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastAgenda.h"

#include <algorithm>

#include "Creature/CreatureLayers.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// How each kind of casting goes: what it shows first and the faces it pulls, and how near it goes and how far it
/// backs off, for its height
struct Style
{
	size_t emote;
	creature_face::Cue emoteFace;
	creature_face::Cue feeling;
	creature_face::Cue facing;
	float goNear;
	float backOff;
};

Style StyleOf(CastStyle style, float height)
{
	switch (style)
	{
	case CastStyle::Lightning:
		return {.emote = animations::k_Angry,
		        .emoteFace = creature_face::Cue::Anger,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = k_LightningCastDistance,
		        .backOff = height + k_LightningBackOff};
	case CastStyle::Helpful:
		return {.emote = animations::k_FeelingNice,
		        .emoteFace = creature_face::Cue::Compassion,
		        .feeling = creature_face::Cue::Compassion,
		        .facing = creature_face::Cue::Compassion,
		        .goNear = 2.0f * height,
		        .backOff = 2.0f * height};
	case CastStyle::Aggressive:
		return {.emote = animations::k_Angry,
		        .emoteFace = creature_face::Cue::Anger,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = 5.0f * height,
		        .backOff = 2.0f * height};
	case CastStyle::Explosion:
		// Angry going near, but amazed as it shows its anger first
		return {.emote = animations::k_Angry,
		        .emoteFace = creature_face::Cue::Amazed,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = k_ExplosionCastDistance,
		        .backOff = 2.0f * height};
	case CastStyle::Playful:
		break;
	case CastStyle::Amusing:
		// As a spell on a creature, but from nearer
		return {.emote = animations::k_FeelPlayful,
		        .feeling = creature_face::Cue::Playfulness,
		        .facing = creature_face::Cue::Compassion,
		        .goNear = k_AmusingCastHeights * height,
		        .backOff = 2.0f * height};
	}
	// Playful going near, but kind as it turns to face the creature it casts on
	return {.emote = animations::k_FeelPlayful,
	        .emoteFace = creature_face::Cue::Playfulness,
	        .feeling = creature_face::Cue::Playfulness,
	        .facing = creature_face::Cue::Compassion,
	        .goNear = 5.0f * height,
	        .backOff = 2.0f * height};
}

/// The step in which the creature goes near the object, its head on it as it walks there, pulling a face
Step GoNear(uint32_t object, float distance, creature_face::Cue face)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = distance},
	        .face = face,
	        .gaze = Gaze {.object = object, .goingTowards = true}};
}

/// The step in which the creature backs off from the object, its head on it
Step GetAway(uint32_t object, float distance)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = distance},
	        .gaze = Gaze {.object = object}};
}

/// The step in which the creature turns to face the object, its head on it, and holds still a moment, pulling a face
Step TurnToFace(uint32_t object, creature_face::Cue face)
{
	return {.kind = Step::Kind::Move,
	        .seconds = k_CastSettleSeconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	        .face = face,
	        .gaze = Gaze {.object = object}};
}

/// The step in which the creature casts a miracle at the object from the casting pose and holds it, grimacing as it
/// casts
Step CastFromPose(uint32_t magicType, uint32_t object)
{
	return {.kind = Step::Kind::Cast,
	        .seconds = k_CastHoldSeconds,
	        .animation = k_CastPose[1],
	        .sequence = k_CastPose,
	        .cast = {.magicType = magicType, .object = object},
	        .face = creature_face::Cue::Grimace};
}

/// The step in which the creature points at the object for a short time, pulling a face
Step PointAt(uint32_t object, creature_face::Cue face)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .object = object, .seconds = k_ImpressPointSeconds},
	        .face = face,
	        .gaze = Gaze {.object = object}};
}
} // namespace

std::vector<Step> creature_mind::CastAt(CastStyle style, uint32_t magicType, uint32_t gesture, uint32_t object, float height,
                                        const Random& random)
{
	const auto how = StyleOf(style, height);
	std::vector<Step> agenda;
	if (random(style == CastStyle::Explosion ? k_ExplosionEmoteLots : k_CastEmoteLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = how.emote, .face = how.emoteFace});
	}
	agenda.push_back(GoNear(object, how.goNear, how.feeling));
	agenda.push_back(GetAway(object, how.backOff));
	agenda.push_back(TurnToFace(object, how.facing));
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	// It grimaces as it casts
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_CastPose[1],
	                  .sequence = k_CastPose,
	                  .cast = {.magicType = magicType, .object = object},
	                  .face = creature_face::Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::CastAmusingAt(std::span<const AmusingSpell> spells, uint32_t object, float height,
                                               const Random& random)
{
	// With no spell to cast, it only shows that it feels playful
	if (spells.empty())
	{
		const auto how = StyleOf(CastStyle::Amusing, height);
		return {{.kind = Step::Kind::Action, .animation = how.emote, .face = how.feeling}};
	}
	// It picks the spell before it decides to show how it feels
	const auto& spell = spells[random(static_cast<uint32_t>(spells.size()))];
	return CastAt(CastStyle::Amusing, spell.magicType, spell.gesture, object, height, random);
}

bool creature_mind::SeemsFriendly(std::optional<creature_desires::Desire> mine, std::optional<creature_desires::Desire> theirs)
{
	using creature_desires::Desire;
	// The other creature's anger or fear rules it out, and only the creature's own anger
	return theirs != Desire::Anger && theirs != Desire::Fear && mine != Desire::Anger;
}

std::vector<Step> creature_mind::CastFoodAt(uint32_t gesture, uint32_t object, float goNear)
{
	std::vector<Step> agenda;
	agenda.push_back(GoNear(object, goNear, creature_face::Cue::Compassion));
	agenda.push_back(TurnToFace(object, creature_face::Cue::Compassion));
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Food), .object = object},
	                  .face = creature_face::Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::CastWaterAt(uint32_t object, float height)
{
	std::vector<Step> agenda;
	agenda.push_back(GoNear(object, 2.0f * height, creature_face::Cue::None));
	agenda.push_back(TurnToFace(object, creature_face::Cue::Smile));
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Water), .object = object},
	                  .face = creature_face::Cue::Smile});
	return agenda;
}

std::vector<Step> creature_mind::CastWaterOnItself(glm::vec2 spot, uint32_t self, float height)
{
	std::vector<Step> agenda;
	// It goes to within twice its height of the spot, pulling no face
	agenda.push_back(
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::ToPoint, .point = spot, .maxDistance = 2.0f * height}});
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Water), .object = self},
	                  .face = creature_face::Cue::Smile});
	// Then its own fire stops
	agenda.push_back({.kind = Step::Kind::CoolDown});
	return agenda;
}

std::vector<Step> creature_mind::CastFireballAt(uint32_t magicType, uint32_t gesture, uint32_t object, bool objectLiving,
                                                glm::vec2 clearArea, float height, const Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = clearArea, .maxDistance = 2.0f * height},
	                  .gaze = Gaze {.point = glm::vec3 {clearArea.x, 0.0f, clearArea.y}, .goingTowards = true}});
	// It shows its anger amazed
	if (random(k_CastEmoteLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Angry, .face = creature_face::Cue::Amazed});
	}
	if (objectLiving)
	{
		agenda.push_back(GetAway(object, 2.0f * height));
	}
	agenda.push_back(TurnToFace(object, creature_face::Cue::Anger));
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	agenda.push_back(CastFromPose(magicType, object));
	return agenda;
}

std::vector<Step> creature_mind::CastShieldAt(uint32_t magicType, uint32_t object)
{
	// It turns kindly to what it shields, with no look of its own
	auto turn = TurnToFace(object, creature_face::Cue::Compassion);
	turn.gaze.reset();
	return {turn, CastFromPose(magicType, object)};
}

std::vector<Step> creature_mind::CastForestAt(uint32_t magicType, uint32_t object, glm::vec3 place, float height)
{
	// The walk has no face, its head on the villager as it goes; the turn is kind, its head on the villager
	return {
	    GoNear(object, k_ForestCastHeights * height, creature_face::Cue::None),
	    TurnToFace(object, creature_face::Cue::Compassion),
	    {.kind = Step::Kind::Cast,
	     .seconds = k_ForestHoldSeconds,
	     .animation = k_ScatterPose[1],
	     .sequence = k_ScatterPose,
	     .cast = {.magicType = magicType, .place = place},
	     .face = creature_face::Cue::Grimace},
	};
}

std::vector<Step> creature_mind::CastToImpress(std::optional<uint32_t> magicType, uint32_t object, float height,
                                               const Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back(GoNear(object, k_ImpressCastHeights * height, creature_face::Cue::None));
	agenda.push_back(GetAway(object, 2.0f * height));
	const bool pointedFirst = random(k_ImpressPointLots) == 0;
	// Pointing first it is amazed; it beckons playfully
	if (pointedFirst)
	{
		agenda.push_back(PointAt(object, creature_face::Cue::Amazed));
	}
	if (random(k_ImpressBeckonLots) != 0)
	{
		agenda.push_back(
		    {.kind = Step::Kind::Action, .animation = animations::k_Summon, .face = creature_face::Cue::Playfulness});
	}
	agenda.push_back(TurnToFace(object, creature_face::Cue::None));
	if (magicType.has_value())
	{
		agenda.push_back(CastFromPose(*magicType, object));
	}
	// Pointing after the cast it smiles
	if (!pointedFirst)
	{
		agenda.push_back(PointAt(object, creature_face::Cue::Smile));
	}
	return agenda;
}

std::optional<size_t> creature_mind::PickImpressiveMiracle(const std::array<bool, 4>& known,
                                                           const std::array<uint32_t, 4>& castTurn)
{
	std::optional<size_t> picked;
	for (size_t i = 0; i < known.size(); ++i)
	{
		// Of equal turns, the last miracle in the list wins
		if (known.at(i) && (!picked.has_value() || !(castTurn.at(i) > castTurn.at(*picked))))
		{
			picked = i;
		}
	}
	return picked;
}

bool creature_mind::CanCastToImpress(std::span<const ImpressiveMiracleState> miracles)
{
	return std::ranges::any_of(miracles, [](const ImpressiveMiracleState& miracle) {
		// Seen more than half the times it must see it: a share that is no number (none seen of none needed) fails
		const float share = miracle.seen / miracle.needed;
		return miracle.known && miracle.canPay && share > 0.5f;
	});
}
