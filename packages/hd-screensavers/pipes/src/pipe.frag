/*
 * Based on hacks/glx/pipes.c of xscreensaver, from xlockmore, which carries
 * this notice:
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 *   This file is provided AS IS with no warranties of any kind.  The author
 *   shall have no liability with respect to the infringement of copyrights,
 *   trade secrets or any patents by this file or any part thereof.  In no
 *   event will the author be liable for any lost revenue or profits or
 *   other special, indirect and consequential damages.
 *
 *   This program was inspired on a WindowsNT(R)'s screen saver. It was written
 *   from scratch and it was not based on any other source code.
 *
 *   ==========================================================================
 *   The routine myElbow is derivated from the doughnut routine from the MesaGL
 *   library (more especifically the Mesaaux library) written by Brian Paul.
 *   ==========================================================================
 *
 *   Thanks goes to Brian Paul for making it possible and inexpensive to use
 *   OpenGL at home.
 *
 *   Since I'm not a native English speaker, my apologies for any grammatical
 *   mistake.
 *
 *   My e-mail address is
 *   m-vianna@usa.net
 *   Marcelo F. Vianna (Apr-09-1997)
 */

// The fragment shader of the pipes: the lighting of OpenGL 1.x with the
// lights and materials of pipes.c, per pixel. In gamma space, as OpenGL 1.x
// lit: the colour goes to the screen as it comes out, on an image of 8 bit
// UNORM. See pipes.hpp.

#version 450

layout(location = 0) in vec3 eye_normal;
layout(location = 1) flat in uint system_colour;

layout(location = 0) out vec4 colour;

// The diffuse colours of the systems: MaterialRed, MaterialGreen,
// MaterialBlue, MaterialCyan, MaterialYellow, MaterialMagenta and
// MaterialWhite, in the order of the colours of the walk; then that of the
// bolts, MaterialGray.
const vec3 DIFFUSE[8] = vec3[](
    vec3(0.7, 0.0, 0.0),
    vec3(0.1, 0.5, 0.2),
    vec3(0.0, 0.0, 0.7),
    vec3(0.2, 0.5, 0.7),
    vec3(0.7, 0.7, 0.0),
    vec3(0.6, 0.2, 0.5),
    vec3(0.7, 0.7, 0.7),
    vec3(0.2, 0.2, 0.2));

// Only GL_LIGHT0 is on, a directional light from position0 (1, 1, 1), set
// with the modelview at the identity, so in eye space. The viewer is far,
// along +z, as without GL_LIGHT_MODEL_LOCAL_VIEWER; HALF is the half vector
// of LIGHT and (0, 0, 1).
const vec3 LIGHT = vec3(0.5773503, 0.5773503, 0.5773503);
const vec3 HALF = vec3(0.3250576, 0.3250576, 0.8880738);
// The ambient light: the default ambient of the material, 0.2, times
// lmodel_ambient, 0.5, and ambient0, 0.4.
const float AMBIENT = 0.2 * (0.5 + 0.4);
// front_specular, 0.7, times the specular of GL_LIGHT0, 1, and front_shininess.
const float SPECULAR = 0.7;
const float SHININESS = 60.0;

void main()
{
    vec3 n = normalize(eye_normal);
    // GL_LIGHT_MODEL_TWO_SIDE: a back face is lit from its own side.
    if (!gl_FrontFacing) {
        n = -n;
    }
    vec3 c = vec3(AMBIENT);
    float diffuse = dot(n, LIGHT);
    if (diffuse > 0.0) {
        c += diffuse * DIFFUSE[system_colour] + SPECULAR * pow(max(dot(n, HALF), 0.0), SHININESS);
    }
    colour = vec4(min(c, vec3(1.0)), 1.0);
}
