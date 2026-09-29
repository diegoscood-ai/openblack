$input v_texcoord0, v_color0, v_landLight, v_landSpecular

#include <bgfx_shader.sh>

SAMPLER2DARRAY(s0_foliage, 0);

void main()
{
	vec4 texel = texture2DArray(s0_foliage, vec3(v_texcoord0.xy, floor(v_texcoord0.z + 0.5f)));
	// a ragged base instead of the image's straight bottom edge: each column of texels starts a little higher, so
	// the blades seem to come out of the ground
	float column = floor(v_texcoord0.x * 96.0f);
	float ragged = fract(sin(column * 12.9898f + v_texcoord0.z * 78.233f) * 43758.5453f);
	if (texel.a < 0.35f || v_texcoord0.w < 0.09f * ragged)
	{
		discard;
	}

	// Tint (Foliage::Tint, v_color0.a): 0 the image's colours, 1 only grey texels take the ground colour, 2 all of
	// them. The grey level scales the ground colour (0.5 = the ground itself), darker at the base, lighter at the tips.
	float high = max(texel.r, max(texel.g, texel.b));
	float low = min(texel.r, min(texel.g, texel.b));
	float saturation = (high - low) / max(high, 0.0001f);
	float grey = dot(texel.rgb, vec3(0.299f, 0.587f, 0.114f));
	float mode = v_color0.a;
	float amount = mode > 1.5f ? 1.0f : (mode > 0.5f ? 1.0f - smoothstep(0.1f, 0.2f, saturation) : 0.0f);
	vec3 tinted = v_color0.rgb * grey * 2.0f * mix(0.8f, 1.15f, v_texcoord0.w);
	vec3 colour = mix(texel.rgb, tinted, amount);

	// a sharp edge from the alpha (for alpha to coverage with MSAA; plain alpha test otherwise)
	float alpha = saturate((texel.a - 0.35f) / max(fwidth(texel.a), 0.0001f) + 0.5f);
	gl_FragColor = vec4(colour * v_landLight + v_landSpecular, alpha);
}
