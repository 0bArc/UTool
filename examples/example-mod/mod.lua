-- Example mod. Workflow for new changes:
-- 1) Load your game in UTool Studio
-- 2) Search Assets for what you want (e.g. "health", "xp")
-- 3) Preview → Insert snippet → edit :set / :map → Save → Build
-- See docs/04-find-assets.md

utool.mod {
  id = "example.mod",
  name = "Example Mod",
  version = "0.1.1",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/example_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    zip = true,
  },
}

-- After Insert snippet from Preview, it looks like:
-- utool.asset("SomeAsset.json")
--   :row("SomeRow")
--   :field("SomeField")
--   :set(123)
