// =====[ GENERAL ]=====

constexpr char sql_getpb[] = R"(
    SELECT Times.RunTime, Times.Teleports 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        WHERE Times.SteamID64=%llu AND MapCourses.ID=%d
        AND Times.ModeID=%d AND Times.StyleIDFlags=%llu
        ORDER BY Times.RunTime 
        LIMIT %d
)";

constexpr char sql_getpbpro[] = R"(
    SELECT Times.RunTime 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        WHERE Times.SteamID64=%llu AND MapCourses.ID=%d
        AND Times.ModeID=%d AND Times.StyleIDFlags=%llu
        AND Times.Teleports=0 
        ORDER BY Times.RunTime 
        LIMIT %d
)";

constexpr char sql_getmaptop[] = R"(
    SELECT t.TimeID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID 
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 
        AND t2.MapCourseID=t.MapCourseID AND t2.ModeID=t.ModeID AND t2.RunTime<t.RunTime 
        WHERE t2.TimeID IS NULL AND p.Cheater=0 AND mc.MapID=%d AND mc.Name='%s' AND t.ModeID=%d 
        ORDER BY PBTime 
        LIMIT %d
)";

constexpr char sql_getmaptoppro[] = R"(
    SELECT t.TimeID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID 
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID 
        AND t2.ModeID=t.ModeID AND t2.RunTime<t.RunTime AND t.Teleports=0 AND t2.Teleports=0 
        WHERE t2.TimeID IS NULL AND p.Cheater=0 AND mc.MapID=%d 
        AND mc.Name='%s' AND t.ModeID=%d AND t.Teleports=0 
        ORDER BY PBTime 
        LIMIT %d
)";

constexpr char sql_getwrs[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d 
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getwrspro[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND Times.Teleports=0 
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getpbs[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        WHERE Times.SteamID64=%llu AND MapCourses.MapID=%d 
        GROUP BY MapCourses.Name, Times.ModeID
)";

constexpr char sql_getpbspro[] = R"(
    SELECT MIN(Times.RunTime), MapCourses.Name, Times.ModeID 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        WHERE Times.SteamID64=%llu AND MapCourses.MapID=%d AND Times.Teleports=0 
        GROUP BY MapCourses.Name, Times.ModeID
)";

// The following queries should have no style!

constexpr char sql_getmaprank[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.RunTime < 
        (SELECT MIN(Times.RunTime) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Times.SteamID64=%llu AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d AND Times.StyleIDFlags=0) 
    + 1
)";

constexpr char sql_getmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.Teleports=0 
        AND Times.RunTime < 
        (SELECT MIN(Times.RunTime) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND Times.SteamID64=%llu AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.Teleports=0) 
    + 1
)";

constexpr char sql_getlowestmaprank[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.StyleIDFlags=0
)";

constexpr char sql_getlowestmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        WHERE Players.Cheater=0 AND MapCourses.MapID=%d 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.Teleports=0
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

constexpr char sql_getrecentrecords[] = R"(
    SELECT Maps.Name, MapCourses.Name, MapCourses.ID, Players.Alias, a.RunTime 
        FROM Times AS a 
        INNER JOIN MapCourses ON a.MapCourseID=MapCourses.ID 
        INNER JOIN Maps ON MapCourses.MapID=Maps.MapID 
        INNER JOIN Players ON a.SteamID64=Players.SteamID64 
        WHERE Players.Cheater=0 AND a.ModeID=%d AND Times.StyleIDFlags=0 
        AND NOT EXISTS 
        (SELECT * 
        FROM Times AS b 
        WHERE a.MapCourseID=b.MapCourseID 
        AND a.ModeID=b.ModeID AND a.StyleIDFlags=b.StyleIDFlags 
        AND a.Created>b.Created AND a.RunTime>b.RunTime) 
        ORDER BY a.TimeID DESC 
        LIMIT %d
)";

constexpr char sql_getrecentrecords_pro[] = R"(
    SELECT Maps.Name, MapCourses.Name, MapCourses.ID, Players.Alias, a.RunTime 
        FROM Times AS a 
        INNER JOIN MapCourses ON a.MapCourseID=MapCourses.ID 
        INNER JOIN Maps ON MapCourses.MapID=Maps.MapID 
        INNER JOIN Players ON a.SteamID64=Players.SteamID64 
        WHERE Players.Cheater=0 AND a.ModeID=%d AND Times.StyleIDFlags=0 AND a.Teleports=0 
        AND NOT EXISTS 
        (SELECT * 
        FROM Times AS b 
        WHERE b.Teleports=0 AND a.MapCourseID=b.MapCourseID 
        AND a.ModeID=b.ModeID AND a.StyleIDFlags=b.StyleIDFlags 
        AND a.Created>b.Created AND a.RunTime>b.RunTime) 
        ORDER BY a.TimeID DESC 
        LIMIT %d
)";
