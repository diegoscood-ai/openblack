$input v_texcoord0, v_color0, v_texcoord1

#include <bgfx_shader.sh>

// The temple's rooms (src/Graphics/RendererTemple.cpp). The game draws the lit parts of the temple unlit, with the
// lightmap on the second texture stage: the texture times the lightmap, doubled, which saturates, and the texture's
// alpha. The rest are shaded by the game's one light (vs_object_temple_instanced). In the temple's depth fog the
// vertices carry the colour in the temple's light and the colour added (v_texcoord1, w 1), fogged.
SAMPLER2D(s_diffuse, 0);
#ifdef USE_LIGHTMAP
SAMPLER2D(s_lightmap, 3);
#endif // USE_LIGHTMAP
#ifdef USE_REFLECTION
SAMPLER2D(s_reflection, 4);
#endif // USE_REFLECTION
#ifdef USE_ENVIRONMENT
// The environment map on the second texture stage, read where vs_object_temple_instanced points it
SAMPLER2D(s_environment, 5);
#endif // USE_ENVIRONMENT
uniform vec4 u_skyAlphaThreshold; // y: ALPHAREF / 255 (< 0: no alpha test), w: the stage's alpha
                                  // (render_modes::PrimitiveAlpha): 0 none, else the texture's
uniform vec4 u_materialColour;    // rgb: the material's colour, w > 0: an untextured primitive
// rgb: the temple's light, which every colour is multiplied by
uniform vec4 u_templeLight;
// rgb: added after the texture stages, as the game adds the vertices' specular: the temple's light's colour added, or
// the glow of a control under the cursor in its place
uniform vec4 u_glow;
// x: 1 = in its own colour and the temple's light alone, unshaded by the game's light (the pool's water, the map's
// markers), y: the opacity its alpha is multiplied by (over the reflection, the floor's: 1 for the main room, 0xD0 of
// 0xFF for the picture rooms)
uniform vec4 u_templeDraw;

void main()
{
	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
	}
	vec3 templeLight = u_templeLight.rgb;
	vec3 added = u_glow.rgb;
	if (v_texcoord1.w > 0.5f)
	{
		templeLight = vec3_splat(1.0f);
		added = v_texcoord1.rgb;
	}
#ifdef USE_LIGHTMAP
	// v_color0 is white unless the fog has the wall's colour there
	vec3 light = texture2D(s_lightmap, v_texcoord0.zw).rgb * 2.0f * v_color0.rgb;
#elif defined(USE_REFLECTION)
	vec3 light = v_color0.rgb;
#else
	vec3 light = u_templeDraw.x > 0.5f ? vec3_splat(1.0f) : v_color0.rgb;
#endif // USE_LIGHTMAP
	vec3 colour = min(diffuseTex.rgb * templeLight * light, vec3_splat(1.0f));
#ifdef USE_ENVIRONMENT
	// The second texture stage adds the environment map, which saturates
	colour = min(colour + texture2D(s_environment, v_texcoord0.zw).rgb, vec3_splat(1.0f));
#endif // USE_ENVIRONMENT
	colour = min(colour + added, vec3_splat(1.0f));
#ifdef USE_REFLECTION
	// The game draws a room mirrored through its floor, then blends the floor over it by the floor's alpha times the
	// floor's opacity. The reflection pass is drawn from the mirrored camera with the same projection, so it lines up on
	// the screen.
	vec3 reflection = texture2D(s_reflection, gl_FragCoord.xy * u_viewTexel.xy).rgb;
	gl_FragColor = vec4(mix(reflection, colour, diffuseTex.a * u_templeDraw.y), 1.0f);
#else
	// The alpha tested modes: ALPHAFUNC GREATEREQUAL against ALPHAREF, as fs_object
	float alphaThreshold = u_skyAlphaThreshold.y;
	if (alphaThreshold >= 0.0f && floor(diffuseTex.a * 255.0f + 0.5f) < floor(alphaThreshold * 255.0f + 0.5f))
	{
		discard;
	}
	gl_FragColor = vec4(colour, (u_skyAlphaThreshold.w > 0.5f ? diffuseTex.a : 1.0f) * u_templeDraw.y);
#endif // USE_REFLECTION
}
