CREATE TABLE candles (
    id VARCHAR PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    open DOUBLE PRECISION NOT NULL,
    close DOUBLE PRECISION NOT NULL,
    high DOUBLE PRECISION NOT NULL,
    low DOUBLE PRECISION NOT NULL,
    volume DOUBLE PRECISION NOT NULL,
    exchange_name VARCHAR NOT NULL,
    symbol VARCHAR NOT NULL,
    timeframe VARCHAR NOT NULL
);
CREATE UNIQUE INDEX idx_candles_exchange_symbol_timeframe_timestamp ON candles(exchange_name, symbol, timeframe, timestamp);
CREATE INDEX idx_candles_timestamp ON candles(timestamp);
