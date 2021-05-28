#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <string_view>
#include <memory>

#include "Url.h"

class WebRequestAbstract
{
public:

    WebRequestAbstract(boost::asio::io_context &ctx) : _resolver{ctx} {}

    virtual void async_ConnectAndGet(Url &url, std::string_view reqHeader) = 0;

    virtual void Connect(Url &url) = 0;
    
    virtual std::string Get(std::string_view reqHeader) = 0;

    virtual void benchmark(Url &url, std::string reqHeader) = 0;

    void set_reqCount(size_t lim)
    {
        reqCount = lim;
    }

    virtual ~WebRequestAbstract()
    {

    }
protected:
    boost::asio::ip::tcp::resolver _resolver;
    size_t reqCount;
};

typedef std::unique_ptr<WebRequestAbstract> WebRequest;