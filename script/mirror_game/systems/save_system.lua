local SaveSystem = {}

local save_file_path = "save_data.json"

-- 简单的 JSON 序列化辅助函数 (如果引擎没有自带 json 库)
local function serialize(tbl)
    local function serialize_val(val)
        local t = type(val)
        if t == "string" then
            return string.format("%q", val)
        elseif t == "number" or t == "boolean" then
            return tostring(val)
        elseif t == "table" then
            local res = "{"
            local first = true
            for k, v in pairs(val) do
                if not first then res = res .. "," end
                if type(k) == "string" then
                    res = res .. string.format("[%q]=", k)
                else
                    res = res .. string.format("[%s]=", tostring(k))
                end
                res = res .. serialize_val(v)
                first = false
            end
            res = res .. "}"
            return res
        else
            return "nil"
        end
    end
    return serialize_val(tbl)
end

function SaveSystem.SaveGame(player)
    if not player then return false end
    
    local save_data = {
        coin = player.coin,
        gold = player.gold,
        reputation = player.reputation,
        human = {
            level = player.humans[1].level,
            exp = player.humans[1].exp,
            hp = player.humans[1].hp,
        },
        items = {},
        equips = {},
        equipped = {}
    }
    
    -- 保存物品
    for _, item in ipairs(player.bag_items) do
        table.insert(save_data.items, {id = item.id, count = item.count})
    end
    
    -- 保存装备
    for _, equip in ipairs(player.bag_equips) do
        table.insert(save_data.equips, {id = equip.id})
    end
    
    -- 保存穿戴状态 (假设只存部位1)
    local weapon = player.humans[1].equipments[1]
    if weapon then
        save_data.equipped[1] = weapon.id
    end
    
    local data_str = serialize(save_data)
    
    local file = io.open(save_file_path, "w")
    if file then
        file:write("return " .. data_str)
        file:close()
        print("Game Saved Successfully!")
        return true
    end
    print("Failed to save game.")
    return false
end

function SaveSystem.LoadGame(player)
    local file = io.open(save_file_path, "r")
    if not file then
        print("No save file found.")
        return false
    end
    file:close()
    
    local chunk = loadfile(save_file_path)
    if not chunk then
        print("Failed to parse save file.")
        return false
    end
    
    local save_data = chunk()
    if not save_data then return false end
    
    -- 恢复数据
    player.coin = save_data.coin or 0
    player.gold = save_data.gold or 0
    player.reputation = save_data.reputation or 0
    
    player.humans[1].level = save_data.human.level or 1
    player.humans[1].exp = save_data.human.exp or 0
    player.humans[1].hp = save_data.human.hp or 100
    
    player.bag_items = {}
    for _, item_data in ipairs(save_data.items or {}) do
        player:AddItem(item_data.id, item_data.count)
    end
    
    player.bag_equips = {}
    for _, eq_data in ipairs(save_data.equips or {}) do
        player:AddEquip(eq_data.id)
    end
    
    -- 恢复穿戴状态
    if save_data.equipped and save_data.equipped[1] then
        for _, eq in ipairs(player.bag_equips) do
            if eq.id == save_data.equipped[1] then
                player.humans[1]:WearEquip(1, eq)
                break
            end
        end
    end
    
    print("Game Loaded Successfully!")
    return true
end

return SaveSystem
