// =====[ TIMES ]=====

constexpr char sqlite_times_create[] = "\
CREATE TABLE IF NOT EXISTS Times ( \
    TimeID INTEGER NOT NULL, \
    SteamID32 INTEGER NOT NULL, \
    MapCourseID INTEGER NOT NULL, \
    ModeID INTEGER NOT NULL, \
    StyleIDFlags INTEGER NOT NULL, \
    RunTime INTEGER NOT NULL, \
    Teleports INTEGER NOT NULL, \
    Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Times PRIMARY KEY (TimeID), \
    CONSTRAINT FK_Times_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32) \
    ON UPDATE CASCADE ON DELETE CASCADE, \
	CONSTRAINT FK_Times_MapCourseID \
    FOREIGN KEY (MapCourseID) REFERENCES MapCourses(MapCourseID) \
    ON UPDATE CASCADE ON DELETE CASCADE, \
	CONSTRAINT FK_Times_Mode FOREIGN KEY (ModeID) REFERENCES MapCourses(ModeID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char mysql_times_create[] = "\
CREATE TABLE IF NOT EXISTS Times ( \
    TimeID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
    SteamID32 INTEGER UNSIGNED NOT NULL, \
    MapCourseID INTEGER UNSIGNED NOT NULL, \
    ModeID INTEGER UNSIGNED NOT NULL, \
    StyleIDFlags INTEGER UNSIGNED NOT NULL, \
    RunTime INTEGER UNSIGNED NOT NULL, \
    Teleports SMALLINT UNSIGNED NOT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Times PRIMARY KEY (TimeID), \
    CONSTRAINT FK_Times_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32) \
    ON UPDATE CASCADE ON DELETE CASCADE, \
    CONSTRAINT FK_Times_MapCourseID FOREIGN KEY (MapCourseID) REFERENCES MapCourses(MapCourseID) \
    ON UPDATE CASCADE ON DELETE CASCADE, \
	CONSTRAINT FK_Times_Mode FOREIGN KEY (ModeID) REFERENCES MapCourses(ModeID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char sql_times_insert[] = "\
INSERT INTO Times (SteamID32, MapCourseID, Mode, Style, RunTime, Teleports) \
    SELECT %d, MapCourseID, %d, %d, %d, %d \
    FROM MapCourses \
    WHERE MapID=%d AND Course=%d";

constexpr char sql_times_delete[] = "\
DELETE FROM Times \
    WHERE TimeID=%d";
