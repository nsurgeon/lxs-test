/*
 * dispatcher.hpp
 *
 * event , job dispatchers between workers
 *
 *  Created on: Dec 3, 2018
 *      Author: aohorodnyk
 */

#pragma once

#include <list>
#include <map>
#include <mutex>
#include <condition_variable>


template<class M>
class pipe_multiplex {
public:
    typedef std::list<M> message_list_t;
    class source {
    public:
        source(message_list_t &q, std::mutex &l, std::condition_variable &c)
        :
            lock_(l), notify_(c), events_(q)
        { }

        M consume() {
            std::unique_lock<std::mutex> g(lock_);
            while (events_.empty()) {
                notify_.wait(g);
            }
            if(!events_.empty()) {
                M val = *events_.begin();
                events_.pop_front();
                return val;
            }
            return  M();
        }

        M operator()() {
            return consume();
        }

    private:
        std::mutex &lock_;
        std::condition_variable &notify_;
        message_list_t &events_;
    };

public:
    source get_consumer() {
        std::lock_guard<std::mutex> g(lock_);
        consumers_.push_back(message_list_t());
        return source(consumers_.back(), lock_, notify_);
    }

    void publish(const M &message) {
        std::lock_guard<std::mutex> g(lock_);
        for(auto& queue : consumers_) {
            queue.push_back(message);
        }
        notify_.notify_all();
    }

private:
    typedef std::list<message_list_t> consumers_list_t;

private:
    std::mutex lock_;
    std::condition_variable notify_;
    consumers_list_t consumers_;
};


template<class M, class V, typename ID = void* >
class pipe_barrier {
public:
    typedef ID id_t;
    typedef V value_t;
    typedef std::vector<value_t> values_list_t;

    struct result_t {
        values_list_t values_;
        M             data_;
        uint64_t      count_;
    };

    typedef std::map< id_t, result_t> storage_map_t;
    typedef std::function<void (const result_t&)> consumer_t;

public:

    pipe_barrier(const uint64_t barrier_val, consumer_t processor)
    :
        PUBLISH_BARRIER(barrier_val), processor_(processor)
    {}

    void publish(M &message, const value_t &val) {
        std::lock_guard<std::mutex> g(lock_);
        id_t id = message.first.get();
        auto rs = data_map_.find(id);
        if(rs != data_map_.end()) {
            rs->second.count_++;
            rs->second.values_.push_back(val);
        } else {
            rs = data_map_.insert(std::make_pair(id, result_t{{val}, message, 1})).first;
        }
        if(rs->second.count_ == PUBLISH_BARRIER) {
            processor_(rs->second);
            data_map_.erase(rs);
        }
    }

private:
    const uint64_t PUBLISH_BARRIER;
    consumer_t processor_;

    std::mutex lock_;
    std::condition_variable notify_;
    storage_map_t data_map_;
};

