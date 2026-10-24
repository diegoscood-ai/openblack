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

#include <deque>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The steps a computer player's hand can be given, by the original's numbers. A script or the player's mind lines them
/// up one after the other; some need a move of the hand first, which is put in front of them when they start.
enum class HandSubAction : uint8_t
{
	PickUpObject = 0,
	MoveHandToPos = 1,
	MoveHandToObject = 2,
	MoveHandToCreatureHead = 3,
	MoveHandNearPos = 4,
	MoveHandNearObject = 5,
	MoveHandToPosStopWhenOutOfInfluence = 6,
	ThrowObjectInHandAtPos = 7,
	DropObjectInHandAtPos = 8,
	SayText = 9,
	GiveObjectInHandToCreature = 10,
	SetLeash = 11,
	AttachLeashToObject = 12,
	SlapCreature = 13,
	StrokeCreature = 14,
	CastSpellAtObject = 15,
	CastSpellAtPos = 16,
	CastSpellTowardPos = 17,
	CastShieldAtPos = 18,
	CloseDownCurrentSpell = 19,
	RaiseTotem = 20,
	LowerTotem = 21,
	MakeGesture = 22,
	Wait = 23,
	DropObjectInHandOnObject = 24,
	ForceCreatureToPerformAction = 25,
	ForceCreatureDominantDesire = 26,
	WaitForObjectToBeNearPos = 27,
	WaitForSpellToWearOff = 28,
	CreateFoodFromContainer = 29,
	CreateWoodFromContainer = 30,
	SetCreatureSpeedForJourney = 31,
	MakeWorshippersChantForSpell = 32,
	MakeWorshippersChantForSpellNoWait = 33,
	SetHandToPos = 34,
	MoveHandToPosDirectYInterpolation = 35,
	SetHandToPosDirectY = 36,
	/// No step: a step without a move in front of it
	None = 37,
};

/// What a step acts on
struct HandArgument
{
	enum class Kind : uint8_t
	{
		Thing = 0,
		Point = 1,
		Int = 3,
		Float = 5,
	};

	Kind kind {Kind::Float};
	entt::entity thing {entt::null};
	glm::vec3 point {0.0f};
	int32_t intValue {0};
	float floatValue {0.0f};
};

/// One step of the hand and what it acts on
struct HandStep
{
	HandSubAction type {HandSubAction::None};
	std::vector<HandArgument> arguments;
};

/// An action that a script queued for the computer player, by its name, to start when the hand is free
struct QueuedHandAction
{
	std::string name;
	entt::entity first {entt::null};
	entt::entity second {entt::null};
};

/// The hand of a computer player, on the player's entity. It is made the first time a script gives the hand something
/// to do. It moves through its steps one stage at a time, one stage each turn.
struct ComputerHand
{
	/// The speed in metres a second that every player's hand starts with
	static constexpr float k_StartingSpeed = 90.0f;

	/// Where the hand is
	glm::vec3 point {0.0f};
	/// Where the step under way takes the hand
	glm::vec3 target {0.0f};
	/// The speed of the steps under way, in metres a second
	float speed {k_StartingSpeed};
	/// The player's own speed, which a release gives the steps again
	float playerSpeed {k_StartingSpeed};
	/// The middle of the map cell (128, 128), the hand's place on the map grid before its first turn
	static constexpr int32_t k_StartingMapPlace = (128 << 16) | 0x8000;

	/// The hand's place on the map grid as of its last turn: x and z in map units
	int32_t mapX {k_StartingMapPlace};
	int32_t mapZ {k_StartingMapPlace};
	/// Where the hand was as its last turn of steps began: where the thing it holds is drawn from that turn on
	glm::vec3 turnStartPoint {0.0f};
	/// The turns of steps the hand has had, so that the draw sees each one
	uint32_t stepTurns {0};
	/// The steps, the one under way first
	std::deque<HandStep> steps;
	/// The stage of the first step: 0 its start, 1 while it goes on, 2 its end
	uint8_t stage {0};
	/// The first step has not yet been looked at for a move to put in front of it
	bool checkPrerequisite {true};
	/// A move was put in front of the first step that needed it
	bool prerequisiteInserted {false};
	/// A script holds the hand, so the player's mind does not choose for it
	bool held {false};
	/// A script gave the hand steps that it has not finished
	bool busy {false};
	/// What the hand holds
	entt::entity heldObject {entt::null};
	/// The actions queued by scripts, the next first
	std::deque<QueuedHandAction> queue;
};

} // namespace openblack::ecs::components
