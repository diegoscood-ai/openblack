#ifndef TEMPLE_FOG_SH
#define TEMPLE_FOG_SH

// The temple's depth fog on its lightmapped walls, at each vertex: the GPU side of temple_fade::AtVertex
// (src/3D/TempleLight.h), with the same rounding. The submeshes fogged once have it done on the CPU
// (temple_fade::Once). Colours here are bytes, 0 to 255.

// x: the depth the fog starts at, y: where it ends, z: its steps in one unit of depth, w: 1 for a submesh fogged at each
// vertex, 2 for one fogged once on the CPU, 0 for none
uniform vec4 u_templeFog;
// rgb: the colour the fog adds at its end, 0 to 255
uniform vec4 u_templeFogColour;

// The step of a depth between the start and the end, 0 to 1023: the way from the start times the steps per unit,
// truncated
float TempleFogStep(float depth)
{
	return clamp(floor((depth - u_templeFog.x) * u_templeFog.z), 0.0f, 1023.0f);
}

// The share of a colour a step leaves, of 256: 256 less 156 of 1024 a step, truncated, taken as a byte of 255
float TempleFogShare(float step)
{
	float left = 256.0f - floor(156.0f * step / 1024.0f);
	return floor(255.0f * left / 256.0f);
}

// The FPU's default rounding: to nearest, halves to even
vec3 TempleFogRound(vec3 x)
{
	vec3 r = floor(x + 0.5f);
	return r - step(vec3_splat(0.5f), mod(r, vec3_splat(2.0f))) * step(vec3_splat(0.5f), r - x);
}

// The colour the fog adds at a step: the step's share of the end's colour, rounded
vec3 TempleFogAdded(float step)
{
	return TempleFogRound(u_templeFogColour.rgb * (step / 1024.0f));
}

// A wall's vertex of a colour and an added colour, fogged at its depth: nearer than the start it keeps both, from the
// end on it takes the last step's grey and colour alone, and between them each byte moves towards the step's by a
// quarter of the step over 256, rounded down
void TempleFogVertex(float depth, inout vec3 colour, inout vec3 added)
{
	if (!(depth > u_templeFog.x))
	{
		return;
	}
	if (!(depth < u_templeFog.y))
	{
		colour = vec3_splat(TempleFogShare(1023.0f));
		added = TempleFogAdded(1023.0f);
		return;
	}
	float fogStep = TempleFogStep(depth);
	float towards = floor(fogStep / 4.0f);
	colour += floor((vec3_splat(TempleFogShare(fogStep)) - colour) * towards / 256.0f);
	added += floor((TempleFogAdded(fogStep) - added) * towards / 256.0f);
}

#endif // TEMPLE_FOG_SH
