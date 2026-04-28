#pragma once
#include <stdint.h>

template <typename T, uint16_t N>
struct CircularBuffer {
    T        data[N];
    uint16_t head  = 0;
    uint16_t count = 0;

    void push(T val) {
        data[head] = val;
        head = (head + 1) % N;
        if (count < N) count++;
    }

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
