$input a_position, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0, v_color0, v_landLight, v_landSpecular

// Mod world.foliage: one plane per plant (3D/Foliage.cpp). i_data0: base xyz, width; i_data1: height, texture
// layer, land luminosity 0..1, yaw; i_data2: v of the image's top, sway, ground material, tint mode; i_data3: ground
// texture uv (one block = 0..1), lean, sway phase; i_data4: ground height at the left / right end, colour source
// (0 the ground texture, 1 the colour in w, 2 flat), colour r * 65536 + g * 256 + b (field crops). Flat ones: i_data3.z
// = shade of the ground colour, i_data4.xy = ground slope across / along, i_data4.w = the opacity when blended (0:
// alpha tested).

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
	// sheared to follow the ground under the two ends (the plant stays upright)
	float groundEnd = mix(i_data4.x, i_data4.y, corner.x + 0.5f) * grow;
	vec3 position = base + across * (corner.x * width) + up * (corner.y * height) + sway + vec3(0.0f, groundEnd, 0.0f);

	v_texcoord0 = vec4(corner.x + 0.5f, mix(1.0f, i_data2.x, corner.y), i_data1.y, corner.y);

	// flat (i_data4.z = 2: the beach's things, the flyers): lying on the ground, centred on the base, the image's top
	// along the side; tilted by the ground's slope across and along it (i_data4.xy). Full size; the blended ones
	// (i_data4.w > 0, their opacity) fade out instead of shrinking. The fragment shader tells them by v_texcoord0.w:
	// 2..3 blended (2 + fade times opacity), 5 alpha tested. Folding (i_data4.z = 3, the flyers, alpha tested): the quad split down the middle
	// (x = 0, the body), each half turned up about it by i_data4.w radians.
	if (i_data4.z > 1.5f)
	{
		float along = corner.y - 0.5f;
		float fold = i_data4.z > 2.5f ? i_data4.w : 0.0f;
		position = base + across * (corner.x * i_data0.w * cos(fold)) + side * (along * i_data1.x) +
		           vec3(0.0f, corner.x * i_data0.w * i_data4.x + along * i_data1.x * i_data4.y +
		                          abs(corner.x) * i_data0.w * sin(fold), 0.0f);
		v_texcoord0.w = i_data4.z < 2.5f && i_data4.w > 0.001f ? 2.0f + grow * min(i_data4.w, 1.0f) : 5.0f;
	}

	// the ground colour under the plant: its material texture at that spot, a few mip levels down (local average)
	vec3 ground = texture2DArrayLod(s2_materials, vec3(i_data3.xy * u_foliageParams.y, i_data2.z), 3.0f).rgb;
	if (i_data4.z > 0.5f && i_data4.z < 1.5f)
	{
		float packedColour = i_data4.w;
		float red = floor(packedColour / 65536.0f);
		float green = floor((packedColour - red * 65536.0f) / 256.0f);
		ground = vec3(red, green, packedColour - red * 65536.0f - green * 256.0f) / 255.0f;
	}
	// flat ones: i_data3.z scales the ground colour they take (shade, e.g. darker wet sand)
	if (i_data4.z > 1.5f && i_data3.z > 0.0f)
	{
		ground *= i_data3.z;
	}
	v_color0 = vec4(ground, i_data2.w);

	// lit like the land under it: landscape light table[luminosity], then the distance haze
	vec3 light = texture2DLod(s1_landLight, vec2((floor(i_data1.z * 255.0f + 0.5f) + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;
	vec4 viewPosition = mul(u_view, vec4(position, 1.0f));
	float hazeT = u_haze.w * saturate((viewPosition.z - u_haze.x) / (u_haze.y - u_haze.x));
	v_landLight = light * (256.0f - floor((256.0f - u_haze.z) * hazeT)) / 256.0f;
	v_landSpecular = floor(u_hazeColour.rgb * hazeT + 0.5f) / 255.0f;

	gl_Position = mul(u_proj, viewPosition);
}
