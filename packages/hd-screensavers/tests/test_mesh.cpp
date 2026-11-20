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
// seam; and that the flanges and bolts of a bolted elbow are where those of
// pipeobjs.c are, with their normals and triangles facing out.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "mesh.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <string>
#include <vector>

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

Vec3 sub(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/** @brief A closed convex solid of a mesh: its middle and its axis. */
struct Solid {
    Vec3 middle;
    Vec3 axis;
};

/**
 * @brief Checks the normals and the triangles of a mesh of closed convex solids.
 *
 * Each triangle belongs to the solid whose middle is nearest. From the
 * middle of its solid every normal points away, and every triangle is
 * counter-clockwise seen from outside.
 *
 * @return The triangles of each solid.
 */
std::vector<int> check_solids(const Mesh& m, const std::string& name, const std::vector<Solid>& solids)
{
    check(!m.vertices.empty() && !m.indices.empty() && m.indices.size() % 3 == 0, name + ": triangles");
    std::vector<int> triangles(solids.size());
    bool unit = true;
    bool out = true;
    bool ccw = true;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3 a = position(m, m.indices[t]);
        const Vec3 b = position(m, m.indices[t + 1]);
        const Vec3 c = position(m, m.indices[t + 2]);
        const Vec3 centroid = {(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3};
        std::size_t nearest = 0;
        for (std::size_t s = 1; s < solids.size(); s++) {
            if (length(sub(centroid, solids[s].middle)) < length(sub(centroid, solids[nearest].middle))) {
                nearest = s;
            }
        }
        triangles[nearest]++;
        const Vec3& middle = solids[nearest].middle;
        for (int k = 0; k < 3; k++) {
            const Vertex& v = m.vertices[m.indices[t + k]];
            const Vec3 n = {v.normal[0], v.normal[1], v.normal[2]};
            unit = unit && near(length(n), 1.0f);
            out = out && dot(n, sub(position(m, m.indices[t + k]), middle)) > 0.0f;
        }
        const Vec3 u = sub(b, a);
        const Vec3 v = sub(c, a);
        const Vec3 face = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        ccw = ccw && length(face) > 0.0f && dot(face, sub(centroid, middle)) > 0.0f;
    }
    check(unit, name + ": normals of unit length");
    check(out, name + ": the normals point out");
    check(ccw, name + ": every triangle counter-clockwise seen from outside");
    return triangles;
}

/**
 * @brief From the frame of pipeobjs.c to that of make_elbow().
 *
 * myElbow of pipes.c draws LWO_ElbowCoins and LWO_ElbowBolts after
 * glRotatef(90, 0, 0, -1), glRotatef(90, 0, 1, 0) and
 * glTranslatef(0, 1/3, 1/3), in an elbow that comes in along -y and goes
 * out along -x with the middle of the cell at (1/3, -1/3, 0); that elbow
 * turned to come in along +z and go out along +x, as make_elbow(), takes
 * (x, y, z) of pipeobjs.c to (-y, x, z).
 */
Vec3 from_pipeobjs(const Vec3& p)
{
    return {-p[1], p[0], p[2]};
}

/**
 * @brief The flanges as pipeobjs.c has them: discs of radius 0.5, 0.08
 *        thick, about its y axis from -0.6 to -0.52 and about its z axis from
 *        -0.6 to -0.52; the axis each points along, out of the cell.
 */
std::array<Solid, 2> pipeobjs_flanges()
{
    return {{
        {from_pipeobjs({0.0f, -0.56f, 0.0f}), from_pipeobjs({0.0f, -1.0f, 0.0f})},
        {from_pipeobjs({0.0f, 0.0f, -0.56f}), from_pipeobjs({0.0f, 0.0f, -1.0f})},
    }};
}

void test_flanges()
{
    const Mesh flanges = make_flanges();
    const auto solids = pipeobjs_flanges();
    bool place = true;
    int rim = 0;
    for (const Vertex& v : flanges.vertices) {
        const Vec3 p = {v.position[0], v.position[1], v.position[2]};
        const Solid& f = length(sub(p, solids[0].middle)) < length(sub(p, solids[1].middle)) ? solids[0] : solids[1];
        const float along = dot(p, f.axis);
        const Vec3 off = {p[0] - along * f.axis[0], p[1] - along * f.axis[1], p[2] - along * f.axis[2]};
        place = place && (near(along, 0.52f) || near(along, 0.6f)) && length(off) < 0.5f + 1e-5f;
        rim += near(length(off), 0.5f) ? 1 : 0;
    }
    check(place, "flanges: discs of radius 0.5 from 0.52 to 0.6 out along the links, as in pipeobjs.c");
    check(rim >= 2 * 2 * SEGMENTS, "flanges: a rim of 64 segments");
    check(FLANGE_NEAR > PIPE_RADIUS && FLANGE_FAR + BOLT_HEIGHT < CELL_SIZE / 2,
          "flanges: on the links, with their bolts in the cell");
    check(FLANGE_RADIUS > PIPE_RADIUS && BOLT_CIRCLE - BOLT_RADIUS * std::sqrt(3.0f) / 2 > PIPE_RADIUS
              && BOLT_CIRCLE + BOLT_RADIUS < FLANGE_RADIUS,
          "flanges: around the pipe, with the bolts on them");
    const auto triangles = check_solids(flanges, "flanges", {solids.begin(), solids.end()});
    check(triangles[0] == triangles[1] && triangles[0] == 4 * SEGMENTS, "flanges: a rim and two faces each");
}

void test_bolts()
{
    // pipeobjs.c has six bolts through each flange, 0.425 from the axis, the
    // first in the line of its x axis, the others 60° on: each a hexagonal
    // head 0.06 high on each face of the flange, 0.07 from the middle to the
    // corners, a flat towards the axis.
    constexpr float PI = std::numbers::pi_v<float>;
    struct Expected {
        Solid solid;
        /** @brief The direction from the axis of the pipe out to the bolt. */
        Vec3 out;
    };
    std::vector<Expected> expected;
    const auto flanges = pipeobjs_flanges();
    const std::array<std::array<Vec3, 2>, 2> across = {{
        {from_pipeobjs({1, 0, 0}), from_pipeobjs({0, 0, 1})},
        {from_pipeobjs({1, 0, 0}), from_pipeobjs({0, 1, 0})},
    }};
    for (int f = 0; f < 2; f++) {
        const Vec3& w = flanges[f].axis;
        for (int k = 0; k < 6; k++) {
            const float a = 2 * PI * k / 6;
            const auto& [u, v] = across[f];
            const Vec3 out = {std::cos(a) * u[0] + std::sin(a) * v[0], std::cos(a) * u[1] + std::sin(a) * v[1],
                              std::cos(a) * u[2] + std::sin(a) * v[2]};
            for (float h : {0.49f, 0.63f}) {
                expected.push_back({{{0.425f * out[0] + h * w[0], 0.425f * out[1] + h * w[1], 0.425f * out[2] + h * w[2]},
                                     w},
                                    out});
            }
        }
    }

    const Mesh bolts = make_bolts();
    std::vector<Solid> solids;
    for (const Expected& e : expected) {
        solids.push_back(e.solid);
    }
    const auto triangles = check_solids(bolts, "bolts", solids);
    bool all = true;
    for (int n : triangles) {
        all = all && n == 4 * 6;
    }
    check(all, "bolts: 24 hexagonal prisms where pipeobjs.c has them, 24 triangles each");

    // Every vertex in the head nearest to it: within its height, at the
    // corners of a hexagon, with the flat towards the axis of the pipe.
    bool place = true;
    for (const Vertex& v : bolts.vertices) {
        const Vec3 p = {v.position[0], v.position[1], v.position[2]};
        const Expected* e = &expected[0];
        for (const Expected& x : expected) {
            if (length(sub(p, x.solid.middle)) < length(sub(p, e->solid.middle))) {
                e = &x;
            }
        }
        const Vec3 d = sub(p, e->solid.middle);
        const float along = dot(d, e->solid.axis);
        const Vec3 off = {d[0] - along * e->solid.axis[0], d[1] - along * e->solid.axis[1],
                          d[2] - along * e->solid.axis[2]};
        const float r = length(off);
        place = place && near(std::fabs(along), 0.03f) && (r < 1e-5f || near(r, 0.07f))
                && dot(off, e->out) > -0.07f * std::sqrt(3.0f) / 2 - 1e-5f;
    }
    check(place, "bolts: heads 0.06 high, 0.07 to the corners, a flat towards the axis");
}

} // namespace

int main()
{
    test_tube();
    test_elbow();
    test_ball();
    test_flanges();
    test_bolts();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
