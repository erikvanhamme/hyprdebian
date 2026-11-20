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

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

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

using Vec3 = std::array<float, 3>;

/** @brief Where the point @p a, @p b, @p h of a frame of @p u, @p v and @p w about @p origin is. */
Vec3 in_frame(const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& w, float a, float b, float h)
{
    return {origin[0] + a * u[0] + b * v[0] + h * w[0], origin[1] + a * u[1] + b * v[1] + h * w[1],
            origin[2] + a * u[2] + b * v[2] + h * w[2]};
}

/**
 * @brief Adds a closed prism, a polygon swept along an axis.
 *
 * The axis goes through @p origin along @p w, with @p u and @p v across it.
 * The corners of the polygon are @p radius from the axis, the first at the
 * angle @p phase from u towards v; the prism goes from @p from to @p to
 * along w. Its two ends are flat; its sides are smooth, a cylinder of
 * @p sides segments, or flat, a polygon of @p sides sides.
 */
void add_prism(Mesh& mesh, const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& w, int sides, float radius,
               float phase, float from, float to, bool smooth)
{
    const auto corner = [&](int j, float h) {
        const float a = phase + 2.0f * PI * j / sides;
        return in_frame(origin, u, v, w, radius * std::cos(a), radius * std::sin(a), h);
    };
    const auto across = [&](float a) { return in_frame({0, 0, 0}, u, v, w, std::cos(a), std::sin(a), 0.0f); };
    const auto add = [&](const Vec3& p, const Vec3& n) { return add_vertex(mesh, p[0], p[1], p[2], n[0], n[1], n[2]); };

    if (smooth) {
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (float h : {from, to}) {
            for (int j = 0; j <= sides; j++) {
                add(corner(j, h), across(phase + 2.0f * PI * j / sides));
            }
        }
        connect_rings(mesh, first, 2, sides + 1);
    } else {
        for (int j = 0; j < sides; j++) {
            // The normal of a flat side is halfway between its corners.
            const Vec3 n = across(phase + 2.0f * PI * (j + 0.5f) / sides);
            const uint32_t a = add(corner(j, from), n);
            const uint32_t b = add(corner(j + 1, from), n);
            const uint32_t c = add(corner(j, to), n);
            const uint32_t d = add(corner(j + 1, to), n);
            add_triangle(mesh, a, b, d);
            add_triangle(mesh, a, d, c);
        }
    }
    for (auto [h, sign] : {std::pair{from, -1.0f}, std::pair{to, 1.0f}}) {
        const Vec3 n = {sign * w[0], sign * w[1], sign * w[2]};
        const uint32_t middle = add(in_frame(origin, u, v, w, 0.0f, 0.0f, h), n);
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (int j = 0; j < sides; j++) {
            add(corner(j, h), n);
        }
        for (int j = 0; j < sides; j++) {
            add_triangle(mesh, middle, first + j, first + (j + 1) % sides);
        }
    }
}

/**
 * @brief The axes of the two flanges of a bolted elbow, as make_flanges() places them.
 *
 * For each the axis w, from the middle of the cell out along the link, and
 * u, the line of y in which the first bolt is, and v across both.
 */
struct FlangeAxes {
    Vec3 u;
    Vec3 v;
    Vec3 w;
};
constexpr std::array<FlangeAxes, 2> FLANGES = {{
    {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, // on the link the pipe comes in from, along -z
    {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},  // on the link it goes out on, along +x
}};

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

Mesh make_flanges()
{
    Mesh mesh;
    for (const FlangeAxes& f : FLANGES) {
        add_prism(mesh, {0, 0, 0}, f.u, f.v, f.w, SEGMENTS, FLANGE_RADIUS, 0.0f, FLANGE_NEAR, FLANGE_FAR, true);
    }
    return mesh;
}

Mesh make_bolts()
{
    Mesh mesh;
    for (const FlangeAxes& f : FLANGES) {
        for (int k = 0; k < BOLTS; k++) {
            // A bolt in a frame of its own: u out from the axis of the pipe, v
            // around it. The corners are 30° off u, so a flat faces the axis.
            const float a = 2.0f * PI * k / BOLTS;
            const Vec3 out = in_frame({0, 0, 0}, f.u, f.v, f.w, std::cos(a), std::sin(a), 0.0f);
            const Vec3 around = in_frame({0, 0, 0}, f.u, f.v, f.w, -std::sin(a), std::cos(a), 0.0f);
            const Vec3 middle = in_frame({0, 0, 0}, out, around, f.w, BOLT_CIRCLE, 0.0f, 0.0f);
            for (float face : {FLANGE_NEAR - BOLT_HEIGHT, FLANGE_FAR}) {
                add_prism(mesh, middle, out, around, f.w, 6, BOLT_RADIUS, PI / 6, face, face + BOLT_HEIGHT, false);
            }
        }
    }
    return mesh;
}
