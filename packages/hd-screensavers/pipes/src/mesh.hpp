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
 *        the bolts of the elbows and the equipment of the factory, after the
 *        objects of pipeobjs.c, and the teapot of teapot.c.
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
 * The equipment of the factory takes the place of a straight piece, in a
 * cell with its middle at the origin and the pipe along z, as pipeobjs.c has
 * it. A valve and a gauge sit on a body, a wider piece of pipe with an
 * octagonal plate at each end, the bolts of pipeobjs.c. The valve has a
 * wheel on a stem out along +x; the gauge stands on a connector up along +y
 * and has a round head with a white face and a needle towards +z. The
 * teapot is the Newell teapot, upright, as unit_teapot of teapot.c places
 * it. The sizes and places are measured from pipeobjs.c; the shapes are
 * made here, smoother, as the flanges.
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

/** @brief The body of a valve and a gauge, LWO_PipeBetweenBolts: its radius, and its half length along z. */
constexpr float BODY_RADIUS = 0.5f;
constexpr float BODY_HALF_LENGTH = 0.5f;
/** @brief The plates at the ends of the body, LWO_Bolts3D: octagons with a flat on each axis, from the body to END_PLATE_FAR. */
constexpr float END_PLATE_APOTHEM = 0.4f;
constexpr float END_PLATE_FAR = 0.6f;

/** @brief The wheel of a valve, LWO_BigValve, about the x axis: the stem from the body out to the spindle. */
constexpr float STEM_RADIUS = 0.2f;
constexpr float STEM_FROM = 0.42f;
constexpr float STEM_TO = 0.54f;
/** @brief The spindle, from the stem to the end of the valve. */
constexpr float SPINDLE_RADIUS = 0.1f;
constexpr float SPINDLE_TO = 0.8f;
/** @brief The plane of the rim and the spokes. */
constexpr float WHEEL_X = 0.73f;
/** @brief The rim: a torus about the x axis, the radius to the middle of its ring, and that of the ring. */
constexpr float RIM_RADIUS = 0.55f;
constexpr float RIM_THICKNESS = 0.07f;
/** @brief The four spokes, along ±y and ±z, from inside the spindle to the middle of the rim. */
constexpr float SPOKE_RADIUS = 0.06f;
constexpr float SPOKE_FROM = 0.06f;

/** @brief The connector of a gauge, LWO_GuageConnector, up along y: a collar on the body, a neck and a collar under the head. */
constexpr float COLLAR_RADIUS = 0.2f;
constexpr float NECK_RADIUS = 0.15f;
constexpr float CONNECTOR_FROM = 0.4f;
constexpr float NECK_FROM = 0.55f;
constexpr float NECK_TO = 0.8f;
/** @brief The top of the upper collar, in the head. */
constexpr float CONNECTOR_TO = 0.876f;
/** @brief The middle of the head of a gauge above the middle of the cell; pipes.c: "the object really is centered on 1.3333300000". */
constexpr float GAUGE_HEIGHT = 1.33333f;
/** @brief The head, LWO_GuageHead: a short cylinder along z, from -1/3 to 1/3, with a bore at the front for the face. */
constexpr float HEAD_RADIUS = 0.5f;
constexpr float HEAD_HALF_LENGTH = 1.0f / 3.0f;
/** @brief The face, LWO_GuageFace: a disc at the bottom of the bore. */
constexpr float FACE_RADIUS = 0.45f;
constexpr float FACE_Z = 0.2f;
/** @brief The dial, LWO_GuageDial: a hub in the middle of the face, and the needle, which points down. */
constexpr float HUB_RADIUS = 0.05f;
constexpr float HUB_TO = 0.3f;
constexpr float NEEDLE_Z = 0.275f;
constexpr float NEEDLE_LENGTH = 0.3733f;

/** @brief The segments around a tube, an elbow, a ball, a flange and the round parts of the equipment. */
constexpr int SEGMENTS = 64;
/** @brief The rings along the bend of an elbow. */
constexpr int ELBOW_RINGS = 24;
/** @brief The rings of a ball, from pole to pole. */
constexpr int BALL_RINGS = 32;
/** @brief The segments around the ring of the rim of a wheel. */
constexpr int RIM_RINGS = 24;
/** @brief The rows and the columns of triangles of each patch of the teapot; unit_teapot of pipes.c has 12. */
constexpr int TEAPOT_GRID = 32;

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

/**
 * @brief The body of a valve or a gauge, LWO_PipeBetweenBolts of pipeobjs.c.
 *
 * About the z axis, from -BODY_HALF_LENGTH to BODY_HALF_LENGTH, with a
 * radius of BODY_RADIUS; closed. Drawn in the colour of the system.
 */
Mesh make_body();

/**
 * @brief The plates at the ends of the body, LWO_Bolts3D of pipeobjs.c.
 *
 * Two closed octagonal prisms about the z axis, from BODY_HALF_LENGTH to
 * END_PLATE_FAR and from -END_PLATE_FAR to -BODY_HALF_LENGTH, with a flat
 * END_PLATE_APOTHEM from the axis towards ±x and ±y. Drawn in grey.
 */
Mesh make_end_plates();

/**
 * @brief The wheel of a valve, LWO_BigValve of pipeobjs.c.
 *
 * About the x axis, on a body along z: the stem and the spindle, closed
 * cylinders, the four spokes in the plane x = WHEEL_X, closed cylinders
 * along ±y and ±z, and the rim, a torus in that plane. Drawn in the colour
 * of the wheel.
 */
Mesh make_wheel();

/**
 * @brief The connector of a gauge, LWO_GuageConnector of pipeobjs.c.
 *
 * Three closed cylinders about the y axis: the lower collar from
 * CONNECTOR_FROM to NECK_FROM, the neck to NECK_TO, the upper collar to
 * CONNECTOR_TO. Drawn in grey.
 */
Mesh make_connector();

/**
 * @brief The head of a gauge, LWO_GuageHead of pipeobjs.c.
 *
 * About an axis along z through (0, GAUGE_HEIGHT, 0): a cylinder of
 * HEAD_RADIUS from -HEAD_HALF_LENGTH to HEAD_HALF_LENGTH, closed at the
 * back, and at the front a ring down to FACE_RADIUS and a bore of that
 * radius back to FACE_Z, whose normals point to its axis. Drawn in the
 * colour of the system.
 */
Mesh make_head();

/**
 * @brief The face of a gauge, LWO_GuageFace of pipeobjs.c.
 *
 * A disc of FACE_RADIUS about (0, GAUGE_HEIGHT, FACE_Z), facing +z, at the
 * bottom of the bore of the head. Drawn in white, MaterialWhite of pipes.c.
 */
Mesh make_face();

/**
 * @brief The dial of a gauge, LWO_GuageDial of pipeobjs.c.
 *
 * About the origin, to be moved to the middle of the face: the hub, a
 * cylinder about the z axis from FACE_Z to HUB_TO, closed, and the needle, a
 * flat triangle at NEEDLE_Z facing +z, from the hub down to NEEDLE_LENGTH
 * below the axis. Drawn in grey.
 */
Mesh make_dial();

/**
 * @brief The teapot, unit_teapot of teapot.c.
 *
 * The patches of teapot.hpp, each tessellated into TEAPOT_GRID × TEAPOT_GRID
 * squares of two triangles, with the normals of the patches, put where
 * unit_teapot puts them: glRotatef(270, 1, 0, 0), glScalef(0.5, 0.5, 0.5)
 * and glTranslatef(0, 0, -1.5) take (x, y, z) of teapot.c to
 * (x / 2, (z - 1.5) / 2, -y / 2). Upright along +y, the spout towards +x;
 * open where the Newell teapot is, between the lid and the rim and at the
 * ends of the spout and the handle. Drawn in the colour of the system.
 */
Mesh make_teapot();
