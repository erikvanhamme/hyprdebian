/* Copyright (c) Mark J. Kilgard, 1994. */

/**
(c) Copyright 1993, Silicon Graphics, Inc.

ALL RIGHTS RESERVED

Permission to use, copy, modify, and distribute this software
for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that
both the copyright notice and this permission notice appear in
supporting documentation, and that the name of Silicon
Graphics, Inc. not be used in advertising or publicity
pertaining to distribution of the software without specific,
written prior permission.

THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU
"AS-IS" AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR
OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  IN NO
EVENT SHALL SILICON GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE
ELSE FOR ANY DIRECT, SPECIAL, INCIDENTAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER,
INCLUDING WITHOUT LIMITATION, LOSS OF PROFIT, LOSS OF USE,
SAVINGS OR REVENUE, OR THE CLAIMS OF THIRD PARTIES, WHETHER OR
NOT SILICON GRAPHICS, INC.  HAS BEEN ADVISED OF THE POSSIBILITY
OF SUCH LOSS, HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
ARISING OUT OF OR IN CONNECTION WITH THE POSSESSION, USE OR
PERFORMANCE OF THIS SOFTWARE.

US Government Users Restricted Rights

Use, duplication, or disclosure by the Government is subject to
restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
(c)(1)(ii) of the Rights in Technical Data and Computer
Software clause at DFARS 252.227-7013 and/or in similar or
successor clauses in the FAR or the DOD or NASA FAR
Supplement.  Unpublished-- rights reserved under the copyright
laws of the United States.  Contractor/manufacturer is Silicon
Graphics, Inc., 2011 N.  Shoreline Blvd., Mountain View, CA
94039-7311.

OpenGL(TM) is a trademark of Silicon Graphics, Inc.
*/

// The Newell teapot of hacks/glx/teapot.c of xscreensaver, which carries the
// notice above, for the teapot of the factory of hd-pipes.

/**
 * @file teapot.hpp
 * @brief The Bézier patches of the teapot, as unit_teapot of teapot.c makes them.
 *
 * teapot.c holds a quarter of the rim, the body, the lid and the bottom, and
 * half of the handle and the spout; unit_teapot reflects them into the whole
 * teapot. make_teapot() of mesh.hpp tessellates the patches.
 */
#pragma once

#include <array>
#include <vector>

/** @brief A bicubic Bézier patch: its 4×4 control points, row by row, each x, y and z. */
using Patch = std::array<std::array<float, 3>, 16>;

/**
 * @brief The 32 patches of the whole teapot.
 *
 * In the frame of teapot.c: z up, the bottom at z = 0 and the knob of the
 * lid at 3.15, the spout along +x and the handle along -x. The corners of
 * each patch are on the surface. Along a row of control points is u, from
 * row to row v, as glMap2f of unit_teapot takes them; the cross product of
 * the derivatives along u and along v points out of the teapot.
 */
std::vector<Patch> teapot_patches();
