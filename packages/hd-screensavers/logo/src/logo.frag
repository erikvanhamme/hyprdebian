// The fragment shader of hd-logo: the mask, filtered linearly between its
// mipmaps, tints the colour of the logo over the black background.
#version 450

layout(push_constant) uniform Push {
    vec4 rect;
    vec4 colour;
} push;

layout(set = 0, binding = 0) uniform sampler2D mask;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;

void main()
{
    colour = vec4(push.colour.rgb * texture(mask, uv).r, 1.0);
}
