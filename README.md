# Translation Reports

Automated translation linter reports for cs2kz-metamod.

## Status

| Metric | Value |
|--------|-------|
| **Linter Errors** | 0 |
| **Linter Warnings** | 2 |
| **Total Languages** | 15 |
| **Total Phrases** | 1104 |
| **Phrases Missing Translations** | 1104 |
| **Total Missing Entries** | 7664 |
| **Menu Files** | 11 |

## Language Coverage (Phrases)

```
  chi (schinese)       [██████████████████░░]  91.8% (1013/1104)
  de (german)          [█████████░░░░░░░░░░░]  47.1% (520/1104)
  en (english)         [████████████████████] 100.0% (1104/1104)
  es (spanish)         [████████░░░░░░░░░░░░]  44.0% (486/1104)
  fi (finnish)         [████░░░░░░░░░░░░░░░░]  21.3% (235/1104)
  it (italian)         [████░░░░░░░░░░░░░░░░]  20.6% (227/1104)
  ko (korean)          [███████████░░░░░░░░░]  59.2% (654/1104)
  lv (latvian)         [█████░░░░░░░░░░░░░░░]  29.9% (330/1104)
  nl (dutch)           [░░░░░░░░░░░░░░░░░░░░]   0.5% (5/1104)
  pl (polish)          [█████████░░░░░░░░░░░]  47.3% (522/1104)
  ru (russian)         [██████████████████░░]  90.2% (996/1104)
  sv (swedish)         [███████░░░░░░░░░░░░░]  35.7% (394/1104)
  tr (turkish)         [████░░░░░░░░░░░░░░░░]  23.9% (264/1104)
  ua (ukrainian)       [███████████████████░]  97.4% (1075/1104)
  zho (tchinese)       [███████████████████░]  97.0% (1071/1104)
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

- **Commit:** [`dd23ad5`](https://github.com/KZGlobalTeam/cs2kz-metamod/commit/dd23ad5e95b08f8d7479000bc596b3f42f8807a4)
- **Time:** 2026-10-22 07:02:51 UTC
- **Message:** Switch translations check to use improved kv-lint action instead of local script

---

*This report is automatically generated on each push to master.*
