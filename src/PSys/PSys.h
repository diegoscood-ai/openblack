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
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "PSysFile.h"

// The original's generic particle system (PSysManager, AtomCollection, AtomCore, the modifier classes of the spell
// files). Report: dev\tmp_dis\psys\psys_report.md; wiki: rendering.md, "Partículas".

namespace openblack::psys
{
class Effect;
struct Collection;
class Modifier;

/// ParticleCreator (fn_006A85E0) and its sprite form (ParticleSpriteCreator, 0x6AA0D0); the other kinds (point, mesh,
/// mist, chain, light map...) are not drawn yet.
struct Creator
{
	enum class Kind
	{
		Point,
		Sprite,
		Other,
	};
	Kind kind {Kind::Point};
	std::string className;
	uint8_t r {255}, g {255}, b {255}, a {255};
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
};

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
	Collection* collection {nullptr};
	const Creator* creator {nullptr};
	glm::vec3 position {0.0f}; ///< +0x80, local to the parent atom in a hierarchy
	glm::vec3 velocity {0.0f}; ///< +0x34
	glm::mat3 rotation {1.0f}; ///< +0x44
	float baseScale {1.0f};    ///< +0x74 (InitialScale)
	float ruleScale {1.0f};    ///< +0x78
	float stretch {1.0f};      ///< +0x7C
	std::array<uint8_t, 4> colour {255, 255, 255, 255}; ///< +0x8C ARGB as r, g, b, a
	float birth {0.0f};
	bool visible {true}; ///< flag 0x10 (EventConditionAtomInUse)
	float frame {0.0f};
	float frameRate {0.0f};
	float gravity {1.0f}; ///< +0x11C
	uint32_t random {0};  ///< +0x12C
	DrawState previous;
	DrawState current;
	bool drawn {false};
	std::vector<std::unique_ptr<Collection>> subCollections;
	/// per-modifier data of this atom (latches, phases)
	std::unordered_map<const Modifier*, glm::vec4> data;
};

/// AtomCollection (0x54 bytes): one live instance of a group
struct Collection
{
	int group {0};
	Atom* parent {nullptr};
	float birth {0.0f};
	float alpha {255.0f}; ///< +0x50
	bool hierarchy {false};
	std::vector<std::unique_ptr<Atom>> atoms;
	struct Slot
	{
		const Modifier* modifier {nullptr};
		bool attached {true};
		glm::vec4 state {0.0f}; ///< per-collection data (+0x24): emitter timing and counts
		bool first {true};
	};
	std::vector<Slot> modifiers;
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
	[[nodiscard]] glm::vec3 GetOrigin() const { return _origin; }
	[[nodiscard]] float GetAge() const { return _age; }
	[[nodiscard]] float GetCloseAge() const { return _closeAge; }
	[[nodiscard]] float GetMagnitude() const { return _magnitude; }
	[[nodiscard]] float GetDt() const { return _dt; }
	[[nodiscard]] const File& GetFile() const { return *_file; }

	// used by the modifiers
	[[nodiscard]] float Random(float max);            ///< PSysFloatRand, [0, max)
	[[nodiscard]] glm::vec3 RandomInBall();            ///< PSysRandR3
	[[nodiscard]] float FloatProvider(const std::string& name, float fallback) const;
	[[nodiscard]] bool ConditionForCollection(const std::string& name, const Collection& collection) const;
	[[nodiscard]] bool ConditionForAtom(const std::string& name, const Atom& atom) const;
	[[nodiscard]] const Creator* FindCreator(const std::string& name) const;
	/// CommonInitNewAtom 0x674C60 + the creator's init: a new atom in the collection, its NextGroups sub-collections
	Atom& NewAtom(Collection& collection, const Creator* creator, const std::vector<int>& nextGroups);
	[[nodiscard]] glm::vec3 SpawnPosition(const Collection& collection) const;
	[[nodiscard]] glm::vec3 GlobalPosition(const Atom& atom) const;
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
	};
	void Collect(float t, std::vector<DrawAtom>& out) const;
	[[nodiscard]] size_t AtomCount() const { return _atomCount; }

private:
	void CreateCollection(int group, Atom* parent, std::vector<std::unique_ptr<Collection>>& into);
	void UpdateCollection(Collection& collection);
	void PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation);
	void CollectCollection(const Collection& collection, float t, std::vector<DrawAtom>& out) const;
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
};

} // namespace openblack::psys
