
constexpr char mysql_modes_create[] = "\
	CREATE TABLE IF NOT EXISTS `Modes` ( \
	ModeID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
	Name VARCHAR(16) NOT NULL UNIQUE, \
	CONSTRAINT PK_Modes PRIMARY KEY (ModeID)";

constexpr char sqlite_modes_create[] = "\
	CREATE TABLE IF NOT EXISTS `Modes` ( \
	ModeID INTEGER NOT NULL, \
	Name VARCHAR(16) NOT NULL UNIQUE, \
	CONSTRAINT PK_Modes PRIMARY KEY (ModeID)";
