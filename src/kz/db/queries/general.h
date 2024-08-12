// =====[ GENERAL ]=====

constexpr char sql_getwrs[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s'
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getwrspro[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Maps.Name='%s' AND Times.Teleports=0 
        GROUP BY MapCourses.Name, Times.ModeID
)";

// Caching PBs

constexpr char sql_getpbs[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        WHERE Times.SteamID64=%llu AND Maps.Name='%s'
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getpbspro[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        WHERE Times.SteamID64=%llu AND Maps.Name='%s' AND Times.Teleports=0 
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getcount_courses[] = R"(
    SELECT COUNT(*) 
        FROM MapCourses 
        INNER JOIN Maps ON Maps.MapID=MapCourses.MapID
)";

constexpr char sql_getcount_coursescompleted[] = R"(
    SELECT COUNT(DISTINCT Times.MapCourseID) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.MapID=MapCourses.MapID 
        AND Times.SteamID64=%llu AND Times.ModeID=%d AND Times.StyleIDFlags=0
)";

constexpr char sql_getcount_coursescompletedpro[] = R"(
    SELECT COUNT(DISTINCT Times.MapCourseID) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.MapID=MapCourses.MapID 
        AND Times.SteamID64=%llu AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.Teleports=0
)";

constexpr char sql_gettopplayers[] = R"(
    SELECT Players.SteamID64, Players.Alias, COUNT(*) AS RecordCount 
        FROM Times 
        INNER JOIN 
        (SELECT Times.MapCourseID, Times.ModeID, MIN(Times.RunTime) AS RecordTime 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.MapID=MapCourses.MapID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 
        GROUP BY Times.MapCourseID) Records 
        ON Times.MapCourseID=Records.MapCourseID AND Times.ModeID=Records.ModeID AND Times.RunTime=Records.RecordTime 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        GROUP BY Players.SteamID64, Players.Alias 
        ORDER BY RecordCount DESC 
        LIMIT %d
)";

constexpr char sql_gettopplayerspro[] = R"(
    SELECT Players.SteamID64, Players.Alias, COUNT(*) AS RecordCount 
        FROM Times 
        INNER JOIN 
        (SELECT Times.MapCourseID, Times.ModeID, MIN(Times.RunTime) AS RecordTime 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.MapID=MapCourses.MapID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND 
        AND Times.ModeID=%d AND Times.Teleports=0 
        GROUP BY Times.MapCourseID) Records 
        ON Times.MapCourseID=Records.MapCourseID AND Times.ModeID=Records.ModeID AND Times.RunTime=Records.RecordTime AND Times.Teleports=0 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        GROUP BY Players.SteamID64, Players.Alias 
        ORDER BY RecordCount DESC 
        LIMIT %d
)";

constexpr char sql_getaverage[] = R"(
    SELECT AVG(PBTime), COUNT(*) 
        FROM 
        (SELECT MIN(Times.RunTime) AS PBTime 
        FROM Times 
        INNER JOIN MapCourses ON Times.MapCourseID=MapCourses.ID 
        INNER JOIN Players ON Times.SteamID64=Players.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        GROUP BY Times.SteamID64) AS PBTimes
)";

constexpr char sql_getaverage_pro[] = R"(
    SELECT AVG(PBTime), COUNT(*) 
        FROM 
        (SELECT MIN(Times.RunTime) AS PBTime 
        FROM Times 
        INNER JOIN MapCourses ON Times.MapCourseID=MapCourses.ID 
        INNER JOIN Players ON Times.SteamID64=Players.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d AND Times.Teleports=0 
        GROUP BY Times.SteamID64) AS PBTimes
)";
