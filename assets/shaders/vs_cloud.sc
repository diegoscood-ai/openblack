$input a_position, a_texcoord0, a_normal
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_cloud;       // xy: texture atlas offset of the animation frame, z: ambient (210 / 256 for the clouds and
                            // the shrinking mists, 90 / 256 for the other ones), w: unused
uniform vec4 u_cloudLight;  // xyz: the light's direction in the mesh's own space (fn_00855340)
uniform vec4 u_cloudColour; // rgb: cloud colour, a: alpha

// A sky cloud or a map mist (mist.l3d) drawn by LH3DObject::Draw with a temporary light straight above (0, 500000, 0)
// and ambient 210 / 256 (fn_007FA300 effect branch 0x7FA3B1); mists without the effect flag keep the models' light
// (ambient 90 / 256, the light at (-500000, 500000, -500000))
void main()
{
	v_texcoord0 = vec4(a_texcoord0 + u_cloud.xy, 0.0f, 0.0f);
	// fn_0084BA90 dots the untransformed vertex normal with the light's position brought into the mesh's own space by
	// the inverse of the object matrix (so with a non-uniform scale it is not the light of a rotated normal)
	// 0x84BB90..0x84BC1D in integers: I = fistp(255 * dot), f = amb if I < 0, else amb + ((255 - amb) * I >> 8), and
	// the diffuse (c * f) >> 8 (so f is 210..254 for the clouds, never the full 256)
	float ambient = floor(u_cloud.z * 256.0f + 0.5f);
	float intensity = floor(255.0f * dot(a_normal.xyz, u_cloudLight.xyz) + 0.5f);
	float light = intensity < 0.0f ? ambient : ambient + floor((255.0f - ambient) * intensity / 256.0f);
	vec3 colour = floor(u_cloudColour.rgb * 255.0f + 0.5f);
	v_color0 = vec4(floor(colour * light / 256.0f) / 255.0f, u_cloudColour.a);
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position.xyz, 1.0f)));
}
