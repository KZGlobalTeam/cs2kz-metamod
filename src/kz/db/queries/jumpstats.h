// =====[ JUMPSTATS ]=====

constexpr char sqlite_jumpstats_create[] = R"(
    CREATE TABLE IF NOT EXISTS Jumpstats ( 
        ID INTEGER NOT NULL, 
        SteamID64 INTEGER NOT NULL, 
        JumpType INTEGER NOT NULL, 
        Mode INTEGER NOT NULL, 
        Distance INTEGER NOT NULL, 
        IsBlockJump INTEGER NOT NULL, 
        Block INTEGER NOT NULL, 
        Strafes INTEGER NOT NULL, 
        Sync INTEGER NOT NULL, 
        Pre INTEGER NOT NULL, 
        Max INTEGER NOT NULL, 
        Airtime INTEGER NOT NULL, 
        Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Jumpstats PRIMARY KEY (ID), 
        CONSTRAINT FK_Jumpstats_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char mysql_jumpstats_create[] = R"(
    CREATE TABLE IF NOT EXISTS Jumpstats ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        SteamID64 BIGINT UNSIGNED NOT NULL, 
        JumpType TINYINT UNSIGNED NOT NULL, 
        Mode TINYINT UNSIGNED NOT NULL, 
        Distance INTEGER UNSIGNED NOT NULL, 
        IsBlockJump TINYINT UNSIGNED NOT NULL, 
        Block SMALLINT UNSIGNED NOT NULL, 
        Strafes INTEGER UNSIGNED NOT NULL, 
        Sync INTEGER UNSIGNED NOT NULL, 
        Pre INTEGER UNSIGNED NOT NULL, 
        Max INTEGER UNSIGNED NOT NULL, 
        Airtime INTEGER UNSIGNED NOT NULL, 
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Jumpstats PRIMARY KEY (ID), 
        CONSTRAINT FK_Jumpstats_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
        ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sql_jumpstats_insert[] = R"(
    INSERT INTO Jumpstats (SteamID64, JumpType, Mode, Distance, IsBlockJump, Block, Strafes, Sync, Pre, Max, Airtime) 
        VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)
)";

constexpr char sql_jumpstats_update[] = R"(
    UPDATE Jumpstats 
        SET 
            SteamID64=%llu, 
            JumpType=%d, 
            Mode=%d, 
            Distance=%d, 
            IsBlockJump=%d, 
            Block=%d, 
            Strafes=%d, 
            Sync=%d, 
            Pre=%d, 
            Max=%d, 
            Airtime=%d 
        WHERE 
            ID=%d
)";

constexpr char sql_jumpstats_getrecord[] = R"(
    SELECT ID, Distance, Block 
        FROM 
            Jumpstats 
        WHERE 
            SteamID64=%llu AND 
            JumpType=%d AND 
            Mode=%d AND 
            IsBlockJump=%d 
        ORDER BY Block DESC, Distance DESC
)";

constexpr char sql_jumpstats_deleterecord[] = R"(
    DELETE 
        FROM 
            Jumpstats 
        WHERE 
            ID = 
            ( SELECT * FROM ( 
                SELECT ID 
                    FROM 
                        Jumpstats 
                    WHERE 
                        SteamID64=%llu AND 
                        JumpType=%d AND 
                        Mode=%d AND 
                        IsBlockJump=%d 
                    ORDER BY Block DESC, Distance DESC 
                    LIMIT 1 
                ) AS tmp 
            )
)";

constexpr char sql_jumpstats_deleteallrecords[] = R"(
    DELETE 
        FROM 
            Jumpstats 
        WHERE 
            SteamID64 = %llu;
)";

constexpr char sql_jumpstats_deletejump[] = R"(
    DELETE 
        FROM 
            Jumpstats 
        WHERE 
            ID = %d;
)";

constexpr char sql_jumpstats_getpbs[] = R"(
    SELECT MAX(Distance), Mode, JumpType 
        FROM 
            Jumpstats 
        WHERE 
            SteamID64=%llu 
        GROUP BY 
            Mode, JumpType
)";

constexpr char sql_jumpstats_getblockpbs[] = R"(
    SELECT MAX(js.Distance), js.Mode, js.JumpType, js.Block 
        FROM 
            Jumpstats js 
        INNER JOIN 
        ( 
            SELECT Mode, JumpType, MAX(BLOCK) Block 
                FROM 
                    Jumpstats 
                WHERE 
                    IsBlockJump=1 AND 
                    SteamID64=%llu 
                GROUP BY 
                    Mode, JumpType 
        ) pb 
        ON 
            js.Mode=pb.Mode AND 
            js.JumpType=pb.JumpType AND 
            js.Block=pb.Block 
        WHERE 
            js.SteamID64=%llu 
        GROUP BY 
            js.Mode, js.JumpType, js.Block
)";

constexpr char sql_jumpstats_ranking_gettop[] = R"(
    SELECT j.ID, p.SteamID64, p.Alias, j.Block, j.Distance, j.Strafes, j.Sync, j.Pre, j.Max, j.Airtime 
        FROM 
            Jumpstats j 
        INNER JOIN 
            Players p ON 
                p.SteamID64=j.SteamID64 AND 
                p.Cheater = 0 
        INNER JOIN 
            ( 
                SELECT j.SteamID64, j.JumpType, j.Mode, j.IsBlockJump, MAX(j.Distance) BestDistance 
                    FROM 
                        Jumpstats j 
                    INNER JOIN 
                        ( 
                            SELECT SteamID64, MAX(Block) AS MaxBlockDist 
                                FROM 
                                    Jumpstats 
                                WHERE 
                                    JumpType = %d AND 
                                    Mode = %d AND 
                                    IsBlockJump = %d 
                                GROUP BY SteamID64 
                        ) MaxBlock ON 
                            j.SteamID64 = MaxBlock.SteamID64 AND 
                            j.Block = MaxBlock.MaxBlockDist 
                    WHERE 
                        j.JumpType = %d AND 
                        j.Mode = %d AND 
                        j.IsBlockJump = %d 
                    GROUP BY j.SteamID64, j.JumpType, j.Mode, j.IsBlockJump 
            ) MaxDist ON 
                j.SteamID64 = MaxDist.SteamID64 AND 
                j.JumpType = MaxDist.JumpType AND 
                j.Mode = MaxDist.Mode AND 
                j.IsBlockJump = MaxDist.IsBlockJump AND 
                j.Distance = MaxDist.BestDistance 
        ORDER BY j.Block DESC, j.Distance DESC 
        LIMIT %d
)";

constexpr char sql_jumpstats_ranking_getrecord[] = R"(
    SELECT ID, Distance, Block 
        FROM 
            Jumpstats rec 
        WHERE 
            SteamID64 = %llu AND 
            JumpType = %d AND 
            Mode = %d AND 
            IsBlockJump = %d 
        ORDER BY Block DESC, Distance DESC
)";

constexpr char sql_jumpstats_ranking_getpbs[] = R"(
    SELECT b.ID, b.JumpType, b.Distance, b.Strafes, b.Sync, b.Pre, b.Max, b.Airtime 
        FROM Jumpstats b 
        INNER JOIN ( 
            SELECT a.SteamID64, a.Mode, a.JumpType, MAX(a.Distance) Distance 
            FROM Jumpstats a 
            WHERE a.SteamID64=%llu AND a.Mode=%d AND NOT a.IsBlockJump 
            GROUP BY a.JumpType, a.Mode, a.SteamID64 
        ) a ON a.JumpType=b.JumpType AND a.Distance=b.Distance 
        WHERE a.SteamID64=b.SteamID64 AND a.Mode=b.Mode AND NOT b.IsBlockJump 
        ORDER BY b.JumpType
)";

constexpr char sql_jumpstats_ranking_getblockpbs[] = R"(
    SELECT c.ID, c.JumpType, c.Block, c.Distance, c.Strafes, c.Sync, c.Pre, c.Max, c.Airtime 
        FROM Jumpstats c 
        INNER JOIN ( 
            SELECT a.SteamID64, a.Mode, a.JumpType, a.Block, MAX(b.Distance) Distance 
            FROM Jumpstats b 
            INNER JOIN ( 
                SELECT a.SteamID64, a.Mode, a.JumpType, MAX(a.Block) Block 
                FROM Jumpstats a 
                WHERE a.SteamID64=%llu AND a.Mode=%d AND a.IsBlockJump 
                GROUP BY a.JumpType, a.Mode, a.SteamID64 
            ) a ON a.JumpType=b.JumpType AND a.Block=b.Block 
            WHERE a.SteamID64=b.SteamID64 AND a.Mode=b.Mode AND b.IsBlockJump 
            GROUP BY a.JumpType, a.Mode, a.SteamID64, a.Block 
        ) b ON b.JumpType=c.JumpType AND b.Block=c.Block AND b.Distance=c.Distance 
        WHERE b.SteamID64=c.SteamID64 AND b.Mode=c.Mode AND c.IsBlockJump 
        ORDER BY c.JumpType
)";
