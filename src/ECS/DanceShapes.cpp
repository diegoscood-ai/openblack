/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceShapes.h"

#include <cmath>

#include <algorithm>
#include <array>

using namespace openblack::ecs;
using dance_shapes::Shape;

namespace
{
/// The game rounds each step of its sums to a float, and working each in double and rounding it once gives the same
/// points. Its sines, cosines and angles are not rounded: they keep their full precision until the step that uses them.
[[nodiscard]] float Round(double value)
{
	return static_cast<float>(value);
}

[[nodiscard]] double Sin(float angle)
{
	return std::sin(static_cast<double>(angle));
}

[[nodiscard]] double Cos(float angle)
{
	return std::cos(static_cast<double>(angle));
}

[[nodiscard]] int32_t Truncate(float value)
{
	return static_cast<int32_t>(value);
}

/// A 512th of a circle: the circles take every other one
constexpr double k_CircleStep = 0.012271846302734375;
constexpr double k_Radius = 65535.0;

Shape Circle()
{
	Shape shape {};
	for (size_t k = 0; k < shape.size(); ++k)
	{
		const float angle = Round(static_cast<double>(2 * k) * k_CircleStep);
		shape[k] = {Truncate(Round(Cos(angle) * k_Radius)), Truncate(Round(Sin(angle) * k_Radius))};
	}
	return shape;
}

/// A circle whose edge goes in and out six times
Shape WavyCircle()
{
	constexpr double k_Waves = 6.0;
	constexpr double k_WaveHeight = 8191.0;
	Shape shape {};
	for (size_t k = 0; k < shape.size(); ++k)
	{
		const float angle = Round(static_cast<double>(2 * k) * k_CircleStep);
		const float wave = Round(Sin(Round(static_cast<double>(angle) * k_Waves)) * k_WaveHeight);
		const float radius = Round(static_cast<double>(wave) + k_Radius);
		shape[k] = {Truncate(Round(Cos(angle) * static_cast<double>(radius))),
		            Truncate(Round(Sin(angle) * static_cast<double>(radius)))};
	}
	return shape;
}

/// Three turns outwards from the centre
Shape Spiral()
{
	constexpr double k_Step = 0.07363107781640625;
	constexpr double k_End = 18.849555921;
	constexpr double k_Scale = 3276.0;
	Shape shape {};
	float angle = 0.0f;
	for (size_t k = 0; k < shape.size(); ++k)
	{
		const float cosine = Round(Cos(angle) * static_cast<double>(angle));
		const float sine = Round(Sin(angle) * static_cast<double>(angle));
		shape[k] = {Truncate(Round(static_cast<double>(cosine) * k_Scale)),
		            Truncate(Round(static_cast<double>(sine) * k_Scale))};
		angle = Round(static_cast<double>(angle) + k_Step);
		if (!(static_cast<double>(angle) < k_End))
		{
			break;
		}
	}
	return shape;
}

/// A wave along z. The game means it to run the whole way, but its first half stops after its first point, which a
/// comparison of a negative number as an unsigned one ends.
Shape Wave()
{
	constexpr double k_Step = 0.02454369260546875;
	constexpr int32_t k_Start = -0x3FC0;
	constexpr int32_t k_ZStep = 0xFF;
	Shape shape {};
	shape[0] = {Truncate(Round(Sin(0.0f) * k_Radius)), k_Start / 2};
	for (size_t j = 0; j < shape.size() / 2; ++j)
	{
		const float angle = Round(static_cast<double>(2 * j) * k_Step);
		const int32_t z = k_Start + k_ZStep * static_cast<int32_t>(j);
		shape[shape.size() / 2 + j] = {Truncate(Round(Sin(angle) * -k_Radius)), z / 2};
	}
	return shape;
}

/// A square. The game means each side to have 64 points, but each stops after its first, which a comparison of a
/// negative number as an unsigned one ends: four corners at a quarter of the way round each
Shape Square()
{
	Shape shape {};
	shape[0] = {-0xFFFF, -0xFFE0};
	shape[64] = {-0xFFE0, 0xFFFF};
	shape[128] = {0xFFFF, 0xFFE0};
	shape[192] = {0xFFE0, -0xFFFF};
	return shape;
}

/// A line through the centre, along z, or along x
Shape Line(bool alongX)
{
	Shape shape {};
	for (size_t k = 0; k < shape.size(); ++k)
	{
		const int32_t along = static_cast<int32_t>(k) * 512 - 0x10000;
		shape[k] = alongX ? glm::ivec2 {along, 0} : glm::ivec2 {0, along};
	}
	return shape;
}

/// A line out along x from a little way off the centre
Shape LineOut()
{
	Shape shape {};
	for (size_t k = 0; k < shape.size(); ++k)
	{
		shape[k] = {static_cast<int32_t>(k) * 224 + 0x2000, 0};
	}
	return shape;
}

/// A point on a straight line between two others, i out of n of the way: along its angle by its length, each part
/// truncated. The angle, its sine and its cosine keep their full precision.
[[nodiscard]] glm::ivec2 Between(glm::ivec2 from, glm::ivec2 to, int32_t i, int32_t n)
{
	const double dx = static_cast<double>(to.x - from.x);
	const double dz = static_cast<double>(to.y - from.y);
	const double angle = std::atan2(dx, dz);
	const float squares = Round(static_cast<double>(Round(dx * dx)) + static_cast<double>(Round(dz * dz)));
	const float length = Round(std::sqrt(static_cast<double>(squares)));
	const auto part = [i, n, length](double trig, int32_t start) {
		const float a = Round(trig * static_cast<double>(i));
		const float b = Round(static_cast<double>(a) * static_cast<double>(length));
		const float c = Round(static_cast<double>(b) / static_cast<double>(n));
		return Truncate(Round(static_cast<double>(c) + static_cast<double>(start)));
	};
	return {part(std::sin(angle), from.x), part(std::cos(angle), from.y)};
}

/// Eight corners round the circle joined by straight lines of 32 points each
Shape Octagon()
{
	constexpr double k_Degree = 0.008726646259722222; // half a degree, in radians
	constexpr int32_t k_PointsPerSide = 32;
	std::array<glm::ivec2, 8> corners {};
	for (size_t k = 0; k < corners.size(); ++k)
	{
		const float angle = Round(static_cast<double>(90 * k) * k_Degree);
		corners[k] = {Truncate(Round(Cos(angle) * k_Radius)), Truncate(Round(Sin(angle) * k_Radius))};
	}
	Shape shape {};
	for (size_t side = 0; side < corners.size(); ++side)
	{
		const auto from = corners[side];
		const auto to = corners[(side + 1) % corners.size()];
		for (int32_t i = 0; i < k_PointsPerSide; ++i)
		{
			shape[side * k_PointsPerSide + static_cast<size_t>(i)] = Between(from, to, i, k_PointsPerSide);
		}
	}
	return shape;
}

/// The points of a side of the triangle or of the heart's straight lines
constexpr int32_t k_SidePoints = 85;
constexpr auto k_Side = static_cast<size_t>(k_SidePoints);

/// Joins two points with a straight line of a side's points from the given one on, and the last side its end point too
void JoinSide(Shape& shape, size_t first, glm::ivec2 from, glm::ivec2 to, bool withEnd)
{
	const int32_t count = withEnd ? k_SidePoints + 1 : k_SidePoints;
	for (int32_t i = 0; i < count; ++i)
	{
		shape[first + static_cast<size_t>(i)] = Between(from, to, i, k_SidePoints);
	}
}

/// A triangle pointing along -x, its three sides joined by straight lines; its last point is its first again
Shape Triangle()
{
	constexpr double k_Corner = 0.5235987755833333; // a twelfth of a circle, in radians
	const glm::ivec2 top {Truncate(Round(std::sin(k_Corner) * k_Radius)), Truncate(Round(std::cos(k_Corner) * k_Radius))};
	const glm::ivec2 point {-65535, 0};
	const glm::ivec2 bottom {top.x, Truncate(Round(std::cos(k_Corner) * -k_Radius))};
	Shape shape {};
	JoinSide(shape, 0, point, top, false);
	JoinSide(shape, k_Side, top, bottom, false);
	JoinSide(shape, 2 * k_Side, bottom, point, true);
	return shape;
}

/// A heart pointing along +x: two half circles of half the radius side by side along z, then straight lines out to
/// its tip and back; its last point is its first again
Shape Heart()
{
	constexpr double k_Step = 0.03695991357058823;
	constexpr double k_Quarter = 1.57079632675; // a quarter of a circle, in radians
	constexpr double k_Half = 32767.0;
	constexpr size_t k_FirstLobe = 42;
	constexpr size_t k_SecondLobe = 43;
	const auto lobeAngle = [](size_t j) {
		return Round(static_cast<double>(Round(static_cast<double>(2 * j) * k_Step)) - k_Quarter);
	};
	Shape shape {};
	for (size_t j = 0; j < k_FirstLobe; ++j)
	{
		const float angle = lobeAngle(j);
		shape[j] = {Truncate(Round(Cos(angle) * -k_Half)),
		            Truncate(Round(static_cast<double>(Round(Sin(angle) * k_Half)) - k_Half))};
	}
	for (size_t j = 0; j < k_SecondLobe; ++j)
	{
		const float angle = lobeAngle(j);
		shape[k_FirstLobe + j] = {Truncate(Round(Cos(angle) * -k_Half)),
		                          Truncate(Round(static_cast<double>(Round(Sin(angle) + 1.0)) * k_Half))};
	}
	const glm::ivec2 start {0, -65534};
	const glm::ivec2 middle {0, 65534};
	const glm::ivec2 tip {131068, 0};
	JoinSide(shape, k_Side, middle, tip, false);
	JoinSide(shape, 2 * k_Side, tip, start, true);
	return shape;
}

constexpr std::array<const char*, dance_shapes::k_ShapeCount> k_FileNames {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Star",
    "Man",
    "Cock",
    "Fish",
    "Lightning",
    "FigureEight",
    "Arc",
    "LetterA",
    "LetterB",
    "LetterC",
    "LetterD",
    "LetterE",
    "LetterF",
    "LetterG",
    "LetterH",
    "LetterI",
    "LetterJ",
    "LetterK",
    "LetterL",
    "LetterM",
    "LetterN",
    "LetterO",
    "LetterP",
    "LetterQ",
    "LetterR",
    "LetterS",
    "LetterT",
    "LetterU",
    "LetterV",
    "LetterW",
    "LetterX",
    "LetterY",
    "LetterZ",
};
} // namespace

Shape dance_shapes::Build(std::size_t index)
{
	switch (index)
	{
	case 0:
		return Circle();
	case 1:
		return WavyCircle();
	case 2:
		return Spiral();
	case 3:
		return Wave();
	case 4:
		return Square();
	case 5:
		return Line(false);
	case 6:
		return Line(true);
	case 8:
		return Triangle();
	case 9:
		return Heart();
	case 10:
		return Octagon();
	case 11:
		return LineOut();
	default:
		// The point (7), and those not worked out yet:
		// TODO(opening): the random shape (12) drawn from the game's random numbers as it starts, and the shapes read
		// from the data folder (13 on)
		return Shape {};
	}
}

const char* dance_shapes::FileName(std::size_t index)
{
	return index < k_FileNames.size() ? k_FileNames.at(index) : "";
}

Shape dance_shapes::FromPoints(std::span<const glm::ivec2> points)
{
	Shape shape {};
	std::ranges::copy(points.first(std::min(points.size(), shape.size())), shape.begin());
	return shape;
}
