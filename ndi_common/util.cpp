/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#include "stdafx.h"
#include "util.h"
#include <sys/time.h>

bool set_max_priority(int offset_from_max)
{
#ifndef _WIN32
	struct sched_param params = { };
	params.sched_priority = sched_get_priority_max(SCHED_FIFO) - offset_from_max;
	return pthread_setschedparam(pthread_self(), SCHED_FIFO, &params) == 0;
#else
	return false;
#endif
}

