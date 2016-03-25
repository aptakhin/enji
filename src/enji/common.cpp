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

OweMem::OweMem()
:   data{nullptr},
    size{0} {
}

OweMem::OweMem(const char* data, size_t size)
:   data{data},
    size{size} {
}

void OweMem::to_uv_buf(uv_buf_t* cpy) {
    cpy->base = (char*) data;
    cpy->len = size_t(size);
}

} // namespace enji