#!/usr/bin/env python3
"""
Translation file linter for cs2kz-metamod phrase files and menu files.
Checks for format issues and missing/duplicate language entries.

Usage:
    python check_translations.py [base_directory]

If no directory is specified, defaults to current directory.
The script will look for:
  - translations/ folder for phrase files and config.txt
  - menu/ folder for menu translation files
  
Ignoring sections:
    Use // lint-ignore-start and // lint-ignore-end comments to ignore sections.
    Use // lint-ignore-next to ignore the next line.
    Use // lint-ignore at the end of a line to ignore that specific line.

    Example:
        // lint-ignore-start
        "Problematic Phrase"
        {
            "en"    "This won't be checked"
        }
        // lint-ignore-end

        // lint-ignore-next
        "en"    "This line is also ignored"

        "en"    "Ignore this specific line"  // lint-ignore
"""

import json
import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional
from collections import defaultdict
from io import StringIO


@dataclass
class Issue:
    file: str
    line: int
    issue_type: str
    category: str
    message: str
    context: Optional[str] = None

    def __str__(self) -> str:
        prefix = "ERROR" if self.issue_type == "error" else "WARNING"
        msg = f"{self.file}:{self.line}: [{prefix}] {self.category}: {self.message}"
        if self.context:
            msg += f"\n    Context: {self.context}"
        return msg


@dataclass
class PhraseBlock:
    name: str
    start_line: int
    end_line: int
    languages: dict = field(default_factory=dict)
    format_spec: Optional[str] = None
    format_line: int = 0
    placeholders: set = field(default_factory=set)
    ignored: bool = False


@dataclass
class MenuCommand:
    name: str
    start_line: int
    end_line: int
    hotkey: Optional[str] = None
    label: Optional[str] = None
    cmd: Optional[str] = None


@dataclass
class MenuGroup:
    name: str
    start_line: int
    end_line: int
    hotkey: Optional[str] = None
    title: Optional[str] = None
    timeout: Optional[str] = None
    commands: list = field(default_factory=list)


class IgnoreTracker:

    IGNORE_START = "lint-ignore-start"
    IGNORE_END = "lint-ignore-end"
    IGNORE_NEXT = "lint-ignore-next"
    IGNORE_LINE = "lint-ignore"

    def __init__(self, lines: list[str]):
        self.ignored_lines: set[int] = set()
        self._parse_ignore_comments(lines)

    def _parse_ignore_comments(self, lines: list[str]) -> None:
        in_ignored_block = False
        ignore_next = False

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            if self.IGNORE_START in stripped:
                in_ignored_block = True
                self.ignored_lines.add(line_num)
                continue

            if self.IGNORE_END in stripped:
                in_ignored_block = False
                self.ignored_lines.add(line_num)
                continue

            if self.IGNORE_NEXT in stripped and self.IGNORE_START not in stripped:
                ignore_next = True
                self.ignored_lines.add(line_num)
                continue

            if in_ignored_block:
                self.ignored_lines.add(line_num)
                continue

            if ignore_next:
                self.ignored_lines.add(line_num)
                ignore_next = False
                continue

            if self.IGNORE_LINE in stripped:
                comment_part = stripped.split("//")[-1] if "//" in stripped else ""
                if self.IGNORE_LINE in comment_part:
                    if (self.IGNORE_START not in comment_part and
                        self.IGNORE_END not in comment_part and
                        self.IGNORE_NEXT not in comment_part):
                        self.ignored_lines.add(line_num)

    def is_ignored(self, line_num: int) -> bool:
        return line_num in self.ignored_lines

    def is_range_ignored(self, start_line: int, end_line: int) -> bool:
        return all(self.is_ignored(ln) for ln in range(start_line, end_line + 1))


class TranslationLinter:

    DEFAULT_KNOWN_LANGUAGES = {
        "en", "chi", "ua", "sv", "fi", "ko", "lv", "de", "ru", "pl",
        "pt", "es", "fr", "it", "nl", "tr", "hu", "cs", "da", "no",
        "jp", "tw", "br", "bg", "ro", "el", "th", "vi", "id", "ms",
        "ar", "cze", "he", "lt", "pt_p", "sk", "zho"
    }

    KNOWN_COLOR_TAGS = {
        "yellow", "grey", "gray", "default", "green", "red", "blue",
        "white", "orange", "purple", "lime", "lightgreen", "darkgreen",
        "lightblue", "darkblue", "pink", "lightred", "darkred", "olive",
        "bluegrey", "normal", "unusual", "vintage", "unique", "community",
        "developer", "haunted", "gold"
    }

    SPECIAL_DIRECTIVES = {"#format"}

    MENU_LANGUAGE_MAP = {
        "english": "en",
        "arabic": "ar",
        "brazilian": "pt",
        "bulgarian": "bg",
        "czech": "cze",
        "danish": "da",
        "dutch": "nl",
        "finnish": "fi",
        "french": "fr",
        "german": "de",
        "greek": "el",
        "hebrew": "he",
        "hungarian": "hu",
        "italian": "it",
        "japanese": "jp",
        "koreana": "ko",
        "korean": "ko",
        "latvian": "lv",
        "lithuanian": "lt",
        "norwegian": "no",
        "polish": "pl",
        "portuguese": "pt_p",
        "romanian": "ro",
        "russian": "ru",
        "schinese": "chi",
        "slovak": "sk",
        "spanish": "es",
        "swedish": "sv",
        "tchinese": "zho",
        "thai": "th",
        "turkish": "tr",
        "ukrainian": "ua",
        "vietnamese": "vi",
    }

    def __init__(self, base_dir: str = "."):
        self.base_dir = Path(base_dir).resolve()
        self.translations_dir = self.base_dir / "translations"
        self.menu_dir = self.base_dir / "menu"
        
        self.issues: list[Issue] = []
        self.all_phrase_blocks: dict[str, list[PhraseBlock]] = {}
        self.ignore_trackers: dict[str, IgnoreTracker] = {}
        self.global_languages: set[str] = set()

        self.known_languages = self._parse_config_languages()
        self.language_names = self._parse_config_language_names()

        self.menu_files: dict[str, list[MenuGroup]] = {}
        self.menu_languages: set[str] = set()
        
        print(f"Base directory: {self.base_dir}")
        print(f"Translations directory: {self.translations_dir}")
        print(f"Menu directory: {self.menu_dir}\n")

    def _parse_config_languages(self) -> set[str]:
        config_path = self.translations_dir / "config.txt"

        if not config_path.exists():
            print(f"config.txt not found in {self.translations_dir}, using default language list")
            return self.DEFAULT_KNOWN_LANGUAGES.copy()

        try:
            with open(config_path, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            print(f"Failed to read config.txt: {e}, using default language list")
            return self.DEFAULT_KNOWN_LANGUAGES.copy()

        languages = set()

        for line in content.splitlines():
            stripped = line.strip()

            if not stripped or stripped.startswith("//") or stripped in ["{", "}", '"Languages"']:
                continue

            match = re.match(r'^"([^"]+)"\s+"([^"]+)"', stripped)
            if match:
                lang_code = match.group(2)
                languages.add(lang_code)

        if languages:
            print(f"Loaded {len(languages)} language codes from config.txt")
            return languages
        else:
            print(f"No languages found in config.txt, using default language list")
            return self.DEFAULT_KNOWN_LANGUAGES.copy()

    def _parse_config_language_names(self) -> dict[str, str]:
        config_path = self.translations_dir / "config.txt"

        if not config_path.exists():
            return {}

        try:
            with open(config_path, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception:
            return {}

        names = {}

        for line in content.splitlines():
            stripped = line.strip()

            if not stripped or stripped.startswith("//") or stripped in ["{", "}", '"Languages"']:
                continue

            match = re.match(r'^"([^"]+)"\s+"([^"]+)"', stripped)
            if match:
                lang_name = match.group(1)
                lang_code = match.group(2)
                if lang_code not in names:
                    names[lang_code] = lang_name

        return names

    def _get_language_display_name(self, code: str) -> str:
        if code in self.language_names:
            return f"{code} ({self.language_names[code]})"
        return code

    def _add_issue(self, filename: str, line: int, issue_type: str,
                   category: str, message: str, context: Optional[str] = None) -> None:
        if filename in self.ignore_trackers:
            if self.ignore_trackers[filename].is_ignored(line):
                return

        self.issues.append(Issue(
            file=filename,
            line=line,
            issue_type=issue_type,
            category=category,
            message=message,
            context=context
        ))

    def _extract_placeholders(self, value: str) -> set[str]:
        tags = set(re.findall(r'\{([^}]+)\}', value))
        return {
            tag for tag in tags
            if tag.lower() not in self.KNOWN_COLOR_TAGS and not tag.startswith("#")
        }

    def _strip_strings_from_line(self, line: str) -> str:
        result = []
        i = 0
        in_string = False

        while i < len(line):
            char = line[i]

            if char == '\\' and in_string and i + 1 < len(line):
                i += 2
                continue
            elif char == '"':
                in_string = not in_string
                i += 1
                continue
            elif not in_string:
                result.append(char)

            i += 1

        return ''.join(result)

    def lint_all(self) -> list[Issue]:
        if self.translations_dir.exists():
            pattern = "cs2kz-*.phrases.txt"
            files = sorted(self.translations_dir.glob(pattern))

            if files:
                print(f"Checking {len(files)} translation files in '{self.translations_dir}'...\n")
                for filepath in files:
                    self.lint_file(filepath)
            else:
                print(f"No files matching '{pattern}' found in '{self.translations_dir}'")
        else:
            print(f"Error: Translations directory '{self.translations_dir}' does not exist.")

        self._collect_global_languages()
        self._check_language_consistency()

        if self.menu_dir.exists():
            self._lint_menu_files()
        else:
            print(f"Warning: Menu directory '{self.menu_dir}' does not exist.")

        return self.issues

    def _collect_global_languages(self) -> None:
        for filename, blocks in self.all_phrase_blocks.items():
            for block in blocks:
                if not block.ignored:
                    self.global_languages.update(block.languages.keys())

    def lint_file(self, filepath: Path) -> None:
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
                lines = content.splitlines()
        except UnicodeDecodeError:
            self.issues.append(Issue(
                file=filepath.name,
                line=0,
                issue_type="error",
                category="Encoding",
                message="File is not valid UTF-8"
            ))
            return
        except Exception as e:
            self.issues.append(Issue(
                file=filepath.name,
                line=0,
                issue_type="error",
                category="File",
                message=f"Could not read file: {e}"
            ))
            return

        self.ignore_trackers[filepath.name] = IgnoreTracker(lines)

        phrase_blocks = self._parse_phrases(filepath.name, lines)
        self.all_phrase_blocks[filepath.name] = phrase_blocks
        self._check_format_issues(filepath.name, lines, phrase_blocks)
        self._check_phrase_blocks(filepath.name, phrase_blocks)

    def _parse_format_spec(self, format_value: str) -> set[str]:
        placeholders = set()
        if not format_value:
            return placeholders

        for part in format_value.split(","):
            part = part.strip()
            if ":" in part:
                name = part.split(":")[0].strip()
                if name:
                    placeholders.add(name)
            elif part:
                placeholders.add(part)

        return placeholders

    def _get_valid_placeholders_for_line(self, line_num: int, phrase_blocks: list[PhraseBlock]) -> set[str]:
        for block in phrase_blocks:
            if block.start_line <= line_num <= block.end_line:
                valid = set(block.placeholders)

                if "en" in block.languages:
                    en_value = block.languages["en"][1]
                    en_placeholders = self._extract_placeholders(en_value)
                    valid.update(en_placeholders)

                return valid
        return set()

    def _check_format_issues(self, filename: str, lines: list[str], phrase_blocks: list[PhraseBlock]) -> None:
        brace_stack = []
        prev_line_empty = False

        for line_num, line in enumerate(lines, 1):
            original_line = line
            stripped = line.strip()

            if any(x in stripped for x in [IgnoreTracker.IGNORE_START,
                                            IgnoreTracker.IGNORE_END,
                                            IgnoreTracker.IGNORE_NEXT]):
                prev_line_empty = False
                continue

            if line != line.rstrip():
                self._add_issue(
                    filename, line_num, "warning", "Formatting",
                    "Line has trailing whitespace"
                )

            if original_line.endswith("\r"):
                self._add_issue(
                    filename, line_num, "warning", "Formatting",
                    "Line has Windows-style (CRLF) line ending"
                )

            if stripped == "":
                if prev_line_empty:
                    self._add_issue(
                        filename, line_num, "warning", "Formatting",
                        "Multiple consecutive empty lines"
                    )
                prev_line_empty = True
            else:
                prev_line_empty = False

            stripped_no_strings = self._strip_strings_from_line(stripped)

            for char in stripped_no_strings:
                if char == '{':
                    brace_stack.append((line_num, '{'))
                elif char == '}':
                    if not brace_stack:
                        self._add_issue(
                            filename, line_num, "error", "Syntax",
                            "Unmatched closing brace '}'",
                            stripped[:60]
                        )
                    else:
                        brace_stack.pop()

            quote_count = stripped.count('"') - stripped.count('\\"')
            if quote_count % 2 != 0:
                self._add_issue(
                    filename, line_num, "error", "Syntax",
                    "Odd number of quotes - possible unclosed string",
                    stripped[:80]
                )

            if re.match(r'^\s*"[^"]+"\s+".*"', stripped):
                valid_placeholders = self._get_valid_placeholders_for_line(line_num, phrase_blocks)

                value_match = re.search(r'^\s*"[^"]+"\s+"(.*)"', stripped)
                if value_match:
                    value = value_match.group(1)
                    tags = re.findall(r'\{([^}]+)\}', value)

                    for tag in tags:
                        tag_lower = tag.lower()
                        if (tag_lower not in self.KNOWN_COLOR_TAGS and
                            tag not in valid_placeholders and
                            not re.match(r'^\d+$', tag) and
                            not tag.startswith("#")):
                            self._add_issue(
                                filename, line_num, "warning", "Format",
                                f"Unknown format tag: {{{tag}}}",
                                stripped[:80]
                            )

        for line_num, brace in brace_stack:
            self._add_issue(
                filename, line_num, "error", "Syntax",
                "Unclosed opening brace '{'"
            )

    def _parse_phrases(self, filename: str, lines: list[str]) -> list[PhraseBlock]:
        phrase_blocks = []
        current_block: Optional[PhraseBlock] = None
        brace_depth = 0
        in_phrases_section = False
        ignore_tracker = self.ignore_trackers.get(filename)

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            if not stripped or stripped.startswith("//"):
                continue

            if stripped == '"Phrases"':
                in_phrases_section = True
                continue

            if stripped == "{":
                brace_depth += 1
                continue

            if stripped == "}":
                brace_depth -= 1
                if current_block and brace_depth == 1:
                    current_block.end_line = line_num
                    if ignore_tracker and ignore_tracker.is_range_ignored(
                            current_block.start_line, current_block.end_line):
                        current_block.ignored = True
                    phrase_blocks.append(current_block)
                    current_block = None
                continue

            if brace_depth == 1 and stripped.startswith('"') and stripped.endswith('"'):
                phrase_name = stripped[1:-1]
                current_block = PhraseBlock(
                    name=phrase_name,
                    start_line=line_num,
                    end_line=line_num
                )
                continue

            if brace_depth == 2 and current_block:
                match = re.match(r'^"([^"]+)"\s+"(.*)"\s*$', stripped)
                if match:
                    key = match.group(1)
                    value = match.group(2)

                    if key == "#format":
                        current_block.format_spec = value
                        current_block.format_line = line_num
                        current_block.placeholders = self._parse_format_spec(value)
                    elif key in self.SPECIAL_DIRECTIVES:
                        pass
                    else:
                        if key in current_block.languages:
                            prev_line = current_block.languages[key][0]
                            self._add_issue(
                                filename, line_num, "error", "Duplicate",
                                f"Duplicate language entry '{key}' in phrase '{current_block.name}'",
                                f"First occurrence at line {prev_line}"
                            )
                        else:
                            current_block.languages[key] = (line_num, value)
                else:
                    if stripped and not stripped.startswith("//"):
                        self._add_issue(
                            filename, line_num, "error", "Format",
                            "Line doesn't match expected format: \"key\" \"value\"",
                            stripped[:80]
                        )

        return phrase_blocks

    def _check_phrase_blocks(self, filename: str, blocks: list[PhraseBlock]) -> None:
        phrase_names = {}

        for block in blocks:
            if block.ignored:
                continue

            if block.name in phrase_names:
                self._add_issue(
                    filename, block.start_line, "error", "Duplicate",
                    f"Duplicate phrase name '{block.name}'",
                    f"First occurrence at line {phrase_names[block.name]}"
                )
            else:
                phrase_names[block.name] = block.start_line

            if "en" not in block.languages:
                self._add_issue(
                    filename, block.start_line, "error", "Missing",
                    f"Missing English ('en') translation for phrase '{block.name}'"
                )

            for lang, (line_num, value) in block.languages.items():
                if not value.strip():
                    self._add_issue(
                        filename, line_num, "warning", "Empty",
                        f"Empty translation for language '{lang}' in phrase '{block.name}'"
                    )

            for lang, (line_num, value) in block.languages.items():
                if value.rstrip().endswith("[...]"):
                    self._add_issue(
                        filename, line_num, "warning", "Truncated",
                        f"Translation appears to be truncated for language '{lang}' in phrase '{block.name}'",
                        value[:60] + "..."
                    )

            if "en" in block.languages:
                en_value = block.languages["en"][1]
                en_placeholders = self._extract_placeholders(en_value)

                for lang, (line_num, value) in block.languages.items():
                    if lang == "en":
                        continue

                    lang_placeholders = self._extract_placeholders(value)
                    missing = en_placeholders - lang_placeholders

                    if missing:
                        self._add_issue(
                            filename, line_num, "error", "Placeholder",
                            f"Missing placeholder(s) {{{', '.join(sorted(missing))}}} in '{lang}' translation for '{block.name}' (present in 'en')"
                        )

    def _check_language_consistency(self) -> None:
        if self.global_languages:
            print(f"Languages found across all files: {sorted(self.global_languages)}\n")

        for filename, blocks in self.all_phrase_blocks.items():
            for block in blocks:
                if block.ignored:
                    continue
                for lang in block.languages:
                    if lang not in self.known_languages:
                        line_num = block.languages[lang][0]
                        self._add_issue(
                            filename, line_num, "error", "Unknown",
                            f"Unknown language code '{lang}' in phrase '{block.name}' (not in config.txt)"
                        )

    # ==================== MENU FILE LINTING ====================

    def _lint_menu_files(self) -> None:
        menu_files = list(self.menu_dir.glob("*.txt"))

        if not menu_files:
            print(f"No menu files found in '{self.menu_dir}'")
            return

        print(f"Checking {len(menu_files)} menu files in '{self.menu_dir}'...\n")

        english_file = self.menu_dir / "english.txt"
        if not english_file.exists():
            self._add_issue(
                "menu/",
                0,
                "error",
                "Missing",
                "Missing english.txt - this is the reference file for menu translations"
            )
            return

        english_groups = self._parse_menu_file(english_file)
        if english_groups is None:
            return

        self.menu_files["english.txt"] = english_groups
        self.menu_languages.add("english")

        for menu_file in sorted(menu_files):
            if menu_file.name == "english.txt":
                continue

            lang_name = menu_file.stem
            self.menu_languages.add(lang_name)

            lang_groups = self._parse_menu_file(menu_file)
            if lang_groups is None:
                continue

            self.menu_files[menu_file.name] = lang_groups
            self._compare_menu_to_english(menu_file.name, lang_groups, english_groups)

        self._check_missing_menu_languages()

    def _parse_menu_file(self, filepath: Path) -> Optional[list[MenuGroup]]:
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
                lines = content.splitlines()
        except UnicodeDecodeError:
            self._add_issue(
                f"menu/{filepath.name}",
                0,
                "error",
                "Encoding",
                "File is not valid UTF-8"
            )
            return None
        except Exception as e:
            self._add_issue(
                f"menu/{filepath.name}",
                0,
                "error",
                "File",
                f"Could not read file: {e}"
            )
            return None

        self.ignore_trackers[f"menu/{filepath.name}"] = IgnoreTracker(lines)
        self._check_menu_format_issues(f"menu/{filepath.name}", lines)

        groups = []
        current_group: Optional[MenuGroup] = None
        current_command: Optional[MenuCommand] = None
        brace_depth = 0
        section_stack = []

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            if not stripped or stripped.startswith("//"):
                continue

            if stripped == "{":
                brace_depth += 1
                continue

            if stripped == "}":
                brace_depth -= 1
                if current_command and brace_depth == 3:
                    current_command.end_line = line_num
                    if current_group:
                        current_group.commands.append(current_command)
                    current_command = None
                elif current_group and brace_depth == 2:
                    current_group.end_line = line_num
                    groups.append(current_group)
                    current_group = None
                elif section_stack and brace_depth == len(section_stack) - 1:
                    section_stack.pop()
                continue

            kv_match = re.match(r'^"([^"]+)"\s+"([^"]*)"', stripped)
            section_match = re.match(r'^"([^"]+)"$', stripped)

            if kv_match:
                key = kv_match.group(1)
                value = kv_match.group(2)

                if current_command:
                    if key == "hotkey":
                        current_command.hotkey = value
                    elif key == "label":
                        current_command.label = value
                    elif key == "cmd":
                        current_command.cmd = value
                elif current_group:
                    if key == "hotkey":
                        current_group.hotkey = value
                    elif key == "title":
                        current_group.title = value
                    elif key == "timeout":
                        current_group.timeout = value

            elif section_match:
                key = section_match.group(1)

                if key == "Groups":
                    section_stack.append("Groups")
                elif key == "Commands":
                    section_stack.append("Commands")
                elif key in ["RadioPanel.txt"]:
                    pass
                elif "Groups" in section_stack and "Commands" not in section_stack and brace_depth == 2:
                    current_group = MenuGroup(
                        name=key,
                        start_line=line_num,
                        end_line=line_num
                    )
                elif "Commands" in section_stack and brace_depth == 4:
                    current_command = MenuCommand(
                        name=key,
                        start_line=line_num,
                        end_line=line_num
                    )

        return groups

    def _check_menu_format_issues(self, filename: str, lines: list[str]) -> None:
        brace_stack = []
        prev_line_empty = False

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            if line != line.rstrip():
                self._add_issue(
                    filename, line_num, "warning", "Formatting",
                    "Line has trailing whitespace"
                )

            if stripped == "":
                if prev_line_empty:
                    self._add_issue(
                        filename, line_num, "warning", "Formatting",
                        "Multiple consecutive empty lines"
                    )
                prev_line_empty = True
            else:
                prev_line_empty = False

            if stripped.startswith("//"):
                continue

            stripped_no_strings = self._strip_strings_from_line(stripped)

            for char in stripped_no_strings:
                if char == '{':
                    brace_stack.append((line_num, '{'))
                elif char == '}':
                    if not brace_stack:
                        self._add_issue(
                            filename, line_num, "error", "Syntax",
                            "Unmatched closing brace '}'",
                            stripped[:60]
                        )
                    else:
                        brace_stack.pop()

        for line_num, brace in brace_stack:
            self._add_issue(
                filename, line_num, "error", "Syntax",
                "Unclosed opening brace '{'"
            )

    def _compare_menu_to_english(self, filename: str, lang_groups: list[MenuGroup],
                                  english_groups: list[MenuGroup]) -> None:
        english_group_map = {g.name: g for g in english_groups}
        lang_group_map = {g.name: g for g in lang_groups}

        for en_group_name, en_group in english_group_map.items():
            if en_group_name not in lang_group_map:
                self._add_issue(
                    f"menu/{filename}",
                    0,
                    "warning",
                    "Missing",
                    f"Missing menu group '{en_group_name}' (exists in english.txt)"
                )
                continue

            lang_group = lang_group_map[en_group_name]

            en_commands = {c.name: c for c in en_group.commands}
            lang_commands = {c.name: c for c in lang_group.commands}

            for en_cmd_name, en_cmd in en_commands.items():
                if en_cmd_name not in lang_commands:
                    self._add_issue(
                        f"menu/{filename}",
                        lang_group.start_line,
                        "warning",
                        "Missing",
                        f"Missing command '{en_cmd_name}' in group '{en_group_name}' (exists in english.txt)"
                    )
                    continue

                lang_cmd = lang_commands[en_cmd_name]

                if en_cmd.label and not lang_cmd.label:
                    self._add_issue(
                        f"menu/{filename}",
                        lang_cmd.start_line,
                        "warning",
                        "Missing",
                        f"Missing label for command '{en_cmd_name}' in group '{en_group_name}'"
                    )

                if en_cmd.hotkey != lang_cmd.hotkey:
                    self._add_issue(
                        f"menu/{filename}",
                        lang_cmd.start_line,
                        "warning",
                        "Mismatch",
                        f"Hotkey mismatch for '{en_cmd_name}': english='{en_cmd.hotkey}', {filename}='{lang_cmd.hotkey}'"
                    )

                if en_cmd.cmd != lang_cmd.cmd:
                    self._add_issue(
                        f"menu/{filename}",
                        lang_cmd.start_line,
                        "warning",
                        "Mismatch",
                        f"Command mismatch for '{en_cmd_name}': english='{en_cmd.cmd}', {filename}='{lang_cmd.cmd}'"
                    )

        for lang_group_name in lang_group_map:
            if lang_group_name not in english_group_map:
                self._add_issue(
                    f"menu/{filename}",
                    lang_group_map[lang_group_name].start_line,
                    "warning",
                    "Extra",
                    f"Extra menu group '{lang_group_name}' not in english.txt"
                )

    def _check_missing_menu_languages(self) -> None:
        code_to_name = {v: k for k, v in self.MENU_LANGUAGE_MAP.items()}

        for lang_code in self.known_languages:
            lang_name = code_to_name.get(lang_code)
            if lang_name and lang_name not in self.menu_languages:
                menu_file = self.menu_dir / f"{lang_name}.txt"
                if not menu_file.exists():
                    self._add_issue(
                        "menu/",
                        0,
                        "warning",
                        "Missing",
                        f"Missing menu translation file: {lang_name}.txt (for language code '{lang_code}')"
                    )

    # ==================== REPORTING ====================

    def print_summary(self) -> None:
        if not self.issues:
            print("No issues found!")
            return

        by_file: dict[str, list[Issue]] = defaultdict(list)
        for issue in self.issues:
            by_file[issue.file].append(issue)

        errors = sum(1 for i in self.issues if i.issue_type == "error")
        warnings = sum(1 for i in self.issues if i.issue_type == "warning")

        print("=" * 70)
        print(f"SUMMARY: {errors} error(s), {warnings} warning(s)")
        print("=" * 70)

        for filename in sorted(by_file.keys()):
            issues = by_file[filename]
            file_errors = sum(1 for i in issues if i.issue_type == "error")
            file_warnings = sum(1 for i in issues if i.issue_type == "warning")
            print(f"\n{filename} ({file_errors} errors, {file_warnings} warnings)")
            print("-" * 50)
            for issue in sorted(issues, key=lambda x: x.line):
                print(f"  {issue}")

        print("\n" + "=" * 70)
        print(f"TOTAL: {errors} error(s), {warnings} warning(s) in {len(by_file)} file(s)")
        print("=" * 70)

    def _collect_missing_data(self) -> tuple[dict, dict, int, int, int]:
        total_missing = 0
        total_phrases = 0
        phrases_with_missing = 0
        missing_per_language: dict[str, int] = defaultdict(int)
        missing_by_file: dict[str, list] = {}

        for filename, blocks in sorted(self.all_phrase_blocks.items()):
            if not blocks:
                continue

            file_missing_report = []

            for block in blocks:
                if block.ignored:
                    continue
                if "en" not in block.languages:
                    continue

                total_phrases += 1
                missing = self.global_languages - set(block.languages.keys())

                if missing:
                    phrases_with_missing += 1
                    sorted_missing = sorted(missing)
                    file_missing_report.append({
                        "phrase": block.name,
                        "line": block.start_line,
                        "missing_languages": sorted_missing,
                        "existing_languages": sorted(block.languages.keys()),
                        "english_text": block.languages.get("en", (0, ""))[1]
                    })

                    for lang in missing:
                        missing_per_language[lang] += 1
                    total_missing += len(missing)

            if file_missing_report:
                missing_by_file[filename] = file_missing_report

        return missing_by_file, dict(missing_per_language), total_missing, total_phrases, phrases_with_missing

    def _collect_missing_menu_data(self) -> dict:
        if "english.txt" not in self.menu_files:
            return {}

        english_groups = self.menu_files["english.txt"]
        missing_data = {}

        code_to_name = {v: k for k, v in self.MENU_LANGUAGE_MAP.items()}

        for lang_code in sorted(self.known_languages):
            lang_name = code_to_name.get(lang_code)
            if not lang_name:
                continue

            menu_file = f"{lang_name}.txt"

            if menu_file not in self.menu_files:
                if not (self.menu_dir / menu_file).exists():
                    missing_data[lang_code] = {
                        "name": lang_name,
                        "file_exists": False,
                        "missing_groups": [g.name for g in english_groups],
                        "missing_commands": []
                    }
            else:
                lang_groups = self.menu_files[menu_file]
                lang_group_map = {g.name: g for g in lang_groups}

                missing_groups = []
                missing_commands = []

                for en_group in english_groups:
                    if en_group.name not in lang_group_map:
                        missing_groups.append(en_group.name)
                    else:
                        lang_group = lang_group_map[en_group.name]
                        lang_cmd_names = {c.name for c in lang_group.commands}

                        for en_cmd in en_group.commands:
                            if en_cmd.name not in lang_cmd_names:
                                missing_commands.append({
                                    "group": en_group.name,
                                    "command": en_cmd.name
                                })

                if missing_groups or missing_commands:
                    missing_data[lang_code] = {
                        "name": lang_name,
                        "file_exists": True,
                        "missing_groups": missing_groups,
                        "missing_commands": missing_commands
                    }

        return missing_data

    def generate_missing_languages_report(self, output_file: Optional[str] = None) -> str:
        output = StringIO()
        sorted_languages = sorted(self.global_languages)

        missing_by_file, missing_per_language, total_missing, total_phrases, phrases_with_missing = self._collect_missing_data()
        missing_menu_data = self._collect_missing_menu_data()

        output.write("=" * 70 + "\n")
        output.write("MISSING TRANSLATIONS REPORT\n")
        output.write(f"Generated for: {self.base_dir}\n")
        output.write("=" * 70 + "\n\n")

        output.write("SUMMARY\n")
        output.write("-" * 70 + "\n")
        output.write(f"Total languages: {len(self.global_languages)}\n")
        output.write(f"Total phrases: {total_phrases}\n")
        output.write(f"Phrases with missing translations: {phrases_with_missing}\n")
        output.write(f"Total missing translation entries: {total_missing}\n\n")

        output.write("LANGUAGE COVERAGE (Phrases)\n")
        output.write("-" * 70 + "\n\n")

        for lang in sorted_languages:
            missing_count = missing_per_language.get(lang, 0)
            coverage = ((total_phrases - missing_count) / total_phrases * 100) if total_phrases > 0 else 0
            bar_length = int(coverage / 5)
            bar = "█" * bar_length + "░" * (20 - bar_length)
            lang_display = self._get_language_display_name(lang)
            output.write(f"  {lang_display:20} [{bar}] {coverage:5.1f}% ({total_phrases - missing_count}/{total_phrases})\n")

        output.write("\n")
        output.write(f"Global languages ({len(sorted_languages)}): {', '.join(sorted_languages)}\n")

        if missing_menu_data or self.menu_files:
            output.write("\n" + "=" * 70 + "\n")
            output.write("MENU TRANSLATION STATUS\n")
            output.write("-" * 70 + "\n\n")

            code_to_name = {v: k for k, v in self.MENU_LANGUAGE_MAP.items()}

            for lang_code in sorted(self.known_languages):
                lang_name = code_to_name.get(lang_code, lang_code)

                if lang_code in missing_menu_data:
                    data = missing_menu_data[lang_code]
                    if not data["file_exists"]:
                        output.write(f"  {lang_code:5} ({lang_name:12}): File missing\n")
                    else:
                        issues = len(data["missing_groups"]) + len(data["missing_commands"])
                        output.write(f"  {lang_code:5} ({lang_name:12}): {issues} missing items\n")
                elif f"{lang_name}.txt" in self.menu_files:
                    output.write(f"  {lang_code:5} ({lang_name:12}): Complete\n")

        output.write("\n" + "=" * 70 + "\n")
        output.write("DETAILED MISSING TRANSLATIONS (Phrases)\n")
        output.write("=" * 70 + "\n")

        for filename, file_report in missing_by_file.items():
            output.write(f"\n{filename}\n")
            output.write("-" * 70 + "\n")

            for item in file_report:
                output.write(f"  Line {item['line']}: \"{item['phrase']}\"\n")
                output.write(f"    Missing ({len(item['missing_languages'])}): {', '.join(item['missing_languages'])}\n")

        if missing_menu_data:
            output.write("\n" + "=" * 70 + "\n")
            output.write("DETAILED MISSING MENU TRANSLATIONS\n")
            output.write("=" * 70 + "\n")

            for lang_code, data in sorted(missing_menu_data.items()):
                output.write(f"\n{data['name']}.txt ({lang_code})\n")
                output.write("-" * 70 + "\n")

                if not data["file_exists"]:
                    output.write(" Entire file is missing\n")
                else:
                    if data["missing_groups"]:
                        output.write(f"  Missing groups: {', '.join(data['missing_groups'])}\n")
                    if data["missing_commands"]:
                        for cmd in data["missing_commands"]:
                            output.write(f"  Missing command: {cmd['command']} (in group {cmd['group']})\n")

        output.write("\n" + "=" * 70 + "\n")

        report_content = output.getvalue()

        if output_file:
            try:
                with open(output_file, "w", encoding="utf-8") as f:
                    f.write(report_content)
                print(f"Missing translations report written to: {output_file}")
            except Exception as e:
                print(f"Failed to write report to {output_file}: {e}")

        return report_content

    def generate_missing_languages_json(self, output_file: Optional[str] = None) -> dict:
        sorted_languages = sorted(self.global_languages)
        missing_by_file, missing_per_language, total_missing, total_phrases, phrases_with_missing = self._collect_missing_data()
        missing_menu_data = self._collect_missing_menu_data()

        languages_data = {}
        for lang in sorted_languages:
            missing_count = missing_per_language.get(lang, 0)
            coverage = ((total_phrases - missing_count) / total_phrases * 100) if total_phrases > 0 else 0

            missing_phrases = []
            for filename, file_report in missing_by_file.items():
                for item in file_report:
                    if lang in item["missing_languages"]:
                        missing_phrases.append({
                            "file": filename,
                            "phrase": item["phrase"],
                            "line": item["line"],
                            "english_text": item["english_text"]
                        })

            languages_data[lang] = {
                "name": self.language_names.get(lang, lang),
                "coverage_percent": round(coverage, 2),
                "translated": total_phrases - missing_count,
                "missing": missing_count,
                "missing_phrases": missing_phrases,
                "menu": missing_menu_data.get(lang, {"file_exists": True, "missing_groups": [], "missing_commands": []})
            }

        files_data = {}
        for filename, file_report in missing_by_file.items():
            files_data[filename] = {
                "phrases_with_missing": len(file_report),
                "phrases": [
                    {
                        "name": item["phrase"],
                        "line": item["line"],
                        "english_text": item["english_text"],
                        "existing_languages": item["existing_languages"],
                        "missing_languages": item["missing_languages"]
                    }
                    for item in file_report
                ]
            }

        json_data = {
            "summary": {
                "total_languages": len(self.global_languages),
                "total_phrases": total_phrases,
                "phrases_with_missing": phrases_with_missing,
                "total_missing_entries": total_missing,
                "all_languages": sorted_languages,
                "known_languages_from_config": sorted(self.known_languages)
            },
            "languages": languages_data,
            "files": files_data,
            "menu": {
                "languages_with_menu": sorted(self.menu_languages),
                "missing": missing_menu_data
            }
        }

        if output_file:
            try:
                with open(output_file, "w", encoding="utf-8") as f:
                    json.dump(json_data, f, indent=2, ensure_ascii=False)
                print(f"Missing translations JSON written to: {output_file}")
            except Exception as e:
                print(f"Failed to write JSON to {output_file}: {e}")

        return json_data


def main():
    base_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    
    linter = TranslationLinter(base_dir)
    issues = linter.lint_all()
    linter.print_summary()

    linter.generate_missing_languages_report("missing-translations.txt")
    linter.generate_missing_languages_json("missing-translations.json")

    errors = sum(1 for i in issues if i.issue_type == "error")
    sys.exit(1 if errors > 0 else 0)


if __name__ == "__main__":
    main()
