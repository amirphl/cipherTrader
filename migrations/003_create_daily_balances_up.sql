CREATE TABLE daily_balances (
    id VARCHAR PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    identifier VARCHAR NULL,
    exchange_name VARCHAR NOT NULL,
    asset VARCHAR NOT NULL,
    balance DOUBLE PRECISION NOT NULL
);
CREATE UNIQUE INDEX idx_daily_balances_unique ON daily_balances (identifier, exchange_name, asset, timestamp);
CREATE INDEX idx_daily_balances_lookup ON daily_balances (identifier, exchange_name);
