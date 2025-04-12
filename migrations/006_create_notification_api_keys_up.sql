CREATE TABLE notification_api_keys (
    id VARCHAR PRIMARY KEY,
    name VARCHAR NOT NULL UNIQUE,
    driver VARCHAR NOT NULL,
    fields TEXT NOT NULL,
    created_at BIGINT NOT NULL
);
CREATE INDEX idx_notification_api_keys_driver ON notification_api_keys(driver);
