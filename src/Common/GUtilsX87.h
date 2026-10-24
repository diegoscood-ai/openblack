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

[[nodiscard]] constexpr Reduced Reduce(Pair angle)
{
	Pair r = angle;
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

/// The FPU's sine and cosine of an angle left unrounded on its stack (an arctangent), unrounded. An angle that is not
/// a number or infinite gives not a number; one of 2^63 or more is given back unchanged, as the FPU leaves it.
[[nodiscard]] constexpr SineCosine SinCos(Extended angle)
{
	const double high = angle.high;
	if (high != high || high == std::numeric_limits<double>::infinity() || high == -std::numeric_limits<double>::infinity())
	{
		const auto nan = std::bit_cast<double>(uint64_t {0xFFF8000000000000u});
		return {{nan, 0.0}, {nan, 0.0}};
	}
	if (!(high < detail::k_LargestAngle && high > -detail::k_LargestAngle))
	{
		return {angle, angle};
	}
	if (high == 0.0)
	{
		return {{high, 0.0}, {1.0, 0.0}};
	}
	const auto [quarters, r] = detail::Reduce({angle.high, angle.low});
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

/// The FPU's sine and cosine of an angle (a float, or a value at float precision), unrounded
[[nodiscard]] constexpr SineCosine SinCos(double angle)
{
	return SinCos(Extended {angle, 0.0});
}

[[nodiscard]] constexpr Extended Sin(double angle)
{
	return SinCos(angle).sin;
}

[[nodiscard]] constexpr Extended Cos(double angle)
{
	return SinCos(angle).cos;
}

namespace detail
{

/// The true pi and half of it, as the top 53 bits and the next 53 (for the arctangent, which is not the FPU's
/// reduction)
constexpr Pair k_TruePi {0x1.921fb54442d18p+1, 0x1.1a62633145c07p-53};
constexpr Pair k_TrueHalfPi {0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54};
constexpr Pair k_Ln2 {0x1.62e42fefa39efp-1, 0x1.abc9e3b39803fp-56};
constexpr Pair k_Log2E {0x1.71547652b82fep+0, 0x1.777d0ffda0d24p-56};

/// The FPU's own angles, rounded to 64 bits: pi, pi / 2, pi / 4 and 3 pi / 4
constexpr Extended k_FpuPi {0x1.921fb54442d18p+1, 0x1.1a8p-53};
constexpr Extended k_FpuHalfPi {0x1.921fb54442d18p+0, 0x1.1a8p-54};
constexpr Extended k_FpuQuarterPi {0x1.921fb54442d18p-1, 0x1.1a8p-55};
constexpr Extended k_FpuThreeQuarterPi {0x1.2d97c7f3321d2p+1, 0x1.a8p-54};

/// The FPU's default not a number, which an invalid operation gives
constexpr uint64_t k_IndefiniteBits = 0xFFF8000000000000u;
constexpr uint64_t k_QuietBit = uint64_t {1} << 51;

[[nodiscard]] constexpr double Indefinite()
{
	return std::bit_cast<double>(k_IndefiniteBits);
}

[[nodiscard]] constexpr double Quiet(double nan)
{
	return std::bit_cast<double>(std::bit_cast<uint64_t>(nan) | k_QuietBit);
}

/// The not a number an operation on two values gives when one or both are: the one with the larger significand (the
/// positive one if they are the same), made quiet
[[nodiscard]] constexpr double ChooseNaN(double a, double b)
{
	if (b != b && a != a)
	{
		const uint64_t fa = std::bit_cast<uint64_t>(a) & (k_QuietBit - 1);
		const uint64_t fb = std::bit_cast<uint64_t>(b) & (k_QuietBit - 1);
		if (fa != fb)
		{
			return Quiet(fa > fb ? a : b);
		}
		return Quiet(std::bit_cast<uint64_t>(a) >> 63 == 0 ? a : b);
	}
	return Quiet(a != a ? a : b);
}

[[nodiscard]] constexpr Pair Div(Pair a, Pair b)
{
	const double q1 = a.high / b.high;
	auto r = Add(a, Negate(Mul({q1, 0.0}, b)));
	const double q2 = r.high / b.high;
	r = Add(r, Negate(Mul({q2, 0.0}, b)));
	const double q3 = r.high / b.high;
	return Add(FastTwoSum(q1, q2), {q3, 0.0});
}

[[nodiscard]] inline Pair Sqrt(Pair a)
{
	const double s = std::sqrt(a.high);
	const auto [p, e] = TwoProduct(s, s);
	return FastTwoSum(s, (((a.high - p) - e) + a.low) / (2.0 * s));
}

/// atan(u) for 0 <= u <= 1: three halvings of the angle, atan(u) = 2 atan(u / (1 + sqrt(1 + u^2))), bring u under
/// tan(pi / 32), where twenty terms of the series reach far past 64 bits
constexpr int k_AtanHalvings = 3;
constexpr int k_AtanTerms = 20;

[[nodiscard]] inline Pair AtanOfUnit(Pair u)
{
	for (int i = 0; i < k_AtanHalvings; ++i)
	{
		u = Div(u, Add({1.0, 0.0}, Sqrt(Add({1.0, 0.0}, Mul(u, u)))));
	}
	const auto u2 = Mul(u, u);
	Pair sum {0.0, 0.0};
	for (int k = k_AtanTerms - 1; k >= 0; --k)
	{
		const auto term = Div(Pair {1.0, 0.0}, static_cast<double>(2 * k + 1));
		sum = Add(Mul(sum, u2), (k & 1) != 0 ? Negate(term) : term);
	}
	const auto r = Mul(sum, u);
	constexpr double k_Halvings = 1 << k_AtanHalvings;
	return {r.high * k_Halvings, r.low * k_Halvings};
}

/// log2(x) of a positive, finite x: x = m 2^e with m between sqrt(1/2) and sqrt(2), ln m = 2 atanh((m - 1) / (m + 1))
constexpr double k_SqrtHalf = 0.7071067811865476;
constexpr int k_LogTerms = 24;

[[nodiscard]] inline Pair Log2(double x)
{
	int exponent = 0;
	double m = std::frexp(x, &exponent);
	if (m < k_SqrtHalf)
	{
		m *= 2.0;
		--exponent;
	}
	const auto u = Div(Pair {m - 1.0, 0.0}, TwoSum(m, 1.0));
	const auto u2 = Mul(u, u);
	Pair sum {0.0, 0.0};
	for (int k = k_LogTerms - 1; k >= 0; --k)
	{
		sum = Add(Mul(sum, u2), Div(Pair {1.0, 0.0}, static_cast<double>(2 * k + 1)));
	}
	const auto halfLn = Mul(sum, u);
	return Add({static_cast<double>(exponent), 0.0}, Mul({halfLn.high * 2.0, halfLn.low * 2.0}, k_Log2E));
}

/// e^w - 1 for |w| <= ln(2) / 2
constexpr int k_ExpTerms = 27;

[[nodiscard]] constexpr Pair ExpMinusOne(Pair w)
{
	Pair sum {1.0, 0.0};
	for (int k = k_ExpTerms; k >= 2; --k)
	{
		sum = Add({1.0, 0.0}, Div(Mul(sum, w), static_cast<double>(k)));
	}
	return Mul(sum, w);
}

/// The nearest whole number to an exact sum, ties to even
[[nodiscard]] inline double NearestWhole(Pair t)
{
	double n = std::nearbyint(t.high);
	const auto rest = Add(t, {-n, 0.0});
	const bool odd = std::fmod(n, 2.0) != 0.0;
	if (rest.high > 0.5 || (rest.high == 0.5 && (rest.low > 0.0 || (rest.low == 0.0 && odd))))
	{
		n += 1.0;
	}
	else if (rest.high < -0.5 || (rest.high == -0.5 && (rest.low < 0.0 || (rest.low == 0.0 && odd))))
	{
		n -= 1.0;
	}
	return n;
}

/// A power of two at float precision, as mantissa x 2^exponent: the whole part of t taken out, the fraction rounded to
/// a float's mantissa, its power less one exact to 64 bits, one added (rounded) and the whole part scaled back in
struct Scaled
{
	double mantissa;
	double exponent;
};

[[nodiscard]] inline Scaled TwoTo(Extended t)
{
	const Pair exact {t.high, t.low};
	const double whole = NearestWhole(exact);
	const auto fraction = Add(exact, {-whole, 0.0});
	const double f = RoundWithRest(fraction.high, fraction.low);
	const auto minusOne = RoundTo64Bits(ExpMinusOne(Mul({f, 0.0}, k_Ln2)));
	const auto sum = Add({1.0, 0.0}, minusOne);
	return {RoundWithRest(sum.high, sum.low), whole};
}

[[nodiscard]] inline bool IsWhole(double value)
{
	return value == std::floor(value);
}

/// mantissa x 2^exponent as the runtime returns it as a double: infinite past the double's range; below its smallest
/// normal the runtime denormalises it itself, cutting the bits that do not fit (towards zero), and gives a plus zero
/// when less than half the smallest denormal is left (a value too small even for its wider scaling keeps its sign)
[[nodiscard]] inline double ToRuntimeDouble(Scaled v, bool negative)
{
	constexpr double k_Overflow = 1024.0;
	constexpr double k_Normal = -1022.0;
	constexpr double k_Smallest = -1074.0;
	constexpr double k_PlusZero = -1075.0;
	constexpr double k_Unscalable = -2558.0;
	const double sign = negative ? -1.0 : 1.0;
	int mantissaExponent = 0;
	const double m = std::frexp(v.mantissa, &mantissaExponent); // m in [1/2, 1)
	const double exponent = v.exponent + static_cast<double>(mantissaExponent) - 1.0;
	if (exponent >= k_Overflow)
	{
		return sign * std::numeric_limits<double>::infinity();
	}
	if (exponent >= k_Normal)
	{
		return sign * std::ldexp(v.mantissa, static_cast<int>(v.exponent));
	}
	if (exponent < k_Unscalable)
	{
		return sign * 0.0;
	}
	if (exponent < k_PlusZero)
	{
		return 0.0;
	}
	// the value as a whole number of 2^-1074, cut towards zero
	const double units = std::floor(std::ldexp(m, static_cast<int>(exponent - k_Smallest) + 1));
	return sign * std::ldexp(units, static_cast<int>(k_Smallest));
}

/// The runtime's helper for an infinite base or exponent
[[nodiscard]] inline double PowOfInfinity(double x, double y)
{
	constexpr double k_Infinity = std::numeric_limits<double>::infinity();
	const double magnitude = std::abs(x);
	if (y == k_Infinity)
	{
		return magnitude > 1.0 ? k_Infinity : (magnitude < 1.0 ? 0.0 : Indefinite());
	}
	if (y == -k_Infinity)
	{
		return magnitude > 1.0 ? 0.0 : (magnitude < 1.0 ? k_Infinity : Indefinite());
	}
	if (x == k_Infinity)
	{
		return y > 0.0 ? k_Infinity : (y == 0.0 ? 1.0 : 0.0);
	}
	// x is minus infinity; the runtime tests the exponent's parity at full precision
	const bool odd = IsWhole(y) && !IsWhole(y * 0.5);
	if (y > 0.0)
	{
		return odd ? -k_Infinity : k_Infinity;
	}
	if (y == 0.0)
	{
		return 1.0;
	}
	return odd ? -0.0 : 0.0;
}

} // namespace detail

/// The FPU's arctangent of y / x in the quadrant of (x, y), atan2(y, x), unrounded: rounded once from the exact angle to
/// 64 bits. The FPU's own last bit differs in about one result in forty (it is not always correctly rounded), which
/// reaches a float or double result only when that result lands within 2^-40 of a halfway point. Takes values that are
/// floats or at float precision (any doubles whose quotient stays a normal double).
[[nodiscard]] inline Extended Atan2(double y, double x)
{
	if (y != y || x != x)
	{
		return {detail::ChooseNaN(y, x), 0.0};
	}
	const bool yNegative = std::signbit(y);
	const bool xNegative = std::signbit(x);
	const auto withSign = [yNegative](Extended v) { return yNegative ? Extended {-v.high, -v.low} : v; };
	if (y == 0.0)
	{
		return xNegative ? withSign(detail::k_FpuPi) : Extended {y, 0.0};
	}
	if (std::isinf(y))
	{
		if (std::isinf(x))
		{
			return withSign(xNegative ? detail::k_FpuThreeQuarterPi : detail::k_FpuQuarterPi);
		}
		return withSign(detail::k_FpuHalfPi);
	}
	if (std::isinf(x))
	{
		return xNegative ? withSign(detail::k_FpuPi) : Extended {yNegative ? -0.0 : 0.0, 0.0};
	}
	if (x == 0.0)
	{
		return withSign(detail::k_FpuHalfPi);
	}
	const double a = std::abs(y);
	const double b = std::abs(x);
	auto r = a <= b
	             ? detail::AtanOfUnit(detail::Div(detail::Pair {a, 0.0}, detail::Pair {b, 0.0}))
	             : detail::Add(detail::k_TrueHalfPi,
	                           detail::Negate(detail::AtanOfUnit(detail::Div(detail::Pair {b, 0.0}, detail::Pair {a, 0.0}))));
	if (xNegative)
	{
		r = detail::Add(detail::k_TruePi, detail::Negate(r));
	}
	r = detail::RoundTo64Bits(r);
	return withSign({r.high, r.low});
}

/// The C runtime's arccosine as the game calls it: the arctangent of sqrt((1 + x)(1 - x)) over x, each step of the
/// root at float precision, the arctangent rounded to a double on its way out (the game always has an inexact result
/// pending, which sends the runtime down that path). Not a number outside [-1, 1]; 0 at 1 and pi (as a double) at -1.
[[nodiscard]] inline Extended Acos(double x)
{
	if (x != x)
	{
		return {detail::Quiet(x), 0.0};
	}
	if (!(std::abs(x) <= 1.0))
	{
		return {detail::Indefinite(), 0.0};
	}
	if (x == 1.0)
	{
		return {0.0, 0.0};
	}
	if (x == -1.0)
	{
		return {detail::k_FpuPi.high + detail::k_FpuPi.low, 0.0};
	}
	const auto angle = Atan2(Sqrt(Mul(Add(1.0, x), Sub(1.0, x))), x);
	return {angle.high + angle.low, 0.0};
}

/// The FPU's y log2(x), unrounded (rounded once to 64 bits; its own last bit can differ as the arctangent's does). Not a
/// number for a negative x or for 0 x infinity; infinite for x = 0 or x infinite. Exact for |y| up to 2^900.
[[nodiscard]] inline Extended Log2Times(double x, Extended y)
{
	constexpr double k_Infinity = std::numeric_limits<double>::infinity();
	if (x != x || y.high != y.high)
	{
		return {detail::ChooseNaN(y.high, x), 0.0};
	}
	if (x < 0.0)
	{
		return {detail::Indefinite(), 0.0};
	}
	const bool yNegative = std::signbit(y.high);
	if (x == 0.0)
	{
		return {y.high == 0.0 ? detail::Indefinite() : (yNegative ? k_Infinity : -k_Infinity), 0.0};
	}
	if (x == k_Infinity)
	{
		return {y.high == 0.0 ? detail::Indefinite() : (yNegative ? -k_Infinity : k_Infinity), 0.0};
	}
	if (std::isinf(y.high))
	{
		if (x == 1.0)
		{
			return {detail::Indefinite(), 0.0};
		}
		return {(x > 1.0) == !yNegative ? k_Infinity : -k_Infinity, 0.0};
	}
	if (x == 1.0)
	{
		return {yNegative ? -0.0 : 0.0, 0.0};
	}
	if (std::abs(y.high) > 0x1p900)
	{
		// so far out that only the size and the sign of the result matter (it may pass a double's range)
		return {y.high * detail::Log2(x).high, 0.0};
	}
	const auto r = detail::RoundTo64Bits(detail::Mul(detail::Log2(x), {y.high, y.low}));
	return {r.high, r.low};
}

/// The FPU's natural logarithm (its ln 2 times log2 x), unrounded
[[nodiscard]] inline Extended Log(double x)
{
	constexpr Extended k_FpuLn2 {0x1.62e42fefa39efp-1, 0x1.ac00000000000p-56};
	return Log2Times(x, k_FpuLn2);
}

/// The C runtime's pow as the game calls it: y log2(x) to 64 bits, then two to that power at float precision, so the
/// result holds a float's mantissa. A negative base takes a whole exponent (whose parity is tested at float precision,
/// on half of it); outside a double's range the result is infinite, or denormalised as the runtime does it.
[[nodiscard]] inline double Pow(double x, double y)
{
	if (x != x || y != y)
	{
		return detail::ChooseNaN(y, x);
	}
	if (std::isinf(x) || std::isinf(y))
	{
		return detail::PowOfInfinity(x, y);
	}
	const auto odd = [](double whole) { return !detail::IsWhole(Mul(whole, 0.5)); };
	if (x == 0.0)
	{
		if (y == 0.0)
		{
			return 1.0;
		}
		if ((std::bit_cast<uint64_t>(y) & 0x7FF0000000000000u) == 0)
		{
			return 0.0; // a denormal exponent
		}
		const bool negative = std::signbit(x) && detail::IsWhole(y) && odd(y);
		if (y < 0.0)
		{
			return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		}
		return negative ? -0.0 : 0.0;
	}
	bool negative = false;
	if (x < 0.0)
	{
		if (!detail::IsWhole(y))
		{
			return detail::Indefinite();
		}
		negative = odd(y);
		x = -x;
	}
	const auto t = Log2Times(x, {y, 0.0});
	if (std::isinf(t.high) || std::abs(t.high) > 0x1p20)
	{
		// far past a double's range either way (the FPU's own range keeps the scaled value until the runtime's test)
		return detail::ToRuntimeDouble({1.0, t.high > 0.0 ? 0x1p20 : -0x1p20}, negative);
	}
	return detail::ToRuntimeDouble(detail::TwoTo(t), negative);
}

} // namespace openblack::gutils::x87
