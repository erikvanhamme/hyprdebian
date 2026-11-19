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

// The vertex shader of the pipes: puts a vertex of a mesh where its
// instance, a piece of a pipe, is, and gives the fragment shader the normal
// in eye space, where the light of pipes.c is. See pipes.hpp.

#version 450

// The transformations of draw_pipes: modelview takes the units of pipes.c
// to eye space, projection is that of gluPerspective. modelview turns and
// scales, the same on every axis, so it also takes the normals.
layout(push_constant) uniform Transform {
    mat4 modelview;
    mat4 projection;
} transform;

// The vertex of the mesh.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
// The instance: where the axes of the mesh go, scaled, and its origin, and the colour of the system.
layout(location = 2) in vec3 axis_x;
layout(location = 3) in vec3 axis_y;
layout(location = 4) in vec3 axis_z;
layout(location = 5) in vec3 origin;
layout(location = 6) in uint colour;

layout(location = 0) out vec3 eye_normal;
layout(location = 1) flat out uint system_colour;

void main()
{
    mat3 model = mat3(axis_x, axis_y, axis_z);
    vec4 eye = transform.modelview * vec4(model * position + origin, 1.0);
    // Not of unit length, glEnable(GL_NORMALIZE): the fragment shader normalizes.
    eye_normal = mat3(transform.modelview) * (model * normal);
    system_colour = colour;
    gl_Position = transform.projection * eye;
}
