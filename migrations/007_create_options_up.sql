CREATE TABLE options (
    id VARCHAR PRIMARY KEY,
    updated_at BIGINT NOT NULL,
    option_type VARCHAR NOT NULL,
    value TEXT NOT NULL
);
CREATE INDEX idx_options_type ON options(option_type);
