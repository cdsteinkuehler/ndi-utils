/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define LOG_FATAL    (1)
#define LOG_ERR      (2)
#define LOG_WARN     (3)
#define LOG_INFO     (4)
#define LOG_DBG      (5)

#define LOG(level, ...) do {  \
	if (level <= debug_level) { \
		fprintf(dbgstream, __VA_ARGS__); \
		if (debug_flush) { \
			fflush(dbgstream); \
		} \
	} \
} while (0)

extern FILE *dbgstream;
extern int  debug_level;
extern bool debug_flush;
