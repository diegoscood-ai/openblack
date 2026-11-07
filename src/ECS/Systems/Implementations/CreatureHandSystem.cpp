/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureHandSystem.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/TriangleCollide.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkinRay.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureSkin.h"
#include "ECS/HandLandPoint.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "HandSystemDetail.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace feedback = openblack::creature_feedback;

namespace
{
/// The furthest a line of sight is followed to a creature
constexpr float k_RayLength = 1e5f;

/// The camera's mode changed: its time in the mode starts again
void CameraModeChanged()
{
	if (Locator::scriptState::has_value())
	{
		script_camera::ModeChanged();
	}
}

entt::entity PlayerHand()
{
	return Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	if (!rigs.Contains(id))
	{
		return nullptr;
	}
	const auto& rig = *rigs.Handle(id);
	return rig.actionPoints.has_value() ? &*rig.actionPoints : nullptr;
}

/// The creature's body as the hand can touch it this frame: capsules round its bones as the body is drawn this frame
/// (creature_turn_pose::BonesDrawn: between its last two turn poses), in the world, placed by the body's drawn matrix
/// (ecs::DrawnBodyModel), as thick as the size it is drawn at (`drawnSize`: the pen's in its pen)
std::vector<feedback::Capsule> BodyOf(float drawnSize, const CreatureAnimation& animation, const glm::mat4& drawn)
{
	return feedback::BodyCapsules(animation.skeleton.parents, creature_turn_pose::BonesDrawn(animation), drawn,
	                              feedback::k_BodyRadiusShare * feedback::k_HeightAtSizeOne * drawnSize);
}

/// Where each part of the body a stroke can land on is, in the world, read from the body's latest turn pose: neither the
/// pose kept for the rest of the game while the hand holds the creature, nor the body as it is drawn this frame. A body
/// not posed yet has no part placed. None without the species' parts
std::optional<feedback::PartPlaces> PartsOf(const Creature& creature, const CreatureAnimation& animation,
                                            const CreatureTurnPose* pose)
{
	const auto* points = PointsOf(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	const auto mirror = [&](uint32_t bone) { return bone < animation.mirror.size() ? animation.mirror[bone] : bone; };
	const std::array<uint32_t, feedback::k_BodyPartCount> bones {
	    points->head,
	    points->rightArmpit,
	    mirror(points->rightArmpit),
	    points->belly,
	    points->groin,
	    points->rightFoot,
	    mirror(points->rightFoot),
	    points->rightHand,
	    mirror(points->rightHand),
	};
	return feedback::PlacesOfParts(bones, pose != nullptr ? creature_turn_pose::Latest(*pose) : std::span<const glm::mat4> {});
}

/// The heading the creature's body is drawn at, or the one it is turning to while it turns
float HeadingOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	if (const auto* walking = registry.TryGet<const CreatureLocomotion>(creature); walking != nullptr && walking->started)
	{
		const bool turning =
		    walking->motion == CreatureLocomotion::Motion::Turning || walking->motion == CreatureLocomotion::Motion::Stepping;
		return turning ? walking->targetHeading : walking->heading;
	}
	const auto ahead = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
	return creature_locomotion::HeadingOf({ahead.x, ahead.z});
}

/// The camera's place for the creature as it now stands, as tall as it is drawn
std::optional<creature_interact::View> InteractViewOf(const ecs::Registry& registry, entt::entity creature)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto height = feedback::k_HeightAtSizeOne * ecs::creature_pose::DrawnSize(registry, creature);
	return creature_interact::ViewOf(transform.position, height, HeadingOf(registry, creature, transform));
}

/// Where a line of sight meets the upright plane through the creature that faces back along it
std::optional<glm::vec3> OnPlaneThrough(const glm::vec3& centre, const glm::vec3& origin, const glm::vec3& direction)
{
	auto normal = glm::vec3(direction.x, 0.0f, direction.z);
	if (glm::length(normal) < 1e-4f)
	{
		normal = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	normal = glm::normalize(normal);
	const auto facing = glm::dot(direction, normal);
	if (std::abs(facing) < 1e-6f)
	{
		return std::nullopt;
	}
	const auto along = glm::dot(centre - origin, normal) / facing;
	return along > 0.0f ? std::optional(origin + (direction * along)) : std::nullopt;
}
} // namespace

bool CreatureHandSystem::Grab(entt::entity creature)
{
	return MayHold(creature) && HoldAtTurn(creature);
}

bool CreatureHandSystem::HoldAtTurn(entt::entity creature)
{
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(creature) || !lookup.AllOf<Creature>(creature))
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = PlayerHand();
	registry.AssignOrReplaceState<HandOnCreature>(hand, HandOnCreature {.creature = creature});
	if (std::as_const(registry).AllOf<HandLastFeedback>(hand))
	{
		registry.RemoveState<HandLastFeedback>(hand);
	}
	_pose.reset();
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().HandTookHold(creature);
	}
	return true;
}

bool CreatureHandSystem::MayHold(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return false;
	}
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	const auto* knockedOut = registry.TryGet<const CreatureKnockedOut>(creature);
	const creature_hand::Holdable holdable {
	    .owner = body->owner,
	    .species = body->species,
	    .frozen = spells != nullptr && spells->spells.IsActive(creature_spells::Spell::Freeze),
	    .scriptControlled = ecs::script_held::IsControlledByScript(creature),
	    .outCold = knockedOut != nullptr && creature_hand::IsOutCold(knockedOut->stage, knockedOut->permanent),
	};
	if (!creature_hand::MayHold(holdable))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The hand can't hold creature {}: it is nobody's, an ogre, frozen, out cold or under a script",
		                   entt::to_integral(creature));
		return false;
	}
	return true;
}

std::optional<CreatureHandSystem::CreatureHit> CreatureHandSystem::CreatureAlong(const glm::vec3& rayOrigin,
                                                                                 const glm::vec3& rayDirection) const
{
	std::optional<CreatureHit> nearest;
	float best = k_RayLength;
	// the body where and as it is drawn this frame, the matrix the hand sees
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	registry.Each<const Creature, const CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature&, const CreatureAnimation& animation, const Transform&) {
		    // its skin as it is drawn, met as the game's draw collision meets any drawn object (triangle_collide::First:
		    // the first triangle in the order of its parts, primitives and triangles, a one-sided primitive's back faces
		    // left out); (openblack) one whose mesh is not loaded by the capsules round its posed bones
		    std::optional<float> hit;
		    if (const auto skin = ecs::creature_skin::DrawnSkin(registry, entity); !skin.empty())
		    {
			    std::vector<triangle_collide::Range> ranges;
			    for (const auto& part : skin)
			    {
				    ranges.clear();
				    for (const auto& primitive : part.primitives)
				    {
					    ranges.push_back({.first = primitive.first, .count = primitive.count, .twoSided = primitive.twoSided});
				    }
				    if (const auto first = triangle_collide::First(part.corners, part.indices, ranges, rayOrigin, rayDirection);
				        first.has_value())
				    {
					    hit = first->t;
					    break;
				    }
			    }
		    }
		    else
		    {
			    const auto body =
			        BodyOf(ecs::creature_pose::DrawnSize(registry, entity), animation, ecs::DrawnBodyModel(registry, entity));
			    hit = feedback::RayHit(rayOrigin, rayDirection, body);
		    }
		    if (hit.has_value() && *hit < best)
		    {
			    best = *hit;
			    nearest = CreatureHit {.creature = entity, .distance = *hit};
		    }
	    });
	return nearest;
}

std::optional<CreatureHandSystem::HandPose> CreatureHandSystem::Update(const glm::vec3& rayOrigin,
                                                                       const glm::vec3& rayDirection, glm::vec2 cursor,
                                                                       float screenWidth, float seconds,
                                                                       const CursorSweep& sweep)
{
	_pose.reset();
	auto& registry = Locator::entitiesRegistry::value();
	// read through the const registry first: without a contact nothing is written, and no storage is made
	if (!Locator::handSystem::has_value() || !std::as_const(registry).AllOf<HandOnCreature>(PlayerHand()))
	{
		return std::nullopt;
	}
	auto* contact = &registry.Get<HandOnCreature>(PlayerHand());
	const auto creatureEntity = contact->creature;
	if (!registry.Valid(creatureEntity) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creatureEntity))
	{
		registry.RemoveState<HandOnCreature>(PlayerHand());
		return std::nullopt;
	}
	const auto& animation = registry.Get<const CreatureAnimation>(creatureEntity);
	// the body where and as it is drawn this frame, the one the hand touches
	const auto drawn = ecs::DrawnBodyModel(std::as_const(registry), creatureEntity);
	const auto ms = seconds * 1000.0f;
	// its height is the size it is drawn at's, as object::GetHeight gives it (the pen's in its pen)
	const auto drawnSize = ecs::creature_pose::DrawnSize(std::as_const(registry), creatureEntity);
	const auto height = feedback::k_HeightAtSizeOne * drawnSize;
	const auto centre = glm::vec3(drawn[3]) + glm::vec3(0.0f, height * 0.5f, 0.0f);

	// Where the hand is: on the body under the cursor, or beside it on the plane through the creature. The body is its
	// skin as it is drawn, met triangle by triangle as the game's mesh test meets it, the way out of it at the touch the
	// hit triangle's (minus the inward normal the game's test gives). (openblack) a creature whose mesh is not loaded
	// has no skin: its capsules round its posed bones stand in, the way out from the touched capsule's axis
	const auto onPlane = OnPlaneThrough(centre, rayOrigin, rayDirection);
	const auto body = BodyOf(drawnSize, animation, drawn);
	const auto skin = ecs::creature_skin::DrawnSkin(std::as_const(registry), creatureEntity);
	std::optional<glm::vec3> touch;
	std::optional<glm::vec3> outward;
	if (!skin.empty())
	{
		if (const auto meshHit = creature_skin_ray::Nearest(skin, rayOrigin, rayDirection); meshHit.has_value())
		{
			touch = meshHit->point;
			outward = creature_skin_ray::OutwardOf(skin, meshHit->spot);
		}
	}
	else if (const auto hit = feedback::RayHit(rayOrigin, rayDirection, body); hit.has_value())
	{
		touch = rayOrigin + (rayDirection * *hit);
		outward = feedback::OutwardAt(*touch, body);
	}
	// missing it, the hand looks at the body once more, through halfway between the cursor and where it last was; that
	// hit counts as the frame's
	if (!touch.has_value() && sweep.secondLook.has_value())
	{
		const auto& again = *sweep.secondLook;
		if (!skin.empty())
		{
			if (const auto meshHit = creature_skin_ray::Nearest(skin, again.origin, again.direction); meshHit.has_value())
			{
				touch = meshHit->point;
				outward = creature_skin_ray::OutwardOf(skin, meshHit->spot);
			}
		}
		else if (const auto hit = feedback::RayHit(again.origin, again.direction, body); hit.has_value())
		{
			touch = again.origin + (again.direction * *hit);
			outward = feedback::OutwardAt(*touch, body);
		}
		if (touch.has_value())
		{
			++contact->secondLookHits;
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Creature hand: the second look found creature {}'s body at ({:.2f}, {:.2f}, {:.2f})",
			                   entt::to_integral(creatureEntity), touch->x, touch->y, touch->z);
		}
	}
	const auto point = onPlane.value_or(touch.value_or(centre));
	// The hand is drawn larger on a bigger creature, by the square root of its size as the game's power gives it
	const auto handScale = creature_hand::HandScaleOnCreature(drawnSize);

	contact->heldMs += ms;
	contact->blendStarts = 0;
	// the body's bottom bone where the game reads its pose, which the hand's distance keeps clear of
	std::optional<glm::vec3> groin;
	const auto* points = PointsOf(registry.Get<const Creature>(creatureEntity));
	const auto* turnPose = std::as_const(registry).TryGet<const CreatureTurnPose>(creatureEntity);
	if (points != nullptr && turnPose != nullptr)
	{
		const auto bones = creature_turn_pose::Safe(*turnPose);
		if (points->groin < bones.size())
		{
			groin = glm::vec3(bones[points->groin][3]);
		}
	}
	// The sweep: this frame's cursor and last frame's, each put on its line from the eye at the hand's distance as the
	// frame starts (no further than the body's bottom allows), with this frame's camera
	const float depth = hand_detail::GroinClampedDistance(sweep.distance, rayOrigin, groin, height);
	const auto sweepSpeed =
	    feedback::SweepSpeed(ecs::hand_land_point::AlongMouseRay(rayOrigin, sweep.nearNow, depth),
	                         ecs::hand_land_point::AlongMouseRay(rayOrigin, sweep.nearLast, depth), seconds);
	// Strokes and slaps are weighed once a frame, each step on that frame's milliseconds (creature_feedback::Pace is
	// the fix kept beside it, unused)
	if (ms > 0.0f)
	{
		Step(*contact, {.point = point,
		                .sweepSpeed = sweepSpeed,
		                .touch = touch,
		                .cursor = cursor,
		                .screenWidth = screenWidth,
		                .stepMs = ms,
		                .height = height});
	}
	// Each frame it looks at the eye, or near the hand once the hand is far from its body's bottom: while the hand holds
	// it nothing else sets where it looks. Once the button has let go nothing does, so the head the let-go turned back
	// ahead comes to rest and the hold can end
	if (const auto hand = Locator::handSystem::value().GetPlayerHandPositions()[0];
	    !contact->released && groin.has_value() && hand.has_value())
	{
		const auto look = feedback::HeldLookPoint(rayOrigin, rayDirection, *hand, *groin, height);
		registry.Get<CreatureAnimation>(creatureEntity).lookAt = look;
		if (auto* eyes = registry.TryGet<CreatureEyes>(creatureEntity); eyes != nullptr)
		{
			eyes->lookAt = look;
		}
		// the game log tells each change between the eye and the hand
		const bool atEye = look == rayOrigin;
		if (!contact->lookAt.has_value() || atEye != contact->lookingAtEye)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature hand: creature {} looks {}", entt::to_integral(creatureEntity),
			                   atEye ? "at the eye" : "near the hand");
		}
		contact->lookAt = look;
		contact->lookingAtEye = atEye;
	}
	_pose = HandPose {.position = touch.value_or(point),
	                  .onBody = touch.has_value(),
	                  .cycle = contact->cycle,
	                  .scale = handScale,
	                  .blendStarts = contact->blendStarts,
	                  .height = height,
	                  .groin = groin,
	                  .outward = outward};
	return _pose;
}

void CreatureHandSystem::Step(HandOnCreature& contact, const StepInput& input)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto creatureEntity = contact.creature;
	const auto& creature = registry.Get<const Creature>(creatureEntity);
	const auto& animation = registry.Get<const CreatureAnimation>(creatureEntity);
	const auto& transform = registry.Get<const Transform>(creatureEntity);
	const auto height = input.height;
	const bool onBody = input.touch.has_value();

	auto& clocks = contact.clocks;
	clocks.sinceTouch += input.stepMs;
	clocks.ready += input.stepMs;
	contact.slapReadyMs += input.stepMs;
	// How fast the cursor sweeps across the land at the hand's depth, up and down not counting, not eased
	contact.speed = input.sweepSpeed;
	const bool cursorMoved = input.cursor.x != contact.lastCursor.x;
	bool stroking = false;

	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(input.point))
	                                                        : transform.position.y;
	if (!Locator::creatureMindSystem::has_value())
	{
		std::fputs("creature hand: no creature minds in the locator (Locator::creatureMindSystem)\n", stderr);
		std::abort();
	}
	auto& minds = Locator::creatureMindSystem::value();
	// A slap is weighed only as the hand comes onto the body or the cursor crosses the screen's middle, a second after
	// taking hold or the last stroke; it is mirrored when the cursor moves left
	std::optional<feedback::Slap> slap;
	if (feedback::SlapConsidered(onBody, contact.wasOnBody, input.cursor.x, contact.lastCursor.x, input.screenWidth * 0.5f) &&
	    feedback::SlapReady(contact.slapReadyMs))
	{
		slap = feedback::ClassifySlap(input.point.y - ground, contact.speed, height, input.cursor.x < contact.lastCursor.x);
		// the game log tells each sweep weighed, and what it made
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Creature hand: a sweep weighed at {:.1f} a second, the creature {:.1f} high: {}", contact.speed,
		                   height,
		                   slap.has_value() ? fmt::format("a {} slap, animation {}{}", slap->gentle ? "gentle" : "hard",
		                                                  slap->animation, slap->mirrored ? ", mirrored" : "")
		                                    : std::string("no slap"));
	}
	if (slap.has_value())
	{
		// Every third hard slap in a row at one height bruises when it lands on the body, the count starting again after
		// each: the player's own creature loses a little life at the next turn, whichever creature was slapped
		if (feedback::CountSlap(contact.hardSlaps, *slap, onBody))
		{
			game_packets::Push(game_packets::Packet {game_packets::Type::CreatureHardSlap});
		}
		// The slap only counts when the creature can reel from it
		if (minds.ForceAction(creatureEntity, slap->animation, slap->mirrored, std::nullopt, feedback::k_SlapInterruptsAfter))
		{
			clocks.sinceTouch = 0.0f;
			clocks.ready = feedback::k_ReadyAfterTouchMs;
			contact.sum = feedback::AfterSlap(contact.sum, slap->gentle);
			contact.strokedOrSlapped = true;
		}
		clocks.contact = 0.0f;
	}
	else if (!onBody)
	{
		contact.lastPart.reset();
	}
	else
	{
		// Resting on the body long enough, the moving hand caresses it, and strokes the part it is nearest when one is due
		clocks.contact += input.stepMs;
		const auto parts = clocks.contact > feedback::k_StrokeHoldMs && cursorMoved
		                       ? PartsOf(creature, animation, registry.TryGet<const CreatureTurnPose>(creatureEntity))
		                       : std::nullopt;
		if (parts.has_value())
		{
			stroking = true;
			const auto part = feedback::NearestPart(*input.touch, *parts);
			const auto index = static_cast<size_t>(part);
			if (feedback::StrokeDue(clocks, contact.lastPart, part, contact.sum) &&
			    minds.ForceAction(creatureEntity, feedback::k_RewardAnimations.at(index), feedback::k_RewardMirrored.at(index),
			                      feedback::RewardFace(part), feedback::k_StrokeInterruptsAfter))
			{
				contact.lastPart = part;
				clocks.sinceTouch = 0.0f;
				clocks.ready = feedback::k_ReadyAfterTouchMs;
				contact.slapReadyMs = 0.0f;
				contact.sum = feedback::AfterStroke(contact.sum);
				contact.strokedOrSlapped = true;
			}
		}
	}
	const bool fast = contact.speed > feedback::k_SlapSpeed * height;
	const auto cycle = feedback::NextCycle(contact.cycle, fast, stroking, onBody);
	contact.cycle = cycle.cycle;
	contact.blendStarts = cycle.blendStarts;
	contact.cycleBlends += cycle.blendStarts;
	contact.wasOnBody = onBody;
	contact.lastPoint = input.point;
	contact.lastCursor = input.cursor;
}

std::optional<CreatureHandSystem::Feedback> CreatureHandSystem::Release()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto hand = PlayerHand();
	const auto* contact = std::as_const(registry).TryGet<const HandOnCreature>(hand);
	if (contact == nullptr)
	{
		return std::nullopt;
	}
	const auto creature = contact->creature;
	const auto sum = contact->sum;
	registry.RemoveState<HandOnCreature>(hand);
	registry.AssignOrReplaceState<HandLastFeedback>(hand, HandLastFeedback {.sum = sum});
	_pose.reset();
	// the hand sends it to the mind as a packet, applied at the next turn's start
	return Feedback {.creature = creature, .feedback = feedback::Delivered(sum)};
}

HandOnCreature* CreatureHandSystem::HoldByCommand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value() || !registry.Valid(creature) ||
	    !std::as_const(registry).AllOf<Creature, CreatureAnimation, Transform>(creature) || !MayHold(creature))
	{
		return nullptr;
	}
	const auto hand = PlayerHand();
	if (auto* contact = registry.TryGet<HandOnCreature>(hand); contact != nullptr)
	{
		if (contact->creature != creature)
		{
			return nullptr;
		}
		contact->byCommand = true;
		return contact;
	}
	if (std::as_const(registry).AllOf<HandLastFeedback>(hand))
	{
		registry.RemoveState<HandLastFeedback>(hand);
	}
	_pose.reset();
	return &registry.AssignOrReplaceState<HandOnCreature>(hand, HandOnCreature {.creature = creature, .byCommand = true});
}

bool CreatureHandSystem::Stroke(entt::entity creature, feedback::BodyPart part)
{
	auto* contact = HoldByCommand(creature);
	if (contact == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	const auto index = static_cast<size_t>(part);
	contact->lastPart = part;
	contact->cycle = feedback::TouchCycle::Tickle;
	if (!Locator::creatureMindSystem::value().ForceAction(creature, feedback::k_RewardAnimations.at(index),
	                                                      feedback::k_RewardMirrored.at(index), feedback::RewardFace(part),
	                                                      feedback::k_StrokeInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterStroke(contact->sum);
	contact->clocks.sinceTouch = 0.0f;
	contact->clocks.ready = feedback::k_ReadyAfterTouchMs;
	contact->slapReadyMs = 0.0f;
	contact->strokedOrSlapped = true;
	return true;
}

bool CreatureHandSystem::Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight)
{
	auto* contact = HoldByCommand(creature);
	if (contact == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto height = feedback::k_HeightAtSizeOne * ecs::creature_pose::DrawnSize(std::as_const(registry), creature);
	// As fast across the body as a gentle slap or a hard one is
	const auto speed =
	    (gentle ? (feedback::k_SlapSpeed + feedback::k_HardSlapSpeed) * 0.5f : feedback::k_HardSlapSpeed * 1.5f) * height;
	// mirrored when the hand sweeps left
	const auto slap = feedback::ClassifySlap(heightShare * height, speed, height, !sweepsRight);
	if (!slap.has_value())
	{
		return false;
	}
	contact->lastPart.reset();
	contact->lastPoint =
	    std::as_const(registry).Get<const Transform>(creature).position + glm::vec3(0.0f, heightShare * height, 0.0f);
	contact->cycle = feedback::TouchCycle::Slap;
	contact->strokedOrSlapped = true;
	if (!Locator::creatureMindSystem::value().ForceAction(creature, slap->animation, slap->mirrored, std::nullopt,
	                                                      feedback::k_SlapInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterSlap(contact->sum, slap->gentle);
	contact->clocks.sinceTouch = 0.0f;
	contact->clocks.ready = feedback::k_ReadyAfterTouchMs;
	return true;
}

void CreatureHandSystem::FollowWithCamera(entt::entity creature)
{
	if (!_cameraOn || !Locator::camera::has_value())
	{
		GiveCameraBack();
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (_cameraModel == nullptr)
	{
		// Asked once as the interaction starts. Any camera that can be left gives way, the player's own, Creature Mode's,
		// a miracle's placed path or a script's dual camera: not a living script camera, the temple's or the editor's,
		// and not while the creature is going
		if (!creature_interact::AsksForTheCamera(_cameraAsked))
		{
			return;
		}
		_cameraAsked = true;
		auto& camera = Locator::camera::value();
		const auto holder = creature_interact::CurrentHolder(camera);
		if (!creature_interact::CameraGivesWay(holder, ecs::IsAvailable(creature)))
		{
			return;
		}
		_takenFrom = holder;
		// The player's camera flies back to where it was going once it has the view again; Creature Mode's goes on
		// following
		if (creature_interact::HolderOf(camera.GetModel()) == creature_interact::Holder::PlayersCamera)
		{
			const auto& world = camera.GetModel();
			_returnTo = creature_interact::View {.origin = world.GetTargetOrigin(), .focus = world.GetTargetFocus()};
		}
		// The interaction's camera is current above every other, the script's modes and the placed paths included,
		// going on from the camera's zoomers as they are
		if (Locator::scriptState::has_value())
		{
			script_camera::Cover(camera.GetOriginZoomer(), camera.GetFocusZoomer());
		}
		if (Locator::cameraPathSystem::has_value())
		{
			Locator::cameraPathSystem::value().SetCovered(true);
		}
		auto model = std::make_unique<CreatureInteractCameraModel>(camera.GetOrigin(), camera.GetFocus());
		_cameraModel = model.get();
		_cameraModel->SetPlayerModel(camera.SetModel(std::move(model)));
		CameraModeChanged();
	}
	// The camera follows the creature, until the player has no creature
	const bool playerHasCreature = Locator::playerSystem::has_value() &&
	                               Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value();
	_cameraModel->SetTarget(playerHasCreature ? InteractViewOf(registry, creature) : std::nullopt);
}

void CreatureHandSystem::GiveCameraBack()
{
	// the next interaction asks for the camera again
	const bool asked = std::exchange(_cameraAsked, false);
	auto* model = std::exchange(_cameraModel, nullptr);
	const auto returnTo = std::exchange(_returnTo, std::nullopt);
	switch (creature_interact::AtTheEndOfTheHold(asked && _cameraOn, model != nullptr))
	{
	case creature_interact::HoldEnd::Nothing:
		return;
	case creature_interact::HoldEnd::TakeOff:
		TakeOffTheCameraThatKeptTheView();
		return;
	case creature_interact::HoldEnd::GiveBack:
		break;
	}
	// The script's modes and the placed paths under the interaction's camera are no longer covered: the top one left
	// has the view again, unless something else took the camera over meanwhile
	if (Locator::scriptState::has_value() && Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		script_camera::Uncover(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	if (Locator::cameraPathSystem::has_value())
	{
		Locator::cameraPathSystem::value().SetCovered(false);
	}
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// Something else may have taken the camera over meanwhile, taking ours with it
	if (&camera.GetModel() != model)
	{
		return;
	}
	auto player = model->TakePlayerModel();
	if (player == nullptr)
	{
		return;
	}
	const auto origin = camera.GetOrigin();
	const auto focus = camera.GetFocus();
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(player.get());
	auto* follow = dynamic_cast<CreatureCameraModel*>(player.get());
	// the interaction's model goes with this
	std::ignore = camera.SetModel(std::move(player));
	CameraModeChanged();
	const auto holder = creature_interact::CurrentHolder(camera);
	// The player's camera starts again from where the camera now is and flies back to where it was going, wooshing only
	// from far away
	if (holder == creature_interact::Holder::PlayersCamera && world != nullptr && returnTo.has_value())
	{
		world->PlaceAt(origin, focus);
		world->SetFlight(returnTo->origin, returnTo->focus);
	}
	// Creature Mode's camera goes on following its creature from where the camera now is, at its starting pace again
	if (holder == creature_interact::Holder::CreatureMode && follow != nullptr)
	{
		follow->Restart();
	}
	// A placed path or a dual camera moves the camera again from where it is: their restart does nothing more (the
	// path's close near plane comes back with it)
}

void CreatureHandSystem::TakeOffTheCameraThatKeptTheView()
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto holder = creature_interact::CurrentHolder(camera);
	if (!creature_interact::TakenOffAtTheEnd(holder))
	{
		return;
	}
	bool takenOff = false;
	switch (holder)
	{
	case creature_interact::Holder::ScriptCamera:
		takenOff = Locator::scriptState::has_value() &&
		           script_camera::TakeOffScriptMode(camera.GetOriginZoomer(), camera.GetFocusZoomer());
		break;
	case creature_interact::Holder::DualCamera:
		takenOff = Locator::scriptState::has_value() && script_camera::ReleaseDual();
		break;
	case creature_interact::Holder::PlacedPath:
		takenOff = Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().TakeOffCurrent();
		break;
	case creature_interact::Holder::CreatureMode:
		takenOff = Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive();
		if (takenOff)
		{
			Locator::creatureModeSystem::value().Leave();
		}
		break;
	case creature_interact::Holder::PlayersCamera:
	case creature_interact::Holder::Other:
		break;
	}
	if (!takenOff)
	{
		return;
	}
	_lastTakenOff = holder;
	CameraModeChanged();
	// The camera under it has the view: the player's camera starts again from where the camera is and flies back to where
	// it was going, as when the interaction gives the view back to it
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel());
	if (world != nullptr && creature_interact::CurrentHolder(camera) == creature_interact::Holder::PlayersCamera)
	{
		const auto origin = camera.GetOrigin();
		const auto focus = camera.GetFocus();
		const auto returnTo = creature_interact::View {.origin = world->GetTargetOrigin(), .focus = world->GetTargetFocus()};
		world->PlaceAt(origin, focus);
		world->SetFlight(returnTo.origin, returnTo.focus);
	}
}

void CreatureHandSystem::SetInteractionCamera(bool on)
{
	_cameraOn = on;
	if (!on)
	{
		GiveCameraBack();
	}
}

bool CreatureHandSystem::IsHeldByCommand() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr && contact->byCommand;
}

std::optional<entt::entity> CreatureHandSystem::GetCreature() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr ? std::optional(contact->creature) : std::nullopt;
}

float CreatureHandSystem::GetFeedbackSum() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr ? contact->sum : 0.0f;
}

float CreatureHandSystem::GetLastFeedbackSum() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value())
	{
		return 0.0f;
	}
	if (const auto* contact = registry.TryGet<const HandOnCreature>(PlayerHand()))
	{
		return contact->sum;
	}
	const auto* last = registry.TryGet<const HandLastFeedback>(PlayerHand());
	return last != nullptr ? last->sum : 0.0f;
}
