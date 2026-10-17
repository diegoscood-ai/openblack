/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>

#include "ECS/DynamicsWorld.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Systems/DynamicsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::systems
{
/// The game's physics, with its state as the game starts it
class DynamicsSystem final: public DynamicsSystemInterface
{
public:
	/// The physics in the game's own world
	DynamicsSystem();
	/// The same in the world given
	explicit DynamicsSystem(std::unique_ptr<dynamics::World> world);

	[[nodiscard]] PhysicsClass ClassOf(entt::entity entity) override;
	void SetClassHooks(std::unique_ptr<PhysicsClassHooks> hooks) override;
	[[nodiscard]] PhysicsClassHooks& ClassHooks() override;
	void LoadConstants() override;
	[[nodiscard]] const openblack::physics::PhysicsData& Constants(int type) override;
	[[nodiscard]] int ConstantsType(entt::entity entity) override;
	[[nodiscard]] bool PhysicallyDestroysAbodes(entt::entity entity) override;
	[[nodiscard]] bool InteractsWithPhysicsObjects(entt::entity entity) override;
	[[nodiscard]] bool CanBecomeAPhysicsObject(entt::entity entity) override;
	[[nodiscard]] bool CanWakeKnockedProxy(entt::entity entity) override;
	[[nodiscard]] bool SetUpBody(entt::entity entity, openblack::physics::Body& body, bool dynamic) override;
	[[nodiscard]] float WeightOf(entt::entity entity) override;
	[[nodiscard]] float BodyMassOf(entt::entity entity) override;
	[[nodiscard]] const GObjectInfo* ObjectInfo(entt::entity entity) override;
	PhysicsEntry* AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                        entt::entity thrower = entt::null, bool fromHand = false) override;
	PhysicsEntry* AddObject(entt::entity entity, glm::vec3 velocity, BodySpin spin, entt::entity thrower = entt::null,
	                        bool fromHand = false) override;
	PhysicsEntry* InitialisePhysics(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity) override;
	void InitialisePhysicsOfClass(PhysicsEntry& po, bool fromHand) override;
	void ThrowFromHandOfClass(entt::entity entity, bool dontReplant) override;
	void SyncTurnStart(PhysicsEntry& po) override;
	PhysicsEntry* AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                               std::optional<glm::vec3> angularMomentum) override;
	PhysicsEntry* AddDroppedObject(entt::entity entity, glm::vec3 velocity, BodySpin spin,
	                               std::optional<glm::vec3> angularMomentum) override;
	void RemoveObject(entt::entity entity) override;
	PhysicsEntry* FinishDroppedObject(PhysicsEntry& body, std::optional<glm::vec3> angularMomentum) override;
	void RemoveObjectWithEndPhysics(entt::entity entity) override;
	bool EndPhysicsAsObject(entt::entity entity) override;
	void RaiseClearOfWhatIsUnder(PhysicsEntry& po) override;
	[[nodiscard]] PhysicsEntry* Find(entt::entity entity) override;
	[[nodiscard]] bool IsFlying(entt::entity entity) override;
	float PushObject(entt::entity object, entt::entity pusher) override;
	[[nodiscard]] glm::mat3 RotationFromRows(entt::entity entity, const glm::mat3& rows) override;
	[[nodiscard]] glm::mat3 DrawQuarterTurn(const glm::mat3& rows) override;
	void CheckAllCreaturesForCatching(entt::entity object, PhysicsEntry& po) override;
	void ProcessTurn() override;
	void RecordHit(entt::entity hit, entt::entity hitter) override;
	[[nodiscard]] entt::entity GetHitObject() const override;
	[[nodiscard]] entt::entity GetObjectWhichHit() const override;
	void UpdateFrame(float turnFraction, float seconds) override;
	void ResetSimulation() override;
	void BeginPrediction(entt::entity object) override;
	void SetPrediction(entt::entity object, openblack::physics::Body body,
	                   std::vector<std::pair<glm::mat3, glm::vec3>> history) override;
	void EndPredictionOf(entt::entity object) override;
	[[nodiscard]] bool IsPredictionObject(entt::entity object) override;
	void ForEachEntry(const std::function<void(const PhysicsEntry&)>& func) override;
	void AddPuff(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb) override;
	void AddPuff(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
	             openblack::physics::turn::PuffKind kind) override;
	[[nodiscard]] glm::vec3 SyncedPuffVelocity() override;
	void ClearDust() override;
	void SnapshotDust(std::vector<physics::DustParticleDraw>& out) override;
	void PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track) override;
	[[nodiscard]] physics::State& GetState() noexcept override { return _state; }

private:
	// The turn's steps and the helpers they share
	[[nodiscard]] DynamicsSystemInterface& Dynamics();
	[[nodiscard]] physics::State& PhysicsState();
	[[nodiscard]] std::optional<graphics::TextureHandle> DustTexture();
	[[nodiscard]] int CollideSoundType(entt::entity entity);
	void AttemptCollisionSound(const PhysicsEntry& po);
	void UpdateDust(float seconds);
	void EndPrediction();
	[[nodiscard]] const graphics::L3DMesh* MeshOf(entt::entity entity);
	[[nodiscard]] bool MeshIs(entt::entity entity, uint32_t meshIndex);
	[[nodiscard]] const GObjectInfo* InfoOf(entt::entity entity);
	bool SetUpBodyOf(entt::entity entity, openblack::physics::Body& body, bool dynamic, bool handPose = false);
	void SyncTransform(const PhysicsEntry& po);
	bool ReactToPhysicsImpact(PhysicsEntry& po);
	entt::entity EndPhysicsOfClass(PhysicsEntry& po, bool turnStop);
	entt::entity EndPhysics(PhysicsEntry& po, bool turnStop);
	/// Whether the object is one that breaks buildings (nothing is known of an object already gone)
	[[nodiscard]] bool BreaksBuildings(entt::entity entity);
	void ForgetAsBuildingHitter(entt::entity object, bool breaksBuildings);
	void RemoveAt(size_t index);
	/// The object's InPhysics tag set again from the list and the carry: there while it has a moving body or is carried
	void SyncInPhysics(entt::entity entity);
	/// RemoveObject's work on the entry at index, which AddObject also does on a resting body it replaces (all but the
	/// buildings forgetting their hitter, which only RemoveObject does)
	void TakeOutAt(size_t index);
	void MakeSureEndSlotIsFree();
	void AddProxy(entt::entity entity);
	PhysicsEntry* Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower, bool fromHand,
	                  bool spreadReaction, bool initialiseClass);
	void BeginTurn();
	void Substep();
	void EndTurn();
	/// The end of a turn's work on these bodies (physics::turn::EndOfTurnSteps)
	class EndTurnSteps;

	std::unique_ptr<dynamics::World> _world;
	physics::State _state;
	/// A turn's steps and its end are running: nothing leaves the list then
	bool _inTurnUpdate {false};
	/// The scripts' record (RecordHit)
	entt::entity _hitObject {entt::null};
	entt::entity _objectWhichHit {entt::null};
};
} // namespace openblack::ecs::systems
