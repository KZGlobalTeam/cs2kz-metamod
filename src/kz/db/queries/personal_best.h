
constexpr char sql_getpb[] = R"(
    SELECT Times.RunTime, Times.Teleports 
        FROM Times
        INNER JOIN MapCourses ON Times.MapCourseID = MapCourses.ID
        INNER JOIN Maps ON MapCourses.MapID = Maps.ID
        WHERE Times.SteamID64=%llu 
        AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=%llu
        ORDER BY Times.RunTime 
        LIMIT %d
)";

constexpr char sql_getpbpro[] = R"(
    SELECT Times.RunTime 
        FROM Times
        INNER JOIN MapCourses ON MapCourses.ID = Times.MapCourseID
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        WHERE Times.SteamID64=%llu
        AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=%llu
        AND Times.Teleports=0 
        ORDER BY Times.RunTime 
        LIMIT %d
)";

// The following queries should have no style!

constexpr char sql_getmaprank[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.RunTime < 
            (SELECT MIN(Times.RunTime) 
            FROM Times 
            INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
            INNER JOIN Maps ON Maps.ID = MapCourses.MapID
            INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
            WHERE Players.Cheater=0 AND Times.SteamID64=%llu AND Maps.Name='%s'
            AND MapCourses.Name='%s' AND Times.ModeID=%d AND Times.StyleIDFlags=0) 
    + 1
)";

constexpr char sql_getmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.Teleports=0 
        AND Times.RunTime < 
            (SELECT MIN(Times.RunTime) 
            FROM Times 
            INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
            INNER JOIN Maps ON Maps.ID = MapCourses.MapID
            INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
            WHERE Players.Cheater=0 AND Times.SteamID64=%llu AND Maps.Name='%s' 
            AND MapCourses.Name='%s' AND Times.ModeID=%d 
            AND Times.StyleIDFlags=0 AND Times.Teleports=0) 
    + 1
)";

constexpr char sql_getlowestmaprank[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s' 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.StyleIDFlags=0
)";

constexpr char sql_getlowestmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s'
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.Teleports=0
)";
