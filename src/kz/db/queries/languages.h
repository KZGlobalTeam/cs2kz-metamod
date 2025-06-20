#pragma once

constexpr char mysql_ukrainian_language_fix[] = R"(
	UPDATE Players
	SET Preferences = JSON_SET(Preferences, '$.preferredLanguage', 'uk')
	WHERE JSON_UNQUOTE(JSON_EXTRACT(Preferences, '$.preferredLanguage')) = 'ua';
)";

constexpr char sqlite_ukrainian_language_fix[] = R"(
	UPDATE Players
	SET Preferences = json_set(Preferences, '$.preferredLanguage', 'uk')
	WHERE json_extract(Preferences, '$.preferredLanguage') = 'ua';
)";
