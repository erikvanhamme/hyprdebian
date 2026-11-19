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

// The tests of the shapes of hd-pipes, pipes/src/mesh.cpp, after pipes.c:
// that the tube, the elbow and the ball have the size and place of those of
// pipes.c, that their normals are of unit length and point out, that every
// triangle faces out, and that the ends of the elbow meet a tube without a
// seam.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "mesh.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

namespace {

int failures = 0;

/** @brief Counts and prints a check that fails. */
void check(bool ok, const std::string& what)
{
    if (!ok) {
        failures++;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

using Vec3 = std::array<float, 3>;

bool near(float a, float b)
{
    return std::fabs(a - b) < 1e-5f;
}

float length(const Vec3& v)
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

Vec3 position(const Mesh& m, uint32_t i)
{
    return {m.vertices[i].position[0], m.vertices[i].position[1], m.vertices[i].position[2]};
}

/**
 * @brief Checks the normals and the triangles of a mesh.
 * @param outward The direction out of the shape at a point on it, worked out
 *                from its shape and not from the normals of the mesh.
 */
void check_mesh(const Mesh& m, const std::string& name, const std::function<Vec3(const Vec3&)>& outward)
{
    check(!m.vertices.empty() && !m.indices.empty() && m.indices.size() % 3 == 0, name + ": triangles");
    bool unit = true;
    bool out = true;
    for (const Vertex& v : m.vertices) {
        const Vec3 n = {v.normal[0], v.normal[1], v.normal[2]};
        const Vec3 o = outward({v.position[0], v.position[1], v.position[2]});
        unit = unit && near(length(n), 1.0f);
        out = out && n[0] * o[0] + n[1] * o[1] + n[2] * o[2] >= 0.99f * length(o);
    }
    check(unit, name + ": normals of unit length");
    check(out, name + ": the normals are those of the shape");

    bool ccw = true;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3 a = position(m, m.indices[t]);
        const Vec3 b = position(m, m.indices[t + 1]);
        const Vec3 c = position(m, m.indices[t + 2]);
        const Vec3 u = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const Vec3 v = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const Vec3 face = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const Vec3 middle = {(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3};
        const Vec3 o = outward(middle);
        ccw = ccw && length(face) > 0.0f && face[0] * o[0] + face[1] * o[1] + face[2] * o[2] > 0.0f;
    }
    check(ccw, name + ": every triangle counter-clockwise seen from outside");
}

/** @brief Whether the ring of a tube has a vertex at (x, y). */
bool on_tube_ring(const Mesh& tube, float x, float y)
{
    for (const Vertex& v : tube.vertices) {
        if (near(v.position[0], x) && near(v.position[1], y)) {
            return true;
        }
    }
    return false;
}

void test_tube()
{
    const Mesh tube = make_tube();
    check(tube.vertices.size() == 2 * (SEGMENTS + 1), "tube: two rings of 64 segments");
    check(tube.indices.size() == 3 * 2 * SEGMENTS, "tube: two triangles per segment");
    bool shape = true;
    for (const Vertex& v : tube.vertices) {
        shape = shape && near(std::hypot(v.position[0], v.position[1]), PIPE_RADIUS) &&
                near(std::fabs(v.position[2]), PIPE_RADIUS);
    }
    check(shape, "tube: radius 1/3, from z = -1/3 to 1/3");
    check_mesh(tube, "tube", [](const Vec3& p) { return Vec3{p[0], p[1], 0.0f}; });
}

void test_elbow()
{
    const Mesh elbow = make_elbow();
    const Mesh tube = make_tube();
    check(elbow.vertices.size() == (ELBOW_RINGS + 1) * (SEGMENTS + 1), "elbow: 25 rings of 64 segments");
    // The axis of the pipe goes about (1/3, y, -1/3) with a radius of 1/3,
    // the radius of the pipe, so the inside of the bend is one point; out of
    // it is every way.
    const auto outward = [](const Vec3& p) {
        const float dx = p[0] - PIPE_RADIUS;
        const float dz = p[2] + PIPE_RADIUS;
        const float d = std::hypot(dx, dz);
        if (d < 1e-4f) {
            return Vec3{0.0f, 0.0f, 0.0f};
        }
        const Vec3 axis = {PIPE_RADIUS + dx / d * PIPE_RADIUS, 0.0f, -PIPE_RADIUS + dz / d * PIPE_RADIUS};
        return Vec3{p[0] - axis[0], p[1] - axis[1], p[2] - axis[2]};
    };
    bool radius = true;
    for (const Vertex& v : elbow.vertices) {
        const float r = length(outward({v.position[0], v.position[1], v.position[2]}));
        radius = radius && (r == 0.0f || near(r, PIPE_RADIUS));
    }
    check(radius, "elbow: a pipe of radius 1/3");
    check_mesh(elbow, "elbow", outward);

    // The first ring is the end of a tube along z, at the face z = -1/3; the
    // last that of a tube along x, at the face x = 1/3.
    bool in = true;
    bool out = true;
    for (int j = 0; j <= SEGMENTS; j++) {
        const Vertex& first = elbow.vertices[j];
        const Vertex& last = elbow.vertices[ELBOW_RINGS * (SEGMENTS + 1) + j];
        in = in && near(first.position[2], -PIPE_RADIUS) && on_tube_ring(tube, first.position[0], first.position[1]);
        out = out && near(last.position[0], PIPE_RADIUS) && on_tube_ring(tube, last.position[1], last.position[2]);
    }
    check(in, "elbow: comes in where a tube along z ends, vertex on vertex");
    check(out, "elbow: goes out where a tube along x starts, vertex on vertex");
}

void test_ball()
{
    const Mesh ball = make_ball();
    bool shape = true;
    for (const Vertex& v : ball.vertices) {
        shape = shape && near(length({v.position[0], v.position[1], v.position[2]}), 1.0f);
    }
    check(shape, "ball: radius 1");
    check_mesh(ball, "ball", [](const Vec3& p) { return p; });
}

} // namespace

int main()
{
    test_tube();
    test_elbow();
    test_ball();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
