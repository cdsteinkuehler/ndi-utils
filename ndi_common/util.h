/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Set the current thread's priority to the max
// offset_from_max is essentially sched_get_priority_max() - offset_from_max
bool set_max_priority(int offset_from_max=0);
