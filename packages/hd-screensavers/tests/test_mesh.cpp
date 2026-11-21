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
// seam; that the flanges and bolts of a bolted elbow and the valves and
// gauges of the factory are where those of pipeobjs.c are, with their
// normals and triangles facing out; and that the teapot is where
// unit_teapot of teapot.c puts it, faces out and is closed where its
// patches meet.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <numbers>
#include <string>
#include <tuple>
#include <utility>
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

/**
 * @brief A part of a shape: where its surface is, and which way is out.
 *
 * For a convex part out is away from its middle; for the others it is
 * worked out from their shape.
 */
struct Part {
    std::string name;
    /** @brief Whether a point is on the surface of the part. */
    std::function<bool(const Vec3&)> on;
    /** @brief The direction out of the part at a point on it. */
    std::function<Vec3(const Vec3&)> outward;
};

/**
 * @brief Checks the normals and the triangles of a mesh of parts.
 *
 * Each triangle belongs to the first part that all three of its vertices
 * are on; where two parts meet in a plane, as the end of a spindle on a
 * stem, the narrower comes first. Every normal points out of its part, and
 * every triangle is counter-clockwise seen from outside.
 *
 * @return The triangles of each part.
 */
std::vector<int> check_parts(const Mesh& m, const std::string& name, const std::vector<Part>& parts)
{
    check(!m.vertices.empty() && !m.indices.empty() && m.indices.size() % 3 == 0, name + ": triangles");
    std::vector<int> triangles(parts.size());
    bool placed = true;
    bool unit = true;
    bool out = true;
    bool ccw = true;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3 a = position(m, m.indices[t]);
        const Vec3 b = position(m, m.indices[t + 1]);
        const Vec3 c = position(m, m.indices[t + 2]);
        std::size_t part = 0;
        while (part < parts.size() && !(parts[part].on(a) && parts[part].on(b) && parts[part].on(c))) {
            part++;
        }
        if (part == parts.size()) {
            placed = false;
            continue;
        }
        triangles[part]++;
        const Part& p = parts[part];
        for (int k = 0; k < 3; k++) {
            const Vertex& v = m.vertices[m.indices[t + k]];
            const Vec3 n = {v.normal[0], v.normal[1], v.normal[2]};
            unit = unit && near(length(n), 1.0f);
            out = out && dot(n, p.outward(position(m, m.indices[t + k]))) > 0.0f;
        }
        const Vec3 u = sub(b, a);
        const Vec3 v = sub(c, a);
        const Vec3 face = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const Vec3 centroid = {(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3};
        ccw = ccw && length(face) > 0.0f && dot(face, p.outward(centroid)) > 0.0f;
    }
    check(placed, name + ": every triangle on one of its parts, as in pipeobjs.c");
    check(unit, name + ": normals of unit length");
    check(out, name + ": the normals point out");
    check(ccw, name + ": every triangle counter-clockwise seen from outside");
    for (std::size_t i = 0; i < parts.size(); i++) {
        check(triangles[i] > 0, name + ": the " + parts[i].name);
    }
    return triangles;
}

/** @brief Whether @p x is from @p from to @p to, give or take a little. */
bool within(float x, float from, float to)
{
    return x > from - 1e-5f && x < to + 1e-5f;
}

/** @brief The point @p p in the frame of an axis along the unit vector @p w through @p origin: how far along, and how far off. */
std::pair<float, float> axial(const Vec3& p, const Vec3& origin, const Vec3& w)
{
    const Vec3 d = sub(p, origin);
    const float along = dot(d, w);
    return {along, length({d[0] - along * w[0], d[1] - along * w[1], d[2] - along * w[2]})};
}

/** @brief A closed cylinder about the axis along @p w through @p origin, of @p radius, from @p from to @p to along it. */
Part cylinder(const std::string& name, const Vec3& origin, const Vec3& w, float radius, float from, float to)
{
    const float middle = (from + to) / 2;
    return {name,
            [=](const Vec3& p) {
                const auto [along, off] = axial(p, origin, w);
                return (near(off, radius) && within(along, from, to)) ||
                       ((near(along, from) || near(along, to)) && off < radius + 1e-5f);
            },
            [=](const Vec3& p) {
                return sub(p, {origin[0] + middle * w[0], origin[1] + middle * w[1], origin[2] + middle * w[2]});
            }};
}

/**
 * @brief A closed octagonal prism about the z axis, from @p from to @p to,
 *        with a flat @p apothem from the axis towards ±x, ±y and the
 *        diagonals.
 */
Part octagon(const std::string& name, float apothem, float from, float to)
{
    return {name,
            [=](const Vec3& p) {
                const float d = std::max({std::fabs(p[0]), std::fabs(p[1]),
                                          (std::fabs(p[0]) + std::fabs(p[1])) / std::sqrt(2.0f)});
                return (near(d, apothem) && within(p[2], from, to)) ||
                       ((near(p[2], from) || near(p[2], to)) && d < apothem + 1e-5f);
            },
            [=](const Vec3& p) { return Vec3{p[0], p[1], p[2] - (from + to) / 2}; }};
}

void test_body()
{
    // LWO_PipeBetweenBolts: a cylinder along z of radius 0.5 from -0.5 to 0.5, closed.
    const auto triangles = check_parts(make_body(), "body", {cylinder("cylinder", {0, 0, 0}, {0, 0, 1}, 0.5f, -0.5f, 0.5f)});
    check(triangles[0] == 4 * SEGMENTS, "body: a side and two ends of 64 segments");
}

void test_end_plates()
{
    // LWO_Bolts3D: octagons with flats 0.4 from the axis towards x and y, from 0.5 to 0.6 and from
    // -0.6 to -0.5; flat sides.
    const Mesh plates = make_end_plates();
    const auto triangles = check_parts(plates, "end plates",
                                       {octagon("plate at +z", 0.4f, 0.5f, 0.6f), octagon("plate at -z", 0.4f, -0.6f, -0.5f)});
    check(triangles[0] == 32 && triangles[1] == 32, "end plates: 8 flat sides and two ends each");
    bool flat = true;
    for (const Vertex& v : plates.vertices) {
        const float n = std::max(std::fabs(v.normal[0]), std::fabs(v.normal[1]));
        flat = flat && (near(std::fabs(v.normal[2]), 1.0f) || near(n, 1.0f) || near(n, std::sqrt(0.5f)));
    }
    check(flat, "end plates: the normals of flat sides, towards the axes and the diagonals");
}

void test_wheel()
{
    // LWO_BigValve, about the x axis: a stem of radius 0.2 from 0.42 to 0.54, a spindle of 0.1 to
    // 0.8, four spokes of 0.06 along ±y and ±z from 0.06 to 0.55 in the plane x = 0.73, and the rim,
    // a torus of 0.55 and 0.07 in that plane.
    const Vec3 hub = {0.73f, 0, 0};
    const Part rim = {"rim",
                      [=](const Vec3& p) {
                          const auto [along, off] = axial(p, hub, {1, 0, 0});
                          return near(std::hypot(along, off - 0.55f), 0.07f);
                      },
                      [=](const Vec3& p) {
                          const auto [along, off] = axial(p, hub, {1, 0, 0});
                          const float s = 0.55f / off;
                          return Vec3{p[0] - 0.73f, p[1] - p[1] * s, p[2] - p[2] * s};
                      }};
    const auto triangles = check_parts(make_wheel(), "wheel",
                                       {cylinder("spindle", {0, 0, 0}, {1, 0, 0}, 0.1f, 0.54f, 0.8f),
                                        cylinder("stem", {0, 0, 0}, {1, 0, 0}, 0.2f, 0.42f, 0.54f),
                                        cylinder("spoke along +y", hub, {0, 1, 0}, 0.06f, 0.06f, 0.55f),
                                        cylinder("spoke along -y", hub, {0, -1, 0}, 0.06f, 0.06f, 0.55f),
                                        cylinder("spoke along +z", hub, {0, 0, 1}, 0.06f, 0.06f, 0.55f),
                                        cylinder("spoke along -z", hub, {0, 0, -1}, 0.06f, 0.06f, 0.55f), rim});
    check(triangles[6] == 2 * SEGMENTS * RIM_RINGS, "wheel: a rim of 64 × 24");
}

void test_connector()
{
    // LWO_GuageConnector, up along y: collars of radius 0.2 from 0.4 to 0.55 and from 0.8 into the
    // head, at 0.876, and a neck of 0.15 between them.
    check_parts(make_connector(), "connector",
                {cylinder("neck", {0, 0, 0}, {0, 1, 0}, 0.15f, 0.55f, 0.8f),
                 cylinder("lower collar", {0, 0, 0}, {0, 1, 0}, 0.2f, 0.4f, 0.55f),
                 cylinder("upper collar", {0, 0, 0}, {0, 1, 0}, 0.2f, 0.8f, 0.876f)});
    // The top of the upper collar is inside the head, a cylinder of 0.5 along z about y = 1.33333.
    check(std::hypot(0.2f, 0.876f - 1.33333f) < 0.5f, "connector: the upper collar ends inside the head");
}

void test_head()
{
    // LWO_GuageHead, about an axis along z through y = 1.33333: a cylinder of radius 0.5 from -1/3
    // to 1/3, closed at the back; at the front a ring to 0.45, and a bore of 0.45 back to 0.2, whose
    // inside faces its axis.
    const Vec3 middle = {0, 1.33333f, 0};
    const Vec3 z = {0, 0, 1};
    const float third = 1.0f / 3.0f;
    const auto off = [=](const Vec3& p) { return axial(p, middle, z); };
    const auto radial = [=](const Vec3& p) { return Vec3{p[0], p[1] - middle[1], 0.0f}; };
    const auto triangles = check_parts(
        make_head(), "head",
        {{"side", [=](const Vec3& p) { return near(off(p).second, 0.5f) && within(p[2], -third, third); }, radial},
         {"back", [=](const Vec3& p) { return near(p[2], -third) && off(p).second < 0.5f + 1e-5f; },
          [](const Vec3&) { return Vec3{0, 0, -1}; }},
         {"front ring", [=](const Vec3& p) { return near(p[2], third) && within(off(p).second, 0.45f, 0.5f); },
          [](const Vec3&) { return Vec3{0, 0, 1}; }},
         {"bore", [=](const Vec3& p) { return near(off(p).second, 0.45f) && within(p[2], 0.2f, third); },
          [=](const Vec3& p) {
              const Vec3 r = radial(p);
              return Vec3{-r[0], -r[1], 0.0f};
          }}});
    check(triangles[0] == 2 * SEGMENTS && triangles[1] == SEGMENTS && triangles[2] == 2 * SEGMENTS &&
              triangles[3] == 2 * SEGMENTS,
          "head: of 64 segments");
}

void test_face()
{
    // LWO_GuageFace: a disc of radius 0.45 at z = 0.2 about y = 1.33333, facing +z.
    const auto triangles = check_parts(
        make_face(), "face",
        {{"disc", [](const Vec3& p) { return near(p[2], 0.2f) && std::hypot(p[0], p[1] - 1.33333f) < 0.45f + 1e-5f; },
          [](const Vec3&) { return Vec3{0, 0, 1}; }}});
    check(triangles[0] == SEGMENTS, "face: of 64 segments");
}

void test_dial()
{
    // LWO_GuageDial: a hub of radius 0.05 along z from 0.2 to 0.3, and the needle, a triangle at
    // 0.275 facing +z, from (-0.035355, -0.0354) and (0.035355, -0.0354) on the hub to (0, -0.3733).
    const Mesh dial = make_dial();
    const auto triangles = check_parts(
        dial, "dial",
        {cylinder("hub", {0, 0, 0}, {0, 0, 1}, 0.05f, 0.2f, 0.3f),
         {"needle", [](const Vec3& p) { return near(p[2], 0.275f); }, [](const Vec3&) { return Vec3{0, 0, 1}; }}});
    check(triangles[1] == 1, "dial: the needle is one triangle");
    std::vector<Vec3> needle;
    for (const Vertex& v : dial.vertices) {
        if (near(v.position[2], 0.275f)) {
            needle.push_back({v.position[0], v.position[1], v.position[2]});
        }
    }
    bool tip = false;
    bool base = needle.size() == 3;
    for (const Vec3& p : needle) {
        tip = tip || (near(p[0], 0.0f) && std::fabs(p[1] + 0.3733f) < 1e-4f);
        base = base && ((std::fabs(std::fabs(p[0]) - 0.035355f) < 1e-4f && std::fabs(p[1] + 0.0354f) < 1e-4f) ||
                        near(p[0], 0.0f));
    }
    check(tip && base, "dial: the needle points down, from the hub to 0.3733");
}

void test_teapot()
{
    // unit_teapot takes (x, y, z) of teapot.c to (x / 2, (z - 1.5) / 2, -y / 2): the middle of the
    // bottom, (0, 0, 0), comes to y = -0.75, the knob, (0, 0, 3.15), to y = 0.825, and the body,
    // whose widest ring is of radius 2 at z = 0.9, is 1 wide about the y axis at y = -0.3.
    const Mesh teapot = make_teapot();
    check(teapot.vertices.size() == 32 * (TEAPOT_GRID + 1) * (TEAPOT_GRID + 1), "teapot: 32 patches");
    float low = 1e9f, high = -1e9f, wide = 0.0f, spout = -1e9f, handle = 1e9f;
    bool ring = false;
    for (const Vertex& v : teapot.vertices) {
        const Vec3 p = {v.position[0], v.position[1], v.position[2]};
        low = std::min(low, p[1]);
        high = std::max(high, p[1]);
        wide = std::max(wide, std::fabs(p[2]));
        spout = std::max(spout, p[0]);
        handle = std::min(handle, p[0]);
        ring = ring || (near(p[0], 1.0f) && near(p[1], -0.3f) && near(p[2], 0.0f));
    }
    check(near(low, -0.75f) && near(high, 0.825f), "teapot: from the bottom at -0.75 to the knob at 0.825");
    check(near(wide, 1.0f) && ring, "teapot: the body 1 wide, at y = -0.3");
    check(spout > 1.6f && spout < 1.7625f && handle < -1.4f && handle >= -1.5f,
          "teapot: the spout towards +x, the handle towards -x");

    // The normals: of unit length, every triangle counter-clockwise seen from where they point, and
    // on the body away from its axis.
    bool unit = true;
    bool ccw = true;
    bool body = true;
    double volume = 0.0;
    for (const Vertex& v : teapot.vertices) {
        const Vec3 n = {v.normal[0], v.normal[1], v.normal[2]};
        unit = unit && near(length(n), 1.0f);
        // The body is from z = 0.15 to 2.4 of teapot.c; away from the handle and the spout, which
        // are within 0.66 of the plane y = 0.
        if (v.position[1] > -0.67f && v.position[1] < 0.43f && std::fabs(v.position[2]) > 0.4f) {
            body = body && n[0] * v.position[0] + n[2] * v.position[2] > 0.0f;
        }
    }
    for (std::size_t t = 0; t + 2 < teapot.indices.size(); t += 3) {
        const Vec3 a = position(teapot, teapot.indices[t]);
        const Vec3 b = position(teapot, teapot.indices[t + 1]);
        const Vec3 c = position(teapot, teapot.indices[t + 2]);
        const Vec3 u = sub(b, a);
        const Vec3 w = sub(c, a);
        const Vec3 face = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
        for (int k = 0; k < 3; k++) {
            const float* n = teapot.vertices[teapot.indices[t + k]].normal;
            ccw = ccw && length(face) > 0.0f && dot(face, {n[0], n[1], n[2]}) > 0.0f;
        }
        volume += dot(a, {b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0]}) / 6.0;
    }
    check(unit, "teapot: normals of unit length");
    check(ccw, "teapot: every triangle counter-clockwise seen from where its normals point");
    check(body, "teapot: the normals of the body point away from its axis");
    check(volume > 0.0, "teapot: the triangles face out, the volume they enclose is positive");

    // Closed where the patches meet: with the vertices at the same place taken as one, every edge
    // is between two triangles, but those of the openings of the Newell teapot: the edge of the lid
    // and the inside of the rim, 4 patches each, and the two ends of the spout and of the handle,
    // 2 patches each. And where the patches meet they have the same normal.
    std::map<std::tuple<long, long, long>, uint32_t> welded;
    std::vector<uint32_t> id(teapot.vertices.size());
    for (std::size_t i = 0; i < teapot.vertices.size(); i++) {
        const Vertex& v = teapot.vertices[i];
        const auto key = std::make_tuple(std::lround(v.position[0] * 1e5), std::lround(v.position[1] * 1e5),
                                         std::lround(v.position[2] * 1e5));
        id[i] = welded.emplace(key, static_cast<uint32_t>(i)).first->second;
    }
    std::map<std::pair<uint32_t, uint32_t>, int> edges;
    for (std::size_t t = 0; t + 2 < teapot.indices.size(); t += 3) {
        for (int k = 0; k < 3; k++) {
            const uint32_t a = id[teapot.indices[t + k]];
            const uint32_t b = id[teapot.indices[t + (k + 1) % 3]];
            edges[{std::min(a, b), std::max(a, b)}]++;
        }
    }
    int open = 0;
    bool twice = true;
    bool openings = true;
    std::vector<bool> on_opening(teapot.vertices.size());
    for (const auto& [edge, n] : edges) {
        twice = twice && n <= 2 && edge.first != edge.second;
        if (n == 1) {
            open++;
            for (uint32_t i : {edge.first, edge.second}) {
                on_opening[i] = true;
                // Back in the frame of teapot.c.
                const float* p = teapot.vertices[i].position;
                const float x = 2 * p[0];
                const float z = 2 * p[1] + 1.5f;
                const bool lid_or_rim_or_tip = near(z, 2.4f);
                const bool spout_base = near(x, 1.7f);
                const bool handle_end = x >= -2.0f && x <= -1.5f && (within(z, 0.6f, 0.9f) || within(z, 2.025f, 2.25f));
                openings = openings && (lid_or_rim_or_tip || spout_base || handle_end);
            }
        }
    }
    // The ends of the handle touch the body, but do not meet it along an edge.
    bool smooth = true;
    for (std::size_t i = 0; i < teapot.vertices.size(); i++) {
        if (!on_opening[id[i]]) {
            const float* a = teapot.vertices[i].normal;
            const float* b = teapot.vertices[id[i]].normal;
            smooth = smooth && dot({a[0], a[1], a[2]}, {b[0], b[1], b[2]}) > 0.99f;
        }
    }
    check(twice, "teapot: no edge of more than two triangles");
    check(open == 16 * TEAPOT_GRID && openings, "teapot: closed where the patches meet, open where the Newell teapot is");
    check(smooth, "teapot: where the patches meet, the same normal");
}

} // namespace

int main()
{
    test_tube();
    test_elbow();
    test_ball();
    test_flanges();
    test_bolts();
    test_body();
    test_end_plates();
    test_wheel();
    test_connector();
    test_head();
    test_face();
    test_dial();
    test_teapot();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
