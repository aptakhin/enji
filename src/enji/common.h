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

class Defer {
public:
    typedef std::function<void (void)> Deleter;

    Defer(Deleter&& deleter)
    :   deleter_(deleter) {}

    ~Defer() {
        deleter_();
    }

    void reset(Deleter&& deleter) {
        deleter_ = deleter;
    }

private:
    Deleter deleter_;
};

template<typename Resource>
class ScopeExit {
public:
    typedef std::function<void(Resource*)> Deleter;

    ScopeExit() { }

    ScopeExit(Resource&& resource, Deleter&& deleter)
    :   on_{std::forward<Resource>(resource), std::forward<Deleter>(deleter)} {}

    void reset(Resource&& resource, Deleter&& deleter) {
        on_ = std::unique_ptr<Resource, Deleter>{
            std::forward<Resource>(resource), std::forward<Deleter>(deleter)
        };
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
    :   on_{resource, std::forward<Deleter>(deleter)} { }

    void reset(Resource* resource, Deleter&& deleter) {
        on_ = std::unique_ptr<Resource, Deleter>{resource, std::forward<Deleter>(deleter)};
    }

    Resource* operator ~() const {
        return on_.get();
    }

private:
    std::unique_ptr<Resource, Deleter> on_;
};


class Thread {
public:
    typedef std::function<void (void)> Func;

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
        std::lock_guard<std::mutex> guard{mutex_};
        queue_.emplace(value);
    }

    bool pop(T& obj) {
        std::lock_guard<std::mutex> guard{mutex_};
        if (queue_.empty())
            return false;
        obj = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard{mutex_};
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
#define ZEROCHECK(resp_code, exc, enji_error) UVCHECK(resp_code, exc, enji_error)

bool is_slash(const char c);

template<typename Append>
void path_join_append_one(String& to, Append append) {
    if (!to.empty() && !is_slash(to[to.size() - 1])) {
        to += '/';
    }
    to += append;
}

template<typename First>
void path_join_impl(String& buf, First first) {
    path_join_append_one(buf, first);
}

template<typename First, typename... Types>
void path_join_impl(String& buf, First first, Types... tail) {
    path_join_append_one(buf, first);
    path_join_impl(buf, tail...);
}

template<typename ... Types>
String path_join(Types... args) {
    String buf;
    path_join_impl(buf, args...);
    return buf;
}

String path_dirname(const String& filename);
String path_extension(const String& filename);

template <typename ContA, typename ContB>
class Zip {
public:
    typedef typename ContA::value_type ValA;
    typedef typename ContB::value_type ValB;

    Zip(ContA& a, ContB& b)
        : begin_(a.begin(), b.begin()),
        end_(a.end(), b.end()) {
    }

    template <typename IterA, typename IterB>
    class Iter {
    private:
        friend class Zip;

        Iter(IterA ait, IterB bit)
            : ait_(ait), bit_(bit) {}

    public:
        Iter& operator ++() {
            ++ait_, ++bit_;
            return *this;
        }

        bool operator == (const Iter& other) const {
            return ait_ == other.ait_ && bit_ == other.bit_;
        }

        bool operator != (const Iter& other) const {
            return !(*this == other);
        }

        Iter& operator * () {
            fst = &*ait_; snd = &*bit_;
            return *this;
        }

        ValA* fst;
        ValB* snd;

    private:
        IterA ait_;
        IterB bit_;
    };

    typedef Iter<typename ContA::iterator, typename ContB::iterator> It;

    It begin() const { return begin_; }
    It end() const { return end_; }

private:
    It begin_, end_;
};

template <typename ContA, typename ContB>
Zip<ContA, ContB> zip(ContA& a, ContB& b) {
    return{a, b};
}

enum class ValueType {
    NONE,
    DICT,
    ARRAY,
    REAL,
    STR,
};

class Value {
public:
    Value();
    explicit Value(std::map<Value, Value> dict);
    explicit Value(std::vector<Value> arr);
    Value(double d);
    Value(String str);
    Value(const char* str);

    std::map<Value, Value>& dict();
    std::vector<Value>& array();
    double& real();
    String& str();

    const std::map<Value, Value>& dict() const;
    const std::vector<Value>& array() const;
    const double& real() const;
    const String& str() const;

    Value& operator [] (const char* key);
    const Value& operator [] (const char* key) const;

    const std::map<Value, Value>* is_dict() const;
    const std::vector<Value>* is_array() const;
    const double* is_real() const;
    const String* is_str() const;

    ValueType type() const { return type_; }

private:
    ValueType type_;

    std::map<Value, Value> dict_;
    std::vector<Value> array_;
    double real_;
    std::string str_;
};

bool operator < (const Value& a, const Value& b);
bool operator == (const Value& a, const Value& b);
bool operator != (const Value& a, const Value& b);

} // namespace enji