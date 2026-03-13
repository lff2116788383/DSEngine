local ConfigParser = require("script/rpg_framework/config_parser")
local MapLoader = require("script/rpg_framework/map_loader")
local RPGStats = require("script/rpg_framework/components/rpg_stats")

return {
    ConfigParser = ConfigParser,
    MapLoader = MapLoader,
    Components = {
        RPGStats = RPGStats
    }
}
