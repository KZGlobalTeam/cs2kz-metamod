
constexpr char mysql_styles_create[] = "\
	CREATE TABLE IF NOT EXISTS `Styles` ( \
	StyleID INTEGER UNSIGNED NOT NULL AUTO_INCREMENT, \
	Name VARCHAR(16) NOT NULL UNIQUE, \
	CONSTRAINT PK_Styles PRIMARY KEY (StyleID))";

constexpr char mysql_styles_create[] = "\
	CREATE TABLE IF NOT EXISTS `Styles` ( \
	StyleID INTEGER NOT NULL, \
	Name VARCHAR(16) NOT NULL UNIQUE, \
	CONSTRAINT PK_Styles PRIMARY KEY (StyleID))";
