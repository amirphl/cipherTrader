CREATE TABLE IF NOT EXISTS trades (
    id VARCHAR PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    price DOUBLE PRECISION NOT NULL,
    buy_qty DOUBLE PRECISION NOT NULL,
    sell_qty DOUBLE PRECISION NOT NULL,
    buy_count INTEGER NOT NULL,
    sell_count INTEGER NOT NULL,
    symbol VARCHAR NOT NULL,
    exchange_name VARCHAR NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS trades_exchange_symbol_timestamp_idx ON trades(exchange_name, symbol, timestamp);
CREATE INDEX IF NOT EXISTS trades_symbol_idx ON trades(symbol);
CREATE INDEX IF NOT EXISTS trades_exchange_idx ON trades(exchange_name);
CREATE INDEX IF NOT EXISTS trades_timestamp_idx ON trades(timestamp); 
