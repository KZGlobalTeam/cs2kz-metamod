// =====[ GENERAL ]=====

constexpr char sql_getpb[] = "\
SELECT Times.RunTime, Times.Teleports \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    WHERE Times.SteamID32=%d AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d \
    ORDER BY Times.RunTime \
    LIMIT %d";

constexpr char sql_getpbpro[] = "\
SELECT Times.RunTime \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    WHERE Times.SteamID32=%d AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d AND Times.Teleports=0 \
    ORDER BY Times.RunTime \
    LIMIT %d";

constexpr char sql_getmaptop[] = "\
SELECT t.TimeID, t.SteamID32, p.Alias, t.RunTime AS PBTime, t.Teleports \
    FROM Times t \
    INNER JOIN MapCourses mc ON mc.MapCourseID=t.MapCourseID \
    INNER JOIN Players p ON p.SteamID32=t.SteamID32 \
    LEFT OUTER JOIN Times t2 ON t2.SteamID32=t.SteamID32 \
    AND t2.MapCourseID=t.MapCourseID AND t2.Mode=t.Mode AND t2.RunTime<t.RunTime \
    WHERE t2.TimeID IS NULL AND p.Cheater=0 AND mc.MapID=%d AND mc.Course=%d AND t.Mode=%d \
    ORDER BY PBTime \
    LIMIT %d";

constexpr char sql_getmaptoppro[] = "\
SELECT t.TimeID, t.SteamID32, p.Alias, t.RunTime AS PBTime, t.Teleports \
    FROM Times t \
    INNER JOIN MapCourses mc ON mc.MapCourseID=t.MapCourseID \
    INNER JOIN Players p ON p.SteamID32=t.SteamID32 \
    LEFT OUTER JOIN Times t2 ON t2.SteamID32=t.SteamID32 AND t2.MapCourseID=t.MapCourseID \
    AND t2.Mode=t.Mode AND t2.RunTime<t.RunTime AND t.Teleports=0 AND t2.Teleports=0 \
    WHERE t2.TimeID IS NULL AND p.Cheater=0 AND mc.MapID=%d \
    AND mc.Course=%d AND t.Mode=%d AND t.Teleports=0 \
    ORDER BY PBTime \
    LIMIT %d";

constexpr char sql_getwrs[] = "\
SELECT MIN(Times.RunTime), MapCourses.Course, Times.Mode \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d \
    GROUP BY MapCourses.Course, Times.Mode";

constexpr char sql_getwrspro[] = "\
SELECT MIN(Times.RunTime), MapCourses.Course, Times.Mode \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND Times.Teleports=0 \
    GROUP BY MapCourses.Course, Times.Mode";

constexpr char sql_getpbs[] = "\
SELECT MIN(Times.RunTime), MapCourses.Course, Times.Mode \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    WHERE Times.SteamID32=%d AND MapCourses.MapID=%d \
    GROUP BY MapCourses.Course, Times.Mode";

constexpr char sql_getpbspro[] = "\
SELECT MIN(Times.RunTime), MapCourses.Course, Times.Mode \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    WHERE Times.SteamID32=%d AND MapCourses.MapID=%d AND Times.Teleports=0 \
    GROUP BY MapCourses.Course, Times.Mode";

constexpr char sql_getmaprank[] = "\
SELECT COUNT(DISTINCT Times.SteamID32) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND MapCourses.Course=%d \
    AND Times.Mode=%d AND Times.RunTime < \
    (SELECT MIN(Times.RunTime) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND Times.SteamID32=%d AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d) \
    + 1";

constexpr char sql_getmaprankpro[] = "\
SELECT COUNT(DISTINCT Times.SteamID32) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d AND MapCourses.Course=%d \
    AND Times.Mode=%d AND Times.Teleports=0 \
    AND Times.RunTime < \
    (SELECT MIN(Times.RunTime) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND Times.SteamID32=%d AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d AND Times.Teleports=0) \
    + 1";

constexpr char sql_getlowestmaprank[] = "\
SELECT COUNT(DISTINCT Times.SteamID32) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d";

constexpr char sql_getlowestmaprankpro[] = "\
SELECT COUNT(DISTINCT Times.SteamID32) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d AND Times.Teleports=0";

constexpr char sql_getcount_courses[] = "\
SELECT COUNT(*) \
    FROM MapCourses \
    INNER JOIN Maps ON Maps.MapID=MapCourses.MapID";

constexpr char sql_getcount_coursescompleted[] = "\
SELECT COUNT(DISTINCT Times.MapCourseID) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Maps ON Maps.MapID=MapCourses.MapID \
    AND Times.SteamID32=%d AND Times.Mode=%d";

constexpr char sql_getcount_coursescompletedpro[] = "\
SELECT COUNT(DISTINCT Times.MapCourseID) \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Maps ON Maps.MapID=MapCourses.MapID \
    AND Times.SteamID32=%d AND Times.Mode=%d AND Times.Teleports=0";

constexpr char sql_gettopplayers[] = "\
SELECT Players.SteamID32, Players.Alias, COUNT(*) AS RecordCount \
    FROM Times \
    INNER JOIN \
    (SELECT Times.MapCourseID, Times.Mode, MIN(Times.RunTime) AS RecordTime \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Maps ON Maps.MapID=MapCourses.MapID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND Maps.InRankedPool=1 AND MapCourses.Course=0 \
    AND Times.Mode=%d \
    GROUP BY Times.MapCourseID) Records \
    ON Times.MapCourseID=Records.MapCourseID AND Times.Mode=Records.Mode AND Times.RunTime=Records.RecordTime \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    GROUP BY Players.SteamID32, Players.Alias \
    ORDER BY RecordCount DESC \
    LIMIT %d"; // Doesn't include bonuses

constexpr char sql_gettopplayerspro[] = "\
SELECT Players.SteamID32, Players.Alias, COUNT(*) AS RecordCount \
    FROM Times \
    INNER JOIN \
    (SELECT Times.MapCourseID, Times.Mode, MIN(Times.RunTime) AS RecordTime \
    FROM Times \
    INNER JOIN MapCourses ON MapCourses.MapCourseID=Times.MapCourseID \
    INNER JOIN Maps ON Maps.MapID=MapCourses.MapID \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    WHERE Players.Cheater=0 AND Maps.InRankedPool=1 AND MapCourses.Course=0 \
    AND Times.Mode=%d AND Times.Teleports=0 \
    GROUP BY Times.MapCourseID) Records \
    ON Times.MapCourseID=Records.MapCourseID AND Times.Mode=Records.Mode AND Times.RunTime=Records.RecordTime AND Times.Teleports=0 \
    INNER JOIN Players ON Players.SteamID32=Times.SteamID32 \
    GROUP BY Players.SteamID32, Players.Alias \
    ORDER BY RecordCount DESC \
    LIMIT %d"; // Doesn't include bonuses

constexpr char sql_getaverage[] = "\
SELECT AVG(PBTime), COUNT(*) \
    FROM \
    (SELECT MIN(Times.RunTime) AS PBTime \
    FROM Times \
    INNER JOIN MapCourses ON Times.MapCourseID=MapCourses.MapCourseID \
    INNER JOIN Players ON Times.SteamID32=Players.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d \
    GROUP BY Times.SteamID32) AS PBTimes";

constexpr char sql_getaverage_pro[] = "\
SELECT AVG(PBTime), COUNT(*) \
    FROM \
    (SELECT MIN(Times.RunTime) AS PBTime \
    FROM Times \
    INNER JOIN MapCourses ON Times.MapCourseID=MapCourses.MapCourseID \
    INNER JOIN Players ON Times.SteamID32=Players.SteamID32 \
    WHERE Players.Cheater=0 AND MapCourses.MapID=%d \
    AND MapCourses.Course=%d AND Times.Mode=%d AND Times.Teleports=0 \
    GROUP BY Times.SteamID32) AS PBTimes";

constexpr char sql_getrecentrecords[] = "\
SELECT Maps.Name, MapCourses.Course, MapCourses.MapCourseID, Players.Alias, a.RunTime \
    FROM Times AS a \
    INNER JOIN MapCourses ON a.MapCourseID=MapCourses.MapCourseID \
    INNER JOIN Maps ON MapCourses.MapID=Maps.MapID \
    INNER JOIN Players ON a.SteamID32=Players.SteamID32 \
    WHERE Players.Cheater=0 AND Maps.InRankedPool AND a.Mode=%d \
    AND NOT EXISTS \
    (SELECT * \
    FROM Times AS b \
    WHERE a.MapCourseID=b.MapCourseID AND a.Mode=b.Mode \
    AND a.Created>b.Created AND a.RunTime>b.RunTime) \
    ORDER BY a.TimeID DESC \
    LIMIT %d";

constexpr char sql_getrecentrecords_pro[] = "\
SELECT Maps.Name, MapCourses.Course, MapCourses.MapCourseID, Players.Alias, a.RunTime \
    FROM Times AS a \
    INNER JOIN MapCourses ON a.MapCourseID=MapCourses.MapCourseID \
    INNER JOIN Maps ON MapCourses.MapID=Maps.MapID \
    INNER JOIN Players ON a.SteamID32=Players.SteamID32 \
    WHERE Players.Cheater=0 AND Maps.InRankedPool AND a.Mode=%d AND a.Teleports=0 \
    AND NOT EXISTS \
    (SELECT * \
    FROM Times AS b \
    WHERE b.Teleports=0 AND a.MapCourseID=b.MapCourseID AND a.Mode=b.Mode \
    AND a.Created>b.Created AND a.RunTime>b.RunTime) \
    ORDER BY a.TimeID DESC \
    LIMIT %d";
