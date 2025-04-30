 CREATE TABLE IF NOT EXISTS tickers (
    id VARCHAR PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    last_price DOUBLE PRECISION NOT NULL,
    volume DOUBLE PRECISION NOT NULL,
    high_price DOUBLE PRECISION NOT NULL,
    low_price DOUBLE PRECISION NOT NULL,
    symbol VARCHAR NOT NULL,
    exchange_name VARCHAR NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS tickers_exchange_symbol_timestamp_idx ON tickers(exchange_name, symbol, timestamp);
CREATE INDEX IF NOT EXISTS tickers_symbol_idx ON tickers(symbol);
CREATE INDEX IF NOT EXISTS tickers_exchange_idx ON tickers(exchange_name);
CREATE INDEX IF NOT EXISTS tickers_timestamp_idx ON tickers(timestamp);
