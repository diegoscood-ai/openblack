/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Physics/Body.h"
#include "Physics/TurnRules.h"

namespace openblack::ecs
{
/// PhysicsEntry: one body of the physics system, thrown or knocked (awake) or a resting obstacle near a moving body
/// (asleep proxy).
struct PhysicsEntry
{
	// The bits of `flags`
	static constexpr uint32_t k_Awake = 0x1;
	/// Pushed by a Living (a villager is not raised over it by RaiseClearOfWhatIsUnder)
	static constexpr uint32_t k_PushedByLiving = 0x2;
	static constexpr uint32_t k_FromHand = 0x4;
	static constexpr uint32_t k_Landed = 0x8;
	static constexpr uint32_t k_NoObjectCollision = 0x10;

	entt::entity entity {entt::null};
	entt::entity thrower {entt::null};
	openblack::physics::Body body;
	/// The turn-start matrix's rows (right / up / fwd = glm's columns 0 / 1 / 2): a villager's or an animal's end of
	/// physics reads the land type from its right.y and the villager's heading from its up / fwd rows. The current matrix
	/// is the body's. Written with turnStartCentre by SyncTurnStart
	glm::mat3 turnStartRotation {1.0f};
	/// The turn-start matrix's translation (the current one is the centre of mass): the drawing interpolates from this
	/// matrix
	glm::vec3 turnStartCentre {0.0f};
	/// A game turn has started since the body was added: until then it is drawn where it is. Before the first turn the
	/// two matrices are equal anyway: SetUpPos, SettleOnLand and so RaiseClearOfWhatIsUnder when it raises the
	/// body all copy the current matrix into the turn-start one (DynamicsSystem::SyncTurnStart)
	bool turnStarted {false};
	uint32_t flags {0};
	bool villager {false};
	/// 1 a villager, 2 a felled tree, 3 a felled tree that has toppled, else 0
	uint8_t kind {0};
	/// The player's hand threw it, directly or through what it hit (inherited by proxies)
	bool byPlayer {false};
	glm::vec3 forceSum {0.0f}; ///< The force of the touched substeps of this turn
	float impact {0.0f};       ///< |forceSum| x 0.05, the mean force of the turn
	/// What hit it, as a place in the physics list: it names whatever entry holds that place when it is read
	/// (physics::hitter::HitterEntry), and stays from turn to turn until a turn whose force counts looks it up again
	std::optional<uint32_t> hitBySlot;
	/// The body it last touched in this turn's steps, as its place in the list (physics::hitter::CaptureLastHits)
	std::optional<uint32_t> lastHitSlot;
	/// the squared distance to the camera at the last substep (the fly-by whoosh)
	float cameraDistance2 {1e9f};

	/// G of the turn: impact / (mass x g), 1 while lying on the ground
	[[nodiscard]] float GLoad() const { return openblack::physics::turn::GLoad(impact, body.Mass()); }
};

/// The object classes whose physics hooks (start, impact reaction, end of physics, sinking) another system owns. One
/// class per object (DynamicsSystem::ClassOf).
enum class PhysicsClass : uint8_t
{
	Villager,
	Animal,
	Tree,
	DeadTree,
	Pot,
	Rock,
	Fragment,
	Building, ///< Abode and StoragePit
	Shield,   ///< A physical shield (MagicShield)
	/// Creature: never flies, only a resting body thrown objects hit
	Creature,
	Scaffold, ///< Scaffold (components::Scaffold)
	Other,
	_Count
};

/// What the physics knows of a body's impact at the end of a game turn.
struct ImpactInfo
{
	float g {0.0f};                  ///< impact / (mass x g), PhysicsEntry::GLoad
	entt::entity hitBy {entt::null}; ///< The body that hit it, or null
	entt::entity thrower {entt::null};
	bool byPlayer {false}; ///< The hand threw it
};
} // namespace openblack::ecs
