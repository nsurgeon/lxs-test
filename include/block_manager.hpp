/*
 * bock_manager.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: aohorodnyk
 */

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>


/**
 * Fixed size allocator
 */
class block_manager {
public:
    typedef char data_t;

    class deleter {
    public:
        deleter(block_manager &owner)
        : owner_(owner)
        { }

        void operator () (data_t *p) const {
            owner_.return_block(p);
        }

    private:
        block_manager& owner_;
    };

    typedef std::shared_ptr< data_t > block_ptr_t;
    typedef std::function< void (data_t*, size_t) > setter_t;

protected:
    typedef std::shared_ptr< data_t > internal_prt_t;
    typedef std::vector<internal_prt_t> data_buffers_t;

    typedef std::vector<data_t*> raw_data_buffers_t;

public:
    block_manager(const size_t size, const size_t count,
            setter_t setter = [](data_t *d, size_t s){ memset(d, s * sizeof(d), 0);},
            size_t align = sizeof(void*));

    ~block_manager();

    void return_block(data_t *p);
    block_ptr_t get_block();

private:
    std::mutex lock_;
    std::condition_variable notify_;

    data_buffers_t arena_;
    raw_data_buffers_t available_;
    setter_t setter_;
    const size_t size_;
    block_ptr_t empty_;
};
