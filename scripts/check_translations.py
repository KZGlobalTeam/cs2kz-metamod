#!/usr/bin/env python3
"""
Translation file linter for cs2kz-metamod phrase files.
Checks for format issues and missing/duplicate language entries in cs2kz-*.phrases.txt files.

Usage:
    python check_translations.py [translations_directory]

If no directory is specified, defaults to ./translations

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

import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional
from collections import defaultdict


@dataclass
class Issue:
    """Represents a single issue found in a translation file."""
    file: str
    line: int
    issue_type: str  # 'error' or 'warning'
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
    """Represents a parsed phrase block with its translations."""
    name: str
    start_line: int
    end_line: int
    languages: dict = field(default_factory=dict)  # lang_code -> (line_number, value)
    format_spec: Optional[str] = None  # The #format directive value
    format_line: int = 0  # Line number of #format directive
    placeholders: set = field(default_factory=set)  # Parsed placeholder names from #format
    ignored: bool = False  # Whether this block is ignored by linter


class IgnoreTracker:
    """Tracks which lines should be ignored based on lint-ignore comments."""

    IGNORE_START = "lint-ignore-start"
    IGNORE_END = "lint-ignore-end"
    IGNORE_NEXT = "lint-ignore-next"
    IGNORE_LINE = "lint-ignore"

    def __init__(self, lines: list[str]):
        self.ignored_lines: set[int] = set()
        self._parse_ignore_comments(lines)

    def _parse_ignore_comments(self, lines: list[str]) -> None:
        """Parse all lines and determine which should be ignored."""
        in_ignored_block = False
        ignore_next = False

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Check for block ignore comments
            if self.IGNORE_START in stripped:
                in_ignored_block = True
                self.ignored_lines.add(line_num)
                continue

            if self.IGNORE_END in stripped:
                in_ignored_block = False
                self.ignored_lines.add(line_num)
                continue

            # Check for ignore-next comment
            if self.IGNORE_NEXT in stripped and self.IGNORE_START not in stripped:
                ignore_next = True
                self.ignored_lines.add(line_num)
                continue

            # If we're in an ignored block, ignore this line
            if in_ignored_block:
                self.ignored_lines.add(line_num)
                continue

            # If previous line was ignore-next, ignore this line
            if ignore_next:
                self.ignored_lines.add(line_num)
                ignore_next = False
                continue

            # Check for inline ignore comment
            if self.IGNORE_LINE in stripped:
                comment_part = stripped.split("//")[-1] if "//" in stripped else ""
                if self.IGNORE_LINE in comment_part:
                    if (self.IGNORE_START not in comment_part and
                        self.IGNORE_END not in comment_part and
                        self.IGNORE_NEXT not in comment_part):
                        self.ignored_lines.add(line_num)

    def is_ignored(self, line_num: int) -> bool:
        """Check if a line number should be ignored."""
        return line_num in self.ignored_lines

    def is_range_ignored(self, start_line: int, end_line: int) -> bool:
        """Check if an entire range of lines is ignored."""
        return all(self.is_ignored(ln) for ln in range(start_line, end_line + 1))


class TranslationLinter:
    """Linter for cs2kz translation phrase files."""

    # Known language codes used in the project
    KNOWN_LANGUAGES = {
        "en", "chi", "ua", "sv", "fi", "ko", "lv", "de", "ru", "pl",
        "pt", "es", "fr", "it", "nl", "tr", "hu", "cs", "da", "no",
        "jp", "tw", "br", "bg", "ro", "el", "th", "vi", "id", "ms"
    }

    # Known color/formatting tags
    KNOWN_COLOR_TAGS = {
        "yellow", "grey", "gray", "default", "green", "red", "blue",
        "white", "orange", "purple", "lime", "lightgreen", "darkgreen",
        "lightblue", "darkblue", "pink", "lightred", "darkred", "olive",
        "bluegrey", "normal", "unusual", "vintage", "unique", "community",
        "developer", "haunted", "gold"
    }

    # Special directives that are not language codes
    SPECIAL_DIRECTIVES = {"#format"}

    def __init__(self, translations_dir: str = "./translations"):
        self.translations_dir = Path(translations_dir)
        self.issues: list[Issue] = []
        self.all_phrase_blocks: dict[str, list[PhraseBlock]] = {}
        self.ignore_trackers: dict[str, IgnoreTracker] = {}

    def _add_issue(self, filename: str, line: int, issue_type: str,
                   category: str, message: str, context: Optional[str] = None) -> None:
        """Add an issue if the line is not ignored."""
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
        """Extract placeholder names from a translation value, excluding color tags."""
        tags = set(re.findall(r'\{([^}]+)\}', value))
        return {
            tag for tag in tags
            if tag.lower() not in self.KNOWN_COLOR_TAGS and not tag.startswith("#")
        }

    def lint_all(self) -> list[Issue]:
        """Lint all cs2kz-*.phrases.txt files in the translations directory."""
        if not self.translations_dir.exists():
            print(f"Error: Directory '{self.translations_dir}' does not exist.")
            sys.exit(1)

        pattern = "cs2kz-*.phrases.txt"
        files = sorted(self.translations_dir.glob(pattern))

        if not files:
            print(f"No files matching '{pattern}' found in '{self.translations_dir}'")
            sys.exit(1)

        print(f"Checking {len(files)} translation files in '{self.translations_dir}'...\n")

        for filepath in files:
            self.lint_file(filepath)

        # Cross-file analysis
        self._check_language_consistency()

        return self.issues

    def lint_file(self, filepath: Path) -> None:
        """Lint a single translation file."""
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

        # Initialize ignore tracker for this file
        self.ignore_trackers[filepath.name] = IgnoreTracker(lines)

        # Parse and check the file
        phrase_blocks = self._parse_phrases(filepath.name, lines)
        self.all_phrase_blocks[filepath.name] = phrase_blocks
        self._check_format_issues(filepath.name, lines, phrase_blocks)
        self._check_phrase_blocks(filepath.name, phrase_blocks)

    def _parse_format_spec(self, format_value: str) -> set[str]:
        """Parse a #format directive and extract placeholder names.

        Format: "name1:type,name2:type,..."
        Example: "api:s,server:s,map:s" -> {"api", "server", "map"}
        """
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

    def _get_placeholders_for_line(self, line_num: int, phrase_blocks: list[PhraseBlock]) -> set[str]:
        """Get the valid placeholders for a given line number based on its phrase block."""
        for block in phrase_blocks:
            if block.start_line <= line_num <= block.end_line:
                return block.placeholders
        return set()

    def _check_format_issues(self, filename: str, lines: list[str], phrase_blocks: list[PhraseBlock]) -> None:
        """Check for general formatting issues."""
        brace_stack = []
        in_string = False
        prev_line_empty = False

        for line_num, line in enumerate(lines, 1):
            original_line = line
            stripped = line.strip()

            # Skip lint-ignore comment lines for most checks
            if any(x in stripped for x in [IgnoreTracker.IGNORE_START,
                                            IgnoreTracker.IGNORE_END,
                                            IgnoreTracker.IGNORE_NEXT]):
                prev_line_empty = False
                continue

            # Check for trailing whitespace
            if line != line.rstrip():
                self._add_issue(
                    filename, line_num, "warning", "Formatting",
                    "Line has trailing whitespace"
                )

            # Check for CRLF line endings
            if original_line.endswith("\r"):
                self._add_issue(
                    filename, line_num, "warning", "Formatting",
                    "Line has Windows-style (CRLF) line ending"
                )

            # Check for multiple consecutive empty lines
            if stripped == "":
                if prev_line_empty:
                    self._add_issue(
                        filename, line_num, "warning", "Formatting",
                        "Multiple consecutive empty lines"
                    )
                prev_line_empty = True
            else:
                prev_line_empty = False

            # Track brace matching
            for i, char in enumerate(stripped):
                if char == '"':
                    in_string = not in_string
                elif not in_string:
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

            # Check for unclosed strings on a line
            quote_count = stripped.count('"')
            escaped_quotes = stripped.count('\\"')
            effective_quotes = quote_count - escaped_quotes
            if effective_quotes % 2 != 0:
                self._add_issue(
                    filename, line_num, "warning", "Syntax",
                    "Odd number of quotes - possible unclosed string",
                    stripped[:80]
                )

            # Check for format tags in translation lines
            if re.match(r'^\s*"[^"]+"\s+".*"', stripped):
                valid_placeholders = self._get_placeholders_for_line(line_num, phrase_blocks)

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

        # Check for unclosed braces at end of file
        for line_num, brace in brace_stack:
            self._add_issue(
                filename, line_num, "error", "Syntax",
                "Unclosed opening brace '{'"
            )

    def _parse_phrases(self, filename: str, lines: list[str]) -> list[PhraseBlock]:
        """Parse the file and extract phrase blocks."""
        phrase_blocks = []
        current_block: Optional[PhraseBlock] = None
        brace_depth = 0
        in_phrases_section = False
        ignore_tracker = self.ignore_trackers.get(filename)

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Skip empty lines and comments
            if not stripped or stripped.startswith("//"):
                continue

            # Track "Phrases" section
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

            # Parse phrase name (at depth 1)
            if brace_depth == 1 and stripped.startswith('"') and stripped.endswith('"'):
                phrase_name = stripped[1:-1]
                current_block = PhraseBlock(
                    name=phrase_name,
                    start_line=line_num,
                    end_line=line_num
                )
                continue

            # Parse entries inside phrase block (at depth 2)
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
                            filename, line_num, "warning", "Format",
                            "Line doesn't match expected format: \"key\" \"value\"",
                            stripped[:80]
                        )

        return phrase_blocks

    def _check_phrase_blocks(self, filename: str, blocks: list[PhraseBlock]) -> None:
        """Check phrase blocks for issues."""
        phrase_names = {}

        for block in blocks:
            # Skip entirely ignored blocks
            if block.ignored:
                continue

            # Check for duplicate phrase names
            if block.name in phrase_names:
                self._add_issue(
                    filename, block.start_line, "error", "Duplicate",
                    f"Duplicate phrase name '{block.name}'",
                    f"First occurrence at line {phrase_names[block.name]}"
                )
            else:
                phrase_names[block.name] = block.start_line

            # Check for missing English translation
            if "en" not in block.languages:
                self._add_issue(
                    filename, block.start_line, "error", "Missing",
                    f"Missing English ('en') translation for phrase '{block.name}'"
                )

            # Check for empty translations
            for lang, (line_num, value) in block.languages.items():
                if not value.strip():
                    self._add_issue(
                        filename, line_num, "warning", "Empty",
                        f"Empty translation for language '{lang}' in phrase '{block.name}'"
                    )

            # Check for truncated translations
            for lang, (line_num, value) in block.languages.items():
                if value.rstrip().endswith("[...]"):
                    self._add_issue(
                        filename, line_num, "warning", "Truncated",
                        f"Translation appears to be truncated for language '{lang}' in phrase '{block.name}'",
                        value[:60] + "..."
                    )

            # Check placeholder consistency against English translation (not #format)
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
                            filename, line_num, "warning", "Placeholder",
                            f"Missing placeholder(s) {{{', '.join(sorted(missing))}}} in '{lang}' translation for '{block.name}' (present in 'en')"
                        )

    def _check_language_consistency(self) -> None:
        """Check for language consistency across all files."""
        all_languages: set[str] = set()
        file_languages: dict[str, set[str]] = {}

        for filename, blocks in self.all_phrase_blocks.items():
            file_langs = set()
            for block in blocks:
                if not block.ignored:
                    file_langs.update(block.languages.keys())
            file_languages[filename] = file_langs
            all_languages.update(file_langs)

        print(f"Languages found across all files: {sorted(all_languages)}\n")

        # Check for unknown language codes
        for filename, blocks in self.all_phrase_blocks.items():
            for block in blocks:
                if block.ignored:
                    continue
                for lang in block.languages:
                    if lang not in self.KNOWN_LANGUAGES:
                        line_num = block.languages[lang][0]
                        self._add_issue(
                            filename, line_num, "warning", "Unknown",
                            f"Unknown language code '{lang}' in phrase '{block.name}'"
                        )

    def print_summary(self) -> None:
        """Print a summary of all issues found."""
        if not self.issues:
            print("✅ No issues found!")
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
            print(f"\n📄 {filename} ({file_errors} errors, {file_warnings} warnings)")
            print("-" * 50)
            for issue in sorted(issues, key=lambda x: x.line):
                print(f"  {issue}")

        print("\n" + "=" * 70)
        print(f"TOTAL: {errors} error(s), {warnings} warning(s) in {len(by_file)} file(s)")
        print("=" * 70)

    def generate_missing_languages_report(self) -> None:
        """Generate a report of missing language translations."""
        print("\n" + "=" * 70)
        print("MISSING TRANSLATIONS REPORT")
        print("=" * 70)

        for filename, blocks in sorted(self.all_phrase_blocks.items()):
            if not blocks:
                continue

            file_langs = set()
            for block in blocks:
                if not block.ignored:
                    file_langs.update(block.languages.keys())

            missing_report = []
            for block in blocks:
                if block.ignored:
                    continue
                missing = file_langs - set(block.languages.keys())
                if missing and "en" in block.languages:
                    missing_report.append((block.name, block.start_line, sorted(missing)))

            if missing_report:
                print(f"\n📄 {filename}")
                print("-" * 50)
                for phrase_name, line_num, missing_langs in missing_report:
                    print(f"  Line {line_num}: '{phrase_name}'")
                    print(f"    Missing: {', '.join(missing_langs)}")


def main():
    """Main entry point."""
    translations_dir = sys.argv[1] if len(sys.argv) > 1 else "./translations"

    linter = TranslationLinter(translations_dir)
    issues = linter.lint_all()
    linter.print_summary()
    linter.generate_missing_languages_report()

    errors = sum(1 for i in issues if i.issue_type == "error")
    sys.exit(1 if errors > 0 else 0)


if __name__ == "__main__":
    main()
