#pragma once
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

constexpr char sql_times_update_id[] = R"(
    UPDATE Times SET ID='%s' WHERE ID='%s'
)";

constexpr char sql_times_any_for_player[] = R"(
    SELECT 1 FROM Times WHERE SteamID64=%llu LIMIT 1
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

// Speeds up the per-player lookups of the replay retention.
constexpr char mysql_times_add_player_index[] = R"(
    CREATE INDEX IX_Times_Player ON Times (SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime)
)";

constexpr char sqlite_times_add_player_index[] = R"(
    CREATE INDEX IF NOT EXISTS IX_Times_Player ON Times (SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime)
)";

// Returns each of these runs with Kept = 1 if it has no styles and is among its player's %d fastest on the course in that mode
// or among the %d fastest without teleports.
// Runs with the same time are ranked by ID which is the order they were set in.
constexpr char sql_times_findkeptruns[] = R"(
    SELECT t.ID, (t.StyleIDFlags = 0 AND (
        (SELECT COUNT(*) FROM Times f
            WHERE f.SteamID64 = t.SteamID64 AND f.MapCourseID = t.MapCourseID AND f.ModeID = t.ModeID AND f.StyleIDFlags = 0
                AND (f.RunTime < t.RunTime OR (f.RunTime = t.RunTime AND f.ID < t.ID))) < %d
        OR (t.Teleports = 0 AND (SELECT COUNT(*) FROM Times f
            WHERE f.SteamID64 = t.SteamID64 AND f.MapCourseID = t.MapCourseID AND f.ModeID = t.ModeID AND f.StyleIDFlags = 0
                AND f.Teleports = 0 AND (f.RunTime < t.RunTime OR (f.RunTime = t.RunTime AND f.ID < t.ID))) < %d))) AS Kept
        FROM Times t
        WHERE t.ID IN (%s)
)";
