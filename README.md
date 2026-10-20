# Translation Reports

Automated translation linter reports for cs2kz-metamod.

## Status

| Metric | Value |
|--------|-------|
| **Linter Errors** | 0 |
| **Linter Warnings** | 24 |
| **Total Languages** | 15 |
| **Total Phrases** | 1092 |
| **Phrases Missing Translations** | 1092 |
| **Total Missing Entries** | 7522 |
| **Menu Files** | 11 |

## Language Coverage (Phrases)

```
  chi (schinese)       [██████████████████░░]  92.6% (1011/1092)
  de (german)          [█████████░░░░░░░░░░░]  47.4% (518/1092)
  en (english)         [████████████████████] 100.0% (1092/1092)
  es (spanish)         [████████░░░░░░░░░░░░]  44.3% (484/1092)
  fi (finnish)         [████░░░░░░░░░░░░░░░░]  21.3% (233/1092)
  it (italian)         [████░░░░░░░░░░░░░░░░]  20.6% (225/1092)
  ko (korean)          [███████████░░░░░░░░░]  59.7% (652/1092)
  lv (latvian)         [██████░░░░░░░░░░░░░░]  30.0% (328/1092)
  nl (dutch)           [░░░░░░░░░░░░░░░░░░░░]   0.5% (5/1092)
  pl (polish)          [█████████░░░░░░░░░░░]  47.6% (520/1092)
  ru (russian)         [██████████████████░░]  91.0% (994/1092)
  sv (swedish)         [███████░░░░░░░░░░░░░]  35.9% (392/1092)
  tr (turkish)         [████░░░░░░░░░░░░░░░░]  24.0% (262/1092)
  ua (ukrainian)       [███████████████████░]  98.3% (1073/1092)
  zho (tchinese)       [███████████████████░]  97.9% (1069/1092)
```

## Menu Translation Status

```
  ar    (arabic      ): File missing
  bg    (bulgarian   ): File missing
  chi   (schinese    ): File missing
  cze   (czech       ): File missing
  da    (danish      ): File missing
  de    (german      ): Complete
  el    (greek       ): File missing
  en    (english     ): Complete
  es    (spanish     ): Complete
  fi    (finnish     ): File missing
  fr    (french      ): File missing
  he    (hebrew      ): File missing
  hu    (hungarian   ): File missing
  it    (italian     ): File missing
  jp    (japanese    ): File missing
  ko    (korean      ): Complete
  lt    (lithuanian  ): File missing
  lv    (latvian     ): Complete
  nl    (dutch       ): Complete
  no    (norwegian   ): File missing
  pl    (polish      ): Complete
  pt    (brazilian   ): File missing
  pt_p  (portuguese  ): File missing
  ro    (romanian    ): File missing
  ru    (russian     ): Complete
  sk    (slovak      ): File missing
  sv    (swedish     ): Complete
  th    (thai        ): File missing
  tr    (turkish     ): File missing
  ua    (ukrainian   ): Complete
  vi    (vietnamese  ): File missing
  zho   (tchinese    ): File missing
```

## Reports

| File | Description |
|------|-------------|
| [translation-lint-results.txt](translation-lint-results.txt) | Linter errors and warnings |
| [missing-translations.txt](missing-translations.txt) | Detailed missing translations report |
| [missing-translations.json](missing-translations.json) | Machine-readable JSON export |

## Last Updated

- **Commit:** [`5fe5811`](https://github.com/KZGlobalTeam/cs2kz-metamod/commit/5fe5811f997fbfc64f32859008f286fee68e03f9)
- **Time:** 2026-10-20 18:42:40 UTC
- **Message:** Fix WriteBufferToFile not respecting return values

---

*This report is automatically generated on each push to master.*
