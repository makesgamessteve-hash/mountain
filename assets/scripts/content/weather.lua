-- Mountain: how likely each weather is to come next, per season (relative
-- weights; the current weather is never chosen twice in a row). MIT License.
local weather_data = {}

function weather_data.apply()
  weather.table{
    spring = { clear = 3.0, cloudy = 3.0, overcast = 2.0, fog = 1.0, rain = 2.5, storm = 0.5, snow = 0.5 },
    summer = { clear = 5.0, cloudy = 3.0, overcast = 1.0, fog = 0.5, rain = 1.5, storm = 1.2, snow = 0.0 },
    autumn = { clear = 2.5, cloudy = 3.0, overcast = 2.5, fog = 2.5, rain = 2.5, storm = 0.3, snow = 0.5 },
    winter = { clear = 2.5, cloudy = 2.0, overcast = 3.0, fog = 2.0, rain = 0.5, storm = 0.0, snow = 3.5 },
  }
end

return weather_data
