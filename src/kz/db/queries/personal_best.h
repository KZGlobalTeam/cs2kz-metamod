#pragma once
constexpr char sql_getpb[] = R"(
    SELECT Times.ID, Times.RunTime, Times.Teleports 
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
    SELECT Times.ID, Times.RunTime 
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
    SELECT COUNT(DISTINCT Times.SteamID64) + 1
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE Bans.ID IS NULL AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.RunTime < 
            (SELECT MIN(Times.RunTime) 
            FROM Times 
            INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
            INNER JOIN Maps ON Maps.ID = MapCourses.MapID
            INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
            LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
            WHERE Bans.ID IS NULL AND Times.SteamID64=%llu AND Maps.Name='%s'
            AND MapCourses.Name='%s' AND Times.ModeID=%d AND Times.StyleIDFlags=0)
)";

constexpr char sql_getmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) + 1
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE Bans.ID IS NULL AND Maps.Name='%s' AND MapCourses.Name='%s' 
        AND Times.ModeID=%d AND Times.StyleIDFlags=0 AND Times.Teleports=0 
        AND Times.RunTime < 
            (SELECT MIN(Times.RunTime) 
            FROM Times 
            INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
            INNER JOIN Maps ON Maps.ID = MapCourses.MapID
            INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
            LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
            WHERE Bans.ID IS NULL AND Times.SteamID64=%llu AND Maps.Name='%s' 
            AND MapCourses.Name='%s' AND Times.ModeID=%d 
            AND Times.StyleIDFlags=0 AND Times.Teleports=0)
)";

constexpr char sql_getlowestmaprank[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE Bans.ID IS NULL AND Maps.Name='%s' 
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.StyleIDFlags=0
)";

constexpr char sql_getlowestmaprankpro[] = R"(
    SELECT COUNT(DISTINCT Times.SteamID64) 
        FROM Times 
        INNER JOIN MapCourses ON MapCourses.ID=Times.MapCourseID 
        INNER JOIN Maps ON Maps.ID = MapCourses.MapID
        INNER JOIN Players ON Players.SteamID64=Times.SteamID64 
        LEFT JOIN Bans ON Bans.SteamID64=Players.SteamID64 AND (Bans.ExpiresAt IS NULL OR Bans.ExpiresAt > CURRENT_TIMESTAMP)
        WHERE Bans.ID IS NULL AND Maps.Name='%s'
        AND MapCourses.Name='%s' AND Times.ModeID=%d 
        AND Times.StyleIDFlags=0 AND Times.Teleports=0
)";

// Caching PBs

constexpr char sql_getpbs[] = R"(
    SELECT x.RunTime, x.MapCourseID, x.ModeID, t.Metadata
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
        INNER JOIN Maps m ON m.ID = mc.MapID
        INNER JOIN (
            SELECT MIN(t.RunTime) AS RunTime, t.MapCourseID, t.ModeID
                FROM Times t
                WHERE t.SteamID64=%llu
                GROUP BY t.MapCourseID, t.ModeID
        ) x ON x.RunTime = t.RunTime AND x.MapCourseID = t.MapCourseID AND x.ModeID = t.ModeID
        WHERE m.Name = '%s'
)";

constexpr char sql_getpbspro[] = R"(
    SELECT x.RunTime, x.MapCourseID, x.ModeID, t.Metadata
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID = t.MapCourseID
        INNER JOIN Maps m ON m.ID = mc.MapID
        INNER JOIN (
            SELECT MIN(t.RunTime) AS RunTime, t.MapCourseID, t.ModeID
                FROM Times t
                WHERE t.SteamID64=%llu AND t.Teleports=0 
                GROUP BY t.MapCourseID, t.ModeID
        ) x ON x.RunTime = t.RunTime AND x.MapCourseID = t.MapCourseID AND x.ModeID = t.ModeID
        WHERE m.Name = '%s'
)";

// A page of a player's personal bests on every course with their server ranks from newest to oldest.
// A rank is one plus the number of unbanned players with a faster time like in the course top.
// The %s slots take " AND <alias>.Teleports=0" for pro runs and "" otherwise.
constexpr char sql_getplayerpbs[] = R"(
    SELECT t.ID, Maps.Name, mc.Name, t.RunTime, t.Teleports,
        (SELECT COUNT(DISTINCT t3.SteamID64) + 1
            FROM Times t3
            LEFT JOIN Bans b3 ON b3.SteamID64=t3.SteamID64 AND (b3.ExpiresAt IS NULL OR b3.ExpiresAt > CURRENT_TIMESTAMP)
            WHERE b3.ID IS NULL AND t3.MapCourseID=t.MapCourseID AND t3.ModeID=t.ModeID AND t3.StyleIDFlags=0
            AND t3.RunTime < t.RunTime%s) AS PlayerRank
        FROM Times t
        INNER JOIN MapCourses mc ON mc.ID=t.MapCourseID
        INNER JOIN Maps ON Maps.ID=mc.MapID
        LEFT OUTER JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID
        AND t2.ModeID=t.ModeID AND t2.StyleIDFlags=t.StyleIDFlags
        AND (t2.RunTime < t.RunTime OR (t2.RunTime = t.RunTime AND t2.ID < t.ID))%s
    WHERE t2.ID IS NULL AND t.SteamID64=%llu AND t.ModeID=%d AND t.StyleIDFlags=0%s
        ORDER BY t.Created DESC, t.ID DESC
        LIMIT %d
        OFFSET %llu
)";

// Counts the courses the player has a personal best on.
constexpr char sql_getplayerpbcount[] = R"(
    SELECT COUNT(DISTINCT t.MapCourseID)
        FROM Times t
        WHERE t.SteamID64=%llu AND t.ModeID=%d AND t.StyleIDFlags=0%s
)";
