/**
 * @file matrix.hpp
 * @brief The 4×4 matrices of the scenes, as OpenGL 1.x built them.
 *
 * Column major, as GLSL takes them. Each function makes the matrix of the
 * OpenGL call of the same name; a scene multiplies them in the order the
 * calls of the reference come in, glTranslatef(...) then glRotatef(...)
 * being translate(...) * rotate(...).
 *
 * The projection is for Vulkan: depth from 0 at the near plane to 1 at the
 * far one, and y flipped, so that the picture is the one of OpenGL with y
 * up and the triangles that OpenGL sees counter-clockwise are the front
 * faces of VK_FRONT_FACE_COUNTER_CLOCKWISE.
 */
#pragma once

#include <array>
#include <cmath>
#include <numbers>

/** @brief A 4×4 matrix of floats, column major. */
struct Mat4 {
    /** @brief The element in column c and row r is m[c * 4 + r]. */
    std::array<float, 16> m{};

    /** @brief The identity. */
    static Mat4 identity()
    {
        Mat4 a;
        a.m[0] = a.m[5] = a.m[10] = a.m[15] = 1.0f;
        return a;
    }

    /** @brief The product, this matrix applied after @p b. */
    Mat4 operator*(const Mat4& b) const
    {
        Mat4 a;
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += m[k * 4 + r] * b.m[c * 4 + k];
                }
                a.m[c * 4 + r] = sum;
            }
        }
        return a;
    }
};

/** @brief glTranslatef. */
inline Mat4 translate(float x, float y, float z)
{
    Mat4 a = Mat4::identity();
    a.m[12] = x;
    a.m[13] = y;
    a.m[14] = z;
    return a;
}

/** @brief glScalef. */
inline Mat4 scale(float x, float y, float z)
{
    Mat4 a = Mat4::identity();
    a.m[0] = x;
    a.m[5] = y;
    a.m[10] = z;
    return a;
}

/** @brief glRotatef: @p degrees counter-clockwise about the axis (x, y, z), which need not be a unit vector. */
inline Mat4 rotate(float degrees, float x, float y, float z)
{
    const float length = std::sqrt(x * x + y * y + z * z);
    x /= length;
    y /= length;
    z /= length;
    const float a = degrees * std::numbers::pi_v<float> / 180.0f;
    const float c = std::cos(a);
    const float s = std::sin(a);
    const float t = 1.0f - c;
    Mat4 r = Mat4::identity();
    r.m[0] = x * x * t + c;
    r.m[1] = y * x * t + z * s;
    r.m[2] = x * z * t - y * s;
    r.m[4] = x * y * t - z * s;
    r.m[5] = y * y * t + c;
    r.m[6] = y * z * t + x * s;
    r.m[8] = x * z * t + y * s;
    r.m[9] = y * z * t - x * s;
    r.m[10] = z * z * t + c;
    return r;
}

/**
 * @brief gluPerspective, for Vulkan.
 * @param fovy   The field of view from bottom to top, in degrees.
 * @param aspect The width divided by the height.
 * @param near   The distance of the near plane.
 * @param far    The distance of the far plane.
 */
inline Mat4 perspective(float fovy, float aspect, float near, float far)
{
    const float f = 1.0f / std::tan(fovy * std::numbers::pi_v<float> / 360.0f);
    Mat4 p;
    p.m[0] = f / aspect;
    p.m[5] = -f;
    p.m[10] = far / (near - far);
    p.m[11] = -1.0f;
    p.m[14] = near * far / (near - far);
    return p;
}
