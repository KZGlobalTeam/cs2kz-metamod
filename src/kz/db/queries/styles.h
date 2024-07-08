// =====[ STYLES ]=====

constexpr char mysql_styles_create[] = "\
CREATE TABLE IF NOT EXISTS `Styles` ( \
    ID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
    Name VARCHAR(16) NOT NULL UNIQUE, \
    CONSTRAINT PK_Styles PRIMARY KEY (ID))";

constexpr char sqlite_styles_create[] = "\
CREATE TABLE IF NOT EXISTS `Styles` ( \
    ID INTEGER NOT NULL, \
    Name VARCHAR(16) NOT NULL UNIQUE, \
    CONSTRAINT PK_Styles PRIMARY KEY (ID))";

constexpr char sqlite_styles_insert[] = "\
INSERT OR IGNORE INTO Styles (Name) VALUES ('%s');";

constexpr char mysql_styles_insert[] = "\
INSERT IGNORE INTO Styles (Name) VALUES ('%s');";

constexpr char sql_styles_findid[] = "\
SELECT ID FROM Styles WHERE Name = '%s'";
