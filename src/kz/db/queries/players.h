
// =====[ PLAYERS ]=====

constexpr char sqlite_players_create[] = "\
CREATE TABLE IF NOT EXISTS Players ( \
    SteamID32 INTEGER NOT NULL, \
    Alias TEXT, \
    Country TEXT, \
    IP TEXT, \
    Cheater INTEGER NOT NULL DEFAULT '0', \
    LastPlayed TIMESTAMP NULL DEFAULT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Player PRIMARY KEY (SteamID32))";

constexpr char mysql_players_create[] = "\
CREATE TABLE IF NOT EXISTS Players ( \
    SteamID32 INTEGER UNSIGNED NOT NULL, \
    Alias VARCHAR(32), \
    Country VARCHAR(45), \
    IP VARCHAR(15), \
    Cheater TINYINT UNSIGNED NOT NULL DEFAULT '0', \
    LastPlayed TIMESTAMP NULL DEFAULT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Player PRIMARY KEY (SteamID32))";

constexpr char sqlite_players_insert[] = "\
INSERT OR IGNORE INTO Players (Alias, Country, IP, SteamID32, LastPlayed) \
    VALUES ('%s', '%s', '%s', %d, CURRENT_TIMESTAMP)";

constexpr char sqlite_players_update[] = "\
UPDATE OR IGNORE Players \
    SET Alias='%s', Country='%s', IP='%s', LastPlayed=CURRENT_TIMESTAMP \
    WHERE SteamID32=%d";

constexpr char mysql_players_upsert[] = "\
INSERT INTO Players (Alias, Country, IP, SteamID32, LastPlayed) \
    VALUES ('%s', '%s', '%s', %d, CURRENT_TIMESTAMP) \
    ON DUPLICATE KEY UPDATE \
    SteamID32=VALUES(SteamID32), Alias=VALUES(Alias), Country=VALUES(Country), \
    IP=VALUES(IP), LastPlayed=VALUES(LastPlayed)";

constexpr char sql_players_get_cheater[] = "\
SELECT Cheater \
    FROM Players \
    WHERE SteamID32=%d";

constexpr char sql_players_set_cheater[] = "\
UPDATE Players \
    SET Cheater=%d \
    WHERE SteamID32=%d";

constexpr char sql_players_getalias[] = "\
SELECT Alias \
    FROM Players \
    WHERE SteamID32=%d";

constexpr char sql_players_searchbyalias[] = "\
SELECT SteamID32, Alias \
    FROM Players \
    WHERE Players.Cheater=0 AND LOWER(Alias) LIKE '%%%s%%' \
    ORDER BY (LOWER(Alias)='%s') DESC, LastPlayed DESC \
    LIMIT 1";
