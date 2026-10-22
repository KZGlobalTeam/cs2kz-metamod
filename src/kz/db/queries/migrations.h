#pragma once
// =====[ MIGRATIONS ]=====

constexpr char mysql_migrations_create[] = R"(
    CREATE TABLE IF NOT EXISTS Migrations ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        CRC32 INTEGER UNSIGNED NOT NULL UNIQUE, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT PK_Migrations PRIMARY KEY (ID))
)";

constexpr char sqlite_migrations_create[] = R"(
    CREATE TABLE IF NOT EXISTS Migrations ( 
        ID INTEGER NOT NULL, 
        CRC32 INTEGER NOT NULL UNIQUE, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT PK_Migrations PRIMARY KEY (ID))
)";

// Advisory lock so servers sharing a database don't apply the same migrations concurrently.
// Held by the session, released explicitly or when the connection closes.
// The wait is kept short so it stays under the client read timeout; longer waits go through retries.
constexpr char mysql_migrations_lock[] = R"(
    SELECT GET_LOCK('cs2kz_migrations', 10)
)";

constexpr char mysql_migrations_unlock[] = R"(
    SELECT RELEASE_LOCK('cs2kz_migrations')
)";

constexpr char sql_migrations_fetchall[] = R"(
    SELECT ID, CRC32 
        FROM Migrations
        ORDER BY ID
)";

constexpr char sql_migrations_insert[] = R"(
    INSERT INTO Migrations (CRC32, Created)
        VALUES ('%u', CURRENT_TIMESTAMP)
)";
