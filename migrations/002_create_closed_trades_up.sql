CREATE TABLE closed_trades (
    id VARCHAR PRIMARY KEY,
    strategy_name VARCHAR NOT NULL,
    symbol VARCHAR NOT NULL,
    exchange_name VARCHAR NOT NULL,
    position_type VARCHAR NOT NULL,
    timeframe VARCHAR NOT NULL,
    opened_at BIGINT NOT NULL,
    closed_at BIGINT NOT NULL,
    leverage INTEGER NOT NULL
);
CREATE INDEX idx_closed_trades_filter ON closed_trades (strategy_name, exchange_name, symbol);
