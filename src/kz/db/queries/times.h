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
    INSERT INTO Times (SteamID64, MapCourseID, ModeID, StyleIDFlags, RunTime, Teleports) 
        SELECT %llu, ID, %d, %llu, %.7f, %llu 
        FROM MapCourses 
        WHERE MapID=%d AND Name='%s'
)";

constexpr char sql_times_delete[] = R"(
    DELETE FROM Times 
        WHERE ID=%d
)";
