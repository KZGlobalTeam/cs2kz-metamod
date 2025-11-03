// =====[ TIMES ]=====

constexpr char sqlite_times_create[] = R"(
    CREATE TABLE IF NOT EXISTS Times ( 
        ID INTEGER NOT NULL, 
        SteamID64 INTEGER NOT NULL, 
        MapCourseID INTEGER NOT NULL, 
        ModeID INTEGER NOT NULL, 
        StyleIDFlags INTEGER NOT NULL, 
        RunTime REAL NOT NULL, 
        Teleports INTEGER NOT NULL, 
        Metadata TEXT,
        Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Times PRIMARY KEY (ID), 
        CONSTRAINT FK_Times_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_MapCourseID 
        FOREIGN KEY (MapCourseID) REFERENCES MapCourses(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_Mode FOREIGN KEY (ModeID) REFERENCES Modes(ID)  
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char mysql_times_create[] = R"(
    CREATE TABLE IF NOT EXISTS Times ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        SteamID64 BIGINT UNSIGNED NOT NULL, 
        MapCourseID INTEGER UNSIGNED NOT NULL, 
        ModeID INTEGER UNSIGNED NOT NULL, 
        StyleIDFlags INTEGER UNSIGNED NOT NULL, 
        RunTime DOUBLE UNSIGNED NOT NULL, 
        Teleports SMALLINT UNSIGNED NOT NULL, 
        Metadata TEXT,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Times PRIMARY KEY (ID), 
        CONSTRAINT FK_Times_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_MapCourseID FOREIGN KEY (MapCourseID) REFERENCES MapCourses(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_Mode FOREIGN KEY (ModeID) REFERENCES Modes(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sql_times_insert[] = R"(
    INSERT INTO Times (ID, SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime, Teleports, Metadata) 
        VALUES ('%s', %llu, %d, %d, %llu, %.7f, %llu, '%s')
)";

constexpr char sql_times_delete[] = R"(
    DELETE FROM Times 
        WHERE ID='%s'
)";

// Migration queries to convert Times.ID to use UUID v7 strings
constexpr char mysql_times_alter_id_column[] = R"(
    ALTER TABLE Times MODIFY COLUMN ID VARCHAR(36) NOT NULL
)";

constexpr char sqlite_times_alter_id_column_1[] = R"(
    CREATE TABLE Times_New ( 
        ID TEXT NOT NULL, 
        SteamID64 INTEGER NOT NULL, 
        MapCourseID INTEGER NOT NULL, 
        ModeID INTEGER NOT NULL, 
        StyleIDFlags INTEGER NOT NULL, 
        RunTime REAL NOT NULL, 
        Teleports INTEGER NOT NULL, 
        Metadata TEXT,
        Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Times_New PRIMARY KEY (ID), 
        CONSTRAINT FK_Times_New_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_New_MapCourseID 
        FOREIGN KEY (MapCourseID) REFERENCES MapCourses(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE, 
        CONSTRAINT FK_Times_New_Mode FOREIGN KEY (ModeID) REFERENCES Modes(ID)  
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sqlite_times_alter_id_column_2[] = R"(
    INSERT INTO Times_New (ID, SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime, Teleports, Metadata, Created)
    SELECT CAST(ID as TEXT), SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime, Teleports, Metadata, Created FROM Times
)";

constexpr char sqlite_times_alter_id_column_3[] = R"(
    DROP TABLE Times
)";

constexpr char sqlite_times_alter_id_column_4[] = R"(
    ALTER TABLE Times_New RENAME TO Times
)";
