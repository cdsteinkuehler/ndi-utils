/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#pragma once

template <class T>
struct queue
{
	// Constructor and destructor
	queue(void);
	queue(int max_depth);
	~queue(void);

	// Add an item to the queue
	bool push(std::shared_ptr<T> item);

	// Remove an item from the queue
	std::shared_ptr<T> pop(void);

	// Set the maximum depth of the queue
	void set_depth(int max_depth);

	// Get the current depth of the queue
	int get_depth(void);

private:
	// How many items can we have queued up
	std::size_t m_max_depth = 3;

	// The lock, condition variable, and queue
	std::mutex m_lock;
	std::condition_variable m_condvar;
	std::queue<std::shared_ptr<T>> m_queue;
};

#include "queue.hpp"
