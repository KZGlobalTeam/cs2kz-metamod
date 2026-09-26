constexpr char sql_getcoursetop[] = R"(
    SELECT t.ID, t.SteamID64, p.Alias, t.RunTime AS PBTime, t.Teleports
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
        INNER JOIN Maps ON Maps.ID = mc.MapID
        INNER JOIN Players p ON p.SteamID64=t.SteamID64
    LEFT JOIN Bans b ON b.SteamID64=t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt > CURRENT_TIMESTAMP)
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64
        AND t2.MapCourseID=t.MapCourseID AND t2.ModeID=t.ModeID
        AND t2.StyleIDFlags=t.StyleIDFlags
		AND (t2.RunTime < t.RunTime OR (t2.RunTime = t.RunTime AND t2.ID < t.ID))
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
        AND (t2.RunTime < t.RunTime OR (t2.RunTime = t.RunTime AND t2.ID < t.ID))
		AND t.Teleports=0 AND t2.Teleports=0
    WHERE t2.ID IS NULL AND b.ID IS NULL AND Maps.Name='%s'
        AND mc.Name='%s' AND t.ModeID=%d AND t.Teleports=0 AND t.StyleIDFlags=0
        ORDER BY PBTime ASC
        LIMIT %d
        OFFSET %d
)";

// Map-level record cache, including the winning replay UUID. No extra query for HUD progress.
constexpr char sql_getsrs[] = R"(
    SELECT t.RunTime, t.MapCourseID, t.ModeID, t.Metadata, t.ID
    FROM Times t
    INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
    INNER JOIN Maps m ON m.ID = mc.MapID
    LEFT JOIN Bans b ON b.SteamID64 = t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt > CURRENT_TIMESTAMP)
    WHERE m.Name = '%s' AND t.StyleIDFlags = 0 AND b.ID IS NULL
    AND NOT EXISTS (
        SELECT 1 FROM Times t2
        LEFT JOIN Bans b2 ON b2.SteamID64 = t2.SteamID64 AND (b2.ExpiresAt IS NULL OR b2.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE t2.MapCourseID = t.MapCourseID AND t2.ModeID = t.ModeID
        AND t2.StyleIDFlags = 0 AND b2.ID IS NULL
        AND (t2.RunTime < t.RunTime OR (t2.RunTime = t.RunTime AND t2.ID < t.ID))
    )
)";

constexpr char sql_getsrspro[] = R"(
    SELECT t.RunTime, t.MapCourseID, t.ModeID, t.Metadata, t.ID
    FROM Times t
    INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
    INNER JOIN Maps m ON m.ID = mc.MapID
    LEFT JOIN Bans b ON b.SteamID64 = t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt > CURRENT_TIMESTAMP)
    WHERE m.Name = '%s' AND t.StyleIDFlags = 0 AND b.ID IS NULL AND t.Teleports = 0
    AND NOT EXISTS (
        SELECT 1 FROM Times t2
        LEFT JOIN Bans b2 ON b2.SteamID64 = t2.SteamID64 AND (b2.ExpiresAt IS NULL OR b2.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE t2.MapCourseID = t.MapCourseID AND t2.ModeID = t.ModeID
        AND t2.StyleIDFlags = 0 AND b2.ID IS NULL AND t2.Teleports = 0
        AND (t2.RunTime < t.RunTime OR (t2.RunTime = t.RunTime AND t2.ID < t.ID))
    )
)";
