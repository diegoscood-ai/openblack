$input v_texcoord0

// River channel footprint (data\river.l3d) into the land alpha target, blended with MIN (fn_00872AB0: the block
// texture's alpha nibble becomes min(dst, src)). Only the footprint's alpha is used; nearest texel, 4-bit like the
// original's ARGB4444.

#include <bgfx_shader.sh>

SAMPLER2D(s_footprint, 0);
uniform vec4 u_footprintSize; // xy: footprint texture size in texels

void main()
{
	vec2 texel = (floor(v_texcoord0.xy * u_footprintSize.xy) + 0.5f) / u_footprintSize.xy;
	float alpha = floor(texture2D(s_footprint, texel).a * 15.0f + 0.5f) / 15.0f;
	gl_FragColor = vec4(alpha, alpha, alpha, alpha);
}
