
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

constexpr char sql_getmaptop[] = R"(
    SELECT t.TimeID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 
        AND t2.MapCourseID=t.MapCourseID AND t2.ModeID=t.ModeID AND t2.RunTime<t.RunTime 
        WHERE t2.TimeID IS NULL AND p.Cheater=0 AND Maps.Name='%s' AND mc.Name='%s' AND t.ModeID=%d 
        ORDER BY PBTime 
        LIMIT %d
)";

constexpr char sql_getmaptoppro[] = R"(
    SELECT t.TimeID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID 
        AND t2.ModeID=t.ModeID AND t2.RunTime<t.RunTime AND t.Teleports=0 AND t2.Teleports=0 
        WHERE t2.TimeID IS NULL AND p.Cheater=0 AND Maps.Name='%s'
        AND mc.Name='%s' AND t.ModeID=%d AND t.Teleports=0 
        ORDER BY PBTime 
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
