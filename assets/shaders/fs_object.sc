$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_skyAlphaThreshold; // x: sky type, y: alpha cut-out threshold, z: alpha to coverage (MSAA mod), w: blended
uniform vec4 u_materialColour;    // rgb: L3D material colour, w > 0: untextured primitive (Smooth*)

// The original lights models on the CPU (fn_0084BA90, D3DTLVERTEX): the vertex diffuse is computed in vs_object and
// the D3D stage is COLOROP = MODULATE(TEXTURE, DIFFUSE) with the specular colour added afterwards (SPECULARENABLE).
void main()
{
	float alphaThreshold = u_skyAlphaThreshold.y;
	bool alphaToCoverage = u_skyAlphaThreshold.z > 0.0f;
	bool blendedMaterial = u_skyAlphaThreshold.w > 0.0f;

	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		// untextured primitive: material colour x object colour (fn_007ACF70, 0x84BAA3)
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
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
	diffuseTex.a = (alphaThreshold > 0.0f || blendedMaterial ? diffuseTex.a : 1.0f) * v_color0.a;
	vec3 specular = vec3(v_texcoord0.zw, v_position.w); // see vs_object
	diffuseTex.rgb = diffuseTex.rgb * v_color0.rgb + specular;
	gl_FragColor = diffuseTex;
}
