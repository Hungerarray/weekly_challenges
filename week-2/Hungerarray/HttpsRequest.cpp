#include <iostream>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "HttpsRequest.h"

using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

ssl::context Create_context()
{
    ssl::context ctx{ssl::context::sslv23_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_none);

    return ctx;
}

// ! another beast of a code, only to be used for challenge. There are better ways to do this then what I have done
void HttpsRequest::async_ConnectAndGet(Url &url, std::string_view reqHeader)
{
    static size_t i = 0;
    Setup_socket(url);

    std::cout << "Starting resolve " << i << std::endl;
    _resolver.async_resolve(url._host, "443",
                            [this, reqHeader, i = i](const system::error_code &er, tcp::resolver::results_type results)
                            {
                                // on resolving failure
                                if (er.failed())
                                {
                                    std::cout << "Resolving host failed: " << i << " " << er.message() << std::endl;
                                    return;
                                }
                                std::cout << "Starting connection" << std::endl;
                                // use the endpoints to connect
                                asio::async_connect(_ssl_socket.lowest_layer(), results,
                                                    [this, reqHeader, i](const system::error_code &er, const tcp::endpoint &endpoint)
                                                    {
                                                        // on failure to connect
                                                        if (er.failed())
                                                        {
                                                            std::cout << "Failed to connect to endpoint: " << i << " " << er.message() << std::endl;
                                                            return;
                                                        }
                                                        _ssl_socket.lowest_layer().set_option(ip::tcp::no_delay(true));
                                                        _ssl_socket.lowest_layer().non_blocking(true);

                                                        std::cout << "Starting handshake" << std::endl;
                                                        // perform handshake
                                                        _ssl_socket.async_handshake(ssl::stream<tcp::socket>::client,
                                                                                    [this, reqHeader, i](const system::error_code &er)
                                                                                    {
                                                                                        // on handshake failure
                                                                                        if (er.failed())
                                                                                        {
                                                                                            std::cout << "Handshake failed: " << i << " " << er.message() << std::endl;
                                                                                            return;
                                                                                        }

                                                                                        std::cout << "Starting request write" << std::endl;
                                                                                        // handshake successful, sending request header
                                                                                        asio::async_write(_ssl_socket, asio::buffer(reqHeader),
                                                                                                          [this, i](const system::error_code &er, size_t snd_bytes)
                                                                                                          {
                                                                                                              // on failure to send request header
                                                                                                              if (er.failed())
                                                                                                              {
                                                                                                                  std::cout << "Failed to send request header: " << i << " " << er.message() << std::endl;
                                                                                                                  return;
                                                                                                              }

                                                                                                              std::cout << "Starting request read" << std::endl;
                                                                                                              // request header sent, get response from server
                                                                                                              std::shared_ptr<std::string> buf{new std::string};
                                                                                                              asio::async_read(_ssl_socket, asio::dynamic_buffer(*buf),
                                                                                                                               [buf, i](const system::error_code &er, size_t recv_bytes)
                                                                                                                               {
                                                                                                                                   // on failure to get response
                                                                                                                                   if (er.failed() && er != asio::error::eof && er != asio::ssl::error::stream_truncated)
                                                                                                                                   {
                                                                                                                                       std::cout << "Failed to get response: " << i << " " << er.message() << std::endl;
                                                                                                                                       return;
                                                                                                                                   }

                                                                                                                                   // got response successfully
                                                                                                                                   // std::cout << *buf << std::endl;
                                                                                                                               });
                                                                                                          });
                                                                                    });
                                                    });
                            });
    ++i;
}

void HttpsRequest::Setup_socket(Url &url)
{
    SSL_set_tlsext_host_name(_ssl_socket.native_handle(), url._host.c_str());
    _ssl_socket.set_verify_callback(ssl::rfc2818_verification(url._host));
}

HttpsRequest::HttpsRequest(io_context &ctx)
    : WebRequestAbstract{ctx},
      _ctx{Create_context()},
      _ssl_socket{ctx, _ctx}

{
}

void HttpsRequest::Connect(Url &url)
{
    Setup_socket(url);
    socket_con_handshake(url);
}

void HttpsRequest::socket_con_handshake(Url &url)
{
    auto endpoints = _resolver.resolve(url._host, url._scheme);
    boost::asio::connect(_ssl_socket.lowest_layer(), endpoints);
    boost::system::error_code ec;
    _ssl_socket.handshake(ssl::stream<tcp::socket>::client, ec);
    if (ec.failed())
    {
        std::cout << "Handshake failed: " << ec.message() << std::endl;
        return;
    }
}

std::string HttpsRequest::Get(std::string_view reqHeader)
{
    boost::system::error_code ec;
    asio::write(_ssl_socket, asio::buffer(reqHeader), ec);

    if (ec.failed())
    {
        std::cout << "Error on write header: " << ec.message() << std::endl;
        return std::string{};
    }

    std::string temp;
    asio::read(_ssl_socket, asio::dynamic_buffer(temp), ec);
    return temp;
}

HttpsRequest::~HttpsRequest()
{
    _ssl_socket.async_shutdown([](const system::error_code &er) {
        return;
    });
}