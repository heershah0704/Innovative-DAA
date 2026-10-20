// priority_queue.hpp
// Binary max-heap written by hand (no std::priority_queue) so it can be explained.
//   push: add at the end, then swap upward while larger than the parent  -> O(log n)
//   pop:  take the root, move the last item to the root, swap downward    -> O(log n)
// `Before(a, b)` returns true when a must come out before b.
#pragma once
#include <utility>
#include <vector>

template <typename T, typename Before>
class MaxHeap {
public:
    explicit MaxHeap(Before before = Before()) : before_(before) {}

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
    const T& top() const { return heap_.front(); }

    void push(const T& item) {
        heap_.push_back(item);
        std::size_t i = heap_.size() - 1;
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!before_(heap_[i], heap_[parent])) break;
            std::swap(heap_[i], heap_[parent]);
            i = parent;
        }
    }

    T pop() {
        T top = heap_.front();
        heap_.front() = heap_.back();
        heap_.pop_back();
        std::size_t i = 0;
        while (true) {
            std::size_t l = 2 * i + 1, r = l + 1, best = i;
            if (l < heap_.size() && before_(heap_[l], heap_[best])) best = l;
            if (r < heap_.size() && before_(heap_[r], heap_[best])) best = r;
            if (best == i) break;
            std::swap(heap_[i], heap_[best]);
            i = best;
        }
        return top;
    }

    // All items in pop order, without changing this heap.
    std::vector<T> sorted() const {
        MaxHeap copy = *this;
        std::vector<T> out;
        while (!copy.empty()) out.push_back(copy.pop());
        return out;
    }

private:
    std::vector<T> heap_;
    Before before_;
};
