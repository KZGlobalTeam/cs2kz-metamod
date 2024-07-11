
// =====[ MAPS ]=====

constexpr char sqlite_maps_create[] = "\
CREATE TABLE IF NOT EXISTS Maps ( \
    MapID INTEGER NOT NULL, \
    Name VARCHAR(32) NOT NULL UNIQUE, \
    LastPlayed TIMESTAMP NULL DEFAULT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Maps PRIMARY KEY (MapID))";

constexpr char mysql_maps_create[] = "\
CREATE TABLE IF NOT EXISTS Maps ( \
    MapID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
    Name VARCHAR(32) NOT NULL UNIQUE, \
    LastPlayed TIMESTAMP NULL DEFAULT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Maps PRIMARY KEY (MapID))";

constexpr char sqlite_maps_insert[] = "\
INSERT OR IGNORE INTO Maps (Name, LastPlayed) \
    VALUES ('%s', CURRENT_TIMESTAMP)";

constexpr char sqlite_maps_update[] = "\
UPDATE OR IGNORE Maps \
    SET LastPlayed=CURRENT_TIMESTAMP \
    WHERE Name='%s'";

constexpr char mysql_maps_upsert[] = "\
INSERT INTO Maps (Name, LastPlayed) \
    VALUES ('%s', CURRENT_TIMESTAMP) \
    ON DUPLICATE KEY UPDATE \
    LastPlayed=CURRENT_TIMESTAMP";

constexpr char sql_maps_findid[] = "\
SELECT MapID, Name \
    FROM Maps \
    WHERE Name LIKE '%%%s%%' \
    ORDER BY (Name='%s') DESC, LENGTH(Name) \
    LIMIT 1";

constexpr char sql_maps_getname[] = "\
SELECT Name \
    FROM Maps \
    WHERE MapID=%d";

constexpr char sql_maps_searchbyname[] = "\
SELECT MapID, Name \
    FROM Maps \
    WHERE Name LIKE '%%%s%%' \
    ORDER BY (Name='%s') DESC, LENGTH(Name) \
    LIMIT 1";
