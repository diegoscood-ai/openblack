$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The atmosphere's screen glow, one frame's step of its 16 x 16 cells (Graphics/AtmosGlow.h): each cell reads one pixel
// of the finished frame and moves each channel an eighth of the way to it times 4096, the eighth cut towards zero, then
// holds it to 0..0xF7000. The values are whole numbers below 2^20, so every float step here is exact.
SAMPLER2D(s_cells, 0); // the cells after the frame before (red, green, blue)
SAMPLER2D(s_frame, 1); // the finished frame

uniform vec4 u_glowGrid; // x, y: the frame's width and height in pixels; z: 1 where its first row is the bottom; w: 1 for
                         // cells still at 0

void main()
{
	ivec2 cell = ivec2(floor(gl_FragCoord.xy));
	float width = u_glowGrid.x;
	float height = u_glowGrid.y;
	// The pixel of the cell, from the top left: columns w / 16 apart from the middle of the first, rows h / 16 apart
	// from h / 32, every division a whole one
	float step = floor(width / 16.0);
	float x = float(cell.x) * step + floor(step / 2.0);
	float y = floor(float(cell.y) * height / 16.0) + floor(height / 32.0);
	float row = u_glowGrid.z > 0.5 ? height - 1.0 - y : y;
	vec3 pixel = texelFetch(s_frame, ivec2(int(x), int(row)), 0).rgb;
	vec3 target = floor(pixel * 255.0 + 0.5) * 4096.0;
	vec3 value = u_glowGrid.w > 0.5 ? vec3_splat(0.0) : texelFetch(s_cells, cell, 0).rgb;
	vec3 delta = target - value;
	value = value + sign(delta) * floor(abs(delta) / 8.0);
	gl_FragColor = vec4(clamp(value, vec3_splat(0.0), vec3_splat(1011712.0)), 0.0);
}
