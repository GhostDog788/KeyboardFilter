#pragma once
#include "StaticBuffer.h"

namespace kstd {

	template<typename T, unsigned int SIZE>
	class CircularBuffer : public StaticBuffer<T, SIZE>
	{
	public:
		CircularBuffer() : StaticBuffer<T, SIZE>() {}
		virtual ~CircularBuffer() = default;

		virtual void push(const T& item)
		{
			size_t prev_head = m_head;
			m_head = (m_head + 1) % SIZE;
			if (prev_head != -1 && prev_head > m_head) m_head_overflowed = true;
			this->m_buffer[m_head] = item;
		}

		virtual bool head(T& head) const {
			if (m_head == -1) return false;
			head = this->m_buffer[m_head];
			return true;
		}

		virtual bool at(size_t offset_from_head, T& item) const {
			if (offset_from_head >= SIZE || (m_head == -1)) return false;
			if (!m_head_overflowed && offset_from_head > m_head) return false;
			int num_head = (int)m_head;
			if ((int)(m_head - offset_from_head) < 0) num_head += SIZE;
			item = this->m_buffer[(num_head - (int)offset_from_head) % SIZE];
			return true;
		}

	private:
		size_t m_head = (size_t)(- 1);
		bool m_head_overflowed = false;
	};
}
