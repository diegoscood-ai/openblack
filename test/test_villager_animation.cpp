/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/VillagerAnimation.h"

using namespace openblack;
using namespace openblack::ecs::villager_animation;

namespace
{
/// Hands out the given draws in turn, recording the limits asked for
struct Rolls
{
	std::vector<uint32_t> values;
	std::vector<uint32_t> limits;
	size_t next {0};
	IntRandom Random()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};

/// A walker at a speed in the game's whole speed units (655.36 to a metre a second), against the men's limits
WalkInputs Walker(uint16_t speed, bool female = false)
{
	return {.female = female, .speed = speed, .walkMax = 1966, .runMax = 2621};
}

struct StateClips
{
	ClipChoice clip;
	Transition transition;
};

/// Each state's clip choice and transition, by state, as the original's villager state table gives them
constexpr std::array<StateClips, 255> k_StateClips = {{
    {ClipChoice::Table, Transition::None},                  // 0 INVALID_STATE
    {ClipChoice::Walk, Transition::Walk},                   // 1 MOVE_TO_POS
    {ClipChoice::Walk, Transition::Walk},                   // 2 MOVE_TO_OBJECT
    {ClipChoice::Walk, Transition::Walk},                   // 3 MOVE_ON_STRUCTURE
    {ClipChoice::Table, Transition::None},                  // 4 IN_SCRIPT
    {ClipChoice::Dance, Transition::None},                  // 5 IN_DANCE
    {ClipChoice::Walk, Transition::Walk},                   // 6 FLEEING_FROM_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 7 LOOKING_AT_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 8 FOLLOWING_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 9 INSPECT_OBJECT_REACTION
    {ClipChoice::Thrown, Transition::None},                 // 10 FLYING
    {ClipChoice::Landed, Transition::None},                 // 11 LANDED
    {ClipChoice::LookAtFlyingObject, Transition::None},     // 12 LOOK_AT_FLYING_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 13 SET_DYING
    {ClipChoice::Dying, Transition::None},                  // 14 DYING
    {ClipChoice::Dead, Transition::None},                   // 15 DEAD
    {ClipChoice::Table, Transition::None},                  // 16 DROWNING
    {ClipChoice::Table, Transition::None},                  // 17 DOWNED
    {ClipChoice::Table, Transition::None},                  // 18 BEING_EATEN
    {ClipChoice::Table, Transition::None},                  // 19 GOTO_FOOD_REACTION
    {ClipChoice::Table, Transition::ArrivesAtResource},     // 20 ARRIVES_AT_FOOD_REACTION
    {ClipChoice::Table, Transition::None},                  // 21 GOTO_WOOD_REACTION
    {ClipChoice::Table, Transition::ArrivesAtResource},     // 22 ARRIVES_AT_WOOD_REACTION
    {ClipChoice::Table, Transition::None},                  // 23 WAIT_FOR_ANIMATION
    {ClipChoice::Table, Transition::None},                  // 24 IN_HAND
    {ClipChoice::Table, Transition::None},                  // 25 GOTO_PICKUP_BALL_REACTION
    {ClipChoice::Table, Transition::None},                  // 26 ARRIVES_AT_PICKUP_BALL_REACTION
    {ClipChoice::Table, Transition::None},                  // 27 MOVE_IN_FLOCK
    {ClipChoice::Walk, Transition::Walk},                   // 28 MOVE_ALONG_PATH
    {ClipChoice::Walk, Transition::Walk},                   // 29 MOVE_ON_PATH
    {ClipChoice::Table, Transition::None},                  // 30 FLEEING_AND_LOOKING_AT_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 31 GOTO_STORAGE_PIT_FOR_DROP_OFF
    {ClipChoice::Table, Transition::None},                  // 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF
    {ClipChoice::Table, Transition::None},                  // 33 GOTO_STORAGE_PIT_FOR_FOOD
    {ClipChoice::Table, Transition::None},                  // 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD
    {ClipChoice::Table, Transition::None},                  // 35 ARRIVES_AT_HOME_WITH_FOOD
    {ClipChoice::Table, Transition::None},                  // 36 GO_HOME
    {ClipChoice::Table, Transition::None},                  // 37 ARRIVES_HOME
    {ClipChoice::Table, Transition::None},                  // 38 AT_HOME
    {ClipChoice::Table, Transition::None},                  // 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS
    {ClipChoice::Table, Transition::None},                  // 40 ARRIVES_AT_BUILDING_SITE
    {ClipChoice::Building, Transition::Building},           // 41 BUILDING
    {ClipChoice::Table, Transition::None},                  // 42 GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {ClipChoice::Table, Transition::None},                  // 43 ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {ClipChoice::Table, Transition::None},                  // 44 GOTO_WORSHIP_SITE_WITH_SUPPLIES
    {ClipChoice::Walk, Transition::Walk},                   // 45 MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES
    {ClipChoice::Table, Transition::None},                  // 46 ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES
    {ClipChoice::Walk, Transition::Walk},                   // 47 FORESTER_MOVE_TO_FOREST
    {ClipChoice::Table, Transition::None},                  // 48 FORESTER_GOTO_FOREST
    {ClipChoice::Table, Transition::None},                  // 49 FORESTER_ARRIVES_AT_FOREST
    {ClipChoice::Table, Transition::None},                  // 50 FORESTER_CHOPS_TREE
    {ClipChoice::Forestering, Transition::None},            // 51 FORESTER_CHOPS_TREE_FOR_BUILDING
    {ClipChoice::Table, Transition::None},                  // 52 FORESTER_FINISHED_FORESTERING
    {ClipChoice::Table, Transition::None},                  // 53 ARRIVES_AT_BIG_FOREST
    {ClipChoice::Table, Transition::None},                  // 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING
    {ClipChoice::Table, Transition::None},                  // 55 FISHERMAN_ARRIVES_AT_FISHING
    {ClipChoice::Table, Transition::None},                  // 56 FISHING
    {ClipChoice::Table, Transition::None},                  // 57 WAIT_FOR_COUNTER
    {ClipChoice::Table, Transition::None},                  // 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    {ClipChoice::Table, Transition::None},                  // 59 ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP
    {ClipChoice::Dance, Transition::None},                  // 60 WORSHIPPING_AT_WORSHIP_SITE
    {ClipChoice::Table, Transition::None},                  // 61 GOTO_ALTAR_FOR_REST
    {ClipChoice::Table, Transition::None},                  // 62 ARRIVES_AT_ALTAR_FOR_REST
    {ClipChoice::Table, Transition::Pray},                  // 63 AT_ALTAR_REST
    {ClipChoice::Table, Transition::None},                  // 64 AT_ALTAR_FINISHED_REST
    {ClipChoice::Dance, Transition::None},                  // 65 RESTART_WORSHIPPING_AT_WORSHIP_SITE
    {ClipChoice::Dance, Transition::None},                  // 66 RESTART_WORSHIPPING_CREATURE
    {ClipChoice::Table, Transition::None},                  // 67 FARMER_ARRIVES_AT_FARM
    {ClipChoice::Table, Transition::None},                  // 68 FARMER_PLANTS_CROP
    {ClipChoice::Table, Transition::None},                  // 69 FARMER_DIGS_UP_CROP
    {ClipChoice::Table, Transition::None},                  // 70 MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION
    {ClipChoice::Walk, Transition::Walk},                   // 71 FOOTBALL_WALK_TO_POSITION
    {ClipChoice::FootballWaitForKickOff, Transition::None}, // 72 FOOTBALL_WAIT_FOR_KICK_OFF
    {ClipChoice::FootballOutfield, Transition::None},       // 73 FOOTBALL_ATTACKER
    {ClipChoice::FootballGoalKeeper, Transition::None},     // 74 FOOTBALL_GOALIE
    {ClipChoice::FootballOutfield, Transition::None},       // 75 FOOTBALL_DEFENDER
    {ClipChoice::Table, Transition::None},                  // 76 FOOTBALL_WON_GOAL
    {ClipChoice::Table, Transition::None},                  // 77 FOOTBALL_LOST_GOAL
    {ClipChoice::Walk, Transition::Walk},                   // 78 START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL
    {ClipChoice::Table, Transition::None},                  // 79 ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL
    {ClipChoice::Table, Transition::None},                  // 80 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START
    {ClipChoice::Table, Transition::None},                  // 81 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END
    {ClipChoice::FootballMatchPaused, Transition::None},    // 82 FOOTBALL_MATCH_PAUSED
    {ClipChoice::FootballWatchMatch, Transition::None},     // 83 FOOTBALL_WATCH_MATCH
    {ClipChoice::Table, Transition::None},                  // 84 FOOTBALL_MEXICAN_WAVE
    {ClipChoice::Table, Transition::None},                  // 85 CREATED
    {ClipChoice::Table, Transition::None},                  // 86 ARRIVES_IN_ABODE_TO_TRADE
    {ClipChoice::Table, Transition::None},                  // 87 ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS
    {ClipChoice::Table, Transition::None},                  // 88 MAKE_SCARED_STIFF
    {ClipChoice::Table, Transition::None},                  // 89 SCARED_STIFF
    {ClipChoice::Dance, Transition::None},                  // 90 WORSHIPPING_CREATURE
    {ClipChoice::Table, Transition::None},                  // 91 SHEPHERD_LOOK_FOR_FLOCK
    {ClipChoice::Walk, Transition::Walk},                   // 92 SHEPHERD_MOVE_FLOCK_TO_WATER
    {ClipChoice::Walk, Transition::Walk},                   // 93 SHEPHERD_MOVE_FLOCK_TO_FOOD
    {ClipChoice::Walk, Transition::Walk},                   // 94 SHEPHERD_MOVE_FLOCK_BACK
    {ClipChoice::Table, Transition::None},                  // 95 SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK
    {ClipChoice::Table, Transition::None},                  // 96 SHEPHERD_WAIT_FOR_FLOCK
    {ClipChoice::Table, Transition::None},                  // 97 SHEPHERD_SLAUGHTER_ANIMAL
    {ClipChoice::Table, Transition::None},                  // 98 SHEPHERD_FETCH_STRAY
    {ClipChoice::Table, Transition::None},                  // 99 SHEPHERD_GOTO_FLOCK
    {ClipChoice::Table, Transition::None},                  // 100 HOUSEWIFE_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 101 HOUSEWIFE_GOTO_STORAGE_PIT
    {ClipChoice::Table, Transition::None},                  // 102 HOUSEWIFE_ARRIVES_AT_STORAGE_PIT
    {ClipChoice::Table, Transition::None},                  // 103 HOUSEWIFE_PICKUP_FROM_STORAGE_PIT
    {ClipChoice::Table, Transition::None},                  // 104 HOUSEWIFE_RETURN_HOME_WITH_FOOD
    {ClipChoice::Table, Transition::None},                  // 105 HOUSEWIFE_MAKE_DINNER
    {ClipChoice::Table, Transition::None},                  // 106 HOUSEWIFE_SERVES_DINNER
    {ClipChoice::Table, Transition::None},                  // 107 HOUSEWIFE_CLEARS_AWAY_DINNER
    {ClipChoice::Table, Transition::None},                  // 108 HOUSEWIFE_DOES_HOUSEWORK
    {ClipChoice::Table, Transition::None},                  // 109 HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT
    {ClipChoice::Table, Transition::None},                  // 110 HOUSEWIFE_STARTS_GIVING_BIRTH
    {ClipChoice::Table, Transition::None},                  // 111 HOUSEWIFE_GIVING_BIRTH
    {ClipChoice::Table, Transition::None},                  // 112 HOUSEWIFE_GIVEN_BIRTH
    {ClipChoice::Table, Transition::None},                  // 113 CHILD_AT_CRECHE
    {ClipChoice::Table, Transition::None},                  // 114 CHILD_FOLLOWS_MOTHER
    {ClipChoice::Table, Transition::None},                  // 115 CHILD_BECOMES_ADULT
    {ClipChoice::Table, Transition::None},                  // 116 SITS_DOWN_TO_DINNER
    {ClipChoice::Table, Transition::None},                  // 117 EAT_FOOD
    {ClipChoice::Table, Transition::None},                  // 118 EAT_FOOD_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 119 GOTO_BED_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 120 SLEEPING_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 121 WAKE_UP_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 122 START_HAVING_SEX
    {ClipChoice::Kissing, Transition::None},                // 123 HAVING_SEX
    {ClipChoice::Table, Transition::None},                  // 124 STOP_HAVING_SEX
    {ClipChoice::Table, Transition::None},                  // 125 START_HAVING_SEX_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 126 HAVING_SEX_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 127 STOP_HAVING_SEX_AT_HOME
    {ClipChoice::Table, Transition::None},                  // 128 WAIT_FOR_DINNER
    {ClipChoice::Table, Transition::None},                  // 129 HOMELESS_START
    {ClipChoice::Table, Transition::None},                  // 130 VAGRANT_START
    {ClipChoice::Table, Transition::Mourn},                 // 131 MORN_DEATH
    {ClipChoice::Table, Transition::None},                  // 132 PERFORM_INSPECTION_REACTION
    {ClipChoice::Walk, Transition::Walk},                   // 133 APPROACH_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 134 INITIALISE_TELL_OTHERS_ABOUT_OBJECT
    {ClipChoice::Table, Transition::None},                  // 135 TELL_OTHERS_ABOUT_INTERESTING_OBJECT
    {ClipChoice::Walk, Transition::Walk},                   // 136 APPROACH_VILLAGER_TO_TALK_TO
    {ClipChoice::Table, Transition::None},                  // 137 TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT
    {ClipChoice::Table, Transition::None},                  // 138 INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {ClipChoice::Table, Transition::None},                  // 139 LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {ClipChoice::Walk, Transition::Walk},                   // 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT
    {ClipChoice::Table, Transition::None},                  // 141 INITIALISE_IMPRESSED_REACTION
    {ClipChoice::Table, Transition::Pray},                  // 142 PERFORM_IMPRESSED_REACTION
    {ClipChoice::Table, Transition::None},                  // 143 INITIALISE_FIGHT_REACTION
    {ClipChoice::WatchFight, Transition::None},             // 144 PERFORM_FIGHT_REACTION
    {ClipChoice::Table, Transition::None},                  // 145 HOMELESS_EAT_DINNER
    {ClipChoice::InspectCreature, Transition::None},        // 146 INSPECT_CREATURE_REACTION
    {ClipChoice::InspectCreature, Transition::None},        // 147 PERFORM_INSPECT_CREATURE_REACTION
    {ClipChoice::InspectCreature, Transition::None},        // 148 APPROACH_CREATURE_REACTION
    {ClipChoice::Table, Transition::None},                  // 149 INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION
    {ClipChoice::Table, Transition::None},                  // 150 PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION
    {ClipChoice::Table, Transition::None},                  // 151 TURN_TO_FACE_MAGIC_TREE
    {ClipChoice::LookAtLargeObject, Transition::None},      // 152 LOOK_AT_MAGIC_TREE
    {ClipChoice::Dance, Transition::None},                  // 153 DANCE_FOR_EDITING_PURPOSES
    {ClipChoice::Walk, Transition::Walk},                   // 154 MOVE_TO_DANCE_POS
    {ClipChoice::RespectCreature, Transition::None},        // 155 INITIALISE_RESPECT_CREATURE_REACTION
    {ClipChoice::RespectCreature, Transition::None},        // 156 PERFORM_RESPECT_CREATURE_REACTION
    {ClipChoice::RespectCreature, Transition::None},        // 157 FINISH_RESPECT_CREATURE_REACTION
    {ClipChoice::Table, Transition::None},                  // 158 APPROACH_HAND_REACTION
    {ClipChoice::Table, Transition::None},                  // 159 FLEEING_FROM_CREATURE_REACTION
    {ClipChoice::InspectCreature, Transition::None},        // 160 TURN_TO_FACE_CREATURE_REACTION
    {ClipChoice::LookAtFlyingObject, Transition::None},     // 161 WATCH_FLYING_OBJECT_REACTION
    {ClipChoice::PointAtFlyingObject, Transition::None},    // 162 POINT_AT_FLYING_OBJECT_REACTION
    {ClipChoice::Table, Transition::None},                  // 163 DECIDE_WHAT_TO_DO
    {ClipChoice::Table, Transition::None},                  // 164 INTERACT_DECIDE_WHAT_TO_DO
    {ClipChoice::Table, Transition::None},                  // 165 EAT_OUTSIDE
    {ClipChoice::Table, Transition::None},                  // 166 RUN_AWAY_FROM_OBJECT_REACTION
    {ClipChoice::Walk, Transition::Walk},                   // 167 MOVE_TOWARDS_CREATURE_REACTION
    {ClipChoice::AmazedByShield, Transition::None},         // 168 AMAZED_BY_MAGIC_SHIELD_REACTION
    {ClipChoice::Table, Transition::None},                  // 169 VILLAGER_GOSSIPS
    {ClipChoice::Table, Transition::None},                  // 170 CHECK_INTERACT_WITH_ANIMAL
    {ClipChoice::Table, Transition::None},                  // 171 CHECK_INTERACT_WITH_WORSHIP_SITE
    {ClipChoice::Table, Transition::None},                  // 172 CHECK_INTERACT_WITH_ABODE
    {ClipChoice::Table, Transition::None},                  // 173 CHECK_INTERACT_WITH_FIELD
    {ClipChoice::Table, Transition::None},                  // 174 CHECK_INTERACT_WITH_FISH_FARM
    {ClipChoice::Table, Transition::None},                  // 175 CHECK_INTERACT_WITH_TREE
    {ClipChoice::Table, Transition::None},                  // 176 CHECK_INTERACT_WITH_BALL
    {ClipChoice::Table, Transition::None},                  // 177 CHECK_INTERACT_WITH_POT
    {ClipChoice::Table, Transition::None},                  // 178 CHECK_INTERACT_WITH_FOOTBALL
    {ClipChoice::Table, Transition::None},                  // 179 CHECK_INTERACT_WITH_VILLAGER
    {ClipChoice::Table, Transition::None},                  // 180 CHECK_INTERACT_WITH_MAGIC_LIVING
    {ClipChoice::Table, Transition::None},                  // 181 CHECK_INTERACT_WITH_ROCK
    {ClipChoice::Forestering, Transition::None},            // 182 ARRIVES_AT_ROCK_FOR_WOOD
    {ClipChoice::Forestering, Transition::None},            // 183 GOT_WOOD_FROM_ROCK
    {ClipChoice::Table, Transition::None},                  // 184 REENTER_BUILDING_STATE
    {ClipChoice::Table, Transition::None},                  // 185 ARRIVE_AT_PUSH_OBJECT
    {ClipChoice::Forestering, Transition::None},            // 186 TAKE_WOOD_FROM_TREE
    {ClipChoice::Forestering, Transition::None},            // 187 TAKE_WOOD_FROM_POT
    {ClipChoice::Forestering, Transition::None},            // 188 TAKE_WOOD_FROM_TREE_FOR_BUILDING
    {ClipChoice::Forestering, Transition::None},            // 189 TAKE_WOOD_FROM_POT_FOR_BUILDING
    {ClipChoice::Table, Transition::None},                  // 190 SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER
    {ClipChoice::Table, Transition::None},                  // 191 SHEPHERD_TAKES_CONTROL_OF_FLOCK
    {ClipChoice::Table, Transition::None},                  // 192 SHEPHERD_RELEASES_CONTROL_OF_FLOCK
    {ClipChoice::Dance, Transition::None},                  // 193 DANCE_BUT_NOT_WORSHIP
    {ClipChoice::Table, Transition::None},                  // 194 FAINTING_REACTION
    {ClipChoice::Table, Transition::None},                  // 195 START_CONFUSED_REACTION
    {ClipChoice::Table, Transition::None},                  // 196 CONFUSED_REACTION
    {ClipChoice::Yawn, Transition::None},                   // 197 AFTER_TAP_ON_ABODE
    {ClipChoice::Table, Transition::None},                  // 198 WEAK_ON_GROUND
    {ClipChoice::Table, Transition::None},                  // 199 SCRIPT_WANDER_AROUND_POSITION
    {ClipChoice::Script, Transition::None},                 // 200 SCRIPT_PLAY_ANIM
    {ClipChoice::Table, Transition::None},                  // 201 GO_TOWARDS_TELEPORT_REACTION
    {ClipChoice::Table, Transition::None},                  // 202 TELEPORT_REACTION
    {ClipChoice::Dance, Transition::None},                  // 203 DANCE_WHILE_REACTING
    {ClipChoice::ControlledByCreature, Transition::None},   // 204 CONTROLLED_BY_CREATURE
    {ClipChoice::Table, Transition::None},                  // 205 POINT_AT_DEAD_PERSON
    {ClipChoice::Table, Transition::None},                  // 206 GO_TOWARDS_DEAD_PERSON
    {ClipChoice::Table, Transition::None},                  // 207 LOOK_AT_DEAD_PERSON
    {ClipChoice::Table, Transition::Mourn},                 // 208 MOURN_DEAD_PERSON
    {ClipChoice::Table, Transition::None},                  // 209 NOTHING_TO_DO
    {ClipChoice::Table, Transition::None},                  // 210 ARRIVES_AT_WORKSHOP_FOR_DROP_OFF
    {ClipChoice::Table, Transition::None},                  // 211 ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS
    {ClipChoice::Table, Transition::None},                  // 212 SHOW_POISONED
    {ClipChoice::Table, Transition::None},                  // 213 HIDING_AT_WORSHIP_SITE
    {ClipChoice::Table, Transition::None},                  // 214 CROWD_REACTION
    {ClipChoice::Table, Transition::None},                  // 215 REACT_TO_FIRE
    {ClipChoice::Table, Transition::None},                  // 216 PUT_OUT_FIRE_BY_BEATING
    {ClipChoice::Table, Transition::None},                  // 217 PUT_OUT_FIRE_WITH_WATER
    {ClipChoice::Table, Transition::None},                  // 218 GET_WATER_TO_PUT_OUT_FIRE
    {ClipChoice::Table, Transition::None},                  // 219 ON_FIRE
    {ClipChoice::Table, Transition::None},                  // 220 MOVE_AROUND_FIRE
    {ClipChoice::Table, Transition::Pray},                  // 221 DISCIPLE_NOTHING_TO_DO
    {ClipChoice::Walk, Transition::Walk},                   // 222 FOOTBALL_MOVE_TO_BALL
    {ClipChoice::Table, Transition::None},                  // 223 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP
    {ClipChoice::Table, Transition::None},                  // 224 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF
    {ClipChoice::Table, Transition::None},                  // 225 BREEDER_DISCIPLE
    {ClipChoice::Table, Transition::None},                  // 226 MISSIONARY_DISCIPLE
    {ClipChoice::Table, Transition::None},                  // 227 REACT_TO_BREEDER
    {ClipChoice::Table, Transition::None},                  // 228 SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER
    {ClipChoice::Table, Transition::None},                  // 229 INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER
    {ClipChoice::Dance, Transition::None},                  // 230 ARTIFACT_DANCE
    {ClipChoice::Walk, Transition::Walk},                   // 231 FLEEING_FROM_PREDATOR_REACTION
    {ClipChoice::Table, Transition::None},                  // 232 WAIT_FOR_WOOD
    {ClipChoice::Table, Transition::None},                  // 233 INSPECT_OBJECT
    {ClipChoice::Table, Transition::None},                  // 234 GO_HOME_AND_CHANGE
    {ClipChoice::Table, Transition::None},                  // 235 WAIT_FOR_MATE
    {ClipChoice::Table, Transition::None},                  // 236 GO_AND_HIDE_IN_NEARBY_BUILDING
    {ClipChoice::Table, Transition::None},                  // 237 LOOK_TO_SEE_IF_IT_IS_SAFE
    {ClipChoice::Table, Transition::SleepOnTheGround},      // 238 SLEEP_IN_TENT
    {ClipChoice::PauseForASecond, Transition::None},        // 239 PAUSE_FOR_A_SECOND
    {ClipChoice::Table, Transition::None},                  // 240 PANIC_REACTION
    {ClipChoice::Table, Transition::None},                  // 241 GET_FOOD_AT_WORSHIP_SITE
    {ClipChoice::Table, Transition::None},                  // 242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {ClipChoice::TownEmergency, Transition::None},          // 243 CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {ClipChoice::RandomCrowd, Transition::None},            // 244 SCRIPT_IN_CROWD
    {ClipChoice::Table, Transition::None},                  // 245 GO_AND_CHILLOUT_OUTSIDE_HOME
    {ClipChoice::SitDown, Transition::SitDown},             // 246 SIT_AND_CHILLOUT
    {ClipChoice::Table, Transition::None},                  // 247 SCRIPT_GO_AND_MOVE_ALONG_PATH
    {ClipChoice::Table, Transition::None},                  // 248 GO_HOME_FROM_WORSHIP
    {ClipChoice::Table, Transition::None},                  // 249 ARRIVES_HOME_FROM_WORSHIP
    {ClipChoice::Table, Transition::SleepOnTheGround},      // 250 SLEEP_IN_TENT_FROM_WORSHIP
    {ClipChoice::Table, Transition::None},                  // 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY
    {ClipChoice::Table, Transition::None},                  // 252 GO_AND_CHILLOUT_IN_TOWN
    {ClipChoice::Dance, Transition::None},                  // 253 WAIT_FOR_ARTIFACT_DANCE
    {ClipChoice::Table, Transition::None},                  // 254 BREEDER_JUST_LANDED
}};
} // namespace

TEST(VillagerAnimation, EveryStateChoosesItsClipsAsTheStateTableDoes)
{
	for (size_t i = 0; i < k_StateClips.size(); ++i)
	{
		const auto state = static_cast<VillagerStates>(i);
		EXPECT_EQ(ClipChoiceOf(state), k_StateClips.at(i).clip) << i;
		EXPECT_EQ(TransitionOf(state), k_StateClips.at(i).transition) << i;
	}
}

TEST(VillagerAnimation, StatesChooseTheirClipsAsTheGameDoes)
{
	EXPECT_EQ(ClipChoiceOf(VillagerStates::MoveToPos), ClipChoice::Walk);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::FleeingFromPredatorReaction), ClipChoice::Walk);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::AfterTapOnAbode), ClipChoice::Yawn);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::SitAndChillout), ClipChoice::SitDown);
	// Going home is walking only through the moving states it sets up
	EXPECT_EQ(ClipChoiceOf(VillagerStates::GoHome), ClipChoice::Table);
	EXPECT_EQ(TransitionOf(VillagerStates::SleepInTent), Transition::SleepOnTheGround);
	EXPECT_EQ(TransitionOf(VillagerStates::MoveOnPath), Transition::Walk);
	EXPECT_EQ(TransitionOf(VillagerStates::AtHome), Transition::None);
	EXPECT_TRUE(IsDeathState(VillagerStates::SetDying));
	EXPECT_TRUE(IsDeathState(VillagerStates::Drowning));
	// 13 to 17: the downed state is one of them, being eaten is not
	EXPECT_TRUE(IsDeathState(VillagerStates::Downed));
	EXPECT_EQ(static_cast<uint32_t>(VillagerStates::SetDying), 13U);
	EXPECT_EQ(static_cast<uint32_t>(VillagerStates::Downed), 17U);
	EXPECT_FALSE(IsDeathState(VillagerStates::BeingEaten));
	EXPECT_FALSE(IsDeathState(VillagerStates::LookAtFlyingObjectReaction));
}

// The hand lets a held villager go into 10, 11 and 13 to 17 only, as the original's exit of the state in the hand
TEST(VillagerAnimation, TheHandLetsAVillagerGoOnlyIntoAFlightALandingOrADeath)
{
	for (uint32_t state = 0; state < 256; ++state)
	{
		const bool expected = state == 10 || state == 11 || (state >= 13 && state <= 17);
		EXPECT_EQ(HandLetsGoInto(static_cast<VillagerStates>(state)), expected) << state;
	}
	EXPECT_TRUE(HandLetsGoInto(VillagerStates::Flying));
	EXPECT_TRUE(HandLetsGoInto(VillagerStates::Landed));
	EXPECT_TRUE(HandLetsGoInto(VillagerStates::Drowning));
	EXPECT_FALSE(HandLetsGoInto(VillagerStates::InHand));
	EXPECT_FALSE(HandLetsGoInto(VillagerStates::BeingEaten));
	EXPECT_FALSE(HandLetsGoInto(VillagerStates::DecideWhatToDo));
}

TEST(VillagerAnimation, VillagersWalkRunOrSprintBySpeedAndSex)
{
	EXPECT_EQ(WalkClip(Walker(1311)), AnimId::PWalkMan);
	EXPECT_EQ(WalkClip(Walker(1967)), AnimId::PRunMan);
	EXPECT_EQ(WalkClip(Walker(2622)), AnimId::PSprintRunMan);
	EXPECT_EQ(WalkClip(Walker(1311, true)), AnimId::PWalkWoman);
	EXPECT_EQ(WalkClip(Walker(2300, true)), AnimId::PRunWoman);
	EXPECT_EQ(WalkClip(Walker(3277, true)), AnimId::PSprintRunWoman);
}

TEST(VillagerAnimation, EachGaitGoesUpToItsLimitIncluded)
{
	// The men's limits: 1966 (3 m/s) walking, 2621 (4 m/s) running
	EXPECT_EQ(GaitOf(0, 1966, 2621), Gait::Walk);
	EXPECT_EQ(GaitOf(1965, 1966, 2621), Gait::Walk);
	EXPECT_EQ(GaitOf(1966, 1966, 2621), Gait::Walk);
	EXPECT_EQ(GaitOf(1967, 1966, 2621), Gait::Run);
	EXPECT_EQ(GaitOf(2620, 1966, 2621), Gait::Run);
	EXPECT_EQ(GaitOf(2621, 1966, 2621), Gait::Run);
	EXPECT_EQ(GaitOf(2622, 1966, 2621), Gait::Sprint);
	EXPECT_EQ(GaitOf(0xFFFF, 1966, 2621), Gait::Sprint);
	// The women's: 1311 (2 m/s) walking, 1966 running
	EXPECT_EQ(GaitOf(1311, 1311, 1966), Gait::Walk);
	EXPECT_EQ(GaitOf(1312, 1311, 1966), Gait::Run);
	EXPECT_EQ(GaitOf(1966, 1311, 1966), Gait::Run);
	EXPECT_EQ(GaitOf(1967, 1311, 1966), Gait::Sprint);
	// The limits are signed: a negative one is below every speed, even a standing one
	EXPECT_EQ(GaitOf(0, -1, 2621), Gait::Run);
	EXPECT_EQ(GaitOf(0, -1, -1), Gait::Sprint);
	// A script's 0.3 and 0.4 m a turn are 1966 and 2621: a man walks and runs at them
	EXPECT_EQ(WalkClip(Walker(1966)), AnimId::PWalkMan);
	EXPECT_EQ(WalkClip(Walker(2621)), AnimId::PRunMan);
	auto woman = Walker(1966, true);
	woman.walkMax = 1311;
	woman.runMax = 1966;
	EXPECT_EQ(WalkClip(woman), AnimId::PRunWoman);
	woman.speed = 1311;
	EXPECT_EQ(WalkClip(woman), AnimId::PWalkWoman);
	woman.carried = CarriedObject::Wood;
	EXPECT_EQ(WalkClip(woman), AnimId::PCarryAxe);
	woman.speed = 1312;
	EXPECT_EQ(WalkClip(woman), AnimId::PCarryObjectRun);
}

TEST(VillagerAnimation, OnlyTheMensSexWalksByTheMensLimits)
{
	EXPECT_EQ(SpeedLimitsIndex(SexType::Male), 1u);
	EXPECT_EQ(SpeedLimitsIndex(SexType::Female), 0u);
	// An info of no sex walks by the women's limits, as the game tests for the men's sex alone
	EXPECT_EQ(SpeedLimitsIndex(SexType::None), 0u);
	static_assert(static_cast<int>(SexType::Male) == 0);
}

TEST(VillagerAnimation, TheHurtCrawlOrLimp)
{
	auto in = Walker(1311);
	in.life = 0.1f;
	EXPECT_EQ(WalkClip(in), AnimId::PCrawlInjured);
	EXPECT_EQ(HurtWalkClip(false, in.life, in.lifeWhenCrawlsWounded, in.lifeWhenWalksWounded), AnimId::PCrawlInjured);
	in.life = 0.3f;
	EXPECT_EQ(WalkClip(in), AnimId::PWalkInjured);
	// The gait alone does not look at the life
	EXPECT_EQ(GaitClip(in), AnimId::PWalkMan);
	in.life = 0.5f;
	EXPECT_EQ(HurtWalkClip(false, in.life, in.lifeWhenCrawlsWounded, in.lifeWhenWalksWounded), std::nullopt);
}

TEST(VillagerAnimation, AVillagerAScriptControlsIsNotLookedAtForItsWounds)
{
	// The wounds are skipped for it, whatever its life: it walks, runs or sprints by its speed alone
	auto in = Walker(1311);
	in.controlledByScript = true;
	in.life = 0.1f;
	EXPECT_EQ(HurtWalkClip(true, in.life, in.lifeWhenCrawlsWounded, in.lifeWhenWalksWounded), std::nullopt);
	EXPECT_EQ(WalkClip(in), AnimId::PWalkMan);
	in.life = 0.3f;
	EXPECT_EQ(WalkClip(in), AnimId::PWalkMan);
	in.life = 0.0f;
	in.speed = Walker(2622).speed;
	EXPECT_EQ(WalkClip(in), AnimId::PSprintRunMan);
	// Released, the same villager crawls again
	in.controlledByScript = false;
	EXPECT_EQ(WalkClip(in), AnimId::PCrawlInjured);
}

TEST(VillagerAnimation, ALoadInTheArmsIsCarriedWalkingOrRunning)
{
	auto in = Walker(1311, true);
	in.carried = CarriedObject::Wood;
	EXPECT_EQ(WalkClip(in), AnimId::PCarryAxe);
	in.speed = Walker(3277).speed;
	EXPECT_EQ(WalkClip(in), AnimId::PCarryObjectRun);
	// A saw, the ball and a hammer don't change the walk
	in.carried = CarriedObject::Hammer;
	EXPECT_EQ(WalkClip(in), AnimId::PSprintRunWoman);
	EXPECT_FALSE(CarriesInArms(CarriedObject::Saw));
	EXPECT_FALSE(CarriesInArms(CarriedObject::NoChange));
	EXPECT_TRUE(CarriesInArms(CarriedObject::Crook));
}

TEST(VillagerAnimation, RandomClipsDrawOnceOnTheGamesStream)
{
	Rolls yawn {.values = {1, 0}};
	EXPECT_EQ(YawnClip(yawn.Random()), AnimId::PYawn2);
	EXPECT_EQ(YawnClip(yawn.Random()), AnimId::PYawn);
	EXPECT_EQ(yawn.limits, (std::vector<uint32_t> {2, 2}));

	Rolls pause {.values = {1}};
	EXPECT_EQ(PauseForASecondClip(false, pause.Random()), AnimId::POverworked2);
	EXPECT_EQ(pause.limits, (std::vector<uint32_t> {2}));

	Rolls emergency {.values = {3, 3, 0}};
	EXPECT_EQ(TownEmergencyClip(false, emergency.Random()), AnimId::PPanicMan);
	EXPECT_EQ(TownEmergencyClip(true, emergency.Random()), AnimId::PPanicWoman);
	EXPECT_EQ(TownEmergencyClip(true, emergency.Random()), AnimId::PAttractYourAttention);
}

TEST(VillagerAnimation, LookingAtALargeObjectGoesByTheTurnsLeft)
{
	// A look of 25 turns: the high byte holds 25, the low byte the turns left
	constexpr auto k_Look = [](uint16_t start, uint16_t left) { return static_cast<uint16_t>((start << 8U) | left); };
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(25, 25)), AnimId::PLookAtHand);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(25, 13)), AnimId::PLookAtHand);
	// 25 >> 1 is 12: from 12 turns left on, it looks about
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(25, 12)), AnimId::PLookingForSomething);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(25, 1)), AnimId::PLookingForSomething);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(20, 11)), AnimId::PLookAtHand);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(20, 10)), AnimId::PLookingForSomething);
	// The bytes compare unsigned
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(0, 255)), AnimId::PLookAtHand);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(255, 128)), AnimId::PLookAtHand);
	EXPECT_EQ(LookAtLargeObjectClip(k_Look(255, 127)), AnimId::PLookingForSomething);
	// A counter of 0 looks about
	EXPECT_EQ(LookAtLargeObjectClip(0), AnimId::PLookingForSomething);
	EXPECT_EQ(static_cast<int>(AnimId::PLookAtHand), 311);
	EXPECT_EQ(static_cast<int>(AnimId::PLookingForSomething), 310);
}

TEST(VillagerAnimation, WatchingAFlyingThingLooksUpOnlyWhileItIsHigh)
{
	constexpr auto k_Stand = static_cast<int32_t>(AnimId::PStand);
	constexpr auto k_LookAtHand = static_cast<int32_t>(AnimId::PLookAtHand);
	// More than 3 m above the land under it: it looks up
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, 3.5f), k_LookAtHand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, std::nextafter(3.0f, 4.0f)), k_LookAtHand);
	// Exactly 3 m is not more: it stands, as it does lower down, below the land or with nothing to watch
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, 3.0f), k_Stand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, 0.5f), k_Stand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, -10.0f), k_Stand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, std::nullopt), k_Stand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, std::numeric_limits<float>::quiet_NaN()), k_Stand);
	// The height is rounded to a float before the compare: just above 3 m in double, but exactly 3.0f, stands
	const float altitude = std::nextafter(3.0f, 4.0f);
	const float land = 1.5e-7f;
	ASSERT_GT(static_cast<double>(altitude) - static_cast<double>(land), 3.0);
	EXPECT_EQ(HeightAboveLand(altitude, land), 3.0f);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, HeightAboveLand(altitude, land)), k_Stand);
	EXPECT_EQ(LookAtFlyingObjectClip(k_Stand, HeightAboveLand(altitude, 0.0f)), k_LookAtHand);
	// Coming from a state that hides the villager, it stays hidden whatever the height
	EXPECT_EQ(LookAtFlyingObjectClip(k_HiddenClip, 50.0f), k_HiddenClip);
	EXPECT_EQ(LookAtFlyingObjectClip(k_HiddenClip, std::nullopt), k_HiddenClip);
	// Any other clip of the state it came from does not matter
	EXPECT_EQ(LookAtFlyingObjectClip(static_cast<int32_t>(AnimId::PHammering), 4.0f), k_LookAtHand);
	EXPECT_EQ(LookAtFlyingObjectClip(-1, 1.0f), k_Stand);
	EXPECT_EQ(k_Stand, 385);
	EXPECT_EQ(k_LookAtHand, 311);
}

TEST(VillagerAnimation, APoisonedVillagerPausesLookingPoisonedWithoutADraw)
{
	Rolls poisoned {.values = {}};
	EXPECT_EQ(PauseForASecondClip(true, poisoned.Random()), AnimId::PPoisoned);
	EXPECT_TRUE(poisoned.limits.empty());
	// The rest draw once: 1 is the second worn-out clip, anything else the first
	Rolls worn {.values = {0, 1}};
	EXPECT_EQ(PauseForASecondClip(false, worn.Random()), AnimId::POverworked1);
	EXPECT_EQ(PauseForASecondClip(false, worn.Random()), AnimId::POverworked2);
	EXPECT_EQ(worn.limits, (std::vector<uint32_t> {2, 2}));
	EXPECT_EQ(static_cast<int>(AnimId::PPoisoned), 342);
	EXPECT_EQ(static_cast<int>(AnimId::POverworked1), 332);
	EXPECT_EQ(static_cast<int>(AnimId::POverworked2), 333);
}

TEST(VillagerAnimation, InspectingTheCreatureDrawsOnlyWhatItNeeds)
{
	// A man only decides between talking and standing
	Rolls man {.values = {3}};
	EXPECT_EQ(InspectCreatureClip(false, man.Random()), AnimId::PTalkingAndPointing);
	EXPECT_EQ(man.limits, (std::vector<uint32_t> {8}));
	// A woman is first scared stiff one time in three
	Rolls scared {.values = {0}};
	EXPECT_EQ(InspectCreatureClip(true, scared.Random()), AnimId::PScaredStiff);
	EXPECT_EQ(scared.limits, (std::vector<uint32_t> {3}));
	Rolls woman {.values = {1, 2}};
	EXPECT_EQ(InspectCreatureClip(true, woman.Random()), AnimId::PStand);
	EXPECT_EQ(woman.limits, (std::vector<uint32_t> {3, 8}));
}

TEST(VillagerAnimation, SittingDownMatchesTheWayItSat)
{
	Rolls none;
	EXPECT_EQ(SitDownClip(true, AnimId::PSittingDown1Into, none.Random()), AnimId::PSittingDown1Sitting);
	EXPECT_EQ(SitDownClip(true, AnimId::PSittingDown2Into, none.Random()), AnimId::PSittingDown2Sitting);
	Rolls one {.values = {1}};
	EXPECT_EQ(SitDownClip(false, AnimId::PStand, one.Random()), AnimId::PSittingDown2Sitting);
	EXPECT_EQ(TransitionClip(Transition::SitDown, true, false, false, AnimId::PSittingDown1Sitting), AnimId::PSittingDown1Into);
	EXPECT_EQ(TransitionClip(Transition::SitDown, false, false, false, AnimId::PSittingDown2Sitting),
	          AnimId::PSittingDown2OutOf);
}

TEST(VillagerAnimation, BuildersCarryTheToolOfTheirStroke)
{
	Rolls saw {.values = {1}};
	const auto choice = BuildingClip(true, AnimId::PStand, saw.Random());
	EXPECT_EQ(choice.clip, AnimId::PSawWood);
	EXPECT_EQ(choice.tool, CarriedObject::Saw);
	// While its into clip plays the working clip follows, without a draw
	Rolls none;
	const auto swing = BuildingClip(true, AnimId::PIntoSledgehammer, none.Random());
	EXPECT_EQ(swing.clip, AnimId::PSledgehammer);
	EXPECT_EQ(swing.tool, CarriedObject::MalletHeavy);
	EXPECT_TRUE(none.limits.empty());
	const auto hammer = BuildingClip(true, AnimId::PIntoHammering, none.Random());
	EXPECT_EQ(hammer.clip, AnimId::PHammering);
	EXPECT_EQ(hammer.tool, CarriedObject::Hammer);
	EXPECT_EQ(BuildingClip(true, AnimId::PIntoSawWood, none.Random()).clip, AnimId::PSawWood);
	EXPECT_TRUE(none.limits.empty());
	// With no transition playing even an into clip draws one of the three: 0 hammer, 1 saw, 2 mallet
	Rolls flagClear {.values = {0, 2}};
	const auto rolled = BuildingClip(false, AnimId::PIntoSledgehammer, flagClear.Random());
	EXPECT_EQ(rolled.clip, AnimId::PHammering);
	EXPECT_EQ(rolled.tool, CarriedObject::Hammer);
	EXPECT_EQ(BuildingClip(false, AnimId::PIntoSawWood, flagClear.Random()).clip, AnimId::PSledgehammer);
	EXPECT_EQ(flagClear.limits, (std::vector<uint32_t> {3, 3}));
	// A working clip is not an into clip: it draws too
	Rolls working {.values = {1}};
	EXPECT_EQ(BuildingClip(true, AnimId::PSledgehammer, working.Random()).clip, AnimId::PSawWood);
	EXPECT_EQ(working.limits, (std::vector<uint32_t> {3}));
	EXPECT_EQ(static_cast<int>(AnimId::PHammering), 276);
	EXPECT_EQ(static_cast<int>(AnimId::PIntoHammering), 284);
	EXPECT_EQ(static_cast<int>(AnimId::PIntoSawWood), 292);
	EXPECT_EQ(static_cast<int>(AnimId::PIntoSledgehammer), 293);
	EXPECT_EQ(static_cast<int>(AnimId::PSawWood), 354);
	EXPECT_EQ(static_cast<int>(AnimId::PSledgehammer), 380);
	EXPECT_EQ(TransitionClip(Transition::Building, false, false, false, AnimId::PHammering), AnimId::POutOfHammering);
	EXPECT_EQ(TransitionClip(Transition::Building, true, false, false, AnimId::PStand), std::nullopt);
}

TEST(VillagerAnimation, OnlyACrawlerGetsUpOrGoesDownIntoACrawl)
{
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, false, AnimId::PWalkMan), std::nullopt);
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, false, AnimId::PCrawlInjured), AnimId::POutOfSleep);
	EXPECT_EQ(TransitionClip(Transition::Walk, true, false, false, AnimId::PCrawlInjured), AnimId::PCrawlInjuredInto);
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, true, AnimId::PCrawlInjured), std::nullopt);
	// A crawler whose walk ends in the downed state does not get up; one whose walk ends in being eaten does
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, IsDeathState(VillagerStates::Downed), AnimId::PCrawlInjured),
	          std::nullopt);
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, IsDeathState(VillagerStates::BeingEaten), AnimId::PCrawlInjured),
	          AnimId::POutOfSleep);
	// Picking up sticks only on the way out, and only to another state
	EXPECT_EQ(TransitionClip(Transition::ArrivesAtResource, false, false, false, AnimId::PStand), AnimId::PPickUpSticks);
	EXPECT_EQ(TransitionClip(Transition::ArrivesAtResource, false, true, false, AnimId::PStand), std::nullopt);
	EXPECT_EQ(TransitionClip(Transition::SleepOnTheGround, true, false, false, AnimId::PStand), AnimId::PIntoSleep);
}

TEST(VillagerAnimation, LookingUpAtFlyingThings)
{
	EXPECT_EQ(LookAtFlyingObjectClip(k_HiddenClip, 10.0f), k_HiddenClip);
	EXPECT_EQ(LookAtFlyingObjectClip(385, 10.0f), static_cast<int32_t>(AnimId::PLookAtHand));
	EXPECT_EQ(LookAtFlyingObjectClip(385, 2.0f), static_cast<int32_t>(AnimId::PStand));
}
