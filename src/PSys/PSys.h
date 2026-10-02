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

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/FrameAnim.h"
#include "PSysFile.h"
#include "SpellLink.h"

// The original's generic particle system (PSysManager, AtomCollection, AtomCore, the modifier classes of the spell
// files). Report: dev\tmp_dis\psys\psys_report.md; wiki: rendering.md, "Partículas".

namespace openblack::audio
{
struct PSysSound;
}

namespace openblack::psys
{
class Effect;
struct Atom;
struct Collection;
class Modifier;

/// ParticleCreator (fn_006A85E0) and its sprite form (ParticleSpriteCreator, 0x6AA0D0); the other kinds (point, mesh,
/// mist, chain, light map...) are not drawn yet. Those derive from it in their own files (PSysRegistry.h).
struct Creator
{
	Creator() = default;
	Creator(const Creator&) = default;
	Creator(Creator&&) = default;
	Creator& operator=(const Creator&) = default;
	Creator& operator=(Creator&&) = default;
	virtual ~Creator() = default;

	enum class Kind
	{
		Point,
		Sprite,
		Mesh,  ///< ParticleMeshCreator / ParticleMeshCreatorAnimTextured (Creators/Mesh.cpp)
		Chain, ///< ParticleChainCreator: the joints of one collection are drawn as a ribbon (Creators/Chain.cpp)
		Other,
	};
	Kind kind {Kind::Point};
	std::string className;
	uint8_t r {255}, g {255}, b {255}, a {255};
	/// +0x24/+0x28/+0x2C SpecColorR/G/B (DefineProperties 0x6B3562..0x6B359B; ctor 0x6A91C4..0x6A91CA: 0)
	int specR {0}, specG {0}, specB {0};
	bool usePlayerColour {false};     ///< +0x0D UsePlayerColor (ctor 0)
	float usePlayerColourBlend {1.0f}; ///< +0x10 UsePlayerColorBlend (ctor 1.0)
	float initialScale {1.0f};
	bool randomiseScale {false};
	// sprites
	std::string texture; ///< e.g. S_SpriteSheet3 (the .raw without extension; its alpha is <name>a.raw)
	int fileOffset {0};
	int spritesPerRow {8};
	int numFrames {1};
	int initFrame {0};
	bool randomiseInitFrame {false};
	bool randomiseFrameDirection {false};
	float frameRate {1.0f};
	bool playAnim {false};
	bool loopAnim {true};
	bool additive {true};    ///< UseAdditiveAlpha: mode 13, else mode 6
	bool writeDepth {false}; ///< MaterialUpdateZBuffer: modes 12 / 5
	int scaleAlpha {255};
	float stretch {1.0f};
	bool horizontal {false}; ///< SetHorozontal: a flat XZ quad
	bool centreAtBase {false};
	bool ignoreRotation {false};
	float originX {0.0f}, originY {0.0f};

	/// The creator's part of a new atom (after CommonInitNewAtom), for the classes that have one
	virtual void InitAtom(Effect& /*effect*/, Atom& /*atom*/) const {}
	/// The frame count of its atoms (AtomCore +0x114, the N of fn_00673EA0 / fn_00679920): NumFrames
	[[nodiscard]] virtual int FramesPerAtom() const { return numFrames; }
	/// A creator that only picks another one (ParticleGoodEvilCreator::CreateParticle 0x6AAA00 calls the chosen creator's
	/// CreateParticle): the creator the atom really gets. Itself for the others.
	[[nodiscard]] virtual const Creator* Resolve(const Effect& /*effect*/) const { return this; }
};

/// fn_006A85E0's UsePlayerColor on an atom colour (r, g, b, a): the player's colour (GetPlayerColour 0x64D800, alpha
/// forced to 0xFF; pure black, the neutral player, becomes white), with a blend b = ftol(Blend x 255) & 0xFF != 255 moved
/// towards white per channel (255 + ((c - 255) b >> 8), & 0xFF), then each channel of the colour x that >> 8 (alpha too)
[[nodiscard]] std::array<uint8_t, 4> TintWithPlayerColour(std::array<uint8_t, 4> rgba, uint32_t playerArgb, float blend);

/// PosScaleRotation of the last two steps, lerped at draw time (fn_00679920)
struct DrawState
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	float scale {1.0f};
	float stretch {1.0f};
	float alpha {255.0f};
	float frame {0.0f};
};

/// AtomCore (0x130 bytes)
struct Atom
{
	Atom() = default;
	Atom(const Atom&) = delete;
	Atom(Atom&&) = delete;
	Atom& operator=(const Atom&) = delete;
	Atom& operator=(Atom&&) = delete;
	/// Its sounds lose their atom (AtomCore::StopSound 0x674500 on each; defined in Audio/Services/SpellSounds.cpp)
	~Atom();

	Collection* collection {nullptr};
	const Creator* creator {nullptr};
	glm::vec3 position {0.0f}; ///< +0x80, local to the parent atom in a hierarchy
	glm::vec3 velocity {0.0f}; ///< +0x34
	glm::mat3 rotation {1.0f}; ///< +0x44
	float baseScale {1.0f};    ///< +0x74 (InitialScale)
	float ruleScale {1.0f};    ///< +0x78
	float stretch {1.0f};      ///< +0x7C
	std::array<uint8_t, 4> colour {255, 255, 255, 255}; ///< +0x8C ARGB as r, g, b, a
	/// +0x90 the specular, D3DCOLOR ARGB: fn_006A85E0 0x6A8748..0x6A875B puts SpecColorR/G/B there, alpha 0 (0 in
	/// every dumped spell file); copied raw to DrawData +0xC (0x679BF4)
	uint32_t specular {0};
	float birth {0.0f};
	bool visible {true}; ///< flag 0x10 (EventConditionAtomInUse)
	float frame {0.0f};     ///< +0x10C (fn_00673EA0 keeps it and +0x108, the previous step's, in [0, 2N))
	float frameRate {0.0f}; ///< +0x110
	bool playAnim {false};  ///< +0x118 PlayAnim: fn_00673EA0 steps the frame only when it is 1 (0x673FD7)
	float gravity {1.0f}; ///< +0x11C
	uint32_t random {0};  ///< +0x12C
	uint32_t flags {0};   ///< +0x94 (bit 3: deflected, SetAtomHasBeenDeflected 0x6A26C0)
	DrawState previous;
	DrawState current;
	bool drawn {false};
	std::vector<std::unique_ptr<Collection>> subCollections;
	/// per-modifier data of this atom (latches, phases)
	std::unordered_map<const Modifier*, glm::vec4> data;
	/// +0x24 as the original keeps it: a modifier's own data object (BaseAtomModifierData, +0x1C its modifier),
	/// destroyed with the atom (UR_HealSpellChakra::AtomData lets go of its target there)
	std::unordered_map<const Modifier*, std::shared_ptr<void>> modifierData;
	/// +0x2C: the sounds it started, newest first (Audio/Services/SpellSounds.h)
	std::vector<std::shared_ptr<audio::PSysSound>> sounds;
	/// A ParticleMistCreator atom's LH3DMist +0x84 (its render object, CreateLH3DMist 0x6AA5A0; Creators/Mist.cpp): seeded
	/// by the ctor 0x7F9560 and advanced by the draw fn_007FA300 only while it is on screen, so it is changed through the
	/// const atoms of the draw; unused by other atoms
	mutable graphics::frame_anim::MistClock mist;
	/// +0x124 its DrawOffset (AtomCore::SetDrawOffset 0x673AF0). Only DrawOffsetLT (0x28 bytes, ctor 0x6C75A0) is
	/// ported; UR_Lightning's CreateForkStructure gives one to every fork joint (0x69131C..0x69134C). fn_00679920 adds
	/// GetOffset to the atom's drawn position every frame, interpolated between the steps or not (0x679B69..0x679BBF)
	struct DrawOffsetLT
	{
		glm::vec3 reference {0.0f}; ///< +0x1C
		float weight {0.0f};        ///< +0x18
		/// SetRefPos 0x6C7600: the point, and the weight clamped to 0..1 (0x6C7604..0x6C7680: a NaN gives 0)
		void SetRefPos(const glm::vec3& point, float w)
		{
			reference = point;
			weight = w > 0.0f ? (w < 1.0f ? w : 1.0f) : 0.0f;
		}
		/// GetOffset 0x6C7690: (the hand of my interface now, GInterface +0x3A0 = CHand, +0x78, - the point) x weight
		[[nodiscard]] glm::vec3 GetOffset(const glm::vec3& hand) const { return (hand - reference) * weight; }
	};
	std::optional<DrawOffsetLT> drawOffset;
};

/// AtomCollection (0x54 bytes): one live instance of a group
struct Collection
{
	int group {0};
	Atom* parent {nullptr};
	float birth {0.0f};
	float alpha {255.0f}; ///< +0x50
	/// +0x38 flags (AtomCollection ctor 0x675CA2: 3). Bit 0x02: the atoms are drawn interpolated between the last two
	/// steps (fn_00679920 0x67999E; without it the current PSR is drawn as is). UR_Lightning clears it on its forks
	/// (0x6912A1, 0x691B42, 0x692B1D)
	uint8_t flags {3};
	bool hierarchy {false};
	/// The Chain of a collection of chain joints (ctor 0x6C8830): its v-scroll +0x3C (frame_anim::ChainScroll, advanced
	/// by the draw fn_0067B3F0, so changed through the const collections of the draw) and its rate +0x4C, set only by
	/// UR_SimpleBeam / UR_Plasma (not ported: 0)
	mutable float chainScroll {0.0f};
	float chainScrollRate {0.0f};
	/// The Chain's repeats along the ribbon (+0x30) when a rule rewrites them (UR_Lightning's NumTexturesToTile,
	/// 0x6923FC); -1: what CreateChain put there from the creator (0x6AA8DC..0x6AA8EB)
	int chainTextures {-1};
	std::vector<std::unique_ptr<Atom>> atoms;
	struct Slot
	{
		const Modifier* modifier {nullptr};
		bool attached {true};
		glm::vec4 state {0.0f}; ///< per-collection data (+0x24): emitter timing and counts
		glm::vec4 extra {0.0f}; ///< more of it (UR_WillowWisp: the amount emitted and the atoms made)
		bool first {true};
		/// an object the rule made and must close later (UR_Explosion: the beam's GParticleContainer, CollectionData
		/// +0x54); 0xFFFFFFFF = none (entt::null)
		uint32_t object {0xFFFFFFFFu};
	};
	std::vector<Slot> modifiers;
	/// +0x24 the BaseCollectionModifierData list (+0x1C each one's modifier): a modifier's CollectionData, found by its
	/// modifier (UR_CloudGather 0x6D4AB0, UR_Tornado 0x6D18C3 and its sub-collections' data 0x6D2B10 / 0x6D2EA6),
	/// destroyed with the collection
	std::unordered_map<const Modifier*, std::shared_ptr<void>> modifierData;
};

/// AtomCollectionModifier (Group, Condition, RemoveOnCloseDown) and the event conditions / float providers it uses
class Modifier
{
public:
	virtual ~Modifier() = default;
	int group {-1};
	bool removeOnCloseDown {false};
	std::string condition;
	/// true while it creates atoms (a create rule or emitter: `finished()` waits for them, mask 4)
	[[nodiscard]] virtual bool Creates() const { return false; }
	/// a class the port doesn't run yet (a spell's effect counts it as a creator until it closes, see AnyCreatorLeft)
	[[nodiscard]] virtual bool Unported() const { return false; }
	/// modifier flag 2 (+0x10) without 4: an effect with no atoms is not finished while it is attached and the effect
	/// is not closing (fn_00673290; UR_HealSpellChakra's ctor 0x6A0810 sets 6 and clears 4: it waits for targets)
	[[nodiscard]] virtual bool KeepsAlive() const { return false; }
	/// ModifyAtomCollection; false detaches it from this collection
	virtual bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const;
	/// ModifyAtomCore; false deletes the atom (remove rules)
	virtual bool ModifyAtom(Effect& /*effect*/, Atom& /*atom*/, Collection::Slot& /*slot*/) const { return true; }
};

/// PSysManager: one effect started from a spell file
class Effect
{
public:
	Effect(std::shared_ptr<const File> file, glm::vec3 origin, float magnitude, uint32_t seed);
	~Effect();

	/// fn_00673340: one step of dt seconds
	void Step(float dt);
	/// SetState(1) 0x672FF0
	void CloseDown();
	[[nodiscard]] bool Finished() const;
	[[nodiscard]] bool Closing() const { return _closing; }
	[[nodiscard]] bool DeleteOnCloseDown() const { return _deleteOnCloseDown; }

	void SetOrigin(glm::vec3 origin) { _origin = origin; }
	/// vt 0x11C SetMagnitude (CHand::DrawSpellInHand gives the in-hand effect the hand's scale every frame)
	void SetMagnitude(float magnitude) { _magnitude = magnitude; }
	[[nodiscard]] glm::vec3 GetOrigin() const { return _origin; }
	[[nodiscard]] float GetAge() const { return _age; }
	[[nodiscard]] float GetCloseAge() const { return _closeAge; }
	[[nodiscard]] float GetMagnitude() const { return _magnitude; }
	[[nodiscard]] float GetDt() const { return _dt; }
	[[nodiscard]] const File& GetFile() const { return *_file; }

	// ---- the spell link (SpellLink.h) ----
	/// PSysInterface::Create 0x68E910 with a Spell: the rules' events go to it; sends event 1 (fn_00673070)
	void SetSink(SpellSink* sink);
	[[nodiscard]] SpellSink* GetSink() const { return _sink; }
	/// PSysProcessInfo of this step (Spell::CoreProcess passes the spell's +0x64 to vt 0x100)
	void SetProcessInfo(const ProcessInfo& info) { _info = info; }
	[[nodiscard]] const ProcessInfo& GetProcessInfo() const { return _info; }
	/// PSysManager::SpellEvent 0x6734C0: to the spell, 0 without one
	int SendSpellEvent(const SpellEventInfo& event) const;
	/// PSysManager::GetPowerUpLevel 0x673510: the spell's level, -1 without one
	[[nodiscard]] int PowerUpLevel() const { return _sink != nullptr ? _sink->PowerUpLevel() : -1; }
	/// PSysManager::NetUnsafeIsMyInterfaceCasting 0x673540: the spell's +0x44, and 1 for an effect without a spell
	[[nodiscard]] bool IsMyInterfaceCasting() const { return _sink == nullptr || _sink->IsMyInterfaceCasting(); }
	/// PSysManager::IsHumanPlayerCasting 0x673580: the spell's +0x4C, 0 without a spell
	[[nodiscard]] bool IsHumanPlayerCasting() const { return _sink != nullptr && _sink->IsHumanPlayerCasting(); }
	/// AddTarget_ (vt 0x114): SpellTargets, read by the target rules (heal chakra, flocks)
	void AddTarget(entt::entity target) { _targets.push_back(target); }
	/// SpellTargets::TakeTargetObject 0x671030: the last one added, taken out (entt::null when there is none)
	entt::entity TakeTarget()
	{
		if (_targets.empty())
		{
			return entt::null;
		}
		const auto target = _targets.back();
		_targets.pop_back();
		return target;
	}
	[[nodiscard]] const std::vector<entt::entity>& GetTargets() const { return _targets; }
	/// SpellTargets' points (+0x14, 12 bytes each: the AddTarget 0x670CF0 that takes an LHPoint), e.g. a shield's
	/// impacts (fn_006D0AF0) for its sparks. fn_006710B0 counts points + objects; fn_00670F00 takes a point first.
	void AddTargetPoint(const glm::vec3& point) { _targetPoints.push_back(point); }
	bool TakeTargetPoint(glm::vec3& out)
	{
		if (_targetPoints.empty())
		{
			return false;
		}
		out = _targetPoints.back();
		_targetPoints.pop_back();
		return true;
	}
	[[nodiscard]] size_t TargetPointCount() const { return _targetPoints.size(); }
	/// PSysInterface::Create's direction argument (the spell's +0xD8; mgr +0x84..)
	void SetDirection(glm::vec3 direction) { _direction = direction; }
	[[nodiscard]] glm::vec3 GetDirection() const { return _direction; }
	/// vt 0x20: the casting player (the good/evil creators and colours read it)
	void SetPlayer(int player) { _player = player; }
	[[nodiscard]] int GetPlayer() const { return _player; }
	/// The draw's alpha (PSysManager +0x14 -> +0x6C, 0..255; PhysicalShield::DrawShield 0x72CED0 sets it): the drawn atoms'
	/// alpha x this / 255
	void SetGlobalAlpha(float alpha) { _globalAlpha = alpha; }
	[[nodiscard]] float GetGlobalAlpha() const { return _globalAlpha; }

	// used by the modifiers
	[[nodiscard]] float Random(float max);            ///< PSysFloatRand, [0, max)
	[[nodiscard]] glm::vec3 RandomInBall();            ///< PSysRandR3
	[[nodiscard]] float FloatProvider(const std::string& name, float fallback) const;
	[[nodiscard]] bool ConditionForCollection(const std::string& name, const Collection& collection) const;
	[[nodiscard]] bool ConditionForAtom(const std::string& name, const Atom& atom) const;
	[[nodiscard]] const Creator* FindCreator(const std::string& name) const;
	/// CommonInitNewAtom 0x674C60 + the creator's init: a new atom in the collection, its NextGroups sub-collections
	Atom& NewAtom(Collection& collection, const Creator* creator, const std::vector<int>& nextGroups);
	/// AtomCore::AddSubCollections 0x673B70: new sub-collections of these groups under the atom
	void AddSubCollections(Atom& atom, const std::vector<int>& groups);
	/// PSysManager::fn_006731B0: a new atom in the first root collection of that group (the lightning's light maps go
	/// into their InitiallyCreated group, not under the fork). nullptr when that group has no root collection.
	Atom* NewAtomInGroup(int group, const Creator* creator);
	/// AtomCore::MoveToBaseGroup 0x673BD0: the atom leaves its collection for the first root collection of that group
	/// (fn_00673180), where it keeps its position, velocity and data (the tornado's flung objects, 0x6D33F1). With no
	/// such collection the original leaves it in none (never updated nor drawn again): (aproximado) here it is deleted. Call it
	/// only while `from` is not being iterated.
	void MoveToBaseGroup(Collection& from, const Atom& atom, int group);
	[[nodiscard]] glm::vec3 SpawnPosition(const Collection& collection) const;
	[[nodiscard]] glm::vec3 GlobalPosition(const Atom& atom) const;
	/// AtomCollection::LocalToGlobal 0x6751D0 / GlobalToLocal 0x675410: a point of the collection's frame. In a hierarchy
	/// the frame is the product of the local matrices (fn_00673DB0: rotation, scale, position) of the ancestor atoms whose
	/// group is flagged in Hierarchies (fn_006752D0); outside one, the world.
	[[nodiscard]] glm::vec3 LocalToGlobal(const Collection& collection, const glm::vec3& local) const;
	[[nodiscard]] glm::vec3 GlobalToLocal(const Collection& collection, const glm::vec3& global) const;
	/// fn_00673DB0's scale of an atom's frame: baseScale x ruleScale, the Y axis also x the stretch
	[[nodiscard]] static glm::vec3 FrameScale(const Atom& atom);
	[[nodiscard]] float AtomAge(const Atom& atom) const { return _age - atom.birth; }
	[[nodiscard]] float CollectionAge(const Collection& collection) const { return _age - collection.birth; }

	/// Every drawn atom with its interpolated state (t: fraction of the step since the last one, 0..1)
	struct DrawAtom
	{
		const Creator* creator;
		glm::vec3 position;
		glm::mat3 rotation;
		float scale;
		float stretch;
		float alpha;
		float frame;
		std::array<uint8_t, 3> colour;
		uint32_t specular {0}; ///< the atom's +0x90 (DrawData +0xC, 0x679BF4), not interpolated
		/// DrawData +0 (0x679C0C): the atom it was taken from (none for the fire's and the town belief's)
		const Atom* atom {nullptr};
	};
	/// kind: the sprites (RendererPSys.cpp) or the meshes (Creators/Mesh.cpp, drawn as instances)
	void Collect(float t, std::vector<DrawAtom>& out, Creator::Kind kind = Creator::Kind::Sprite) const;
	/// One chain collection: its joints in list order, the ribbon fn_0067B3F0 draws them as a strip (Creators/Chain.h)
	struct DrawChain
	{
		const Creator* creator;
		std::vector<DrawAtom> joints;
		const Collection* collection {nullptr}; ///< the chain's collection (its scroll)
		/// The effect's origin: the ribbon is drawn inside the effect's single Z object (fn_006798B0 0x6798DD takes the
		/// fn_0067B370 branch, "draw now", whenever the manager is drawn from the Z-sorter), so its sort key is the
		/// effect's own, PSysManager::AddDrawing 0x6797D0
		glm::vec3 origin {0.0f};
	};
	/// Every collection made of Kind::Chain atoms, interpolated as Collect does
	void CollectChains(float t, std::vector<DrawChain>& out) const;
	[[nodiscard]] size_t AtomCount() const { return _atomCount; }

private:
	void CreateCollection(int group, Atom* parent, std::vector<std::unique_ptr<Collection>>& into);
	void UpdateCollection(Collection& collection);
	void PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation,
	                const glm::vec3& parentScale);
	void CollectCollection(const Collection& collection, float t, std::vector<DrawAtom>& out, Creator::Kind kind) const;
	void CollectChainsOf(const Collection& collection, float t, std::vector<DrawChain>& out) const;
	[[nodiscard]] bool AnyCreatorLeft(const Collection& collection) const;

	std::shared_ptr<const File> _file;
	std::vector<std::unique_ptr<Modifier>> _modifiers; ///< in file order
	std::array<std::vector<const Modifier*>, 25> _groups;
	std::array<bool, 25> _hierarchies {};
	std::unordered_map<std::string, std::unique_ptr<Creator>> _creators;
	std::unordered_map<std::string, float> _floatValues; ///< float providers of this step (fp+0xC)
	std::vector<std::unique_ptr<Collection>> _roots;
	glm::vec3 _origin;
	float _magnitude;
	float _age {0.0f};
	float _closeAge {0.0f};
	float _dt {0.1f};
	bool _closing {false};
	bool _deleteOnCloseDown {true};
	float _maxSpellAge {-1.0f};
	size_t _atomCount {0};
	std::mt19937 _random;
	SpellSink* _sink {nullptr};
	ProcessInfo _info {};
	std::vector<entt::entity> _targets;
	std::vector<glm::vec3> _targetPoints;
	int _player {-1};
	float _globalAlpha {255.0f};
	glm::vec3 _direction {0.0f};
};

} // namespace openblack::psys
