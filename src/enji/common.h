#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <thread>

#include <uv.h>

namespace enji {

typedef std::string String;

template<typename Resource>
class ScopeExit {
public:
    typedef std::function<void(Resource*)> Deleter;

    ScopeExit() { }

    ScopeExit(Resource&& resource, Deleter&& deleter)
        : on_(std::forward<Resource>(resource), std::forward<Deleter>(deleter)) { }

    void reset(Resource&& resource, Deleter&& deleter) {
        on_ = std::unique_ptr<Resource, Deleter>(std::forward<Resource>(resource), std::forward<Deleter>(deleter));
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
        : on_(resource, std::forward<Deleter>(deleter)) { }

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

    T pop() {
        std::lock_guard<std::mutex> guard(mutex_);
        T top = std::move(queue_.front());
        queue_.pop();
        return top;
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
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

    virtual ~IOutputStream() { }
};

class StdInputStream : public IInputStream {
public:
    StdInputStream(std::stringstream& in);

    size_t read(char* data, size_t bytes) override;

    bool eof() const { return in_.eof(); }

    void close() override;

private:
    std::stringstream& in_;
};

class StdOutputStream : public IOutputStream {
public:
    StdOutputStream(std::stringstream& out);

    void write(const char* data, size_t bytes) override;

    void close() override;

private:
    std::stringstream& out_;
};


class IoBuffer {
public:

    std::unique_ptr<IInputStream> create_reader();

    std::unique_ptr<IOutputStream> create_writer();

private:
    std::stringstream buffer_;
};

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
    bool close = false;
} write_req_t;

class UvOutputStream : public IOutputStream {
public:
    UvOutputStream(uv_stream_t* stream, void (* cb_after_write)(uv_write_t*, int),
                   void (* cb_close)(uv_handle_t* handle));

    void write(const char* data, size_t bytes) override;

    void close() override;

private:
    uv_stream_t* stream_ = nullptr;

    void (* cb_after_write_)(uv_write_t*, int);

    void (* cb_close_)(uv_handle_t* handle);

    write_req_t* wr_ = nullptr;
};

} // namespace enji