#ifndef LH3D_COLOUR_SH
#define LH3D_COLOUR_SH

// The GPU side of src/Graphics/Lh3dColour.h: the byte arithmetic of LH3DColor (a D3DCOLOR, 0xAARRGGBB), on colours
// kept as 0..255 floats with integer values. Wiki: rendering-objects.md, "Aritmética de LH3DColor". The CPU and this
// one must stay the same: every routine of the original truncates, none rounds. The alpha rule (multiplied, kept or
// opaque) is the caller's: these work on whatever channels they are given. vs_object.sc decodes the instance's colour
// column with it (lh3d_colour::PackInstance*) and vs_foliage.sc the crops' colour.

// (c t) >> 8 per channel, truncated: fn_0080BF10 0x80BFA3..0x80C00B (the four channels), fn_00809D80 (RGB, 0x809DCF
// keeps a.A), the model light's (c f) >> 8 (0x84BBEA..0x84BC1D), Tree::Draw 0x74B077 and fn_0074B3A0
vec4 Lh3dMulShr8(vec4 c255, vec4 t255)
{
	return floor(c255 * t255 / 256.0f);
}
vec3 Lh3dMulShr8(vec3 c255, vec3 t255)
{
	return floor(c255 * t255 / 256.0f);
}

// min(a + b, 255) per channel: fn_0080BF10 0x80BF1B..0x80BFB9 (the four channels, `cmp 0xFF` / `jb`), fn_00809DE0
// (RGB, `jle` at 0x809E01 / 0x809E19 / 0x809E35, 0x809E46 keeps a.A)
vec4 Lh3dAddSat(vec4 a255, vec4 b255)
{
	return min(a255 + b255, vec4_splat(255.0f));
}
vec3 Lh3dAddSat(vec3 a255, vec3 b255)
{
	return min(a255 + b255, vec3_splat(255.0f));
}

// trunc(c l / 255) per channel: fn_007ACF70, LH3DMist 0x7FA6DF, LH3DCreature::DrawNow 0x48EF10 (the 0x80808081
// multiply). x = c l is an exact integer (0..65025) in a float; a shader division is not correctly rounded (often
// x * rcp(255), 2.5 ULP in GLSL), so a plain floor(x / 255) can drop to k - 1 when x = 255 k. With x + 0.5 the
// quotient is k + (r + 0.5) / 255 (r = x mod 255 <= 254), its fraction in [0.002, 0.998], far from either integer.
// Not checked on a GPU yet (aproximado until then).
vec3 Lh3dMul255(vec3 c255, vec3 l255)
{
	return floor((c255 * l255 + 0.5f) / 255.0f);
}

// openblack's own transport, no original: a 0xRRGGBB packed into one float (exact up to 2^24) by the CPU, as the
// instance and u_objectLight carry it, back to 0..255 per channel (the divisions are by powers of two: exact; the
// parameter is not called "packed", a reserved word of GLSL)
vec3 Lh3dUnpackRgb24(float rgb24)
{
	float red = floor(rgb24 / 65536.0f);
	float green = floor((rgb24 - red * 65536.0f) / 256.0f);
	return vec3(red, green, rgb24 - red * 65536.0f - green * 256.0f);
}

#endif // LH3D_COLOUR_SH
