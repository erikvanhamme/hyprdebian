/**
 * @file buffer.hpp
 * @brief A buffer of the device that the CPU writes into, as the vertices
 *        of the meshes and the instances of the scenes.
 *
 * The memory is host visible and coherent, mapped for as long as the buffer
 * lives: what the CPU writes is there for the next submit. It is also device
 * local when the device has such memory, as with resizable BAR or on an
 * integrated GPU.
 */
#pragma once

#include <cstddef>
#include <vulkan/vulkan.h>

class Gpu;

/** @brief A mapped buffer of the device. */
class Buffer {
public:
    /**
     * @brief Makes the buffer and maps it.
     * @param gpu   The device; it must outlive the buffer.
     * @param size  The size, in bytes.
     * @param usage How the buffer is used.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    Buffer(Gpu& gpu, VkDeviceSize size, VkBufferUsageFlags usage);

    /** @brief Destroys the buffer; no submit that uses it may still run. */
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    /** @brief The buffer. */
    VkBuffer buffer() const { return buffer_; }

    /** @brief The size, in bytes. */
    VkDeviceSize size() const { return size_; }

    /** @brief Where the CPU writes into the buffer. */
    void* data() const { return data_; }

private:
    /** @brief The device. */
    Gpu& gpu_;
    /** @brief The buffer. */
    VkBuffer buffer_ = VK_NULL_HANDLE;
    /** @brief Its memory. */
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    /** @brief The size, in bytes. */
    VkDeviceSize size_ = 0;
    /** @brief The mapping of the memory. */
    void* data_ = nullptr;
};
