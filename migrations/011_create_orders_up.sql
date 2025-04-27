CREATE TABLE orders (
    id UUID PRIMARY KEY,
    trade_id UUID,
    session_id UUID NOT NULL,
    exchange_id VARCHAR,
    vars JSON DEFAULT '{}'::json,
    symbol VARCHAR NOT NULL,
    exchange VARCHAR NOT NULL,
    order_side VARCHAR NOT NULL,
    order_type VARCHAR NOT NULL,
    reduce_only BOOLEAN NOT NULL,
    qty FLOAT NOT NULL,
    filled_qty FLOAT DEFAULT 0,
    price FLOAT,
    status VARCHAR DEFAULT 'ACTIVE',
    created_at BIGINT NOT NULL,
    executed_at BIGINT,
    canceled_at BIGINT,
    CONSTRAINT idx_trade_exchange_symbol_status_created_at 
        UNIQUE (trade_id, exchange, symbol, status, created_at)
);
CREATE INDEX idx_trade_id ON orders(trade_id);
CREATE INDEX idx_session_id ON orders(session_id);
