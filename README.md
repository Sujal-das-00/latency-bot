# Spot Lead-Lag Paper Validation Framework

This project validates whether Coinbase public market movements statistically predict profitable repricing on Phemex Spot.

It is not cross-exchange arbitrage. Coinbase is only a signal source. Phemex Spot is the only hypothetical paper trade venue.

There are no API keys, no authenticated websocket sessions, no REST trading endpoints, no order submission code, no hedging, no shorts, no perpetuals, no transfers, and no inventory balancing.

## What It Does

1. Reads Coinbase and Phemex Spot Level 1 books.
2. Detects when Coinbase has moved while Phemex remains stale inside a configurable lag window.
3. Creates a `PAPER_ONLY` trade at the observed Phemex ask for `BUY`, or Phemex bid for `SELL`.
4. Continues observing future Phemex Spot prices at 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2s, 5s, 10s, and 30s.
5. Calculates entry similarity, latency slippage, favorable/adverse excursion, best exit, markout PnL, expected value, win rate, drawdown, Sharpe, and best hold time.
6. Writes validation logs to CSV.

`SELL` is modeled as a paper markout for selling existing spot exposure on Phemex. It is not a short sale and does not create margin, derivatives, or borrow logic.

## Build

Install `fmt`, `spdlog`, `OpenSSL`, and `Boost` (the last two are needed for the live runner), then configure and build:

```bash
cmake -S . -B build
cmake --build build
```

If `fmt`/`spdlog` are not installed and network access is available, let CMake fetch them:

```bash
cmake -S . -B build -DLATENCY_ARB_FETCH_DEPS=ON
cmake --build build
```

This produces three binaries in `build/`:

- `latency_arb_backtest` — CSV replay backtester
- `latency_arb_live` — public-websocket paper-only live validator
- `latency_arb_tests` — unit tests

## Test

Run the full test suite after building:

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:

```bash
./build/latency_arb_tests
```

## Replay Backtest

Input columns:

```csv
exchange,bid,ask,bid_qty,ask_qty,local_ts_ms
Phemex,99.80,100.00,10,10,1000
Coinbase,100.20,100.30,10,10,1100
```

Run:

```bash
./build/latency_arb_backtest \
  --input=examples/sample_books.csv \
  --output=out \
  --min-spread-bps=8 \
  --min-lag-ms=50 \
  --max-lag-ms=500 \
  --assumed-latency-ms=25 \
  --position-usdt=100 \
  --fee-rate=0.0005
```

Outputs:

- `out/paper_validation.csv`
- `out/statistics.csv`

## Live Market Plan

Realtime observation should use public unauthenticated websockets:

- Coinbase public market data websocket: `wss://ws-feed.exchange.coinbase.com`
- Coinbase channel: `ticker`, using `best_bid`, `best_bid_size`, `best_ask`, `best_ask_size`
- Phemex Spot public websocket: `orderbook.subscribe` for spot symbols such as `sSOLUSDT`
- Phemex book data: top ask/bid from the public orderbook snapshot/incremental messages

REST may be used later for optional cold-start snapshots or product discovery, but not for realtime signal generation.

The live runner should feed the same engine:

```cpp
backtester.on_book(Exchange::Reference, coinbase_book);
backtester.on_book(Exchange::Target, phemex_spot_book);
```

When validation is statistically acceptable, execution should be added behind a separate Phemex Spot execution interface. That interface is intentionally absent from this project right now.

## Run Live (Paper Only)

```bash
./build/latency_arb_live --output=out-live --min-spread-bps=10 --duration-seconds=120
```

Signals are streamed to the terminal and persisted to a date-named log file:

```bash
cat out-live/logs/signals_$(date +%F).log
```

The CSV summaries (`out-live/paper_validation.csv`, `out-live/statistics.csv`) are written when the run finishes (`--duration-seconds` elapses).

## Run 24x7 with pm2

```bash
cd "/home/suajldas/Desktop/experiments/latency bot"
pm2 start ./build/latency_arb_live \
  --name latency-bot \
  --interpreter none \
  -- \
  --output=out-live --min-spread-bps=10 --duration-seconds=86400
```

`pm2` keeps the bot running 24x7 via `autorestart`; `--duration-seconds=86400` makes it finalize a CSV summary and roll over once per day. Persist across reboots with `pm2 save && pm2 startup`.