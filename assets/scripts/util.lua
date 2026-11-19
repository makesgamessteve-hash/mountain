-- Mountain: small helpers shared by the scripts. MIT License.
local util = {}

function util.serialize(t)
  local out = {"{"}
  for k, v in pairs(t) do
    local key = string.format("[%q]", tostring(k))
    local val
    if type(v) == "number" or type(v) == "boolean" then val = tostring(v)
    elseif type(v) == "string" then val = string.format("%q", v) end
    if val then out[#out + 1] = key .. "=" .. val .. "," end
  end
  out[#out + 1] = "}"
  return table.concat(out)
end

function util.deserialize(s)
  if not s then return nil end
  local f = load("return " .. s, "settings", "t", {})
  if not f then return nil end
  local ok, t = pcall(f)
  if ok and type(t) == "table" then return t end
  return nil
end

function util.rgba(r, g, b, a)
  return {r, g, b, a or 1}
end

function util.clock_string(h)
  local hh = math.floor(h)
  local mm = math.floor((h - hh) * 60)
  return string.format("%02d:%02d", hh, mm)
end

function util.capitalize(s)
  return (s:gsub("^%l", string.upper))
end

return util
