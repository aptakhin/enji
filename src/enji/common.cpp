#include "common.h"

namespace enji {

void run_thread(void *arg) {
    Thread *thread = reinterpret_cast<Thread *>(arg);
    thread->func_();
}

void Thread::run(Func &&thread_func) {
    func_ = std::move(thread_func);
    uv_thread_create(&thread_, run_thread, this);
}


StdInputStream::StdInputStream(std::stringstream &in)
    : in_(in) {
}

size_t StdInputStream::read(char *data, size_t bytes) {
    in_.read(data, bytes);
    return in_.gcount();
};

void StdInputStream::close() {

}

StdOutputStream::StdOutputStream(std::stringstream &out)
    : out_(out) {
}

void StdOutputStream::write(const char *data, size_t bytes) {
    out_.write(data, bytes);
};

void StdOutputStream::close() {

}


UvOutputStream::UvOutputStream(uv_stream_t *stream, void (*cb_after_write)(uv_write_t *, int),
                               void (*cb_close)(uv_handle_t *))
    : stream_(stream),
      cb_after_write_(cb_after_write),
      cb_close_(cb_close) {

}

void UvOutputStream::write(const char *data, size_t bytes) {
    wr_ = new write_req_t;
    wr_->buf = uv_buf_init((char *) data, bytes);
    int r = uv_write(&wr_->req, stream_, &wr_->buf, 1, cb_after_write_);
}

void UvOutputStream::close() {
    uv_close((uv_handle_t *) wr_->req.handle, cb_close_);
}

} // namespace enji