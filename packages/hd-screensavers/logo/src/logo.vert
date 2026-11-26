// The vertex shader of hd-logo: one quad, the four corners of a triangle
// strip from gl_VertexIndex, without vertex buffers. rect is the quad in the
// coordinates of Vulkan, from the top left corner (-1, -1) to the bottom
// right one (1, 1); the mask is sampled over all of it.
#version 450

layout(push_constant) uniform Push {
    vec4 rect;   // left, top, right, bottom
    vec4 colour; // the colour of the logo, as it goes on the screen
} push;

layout(location = 0) out vec2 uv;

void main()
{
    // 0: top left, 1: top right, 2: bottom left, 3: bottom right.
    const vec2 corner = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    uv = corner;
    gl_Position = vec4(mix(push.rect.xy, push.rect.zw, corner), 0.0, 1.0);
}
