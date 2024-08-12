/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

// Default constructor
template <class T>
queue<T>::queue(void)
{
}

// Constructor with explicit maximum depth
template <class T>
queue<T>::queue(int max_depth)
	: m_max_depth(max_depth)
{
}

// Destructor
template <class T>
queue<T>::~queue(void)
{
}

// Add an item to the queue
template <class T>
bool queue<T>::push(std::shared_ptr<T> item)
{
	bool queue_maxed = false;

	// Lock the queue
	std::unique_lock<std::mutex> lock_queue(m_lock);

	// Queue this item
	m_queue.push(item);

	LOG(LOG_DBG, ">");	// Pushed an item on the queue

	// Drop items that are to old if the queue is not keeping up
	while ((m_max_depth > 0) && (m_queue.size() > m_max_depth))
	{
		m_queue.pop();
		LOG(LOG_ERR, "*");	// Dropped an item from the queue!
	}

	queue_maxed = ((m_max_depth > 0) && (m_queue.size() == m_max_depth));

	// Unlock and then wake up the thread
	lock_queue.unlock();

	// Wake it up
	m_condvar.notify_one();

	return !queue_maxed;
}

// Get an item off the queue (blocks until an item is actually available)
template <class T>
std::shared_ptr<T> queue<T>::pop()
{
	std::shared_ptr<T> item;

	// Lock the queue
	std::unique_lock<std::mutex> lock_queue(m_lock);

	// Until we are worken up with a frame
	while (m_queue.empty())
		m_condvar.wait(lock_queue);

	// Get the item from the queue
	item = m_queue.front();
	m_queue.pop();

	LOG(LOG_DBG, "<");	// Popped an item off the queue

	// We can unlock the queue now
	lock_queue.unlock();

	return item;
}

// Set the maximum depth of the queue
template <class T>
void queue<T>::set_depth(int max_depth)
{
	m_max_depth = max_depth;
}

// Get the current depth of the queue
template <class T>
int queue<T>::get_depth(void)
{
	return m_queue.size();
}

