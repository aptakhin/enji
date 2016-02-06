#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <thread>

#ifdef _MSC_VER
#   include <mutex>
#endif

#include <uv.h>

namespace enji {

class Connection;

typedef std::string String;

template<typename Resource>
class ScopeExit {
public:
    typedef std::function<void(Resource*)> Deleter;

    ScopeExit() { }

    ScopeExit(Resource&& resource, Deleter&& deleter)
    :   on_(std::forward<Resource>(resource), std::forward<Deleter>(deleter)) {}

    void reset(Resource&& resource, Deleter&& deleter) {
        on_ = std::unique_ptr<Resource, Deleter>(
            std::forward<Resource>(resource), std::forward<Deleter>(deleter)
        );
    }

    Resource* operator ~() {
        return on_.get();
    }

private:
    std::unique_ptr<Resource, Deleter> on_;
};

template<typename Resource>
class ScopePtrExit {
public:
    typedef std::function<void(Resource*)> Deleter;

    ScopePtrExit() { }

    ScopePtrExit(Resource* resource, Deleter&& deleter)
    :   on_(resource, std::forward<Deleter>(deleter)) { }

    void reset(Resource* resource, Deleter&& deleter) {
        on_ = std::unique_ptr<Resource, Deleter>(resource, std::forward<Deleter>(deleter));
    }

    Resource* operator ~() const {
        return on_.get();
    }

private:
    std::unique_ptr<Resource, Deleter> on_;
};


class Thread {
public:
    typedef std::function<void(void)> Func;

    friend void run_thread(void* arg);

    void run(Func&& thread_func);

    //static Thread& current();

protected:
    uv_thread_t thread_;
    Func func_;
};

template<typename T>
class SafeQueue {
public:
    void push(T&& value) {
        std::lock_guard<std::mutex> guard(mutex_);
        queue_.emplace(value);
    }

    bool pop(T& obj) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (queue_.empty())
            return false;
        obj = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
};

class IInputStream {
public:
    virtual size_t read(char* data, size_t bytes) = 0;

    virtual void close() = 0;

    virtual ~IInputStream() { }
};

class IOutputStream {
public:
    virtual void write(const char* data, size_t bytes) = 0;

    virtual void close() = 0;

    virtual void flush() = 0;

    virtual ~IOutputStream() { }
};

struct WriteContext {
    uv_write_t req;
    uv_buf_t buf;
    Connection* conn;
    bool close = false;
};

class StringView {
public:
    StringView(const char* data, ssize_t size);

public:
    const char* data;
    size_t size;
};

class OweMem {
public:
    OweMem();

    OweMem(const char* data, size_t size);

    void to_uv_buf(uv_buf_t* cpy);

public:
    const char* data;
    size_t size;
};


template <typename Exc>
void uvcheck(int resp_code, String&& enji_error, const char* file, int line) {
    if (resp_code != 0) {
        std::ostringstream err;
        err << enji_error << " (uv code: " << resp_code << ", str: " << uv_strerror(resp_code) << ")";
        throw Exc(err.str());
    }
}

template <typename Exc>
void uvcheck(int resp_code, const char* enji_error, const char* file, int line) {
    uvcheck<Exc>(resp_code, String(enji_error), file, line);
}

#define UVCHECK(resp_code, exc, enji_error) { uvcheck<exc>((resp_code), (enji_error), __FILE__, __LINE__); }

} // namespace enji