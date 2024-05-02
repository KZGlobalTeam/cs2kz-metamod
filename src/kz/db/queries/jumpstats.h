// =====[ JUMPSTATS ]=====

constexpr char sqlite_jumpstats_create[] = "\
CREATE TABLE IF NOT EXISTS Jumpstats ( \
    JumpID INTEGER NOT NULL, \
    SteamID32 INTEGER NOT NULL, \
    JumpType INTEGER NOT NULL, \
    Mode INTEGER NOT NULL, \
    Distance INTEGER NOT NULL, \
    IsBlockJump INTEGER NOT NULL, \
    Block INTEGER NOT NULL, \
    Strafes INTEGER NOT NULL, \
    Sync INTEGER NOT NULL, \
    Pre INTEGER NOT NULL, \
    Max INTEGER NOT NULL, \
    Airtime INTEGER NOT NULL, \
    Created INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Jumpstats PRIMARY KEY (JumpID), \
    CONSTRAINT FK_Jumpstats_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char mysql_jumpstats_create[] = "\
CREATE TABLE IF NOT EXISTS Jumpstats ( \
    JumpID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
    SteamID32 INTEGER UNSIGNED NOT NULL, \
    JumpType TINYINT UNSIGNED NOT NULL, \
    Mode TINYINT UNSIGNED NOT NULL, \
    Distance INTEGER UNSIGNED NOT NULL, \
    IsBlockJump TINYINT UNSIGNED NOT NULL, \
    Block SMALLINT UNSIGNED NOT NULL, \
    Strafes INTEGER UNSIGNED NOT NULL, \
    Sync INTEGER UNSIGNED NOT NULL, \
    Pre INTEGER UNSIGNED NOT NULL, \
    Max INTEGER UNSIGNED NOT NULL, \
    Airtime INTEGER UNSIGNED NOT NULL, \
    Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
    CONSTRAINT PK_Jumpstats PRIMARY KEY (JumpID), \
    CONSTRAINT FK_Jumpstats_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char sql_jumpstats_insert[] = "\
INSERT INTO Jumpstats (SteamID32, JumpType, Mode, Distance, IsBlockJump, Block, Strafes, Sync, Pre, Max, Airtime) \
    VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)";

constexpr char sql_jumpstats_update[] = "\
UPDATE Jumpstats \
    SET \
        SteamID32=%d, \
        JumpType=%d, \
        Mode=%d, \
        Distance=%d, \
        IsBlockJump=%d, \
        Block=%d, \
        Strafes=%d, \
        Sync=%d, \
        Pre=%d, \
        Max=%d, \
        Airtime=%d \
    WHERE \
        JumpID=%d";

constexpr char sql_jumpstats_getrecord[] = "\
SELECT JumpID, Distance, Block \
    FROM \
        Jumpstats \
    WHERE \
        SteamID32=%d AND \
        JumpType=%d AND \
        Mode=%d AND \
        IsBlockJump=%d \
    ORDER BY Block DESC, Distance DESC";

constexpr char sql_jumpstats_deleterecord[] = "\
DELETE \
    FROM \
        Jumpstats \
    WHERE \
        JumpID = \
        ( SELECT * FROM ( \
            SELECT JumpID \
                FROM \
                    Jumpstats \
                WHERE \
                    SteamID32=%d AND \
                    JumpType=%d AND \
                    Mode=%d AND \
                    IsBlockJump=%d \
                ORDER BY Block DESC, Distance DESC \
                LIMIT 1 \
			) AS tmp \
        )";

constexpr char sql_jumpstats_deleteallrecords[] = "\
DELETE \
	FROM \
		Jumpstats \
	WHERE \
		SteamID32 = %d;";

constexpr char sql_jumpstats_deletejump[] = "\
DELETE \
	FROM \
		Jumpstats \
	WHERE \
		JumpID = %d;";

constexpr char sql_jumpstats_getpbs[] = "\
SELECT MAX(Distance), Mode, JumpType \
    FROM \
        Jumpstats \
    WHERE \
        SteamID32=%d \
    GROUP BY \
    	Mode, JumpType";

constexpr char sql_jumpstats_getblockpbs[] = "\
SELECT MAX(js.Distance), js.Mode, js.JumpType, js.Block \
	FROM \
		Jumpstats js \
	INNER JOIN \
	( \
		SELECT Mode, JumpType, MAX(BLOCK) Block \
			FROM \
				Jumpstats \
			WHERE \
				IsBlockJump=1 AND \
				SteamID32=%d \
			GROUP BY \ 
				Mode, JumpType \
	) pb \
	ON \
		js.Mode=pb.Mode AND \
		js.JumpType=pb.JumpType AND \
		js.Block=pb.Block \
	WHERE \
		js.SteamID32=%d \
	GROUP BY \
		js.Mode, js.JumpType, js.Block";

constexpr char sql_jumpstats_gettop[] = "\
SELECT j.JumpID, p.SteamID32, p.Alias, j.Block, j.Distance, j.Strafes, j.Sync, j.Pre, j.Max, j.Airtime \
	FROM \
		Jumpstats j \
    INNER JOIN \
        Players p ON \
            p.SteamID32=j.SteamID32 AND \
			p.Cheater = 0 \
	INNER JOIN \
		( \
			SELECT j.SteamID32, j.JumpType, j.Mode, j.IsBlockJump, MAX(j.Distance) BestDistance \
			    FROM \
			        Jumpstats j \
			    INNER JOIN \
			        ( \
			            SELECT SteamID32, MAX(Block) AS MaxBlockDist \
			                FROM \
			                    Jumpstats \
			                WHERE \
			                    JumpType = %d AND \
			                    Mode = %d AND \
			                    IsBlockJump = %d \
			                GROUP BY SteamID32 \
			        ) MaxBlock ON \
			            j.SteamID32 = MaxBlock.SteamID32 AND \
			            j.Block = MaxBlock.MaxBlockDist \
			    WHERE \
			        j.JumpType = %d AND \
			        j.Mode = %d AND \
			        j.IsBlockJump = %d \
			    GROUP BY j.SteamID32, j.JumpType, j.Mode, j.IsBlockJump \
		) MaxDist ON \
			j.SteamID32 = MaxDist.SteamID32 AND \
			j.JumpType = MaxDist.JumpType AND \
			j.Mode = MaxDist.Mode AND \
			j.IsBlockJump = MaxDist.IsBlockJump AND \
			j.Distance = MaxDist.BestDistance \
    ORDER BY j.Block DESC, j.Distance DESC \
    LIMIT %d";

constexpr char sql_jumpstats_getrecord[] = "\
SELECT JumpID, Distance, Block \
    FROM \
        Jumpstats rec \
    WHERE \
        SteamID32 = %d AND \
        JumpType = %d AND \
        Mode = %d AND \
        IsBlockJump = %d \
    ORDER BY Block DESC, Distance DESC";

constexpr char sql_jumpstats_getpbs[] = "\
SELECT b.JumpID, b.JumpType, b.Distance, b.Strafes, b.Sync, b.Pre, b.Max, b.Airtime \
    FROM Jumpstats b \
    INNER JOIN ( \
        SELECT a.SteamID32, a.Mode, a.JumpType, MAX(a.Distance) Distance \
        FROM Jumpstats a \
        WHERE a.SteamID32=%d AND a.Mode=%d AND NOT a.IsBlockJump \
        GROUP BY a.JumpType, a.Mode, a.SteamID32 \
    ) a ON a.JumpType=b.JumpType AND a.Distance=b.Distance \
    WHERE a.SteamID32=b.SteamID32 AND a.Mode=b.Mode AND NOT b.IsBlockJump \
    ORDER BY b.JumpType";

constexpr char sql_jumpstats_getblockpbs[] = "\
SELECT c.JumpID, c.JumpType, c.Block, c.Distance, c.Strafes, c.Sync, c.Pre, c.Max, c.Airtime \
    FROM Jumpstats c \
    INNER JOIN ( \
        SELECT a.SteamID32, a.Mode, a.JumpType, a.Block, MAX(b.Distance) Distance \
        FROM Jumpstats b \
        INNER JOIN ( \
            SELECT a.SteamID32, a.Mode, a.JumpType, MAX(a.Block) Block \
            FROM Jumpstats a \
            WHERE a.SteamID32=%d AND a.Mode=%d AND a.IsBlockJump \
            GROUP BY a.JumpType, a.Mode, a.SteamID32 \
        ) a ON a.JumpType=b.JumpType AND a.Block=b.Block \
        WHERE a.SteamID32=b.SteamID32 AND a.Mode=b.Mode AND b.IsBlockJump \
        GROUP BY a.JumpType, a.Mode, a.SteamID32, a.Block \
    ) b ON b.JumpType=c.JumpType AND b.Block=c.Block AND b.Distance=c.Distance \
    WHERE b.SteamID32=c.SteamID32 AND b.Mode=c.Mode AND c.IsBlockJump \
    ORDER BY c.JumpType";
