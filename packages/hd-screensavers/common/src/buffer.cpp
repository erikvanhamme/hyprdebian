#include "buffer.hpp"

#include "gpu.hpp"

#include <stdexcept>

Buffer::Buffer(Gpu& gpu, VkDeviceSize size, VkBufferUsageFlags usage) : gpu_(gpu), size_(size)
{
    const VkDevice device = gpu_.device();
    const VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    check(vkCreateBuffer(device, &info, nullptr, &buffer_), "create a buffer");
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer_, &requirements);

    // Device local as well when the device has it, else host memory that the
    // device reads.
    constexpr VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t type = 0;
    try {
        type = gpu_.memory_type(requirements.memoryTypeBits, host | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    } catch (const std::runtime_error&) {
        type = gpu_.memory_type(requirements.memoryTypeBits, host);
    }
    const VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = type,
    };
    VkResult result = vkAllocateMemory(device, &alloc, nullptr, &memory_);
    if (result == VK_SUCCESS) {
        result = vkBindBufferMemory(device, buffer_, memory_, 0);
    }
    if (result == VK_SUCCESS) {
        result = vkMapMemory(device, memory_, 0, VK_WHOLE_SIZE, 0, &data_);
    }
    if (result != VK_SUCCESS) {
        // The destructor does not run for a constructor that throws.
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory_, nullptr);
        }
        vkDestroyBuffer(device, buffer_, nullptr);
        check(result, "allocate the memory of a buffer");
    }
}

Buffer::~Buffer()
{
    const VkDevice device = gpu_.device();
    vkDestroyBuffer(device, buffer_, nullptr);
    vkFreeMemory(device, memory_, nullptr);
}
