
// =====[ MAPCOURSES ]=====

constexpr char sqlite_mapcourses_create[] = R"(
    CREATE TABLE IF NOT EXISTS MapCourses ( 
        ID INTEGER NOT NULL, 
        MapID INTEGER NOT NULL, 
        Name VARCHAR(32) NOT NULL, 
        Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_MapCourses PRIMARY KEY (ID), 
        CONSTRAINT UQ_MapCourses_MapIDCourse UNIQUE (MapID, Name), 
        CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char mysql_mapcourses_create[] = R"(
    CREATE TABLE IF NOT EXISTS MapCourses ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        MapID INTEGER UNSIGNED NOT NULL, 
        Name VARCHAR(32) NOT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_MapCourses PRIMARY KEY (ID), 
        CONSTRAINT UQ_MapCourses_MapIDCourse UNIQUE (MapID, Name), 
        CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sqlite_mapcourses_insert[] = R"(
    INSERT OR IGNORE INTO MapCourses (MapID, Name) 
        VALUES (%d, '%s')
)";

constexpr char mysql_mapcourses_insert[] = R"(
    INSERT IGNORE INTO MapCourses (MapID, Name) 
        VALUES (%d, '%s')
)";

constexpr char sql_mapcourses_findid[] = R"(
    SELECT ID 
        FROM MapCourses 
        WHERE MapID=%d AND Name='%s'";
)";
