#pragma once

#include <cstdlib>
#include <memory>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/context/all.hpp>
#include <boost/coroutine/all.hpp>

namespace enji {

template<std::size_t Max, std::size_t Default, std::size_t Min>
class simple_stack_allocator {
public:
    static std::size_t maximum_stacksize() { return Max; }

    static std::size_t default_stacksize() { return Default; }

    static std::size_t minimum_stacksize() { return Min; }

    void* allocate(std::size_t size) const {
        BOOST_ASSERT(minimum_stacksize() <= size);
        BOOST_ASSERT(maximum_stacksize() >= size);

        void* limit = std::malloc(size);
        if (!limit) throw std::bad_alloc();

        return static_cast< char* >( limit) + size;
    }

    void deallocate(void* vp, std::size_t size) const {
        BOOST_ASSERT(vp);
        BOOST_ASSERT(minimum_stacksize() <= size);
        BOOST_ASSERT(maximum_stacksize() >= size);

        void* limit = static_cast< char* >( vp) - size;
        std::free(limit);
    }
};

typedef simple_stack_allocator<
    8 * 1024 * 1024, // 8MB
    64 * 1024, // 64kB
    8 * 1024 // 8kB
> stack_allocator;

class Instance;

class Coro {
public:
    Coro(Instance* parent);

    void set(std::function<void(void)>&& func);

    void yield();

private:
    Instance* parent_;

    boost::context::fcontext_t ctx_;

    std::function<void(void)> func_;
};

class Instance {
public:
    Coro* create();

    void switch_from(Coro* coro);

    void switch_to(Coro* coro);

private:
    boost::context::fcontext_t ctx_;

    Coro* current_;
};


} // namespace enji