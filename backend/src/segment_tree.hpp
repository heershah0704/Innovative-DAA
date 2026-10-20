// segment_tree.hpp
// Iterative segment tree over recorded history.
//   update(i, v): change one second's value          -> O(log n)
//   query(l, r):  combine all values in [l, r]        -> O(log n)
// Used twice: a SUM tree over arrivals per second and a MAX tree over total queue.
#pragma once
#include <functional>
#include <stdexcept>
#include <vector>

template <typename T>
class SegmentTree {
public:
    SegmentTree(int size, std::function<T(T, T)> combine, T identity)
        : size_(size), combine_(combine), identity_(identity) {
        if (size < 1) throw std::invalid_argument("size must be positive");
        n_ = 1;
        while (n_ < size) n_ <<= 1;
        tree_.assign(2 * n_, identity);
    }

    void update(int i, T value) {
        if (i < 0 || i >= size_) throw std::out_of_range("index out of range");
        int p = i + n_;
        tree_[p] = value;
        for (p /= 2; p >= 1; p /= 2) tree_[p] = combine_(tree_[2 * p], tree_[2 * p + 1]);
    }

    T query(int l, int r) const {  // inclusive range
        if (l < 0 || r >= size_ || l > r) throw std::out_of_range("bad range");
        T left = identity_, right = identity_;
        int lo = l + n_, hi = r + n_ + 1;
        while (lo < hi) {
            if (lo & 1) left = combine_(left, tree_[lo++]);
            if (hi & 1) right = combine_(tree_[--hi], right);
            lo /= 2;
            hi /= 2;
        }
        return combine_(left, right);
    }

    int size() const { return size_; }

private:
    int size_, n_;
    std::function<T(T, T)> combine_;
    T identity_;
    std::vector<T> tree_;
};

inline SegmentTree<int> makeSumTree(int n) {
    return SegmentTree<int>(n, [](int a, int b) { return a + b; }, 0);
}
inline SegmentTree<int> makeMaxTree(int n) {
    return SegmentTree<int>(n, [](int a, int b) { return a > b ? a : b; }, -1);
}
