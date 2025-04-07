CREATE TABLE exchange_api_keys (
    id VARCHAR PRIMARY KEY,
    exchange_name VARCHAR NOT NULL,
    name VARCHAR NOT NULL,
    api_key VARCHAR NOT NULL,
    api_secret VARCHAR NOT NULL,
    additional_fields VARCHAR NOT NULL DEFAULT '{}',
    created_at BIGINT NOT NULL
);
CREATE UNIQUE INDEX idx_exchange_api_keys_name ON exchange_api_keys(name);
CREATE INDEX idx_exchange_api_keys_exchange_name ON exchange_api_keys(exchange_name);
