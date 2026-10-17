$input v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ

#include <bgfx_shader.sh>

// A vortex's decal, its depth mask (src/Graphics/RendererVortex.cpp): over vs_terrain, the land block's depth is
// written only where the mask's alpha reaches the hole's reference (render mode 18: depth only, alpha tested
// GREATEREQUAL). The block is then drawn with an equal depth test, so it shows only there.
SAMPLER2D(s13_vortexDecal, 13); // the mask's alpha, clamped
uniform vec4 u_vortexDecal;     // x: the scale of the land's texture coordinates, yz: their offset, w: the reference

void main()
{
	vec2 uv = v_texcoord0.xy * u_vortexDecal.x + u_vortexDecal.yz;
	float alpha = floor(texture2D(s13_vortexDecal, uv).r * 255.0f + 0.5f);
	if (alpha < u_vortexDecal.w)
	{
		discard;
	}
	gl_FragColor = vec4_splat(0.0f);
}
