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

/**
 * @file mesh.hpp
 * @brief The shapes of the pipes, after MakeTube, myElbow and mySphere of pipes.c,
 *        and the bolts of the elbows, after LWO_ElbowCoins and LWO_ElbowBolts of
 *        pipeobjs.c.
 *
 * The units are those of pipes.c: the cells of the grid are 4/3 apart, a
 * pipe has a radius of 1/3. A tube is the straight piece in the middle of a
 * cell and also the link between two cells: 2/3 long, with the 1/3 at each
 * end between them, so that the pieces of a pipe meet. The elbow is a
 * quarter of a torus whose ring has the radius of the pipe, about a point
 * 1/3 back and 1/3 aside of the middle of the cell. The ball is a sphere of
 * radius 1, scaled to that of a start, an end or a joint.
 *
 * A bolted elbow, the type 2 of pipes.c, is an elbow with a flange on each
 * of the two links next to it, a coin of pipeobjs.c, which bolts it onto the
 * pipe: a disc across the pipe, with six bolts through it, which show as a
 * hexagonal head on each of its faces. The sizes and places are measured
 * from pipeobjs.c and the transformations of myElbow; the shapes are made
 * here, smoother: the rim of a flange with 64 segments and smooth normals,
 * its faces flat, and the heads as hexagonal prisms with flat
 * normals.
 *
 * pipes.c draws them with 24 or 25 facets; these are smoother and made once,
 * to be drawn instanced: the tube with 64 segments, the elbow with 64 × 24,
 * the ball with 64 × 32. The normals are smooth, those of the true shape.
 * The rings at the ends of the tube and of the elbow have their vertices at
 * the same angles, so where they meet, at any of the turns of 90° of the
 * grid, there is no seam.
 *
 * Every triangle is counter-clockwise seen from the side its normals point
 * to, the outside, as glFrontFace(GL_CCW) of pipes.c wants it.
 */
#pragma once

#include <cstdint>
#include <vector>

/** @brief The distance between two cells of the grid. */
constexpr float CELL_SIZE = 4.0f / 3.0f;
/** @brief The radius of a pipe, also the half length of a tube. */
constexpr float PIPE_RADIUS = 1.0f / 3.0f;
/** @brief The radius of the balls at the start and the end of a pipe. */
constexpr float END_RADIUS = 0.6f;
/** @brief The radius of the balls where a pipe of balls turns, elbowradius of pipes.c. */
constexpr float JOINT_RADIUS = 0.5f;

/** @brief How far a flange is from the middle of the cell, along the link it is on: its two faces. */
constexpr float FLANGE_NEAR = 0.52f;
constexpr float FLANGE_FAR = 0.6f;
/** @brief The radius of a flange. */
constexpr float FLANGE_RADIUS = 0.5f;
/** @brief The bolts through a flange, 60° apart, the first in the line of y, across the bend. */
constexpr int BOLTS = 6;
/** @brief How far the bolts are from the axis of the pipe. */
constexpr float BOLT_CIRCLE = 0.425f;
/** @brief From the middle of a head or a nut to its corners; a flat faces the axis of the pipe. */
constexpr float BOLT_RADIUS = 0.07f;
/** @brief The height of a head or a nut above the face of the flange. */
constexpr float BOLT_HEIGHT = 0.06f;

/** @brief The segments around a tube, an elbow, a ball and a flange. */
constexpr int SEGMENTS = 64;
/** @brief The rings along the bend of an elbow. */
constexpr int ELBOW_RINGS = 24;
/** @brief The rings of a ball, from pole to pole. */
constexpr int BALL_RINGS = 32;

/** @brief A vertex: where it is and the normal of the shape there. */
struct Vertex {
    float position[3];
    float normal[3];
};

/** @brief A shape: its vertices and the triangles, three indices each. */
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @brief The tube, MakeTube of pipes.c.
 *
 * Along z, from -1/3 to 1/3, with a radius of 1/3 and open ends.
 */
Mesh make_tube();

/**
 * @brief The elbow, myElbow of pipes.c.
 *
 * In a cell with its middle at the origin: the pipe comes in along +z, at
 * the face z = -1/3 of the middle, and goes out along +x, at the face
 * x = 1/3. Open ends.
 */
Mesh make_elbow();

/** @brief The ball, a sphere of radius 1 about the origin. */
Mesh make_ball();

/**
 * @brief The flanges of a bolted elbow, LWO_ElbowCoins of pipeobjs.c.
 *
 * In the place of make_elbow(): one about the z axis, from z = -FLANGE_FAR
 * to -FLANGE_NEAR, on the link the pipe comes in from, and one about the x
 * axis, from x = FLANGE_NEAR to FLANGE_FAR, on the link it goes out on.
 * Closed: a rim and two faces. Drawn in the colour of the system.
 */
Mesh make_flanges();

/**
 * @brief The bolts of a bolted elbow, LWO_ElbowBolts of pipeobjs.c.
 *
 * In the place of make_flanges(): BOLTS through each flange, at BOLT_CIRCLE
 * from the axis of the pipe; each is a head on either face of the flange,
 * a closed hexagonal prism of BOLT_HEIGHT. Drawn in grey, MaterialGray of pipes.c.
 */
Mesh make_bolts();
