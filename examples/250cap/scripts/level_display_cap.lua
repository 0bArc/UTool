-- Raise visible level cap only. Vanilla MaxLevel stays 1000 so orbital XP
-- does not hard-cap progression at 250.
utool.patch_asset("D_CharacterGrowth.json", function()
  utool.editor:SetOnArrayElementsWhere("/Rows", "Name", "Player", "/MaxDisplayLevel", 250)
end)
