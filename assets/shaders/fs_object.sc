#ifdef USE_ENVIRONMENT
$input v_position, v_texcoord0, v_color0, v_snow, v_snowLight, v_environment
#else
$input v_position, v_texcoord0, v_color0, v_snow, v_snowLight
#endif // USE_ENVIRONMENT

#include <bgfx_shader.sh>

// u_objectClip and SeaPlaneDiscard: the plane of the pass under the sea, src/Graphics/SeaPass.h
#include "sea_plane.sh"
// SnowShows and the snow's textures (src/3D/SnowCover.h)
#include "snow_object.sh"

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_skyAlphaThreshold; // x, z: unused (0), y: ALPHAREF / 255 (< 0: no alpha test),
                                  // w: stage 0 alpha (render_modes::PrimitiveAlpha): 0 none, 1 texture, 2 texture x diffuse
uniform vec4 u_materialColour;    // rgb: L3D material colour, w > 0: untextured primitive (Smooth*)
#ifdef USE_CREATURE_SPELL_LOOK
// The creature spells' looks, off until a program defines USE_CREATURE_SPELL_LOOK: the ice drawn over a frozen
// creature, and the static one fizzing out of sight is drawn through, which slides across its skin by
// u_creatureSpellLook.xy. Stages 5, 10 and 15 are free in fs_object and vs_object; a stage no draw binds gets a white
// texture when the program submits (SamplerDefaults).
SAMPLER2D(s_iceEnvironment, 10);
// The ice's alpha
SAMPLER2D(s_iceAlpha, 5);
SAMPLER2D(s_staticAlpha, 15);
// A creature fizzing out of sight is drawn twice. z 1: its depth alone, wherever the static's alpha is at least w; z 2:
// its body, blended at w, over just that depth. z 3: the ice over a frozen mesh, after the mesh. z 0 for anything else.
uniform vec4 u_creatureSpellLook;
// The ice's: the wiggle's first two rows (xyz), which the faces are measured against once taken back to the camera's
// scale, and the colour it is drawn in (creature_frozen_sheen::SheenColour)
uniform vec4 u_frozenAxisX;
uniform vec4 u_frozenAxisY;
uniform vec4 u_frozenSheenColour;
#ifdef USE_DRAW_CAMERA
// A gesture icon's model (vs_object.sc): the models' camera's view and projection, which its ice's faces are measured with
uniform mat4 u_drawViewProj;
#endif // USE_DRAW_CAMERA
#endif // USE_CREATURE_SPELL_LOOK

#ifdef USE_ENVIRONMENT
// The first environment map, on the second texture stage of an environment-mapped model (graphics::material_stages)
SAMPLER2D(s_environment, 14);
// x: how the primitive combines the map with the lit texture (material_stages::EnvironmentStage): 0 not at all, 1 in its
// place, 2 left as it was, 3 multiplied, 4 multiplied and doubled, 5 added
uniform vec4 u_environmentStage;

// The second stage over the lit texture. Added, it is the game's way on cards other than the Voodoo's: the texture
// times the light, plus the map, saturated; the specular comes after it.
vec3 EnvironmentStage(vec3 lit, vec3 environment, float stage)
{
	float chosen = floor(stage + 0.5f);
	if (chosen == 1.0f)
	{
		return environment;
	}
	if (chosen == 3.0f)
	{
		return lit * environment;
	}
	if (chosen == 4.0f)
	{
		return min(lit * environment * 2.0f, vec3_splat(1.0f));
	}
	if (chosen == 5.0f)
	{
		return min(lit + environment, vec3_splat(1.0f));
	}
	return lit;
}
#endif // USE_ENVIRONMENT

// The original lights models on the CPU (D3DTLVERTEX): the vertex diffuse is computed in vs_object and
// the D3D stage is COLOROP = MODULATE(TEXTURE, DIFFUSE) with the specular colour added afterwards (SPECULARENABLE).
void main()
{
	float alphaThreshold = u_skyAlphaThreshold.y;
	float opacity = v_color0.a;
	float alphaSource = u_skyAlphaThreshold.w;
#ifdef USE_CREATURE_SPELL_LOOK
	// Fizzing, its body's alpha is what the level leaves, in place of its own, before the alpha test as well
	if (u_creatureSpellLook.z > 1.5f && u_creatureSpellLook.z < 2.5f)
	{
		opacity = u_creatureSpellLook.w;
	}
#endif // USE_CREATURE_SPELL_LOOK

	// the side of the sea's plane this draw keeps (KeepAbove: reflections and cuts over the water, KeepBelow: the cuts
	// under it), on the real position
	if (SeaPlaneDiscard(v_position.y))
	{
		discard;
	}
#ifdef USE_CREATURE_SPELL_LOOK
	// The ice over a frozen mesh: one texture coordinate for a whole face, from its normal in the clipping space (the
	// camera's x and y times the projection's scales, z the depth), turned away from the eye as a drawn face's is,
	// against the wiggle's rows taken back to the camera's scale: (axis . normal + 1) x 255 / 512. The ice map in the
	// ice's colour, its alpha times the ice's alpha (texture x diffuse), added by that alpha
	if (u_creatureSpellLook.z > 2.5f)
	{
#ifdef USE_DRAW_CAMERA
		// a gesture icon's model: its faces with the models' camera, before the move on the screen
		vec4 clip = mul(u_drawViewProj, vec4(v_position.xyz, 1.0f));
#else
		vec4 clip = mul(u_viewProj, SeaUnmirror(vec4(v_position.xyz, 1.0f)));
#endif // USE_DRAW_CAMERA
		vec3 corner = vec3(clip.x, clip.y, clip.w);
		vec3 face = normalize(cross(dFdx(corner), dFdy(corner)));
		face = dot(face, corner) < 0.0f ? -face : face;
		vec2 scale = vec2(u_proj[0][0], u_proj[1][1]);
		vec3 axisX = vec3(u_frozenAxisX.x / scale.x, u_frozenAxisX.y / scale.y, u_frozenAxisX.z);
		vec3 axisY = vec3(u_frozenAxisY.x / scale.x, u_frozenAxisY.y / scale.y, u_frozenAxisY.z);
		vec2 iceUv = (vec2(dot(axisX, face), dot(axisY, face)) + 1.0f) * 0.498046875f;
		vec3 ice = texture2D(s_iceEnvironment, iceUv).rgb;
		float iceAlpha = texture2D(s_iceAlpha, iceUv).r;
		gl_FragColor = vec4(ice * u_frozenSheenColour.rgb, iceAlpha * u_frozenSheenColour.a);
		return;
	}
	// Fizzing, its depth goes down only where the static over its skin is strong enough, whatever its own texture
	if (u_creatureSpellLook.z > 0.5f && u_creatureSpellLook.z < 1.5f)
	{
		float staticAlpha = floor(texture2D(s_staticAlpha, v_texcoord0.xy + u_creatureSpellLook.xy).r * 255.0f + 0.5f);
		if (staticAlpha < floor(u_creatureSpellLook.w * 255.0f + 0.5f))
		{
			discard;
		}
		gl_FragColor = vec4_splat(0.0f);
		return;
	}
#endif // USE_CREATURE_SPELL_LOOK
	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		// untextured primitive: material colour x object colour
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
	}

	if (alphaThreshold >= 0.0f && floor(diffuseTex.a * (alphaSource > 1.5f ? opacity : 1.0f) * 255.0f + 0.5f) <
	                                       floor(alphaThreshold * 255.0f + 0.5f))
	{
		// the alpha tested modes: ALPHAFUNC GREATEREQUAL against ALPHAREF (render_modes::AlphaRef: the
		// material's, scaled by the object's alpha - 5 only for modes 9 / 15 with their own table), on the
		// stage 0 output: texture x diffuse alpha in 10, 11, 15, 16 (ALPHAOP MODULATE), the texture's in
		// 9 and 18 (SELECTARG1) (inferred: the alpha compared as a byte, rounded)
		discard;
	}
	// The opaque modes' textures may carry no meaningful alpha: they are opaque before fading. (aproximado) with 1 the
	// original's alpha is the texture's alone (SELECTARG1): the same while the object's alpha is 255
	diffuseTex.a = (alphaSource > 0.5f ? diffuseTex.a : 1.0f) * opacity;
	vec3 specular = vec3(v_texcoord0.zw, v_position.w); // see vs_object
	vec3 light = v_color0.rgb;
	vec3 snowTexel = texture2D(s_snow, v_snow.xy).rgb;
	float snowAlpha = texture2D(s_snowAlpha, v_snow.xy).r;
#ifdef USE_ENVIRONMENT
	diffuseTex.rgb =
	    EnvironmentStage(diffuseTex.rgb * light, texture2D(s_environment, v_environment).rgb, u_environmentStage.x) +
	    specular;
#else
	diffuseTex.rgb = diffuseTex.rgb * light + specular;
#endif // USE_ENVIRONMENT
	// Snow covers the object where it shows, its texture in its own light in place of the object's. v_snow.z is 0
	// without snow, and the colour above is kept as it is.
	float snowLevel = floor(v_snow.z * 255.0f + 0.5f);
	if (snowLevel > 0.0f && SnowShows(snowLevel, snowAlpha))
	{
		diffuseTex.rgb = snowTexel * v_snowLight + specular;
	}
	gl_FragColor = diffuseTex;
}
