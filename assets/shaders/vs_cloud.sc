$input a_position, a_texcoord0, a_normal
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_cloud;       // xy: texture atlas offset of the animation frame, z: ambient (210 / 256), w: light (0: from
                            // above, the clouds and the shrinking mists; 1: the models' light, the other mists)
uniform vec4 u_cloudColour; // rgb: cloud colour, a: alpha

// A sky cloud or a map mist (mist.l3d) drawn by LH3DObject::Draw with a temporary light straight above (0, 500000, 0)
// and ambient 210 / 256 (fn_007FA300 effect branch 0x7FA3B1); mists without the effect flag keep the models' light
// (ambient 90 / 256, the light at (-500000, 500000, -500000))
void main()
{
	v_texcoord0 = vec4(a_texcoord0 + u_cloud.xy, 0.0f, 0.0f);
	vec3 normal = normalize(mul(u_model[0], vec4(a_normal.xyz, 0.0f)).xyz + vec3_splat(1e-6f));
	vec3 lightDirection = u_cloud.w > 0.5f ? vec3(-0.57735027f, 0.57735027f, -0.57735027f) : vec3(0.0f, 1.0f, 0.0f);
	float light = u_cloud.z + (1.0f - u_cloud.z) * max(0.0f, dot(normal, lightDirection));
	v_color0 = vec4(u_cloudColour.rgb * light, u_cloudColour.a);
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position.xyz, 1.0f)));
}
