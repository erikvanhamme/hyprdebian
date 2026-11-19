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

// The shapes of the pipes; see mesh.hpp.

#include "mesh.hpp"

#include <cmath>
#include <numbers>

namespace {

constexpr float PI = std::numbers::pi_v<float>;

/**
 * @brief Adds a vertex.
 * @return Its index.
 */
uint32_t add_vertex(Mesh& mesh, float x, float y, float z, float nx, float ny, float nz)
{
    mesh.vertices.push_back({{x, y, z}, {nx, ny, nz}});
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
}

/**
 * @brief Adds a triangle, counter-clockwise seen from where its normals point.
 *
 * A triangle without area, as where the rings of a ball meet at a pole, is
 * left out.
 */
void add_triangle(Mesh& mesh, uint32_t a, uint32_t b, uint32_t c)
{
    const float* pa = mesh.vertices[a].position;
    const float* pb = mesh.vertices[b].position;
    const float* pc = mesh.vertices[c].position;
    const float u[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
    const float v[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
    const float face[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    if (std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]) < 1e-9f) {
        return;
    }
    float outside = 0.0f;
    for (uint32_t i : {a, b, c}) {
        const float* n = mesh.vertices[i].normal;
        outside += face[0] * n[0] + face[1] * n[1] + face[2] * n[2];
    }
    if (outside >= 0.0f) {
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    } else {
        mesh.indices.insert(mesh.indices.end(), {a, c, b});
    }
}

/**
 * @brief Adds the triangles between rings of vertices.
 * @param first The index of the first vertex of the first ring.
 * @param rings The number of rings.
 * @param size  The vertices of a ring; the last is at the place of the first, for smooth normals at the seam.
 */
void connect_rings(Mesh& mesh, uint32_t first, int rings, int size)
{
    for (int i = 0; i + 1 < rings; i++) {
        for (int j = 0; j + 1 < size; j++) {
            const uint32_t a = first + i * size + j;
            const uint32_t b = a + 1;
            const uint32_t c = a + size;
            const uint32_t d = c + 1;
            add_triangle(mesh, a, b, d);
            add_triangle(mesh, a, d, c);
        }
    }
}

} // namespace

Mesh make_tube()
{
    Mesh mesh;
    for (float z : {-PIPE_RADIUS, PIPE_RADIUS}) {
        for (int j = 0; j <= SEGMENTS; j++) {
            const float a = 2.0f * PI * j / SEGMENTS;
            const float c = std::cos(a);
            const float s = std::sin(a);
            add_vertex(mesh, PIPE_RADIUS * c, PIPE_RADIUS * s, z, c, s, 0.0f);
        }
    }
    connect_rings(mesh, 0, 2, SEGMENTS + 1);
    return mesh;
}

Mesh make_elbow()
{
    // The middle of the bend is 1/3 back of the face where the pipe comes in
    // and 1/3 aside, towards where it goes out; the axis of the pipe goes
    // about it with a radius of 1/3, R of myElbow. The ring at the angle t
    // of the bend lies in the plane of y and of e, the direction from the
    // middle of the bend to the axis.
    const float bend_x = PIPE_RADIUS;
    const float bend_z = -PIPE_RADIUS;
    Mesh mesh;
    for (int i = 0; i <= ELBOW_RINGS; i++) {
        const float t = 0.5f * PI * i / ELBOW_RINGS;
        const float ex = -std::cos(t);
        const float ez = std::sin(t);
        for (int j = 0; j <= SEGMENTS; j++) {
            const float a = 2.0f * PI * j / SEGMENTS;
            const float nx = std::cos(a) * ex;
            const float ny = std::sin(a);
            const float nz = std::cos(a) * ez;
            add_vertex(mesh, bend_x + PIPE_RADIUS * ex + PIPE_RADIUS * nx, PIPE_RADIUS * ny,
                       bend_z + PIPE_RADIUS * ez + PIPE_RADIUS * nz, nx, ny, nz);
        }
    }
    connect_rings(mesh, 0, ELBOW_RINGS + 1, SEGMENTS + 1);
    return mesh;
}

Mesh make_ball()
{
    Mesh mesh;
    for (int i = 0; i <= BALL_RINGS; i++) {
        const float polar = PI * i / BALL_RINGS;
        const float y = std::cos(polar);
        const float r = std::sin(polar);
        for (int j = 0; j <= SEGMENTS; j++) {
            const float a = 2.0f * PI * j / SEGMENTS;
            const float x = r * std::cos(a);
            const float z = r * std::sin(a);
            add_vertex(mesh, x, y, z, x, y, z);
        }
    }
    connect_rings(mesh, 0, BALL_RINGS + 1, SEGMENTS + 1);
    return mesh;
}
