
// =====[ PLAYERS ]=====

constexpr char sqlite_players_create[] = R"(
    CREATE TABLE IF NOT EXISTS Players ( 
        SteamID64 INTEGER NOT NULL, 
        Alias TEXT, 
        IP TEXT, 
        Preferences TEXT,
        Cheater INTEGER NOT NULL DEFAULT '0',
        LastPlayed TIMESTAMP NULL DEFAULT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Player PRIMARY KEY (SteamID64))
)";

constexpr char mysql_players_create[] = R"(
    CREATE TABLE IF NOT EXISTS Players ( 
        SteamID64 BIGINT UNSIGNED NOT NULL, 
        Alias VARCHAR(32), 
        IP VARCHAR(15), 
        Preferences TEXT,
        Cheater TINYINT UNSIGNED NOT NULL DEFAULT '0', 
        LastPlayed TIMESTAMP NULL DEFAULT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Player PRIMARY KEY (SteamID64))
)";

constexpr char sqlite_players_insert[] = R"(
    INSERT OR IGNORE INTO Players (Alias, IP, SteamID64, LastPlayed) 
        VALUES ('%s', '%s', %lld, CURRENT_TIMESTAMP)
)";

constexpr char sqlite_players_update[] = R"(
    UPDATE OR IGNORE Players 
        SET Alias='%s', IP='%s', LastPlayed=CURRENT_TIMESTAMP 
        WHERE SteamID64=%lld
)";

constexpr char mysql_players_upsert[] = R"(
    INSERT INTO Players (Alias, IP, SteamID64, LastPlayed) 
        VALUES ('%s', '%s', %lld, CURRENT_TIMESTAMP) 
        ON DUPLICATE KEY UPDATE 
        SteamID64=VALUES(SteamID64), Alias=VALUES(Alias), 
        IP=VALUES(IP), LastPlayed=VALUES(LastPlayed)
)";

constexpr char sql_players_get_infos[] = R"(
    SELECT Cheater, Preferences
        FROM Players 
        WHERE SteamID64=%lld
)";

constexpr char sql_players_set_prefs[] = R"(
    UPDATE Players 
        SET Preferences='%s'
        WHERE SteamID64=%lld
)";

constexpr char sql_players_set_cheater[] = R"(
    UPDATE Players 
        SET Cheater=%lld 
        WHERE SteamID64=%lld
)";

constexpr char sql_players_getalias[] = R"(
    SELECT Alias 
        FROM Players 
        WHERE SteamID64=%lld
)";

constexpr char sql_players_searchbyalias[] = R"(
    SELECT SteamID64, Alias 
        FROM Players 
        WHERE LOWER(Alias) LIKE '%%%s%%' 
        ORDER BY (Players.Cheater=0) DESC, (LOWER(Alias)='%s') DESC, LastPlayed DESC 
        LIMIT 1
)";
