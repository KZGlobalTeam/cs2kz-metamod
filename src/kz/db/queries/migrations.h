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

constexpr char sql_migrations_fetchall[] = R"(
    SELECT ID, CRC32 
        FROM Migrations
        ORDER BY ID
)";

constexpr char sql_migrations_insert[] = R"(
    INSERT INTO Migrations (CRC32, Created)
        VALUES ('%lu', CURRENT_TIMESTAMP)
)";
