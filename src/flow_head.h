/* flow — a single-header C library for interactive node-graph editors in the
 * terminal (a C/terminal analog of xyflow / React Flow).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 dovas-net
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ----------------------------------------------------------------------------
 * Single-header library. In exactly ONE translation unit, #define FLOW_IMPLEMENTATION
 * before #include "flow.h"; include it WITHOUT the macro everywhere else.
 * flow.h is GENERATED from src/ by tools/amalgamate.sh — DO NOT edit flow.h directly; edit src/.
 *
 * Requirements: C99 or later; link with -lm. The interactive run-loop
 * (flow_run / flow_feed / flow_present / flow_term_*) needs a POSIX terminal
 * (Linux / macOS); the model, geometry, rendering, routing, layout, and JSON
 * layers are pure C and embeddable with the host's own I/O. Not thread-safe:
 * serialize calls per flow_t; separate instances are independent.
 *
 * Out-of-memory: flow_new returns NULL and flow_add_node / flow_add_edge return
 * -1 on allocation failure, leaving the graph unmodified (their undo recording is
 * dropped, never fatal). Other paths — render/layout scratch and the
 * selection/clipboard/remove snapshot buffers — currently assume allocation
 * succeeds; #define a FLOW_MALLOC that aborts on failure to make every path
 * fail-fast instead.
 *
 * Credits: terminal raw-mode and escape-sequence approach inspired by tuibox
 * (Cubified, https://github.com/Cubified/tuibox), implemented independently from
 * the standard termios idiom and ANSI/SGR escape sequences. Concepts and API
 * shape are modeled on xyflow / React Flow (MIT, webkid GmbH) — an independent
 * reimplementation; no xyflow code is included.
 */
#define FLOW_VERSION_MAJOR 0
#define FLOW_VERSION_MINOR 3
#define FLOW_VERSION_PATCH 1
#define FLOW_VERSION (FLOW_VERSION_MAJOR * 10000 + FLOW_VERSION_MINOR * 100 + FLOW_VERSION_PATCH)
#define FLOW_VERSION_STRING "0.3.1"
/* Under strict -std=c11, glibc hides POSIX/BSD symbols the implementation needs
   (struct sigaction / sigaction / sigfillset, ioctl / struct winsize, poll) behind a
   feature-test macro. Request them BEFORE the first system header is pulled (the include
   below locks glibc's feature set). Linux/glibc only — macOS and the BSDs expose these by
   default, and defining a feature macro there can instead HIDE BSD symbols. Harmless in
   declaration-only TUs (they include no POSIX headers). __linux__ is a compiler builtin,
   so it is reliable before any include; __GLIBC__ would not be. */
#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
/* Allocator + assert hooks (stb convention). To route flow's heap through your own
   arena/tracker, #define ALL FOUR of FLOW_MALLOC/CALLOC/REALLOC/FREE before including
   flow.h (override the set together — they must pair). Buffers flow returns to you
   (e.g. flow_render_diff / flow_save strings) are then yours to release with the SAME
   allocator. FLOW_ASSERT defaults to assert(); #define it (even to a no-op) to override. */
#ifndef FLOW_MALLOC
#define FLOW_MALLOC(sz)      malloc(sz)
#define FLOW_CALLOC(n, sz)   calloc(n, sz)
#define FLOW_REALLOC(p, sz)  realloc(p, sz)
#define FLOW_FREE(p)         free(p)
#endif
#ifndef FLOW_ASSERT
#include <assert.h>
#define FLOW_ASSERT(x)       assert(x)
#endif
/* POSIX/system headers used ONLY by the implementation (flow_term / flow_run).
   Hoisted here — OUTSIDE the extern "C" below — because a system header inside
   extern "C" is ill-formed and breaks libstdc++/libc++ on some platforms. The
   amalgamator closes the extern "C" at the end of flow.h. */
#ifdef FLOW_IMPLEMENTATION
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
