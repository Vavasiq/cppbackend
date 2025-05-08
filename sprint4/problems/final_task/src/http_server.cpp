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

template <typename Body, typename Fields>
void SessionBase::OnWrite(std::shared_ptr<http::response<Body, Fields>> safe_response, beast::error_code ec, std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        LOG_ERROR(ec.value(), ec.message(), "write");
        return ReportError(ec, "write"sv);
    }
    if (safe_response->need_eof()) {
        return Close();
    }
    std::string ip(stream_.socket().remote_endpoint().address().to_string());
    std::string content_type(safe_response->at(http::field::content_type));
    LOG_RESPONSE_SENT(ip, response_timer_.End(), static_cast<int>(safe_response->result()), content_type);
    Read();
}

using StringBody = http::basic_string_body<char>;
using Fields = http::basic_fields<std::allocator<char>>;
using FileBody = http::basic_file_body<beast::file_posix>;

template void SessionBase::OnWrite<StringBody, Fields>(
    std::shared_ptr<http::response<StringBody, Fields>>, beast::error_code, std::size_t);

template void SessionBase::OnWrite<FileBody, Fields>(
    std::shared_ptr<http::response<FileBody, Fields>>, beast::error_code, std::size_t);
    
}  // namespace http_server