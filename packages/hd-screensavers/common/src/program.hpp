/**
 * @file program.hpp
 * @brief The name of the screensaver that the shared code is part of.
 *
 * The code in common/ is compiled into every screensaver. Each defines
 * PROGRAM in its main.cpp; the shared code puts it before its messages on
 * stderr and uses it as the namespace of the layer surfaces, which window
 * rules of the compositor can match, and as the title and app id of the
 * window of --window.
 */
#pragma once

/** @brief The name of the screensaver, as "hd-pipes". */
extern const char* const PROGRAM;
