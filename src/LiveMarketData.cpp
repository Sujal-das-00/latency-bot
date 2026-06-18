#include "latency_arb/LiveMarketData.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace latency_arb {

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

[[nodiscard]] std::uint64_t now_ms()
{
    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    return static_cast<std::uint64_t>(now.time_since_epoch().count());
}

[[nodiscard]] double json_double(const json& object, const char* key)
{
    if (!object.contains(key) || object[key].is_null()) {
        return 0.0;
    }
    if (object[key].is_number()) {
        return object[key].get<double>();
    }
    return std::stod(object[key].get<std::string>());
}

[[nodiscard]] double phemex_price(std::int64_t price_ep)
{
    return static_cast<double>(price_ep) / 100000000.0;
}

[[nodiscard]] double phemex_qty(std::int64_t qty)
{
    return static_cast<double>(qty) / 100000000.0;
}

class PhemexTopBook {
public:
    void apply(const json& message)
    {
        if (!message.contains("book")) {
            return;
        }

        const auto type = message.value("type", "");
        if (type == "snapshot") {
            bids_.clear();
            asks_.clear();
        }

        const auto& book = message["book"];
        apply_side(book.value("bids", json::array()), bids_);
        apply_side(book.value("asks", json::array()), asks_);
    }

    [[nodiscard]] std::optional<Book> snapshot() const
    {
        if (bids_.empty() || asks_.empty()) {
            return std::nullopt;
        }

        const auto best_bid = bids_.rbegin();
        const auto best_ask = asks_.begin();
        return Book{
            .bid = best_bid->first,
            .ask = best_ask->first,
            .bid_qty = best_bid->second,
            .ask_qty = best_ask->second,
            .local_ts_ms = now_ms()};
    }

private:
    static void apply_side(const json& levels, std::map<double, double>& side)
    {
        for (const auto& level : levels) {
            if (!level.is_array() || level.size() < 2) {
                continue;
            }
            const auto price = phemex_price(level[0].get<std::int64_t>());
            const auto qty = phemex_qty(level[1].get<std::int64_t>());
            if (qty <= 0.0) {
                side.erase(price);
            } else {
                side[price] = qty;
            }
        }
    }

    std::map<double, double> bids_;
    std::map<double, double> asks_;
};

void run_tls_websocket(
    std::stop_token stop_token,
    std::string host,
    std::string port,
    std::string target,
    std::string subscribe_payload,
    const std::function<void(const std::string&)>& on_message)
{
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{ioc};
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws{ioc, ctx};

    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()));
    }

    const auto results = resolver.resolve(host, port);
    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(ws).connect(results);
    ws.next_layer().handshake(ssl::stream_base::client);
    beast::get_lowest_layer(ws).expires_never();
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& request) { request.set(boost::beast::http::field::user_agent, "latency-arb-paper-validator"); }));
    ws.handshake(host, target);
    ws.write(asio::buffer(subscribe_payload));

    // Drive reads asynchronously so we can honour stop_token without blocking
    // forever inside a synchronous read when no messages are arriving.
    beast::flat_buffer buffer;
    bool reading = false;
    bool completed = false;
    beast::error_code read_ec;

    while (!stop_token.stop_requested()) {
        if (!reading) {
            reading = true;
            completed = false;
            buffer.clear();
            ws.async_read(buffer, [&](beast::error_code ec, std::size_t) {
                read_ec = ec;
                completed = true;
                reading = false;
            });
        }

        if (ioc.stopped()) {
            ioc.restart();
        }
        ioc.run_for(std::chrono::milliseconds(200));

        if (completed) {
            if (read_ec) {
                return;
            }
            on_message(beast::buffers_to_string(buffer.data()));
        }
    }

    // Graceful shutdown: cancel any in-flight async read before the
    // synchronous close so the two operations never run concurrently.
    beast::error_code ignored;
    beast::get_lowest_layer(ws).cancel();
    if (ioc.stopped()) {
        ioc.restart();
    }
    ioc.run_for(std::chrono::milliseconds(200));
    ws.close(websocket::close_code::normal, ignored);
}

void run_coinbase(std::stop_token stop_token, const LiveRunConfig& config, Backtester& engine)
{
    const auto subscribe = fmt::format(
        R"({{"type":"subscribe","product_ids":["{}"],"channels":["ticker"]}})",
        config.coinbase_product);

    run_tls_websocket(
        stop_token,
        "ws-feed.exchange.coinbase.com",
        "443",
        "/",
        subscribe,
        [&](const std::string& payload) {
            const auto message = json::parse(payload, nullptr, false);
            if (message.is_discarded() || message.value("type", "") != "ticker") {
                return;
            }

            const Book book{
                .bid = json_double(message, "best_bid"),
                .ask = json_double(message, "best_ask"),
                .bid_qty = json_double(message, "best_bid_size"),
                .ask_qty = json_double(message, "best_ask_size"),
                .local_ts_ms = now_ms()};
            engine.on_book(Exchange::Reference, book);
        });
}

void run_phemex(std::stop_token stop_token, const LiveRunConfig& config, Backtester& engine)
{
    const auto subscribe = fmt::format(
        R"({{"id":1,"method":"orderbook.subscribe","params":["{}"]}})",
        config.phemex_symbol);
    PhemexTopBook top_book;

    run_tls_websocket(
        stop_token,
        "ws.phemex.com",
        "443",
        "/",
        subscribe,
        [&](const std::string& payload) {
            const auto message = json::parse(payload, nullptr, false);
            if (message.is_discarded() || !message.contains("book")) {
                return;
            }

            top_book.apply(message);
            if (auto book = top_book.snapshot()) {
                engine.on_book(Exchange::Target, *book);
            }
        });
}

} // namespace

LiveMarketDataRunner::LiveMarketDataRunner(LiveRunConfig config)
    : config_(std::move(config))
{
}

BacktestResult LiveMarketDataRunner::run()
{
    Backtester engine(config_.engine);

    std::jthread coinbase_thread([&](std::stop_token stop_token) {
        try {
            run_coinbase(stop_token, config_, engine);
        } catch (const std::exception& ex) {
            spdlog::error("coinbase public websocket stopped: {}", ex.what());
        }
    });

    std::jthread phemex_thread([&](std::stop_token stop_token) {
        try {
            run_phemex(stop_token, config_, engine);
        } catch (const std::exception& ex) {
            spdlog::error("phemex public websocket stopped: {}", ex.what());
        }
    });

    spdlog::info(
        "live paper observation started coinbase_product={} phemex_symbol={} duration_seconds={}",
        config_.coinbase_product,
        config_.phemex_symbol,
        config_.duration_seconds);

    std::this_thread::sleep_for(std::chrono::seconds(config_.duration_seconds));
    coinbase_thread.request_stop();
    phemex_thread.request_stop();

    return engine.finalize();
}

} // namespace latency_arb
