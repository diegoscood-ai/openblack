# Collisions

What flying objects hit and how: the land, other loose objects, buildings, trees, people and the creature, and the edges
of the map. Contacts are springs with friction at every point of a body (see [object_dynamics.md](object_dynamics.md)).
What the hits do to life and buildings is in [impact_damage.md](impact_damage.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack tests the game's own way: each body's points against the land (`Body::TouchGround`)
and against other bodies' faces (`Body::TouchFaces`, `src/Physics/Body.cpp`), with the pair
rules and the turn in `src/ECS/Systems/Implementations/DynamicsSystem.cpp`; the creature is a stand-in ball, not its bones' ellipsoids.
No other physics library is linked. Tests:
`test/test_physics_body_pose.cpp`, `test/test_physics_body_water.cpp`.

**Progress: 30/35 done, 0 chosen, 5 partial, 0 inferred — 94%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## Against the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every point of a body is tested against the land height under it; the push is along the flat normal of the land triangle there | done | `Body::TouchGround` (land height per vertex, the triangle's normal through `land_normal::OfCell` with the original's two tables and 16-bit fractions), `src/Physics/Body.cpp`; tests `TestLandNormal.TheFourTriangles`, `TestLandNormal.TableQuantisationOnASteepCell`; scenario "Rocks hurled at the ground" |
| Against the land a body uses its own material's stiffness, damping and friction | done | `Body::SetUpConstants` from the class's row of `Data/PhysicsConstants.txt` (`DynamicsSystem::LoadConstants`, `ConstantsType`); tests `PhysicsMaterials.*` |
| A lump of poo has a material of its own, not a loose object's | done | row 12 of `Data/PhysicsConstants.txt` (`DynamicsSystem::ConstantsType`); test `ParticleCarriedObjectsTest.ALumpOfPooTakesItsOwnRowOfPhysicsConstants`; see [../../../physics/physics.md](../../../physics/physics.md) |
| A thrown object is only ever pushed up out of the land when it leaves the hand, never pulled down to it | done | `Body::SettleOnLand(noPullDown, ...)` called by `physics::from_hand::InitialisePhysicsFromHand` (`src/ECS/Physics/FromHand.cpp`); test `PhysicsBodyPose.SettleOnLand` |
| A body over the sea stops touching the land while any of its points is under the water | done | `Body::TouchGround` sea branch (buoyancy, no contact with the bottom); tests `PhysicsBodyWaterTest.NearestCellDecides`, `NoCellIsWater` |

## Object against object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two bodies are tested only when they are closer than the sum of their radii; then each point of one is tested against the faces of the other | done | `physics::TestsPoints` from `Substep` in `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (sum of radii), then `Body::TouchFaces`; test `PhysicsPairs.WhoTestsAgainstWhom` |
| Between two bodies the softer stiffness and damping are used, and the friction is 0.3 times the smaller friction (things slide off each other easily) | done | `Body::ApplyContacts` (min stiffness and damping, 0.3 x the smaller friction); scenario "Sixty things thrown together" |
| What pushes one body pushes the other back as hard, and turns it | done | `Body::TouchFaces` marks both bodies and `ApplyContacts` works each; no unit test, scenario "Sixty things thrown together" |
| Two resting bodies don't collide with each other, except in the step either was just put into the physics | done | `physics::TestsPoints` skips two resting bodies unless either is `justSetUp`; test `PhysicsPairs.WhoTestsAgainstWhom` |
| A thrown object and its thrower pass through each other for as long as the object stays in the physics list | done | `physics::TestsPoints` thrower test; test `PhysicsPairs.WhoTestsAgainstWhom` |
| Villagers' bodies pass through things a villager pushed or kicked, and those through villagers | done | The pair skip in `physics::TestsPoints` (`PhysicsEntry::k_PushedByLiving`), set by `DynamicsSystem::PushObject` and a forester's felled tree (`ecs::FellTree`, `src/ECS/Trees.cpp`); test `PhysicsPairs.WhoTestsAgainstWhom`. Villagers do not call `PushObject` yet (see [object_dynamics.md](object_dynamics.md)) |
| What a released villager was carrying flies without hitting other objects | done | `villager::CreateDroppedResource` (from `FromHand.cpp` and `VillagerPhysics.cpp`) makes the log through `VillagerStores::MakeDroppedLog` and `DynamicsSystem::AddDroppedObject` with `k_NoObjectCollision`; tests `VillagerResourcesTest.CreateDroppedResource`, `PhysicsPairs.WhoTestsAgainstWhom` |
| Contacts are tested 0.06 s ahead along the body's motion, so fast objects stop at thin walls such as the physical shield | done | `physics::k_LookAheadSeconds` and the predicted vertex length in `TouchFaces` ([physics.md](../../../physics/physics.md#the-physical-shield-test)); no unit test, scenario "Physical shield: a thrown ball bounces off" |
| Each body remembers what it last hit in the turn; what reacts to a hit knows what hit it, and a body thrown by nobody takes the credit of whoever threw what hit it | done | `EndTurn` in `DynamicsSystem.cpp` (`Body::lastHit`, `PhysicsEntry::hitBySlot`, `throwerPlayer` passed on); tests `PhysicsEndOfTurn.*`, `PhysicsTurnRules.LookUpHitter`, `PhysicsHitter.*` |
| How hard a hit was is the mean of the body's total force (gravity included) over the touched steps of the turn; divided by its weight it gives the G of the hit (about 0 for something lying still) | done | `EndTurn` (`impact = the length of forceSum x 0.05`, as the exe's 0x64624A), `PhysicsEntry::GLoad`; test `PhysicsTurn.ImpactIsTheMeanForceAndIgnoresTheSmallest`. The wiki's "about 1 when resting" line ([physics.md](../../../physics/physics.md#manager)) is to be corrected: the sum includes gravity |

## What can be hit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks, statics, mobile objects, villagers, animals, dead trees and carried food or wood are obstacles to flying bodies | done | `DynamicsSystem::InteractsWithPhysicsObjects` (mobile statics and objects, villagers, animals, dead trees, the hand's food and wood pots); scenarios "Sixty things thrown together", "Villagers watch a thrown rock" |
| Standing trees and forests are not obstacles: thrown things pass through them | done | `DynamicsSystem::InteractsWithPhysicsObjects` (`Tree`, `BigForest` excluded); scenario "Sixty things thrown together" |
| Fields are never hit | done | `DynamicsSystem::InteractsWithPhysicsObjects` (`Field` and field abodes excluded; the exe's `Field::InteractsWithPhysicsObjects` 0x528020); no unit test, the fidelity run's thrown objects |
| A creed (the creed and the eyes) is not an obstacle: thrown things pass through it, and nothing is raised clear of it | done | `DynamicsSystem::InteractsWithPhysicsObjects` for a mobile object whose row is of the creed's kind (test `ParticleCarriedObjectsTest.ACreedIsNotInTheWayOfWhatFlies`; [A creed never flies](../../../physics/physics.md#a-creed-never-flies)) |
| Food and wood piles are hit, unless they are part of a storage pit | partial 50% | The exe's `Pot::InteractsWithPhysicsObjects` 0x66CED0 (the piles' too) says no only for a pot in a storage pit, else yes with a 3D object. `DynamicsSystem::InteractsWithPhysicsObjects` hits the hand's food and wood pots but never a pile; the wiki's "piles do not interact" ([physics.md](../../../physics/physics.md#manager)) is wrong |
| Buildings are obstacles only while more than 10% built and still standing (life above 0.01); ruins are not. The village centre always is; worship sites, graveyards, fish farms, football pitches, lanterns, bonfires and totem statues never are | partial 30% | Exe: `MultiMapFixed` 0x52F3D0 (built > 0.1 and life > 0.01), `TownCentre` 0x743BC0 yes, worship site, graveyard, fish farm, football, lantern, bonfire, totem statue no. `DynamicsSystem::InteractsWithPhysicsObjects` tests only the life; the built share and the per-type answers are missing |
| A building is a heavy obstacle that never moves; a hit damages it instead | done | Mass 2000, not dynamic (`DynamicsSystem::BodyMassOf`, `CanBecomeAPhysicsObject`); the hit goes to `BuildingDamageSystem::ReactToImpact`; scenario "Huts broken by thrown boulders"; see [impact_damage.md](impact_damage.md) |
| Pieces broken off buildings hit only the land | done | Fragment bodies have no faces and are not obstacles (`SetUpBody` fragment branch, `InteractsWithPhysicsObjects`, the exe's `Fragment` 0x76F3D0); scenario "Huts broken by thrown boulders" |
| The creature is an obstacle that is never moved by the physics: thrown things hit one ellipsoid per bone | partial 30% | `creature_physics::SetUpBody` (`src/ECS/CreaturePhysics.cpp`): a static obstacle (exe `Creature::InteractsWithPhysicsObjects` 0x479D20 = 1), but a ball of six points, not the bounding sphere with a point per bone, and not mass 1000 (`Creature::SetUpPhysOb` 0x479B90); issue #43; scenario "A rock thrown at a creature" |
| A physical shield is always in the physics as a very heavy static dome (weight 50000 × its scale cubed) that thrown things bounce off | done | `BeginTurn` keeps every physical shield (`magic::map_shield::Shields`), weight from its info at the collision scale (`src/Magic/Objects/MapShield.cpp`); scenario "Physical shield: a thrown ball bounces off" |
| A spiritual shield and the miracle vortices are not obstacles | done | `map_shield::InteractsWithPhysicsObjects` (magic shield no, exe 0x72C330); of the land vortices only the way out of a land is an obstacle (exe `LandscapeVortexIn` 0x5FD8C0 = 1, the others 0x5FFF80 = 0), as `InteractsWithPhysicsObjects`; scenario "A vortex out of the land takes what is thrown into it" |

## Knock-back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A resting object hit by a flying body is knocked into flight (rocks, villagers, animals, pots) and flies on its own | done | `physics::StepResult::Knocked` in `Substep`, woken through the class's `initialisePhysicsKnocked` (`VillagerPhysics.cpp`) or `CanWakeKnockedProxy`; scenarios "Sixty things thrown together", "Villagers watch a thrown rock" |
| A rock hit hard may split in two, the halves keeping its speed and half its spin | done | `ReactToPhysicsImpact` in `DynamicsSystem.cpp` calls `object_physics::KnockRock`, which splits it with the velocity and half the angular momentum (`src/ECS/ObjectPhysics.cpp`); scenario "Rocks hurled at the ground" |
| Whoever threw the first object is credited for what the objects it knocked do | done | `EndTurn` passes `throwerPlayer` on from the hitter; tests `PhysicsTurnRules.LookUpHitter`, `PhysicsEndOfTurn.DamageByThrowingNeedsTheHandACreditAndALivingThingOrARock` |

## Stuck and embedded objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A released object that overlaps things under it is raised, step by step by the deepest overlap through their tops, until nothing pushes it | done | `DynamicsSystem::RaiseClearOfWhatIsUnder` with `Body::OverlapDepth` (exe 0x644800); test `FromHandLanding.AThingRaisedOverWhatIsUnderItDoesNotLandUnlessAComputerPlayersVillager` |
| A villager is not raised over things a villager pushed, nor those over a villager | done | `RaiseClearOfWhatIsUnder` skips that pair through the pure rule `physics::turn::RaisedOver` (a villager over a `k_PushedByLiving` body, or such a body over a villager), as the exe's flag-2 test at 0x644B0C; test `PhysicsTurn.ARaisedBodyGoesOverEveryBodyButTheVillagerAndPushedPairs` |
| The raise never makes a resting body of a map shield or a way out of a land, but every body already in the physics raises, the physical shield too (it is in the physics from its creation) and a way out of a land something moved near | done | `RaiseClearOfWhatIsUnder` makes resting bodies only of what `InteractsWithPhysicsObjects` and has a mesh (never the magic shield or a vortex), and goes over every listed body whatever its class (`physics::turn::RaisedOver`, test `PhysicsTurn.ARaisedBodyGoesOverEveryBodyButTheVillagerAndPushedPairs`); the physical shield is in the physics from the turn start after its creation ([physics-impacts.md](../../../physics/physics-impacts.md)) |

## Map edges

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Looking up map cells for a body near or past the edge uses the nearest edge cells of the 512 by 512 map | partial 90% | `BoxCell` clamps each corner to 0..511 (`DynamicsSystem.cpp`), as the exe's fn_00604250; no test or check covers it |
| An object that comes to rest outside the map is deleted | partial 90% | `DynamicsSystem::EndPhysicsAsObject` from `EndPhysics` (outside `map_coords::InBounds`: `ToBeDeleted`); no test or check covers it |
| A body that falls more than four radii under sea level (off the land into the deep) is deleted | done | `Body::Integrate` returns `StepResult::Delete` below -4 R, then `ToBeDeleted` (or `BuildingDamageSystem::DestroyFragment`) in `Substep`; scenario "Things dropped into the sea" |
