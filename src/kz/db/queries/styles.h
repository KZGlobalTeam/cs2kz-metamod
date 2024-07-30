// =====[ STYLES ]=====

constexpr char mysql_styles_create[] = R"(
    CREATE TABLE IF NOT EXISTS `Styles` ( 
        ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, 
        Name VARCHAR(16) NOT NULL UNIQUE, 
        ShortName VARCHAR(16) NOT NULL UNIQUE, 
        CONSTRAINT PK_Styles PRIMARY KEY (ID))
)";

constexpr char sqlite_styles_create[] = R"(
    CREATE TABLE IF NOT EXISTS `Styles` ( 
        ID INTEGER NOT NULL, 
        Name VARCHAR(16) NOT NULL UNIQUE, 
        ShortName VARCHAR(16) NOT NULL UNIQUE, 
        CONSTRAINT PK_Styles PRIMARY KEY (ID))
)";

constexpr char sqlite_styles_insert[] = R"(
    INSERT OR IGNORE INTO Styles (Name, ShortName) VALUES ('%s', '%s')
)";

constexpr char mysql_styles_insert[] = R"(
    INSERT IGNORE INTO Styles (Name, ShortName) VALUES ('%s', '%s')
)";

constexpr char sql_styles_findid[] = R"(
    SELECT ID FROM Styles WHERE Name = '%s'
)";

constexpr char sql_styles_fetch_all[] = R"(
    SELECT * FROM Styles
)";
