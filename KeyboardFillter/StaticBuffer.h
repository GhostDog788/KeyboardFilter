#pragma once

namespace kstd {

	template<typename T, unsigned int SIZE>
	class StaticBuffer
	{
	public:
		StaticBuffer() : m_buffer{} {};
		StaticBuffer(const StaticBuffer& other) {
			_copy_from(other);
		}
		StaticBuffer& operator=(const StaticBuffer& other) {
			if (this != &other) {
				_copy_from(other);
			}
			return *this;
		}
		StaticBuffer(StaticBuffer&& other) = delete;
		StaticBuffer& operator=(StaticBuffer&& other) = delete;
		virtual ~StaticBuffer() = default;

		virtual T& operator[](size_t index) { return m_buffer[index]; }

		virtual size_t size() const { return SIZE; }
		virtual size_t size_in_bytes() const { return SIZE * sizeof(T); }

	protected:
		T m_buffer[SIZE];

		virtual void _copy_from(const StaticBuffer& other)
		{
			for (size_t i = 0; i < SIZE; ++i) {
				m_buffer[i] = other.m_buffer[i];
			}
		}
	};
}
