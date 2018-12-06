/*
 * main.cpp
 *
 *  Created on: Nov 30, 2018
 *      Author: aohorodnyk
 */


#include <iostream>
#include <exception>
#include <thread>
#include <random>

#include <boost/program_options.hpp>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>

#include <block_manager.hpp>
#include <pipe.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;


class count_down {
public:
    count_down(uint64_t count): count_(count){}
    bool operator() () {
        return count_? count_-- != 0 : false;
    }
private:
    uint64_t count_;
};

typedef std::pair<block_manager::block_ptr_t, size_t> data_segment_t;
typedef boost::crc_32_type crc32_calculator_t;
typedef pipe_barrier<data_segment_t, crc32_calculator_t::value_type> reducer_t;
typedef pipe_multiplex<data_segment_t> multiplexer_t;


class summary{
    fs::path path_;
    uint64_t count_;
public:

    summary(std::string path): count_(0) {
        path_ = fs::canonical(fs::absolute(path));
    }

    void operator ()( const reducer_t::result_t &res ) {
        count_++;
        if ( res.values_.empty( ) ) {
            std::cout << __PRETTY_FUNCTION__ << "Block# " << count_
                      << " don't have calculated CRS's" << std::endl;
            return;
        }

        if ( res.values_.size( ) == 1 ) {
            ; // nothing to compare
        } else if ( std::find_if( res.values_.begin( ) + 1, res.values_.end( ),
                [&res](const reducer_t::value_t &val) -> bool {
                    return val != *res.values_.begin();
                } ) != res.values_.end( ) /*|| true*/ ) {
            fs::path cp = path_;
            std::stringstream fn;
            fn << getpid() << "-block_" << count_ << ".dat";
            cp.append(fn.str());
            std::cout << __PRETTY_FUNCTION__
                    << "Block#: " << count_ << "Status: FAILED, data stored:" << cp
                    << std::endl;
            std::ofstream data(cp.string() , std::ios::binary);
            data.write(res.data_.first.get(), res.data_.second);
            data.close();
            return;
        }
        std::cout << __PRETTY_FUNCTION__ << ": Block# " << std::dec << count_
                << " Status: OK crc: 0x" << std::hex << *res.values_.begin( ) << std::endl;
    }
};


class data_producer {
protected:
    std::shared_ptr<std::random_device> random_dev_;
    void fill_data(block_manager::block_ptr_t &data, size_t size) {
        std::random_device::result_type *ptr = (std::random_device::result_type*)data.get();
        size_t iter = size / sizeof(std::random_device::result_type);
        size_t leftover = size % sizeof(std::random_device::result_type);
        for(size_t i = 0; i < iter; ++i, ++ptr) {
            *ptr = (*random_dev_)();
        }
        auto tmp = (*random_dev_)();
        memcpy(ptr, &tmp, leftover);
    };

public:
    data_producer(
            std::function<bool()> run_predecate,
            uint64_t size,
            block_manager& allocator,
            multiplexer_t& destination,
            const std::string& name): name_(name), run_predecate_(run_predecate),
                                      size_(size), allocator_(allocator),
                                      destination_(destination)
    {
        random_dev_ = std::make_shared<std::random_device>();
    }

    void operator() () {
        try {
            while(run_predecate_()) {
                block_manager::block_ptr_t data = allocator_.get_block();
                fill_data(data, size_);
                destination_.publish(std::make_pair(data, size_));
            }
        } catch (const std::exception &e) {
            std::cout << "Unexpected death of thread: " << name_
                      << " due to error: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "Unexpected and sudden death of thread: " << name_ << std::endl;
        }
    }

private:
    const std::string name_;
    std::function<bool()> run_predecate_;
    const uint64_t size_;
    block_manager& allocator_;
    multiplexer_t& destination_;
};


class calculator {
protected:
    crc32_calculator_t::value_type build_checksum(const void* data, size_t size) {
        crc32_calculator_t cl;
        cl.process_bytes(data, size);
        return cl.checksum();
    }

public:
    calculator(
            std::function<bool()> run_predecate,
            multiplexer_t::source source,
            reducer_t& reduce_pipe,
            std::string name) : run_predecate_(run_predecate), source_(source),
                                reduce_pipe_(reduce_pipe), name_(name)
    {}

    void operator () () {
        try {
            while(run_predecate_()) {
                auto data = source_.consume();
                reduce_pipe_.publish(data, build_checksum(data.first.get(), data.second));
            }
        } catch (const std::exception &e) {
            std::cout << "Unexpected death of thread: " << name_
                      << " due to error: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "Unexpected and sudden death of thread: " << name_ << std::endl;
        }
    }

private:
    std::function<bool()> run_predecate_;
    multiplexer_t::source source_;
    reducer_t& reduce_pipe_;
    const std::string name_;
};


int main(const int argc, const char *argv[]) {
    // parse input arguments
    po::variables_map vm;
    try {
        po::options_description def("Sample applications, generates random blocks and calculate crc32 for each");
        auto range_check = [] (uint64_t v) {
            if(v < 1 || v > UINT64_MAX/2) {
                throw std::runtime_error("Value out of range");
            }
        };
        def.add_options()
                ("help,h", "gives usage information")
                ("items,i", po::value<uint64_t>()->required()->notifier(range_check),
                        "Number of blocks to produce")
                ("sources,s", po::value<uint64_t>()->required()->notifier(range_check),
                         "Source producers count")
                ("size,b", po::value<uint64_t>()->required()->notifier(range_check),
                         "Data block size")
                ("calculators,c", po::value<uint64_t>()->required()->notifier(range_check),
                         "Count of CRC calculation units")
                ("folder,f", po::value<std::string>()->default_value(".")->notifier(
                        [](const std::string& v){
                            if (!fs::is_directory(v)){
                                throw std::runtime_error("Given folder path is invalid or not exists");
                            }
                        }),
                        "Path to folder to store broken blocks")
                ;
        po::store(po::parse_command_line(argc, argv, def), vm);

        if(vm.count("help")){
            std::cout << def << std::endl;
            return 0;
        }

        po::notify(vm);
    } catch (const std::exception &e) {
        std::cout << "Invalid input argument, error: " << e.what() << std::endl
                  << "Use help (-h|--help) for more details " << std::endl;
        return -255;
    }

    int res = 0;

    try {
        std::cout << "Start setup" << std::endl;
        // setup
        auto producer_threads = vm["sources"].as<uint64_t>();
        auto calculator_threads = vm["calculators"].as<uint64_t>();
        auto allocator_capacity = (producer_threads * 2 + 1) + producer_threads / 10;
        auto block_size = vm["size"].as<uint64_t>();
        auto items = vm["items"].as<uint64_t>();
        auto path = vm["folder"].as<std::string>();

        block_manager blocks(block_size, allocator_capacity);
        std::random_device rd;
        summary summurizer(path);

        multiplexer_t multiplexer;
        reducer_t reducer(calculator_threads, summurizer);

        std::mutex items_lock;
        auto producer_go = [&items, &items_lock]() -> bool {
            std::lock_guard<std::mutex> g(items_lock);
            return items ? items-- != 0 : false;
        };

        std::vector<std::thread> calculators;
        calculators.reserve(calculator_threads);
        for(size_t c = 0; c < calculator_threads; ++c) {
            std::stringstream name;
            name << "Reducer_" << c << std::endl;
            calculators.emplace_back(calculator(count_down(items), multiplexer.get_consumer(),
                    reducer, name.str()));

        }

        std::vector<std::thread> producers;
        producers.reserve(producer_threads);
        for(size_t  g = 0; g < producer_threads; ++g) {
            std::stringstream name;
            name << "Producer_" << g << std::endl;
            producers.emplace_back(data_producer(producer_go, block_size,
                    blocks, multiplexer, name.str()));
        }

        // process
        std::cout << "Start processing" << std::endl;

        // wait for completion
        std::for_each(producers.begin(), producers.end(), [](std::thread &th) {th.join();});
        std::for_each(calculators.begin(), calculators.end(), [](std::thread &th) {th.join();});

        std::cout << "Done" << std::endl;
    } catch (const std::exception &e) {
        res = -2;
        std::cerr << "Exception: " << e.what() << " interrupted main process - exist";
    } catch (...) {
        res = -1;
        std::cerr << "Exception: UNKNOWN interrupted main process - exist";
    }

    return res;
}

