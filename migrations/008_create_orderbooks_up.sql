CREATE TABLE IF NOT EXISTS orderbooks (
    id VARCHAR(36) PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    symbol VARCHAR(255) NOT NULL,
    exchange VARCHAR(255) NOT NULL,
    data BYTEA NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS orderbooks_exchange_symbol_timestamp_idx ON orderbooks(exchange, symbol, timestamp);
CREATE INDEX IF NOT EXISTS orderbooks_symbol_idx ON orderbooks(symbol);
CREATE INDEX IF NOT EXISTS orderbooks_timestamp_idx ON orderbooks(timestamp);
