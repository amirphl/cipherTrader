CREATE TABLE logs (
    id VARCHAR PRIMARY KEY,
    session_id VARCHAR NOT NULL,
    timestamp BIGINT NOT NULL,
    message TEXT NOT NULL,
    level SMALLINT NOT NULL
);
CREATE INDEX idx_logs_session_id ON logs(session_id);
CREATE INDEX idx_logs_type_timestamp ON logs(level, timestamp);
