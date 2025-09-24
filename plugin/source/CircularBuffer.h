#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

template <typename FloatType>
class CircularBuffer
{
public:
    explicit CircularBuffer (size_t capacityPow2) : capacity (nextPowerOfTwo (capacityPow2)), mask (capacity - 1), write (0), read (0)
    {
        buffer.assign (capacity, FloatType {});
    }

    void pushBlock (const FloatType* in, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            buffer[write] = in[i];
            write = (write + 1) & mask;
        }
        snapshot.store (write, std::memory_order_release);
    }

    void popBlock (FloatType* output, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            output[i] = buffer[read];
            read = (read + 1) & mask;
        }
    }

    void copyForGui (std::vector<FloatType>& dest) const {}
    size_t getCapacity() const noexcept { return capacity; }

private:
    std::vector<FloatType> buffer;
    size_t capacity, mask;
    size_t write;
    size_t read;
    std::atomic<size_t> snapshot { 0 };

    static size_t nextPowerOfTwo (size_t v) noexcept
    {
        if (v == 0) return 1; // 2^0 = 1

        --v; // handle case where v is already a power of two
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
#if SIZE_MAX > UINT32_MAX
        v |= v >> 32;
#endif
        return ++v;
    }
};
