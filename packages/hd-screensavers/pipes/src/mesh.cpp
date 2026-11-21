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

#include "teapot.hpp"

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
 * @brief Adds the smooth side of a cylinder, open at both ends.
 *
 * About the axis through @p origin along @p w, with @p u and @p v across it,
 * from @p from to @p to along w. The first vertex of each ring is at the
 * angle @p phase from u towards v. The normals point away from the axis, or
 * to it with @p sign -1, as on the inside of a bore.
 */
void add_side(Mesh& mesh, const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& w, int sides, float radius,
              float phase, float from, float to, float sign)
{
    const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
    for (float h : {from, to}) {
        for (int j = 0; j <= sides; j++) {
            const float a = phase + 2.0f * PI * j / sides;
            const Vec3 p = in_frame(origin, u, v, w, radius * std::cos(a), radius * std::sin(a), h);
            const Vec3 n = in_frame({0, 0, 0}, u, v, w, sign * std::cos(a), sign * std::sin(a), 0.0f);
            add_vertex(mesh, p[0], p[1], p[2], n[0], n[1], n[2]);
        }
    }
    connect_rings(mesh, first, 2, sides + 1);
}

/**
 * @brief Adds a flat ring across an axis, or a disc when @p inner is 0.
 *
 * About the axis through @p origin along @p w, with @p u and @p v across it,
 * at @p h along w, from @p inner to @p outer from the axis. It faces along
 * w, or against it with @p sign -1.
 */
void add_ring(Mesh& mesh, const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& w, int sides, float inner,
              float outer, float h, float sign)
{
    const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
    for (float radius : {inner, outer}) {
        for (int j = 0; j <= sides; j++) {
            const float a = 2.0f * PI * j / sides;
            const Vec3 p = in_frame(origin, u, v, w, radius * std::cos(a), radius * std::sin(a), h);
            add_vertex(mesh, p[0], p[1], p[2], sign * w[0], sign * w[1], sign * w[2]);
        }
    }
    connect_rings(mesh, first, 2, sides + 1);
}

/**
 * @brief Adds a whole torus.
 *
 * About the axis through @p origin along @p w, with @p u and @p v across it:
 * a ring of radius @p minor whose middle goes round the axis at @p major,
 * in the plane of u and v; @p segments around the axis, @p rings around the
 * ring.
 */
void add_torus(Mesh& mesh, const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& w, float major, float minor,
               int segments, int rings)
{
    const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
    for (int i = 0; i <= segments; i++) {
        const float a = 2.0f * PI * i / segments;
        for (int j = 0; j <= rings; j++) {
            const float b = 2.0f * PI * j / rings;
            const float out = std::cos(b);
            const Vec3 n = in_frame({0, 0, 0}, u, v, w, out * std::cos(a), out * std::sin(a), std::sin(b));
            const Vec3 p = in_frame(origin, u, v, w, (major + minor * out) * std::cos(a),
                                    (major + minor * out) * std::sin(a), minor * std::sin(b));
            add_vertex(mesh, p[0], p[1], p[2], n[0], n[1], n[2]);
        }
    }
    connect_rings(mesh, first, segments + 1, rings + 1);
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
        add_side(mesh, origin, u, v, w, sides, radius, phase, from, to, 1.0f);
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

/**
 * @brief A point of a patch of the teapot, and the derivatives there along u and v.
 *
 * In double, so that where a row of control points is one point, the
 * points of that row come out as the same float.
 */
struct PatchPoint {
    std::array<double, 3> position;
    std::array<double, 3> along_u;
    std::array<double, 3> along_v;
};

/** @brief The cubic Bernstein polynomials at @p t, and their derivatives. */
void bernstein(double t, double (&b)[4], double (&d)[4])
{
    const double s = 1.0 - t;
    b[0] = s * s * s;
    b[1] = 3.0 * t * s * s;
    b[2] = 3.0 * t * t * s;
    b[3] = t * t * t;
    d[0] = -3.0 * s * s;
    d[1] = 3.0 * s * s - 6.0 * t * s;
    d[2] = 6.0 * t * s - 3.0 * t * t;
    d[3] = 3.0 * t * t;
}

/** @brief A patch at (@p u, @p v), as glEvalMesh2 of unit_teapot: u along a row of control points, v from row to row. */
PatchPoint evaluate(const Patch& patch, double u, double v)
{
    double bu[4], du[4], bv[4], dv[4];
    bernstein(u, bu, du);
    bernstein(v, bv, dv);
    PatchPoint point{};
    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            for (int c = 0; c < 3; c++) {
                const double x = patch[j * 4 + k][c];
                point.position[c] += bv[j] * bu[k] * x;
                point.along_u[c] += bv[j] * du[k] * x;
                point.along_v[c] += dv[j] * bu[k] * x;
            }
        }
    }
    return point;
}

/** @brief The cross product of the derivatives, the normal of glEnable(GL_AUTO_NORMAL), not of unit length. */
std::array<double, 3> auto_normal(const PatchPoint& p)
{
    const auto& a = p.along_u;
    const auto& b = p.along_v;
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double length(const std::array<double, 3>& a)
{
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
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

Mesh make_body()
{
    Mesh mesh;
    add_prism(mesh, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, SEGMENTS, BODY_RADIUS, 0.0f, -BODY_HALF_LENGTH,
              BODY_HALF_LENGTH, true);
    return mesh;
}

Mesh make_end_plates()
{
    // The corners are 22.5° off the axes, so that the flats face them.
    const float corner = END_PLATE_APOTHEM / std::cos(PI / 8);
    Mesh mesh;
    add_prism(mesh, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 8, corner, PI / 8, BODY_HALF_LENGTH, END_PLATE_FAR,
              false);
    add_prism(mesh, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 8, corner, PI / 8, -END_PLATE_FAR, -BODY_HALF_LENGTH,
              false);
    return mesh;
}

Mesh make_wheel()
{
    const Vec3 x = {1, 0, 0};
    const Vec3 y = {0, 1, 0};
    const Vec3 z = {0, 0, 1};
    Mesh mesh;
    add_prism(mesh, {0, 0, 0}, y, z, x, SEGMENTS, STEM_RADIUS, 0.0f, STEM_FROM, STEM_TO, true);
    add_prism(mesh, {0, 0, 0}, y, z, x, SEGMENTS, SPINDLE_RADIUS, 0.0f, STEM_TO, SPINDLE_TO, true);
    for (const Vec3& spoke : {y, z, Vec3{0, -1, 0}, Vec3{0, 0, -1}}) {
        add_prism(mesh, {WHEEL_X, 0, 0}, x, cross(spoke, x), spoke, SEGMENTS, SPOKE_RADIUS, 0.0f, SPOKE_FROM,
                  RIM_RADIUS, true);
    }
    add_torus(mesh, {WHEEL_X, 0, 0}, y, z, x, RIM_RADIUS, RIM_THICKNESS, SEGMENTS, RIM_RINGS);
    return mesh;
}

Mesh make_connector()
{
    const Vec3 x = {1, 0, 0};
    const Vec3 y = {0, 1, 0};
    const Vec3 z = {0, 0, 1};
    Mesh mesh;
    add_prism(mesh, {0, 0, 0}, z, x, y, SEGMENTS, COLLAR_RADIUS, 0.0f, CONNECTOR_FROM, NECK_FROM, true);
    add_prism(mesh, {0, 0, 0}, z, x, y, SEGMENTS, NECK_RADIUS, 0.0f, NECK_FROM, NECK_TO, true);
    add_prism(mesh, {0, 0, 0}, z, x, y, SEGMENTS, COLLAR_RADIUS, 0.0f, NECK_TO, CONNECTOR_TO, true);
    return mesh;
}

Mesh make_head()
{
    const Vec3 middle = {0, GAUGE_HEIGHT, 0};
    const Vec3 x = {1, 0, 0};
    const Vec3 y = {0, 1, 0};
    const Vec3 z = {0, 0, 1};
    Mesh mesh;
    add_side(mesh, middle, x, y, z, SEGMENTS, HEAD_RADIUS, 0.0f, -HEAD_HALF_LENGTH, HEAD_HALF_LENGTH, 1.0f);
    add_ring(mesh, middle, x, y, z, SEGMENTS, 0.0f, HEAD_RADIUS, -HEAD_HALF_LENGTH, -1.0f);
    add_ring(mesh, middle, x, y, z, SEGMENTS, FACE_RADIUS, HEAD_RADIUS, HEAD_HALF_LENGTH, 1.0f);
    add_side(mesh, middle, x, y, z, SEGMENTS, FACE_RADIUS, 0.0f, FACE_Z, HEAD_HALF_LENGTH, -1.0f);
    return mesh;
}

Mesh make_face()
{
    Mesh mesh;
    add_ring(mesh, {0, GAUGE_HEIGHT, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, SEGMENTS, 0.0f, FACE_RADIUS, FACE_Z, 1.0f);
    return mesh;
}

Mesh make_dial()
{
    Mesh mesh;
    add_prism(mesh, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, SEGMENTS, HUB_RADIUS, 0.0f, FACE_Z, HUB_TO, true);
    // The needle starts on the hub, 45° to either side of straight down.
    const float base = HUB_RADIUS * std::sqrt(0.5f);
    const uint32_t a = add_vertex(mesh, -base, -base, NEEDLE_Z, 0, 0, 1);
    const uint32_t b = add_vertex(mesh, base, -base, NEEDLE_Z, 0, 0, 1);
    const uint32_t c = add_vertex(mesh, 0, -NEEDLE_LENGTH, NEEDLE_Z, 0, 0, 1);
    add_triangle(mesh, a, b, c);
    return mesh;
}

Mesh make_teapot()
{
    Mesh mesh;
    for (const Patch& patch : teapot_patches()) {
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (int i = 0; i <= TEAPOT_GRID; i++) {
            for (int j = 0; j <= TEAPOT_GRID; j++) {
                const double u = static_cast<double>(j) / TEAPOT_GRID;
                const double v = static_cast<double>(i) / TEAPOT_GRID;
                const PatchPoint point = evaluate(patch, u, v);
                auto n = auto_normal(point);
                if (length(n) < 1e-9) {
                    // Where a row of control points is one point, as at the knob of the lid and the
                    // middle of the bottom, the normal is that of a point a little way into the patch.
                    n = auto_normal(evaluate(patch, u + (0.5 - u) * 1e-4, v + (0.5 - v) * 1e-4));
                }
                const double l = length(n);
                // Where unit_teapot puts it: (x, y, z) of teapot.c becomes (x / 2, (z - 1.5) / 2, -y / 2).
                const auto& p = point.position;
                add_vertex(mesh, static_cast<float>(0.5 * p[0]), static_cast<float>(0.5 * (p[2] - 1.5)),
                           static_cast<float>(-0.5 * p[1]), static_cast<float>(n[0] / l), static_cast<float>(n[2] / l),
                           static_cast<float>(-n[1] / l));
            }
        }
        connect_rings(mesh, first, TEAPOT_GRID + 1, TEAPOT_GRID + 1);
    }
    return mesh;
}
