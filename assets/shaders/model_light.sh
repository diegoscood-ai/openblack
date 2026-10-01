#ifndef MODEL_LIGHT_SH
#define MODEL_LIGHT_SH

// The GPU side of src/Graphics/ModelLight.h: the one per-vertex model light of the original, fn_0084BA90
// (0x84BB90..0x84BC1D, on the CPU over D3DTLVERTEX), its single point light [0xEA9E90] (placed every frame by
// fn_005E5830) and its ambient [0xC39264] = 90. Wiki: rendering-objects.md, "Luz de los modelos". The CPU and this one
// must stay the same: the rounding of I (fistp, halves to even), the >> 8 of the factor and the truncation of each
// channel. Every model of the original goes through this integer rule: the float formula of the D3D T&L path
// (fn_0082C680, c (90 + 166 N.L) / 256) is unreachable in this build, because start_system writes [0xC386E4] = 1
// (0x642EA7) and so OpenD3D never sets the "Support Hardware T'n'L" flag [0xECA60C] (0x82D0F5), which DrawTnL needs
// (0x80DC7A).

uniform vec4 u_modelLight; // xyz: the light's position in the world [0xEA9E90], w: ambient 0..255 [0xC39264] = 90

// I = fistp(255 (n . l)) (0x84BBAF..0x84BBBE): rounded to the nearest, halves to even (the FPU's default mode). The
// fstp to a float32 before it (0x84BBB5) is not worth copying. `truncate` is the __ftol variant of the same rule
// (0x859649, fn_00859530 / fn_00859D90 / fn_00878C70), which cuts towards zero instead.
float ModelLightI(vec3 nLocal, vec3 lLocal, bool truncate)
{
	float lit = 255.0f * dot(nLocal, lLocal);
	if (truncate)
	{
		return lit < 0.0f ? -floor(-lit) : floor(lit);
	}
	float intensity = floor(lit + 0.5f);
	if (intensity - lit == 0.5f && mod(intensity, 2.0f) != 0.0f)
	{
		intensity -= 1.0f;
	}
	return intensity;
}

// f = I < 0 ? amb : amb + ((255 - amb) I >> 8) (0x84BBC3..0x84BBE5), so with amb = 90 it is 90 or 90..254: never the
// full 256 of a float formula
float ModelLightFactor(float intensity, float ambient)
{
	return intensity < 0.0f ? ambient : ambient + floor((255.0f - ambient) * intensity / 256.0f);
}

// The diffuse of each channel of a 0..255 colour, (c f) >> 8 truncated (0x84BBEA..0x84BC1D). The alpha is untouched:
// the caller keeps it.
vec3 ModelLightDiffuse(vec3 c255, float factor)
{
	return floor(c255 * factor / 256.0f);
}

// The light in the mesh's own space, normalised: fn_00855340 (0x855340, the rigid path) and the per-bone
// B^-1 (W2C Lpos) of the boned one (0x84BD82..0x84BDFE), which come to the same thing because the bone matrices go
// all the way to the camera (B = W2C Obj Bc) and the camera cancels out. The direction is taken from the ORIGIN of the
// bone (or of the object), not from the vertex, and it meets the raw local normal (not rotated, not normalised).
// `axisX/Y/Z` and `origin` are that space's axes and origin in the world; the inverse is the general one
// (LHMatrix::SetInverse 0x7FB290, adjugate / determinant), so the adjugate is enough here: 1 / det goes away with the
// normalisation and only its sign is kept, for the mirrored matrices.
vec3 ModelLightLocal(vec3 axisX, vec3 axisY, vec3 axisZ, vec3 origin, vec3 lightPos)
{
	vec3 toLight = lightPos - origin;
	vec3 adjugateRow0 = cross(axisY, axisZ);
	vec3 adjugateRow1 = cross(axisZ, axisX);
	vec3 adjugateRow2 = cross(axisX, axisY);
	float determinant = dot(axisX, adjugateRow0);
	vec3 local = vec3(dot(adjugateRow0, toLight), dot(adjugateRow1, toLight), dot(adjugateRow2, toLight));
	local *= determinant < 0.0f ? -1.0f : 1.0f;
	// a flattened matrix (a scale of 0) would leave 0 / 0 here; the original's InverseSquareRoot (0x841170) has its own
	// guard for it, and with no direction the vertex only takes the ambient
	float lengthSquared = dot(local, local);
	return lengthSquared > 0.0f ? local / sqrt(lengthSquared) : vec3(0.0f, 0.0f, 0.0f);
}

#endif // MODEL_LIGHT_SH
