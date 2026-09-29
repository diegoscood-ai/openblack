$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1);
SAMPLER2D(s_reflection, 2);

uniform vec4 u_sky;
uniform vec4 u_seaColour; // rgb: landscape light table[255] (the sea vertex colour); negative if unavailable
uniform vec4 u_seaParams; // x: tiling period, y: frame counter, zw: normalised horizontal camera forward
uniform vec4 u_waterMod;  // living water mod: x on/off, y time in seconds (wraps at 1000); z: sea texture repeats
                          // (terrain-x2 mod; 1 in the original)

void main()
{	// unpack uniforms
	float skyType = u_sky.x;

	float skyBightness = skyType / 2.0f;

	float depth = v_texcoord0.z;

	// Ripple: the original draws the sea as screen rows 2 px apart, each moved along the camera forward by
	// 0.9 * sin(i * pi / 8), i = (frame + 2 * (row + 1)) & 15; full beyond view depth 70, none closer than 30.
	float row = floor(gl_FragCoord.y / 2.0f);
	float phase = mod(u_seaParams.y + 2.0f * (row + 1.0f), 16.0f);
	float ripple = 0.9f * sin(phase * 3.14159265f / 8.0f) * saturate((depth - 30.0f) / 40.0f);
	// (with the terrain-x2 mod's shorter period the ripple shrinks with it, so it moves the texture as much as before)
	vec2 uv = (v_texcoord0.xy + u_seaParams.zw * ripple / u_waterMod.z) / u_seaParams.x;
	if (u_waterMod.x > 0.0f)
	{
		// Living water mod: no per-row ripple (static lines while paused, a fast shimmer at modern frame rates);
		// instead sky.raw and skya.raw drift together in a loop, like the original's wind scroll (multiples of 1/1000
		// texture per time unit, so the loop at 1000 is seamless)
		uv = v_texcoord0.xy / u_seaParams.x + vec2(0.020f, 0.012f) * u_waterMod.y;
	}

	// Light: the time-of-day full-light colour, landscape light table entry 255 (0xEDDD08)
	vec3 light = u_seaColour.r >= 0.0f ? u_seaColour.rgb : vec3_splat(mix(0.25f, 1.0f, skyBightness));
	vec3 diffuse_colour = light * texture2D(s_diffuse, uv).rgb;
	// Behind the sea: sky and the mirrored landscape (LandRef)
	vec2 reflectionUv = v_texcoord1.xy / v_texcoord1.w;
	if (u_waterMod.x > 0.0f)
	{
		// Mod: two layers of skya.raw scrolling in different directions act as a wave height map that bends the
		// reflection, less towards the horizon so that the far sea stays calm. The speeds are multiples of 1/1000
		// texture per second, so both layers are back where they started when the time wraps at 1000 s.
		float t = u_waterMod.y;
		vec2 p = v_texcoord0.xy / 40.0f;
		float h1 = texture2D(s_alpha, p + vec2(0.060f, 0.035f) * t).r;
		float h2 = texture2D(s_alpha, p * 1.7f + vec2(-0.045f, 0.070f) * t).r;
		vec2 wave = vec2(h1 - 0.5f, h2 - 0.5f);
		float strength = 0.03f * saturate(1.0f - depth / 1500.0f);
		reflectionUv += wave * strength;
	}
	vec3 reflect_colour = texture2D(s_reflection, reflectionUv).rgb;

	// sky.raw is blended SRCALPHA / INVSRCALPHA with alpha = skya.raw * vertex alpha, the vertex alpha being 255 up to
	// view depth 7000 and falling linearly to 80 at 14000 (0xC39908), then constant.
	float reflection_alpha = texture2D(s_alpha, uv).r;
	float horizon_alpha = mix(1.0f, 80.0f / 255.0f, saturate((depth - 7000.0f) / 7000.0f));

	float alpha = reflection_alpha * horizon_alpha;

	gl_FragColor = vec4(mix(reflect_colour, diffuse_colour, alpha), 1.0f);
}
