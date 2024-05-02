
// =====[ MAPCOURSES ]=====

constexpr char sqlite_mapcourses_create[] = "\
CREATE TABLE IF NOT EXISTS MapCourses ( \
    MapCourseID INTEGER NOT NULL, \
    MapID INTEGER NOT NULL, \
    Course VARCHAR(32) NOT NULL, \
    Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_MapCourses PRIMARY KEY (MapCourseID), \
    CONSTRAINT UQ_MapCourses_MapIDCourse UNIQUE (MapID, Course), \
    CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(MapID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char mysql_mapcourses_create[] = "\
CREATE TABLE IF NOT EXISTS MapCourses ( \
    MapCourseID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
    MapID INTEGER UNSIGNED NOT NULL, \
    Course VARCHAR(32) UNSIGNED NOT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_MapCourses PRIMARY KEY (MapCourseID), \
    CONSTRAINT UQ_MapCourses_MapIDCourse UNIQUE (MapID, Course), \
    CONSTRAINT FK_MapCourses_MapID FOREIGN KEY (MapID) REFERENCES Maps(MapID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char sqlite_mapcourses_insert[] = "\
INSERT OR IGNORE INTO MapCourses (MapID, Course) \
    VALUES (%d, %s)";

constexpr char mysql_mapcourses_insert[] = "\
INSERT IGNORE INTO MapCourses (MapID, Course) \
    VALUES (%d, %s)";

constexpr char sql_mapcourses_findid[] = "\
SELECT MapCourseID \
    FROM MapCourses \
    WHERE MapID=%d AND Course=%s";
