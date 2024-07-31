// =====[ START POSITIONS ]=====

constexpr char sqlite_startpos_create[] = R"(
	CREATE TABLE IF NOT EXISTS StartPosition ( 
		SteamID64 INTEGER NOT NULL, 
		MapID INTEGER NOT NULL, 
		X REAL NOT NULL, 
		Y REAL NOT NULL, 
		Z REAL NOT NULL, 
		Angle0 REAL NOT NULL, 
		Angle1 REAL NOT NULL, 
		CONSTRAINT PK_StartPosition PRIMARY KEY (SteamID64, MapID), 
		CONSTRAINT FK_StartPosition_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64) 
		CONSTRAINT FK_StartPosition_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
		ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char mysql_startpos_create[] = R"(
	CREATE TABLE IF NOT EXISTS StartPosition ( 
		SteamID64 BIGINT UNSIGNED NOT NULL, 
		MapID INTEGER UNSIGNED NOT NULL, 
		X REAL NOT NULL, 
		Y REAL NOT NULL, 
		Z REAL NOT NULL, 
		Angle0 REAL NOT NULL, 
		Angle1 REAL NOT NULL, 
		CONSTRAINT PK_StartPosition PRIMARY KEY (SteamID64, MapID), 
		CONSTRAINT FK_StartPosition_SteamID64 FOREIGN KEY (SteamID64) REFERENCES Players(SteamID64), 
		CONSTRAINT FK_StartPosition_MapID FOREIGN KEY (MapID) REFERENCES Maps(ID) 
		ON UPDATE CASCADE ON DELETE CASCADE)
)";

constexpr char sql_startpos_upsert[] = R"(
	REPLACE INTO StartPosition (SteamID64, MapID, X, Y, Z, Angle0, Angle1) 
		VALUES (%llu, %d, %f, %f, %f, %f, %f)
)";

constexpr char sql_startpos_get[] = R"(
	SELECT SteamID64, MapID, X, Y, Z, Angle0, Angle1 
		FROM 
			StartPosition 
		WHERE 
			SteamID64 = %llu AND 
			MapID = %d
)";
