constexpr char sql_getcoursetop[] = R"(
    SELECT t.ID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
    LEFT JOIN Bans b ON b.SteamID64=t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt > CURRENT_TIMESTAMP)
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 
        AND t2.MapCourseID=t.MapCourseID AND t2.ModeID=t.ModeID
        AND t2.StyleIDFlags=t.StyleIDFlags AND t2.RunTime<t.RunTime 
    WHERE t2.ID IS NULL AND b.ID IS NULL AND Maps.Name='%s' AND mc.Name='%s' AND t.ModeID=%d AND t.StyleIDFlags=0
        ORDER BY PBTime ASC
        LIMIT %d
        OFFSET %d
)";

constexpr char sql_getcoursetoppro[] = R"(
    SELECT t.ID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports 
        FROM Times t 
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID 
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64 
    LEFT JOIN Bans b ON b.SteamID64=t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt > CURRENT_TIMESTAMP)
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID 
        AND t2.ModeID=t.ModeID AND t2.StyleIDFlags=t.StyleIDFlags 
        AND t2.RunTime<t.RunTime AND t.Teleports=0 AND t2.Teleports=0 
    WHERE t2.ID IS NULL AND b.ID IS NULL AND Maps.Name='%s'
        AND mc.Name='%s' AND t.ModeID=%d AND t.Teleports=0 AND t.StyleIDFlags=0 
        ORDER BY PBTime ASC
        LIMIT %d
        OFFSET %d
)";

// Caching PBs

constexpr char sql_getsrs[] = R"(
    SELECT x.RunTime, x.MapCourseID, x.ModeID, t.Metadata
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
        INNER JOIN Maps m ON m.ID = mc.MapID
        INNER JOIN (
            SELECT MIN(t.RunTime) AS RunTime, t.MapCourseID, t.ModeID
                FROM Times t
                GROUP BY t.MapCourseID, t.ModeID
        ) x ON x.RunTime = t.RunTime AND x.MapCourseID = t.MapCourseID AND x.ModeID = t.ModeID
        WHERE m.Name = '%s'
)";

constexpr char sql_getsrspro[] = R"(
    SELECT x.RunTime, x.MapCourseID, x.ModeID, t.Metadata
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
        INNER JOIN Maps m ON m.ID = mc.MapID
        INNER JOIN (
            SELECT MIN(t.RunTime) AS RunTime, t.MapCourseID, t.ModeID
                FROM Times t
                WHERE t.Teleports=0 
                GROUP BY t.MapCourseID, t.ModeID
        ) x ON x.RunTime = t.RunTime AND x.MapCourseID = t.MapCourseID AND x.ModeID = t.ModeID
        WHERE m.Name = '%s' 
)";
