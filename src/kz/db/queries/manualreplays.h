#pragma once
// =====[ MANUAL REPLAYS ]=====

constexpr char sqlite_manualreplays_create[] = R"(
    CREATE TABLE IF NOT EXISTS ManualReplays (
        ID TEXT NOT NULL,
        SteamID64 INTEGER NOT NULL,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT PK_ManualReplays PRIMARY KEY (ID))
)";

constexpr char sqlite_manualreplays_add_player_index[] = R"(
    CREATE INDEX IF NOT EXISTS IX_ManualReplays_Player ON ManualReplays (SteamID64)
)";

constexpr char mysql_manualreplays_create[] = R"(
    CREATE TABLE IF NOT EXISTS ManualReplays (
        ID CHAR(36) NOT NULL,
        SteamID64 BIGINT UNSIGNED NOT NULL,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT PK_ManualReplays PRIMARY KEY (ID),
        INDEX IX_ManualReplays_Player (SteamID64))
)";

constexpr char sql_manualreplays_insert[] = R"(
    INSERT INTO ManualReplays (ID, SteamID64) VALUES ('%s', %llu)
)";

// Keeps a player's newest manual saves.
// The derived table lets MySQL read the table it deletes from.
constexpr char sql_manualreplays_prune[] = R"(
    DELETE FROM ManualReplays
        WHERE SteamID64=%llu
            AND ID NOT IN (
                SELECT ID FROM (
                    SELECT ID FROM ManualReplays
                        WHERE SteamID64=%llu
                        ORDER BY Created DESC, ID DESC
                        LIMIT %d
                ) newest
            )
)";

// The manual saves among these that their player still keeps.
constexpr char sql_manualreplays_finduuids[] = R"(
    SELECT ID FROM ManualReplays WHERE ID IN (%s)
)";
