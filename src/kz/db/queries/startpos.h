// =====[ START POSITIONS ]=====

constexpr char sqlite_startpos_create[] = "\
CREATE TABLE IF NOT EXISTS StartPosition ( \
	SteamID32 INTEGER NOT NULL, \
	MapID INTEGER NOT NULL, \
	X REAL NOT NULL, \
	Y REAL NOT NULL, \
	Z REAL NOT NULL, \
	Angle0 REAL NOT NULL, \
	Angle1 REAL NOT NULL, \
	CONSTRAINT PK_StartPosition PRIMARY KEY (SteamID32, MapID), \
    CONSTRAINT FK_StartPosition_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32) \
    CONSTRAINT FK_StartPosition_MapID FOREIGN KEY (MapID) REFERENCES Maps(MapID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char mysql_startpos_create[] = "\
CREATE TABLE IF NOT EXISTS StartPosition ( \
	SteamID32 INTEGER UNSIGNED NOT NULL, \
	MapID INTEGER UNSIGNED NOT NULL, \
	X REAL NOT NULL, \
	Y REAL NOT NULL, \
	Z REAL NOT NULL, \
	Angle0 REAL NOT NULL, \
	Angle1 REAL NOT NULL, \
	CONSTRAINT PK_StartPosition PRIMARY KEY (SteamID32, MapID), \
    CONSTRAINT FK_StartPosition_SteamID32 FOREIGN KEY (SteamID32) REFERENCES Players(SteamID32), \
    CONSTRAINT FK_StartPosition_MapID FOREIGN KEY (MapID) REFERENCES Maps(MapID) \
    ON UPDATE CASCADE ON DELETE CASCADE)";

constexpr char sql_startpos_upsert[] = "\
REPLACE INTO StartPosition (SteamID32, MapID, X, Y, Z, Angle0, Angle1) \
	VALUES (%d, %d, %f, %f, %f, %f, %f)";

constexpr char sql_startpos_get[] = "\
SELECT SteamID32, MapID, X, Y, Z, Angle0, Angle1 \
	FROM \
		StartPosition \
	WHERE \
		SteamID32 = %d AND \
		MapID = %d";
