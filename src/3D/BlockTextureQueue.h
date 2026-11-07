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

#include <optional>
#include <span>
#include <vector>

#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The land's queue of block textures to paint again, worked a little every frame. A block whose texture is out of
/// date is asked for when it is drawn; each frame the requests are sorted by their distance from the camera, the
/// nearest first, and the first few paint some of their block's strips (one strip is one row of 16 cells along the
/// block's x); after the last strip a request puts in the prints over the block and then hands the finished texture
/// over in one go, so a block keeps showing its old texture until then. Pure logic: what is painted, and how, is the
/// caller's (Steps). docs/wiki/world/terrain.md#the-block-texture-queue
namespace openblack::block_texture
{

/// A block's texture is painted in this many strips, one row of cells along the block's x each
constexpr int32_t k_StripsPerBlock = 16;
/// The requests worked in one frame, the nearest one counted once though it is worked twice
constexpr int32_t k_RequestsPerFrame = 6;
/// At this detail index (0..4) a request paints all its strips the first time it is worked
constexpr int32_t k_AllStripsDetailIndex = 4;
/// A request's distance is taken from the block's centre at ground level: its map position plus this on x and z
constexpr float k_RequestCentreOffset = 80.0f;

/// One block waiting for its texture
struct TextureRequest
{
	/// The block, as the caller numbers them
	uint16_t block {};
	/// The block's centre at ground level: (map x + 80, 0, map z + 80)
	glm::vec3 centre {};
	/// Its distance from the camera, worked out again at the start of every frame
	float distance {};
	/// The next strip to paint, 0..15; 16 once all are painted (the prints come next) and 17 or more once the texture
	/// is finished
	int32_t strip {};
};

/// What one frame asks the caller to do, in order
enum class StepKind : uint8_t
{
	/// Paint strip `strip` of the block into its request's own texture, from the land as it is now
	Strip,
	/// Put the prints over the block's whole request texture (after its last strip)
	Prints,
	/// The request is finished: its texture replaces the block's, and the request is gone
	Finish,
};

struct Step
{
	StepKind kind {};
	uint16_t block {};
	/// The strip, for StepKind::Strip. A request that changed to painting all its strips at once part way through
	/// asks for strips from 16 up as well, which hold nothing
	int32_t strip {};
};

/// The distance the queue sorts by: sqrt((dz dz + dy dy) + dx dx) from the eye to the centre, each operation rounded to
/// a float
[[nodiscard]] float RequestDistance(glm::vec3 centre, glm::vec3 eye);

class TextureQueue
{
public:
	/// Asks for a block's texture. A block already waiting starts again from its first strip, in its place; any other
	/// goes in front of the others, from its first strip, centred on blockMapPos + 80 on x and z
	void Request(uint16_t block, glm::vec2 blockMapPos);

	/// One frame's work, its steps appended to `out` in order. Every request's distance from the eye is worked out
	/// again and the requests are sorted by it, the nearest first: adjacent requests swap while the first is farther,
	/// strictly, so equal distances keep their order. Then the nearest request is worked once, and the requests from
	/// the nearest on are worked once each, at most k_RequestsPerFrame of them; working a request paints its next strip
	/// (all its strips from that one on at k_AllStripsDetailIndex), or after its last strip puts in its prints. The
	/// first request worked past its prints finishes, and nothing more is worked that frame
	void Update(glm::vec3 eye, int32_t detailIndex, std::vector<Step>& out);

	/// The requests, the next to be worked first (until the next Update sorts them again)
	[[nodiscard]] std::span<const TextureRequest> Requests() const { return _requests; }
	/// The block is waiting
	[[nodiscard]] bool Has(uint16_t block) const;
	/// Every request dropped, as the land's release drops them
	void Clear() { _requests.clear(); }

private:
	/// Works one request: its next strip, all its strips from there, or its prints; the strip counter moves on
	static void Work(TextureRequest& request, int32_t detailIndex, std::vector<Step>& out);

	std::vector<TextureRequest> _requests;
};

/// Each block's two marks between the land and the queue: its texture is out of date, and it has been drawn since the
/// land opened. A block's first paint in the original comes with its first draw, after every change before it, so a
/// change of a block not drawn yet is painted at once; a drawn block waits for the queue and shows its old texture
class BlockMarks
{
public:
	/// The land opens with every block painted: none out of date, none drawn
	void Reset(size_t blocks);
	/// The block's cells changed: its texture is out of date
	void Changed(size_t block);
	/// After the changes: the block is to be painted at once, being out of date and not drawn yet; then it is no
	/// longer out of date
	[[nodiscard]] bool TakePaintNow(size_t block);
	/// The block is drawn: it is to be asked for, being out of date; then it is no longer out of date
	[[nodiscard]] bool TakeDrawn(size_t block);
	/// Every block out of date
	void AllOutOfDate();
	/// The block has been drawn since the land opened
	[[nodiscard]] bool Drawn(size_t block) const { return block < _drawn.size() && _drawn[block] != 0; }

private:
	std::vector<uint8_t> _outOfDate;
	std::vector<uint8_t> _drawn;
};

/// The land's static shadows fall along the default sun: a point h above an object's base lands h further on x and on z
constexpr float k_ShadowSlopeX = 1.0f;
constexpr float k_ShadowSlopeZ = 1.0f;
/// Metres to block places: a block is 160 m wide (1 / 160 as a float)
constexpr float k_BlocksPerMetre = 0.00625f;
/// The highest block place on either axis that a marked rectangle is clamped to
constexpr int32_t k_LastBlockPlace = 0x1FF;

/// What an object put on the map's cells or taken off them has over the land, for the blocks it marks out of date
struct MarkingObject
{
	/// Its place on the map, x and z
	glm::vec2 position {};
	/// Its turn about the vertical and its scale: its matrix is made from them again, the scale turned
	float yAngle {};
	float scale {1.0f};
	/// Its shadow goes into the land's textures: its mesh's box counts, cast along the sun
	bool shadow {};
	/// Its footprint goes into them: its mesh's footprint counts
	bool footprint {};
	/// The mesh's box: its centre and its half size
	glm::vec3 boxCentre {};
	glm::vec3 boxHalfSize {};
	/// The corners of the mesh's footprint triangles, x and z on the mesh; none when the mesh has no footprint
	std::span<const glm::vec2> footprintCorners;
};

/// A rectangle of the block grid, by block place (map x and z / 160), both ends in
struct BlockRange
{
	glm::ivec2 min {};
	glm::ivec2 max {};
};

/// The blocks an object marks out of date, as the original works them out when an object is put on the map's cells or
/// taken off them: none unless its shadow or its footprint goes into the land's textures. The object's matrix is its
/// scale turned by its angle. The rectangle starts at its position; with its shadow it takes in the eight corners of its
/// mesh's box through the matrix, each moved along the sun by its height over the object's base; with its footprint the
/// corners of the footprint's triangles through the matrix (x and z only). Each operation is rounded to a float in the
/// original's order. The sides x 1/160 are truncated toward zero; a lower side below 0 becomes 0, an upper side outside
/// 0..0x1FF (read unsigned, so a negative one too) becomes 0x1FF. The caller marks the blocks of the rectangle that
/// exist; min > max is an empty rectangle
[[nodiscard]] std::optional<BlockRange> BlocksMarkedBy(const MarkingObject& object);

} // namespace openblack::block_texture
