/*
 * block_manager.cpp
 *
 *  Created on: Dec 3, 2018
 *      Author: aohorodnyk
 */

#include <block_manager.hpp>


block_manager::block_manager(const size_t size, const size_t count, setter_t setter, size_t align)
: setter_(setter), size_(size + size % align)
{
    arena_.reserve(count);
    available_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        arena_.emplace_back( new data_t[size_] );
        setter_( arena_.back().get(), size_);
        available_.push_back(arena_.back().get());
    }
}

block_manager::~block_manager() {
    std::unique_lock<std::mutex> g(lock_);
    assert(arena_.size() == available_.size());
}

void block_manager::return_block(data_t *p) {
    std::unique_lock<std::mutex> g(lock_);
    available_.push_back(p);
    setter_(p, size_);
    notify_.notify_one();
}

block_manager::block_ptr_t block_manager::get_block() {
    std::unique_lock<std::mutex> g(lock_);
    while (available_.empty()) {
        notify_.wait(g);
    }
    if ( available_.size() ) {
        auto ptr = block_ptr_t(available_.back(), deleter(*this));
        available_.pop_back();
        return ptr;
    }
    return empty_;
}
