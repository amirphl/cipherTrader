CREATE TABLE options (
    id VARCHAR PRIMARY KEY,
    updated_at BIGINT NOT NULL,
    type VARCHAR NOT NULL,
    json TEXT NOT NULL
);
CREATE INDEX idx_options_type ON options(type);
