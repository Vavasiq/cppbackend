#include "http_server.h"

namespace http_server {

// Реализация функции ReportError
void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    std::cerr << what << ": "sv << ec.message() << std::endl;
}

// Реализация конструктора SessionBase
SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {
}

// Реализация деструктора SessionBase
SessionBase::~SessionBase() = default;

// Реализация метода Run
void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

// Реализация метода Read
void SessionBase::Read() {
    using namespace std::literals;
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

// Реализация метода OnRead
void SessionBase::OnRead(beast::error_code ec, std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

// Реализация метода OnWrite
void SessionBase::OnWrite(bool close, beast::error_code ec, std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        return ReportError(ec, "write"sv);
    }
    if (close) {
        return Close();
    }
    Read();
}

// Реализация метода Close
void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    ReportError(ec, "close");
}

}  // namespace http_server
