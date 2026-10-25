/**
 * @file program.hpp
 * @brief The name of the program that the shared code is part of.
 *
 * The code in common/ is compiled into hd-menu and hd-message. Each defines
 * PROGRAM in its main.cpp; the shared code puts it before its messages on
 * stderr and uses it as the namespace of the layer surface, which window
 * rules of the compositor can match.
 */
#pragma once

/** @brief The name of the program: "hd-menu" or "hd-message". */
extern const char* const PROGRAM;
