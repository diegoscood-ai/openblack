$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The atmosphere's screen glow's texture from its cells (Graphics/AtmosGlow.h): each channel the 4 bits above the value's
// 16th, rounded up from 0x80 for the red, 0x800 for the green and 0x8000 for the blue, alpha 15, each 4-bit channel
// widened to 8 bits (n * 17) as a 16-bit texture is sampled
SAMPLER2D(s_cells, 0);

void main()
{
	vec3 value = texelFetch(s_cells, ivec2(floor(gl_FragCoord.xy)), 0).rgb;
	vec3 nibble = floor((value + vec3(128.0, 2048.0, 32768.0)) / 65536.0);
	gl_FragColor = vec4(nibble * (17.0 / 255.0), 1.0);
}
