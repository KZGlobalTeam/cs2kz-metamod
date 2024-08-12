
constexpr char sql_getmaptop[] = R"(
    SELECT t.ID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 
        AND t2.MapCourseID=t.MapCourseID AND t2.ModeID=t.ModeID
        AND t2.StyleIDFlags=t.StyleIDFlags AND t2.RunTime<t.RunTime 
        WHERE t2.ID IS NULL AND p.Cheater=0 AND Maps.Name='%s' AND mc.Name='%s' AND t.ModeID=%d AND t.StyleIDFlags=0
        ORDER BY PBTime 
        LIMIT %d
)";

constexpr char sql_getmaptoppro[] = R"(
    SELECT t.ID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID 
        AND t2.ModeID=t.ModeID AND t2.StyleIDFlags=t.StyleIDFlags 
        AND t2.RunTime<t.RunTime AND t.Teleports=0 AND t2.Teleports=0 
        WHERE t2.ID IS NULL AND p.Cheater=0 AND Maps.Name='%s'
        AND mc.Name='%s' AND t.ModeID=%d AND t.Teleports=0 AND t.StyleIDFlags=0 
        ORDER BY PBTime 
        LIMIT %d
)";
