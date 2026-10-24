#ifdef USE_LIGHTMAP
$input a_position, a_texcoord0, a_texcoord1, i_data0, i_data1, i_data2, i_data3
#else
$input a_position, a_texcoord0, a_normal, i_data0, i_data1, i_data2, i_data3
#endif // USE_LIGHTMAP
$output v_texcoord0, v_color0, v_texcoord1

// The temple's rooms and what the renderer draws in them (src/Graphics/RendererTemple.cpp). Their meshes have no
// bones: a door's leaf turns by the one model matrix of its draw.
#define BGFX_CONFIG_MAX_BONES 1

#include <bgfx_shader.sh>

#ifndef USE_LIGHTMAP
// ModelLightLocal / ModelLightI / ModelLightFactor / ModelLightDiffuse and u_modelLight: the GPU side of
// src/Graphics/ModelLight.h
#include "model_light.sh"
#endif // USE_LIGHTMAP
// TempleFogVertex, u_templeFog and u_templeFogColour: the temple's depth fog (src/3D/TempleLight.h, temple_fade)
#include "temple_fog.sh"

// x: how far back, as a fraction of its depth, the mesh is pushed towards the far plane at 0: the temple's rooms other
// than the one the player is in, which overlap it at the doorways
uniform vec4 u_depthBias;
// xy: how far the texture has slid across the mesh, as the creature's room's waterfall does
uniform vec4 u_uvOffset;
// rgb: the temple's light, which every colour is multiplied by; fogged once, the colour in whole bytes
uniform vec4 u_templeLight;
// rgb: added after the texture stages, the temple's light's colour added or a control's glow; fogged once, with the
// fog's colour added
uniform vec4 u_glow;

void main()
{
	mat4 model;
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f);
	vec4 world = instMul(model, mul(u_model[0], vec4(a_position.xyz, 1.0f)));

#ifdef USE_LIGHTMAP
	// Lit by its lightmap alone, read with the second texture coordinates
	v_texcoord0 = vec4(a_texcoord0 + u_uvOffset.xy, a_texcoord1.xy);
	v_color0 = vec4_splat(1.0f);
	// w: 0 unfogged, the fragment taking the light and its added colour from the uniforms; 1 fogged, from the vertex
	v_texcoord1 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	if (u_templeFog.w > 0.5f)
	{
		// The wall's colour in the temple's light and its added colour, each byte moved towards the fog's at the vertex
		vec3 colour = floor(u_templeLight.rgb * 255.0f + 0.5f);
		vec3 added = floor(u_glow.rgb * 255.0f + 0.5f);
		TempleFogVertex(mul(u_view, world).z, colour, added);
		v_color0 = vec4(colour / 255.0f, 1.0f);
		v_texcoord1 = vec4(added / 255.0f, 1.0f);
	}
#else
	// A white object shaded by the game's one light, from the origin of the mesh, against the vertex's own normal
	vec3 axisX = instMul(model, vec4(mul(u_model[0], vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 axisY = instMul(model, vec4(mul(u_model[0], vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 axisZ = instMul(model, vec4(mul(u_model[0], vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz, 0.0f)).xyz;
	vec3 origin = instMul(model, mul(u_model[0], vec4(0.0f, 0.0f, 0.0f, 1.0f))).xyz;
	vec3 lightLocal = ModelLightLocal(axisX, axisY, axisZ, origin, u_modelLight.xyz);
	float factor = ModelLightFactor(ModelLightI(a_normal.xyz, lightLocal, false), u_modelLight.w);
	v_texcoord0 = vec4(a_texcoord0 + u_uvOffset.xy, 0.0f, 0.0f);
#ifdef USE_ENVIRONMENT
	// The environment-mapped mode: the environment map's coordinates are where the normal points across and up the
	// camera's view, from 0 to 0.498
	vec3 worldNormal = instMul(model, vec4(mul(u_model[0], vec4(a_normal.xyz, 0.0f)).xyz, 0.0f)).xyz;
	vec3 viewNormal = normalize(mul(u_view, vec4(worldNormal, 0.0f)).xyz);
	v_texcoord0.zw = (viewNormal.xy + 1.0f) * 0.498046875f;
#endif // USE_ENVIRONMENT
	v_color0 = vec4(ModelLightDiffuse(vec3_splat(255.0f), factor) / 255.0f, 1.0f);
	v_texcoord1 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	if (u_templeFog.w > 1.5f)
	{
		// Fogged once: its colour in the temple's light, in whole bytes, is shaded by the light after the fog, and the
		// colour added comes as it is
		v_color0 = vec4(ModelLightDiffuse(floor(u_templeLight.rgb * 255.0f + 0.5f), factor) / 255.0f, 1.0f);
		v_texcoord1 = vec4(u_glow.rgb, 1.0f);
	}
#endif // USE_LIGHTMAP

	gl_Position = mul(u_viewProj, world);
	gl_Position.z *= 1.0f - u_depthBias.x;
}
