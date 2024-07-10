
// =====[ MAPS ]=====

constexpr char sqlite_maps_create[] = R"(
    CREATE TABLE IF NOT EXISTS Maps ( 
        ID INTEGER NOT NULL, 
        Name VARCHAR(32) NOT NULL UNIQUE, 
        LastPlayed TIMESTAMP NULL DEFAULT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Maps PRIMARY KEY (ID))
)";

constexpr char mysql_maps_create[] = R"(
    CREATE TABLE IF NOT EXISTS Maps ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        Name VARCHAR(32) NOT NULL UNIQUE, 
        LastPlayed TIMESTAMP NULL DEFAULT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Maps PRIMARY KEY (ID))
)";

constexpr char sqlite_maps_insert[] = R"(
    INSERT OR IGNORE INTO Maps (Name, LastPlayed) 
        VALUES ('%s', CURRENT_TIMESTAMP)
)";

constexpr char sqlite_maps_update[] = R"(
    UPDATE OR IGNORE Maps 
        SET LastPlayed=CURRENT_TIMESTAMP 
        WHERE Name='%s'
)";

constexpr char mysql_maps_upsert[] = R"(
    INSERT INTO Maps (Name, LastPlayed) 
        VALUES ('%s', CURRENT_TIMESTAMP) 
        ON DUPLICATE KEY UPDATE 
        LastPlayed=CURRENT_TIMESTAMP
)";

constexpr char sql_maps_findid[] = R"(
    SELECT ID, Name 
        FROM Maps 
        WHERE Name LIKE '%%%s%%' 
        ORDER BY (Name='%s') DESC, LENGTH(Name) 
        LIMIT 1
)";

constexpr char sql_maps_getname[] = R"(
    SELECT Name 
        FROM Maps 
        WHERE ID=%d
)";

constexpr char sql_maps_searchbyname[] = R"(
    SELECT ID, Name 
        FROM Maps 
        WHERE Name LIKE '%%%s%%' 
        ORDER BY (Name='%s') DESC, LENGTH(Name) 
        LIMIT 1
)";
