#pragma once
#include <stdint.h>

// Fixed-size, stack-allocated circular buffer.
// No heap allocation — safe for ESP32's limited RAM.
// push(): O(1)   |   at(i): O(1)   |   statistics: O(count)
template <typename T, uint16_t N>
struct CircularBuffer {
    T        data[N];
    uint16_t head  = 0;
    uint16_t count = 0;

    // Overwrite oldest element when full.
    void push(T val) {
        data[head] = val;
        head = (head + 1) % N;
        if (count < N) count++;
    }

    // Logical index: 0 = oldest element, count-1 = newest element.
    T at(uint16_t i) const {
        return data[(head - count + i + N) % N];
    }

    bool full()  const { return count == N; }
    bool empty() const { return count == 0; }

    void clear() {
        head  = 0;
        count = 0;
    }
};
