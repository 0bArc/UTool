# Pipelines

Registry of existing UTool Lua mods for Pak Studio. This folder does **not** invent a new mod format — it points at `examples/*/mod.lua` (and any other allowed roots).

## manifest.json

```json
{
  "pipelines": [
    { "id": "250cap", "path": "../../../examples/250cap" }
  ]
}
```

Paths are relative to this `pipelines/` directory.

Studio also auto-discovers `examples/*/mod.lua` under the repo root so unlisted examples still appear.

## API

- `GET /api/pipelines` — list pipelines
- `GET /api/pipelines/file?id=&path=` — read a file under a pipeline root
- `PUT /api/pipelines/file` — write `{ id, path, content }` (sandboxed)

Build uses existing CLI: `utool pak build-mod <mod-dir>`.
