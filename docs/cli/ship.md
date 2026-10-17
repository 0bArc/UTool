# Ship

`utool mod ship` builds every catalog row with `"ship": true`, then can upload the zips to an existing Nexus mod page. It does not create the Nexus page.

## Catalog

```json
"catalog": [
  {
    "id": "morexp",
    "path": "progression/morexp",
    "category": "progression",
    "ship": true
  }
]
```

`path` is relative to the mods directory (`examples/` when that folder exists). `id` is what `-f` and `utool mod link` use. `"ship": false` leaves the mod out of the weekly run. `utool mod build <folder>` still builds it.

`utool mod new` can add this row. The catalog id is the folder name, not the number in the Nexus URL.

## Week

The week is the public version in the filename (`MoreXP-Week-12_2x.zip`). Put it in `.env` at the repo root:

```text
NEXUSMODS_API_KEY=your-personal-api-key
UTOOL_UPDATE_VERSION=12
```

Copy from [`.env-example`](../../.env-example). Do not commit `.env`.

```powershell
utool mod ship 12
utool mod ship 12 --dry-run
```

The week is written through as `updateVersion` for that run, so `%updateversion%` in `mod.lua` matches the zip names. `--dry-run` builds and prints the upload plan. It does not upload and it does not write `lastShippedWeek`.

If you omit the week, it comes from `UTOOL_UPDATE_VERSION` or from `updateVersion` in `utool.json`.

## Link a Nexus page

Create the page on the site first. The id is the number in `https://www.nexusmods.com/icarus/mods/340`.

```powershell
utool mod link -f morexp 340
```

That sets `modId` on the matching `nexus.files` rows. `fileId` is the v3 id of a file that already exists on that page. After the page has a file:

```powershell
utool mod ship sync
```

`sync` reads the API and writes `fileId` into `utool.json`. `--upload` also tries to fill `fileId` when `modId` is set and `fileId` is still 0. If that resolve fails, the message says to run `utool mod ship sync`. The first file on a brand new page is uploaded in the browser once, then sync, then later weeks go through `--upload`.

```json
"nexus": {
  "gameDomain": "icarus",
  "apiKeyEnv": "NEXUSMODS_API_KEY",
  "files": [
    {
      "id": "morexp-2x",
      "modId": 340,
      "fileId": 0,
      "match": "MoreXP-Week-*_2x.zip",
      "displayName": "MoreXP-Week-%updateversion%_2x.zip",
      "category": "main",
      "archiveExisting": true
    }
  ]
}
```

`match` picks the built zip. `archiveExisting` replaces the old file on that slot instead of adding another one. One entry per tier, because each zip name is a different file on the page.

`apiKeyEnv` defaults to `NEXUSMODS_API_KEY` when omitted.

## Upload

```powershell
utool mod ship 12 --upload
```

Upload runs only when all of these hold:

1. `utool.json` has a `nexus.files` array. An empty list fails with `utool.json has no nexus.files mapping`.
2. `NEXUSMODS_API_KEY` (or `apiKeyEnv`) is set. Missing key fails with `Missing API key`.
3. `UTOOL_UPDATE_VERSION` is set and equals the week on the command line. A mismatch prints both numbers and does not upload.
4. That week is not already `lastShippedWeek`, unless you pass `--single`.

A full upload that succeeds with no failed file writes `lastShippedWeek`. Bump `UTOOL_UPDATE_VERSION` before the next week. A mix of success and failure does not write `lastShippedWeek`. The message is `Partial upload (...); lastShippedWeek not updated.`

Same week, one mod:

```powershell
utool mod ship 12 -f morexp --upload --single
```

`--single`, `-single`, and `--force` are the same override. They print `WARNING: --single re-upload override`. `-f` does not change `lastShippedWeek` even when the upload works. The message is `Upload ok for -f match; lastShippedWeek unchanged`.

Skipped rows:

| Line | Meaning |
|------|---------|
| `Skip <id>: missing modId/fileId` | Link the page, or sync so `fileId` is filled |
| `Skip <id>: no zip matched <glob>` | The build did not produce a zip that fits `match`. Fix the `:zip` name or the glob |

`utool mod ship` with no `--upload` still builds. It ends with `Ship builds done. Pass --upload to push Nexus files.`
