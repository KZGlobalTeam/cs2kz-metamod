// =====[ MODES ]=====

constexpr char mysql_modes_create[] = R"(
    CREATE TABLE IF NOT EXISTS `Modes` ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        Name VARCHAR(32) NOT NULL UNIQUE, 
        ShortName VARCHAR(16) NOT NULL UNIQUE, 
        CONSTRAINT PK_Modes PRIMARY KEY (ID))
)";

constexpr char sqlite_modes_create[] = R"(
    CREATE TABLE IF NOT EXISTS `Modes` ( 
        ID INTEGER NOT NULL, 
        Name VARCHAR(32) NOT NULL UNIQUE, 
        ShortName VARCHAR(16) NOT NULL UNIQUE,
        CONSTRAINT PK_Modes PRIMARY KEY (ID))
)";

constexpr char sqlite_modes_insert[] = R"(
    INSERT OR IGNORE INTO Modes (Name, ShortName) VALUES ('%s', '%s');
)";

constexpr char mysql_modes_insert[] = R"(
    INSERT IGNORE INTO Modes (Name, ShortName) VALUES ('%s', '%s');
)";

constexpr char sql_modes_findid[] = R"(
    SELECT ID FROM Modes WHERE Name = '%s'
)";

constexpr char sql_modes_fetch_all[] = R"(
    SELECT * FROM Modes
)";
