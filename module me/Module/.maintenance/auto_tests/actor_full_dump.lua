-- Full dump: PlayerScripts tree with sources + anything named Actor.
local function p(s) print(s) end
p("[dump] start")
local Players = game:GetService("Players")
local lp = Players.LocalPlayer
local ps = lp:WaitForChild("PlayerScripts", 10)
if not ps then p("[dump] no PlayerScripts") return end

local function fullname(o)
  local n = o.Name
  local pr = o.Parent
  while pr and pr.ClassName ~= "DataModel" do
    n = tostring(pr.Name) .. "." .. n
    pr = pr.Parent
  end
  return n
end

local function dumpTree(root, prefix)
  for _, c in ipairs(root:GetChildren()) do
    local nm = string.lower(c.Name)
    local isScript = c:IsA("LocalScript") or c:IsA("Script") or c:IsA("ModuleScript")
    if isScript or nm:find("actor") or nm:find("music") or nm:find("animate") then
      p("[dump] " .. prefix .. c.Name .. " (class=" .. c.ClassName .. ") path=" .. fullname(c))
      if isScript then
        local ok, src = pcall(function() return c.Source end)
        local s = ok and tostring(src) or ""
        local i = 0
        for ln in s:gmatch("[^\n]+") do
          i = i + 1
          if i <= 40 then p("[dump]   | " .. ln:sub(1, 140)) end
        end
        if i > 40 then p("[dump]   | ... (" .. (i - 40) .. " more lines)") end
      end
    end
    if depth_limit then end
    dumpTree(c, prefix .. "  ")
  end
end
dumpTree(ps, "")

-- Also: list all Instance descendants with actor in name
local d = game:GetDescendants()
for _, v in ipairs(d) do
  if v.Name:lower():find("actor") or v.ClassName == "Actor" then
    p("[dump] descendant: " .. v.Name .. " class=" .. v.ClassName .. " path=" .. fullname(v))
  end
end
p("[dump] done")
