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

#include <vector>

#include <glm/vec3.hpp>

/// LH3DZSorter: the original's single queue of everything blended in the world view (models with alpha and fading ones,
/// sprites, mists and clouds, chimney smoke, particle effects, rain tiles, the hand...). Every caller sends one Z object
/// with its own key (NewZObject 0x83F310); StartFrame empties the queue (fn_0083F3B0, from 0x82F1F9) and FinishFrame
/// draws it once, far to near, after everything drawn at once in the frame (fn_0082F280, from 0x82F480). Report:
/// tmp_dis\unify2\lh3d_zsorter_original.md.
///
/// The original's queue is a linked list in a static buffer of 0x800 entries of 0x18 bytes (0xEDDD30; count 0xEE9D34,
/// head 0xEE9D38); the LH3DZSorter object g_zsorter (0xECA648, made by fn_0083F2B0 in OpenD3D 0x82CDC1) is never read,
/// so it is not ported. Queue keeps the same list, with the caller's own payload in place of the (object, callback) pair.
namespace openblack::graphics::zsorter
{

/// 0x83F315: cmp [0xEE9D34], 0x800. A full queue drops the new entry, whatever its key (0x83F31C)
constexpr uint32_t k_Capacity = 0x800;

/// The order in which a caller adds the squares. The original inlines LH3DTech::GetValueForZSorter (Mac 0x010E7360) in
/// every caller and the x87 sum follows the order of its loads: with the FPU at 24 bits (fn_007DEE00, and 0xFCFF at
/// 0x7DEE0D) each step rounds to a float, so the two orders can differ in the last bit. fn_007DEE00 runs from
/// InitOneTimeOnly and from Process3dEngine after FinishFrame (0x54E426). (inferido) that nothing between it and the
/// AddDrawing calls sets the precision back (D3D7 without FPUPRESERVE also leaves it at 24 bits); the research
/// (lh3d_zsorter_original.md §3.5) had assumed extended precision
enum class SumOrder : uint8_t
{
	/// (x^2 + y^2) + z^2: z, y, x loaded (LH3DSprite::AddDrawing 0x840C70, LH3DMist::AddDrawing 0x7FA83C..0x7FA86B,
	/// LH3DSmoke::AddDrawing 0x7F8D3E..0x7F8D7E, PSysManager::AddDrawing 0x6797E5..0x679828, CHand::AddDrawing
	/// 0x46D1BD..0x46D1F3, fn_00813340 0x8133CF..0x813403, fn_00679F60 0x679F87..0x679FB7)
	XYZ,
	/// (x^2 + z^2) + y^2: y, z, x loaded (LH3DObject::AddDrawing fn_00815A70 0x815F0F..0x815F43, the rain fn_008341B0
	/// 0x834215..0x83426F)
	XZY,
};

/// LH3DTech::GetValueForZSorter: |point - g_camera|^2 in float (g_camera 0xEA1DB8/BC/C0), not the distance (the square
/// root keeps the order but merges keys that differ in the last bit)
[[nodiscard]] float Key(const glm::vec3& point, const glm::vec3& camera, SumOrder order = SumOrder::XYZ) noexcept;

/// The rain's user data K (fn_008341B0 0x834233..0x834259): ((alpha << 8) - ftol(z x -0.0125)) << 8 - ftol(x x
/// -0.0125), the constant at [0x9A3AC0]: alpha x 65536 + 256 trunc(z / 80) + trunc(x / 80) inside the map (outside
/// [0, 20480) the bytes run into each other, as in the original). The caller clamps alpha to <= 0xFF and queues nothing
/// with a negative one (0x8341B8, 0x8341CC)
[[nodiscard]] uint32_t PackRainUser(float x, float z, int32_t alpha) noexcept;

/// What the rain's callback 0x833F80 reads back from K ([0xEE9D30], 0x833F83): byte 0 the tile x, byte 1 the tile z
/// (each x 80, [0x8D060C], to get the tile's corner) and byte 2 ([0xEE9D32]) the alpha
struct RainUser
{
	int32_t tileX;
	int32_t tileZ;
	int32_t alpha;
};
[[nodiscard]] RainUser UnpackRainUser(uint32_t user) noexcept;

/// The queue. Item is what the caller needs to draw the entry later (the original keeps an object and a member function
/// to call on it)
template <typename Item>
class Queue
{
public:
	Queue() { _nodes.reserve(k_Capacity); }

	/// fn_0083F3B0 (0x83F3B0..0x83F3BC): head = 0, count = 0. StartFrame 0x82F123 also clears the "drained" flag
	/// [0xECA610]
	void Begin() noexcept
	{
		_nodes.clear();
		_head = k_None;
		_drained = false;
		_dropped = 0;
	}

	/// LH3DZSorter::NewZObject 0x83F310. The entry goes before the first one whose key is strictly smaller (fld cur.key;
	/// fcomp key; test ah, 1 at 0x83F36A..0x83F376), or last (0x83F39A): far to near, and with equal keys the one that
	/// came first is drawn first. The x87 compare also says "smaller" when either key is NaN (C0 set when unordered),
	/// which !(cur >= key) keeps. Returns false when the queue is full and the entry was dropped (0x83F315/0x83F31C)
	bool Submit(const Item& item, float key, uint32_t user = 0)
	{
		if (_nodes.size() >= k_Capacity)
		{
			++_dropped;
			return false;
		}
		const auto added = static_cast<uint32_t>(_nodes.size());
		_nodes.push_back({item, user, key, k_None});
		uint32_t previous = k_None;
		for (uint32_t current = _head; current != k_None; current = _nodes[current].next)
		{
			if (!(_nodes[current].key >= key))
			{
				_nodes[added].next = current;
				(previous == k_None ? _head : _nodes[previous].next) = added;
				return true;
			}
			previous = current;
		}
		(previous == k_None ? _head : _nodes[previous].next) = added;
		return true;
	}

	/// An entry as the drain hands it out: the caller's item, its user data K and its key
	struct Entry
	{
		const Item* item;
		uint32_t user;
		float key;
	};

	/// fn_0082F280 (from FinishFrame 0x82F480, only while [0xECA610] is 0): [0xECA610] = 1, then from the head (the
	/// farthest) to the end, the user data copied to [0xEE9D30] (0x82F29D) and the callback called. Here the entries
	/// come back in that order for the caller to draw each one (its callback), with K in Entry::user; empty when the
	/// queue was already drained since Begin. The queue is not emptied (Begin does that) and no render state is
	/// touched: each callback sets its own. (inferido) the guard against a second drain: nothing else sets [0xECA610]
	[[nodiscard]] std::vector<Entry> Drain()
	{
		if (_drained)
		{
			return {};
		}
		_drained = true;
		return Ordered();
	}

	/// The entries in draw order, without draining (for traces)
	[[nodiscard]] std::vector<Entry> Ordered() const
	{
		std::vector<Entry> ordered;
		ordered.reserve(_nodes.size());
		for (uint32_t current = _head; current != k_None; current = _nodes[current].next)
		{
			ordered.push_back({&_nodes[current].item, _nodes[current].user, _nodes[current].key});
		}
		return ordered;
	}

	[[nodiscard]] uint32_t Size() const noexcept { return static_cast<uint32_t>(_nodes.size()); }
	[[nodiscard]] bool Empty() const noexcept { return _nodes.empty(); }
	/// (openblack) how many entries the full queue dropped since Begin; the original keeps no count
	[[nodiscard]] uint32_t Dropped() const noexcept { return _dropped; }

private:
	static constexpr uint32_t k_None = UINT32_MAX;
	/// +0x04 object and +0x08 callback (Item), +0x0C K (user), +0x10 key, +0x14 next; +0x00 is never written
	struct Node
	{
		Item item;
		uint32_t user;
		float key;
		uint32_t next;
	};
	std::vector<Node> _nodes; ///< 0xEDDD30, the count is its size (0xEE9D34)
	uint32_t _head {k_None};  ///< 0xEE9D38
	bool _drained {false};    ///< 0xECA610
	uint32_t _dropped {0};
};

} // namespace openblack::graphics::zsorter
