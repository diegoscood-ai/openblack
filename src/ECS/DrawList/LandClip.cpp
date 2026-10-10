/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandClip.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>

#include "3D/AffineMatrix.h"

namespace openblack::ecs::draw_list
{

namespace
{
/// Cells along a block's side
constexpr size_t k_CellsPerSide = k_BlockSide - 1;
/// The side of a cell in world units
constexpr float k_CellSide = 10.0f;
/// Altitudes up to this one are drawn at height 0
constexpr uint8_t k_HighestFlatAltitude = 3;
/// The cell flag of open water whose land is not drawn
constexpr uint8_t k_UndrawnWaterFlag = 0x02;
/// The grid's coarsest step, as a shift: 1 << 2 = 4 cells
constexpr uint32_t k_CoarsestMeshLod = 2;
/// Seam bits run from 0 to 15
constexpr uint32_t k_SeamBitsCount = 16;

/// The edges each set of seam bits leaves out. Only these eight have an entry; the original's table is zero elsewhere
constexpr std::array<SeamEdges, k_SeamBitsCount> k_SeamEdges = [] {
	std::array<SeamEdges, k_SeamBitsCount> edges {};
	edges[k_SeamLowerX] = {.fans = true, .skipFirstRow = true};
	edges[k_SeamLowerZ] = {.fans = true, .skipFirstColumn = true};
	edges[k_SeamLowerX | k_SeamLowerZ] = {.fans = true, .skipFirstRow = true, .skipFirstColumn = true};
	edges[k_SeamHigherZ] = {.fans = true, .skipLastColumn = true};
	edges[k_SeamLowerX | k_SeamHigherZ] = {.fans = true, .skipFirstRow = true, .skipLastColumn = true};
	edges[k_SeamHigherX] = {.fans = true, .skipLastRow = true};
	edges[k_SeamHigherX | k_SeamLowerZ] = {.fans = true, .skipLastRow = true, .skipFirstColumn = true};
	edges[k_SeamHigherX | k_SeamHigherZ] = {.fans = true, .skipLastRow = true, .skipLastColumn = true};
	return edges;
}();

// The seam triangles, as the original's tables hold them once a land is created: points of the 17-point grid at step 0
// (Full) and of the 9-point grid at step 1 (Half), named by the sides whose neighbour is coarser
constexpr std::array<SeamTriangle, 24> k_FullFanLowerX {
    {{0, 2, 18},   {0, 18, 17},  {2, 19, 18},  {2, 4, 20},   {2, 20, 19},  {4, 21, 20},  {4, 6, 22},   {4, 22, 21},
     {6, 23, 22},  {6, 8, 24},   {6, 24, 23},  {8, 25, 24},  {8, 10, 26},  {8, 26, 25},  {10, 27, 26}, {10, 12, 28},
     {10, 28, 27}, {12, 29, 28}, {12, 14, 30}, {12, 30, 29}, {14, 31, 30}, {14, 16, 32}, {14, 32, 31}, {16, 33, 32}}};
constexpr std::array<SeamTriangle, 24> k_FullFanHigherX {
    {{256, 274, 272}, {255, 256, 272}, {256, 257, 274}, {258, 276, 274}, {257, 258, 274}, {258, 259, 276},
     {260, 278, 276}, {259, 260, 276}, {260, 261, 278}, {262, 280, 278}, {261, 262, 278}, {262, 263, 280},
     {264, 282, 280}, {263, 264, 280}, {264, 265, 282}, {266, 284, 282}, {265, 266, 282}, {266, 267, 284},
     {268, 286, 284}, {267, 268, 284}, {268, 269, 286}, {270, 288, 286}, {269, 270, 286}, {270, 271, 288}}};
constexpr std::array<SeamTriangle, 24> k_FullFanLowerZ {
    {{0, 18, 34},     {0, 1, 18},      {18, 35, 34},    {34, 52, 68},    {34, 35, 52},    {52, 69, 68},
     {68, 86, 102},   {68, 69, 86},    {86, 103, 102},  {102, 120, 136}, {102, 103, 120}, {120, 137, 136},
     {136, 154, 170}, {136, 137, 154}, {154, 171, 170}, {170, 188, 204}, {170, 171, 188}, {188, 205, 204},
     {204, 222, 238}, {204, 205, 222}, {222, 239, 238}, {238, 256, 272}, {238, 239, 256}, {256, 273, 272}}};
constexpr std::array<SeamTriangle, 24> k_FullFanHigherZ {
    {{16, 50, 32},    {15, 16, 32},    {32, 50, 49},    {50, 84, 66},    {49, 50, 66},    {66, 84, 83},
     {84, 118, 100},  {83, 84, 100},   {100, 118, 117}, {118, 152, 134}, {117, 118, 134}, {134, 152, 151},
     {152, 186, 168}, {151, 152, 168}, {168, 186, 185}, {186, 220, 202}, {185, 186, 202}, {202, 220, 219},
     {220, 254, 236}, {219, 220, 236}, {236, 254, 253}, {254, 288, 270}, {253, 254, 270}, {270, 288, 287}}};
constexpr std::array<SeamTriangle, 46> k_FullFanLowerXLowerZ {
    {{0, 2, 18},      {2, 19, 18},     {2, 4, 20},      {2, 20, 19},     {4, 21, 20},     {4, 6, 22},      {4, 22, 21},
     {6, 23, 22},     {6, 8, 24},      {6, 24, 23},     {8, 25, 24},     {8, 10, 26},     {8, 26, 25},     {10, 27, 26},
     {10, 12, 28},    {10, 28, 27},    {12, 29, 28},    {12, 14, 30},    {12, 30, 29},    {14, 31, 30},    {14, 16, 32},
     {14, 32, 31},    {16, 33, 32},    {0, 18, 34},     {18, 35, 34},    {34, 52, 68},    {34, 35, 52},    {52, 69, 68},
     {68, 86, 102},   {68, 69, 86},    {86, 103, 102},  {102, 120, 136}, {102, 103, 120}, {120, 137, 136}, {136, 154, 170},
     {136, 137, 154}, {154, 171, 170}, {170, 188, 204}, {170, 171, 188}, {188, 205, 204}, {204, 222, 238}, {204, 205, 222},
     {222, 239, 238}, {238, 256, 272}, {238, 239, 256}, {256, 273, 272}}};
constexpr std::array<SeamTriangle, 46> k_FullFanLowerXHigherZ {
    {{0, 2, 18},      {0, 18, 17},     {2, 19, 18},     {2, 4, 20},      {2, 20, 19},     {4, 21, 20},     {4, 6, 22},
     {4, 22, 21},     {6, 23, 22},     {6, 8, 24},      {6, 24, 23},     {8, 25, 24},     {8, 10, 26},     {8, 26, 25},
     {10, 27, 26},    {10, 12, 28},    {10, 28, 27},    {12, 29, 28},    {12, 14, 30},    {12, 30, 29},    {14, 31, 30},
     {14, 16, 32},    {14, 32, 31},    {16, 50, 32},    {32, 50, 49},    {50, 84, 66},    {49, 50, 66},    {66, 84, 83},
     {84, 118, 100},  {83, 84, 100},   {100, 118, 117}, {118, 152, 134}, {117, 118, 134}, {134, 152, 151}, {152, 186, 168},
     {151, 152, 168}, {168, 186, 185}, {186, 220, 202}, {185, 186, 202}, {202, 220, 219}, {220, 254, 236}, {219, 220, 236},
     {236, 254, 253}, {254, 288, 270}, {253, 254, 270}, {270, 288, 287}}};
constexpr std::array<SeamTriangle, 46> k_FullFanHigherXLowerZ {
    {{256, 274, 272}, {256, 257, 274}, {258, 276, 274}, {257, 258, 274}, {258, 259, 276}, {260, 278, 276}, {259, 260, 276},
     {260, 261, 278}, {262, 280, 278}, {261, 262, 278}, {262, 263, 280}, {264, 282, 280}, {263, 264, 280}, {264, 265, 282},
     {266, 284, 282}, {265, 266, 282}, {266, 267, 284}, {268, 286, 284}, {267, 268, 284}, {268, 269, 286}, {270, 288, 286},
     {269, 270, 286}, {270, 271, 288}, {0, 18, 34},     {0, 1, 18},      {18, 35, 34},    {34, 52, 68},    {34, 35, 52},
     {52, 69, 68},    {68, 86, 102},   {68, 69, 86},    {86, 103, 102},  {102, 120, 136}, {102, 103, 120}, {120, 137, 136},
     {136, 154, 170}, {136, 137, 154}, {154, 171, 170}, {170, 188, 204}, {170, 171, 188}, {188, 205, 204}, {204, 222, 238},
     {204, 205, 222}, {222, 239, 238}, {238, 256, 272}, {238, 239, 256}}};
constexpr std::array<SeamTriangle, 46> k_FullFanHigherXHigherZ {
    {{256, 274, 272}, {255, 256, 272}, {256, 257, 274}, {258, 276, 274}, {257, 258, 274}, {258, 259, 276}, {260, 278, 276},
     {259, 260, 276}, {260, 261, 278}, {262, 280, 278}, {261, 262, 278}, {262, 263, 280}, {264, 282, 280}, {263, 264, 280},
     {264, 265, 282}, {266, 284, 282}, {265, 266, 282}, {266, 267, 284}, {268, 286, 284}, {267, 268, 284}, {268, 269, 286},
     {270, 288, 286}, {269, 270, 286}, {16, 50, 32},    {15, 16, 32},    {32, 50, 49},    {50, 84, 66},    {49, 50, 66},
     {66, 84, 83},    {84, 118, 100},  {83, 84, 100},   {100, 118, 117}, {118, 152, 134}, {117, 118, 134}, {134, 152, 151},
     {152, 186, 168}, {151, 152, 168}, {168, 186, 185}, {186, 220, 202}, {185, 186, 202}, {202, 220, 219}, {220, 254, 236},
     {219, 220, 236}, {236, 254, 253}, {254, 288, 270}, {253, 254, 270}}};
constexpr std::array<SeamTriangle, 12> k_HalfFanLowerX {{{0, 2, 10},
                                                         {0, 10, 9},
                                                         {2, 11, 10},
                                                         {2, 4, 12},
                                                         {2, 12, 11},
                                                         {4, 13, 12},
                                                         {4, 6, 14},
                                                         {4, 14, 13},
                                                         {6, 15, 14},
                                                         {6, 8, 16},
                                                         {6, 16, 15},
                                                         {8, 17, 16}}};
constexpr std::array<SeamTriangle, 12> k_HalfFanHigherX {{{64, 74, 72},
                                                          {63, 64, 72},
                                                          {64, 65, 74},
                                                          {66, 76, 74},
                                                          {65, 66, 74},
                                                          {66, 67, 76},
                                                          {68, 78, 76},
                                                          {67, 68, 76},
                                                          {68, 69, 78},
                                                          {70, 80, 78},
                                                          {69, 70, 78},
                                                          {70, 71, 80}}};
constexpr std::array<SeamTriangle, 12> k_HalfFanLowerZ {{{0, 10, 18},
                                                         {0, 1, 10},
                                                         {10, 19, 18},
                                                         {18, 28, 36},
                                                         {18, 19, 28},
                                                         {28, 37, 36},
                                                         {36, 46, 54},
                                                         {36, 37, 46},
                                                         {46, 55, 54},
                                                         {54, 64, 72},
                                                         {54, 55, 64},
                                                         {64, 73, 72}}};
constexpr std::array<SeamTriangle, 12> k_HalfFanHigherZ {{{8, 26, 16},
                                                          {7, 8, 16},
                                                          {16, 26, 25},
                                                          {26, 44, 34},
                                                          {25, 26, 34},
                                                          {34, 44, 43},
                                                          {44, 62, 52},
                                                          {43, 44, 52},
                                                          {52, 62, 61},
                                                          {62, 80, 70},
                                                          {61, 62, 70},
                                                          {70, 80, 79}}};
constexpr std::array<SeamTriangle, 22> k_HalfFanLowerXLowerZ {
    {{0, 2, 10},   {2, 11, 10},  {2, 4, 12},   {2, 12, 11},  {4, 13, 12},  {4, 6, 14},   {4, 14, 13},  {6, 15, 14},
     {6, 8, 16},   {6, 16, 15},  {8, 17, 16},  {0, 10, 18},  {10, 19, 18}, {18, 28, 36}, {18, 19, 28}, {28, 37, 36},
     {36, 46, 54}, {36, 37, 46}, {46, 55, 54}, {54, 64, 72}, {54, 55, 64}, {64, 73, 72}}};
constexpr std::array<SeamTriangle, 22> k_HalfFanLowerXHigherZ {
    {{0, 2, 10},   {0, 10, 9},   {2, 11, 10},  {2, 4, 12},   {2, 12, 11},  {4, 13, 12},  {4, 6, 14},   {4, 14, 13},
     {6, 15, 14},  {6, 8, 16},   {6, 16, 15},  {8, 26, 16},  {16, 26, 25}, {26, 44, 34}, {25, 26, 34}, {34, 44, 43},
     {44, 62, 52}, {43, 44, 52}, {52, 62, 61}, {62, 80, 70}, {61, 62, 70}, {70, 80, 79}}};
constexpr std::array<SeamTriangle, 22> k_HalfFanHigherXLowerZ {
    {{64, 74, 72}, {64, 65, 74}, {66, 76, 74}, {65, 66, 74}, {66, 67, 76}, {68, 78, 76}, {67, 68, 76}, {68, 69, 78},
     {70, 80, 78}, {69, 70, 78}, {70, 71, 80}, {0, 10, 18},  {0, 1, 10},   {10, 19, 18}, {18, 28, 36}, {18, 19, 28},
     {28, 37, 36}, {36, 46, 54}, {36, 37, 46}, {46, 55, 54}, {54, 64, 72}, {54, 55, 64}}};
constexpr std::array<SeamTriangle, 22> k_HalfFanHigherXHigherZ {
    {{64, 74, 72}, {63, 64, 72}, {64, 65, 74}, {66, 76, 74}, {65, 66, 74}, {66, 67, 76}, {68, 78, 76}, {67, 68, 76},
     {68, 69, 78}, {70, 80, 78}, {69, 70, 78}, {8, 26, 16},  {7, 8, 16},   {16, 26, 25}, {26, 44, 34}, {25, 26, 34},
     {34, 44, 43}, {44, 62, 52}, {43, 44, 52}, {52, 62, 61}, {62, 80, 70}, {61, 62, 70}}};

/// One step's seam triangles, by the sides they run along
struct SeamFans
{
	std::span<const SeamTriangle> lowerX;
	std::span<const SeamTriangle> lowerXHigherZ;
	std::span<const SeamTriangle> lowerXLowerZ;
	std::span<const SeamTriangle> higherX;
	std::span<const SeamTriangle> higherXHigherZ;
	std::span<const SeamTriangle> higherXLowerZ;
	std::span<const SeamTriangle> higherZ;
	std::span<const SeamTriangle> lowerZ;
};

constexpr SeamFans k_FullFans {.lowerX = k_FullFanLowerX,
                               .lowerXHigherZ = k_FullFanLowerXHigherZ,
                               .lowerXLowerZ = k_FullFanLowerXLowerZ,
                               .higherX = k_FullFanHigherX,
                               .higherXHigherZ = k_FullFanHigherXHigherZ,
                               .higherXLowerZ = k_FullFanHigherXLowerZ,
                               .higherZ = k_FullFanHigherZ,
                               .lowerZ = k_FullFanLowerZ};
constexpr SeamFans k_HalfFans {.lowerX = k_HalfFanLowerX,
                               .lowerXHigherZ = k_HalfFanLowerXHigherZ,
                               .lowerXLowerZ = k_HalfFanLowerXLowerZ,
                               .higherX = k_HalfFanHigherX,
                               .higherXHigherZ = k_HalfFanHigherXHigherZ,
                               .higherXLowerZ = k_HalfFanHigherXLowerZ,
                               .higherZ = k_HalfFanHigherZ,
                               .lowerZ = k_HalfFanLowerZ};

/// What the x87's float to integer store gives: the nearest integer, ties to even (its default rounding), and -2^31 for
/// a NaN or a value out of range
[[nodiscard]] int32_t StoreAsInteger(float value)
{
	constexpr float k_Limit = 2147483648.0f;
	if (!(value >= -k_Limit && value < k_Limit))
	{
		return std::numeric_limits<int32_t>::min();
	}
	const double exact = value;
	double rounded = std::floor(exact);
	const double fraction = exact - rounded;
	if (fraction > 0.5 || (fraction == 0.5 && std::fmod(rounded, 2.0) != 0.0))
	{
		rounded += 1.0;
	}
	return static_cast<int32_t>(rounded);
}

/// The sides of the view a point can be outside of. A triangle is clipped by them in this order, from the near side
/// down to the bottom one
constexpr uint32_t k_OutNear = 0x20;
constexpr uint32_t k_OutRight = 0x10;
constexpr uint32_t k_OutLeft = 0x08;
constexpr uint32_t k_OutTop = 0x04;
constexpr uint32_t k_OutBottom = 0x02;

/// The most points one triangle's clip adds: each of the five sides cuts every piece made so far (at most 1, 2, 4, 8
/// and 16 pieces), with two new points a cut
constexpr size_t k_MaxCutPoints = 2 * (1 + 2 + 4 + 8 + 16);

/// One point of the block's grid, or one made by a cut
struct ClipPoint
{
	/// Outside no side: the clamped screen position, and w the near clip over the depth. Otherwise X, Y and w the depth
	float x {};
	float y {};
	float w {};
	/// The k_Out* sides it is outside of
	uint32_t outside {};
};

[[nodiscard]] float ClampToScreen(float value, float max)
{
	// below 0, and NaN, give 0
	if (!(value >= 0.0f))
	{
		return 0.0f;
	}
	return value > max ? max : value;
}

/// The screen area test, on the points in the order they are emitted
[[nodiscard]] bool AreaAboveZero(const ClipPoint& p0, const ClipPoint& p1, const ClipPoint& p2)
{
	const float area = (p2.y - p0.y) * (p1.x - p0.x) - (p1.y - p0.y) * (p2.x - p0.x);
	return area > 0.0f;
}

/// How far inside a side a point is (X, Y and the depth): above 0 inside, below 0 outside
[[nodiscard]] float InsideBy(const ClipPoint& point, uint32_t side, float nearW)
{
	switch (side)
	{
	case k_OutNear:
		return point.w - nearW;
	case k_OutRight:
		return point.w - point.x;
	case k_OutLeft:
		return point.w + point.x;
	case k_OutTop:
		return point.w - point.y;
	default:
		// the bottom side; no point is ever outside a side after it
		return point.w + point.y;
	}
}

class LandClipper
{
public:
	LandClipper(const DrawCamera& camera, glm::vec2 maxScreen)
	    : _worldToClipping(camera.worldToClipping)
	    , _nearW(camera.nearW)
	    , _half(camera.half)
	    , _inverseHalf(1.0f / camera.half.x, 1.0f / camera.half.y)
	    , _maxScreen(maxScreen)
	{
	}

	/// The grid point of cell `index` at (x, h, z), with its sides only when the block is partly outside
	void SetGridPoint(size_t index, glm::vec3 world, bool partlyOutside)
	{
		// the same sums as the box test: the y and z terms first, x last
		const glm::vec3 clip = affine::ToClipForBoxTest(_worldToClipping, world);
		ClipPoint point {clip.x, clip.y, clip.z, 0};
		if (partlyOutside)
		{
			if (!(clip.z >= _nearW))
			{
				point.outside = k_OutNear;
			}
			if (clip.x > clip.z)
			{
				point.outside |= k_OutRight;
			}
			else if (-clip.z > clip.x)
			{
				point.outside |= k_OutLeft;
			}
			if (clip.y > clip.z)
			{
				point.outside |= k_OutTop;
			}
			else if (-clip.z > clip.y)
			{
				point.outside |= k_OutBottom;
			}
		}
		if (point.outside == 0)
		{
			const float r = 1.0f / clip.z;
			point.x = ClampToScreen((r * clip.x + 1.0f) * _half.x, _maxScreen.x);
			point.y = ClampToScreen(_half.y - (r * clip.y) * _half.y, _maxScreen.y);
			point.w = _nearW * r;
		}
		_points.at(index) = point;
	}

	[[nodiscard]] uint32_t Outside(size_t index) const { return _points.at(index).outside; }

	[[nodiscard]] bool FrontFacing(size_t a, size_t b, size_t c) const
	{
		return AreaAboveZero(_points.at(a), _points.at(b), _points.at(c));
	}

	/// Whether a triangle that is not all outside one side leaves a front-facing piece. The cut points of earlier
	/// triangles are never read again, so each triangle starts its cut points afresh
	[[nodiscard]] bool ClipKeepsFront(size_t a, size_t b, size_t c)
	{
		_count = k_BlockCells;
		return Clip(a, b, c, k_OutNear);
	}

private:
	/// The triangle (a, b, c) clipped by `side` and every side after it. A side that leaves two points of three inside
	/// gives two pieces: the first is clipped by the sides left at once, and the second carries on here
	[[nodiscard]] bool Clip(size_t a, size_t b, size_t c, uint32_t side)
	{
		while (side != 0)
		{
			const bool aOut = (Outside(a) & side) != 0;
			const bool bOut = (Outside(b) & side) != 0;
			const bool cOut = (Outside(c) & side) != 0;
			const uint32_t next = side >> 1;
			if (aOut && bOut && cOut)
			{
				return false;
			}
			if (aOut && bOut)
			{
				const size_t ca = Cut(side, c, a);
				const size_t cb = Cut(side, c, b);
				a = c;
				b = ca;
				c = cb;
			}
			else if (aOut && cOut)
			{
				const size_t ba = Cut(side, b, a);
				const size_t bc = Cut(side, b, c);
				a = ba;
				c = bc;
			}
			else if (aOut)
			{
				const size_t ba = Cut(side, b, a);
				const size_t ca = Cut(side, c, a);
				if (Clip(ba, b, c, next))
				{
					return true;
				}
				a = ba;
				b = c;
				c = ca;
			}
			else if (bOut && cOut)
			{
				const size_t ab = Cut(side, a, b);
				const size_t ac = Cut(side, a, c);
				b = ab;
				c = ac;
			}
			else if (bOut)
			{
				const size_t ab = Cut(side, a, b);
				const size_t cb = Cut(side, c, b);
				if (Clip(a, ab, c, next))
				{
					return true;
				}
				a = ab;
				b = cb;
			}
			else if (cOut)
			{
				const size_t ac = Cut(side, a, c);
				const size_t bc = Cut(side, b, c);
				if (Clip(ac, a, b, next))
				{
					return true;
				}
				a = ac;
				c = bc;
			}
			else if ((Outside(a) | Outside(b) | Outside(c)) == 0)
			{
				// nothing left to cut
				break;
			}
			side = next;
		}
		return FrontFacing(a, b, c);
	}

	/// The point where the edge from `inside` (inside `side`) to `outside` (outside it) crosses the side, with the
	/// sides after `side` that it is outside of, and on the screen when it is outside none
	[[nodiscard]] size_t Cut(uint32_t side, size_t inside, size_t outside)
	{
		ClipPoint from = _points.at(inside);
		if (from.outside == 0)
		{
			// back from the screen to X, Y and the depth
			const float depth = _nearW / from.w;
			from.x = ((from.x - _half.x) * _inverseHalf.x) * depth;
			from.y = ((_half.y - from.y) * _inverseHalf.y) * depth;
			from.w = depth;
		}
		const ClipPoint& to = _points.at(outside);
		const float fromInside = InsideBy(from, side, _nearW);
		const float toInside = InsideBy(to, side, _nearW);
		const float t = fromInside / (fromInside - toInside);
		const float s = 1.0f - t;
		ClipPoint point {t * to.x + s * from.x, s * from.y + t * to.y, s * from.w + t * to.w, 0};
		// each test on its own, not "otherwise"
		if (side > k_OutRight && point.x > point.w)
		{
			point.outside |= k_OutRight;
		}
		if (side > k_OutLeft && -point.w > point.x)
		{
			point.outside |= k_OutLeft;
		}
		if (side > k_OutTop && point.y > point.w)
		{
			point.outside |= k_OutTop;
		}
		if (side > k_OutBottom && -point.w > point.y)
		{
			point.outside |= k_OutBottom;
		}
		if (point.outside == 0)
		{
			// sy multiplies in another order than the grid points' does
			const float r = 1.0f / point.w;
			const float sx = (r * point.x + 1.0f) * _half.x;
			const float sy = _half.y - (r * _half.y) * point.y;
			point.x = ClampToScreen(sx, _maxScreen.x);
			point.y = ClampToScreen(sy, _maxScreen.y);
			point.w = r * _nearW;
		}
		_points.at(_count) = point;
		return _count++;
	}

	affine::AffineMatrix _worldToClipping;
	float _nearW;
	glm::vec2 _half;
	/// 1 / half, each rounded once
	glm::vec2 _inverseHalf;
	glm::vec2 _maxScreen;
	std::array<ClipPoint, k_BlockCells + k_MaxCutPoints> _points {};
	size_t _count {k_BlockCells};
};
} // namespace

std::optional<SeamEdges> SeamEdgesFor(uint32_t seamBits)
{
	if (seamBits >= k_SeamBitsCount)
	{
		return std::nullopt;
	}
	return k_SeamEdges.at(seamBits);
}

std::span<const SeamTriangle> SeamFan(uint32_t meshLod, uint32_t seamBits)
{
	const auto edges = SeamEdgesFor(seamBits);
	if (!edges.has_value() || !edges->fans || meshLod > 1)
	{
		return {};
	}
	const SeamFans& fans = meshLod == 0 ? k_FullFans : k_HalfFans;
	if ((seamBits & k_SeamLowerX) != 0)
	{
		if ((seamBits & k_SeamHigherZ) != 0)
		{
			return fans.lowerXHigherZ;
		}
		return (seamBits & k_SeamLowerZ) != 0 ? fans.lowerXLowerZ : fans.lowerX;
	}
	if ((seamBits & k_SeamHigherX) != 0)
	{
		if ((seamBits & k_SeamHigherZ) != 0)
		{
			return fans.higherXHigherZ;
		}
		return (seamBits & k_SeamLowerZ) != 0 ? fans.higherXLowerZ : fans.higherX;
	}
	if ((seamBits & k_SeamHigherZ) != 0)
	{
		return fans.higherZ;
	}
	if ((seamBits & k_SeamLowerZ) != 0)
	{
		return fans.lowerZ;
	}
	return {};
}

void BlendTowardsCoarserGrid(std::span<lnd::LNDCell, k_BlockCells> cells, glm::vec2 blockMapPos, const LodLine& line,
                             bool towardsHalfGrid)
{
	const size_t step = towardsHalfGrid ? 2 : 4;
	const size_t halfStep = step / 2;
	const float weightScale = 256.0f / line.width;
	const float halfWidth = line.width * 0.5f;
	const float stepLength = static_cast<float>(step) * k_CellSide;
	const float halfStepLength = static_cast<float>(halfStep) * k_CellSide;

	// one point, at x and z from the line's point, between the points a and b of the coarser grid
	const auto blend = [&](size_t point, size_t a, size_t b, float alongZTimesX, float z) {
		const float v = alongZTimesX - line.along.x * z;
		if (v >= halfWidth)
		{
			return;
		}
		const uint32_t middle = (static_cast<uint32_t>(cells[a].altitude) + cells[b].altitude) >> 1;
		if (-halfWidth >= v)
		{
			cells[point].altitude = static_cast<uint8_t>(middle);
			return;
		}
		// a NaN v ends here too
		const float weight = (halfWidth - v) * weightScale;
		const auto t = static_cast<uint32_t>(StoreAsInteger(weight));
		// the sums wrap as the original's 32-bit ones do, and the low byte is kept
		const uint32_t blended = ((256u - t) * cells[point].altitude + t * middle) >> 8;
		cells[point].altitude = static_cast<uint8_t>(blended);
	};

	// along the rows of the coarser grid, between the points either side
	float x = blockMapPos.x - line.point.x;
	for (size_t row = 0; row < k_BlockSide; row += step)
	{
		const float alongZTimesX = line.along.z * x;
		float z = (halfStepLength + blockMapPos.y) - line.point.z;
		for (size_t column = halfStep; column < k_BlockSide; column += step)
		{
			const size_t point = row * k_BlockSide + column;
			blend(point, point - halfStep, point + halfStep, alongZTimesX, z);
			z = z + stepLength;
		}
		x = stepLength + x;
	}

	// the centres of the coarser grid's squares, always between the same two corners
	const size_t diagonal = halfStep * (k_BlockSide + 1);
	x = (halfStepLength + blockMapPos.x) - line.point.x;
	for (size_t row = halfStep; row < k_BlockSide; row += step)
	{
		const float alongZTimesX = line.along.z * x;
		float z = (halfStepLength + blockMapPos.y) - line.point.z;
		for (size_t column = halfStep; column < k_BlockSide; column += step)
		{
			const size_t point = row * k_BlockSide + column;
			blend(point, point - diagonal, point + diagonal, alongZTimesX, z);
			z = z + stepLength;
		}
		x = stepLength + x;
	}

	// along the columns of the coarser grid, between the points either side
	const size_t rowsBetween = halfStep * k_BlockSide;
	x = (halfStepLength + blockMapPos.x) - line.point.x;
	for (size_t row = halfStep; row < k_BlockSide; row += step)
	{
		const float alongZTimesX = line.along.z * x;
		float z = blockMapPos.y - line.point.z;
		for (size_t column = 0; column < k_BlockSide; column += step)
		{
			const size_t point = row * k_BlockSide + column;
			blend(point, point - rowsBetween, point + rowsBetween, alongZTimesX, z);
			z = z + stepLength;
		}
		x = stepLength + x;
	}
}

std::optional<bool> KeepsFrontTriangle(std::span<const lnd::LNDCell, k_BlockCells> cells, glm::vec2 blockMapPos,
                                       const DrawCamera& camera, bool partlyOutside, const BlockLod& lod, const LodLines& lines)
{
	if (lod.meshLod > k_CoarsestMeshLod || !camera.maxScreen.has_value())
	{
		return std::nullopt;
	}
	// step 2 never looks at the seam bits
	SeamEdges edges {};
	if (lod.meshLod < k_CoarsestMeshLod)
	{
		const auto seamEdges = SeamEdgesFor(lod.meshLodType);
		if (!seamEdges.has_value())
		{
			return std::nullopt;
		}
		edges = *seamEdges;
	}

	// blending 1 and 3 work on a blended copy
	std::array<lnd::LNDCell, k_BlockCells> blended {};
	std::span<const lnd::LNDCell, k_BlockCells> source = cells;
	if (lod.meshBlending == 1 || lod.meshBlending == 3)
	{
		std::ranges::copy(cells, blended.begin());
		const bool towardsHalfGrid = lod.meshBlending == 1;
		BlendTowardsCoarserGrid(blended, blockMapPos, towardsHalfGrid ? lines.inner : lines.outer, towardsHalfGrid);
		source = blended;
	}

	LandClipper clipper(camera, *camera.maxScreen);

	// the grid: x and z step on from the block's corner, rows along x, one point every `stepCells` cells
	const size_t stepCells = size_t {1} << lod.meshLod;
	const size_t side = k_CellsPerSide / stepCells + 1;
	const float stepLength = static_cast<float>(stepCells) * k_CellSide;
	float x = blockMapPos.x;
	for (size_t row = 0; row < side; ++row)
	{
		float z = blockMapPos.y;
		for (size_t column = 0; column < side; ++column)
		{
			const uint8_t altitude = source[(row * k_BlockSide + column) * stepCells].altitude;
			const float height = altitude > k_HighestFlatAltitude ? static_cast<float>(altitude) * k_BlockHeightScale : 0.0f;
			clipper.SetGridPoint(row * side + column, {x, height, z}, partlyOutside);
			z = z + stepLength;
		}
		x = x + stepLength;
	}

	// a triangle that is kept; the list is never emptied, so the first kept one decides
	const auto keeps = [&clipper](size_t p0, size_t p1, size_t p2, bool allInside) {
		if (allInside)
		{
			return clipper.FrontFacing(p0, p1, p2);
		}
		return (clipper.Outside(p0) & clipper.Outside(p1) & clipper.Outside(p2)) == 0 && clipper.ClipKeepsFront(p0, p1, p2);
	};

	// the grid's squares, two triangles each, in the order they are emitted. The flags are read through a record that
	// does not follow the step or the left-out edges
	const size_t squaresPerSide = side - 1;
	const size_t firstRow = edges.skipFirstRow ? 1 : 0;
	const size_t endRow = squaresPerSide - (edges.skipLastRow ? 1 : 0);
	const size_t firstColumn = edges.skipFirstColumn ? 1 : 0;
	const size_t endColumn = squaresPerSide - (edges.skipLastColumn ? 1 : 0);
	const size_t rowEndStep = (edges.skipFirstColumn || edges.skipLastColumn) ? 2 : 1;
	const bool skipsUndrawnWater = lod.meshBlending == 0;
	size_t a = firstRow * side + firstColumn;
	size_t flagRecord = 0;
	for (size_t row = firstRow; row < endRow; ++row)
	{
		for (size_t column = firstColumn; column < endColumn; ++column)
		{
			const lnd::LNDCell& cell = source[flagRecord];
			if (!skipsUndrawnWater || (cell.flags & k_UndrawnWaterFlag) == 0)
			{
				const size_t b = a + 1;
				const size_t c = a + side;
				const size_t d = c + 1;
				std::array<std::array<size_t, 3>, 2> triangles {{{a, b, d}, {a, d, c}}};
				if (cell.properties.split)
				{
					triangles = {{{b, d, c}, {b, c, a}}};
				}
				const bool allInside = (clipper.Outside(a) | clipper.Outside(b) | clipper.Outside(c) | clipper.Outside(d)) == 0;
				for (const auto& [p0, p1, p2] : triangles)
				{
					if (keeps(p0, p1, p2, allInside))
					{
						return true;
					}
				}
			}
			++a;
			++flagRecord;
		}
		a += rowEndStep;
		++flagRecord;
	}

	// the seam triangles towards coarser neighbours
	for (const auto& [p0, p1, p2] : SeamFan(lod.meshLod, lod.meshLodType))
	{
		const bool allInside = (clipper.Outside(p0) | clipper.Outside(p1) | clipper.Outside(p2)) == 0;
		if (keeps(p0, p1, p2, allInside))
		{
			return true;
		}
	}
	return false;
}

bool TakeLandClipResult(BlockState& block, bool keepsFrontTriangle)
{
	const bool last = (block.visibility & k_LandClipBit) != 0;
	block.visibility = keepsFrontTriangle ? block.visibility | k_InViewBit : block.visibility & ~k_InViewBit;
	if (keepsFrontTriangle == last)
	{
		return false;
	}
	block.visibility = keepsFrontTriangle ? block.visibility | k_LandClipBit : block.visibility & ~k_LandClipBit;
	return true;
}

} // namespace openblack::ecs::draw_list
