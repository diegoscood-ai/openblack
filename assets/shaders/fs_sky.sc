$input v_position, v_texcoord0, v_normal, v_color0, v_snow, v_snowLight

#include <bgfx_shader.sh>

// The sky dome's three textures, one layer per alignment from evil to good: the day / dusk / night blend is already in
// them, built on the CPU by sky_type::DomeBlend
SAMPLER2DARRAY(s_diffuse, 0);
// x, y: the layers of the two alignments' domes mixed. z: how much of the second there is, of 255
uniform vec4 u_skyAlignment;
// The colours the domes are drawn in: their pictures times the first, with the second added
uniform vec4 u_skyModulate;
uniform vec4 u_skyAdd;

// A dome drawn in the frame's colours
vec3 Drawn(vec3 dome)
{
	return clamp(dome * u_skyModulate.rgb + u_skyAdd.rgb, 0.0f, 1.0f);
}

// The first alignment's dome drawn whole, and the second over it
void main()
{
	vec3 lower = Drawn(texture2DArray(s_diffuse, vec3(v_texcoord0.xy, u_skyAlignment.x)).rgb);
	vec3 upper = Drawn(texture2DArray(s_diffuse, vec3(v_texcoord0.xy, u_skyAlignment.y)).rgb);
	// With all of the weight the second dome alone, as the game then draws only that one
	vec3 colour = upper;
	if (u_skyAlignment.z < 255.0f)
	{
		colour = mix(lower, upper, u_skyAlignment.z / 255.0f);
	}
	gl_FragColor = vec4(colour, 1.0f);
}
