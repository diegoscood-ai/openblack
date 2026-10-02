// The original's software distance haze on the GPU: the GPU side of src/Graphics/Haze.h (graphics::haze), with the
// same rounding. u_haze: x near [0xC37220], y far [0xC37224], z k [0xC37228], w on (the "Fog" key [0xC37204]);
// u_hazeColour: rgb the colour [0xC37214..0xC3721C], 0..255. Colours here are bytes, 0..255.
#ifndef HAZE_SH
#define HAZE_SH

uniform vec4 u_haze;
uniform vec4 u_hazeColour;

// t = (min(max(z, near), far) - near) / (far - near) (fn_007FEB30 0x7FEB98..0x7FEBBE, fn_00874AA0 0x874C5B..0x874C9C)
float HazeT(float depth)
{
	return (min(max(depth, u_haze.x), u_haze.y) - u_haze.x) / (u_haze.y - u_haze.x);
}

// f = 256 - __ftol((256 - k) t) (0x7FEBC3 / 0x874C90); t >= 0, so floor is the truncation
float HazeFactor(float t)
{
	return 256.0f - floor((256.0f - u_haze.z) * t);
}

// (c f) >> 8 per byte, only when f < 256 (0x7FEBED..0x7FEC30, 0x874DA1..0x874DF0)
vec3 ApplyHazeDiffuse(vec3 colour, float f)
{
	return f < 256.0f ? floor(colour * f / 256.0f) : colour;
}

// fistp with the FPU's default rounding: to nearest, halves to even
vec3 RoundHalfEven(vec3 x)
{
	vec3 r = floor(x + 0.5f);
	return r - step(vec3_splat(0.5f), mod(r, vec3_splat(2.0f))) * step(vec3_splat(0.5f), r - x);
}

// The haze colour c t with fistp (0x7FEC32..0x7FEC7E, 0x874CD2..0x874D06)
vec3 HazeColour(float t)
{
	return RoundHalfEven(u_hazeColour.rgb * t);
}

// The land's class 2 (0x874C48): [0xE9B6D8], each channel packed with __ftol (fn_007FEAD0 0x7FEB26)
vec3 HazeColourFull()
{
	return floor(u_hazeColour.rgb);
}

// a + b per channel capped at 0xFF (0x7FECA2..0x7FED0D, 0x874D21..0x874D9E)
vec3 HazeAddSaturated(vec3 a, vec3 b)
{
	return min(a + b, vec3_splat(255.0f));
}

#endif // HAZE_SH
