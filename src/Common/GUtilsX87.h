/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>

/// The original's floating point as its FPU works it out, step by step, for code that must give the original's bits.
///
/// The game runs the FPU at 24-bit precision: every multiply, add, subtract, divide and square root rounds its result to
/// a float's 24-bit mantissa, round to nearest even, but the exponent stays wide, so nothing overflows or underflows
/// half way through a chain. The sine and cosine are not rounded by the precision control: they give 64 bits of
/// mantissa, which stay on the FPU's stack until the first operation on them (rounded then, once, from the 64-bit value)
/// or until they are stored to a float.
///
/// Here a value at float precision is a double that holds a float's 24-bit mantissa, with the double's exponent; Mul,
/// Add, Sub, Div and Sqrt take such values and give one back. Working in double and rounding to 24 bits afterwards is
/// exact for these operations: a double has more than twice the bits of a float, so its rounding never moves the second
/// one. A sine or cosine is an Extended, its 64 bits held exactly as the sum of two doubles; an operation that takes one
/// rounds the exact result once, as the FPU does. Store is the store to a float.
///
/// The sine and cosine reduce the angle by the FPU's own pi, 66 bits long, so far from 0 they follow the FPU and not the
/// true sine. They are worked out in double-double arithmetic and rounded to 64 bits; the FPU's own last bit differs
/// in about one result in forty (it is not always correctly rounded), which reaches a float result only when that
/// result lands within 2^-40 of a halfway point. No case seen did.
namespace openblack::gutils::x87
{

/// A sine or cosine as it is left on the FPU's stack: 64 bits of mantissa, the value being high + low exactly
struct Extended
{
	double high {0.0};
	double low {0.0};
};

/// The sine and cosine of one angle, unrounded
struct SineCosine
{
	Extended sin;
	Extended cos;
};

namespace detail
{

constexpr uint64_t k_MantissaMask = (uint64_t {1} << 52) - 1;
/// The 29 bits of a double's mantissa below a float's 24, and the half of them
constexpr uint64_t k_DroppedBits = (uint64_t {1} << 29) - 1;
constexpr uint64_t k_HalfDropped = uint64_t {1} << 28;

/// value rounded to a 24-bit mantissa, ties to even; a non-zero rest (less than half the value's last double bit)
/// breaks an exact tie towards its own side
[[nodiscard]] constexpr double RoundWithRest(double value, double rest)
{
	auto bits = std::bit_cast<uint64_t>(value);
	if (((bits >> 52) & 0x7FFu) == 0x7FFu)
	{
		return value;
	}
	const uint64_t dropped = bits & k_DroppedBits;
	bool up = dropped > k_HalfDropped;
	if (dropped == k_HalfDropped)
	{
		up = rest != 0.0 ? ((rest > 0.0) == (value > 0.0)) : ((bits >> 29) & 1u) != 0;
	}
	bits &= ~k_DroppedBits;
	if (up)
	{
		bits += k_DroppedBits + 1;
	}
	return std::bit_cast<double>(bits);
}

struct Pair
{
	double high;
	double low;
};

[[nodiscard]] constexpr Pair Negate(Pair a)
{
	return {-a.high, -a.low};
}

/// Splits a into two halves of 26 bits each, a = high + low exactly
[[nodiscard]] constexpr Pair Split(double a)
{
	constexpr double k_Splitter = 134217729.0; // 2^27 + 1
	const double t = k_Splitter * a;
	const double high = t - (t - a);
	return {high, a - high};
}

/// a * b exactly, as the rounded product and what it left out
[[nodiscard]] constexpr Pair TwoProduct(double a, double b)
{
	const double p = a * b;
	const auto [ah, al] = Split(a);
	const auto [bh, bl] = Split(b);
	return {p, ((ah * bh - p) + ah * bl + al * bh) + al * bl};
}

/// a + b exactly, as the rounded sum and what it left out
[[nodiscard]] constexpr Pair TwoSum(double a, double b)
{
	const double s = a + b;
	const double bb = s - a;
	return {s, (a - (s - bb)) + (b - bb)};
}

/// a + b exactly when |a| >= |b|
[[nodiscard]] constexpr Pair FastTwoSum(double a, double b)
{
	const double s = a + b;
	return {s, b - (s - a)};
}

[[nodiscard]] constexpr Pair Add(Pair a, Pair b)
{
	auto [s, e] = TwoSum(a.high, b.high);
	const auto [t, f] = TwoSum(a.low, b.low);
	e += t;
	const auto first = FastTwoSum(s, e);
	return FastTwoSum(first.high, first.low + f);
}

[[nodiscard]] constexpr Pair Mul(Pair a, Pair b)
{
	auto [p, e] = TwoProduct(a.high, b.high);
	e += a.high * b.low + a.low * b.high;
	return FastTwoSum(p, e);
}

[[nodiscard]] constexpr Pair Div(Pair a, double d)
{
	const double q1 = a.high / d;
	const auto [p, e] = TwoProduct(q1, d);
	const double q2 = (((a.high - p) - e) + a.low) / d;
	return FastTwoSum(q1, q2);
}

/// The FPU's pi is 0xC90FDAA22168C234C x 2^-66; half of it, as its top 53 bits and the 13 left
constexpr double k_HalfPiHigh = 1.5707963267948966;
constexpr double k_HalfPiLow = 6.123031769111886e-17;
constexpr double k_TwoOverPi = 0.6366197723675814;
constexpr double k_QuarterPi = 0.7853981633974483;
/// Past 2^63 the FPU leaves the angle on its stack as it is
constexpr double k_LargestAngle = 9223372036854775808.0;
constexpr int k_Terms = 14;

/// The angle less a whole number of quarter turns of the FPU's pi, exactly: how many (0 to 3) and what is left
struct Reduced
{
	int quarters;
	Pair rest;
};

[[nodiscard]] constexpr Reduced Reduce(double angle)
{
	Pair r {angle, 0.0};
	int quarters = 0;
	for (int pass = 0; pass < 4 && (r.high < 0.0 ? -r.high : r.high) > k_QuarterPi; ++pass)
	{
		const double v = r.high * k_TwoOverPi;
		const auto turns = static_cast<int64_t>(v + (v >= 0.0 ? 0.5 : -0.5));
		const auto q = static_cast<double>(turns);
		const auto [p, e] = TwoProduct(q, k_HalfPiHigh);
		const auto left = TwoSum(r.high, -p);
		r = Add(Add(left, {-e, r.low}), Negate(TwoProduct(q, k_HalfPiLow)));
		quarters = static_cast<int>((static_cast<uint64_t>(quarters) + static_cast<uint64_t>(turns)) & 3u);
	}
	return {quarters, r};
}

/// A value of more than 64 bits rounded to 64, ties to even
[[nodiscard]] constexpr Pair RoundTo64Bits(Pair v)
{
	if (v.high == 0.0)
	{
		return {v.high, 0.0};
	}
	const auto bits = std::bit_cast<uint64_t>(v.high);
	const int exponent = static_cast<int>((bits >> 52) & 0x7FFu) - 1023;
	if (exponent - 64 < -1022)
	{
		return v;
	}
	// The spacing of 64-bit values about high, a half of it below a power of two that low takes the value under
	auto gridBits = static_cast<uint64_t>(exponent - 63 + 1023) << 52;
	if ((bits & k_MantissaMask) == 0 && v.low != 0.0 && (v.low < 0.0) != (v.high < 0.0))
	{
		gridBits -= uint64_t {1} << 52;
	}
	const auto grid = std::bit_cast<double>(gridBits);
	const double n = v.low / grid;
	auto whole = static_cast<double>(static_cast<int64_t>(n));
	const double fraction = n - whole;
	if (fraction > 0.5 || (fraction == 0.5 && static_cast<int64_t>(whole) % 2 != 0))
	{
		whole += 1.0;
	}
	else if (fraction < -0.5 || (fraction == -0.5 && static_cast<int64_t>(whole) % 2 != 0))
	{
		whole -= 1.0;
	}
	return FastTwoSum(v.high, whole * grid);
}

} // namespace detail

/// value rounded to a float's 24-bit mantissa, the exponent kept: the FPU's rounding after each operation
[[nodiscard]] constexpr double Round(double value)
{
	return detail::RoundWithRest(value, 0.0);
}

/// A sine or cosine rounded once to float precision, as the first operation on it rounds it
[[nodiscard]] constexpr double Round(Extended value)
{
	return detail::RoundWithRest(value.high, value.low);
}

/// The store of a value to a float (fstp dword). Below the smallest normal float the value is rounded again
[[nodiscard]] constexpr float Store(double value)
{
	return static_cast<float>(value);
}

[[nodiscard]] constexpr float Store(Extended value)
{
	return static_cast<float>(Round(value));
}

[[nodiscard]] constexpr double Mul(double a, double b)
{
	return Round(a * b);
}

[[nodiscard]] constexpr double Mul(Extended a, double b)
{
	const auto [p, e] = detail::TwoProduct(a.high, b);
	return detail::RoundWithRest(p, e + a.low * b);
}

[[nodiscard]] constexpr double Mul(double a, Extended b)
{
	return Mul(b, a);
}

[[nodiscard]] constexpr double Add(double a, double b)
{
	return Round(a + b);
}

[[nodiscard]] constexpr double Add(Extended a, Extended b)
{
	const auto sum = detail::Add({a.high, a.low}, {b.high, b.low});
	return detail::RoundWithRest(sum.high, sum.low);
}

[[nodiscard]] constexpr double Add(Extended a, double b)
{
	return Add(a, Extended {b, 0.0});
}

[[nodiscard]] constexpr double Add(double a, Extended b)
{
	return Add(Extended {a, 0.0}, b);
}

[[nodiscard]] constexpr double Sub(double a, double b)
{
	return Round(a - b);
}

[[nodiscard]] constexpr double Sub(Extended a, Extended b)
{
	return Add(a, Extended {-b.high, -b.low});
}

[[nodiscard]] constexpr double Sub(Extended a, double b)
{
	return Add(a, Extended {-b, 0.0});
}

[[nodiscard]] constexpr double Sub(double a, Extended b)
{
	return Add(Extended {a, 0.0}, Extended {-b.high, -b.low});
}

[[nodiscard]] constexpr double Div(double a, double b)
{
	return Round(a / b);
}

[[nodiscard]] inline double Sqrt(double value)
{
	return Round(std::sqrt(value));
}

/// a * b + c, each rounded, the order the original's code most often takes
[[nodiscard]] constexpr double MulAdd(double a, double b, double c)
{
	return Add(Mul(a, b), c);
}

[[nodiscard]] constexpr double MulAdd(Extended a, double b, double c)
{
	return Add(Mul(a, b), c);
}

/// The FPU's sine and cosine of an angle (a float, or a value at float precision), unrounded. An angle that is not
/// a number or infinite gives not a number; one of 2^63 or more is given back unchanged, as the FPU leaves it.
[[nodiscard]] constexpr SineCosine SinCos(double angle)
{
	if (angle != angle || angle == std::numeric_limits<double>::infinity() || angle == -std::numeric_limits<double>::infinity())
	{
		const auto nan = std::bit_cast<double>(uint64_t {0xFFF8000000000000u});
		return {{nan, 0.0}, {nan, 0.0}};
	}
	if (!(angle < detail::k_LargestAngle && angle > -detail::k_LargestAngle))
	{
		return {{angle, 0.0}, {angle, 0.0}};
	}
	if (angle == 0.0)
	{
		return {{angle, 0.0}, {1.0, 0.0}};
	}
	const auto [quarters, r] = detail::Reduce(angle);
	const auto r2 = detail::Mul(r, r);
	detail::Pair s = r;
	detail::Pair term = r;
	for (int n = 1; n < detail::k_Terms; ++n)
	{
		term = detail::Div(detail::Mul(term, r2), static_cast<double>((2 * n) * (2 * n + 1)));
		s = detail::Add(s, (n & 1) != 0 ? detail::Negate(term) : term);
	}
	detail::Pair c {1.0, 0.0};
	term = c;
	for (int n = 1; n < detail::k_Terms; ++n)
	{
		term = detail::Div(detail::Mul(term, r2), static_cast<double>((2 * n - 1) * (2 * n)));
		c = detail::Add(c, (n & 1) != 0 ? detail::Negate(term) : term);
	}
	detail::Pair sine = s;
	detail::Pair cosine = c;
	switch (quarters)
	{
	case 1:
		sine = c;
		cosine = detail::Negate(s);
		break;
	case 2:
		sine = detail::Negate(s);
		cosine = detail::Negate(c);
		break;
	case 3:
		sine = detail::Negate(c);
		cosine = s;
		break;
	default:
		break;
	}
	sine = detail::RoundTo64Bits(sine);
	cosine = detail::RoundTo64Bits(cosine);
	return {{sine.high, sine.low}, {cosine.high, cosine.low}};
}

[[nodiscard]] constexpr Extended Sin(double angle)
{
	return SinCos(angle).sin;
}

[[nodiscard]] constexpr Extended Cos(double angle)
{
	return SinCos(angle).cos;
}

} // namespace openblack::gutils::x87
