#include <cstdlib>

#include "rtos_psram.hpp"

namespace rtos::memory {

void *rtos_psram_malloc(std::size_t size)
{
	void *ptr = std::malloc(size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *rtos_psram_calloc(std::size_t num, std::size_t size)
{
	void *ptr = std::calloc(1, num * size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = num * size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *rtos_psram_realloc(void *ptr, std::size_t size)
{
	if (!ptr)
		return rtos_psram_malloc(size);
	void *original = static_cast<char *>(ptr) - sizeof(std::size_t);
	void *resized = std::realloc(original, size + sizeof(std::size_t));
	if (!resized)
		return nullptr;
	*static_cast<std::size_t *>(resized) = size;
	return static_cast<char *>(resized) + sizeof(std::size_t);
}

void rtos_psram_free(void *ptr)
{
	if (!ptr)
		return;
	std::free(static_cast<char *>(ptr) - sizeof(std::size_t));
}

std::size_t rtos_psram_allocated_size(void *ptr)
{
	if (!ptr)
		return 0;
	return *reinterpret_cast<std::size_t *>(static_cast<char *>(ptr) - sizeof(std::size_t));
}

}