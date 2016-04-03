#include "common.h"

namespace enji {

void run_thread(void* arg) {
    Thread* thread = reinterpret_cast<Thread*>(arg);
    thread->func_();
}

void Thread::run(Func&& thread_func) {
    func_ = std::move(thread_func);
    uv_thread_create(&thread_, run_thread, this);
}

StringView::StringView(const char* data, ssize_t size)
:   data{data},
    size{static_cast<size_t>(size)} {
}

TransferBlock::TransferBlock()
:   data{nullptr},
    size{0} {
}

TransferBlock::TransferBlock(const char* data, size_t size)
:   data{data},
    size{size} {
}

void TransferBlock::free() {
    if (data) {
        deleter(const_cast<char*>(data));
    }
}

void TransferBlock::to_uv_buf(uv_buf_t* cpy) {
    cpy->base = (char*) data;
    cpy->len = size_t(size);
}

bool is_slash(const char c) {
    return c == '/' || c == '\\';
}

String path_dirname(const String& filename) {
    auto last_slash = filename.find_last_of('/');
    if (last_slash == String::npos) {
        last_slash = filename.find_last_of("\\\\");
        if (last_slash == String::npos) {
            return filename;
        }
    }
    return filename.substr(0, last_slash);
}

String path_extension(const String& filename) {
    auto last_dot = filename.find_last_of('.');
    if (last_dot == String::npos) {
        return "";
    } else {
        return filename.substr(last_dot + 1);
    }
}

Value::Value()
:   type_(ValueType::NONE) {}

Value::Value(std::map<Value, Value> dict)
:   type_(ValueType::DICT),
    dict_(dict) {}

Value::Value(std::vector<Value> arr)
:   type_(ValueType::ARRAY),
    array_(arr) {}

Value::Value(double real)
:   type_(ValueType::REAL),
    real_(real) {}

Value::Value(String str)
:   type_(ValueType::STR),
    str_(str) {}

Value::Value(const char* str)
:   type_(ValueType::STR),
    str_(str) {}

Value Value::make_dict() {
    return Value(std::map<Value, Value>{});
}

Value Value::make_array() {
    return Value(std::vector<Value>{});
}

std::map<Value, Value>& Value::dict() {
    if (type_ != ValueType::DICT)
        throw std::logic_error("Value is not a dict!");
    return dict_;
}

std::vector<Value>& Value::array() {
    if (type_ != ValueType::ARRAY)
        throw std::logic_error("Value is not an array!");
    return array_;
}

double& Value::real() {
    if (type_ != ValueType::REAL)
        throw std::logic_error("Value is not a real!");
    return real_;
}

String& Value::str() {
    if (type_ != ValueType::STR)
        throw std::logic_error("Value is not a string!");
    return str_;
}

const std::map<Value, Value>& Value::dict() const {
    if (type_ != ValueType::DICT)
        throw std::logic_error("Value is not a dict!");
    return dict_;
}

const std::vector<Value>& Value::array() const {
    if (type_ != ValueType::ARRAY)
        throw std::logic_error("Value is not an array!");
    return array_;
}

const double& Value::real() const {
    if (type_ != ValueType::REAL)
        throw std::logic_error("Value is not a real!");
    return real_;
}

const String& Value::str() const {
    if (type_ != ValueType::STR)
        throw std::logic_error("Value is not a string!");
    return str_;
}

const std::map<Value, Value>* Value::is_dict() const {
    return type_ == ValueType::DICT ? &dict_ : nullptr;
}

const std::vector<Value>* Value::is_array() const {
    return type_ == ValueType::ARRAY ? &array_ : nullptr;
}

const double* Value::is_real() const {
    return type_ == ValueType::REAL ? &real_ : nullptr;
}

const String* Value::is_str() const {
    return type_ == ValueType::STR ? &str_ : nullptr;
}

Value& Value::operator [] (const char* key) {
    if (type_ != ValueType::DICT)
        throw std::logic_error("Value is not a dict!");
    return dict_[key];
}

const Value& Value::operator [] (const char* key) const {
    if (type_ != ValueType::DICT)
        throw std::logic_error("Value is not a dict!");
    return dict_.at(key);
}

bool operator < (const Value& a, const Value& b) {
    if (a.is_real() && b.is_real()) {
        return a.real() < b.real();
    }
    else if (a.is_str() && b.is_str()) {
        return a.str() < b.str();
    }
    return true;
}

bool operator == (const Value& a, const Value& b) {
    if (a.type() != b.type()) {
        return false;
    }

    if (a.is_dict() && b.is_dict()) {
        auto u = a.dict();
        auto v = b.dict();
        if (u.size() != v.size()) {
            return false;
        }
        for (auto&& cmp : zip(u, v)) {
            if (cmp.fst->first != cmp.snd->first || cmp.fst->second != cmp.snd->second) {
                return false;
            }
        }
    }
    else if (a.is_array() && b.is_array()) {
        if (a.array().size() != b.array().size()) {
            return false;
        }
        auto u = a.array();
        auto v = b.array();
        if (u.size() != v.size()) {
            return false;
        }
        for (auto&& cmp : zip(u, v)) {
            if (cmp.fst != cmp.snd) {
                return false;
            }
        }
    }
    else if (a.is_str() && b.is_str()) {
        return a.str() == b.str();
    }
    else if (a.is_real() && b.is_real()) {
        return a.real() == b.real();
    }

    return true;
}

bool operator != (const Value& a, const Value& b) {
    return !(a == b);
}

} // namespace enji
