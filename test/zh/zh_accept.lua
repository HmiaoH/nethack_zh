-- NetHack Chinese localization acceptance checks.
-- Run in a ZHLANG wizard-mode game: #wizloadlua, then "zh_accept".
-- test/zh/zh_accept.py drives this automatically and parses the
-- "ZHT|<id>|OK|..." / "ZHT|<id>|FAIL|..." lines.  Check ids match the
-- task ids in doc/zh/TASKS.md.  Do not edit expectations to make a
-- failing build pass; change them only together with TASKS.md.

local results = {}

-- keep detail short and cut on a character boundary so the report line
-- itself never splits a UTF-8 sequence
local function short(s)
   s = tostring(s)
   local ok, n = pcall(utf8.len, s)
   if ok and n and n > 14 then
      s = s:sub(1, utf8.offset(s, 15) - 1) .. "~"
   end
   return s
end

local function T(id, fn)
   local ok, pass, detail = pcall(fn)
   local line
   if not ok then
      line = "ZHT|" .. id .. "|FAIL|err:" .. short(pass)
   elseif pass then
      line = "ZHT|" .. id .. "|OK|"
   else
      line = "ZHT|" .. id .. "|FAIL|" .. short(detail)
   end
   results[#results + 1] = line
end

local function eq(got, want)
   return got == want, "got " .. tostring(got)
end

local function wish(str)
   return obj.new(str):totable()
end

local function fields(str, want)
   local o = wish(str)
   for k, v in pairs(want) do
      if o[k] ~= v then
         return false, k .. "=" .. tostring(o[k])
      end
   end
   return true
end

-- B: English grammar helpers must leave Chinese alone ...
T("B1-plural", function() return eq(nh.makeplural("箭"), "箭") end)
T("B1-singular", function() return eq(nh.makesingular("箭"), "箭") end)
T("B1-possessive", function() return eq(nh.s_suffix("长剑"), "长剑的") end)
T("B1-article", function() return eq(nh.an("长剑"), "长剑") end)
-- ... and still handle English exactly as before
T("B1-en-plural", function() return eq(nh.makeplural("arrow"), "arrows") end)
T("B1-en-possessive", function() return eq(nh.s_suffix("Fred"), "Fred's") end)
T("B1-en-article", function() return eq(nh.an("apple"), "an apple") end)

-- C1: the Lua-facing API reports English identifiers
T("C1-otyp-name", function()
   return eq(wish("arrow").otyp_name, "arrow")
end)
T("C1-int-to-objname", function()
   return eq(nh.int_to_objname(wish("arrow").otyp), "arrow")
end)

-- C2: English wishes keep working in the Chinese build
T("C2-en-wish", function()
   return fields("2 blessed +3 silver daggers",
                 { otyp_name = "silver dagger", quan = 2, blessed = 1,
                   spe = 3 })
end)
T("C2-en-wish-corpse", function()
   return fields("jackal corpse", { otyp_name = "corpse",
                                    corpsenm_name = "jackal" })
end)

-- C3: Chinese wishes
T("C3-zh-wish", function()
   return fields("银匕首", { otyp_name = "silver dagger" })
end)
T("C3-zh-wish-prefix", function()
   return fields("2 祝福的 +3 银匕首",
                 { otyp_name = "silver dagger", quan = 2, blessed = 1,
                   spe = 3 })
end)
T("C3-zh-wish-nospace", function()
   return fields("2祝福的+3银匕首",
                 { otyp_name = "silver dagger", quan = 2, blessed = 1,
                   spe = 3 })
end)
T("C3-zh-wish-cursed", function()
   return fields("诅咒的灰龙鳞甲",
                 { otyp_name = "gray dragon scale mail", cursed = 1 })
end)
T("C3-zh-wish-corpse", function()
   return fields("豺狼尸体", { otyp_name = "corpse",
                               corpsenm_name = "jackal" })
end)
T("C3-zh-wish-corpse-de", function()
   return fields("豺狼的尸体", { otyp_name = "corpse",
                                 corpsenm_name = "jackal" })
end)

-- A2: special-level object mimicry resolves English names; a failure
-- shows up as an impossible() message that the driver looks for
T("A2-appear-as-run", function()
   des.monster({ class = "m", appear_as = "obj:luckstone" })
   return true
end)

-- D1: a long Chinese message with no spaces must wrap without
-- splitting characters; the driver validates the raw tty bytes
T("D1-long-message", function()
   pline(string.rep("中文换行测试", 30))
   return true
end)

local ok = 0
for _, l in ipairs(results) do
   if l:find("|OK|", 1, true) then ok = ok + 1 end
end
results[#results + 1] = "ZHT|SUMMARY|" .. ok .. "/" .. (#results)
results[#results + 1] = "ZHT|END"
nh.text(table.concat(results, "\n"))
