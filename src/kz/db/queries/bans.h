
// =====[ BANS ]=====

constexpr char sqlite_bans_create[] = R"(
    CREATE TABLE IF NOT EXISTS Bans ( 
        ID TEXT NOT NULL, 
        SteamID64 INTEGER NOT NULL, 
        Reason TEXT,
        ReplayUUID TEXT NULL DEFAULT NULL,
        ExpiresAt TIMESTAMP NULL DEFAULT NULL,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Ban PRIMARY KEY (ID),
        CONSTRAINT FK_Ban_Player FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) ON DELETE CASCADE)
)";

constexpr char mysql_bans_create[] = R"(
    CREATE TABLE IF NOT EXISTS Bans ( 
        ID CHAR(36) NOT NULL, 
        SteamID64 BIGINT UNSIGNED NOT NULL, 
        Reason TEXT,
        ReplayUUID CHAR(36) NULL DEFAULT NULL,
        ExpiresAt TIMESTAMP NULL DEFAULT NULL,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Ban PRIMARY KEY (ID),
        CONSTRAINT FK_Ban_Player FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) ON DELETE CASCADE,
        INDEX IDX_Ban_SteamID (SteamID64),
        INDEX IDX_Ban_Expires (ExpiresAt))
)";

constexpr char mysql_bans_insert[] = R"(
    INSERT INTO Bans (ID, SteamID64, Reason, ReplayUUID, ExpiresAt) 
        VALUES ('%s', %llu, '%s', '%s', %s)
        ON DUPLICATE KEY UPDATE 
            Reason = VALUES(Reason),
            ReplayUUID = VALUES(ReplayUUID),
            ExpiresAt = VALUES(ExpiresAt)
)";

constexpr char sqlite_bans_insert[] = R"(
    INSERT INTO Bans (ID, SteamID64, Reason, ReplayUUID, ExpiresAt) 
        VALUES ('%s', %llu, '%s', '%s', %s)
        ON CONFLICT(ID) DO UPDATE SET
            Reason = excluded.Reason,
            ReplayUUID = excluded.ReplayUUID,
            ExpiresAt = excluded.ExpiresAt
)";

constexpr char sql_bans_get_active[] = R"(
    SELECT ID, Reason, ExpiresAt, Created
        FROM Bans 
        WHERE SteamID64=%llu 
        AND (ExpiresAt IS NULL OR ExpiresAt > CURRENT_TIMESTAMP)
        ORDER BY Created DESC 
        LIMIT 1
)";

constexpr char sql_bans_remove_active[] = R"(
    UPDATE Bans 
        SET ExpiresAt = CURRENT_TIMESTAMP
        WHERE SteamID64=%llu 
        AND (ExpiresAt IS NULL OR ExpiresAt > CURRENT_TIMESTAMP)
)";

constexpr char sql_bans_remove_by_id[] = R"(
    DELETE FROM Bans 
        WHERE ID='%s'
)";
