$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_skyAlphaThreshold; // x: sky type, y: alpha cut-out threshold, z: alpha to coverage (MSAA mod), w: blended
uniform vec4 u_materialColour;    // rgb: L3D material colour, w > 0: untextured primitive (Smooth*)
uniform vec4 u_objectClip;        // x > 0: discard below the sea (y < 0; reflections draw only the part above water)
uniform vec4 u_window;            // y: mod graphics.hd-people lighting (1 per pixel, 2 + rim), z: its mip bias

// The original lights models on the CPU (fn_0084BA90, D3DTLVERTEX): the vertex diffuse is computed in vs_object and
// the D3D stage is COLOROP = MODULATE(TEXTURE, DIFFUSE) with the specular colour added afterwards (SPECULARENABLE).
void main()
{
	float alphaThreshold = u_skyAlphaThreshold.y;
	// Dissolving fade (components::MeshTint, vs_object: alpha -1 - opacity): a screen pattern of crosses that grow
	// from a point in every 8 x 8 pixel cell until they cover it, instead of alpha blending
	float opacity = v_color0.a;
	if (opacity < -0.5f)
	{
		opacity = -opacity - 1.0f;
		vec2 cell = abs(fract(gl_FragCoord.xy / 8.0f) - 0.5f);
		float arm = max(cell.x, cell.y);
		float thickness = min(cell.x, cell.y);
		if (opacity < 0.999f && (arm > saturate(opacity * 2.0f) * 0.5f || thickness > opacity * 0.5f))
		{
			discard;
		}
		opacity = 1.0f;
	}
	bool alphaToCoverage = u_skyAlphaThreshold.z > 0.0f;
	bool blendedMaterial = u_skyAlphaThreshold.w > 0.0f;

	if (u_objectClip.x > 0.0f && v_position.y < 0.0f)
	{
		discard;
	}
	vec4 diffuseTex = u_window.z < 0.0f ? texture2DBias(s_diffuse, v_texcoord0.xy, u_window.z)
	                                    : texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		// untextured primitive: material colour x object colour (fn_007ACF70, 0x84BAA3)
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
	}
	// components::MeshTint (vs_object puts it in the normal): grey texels x the ground colour like the world.foliage
	// plants (0.5 grey = the ground itself), mixed back towards the texture's own colour
	if (v_normal.y > 500.0f)
	{
		vec3 ground = fract(v_normal);
		float own = floor((v_normal.x - 1000.0f) / 2.0f) / 31.0f;
		float grey = dot(diffuseTex.rgb, vec3(0.299f, 0.587f, 0.114f));
		diffuseTex.rgb = mix(ground * grey * 2.0f, diffuseTex.rgb, own);
	}

	if (alphaToCoverage)
	{
		// MSAA mod: turn the cut-out into a coverage ramp about one pixel wide around the threshold
		diffuseTex.a = saturate((diffuseTex.a - alphaThreshold) / max(fwidth(diffuseTex.a), 0.0001f) + 0.5f);
		if (diffuseTex.a <= 0.0f)
		{
			discard;
		}
	}
	else if (alphaThreshold > 0.0f && diffuseTex.a * 255.0f < alphaThreshold * 255.0f - 5.0f)
	{
		// chroma materials: ALPHAFUNC GREATEREQUAL, ALPHAREF = threshold * object alpha / 255 - 5 (0x82E181)
		discard;
	}
	// Textures of primitives without alpha cut-out may carry no meaningful alpha: they are opaque before fading.
	diffuseTex.a = (alphaThreshold > 0.0f || blendedMaterial ? diffuseTex.a : 1.0f) * opacity;
	vec3 specular = vec3(v_texcoord0.zw, v_position.w); // see vs_object
	vec3 light = v_color0.rgb;
	if (u_window.y > 0.0f && v_normal.y < 500.0f)
	{
		// mod graphics.hd-people: vs_object's vertex light of the original (ambient 90/256 + 166/256 N.L), per pixel on
		// the smooth normals; mode 2 adds a rim of the object's light where the surface turns away from the eye
		vec3 normal = normalize(v_normal);
		const vec3 lightDirection = vec3(-0.57735027f, 0.57735027f, -0.57735027f);
		vec3 lit = light * (90.0f / 256.0f + 166.0f / 256.0f * max(0.0f, dot(normal, lightDirection)));
		if (u_window.y > 1.5f)
		{
			vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
			float facing = saturate(dot(normal, normalize(eye - v_position.xyz)));
			lit += light * 0.8f * pow(1.0f - facing, 2.0f);
		}
		light = lit;
	}
	diffuseTex.rgb = diffuseTex.rgb * light + specular;
	gl_FragColor = diffuseTex;
}
