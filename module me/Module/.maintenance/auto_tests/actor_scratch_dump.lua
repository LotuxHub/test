-- Dump every script-ish descendant: path + source (first 60 lines). Write via print.
local function fullname(o)
  local n = o.Name
  local pr = o.Parent
  while pr and pr.ClassName ~= "DataModel" do
    n = tostring(pr.Name) .. "." .. n
    pr = pr.Parent
  end
  return n
end
local function isScript(o)
  return o:IsA("LocalScript") or o:IsA("Script") or o:IsA("ModuleScript")
end
local count = 0
print("[DUMP] start")
for _, o in ipairs(game:GetDescendants()) do
  if isScript(o) then
    count = count + 1
    if count <= 80 then
      print("[DUMP] #" .. count .. " path=" .. fullname(o) .. " srcLen=" .. tostring(#o.Source))
      local i = 0
      for ln in (tostring(o.Source)):gmatch("[^\n]+") do
        i = i + 1
        if i <= 60 then
          print("[DUMP]   " .. ln:sub(1, 130))
        end
      end
      if i > 60 then
        print("[DUMP]   ... (" .. (i - 60) .. " more lines)")
      end
    end
  end
end
print("[DUMP] total scripts=" .. tostring(count) .. " done")
