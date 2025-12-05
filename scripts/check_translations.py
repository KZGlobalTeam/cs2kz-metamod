#!/usr/bin/env python3
"""
Translation file linter for cs2kz-metamod phrase files.
Checks for format issues and missing/duplicate language entries in cs2kz-*.phrases.txt files.

Usage:
    python check_translations.py [translations_directory]

If no directory is specified, defaults to ./translations
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
        self.all_phrase_blocks: dict[str, list[PhraseBlock]] = {}  # file -> blocks

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
                # Handle format without type specifier
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

            # Check for trailing whitespace
            if line != line.rstrip():
                self.issues.append(Issue(
                    file=filename,
                    line=line_num,
                    issue_type="warning",
                    category="Formatting",
                    message="Line has trailing whitespace"
                ))

            # Check for CRLF line endings (if we can detect)
            if original_line.endswith("\r"):
                self.issues.append(Issue(
                    file=filename,
                    line=line_num,
                    issue_type="warning",
                    category="Formatting",
                    message="Line has Windows-style (CRLF) line ending"
                ))

            # Check for multiple consecutive empty lines
            if stripped == "":
                if prev_line_empty:
                    self.issues.append(Issue(
                        file=filename,
                        line=line_num,
                        issue_type="warning",
                        category="Formatting",
                        message="Multiple consecutive empty lines"
                    ))
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
                            self.issues.append(Issue(
                                file=filename,
                                line=line_num,
                                issue_type="error",
                                category="Syntax",
                                message="Unmatched closing brace '}'",
                                context=stripped[:60]
                            ))
                        else:
                            brace_stack.pop()

            # Check for unclosed strings on a line (basic check)
            quote_count = stripped.count('"')
            # Account for escaped quotes
            escaped_quotes = stripped.count('\\"')
            effective_quotes = quote_count - escaped_quotes
            if effective_quotes % 2 != 0:
                self.issues.append(Issue(
                    file=filename,
                    line=line_num,
                    issue_type="warning",
                    category="Syntax",
                    message="Odd number of quotes - possible unclosed string",
                    context=stripped[:80]
                ))

            # Check for format tags in translation lines
            # Only check lines that look like translations (have quoted content)
            if re.match(r'^\s*"[^"]+"\s+".*"', stripped):
                # Get valid placeholders for this line's phrase block
                valid_placeholders = self._get_placeholders_for_line(line_num, phrase_blocks)

                # Extract the value part (second quoted string)
                value_match = re.search(r'^\s*"[^"]+"\s+"(.*)"', stripped)
                if value_match:
                    value = value_match.group(1)
                    tags = re.findall(r'\{([^}]+)\}', value)

                    for tag in tags:
                        tag_lower = tag.lower()
                        # Check if it's a known color tag, a valid placeholder, or a numeric placeholder
                        if (tag_lower not in self.KNOWN_COLOR_TAGS and
                            tag not in valid_placeholders and
                            not re.match(r'^\d+$', tag) and
                            not tag.startswith("#")):
                            self.issues.append(Issue(
                                file=filename,
                                line=line_num,
                                issue_type="warning",
                                category="Format",
                                message=f"Unknown format tag: {{{tag}}}",
                                context=stripped[:80]
                            ))

        # Check for unclosed braces at end of file
        for line_num, brace in brace_stack:
            self.issues.append(Issue(
                file=filename,
                line=line_num,
                issue_type="error",
                category="Syntax",
                message="Unclosed opening brace '{'",
            ))

    def _parse_phrases(self, filename: str, lines: list[str]) -> list[PhraseBlock]:
        """Parse the file and extract phrase blocks."""
        phrase_blocks = []
        current_block: Optional[PhraseBlock] = None
        brace_depth = 0
        in_phrases_section = False

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

                    # Check if this is a #format directive
                    if key == "#format":
                        current_block.format_spec = value
                        current_block.format_line = line_num
                        current_block.placeholders = self._parse_format_spec(value)
                    elif key in self.SPECIAL_DIRECTIVES:
                        # Other special directives - just skip
                        pass
                    else:
                        # It's a language entry
                        if key in current_block.languages:
                            # Duplicate language entry
                            prev_line = current_block.languages[key][0]
                            self.issues.append(Issue(
                                file=filename,
                                line=line_num,
                                issue_type="error",
                                category="Duplicate",
                                message=f"Duplicate language entry '{key}' in phrase '{current_block.name}'",
                                context=f"First occurrence at line {prev_line}"
                            ))
                        else:
                            current_block.languages[key] = (line_num, value)
                else:
                    # Line doesn't match expected format
                    if stripped and not stripped.startswith("//"):
                        self.issues.append(Issue(
                            file=filename,
                            line=line_num,
                            issue_type="warning",
                            category="Format",
                            message="Line doesn't match expected format: \"key\" \"value\"",
                            context=stripped[:80]
                        ))

        return phrase_blocks

    def _check_phrase_blocks(self, filename: str, blocks: list[PhraseBlock]) -> None:
        """Check phrase blocks for issues."""
        phrase_names = {}

        for block in blocks:
            # Check for duplicate phrase names
            if block.name in phrase_names:
                self.issues.append(Issue(
                    file=filename,
                    line=block.start_line,
                    issue_type="error",
                    category="Duplicate",
                    message=f"Duplicate phrase name '{block.name}'",
                    context=f"First occurrence at line {phrase_names[block.name]}"
                ))
            else:
                phrase_names[block.name] = block.start_line

            # Check for missing English translation (should always exist)
            if "en" not in block.languages:
                self.issues.append(Issue(
                    file=filename,
                    line=block.start_line,
                    issue_type="error",
                    category="Missing",
                    message=f"Missing English ('en') translation for phrase '{block.name}'"
                ))

            # Check for empty translations
            for lang, (line_num, value) in block.languages.items():
                if not value.strip():
                    self.issues.append(Issue(
                        file=filename,
                        line=line_num,
                        issue_type="warning",
                        category="Empty",
                        message=f"Empty translation for language '{lang}' in phrase '{block.name}'"
                    ))

            # Check for truncated translations (ending with [...])
            for lang, (line_num, value) in block.languages.items():
                if value.rstrip().endswith("[...]"):
                    self.issues.append(Issue(
                        file=filename,
                        line=line_num,
                        issue_type="warning",
                        category="Truncated",
                        message=f"Translation appears to be truncated for language '{lang}' in phrase '{block.name}'",
                        context=value[:60] + "..."
                    ))

            # Check placeholder consistency between translations
            if block.format_spec and "en" in block.languages:
                expected_placeholders = block.placeholders

                for lang, (line_num, value) in block.languages.items():
                    # Extract placeholders used in this translation
                    used_tags = set(re.findall(r'\{([^}]+)\}', value))
                    # Filter to only placeholders (not color tags)
                    used_placeholders = {
                        tag for tag in used_tags
                        if tag.lower() not in self.KNOWN_COLOR_TAGS and not tag.startswith("#")
                    }

                    missing = expected_placeholders - used_placeholders
                    extra = used_placeholders - expected_placeholders

                    # Only warn about missing placeholders (extra might be intentional)
                    if missing:
                        self.issues.append(Issue(
                            file=filename,
                            line=line_num,
                            issue_type="warning",
                            category="Placeholder",
                            message=f"Missing placeholder(s) {{{', '.join(sorted(missing))}}} in '{lang}' translation for '{block.name}'",
                        ))

    def _check_language_consistency(self) -> None:
        """Check for language consistency across all files."""
        # Collect all languages used across all files
        all_languages: set[str] = set()
        file_languages: dict[str, set[str]] = {}

        for filename, blocks in self.all_phrase_blocks.items():
            file_langs = set()
            for block in blocks:
                file_langs.update(block.languages.keys())
            file_languages[filename] = file_langs
            all_languages.update(file_langs)

        # Report summary of languages found
        print(f"Languages found across all files: {sorted(all_languages)}\n")

        # Check for unknown language codes
        for filename, blocks in self.all_phrase_blocks.items():
            for block in blocks:
                for lang in block.languages:
                    if lang not in self.KNOWN_LANGUAGES:
                        line_num = block.languages[lang][0]
                        self.issues.append(Issue(
                            file=filename,
                            line=line_num,
                            issue_type="warning",
                            category="Unknown",
                            message=f"Unknown language code '{lang}' in phrase '{block.name}'"
                        ))

    def print_summary(self) -> None:
        """Print a summary of all issues found."""
        if not self.issues:
            print("✅ No issues found!")
            return

        # Group issues by file
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

        # For each file, check which phrases are missing which languages
        for filename, blocks in sorted(self.all_phrase_blocks.items()):
            if not blocks:
                continue

            # Get all languages used in this file
            file_langs = set()
            for block in blocks:
                file_langs.update(block.languages.keys())

            # Check each block for missing languages
            missing_report = []
            for block in blocks:
                missing = file_langs - set(block.languages.keys())
                if missing and "en" in block.languages:  # Only report if has English
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

    # Exit with error code if there are errors
    errors = sum(1 for i in issues if i.issue_type == "error")
    sys.exit(1 if errors > 0 else 0)


if __name__ == "__main__":
    main()
