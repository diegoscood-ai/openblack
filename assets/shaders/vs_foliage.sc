$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_color0, v_landLight, v_landSpecular

// Mod world.foliage: one plane per plant (3D/Foliage.cpp). i_data0: base xyz, width; i_data1: height, texture
// layer, land luminosity 0..1, yaw; i_data2: v of the image's top, sway, ground material, tint mode; i_data3: ground
// texture uv (one block = 0..1), lean, sway phase.

#include <bgfx_shader.sh>

SAMPLER2D(s1_landLight, 1);
SAMPLER2DARRAY(s2_materials, 2);

uniform vec4 u_foliageEye;    // xyz: camera position, w: draw distance
uniform vec4 u_foliageParams; // x: seconds, y: ground material repeats per block
uniform vec4 u_haze;          // x: near, y: far, z: k, w: on (as the terrain)
uniform vec4 u_hazeColour;    // rgb: fog colour 0..255

void main()
{
	vec3 base = i_data0.xyz;
	vec2 corner = a_position.xy;
	float plane = a_position.z;

	// the plants shrink into the ground over the last fifth of the draw distance instead of popping out
	float distanceToEye = distance(base, u_foliageEye.xyz);
	float grow = saturate((u_foliageEye.w - distanceToEye) / (0.2f * u_foliageEye.w));
	float width = i_data0.w * grow;
	float height = i_data1.x * grow;

	// a fixed direction per plant (not towards the camera), leaning a random amount so it also shows from above;
	// the optional second plane (z = 1) stands at right angles and leans the other way
	float yaw = i_data1.w + plane * 1.5707963f;
	vec3 across = vec3(cos(yaw), 0.0f, sin(yaw));
	vec3 side = vec3(-across.z, 0.0f, across.x);
	float lean = i_data3.z * (plane > 0.5f ? -1.0f : 1.0f);
	vec3 up = normalize(vec3(0.0f, 1.0f, 0.0f) + side * lean);

	// wind from one side, the tips bending the most
	float gust = sin(u_foliageParams.x * 1.6f + i_data3.w + base.x * 0.05f + base.z * 0.03f);
	vec3 sway = vec3(0.7f, 0.0f, 0.7f) * (gust * 0.12f * i_data2.y * corner.y * corner.y * height);
	vec3 position = base + across * (corner.x * width) + up * (corner.y * height) + sway;

	v_texcoord0 = vec4(corner.x + 0.5f, mix(1.0f, i_data2.x, corner.y), i_data1.y, corner.y);

	// the ground colour under the plant: its material texture at that spot, a few mip levels down (local average)
	vec3 ground = texture2DArrayLod(s2_materials, vec3(i_data3.xy * u_foliageParams.y, i_data2.z), 3.0f).rgb;
	v_color0 = vec4(ground, i_data2.w);

	// lit like the land under it: landscape light table[luminosity], then the distance haze
	vec3 light = texture2DLod(s1_landLight, vec2((floor(i_data1.z * 255.0f + 0.5f) + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;
	vec4 viewPosition = mul(u_view, vec4(position, 1.0f));
	float hazeT = u_haze.w * saturate((viewPosition.z - u_haze.x) / (u_haze.y - u_haze.x));
	v_landLight = light * (256.0f - floor((256.0f - u_haze.z) * hazeT)) / 256.0f;
	v_landSpecular = floor(u_hazeColour.rgb * hazeT + 0.5f) / 255.0f;

	gl_Position = mul(u_proj, viewPosition);
}
