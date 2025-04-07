#include "http_server.h"
#include <chrono>

namespace http_server {

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {
}

SessionBase::~SessionBase() {
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    using namespace std::literals;
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        LOG_ERROR(ec.value(), ec.message(), "read");
        return ReportError(ec, "read"sv);
    }
    std::string ip(stream_.socket().remote_endpoint().address().to_string());
    std::string url(request_.target());
    std::string method(request_.method_string());
    LOG_REQUEST_RECEIVED(ip, url, method);
    response_timer_.Start();
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    ReportError(ec, "close");
}

}  // namespace http_server
