
// =====[ MAPCOURSES ]=====

constexpr char sqlite_mapcourses_create[] = R"(
    CREATE TABLE IF NOT EXISTS MapCourses ( 
        ID INTEGER NOT NULL, 
        MapID INTEGER NOT NULL, 
        Name VARCHAR(64) NOT NULL, 
        StageID INTEGER NOT NULL,
        Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_MapCourses PRIMARY KEY (ID), 
        CONSTRAINT UQ_MapCourses_MapIDName UNIQUE (MapID, Name), 
        CONSTRAINT UQ_MapCourses_MapIDStageID UNIQUE (MapID, StageID),
        CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char mysql_mapcourses_create[] = R"(
    CREATE TABLE IF NOT EXISTS MapCourses ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        MapID INTEGER UNSIGNED NOT NULL, 
        Name VARCHAR(64) NOT NULL, 
        StageID INTEGER NOT NULL,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_MapCourses PRIMARY KEY (ID), 
        CONSTRAINT UQ_MapCourses_MapIDName UNIQUE (MapID, Name), 
        CONSTRAINT UQ_MapCourses_MapIDStageID UNIQUE (MapID, StageID),
        CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sqlite_mapcourses_insert[] = R"(
    INSERT OR IGNORE INTO MapCourses (MapID, Name, StageID) 
        VALUES (%d, '%s', %d)
)";

constexpr char mysql_mapcourses_insert[] = R"(
    INSERT IGNORE INTO MapCourses (MapID, Name, StageID) 
        VALUES (%d, '%s', %d)
)";

constexpr char sql_mapcourses_findfirst_mapname[] = R"(
    SELECT MapCourses.Name
        FROM MapCourses
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        WHERE Maps.Name LIKE '%%%s%%'
        ORDER BY (Maps.Name='%s') DESC, StageID ASC
        LIMIT 1
)";

constexpr char sql_mapcourses_findname[] = R"(
    SELECT ID 
        FROM MapCourses 
        WHERE MapID=%d AND Name='%s';
)";

constexpr char sql_mapcourses_findstageid[] = R"(
    SELECT ID 
        FROM MapCourses 
        WHERE MapID=%d AND StageID=%d;
)";

constexpr char sql_mapcourses_findfirst[] = R"(
    SELECT ID
        FROM MapCourses
        WHERE MapID=%d
        ORDER BY StageID
        LIMIT 1
)";

constexpr char sql_mapcourses_findall[] = R"(
    SELECT Name, ID 
        FROM MapCourses 
        WHERE MapID=%d;
)";
