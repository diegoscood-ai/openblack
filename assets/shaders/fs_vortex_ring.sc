$input v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ

#include <bgfx_shader.sh>

// A vortex's decal, its ring (src/Graphics/RendererVortex.cpp): over vs_terrain, laid on the land block with an equal
// depth test in render mode 6 (SRCALPHA / INVSRCALPHA): colour = texture x the land's light + the cell colour (the
// D3D specular, as the land's), alpha = the texture's. The renderer gives this pass no haze.
SAMPLER2D(s13_vortexDecal, 13);      // the ring, clamped
SAMPLER2D(s14_vortexDecalAlpha, 14); // its alpha, clamped
uniform vec4 u_vortexDecal;          // x: the scale of the land's texture coordinates, yz: their offset
uniform vec4 u_terrainPass;          // x: the land's light scale (fs_terrain)

void main()
{
	vec2 uv = v_texcoord0.xy * u_vortexDecal.x + u_vortexDecal.yz;
	vec3 colour = texture2D(s13_vortexDecal, uv).rgb * v_landLight * u_terrainPass.x + v_landSpecular;
	gl_FragColor = vec4(colour, texture2D(s14_vortexDecalAlpha, uv).r);
}
