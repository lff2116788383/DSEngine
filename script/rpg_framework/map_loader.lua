local ConfigParser = require("script/rpg_framework/config_parser")

local MapLoader = {}

-- Helper to resolve paths
-- map_path: full path to the map file being loaded
-- relative_path: path found in the config file
local function resolve_path(map_path, relative_path)
    -- Get directory of map_path
    local map_dir = map_path:match("(.*[/\\])") or ""
    
    -- Combine
    local full_path = map_dir .. relative_path
    
    -- Collapse ../ (naive implementation)
    -- DSEngine might handle relative paths if we pass them to LoadFromFile, 
    -- but usually textures need clean paths relative to data/ or executable.
    -- Assuming DSEngine runs from project root.
    
    -- A simple stack-based path resolver
    local parts = {}
    for part in full_path:gmatch("[^/\\]+") do
        if part == ".." then
            if #parts > 0 then table.remove(parts) end
        elseif part ~= "." then
            table.insert(parts, part)
        end
    end
    
    return table.concat(parts, "/")
end

function MapLoader.load(map_file_path)
    local data = ConfigParser.parse(map_file_path)
    if not data then return nil end
    
    local header = data.header
    if not header then 
        print("Error: No [header] in map file")
        return nil 
    end
    
    -- Create Root Map Object
    local map_go = GameObject.new(header.title or "Map")
    local transform = map_go:AddComponent(Transform)
    local grid = map_go:AddComponent(Grid)
    
    -- Configure Grid
    local tile_w = tonumber(header.tilewidth) or 32
    local tile_h = tonumber(header.tileheight) or 32
    grid:set_cell_size(glm.vec2(tile_w, tile_h))
    
    if header.orientation == "isometric" then
        -- Use the new enum we added to C++
        grid:set_cell_layout(Cpp.CellLayout.Isometric)
    else
        grid:set_cell_layout(Cpp.CellLayout.Rectangle)
    end
    
    -- Load Tilesets and map GIDs
    local tilesets = {}
    local current_gid = 1
    
    -- data.tilesets is a section. But 'tileset' key appears multiple times.
    -- ConfigParser puts multiple values in a list.
    local tileset_defs = data.tilesets and data.tilesets.tileset
    if type(tileset_defs) == "string" then tileset_defs = {tileset_defs} end
    
    if tileset_defs then
        for _, def in ipairs(tileset_defs) do
            -- Format: path,w,h,offx,offy
            local path, w, h, offx, offy = def:match("([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)")
            if not path then 
                 -- Try format without offsets: path,w,h
                 path, w, h = def:match("([^,]+),([^,]+),([^,]+)")
                 offx, offy = 0, 0
            end
            
            w = tonumber(w)
            h = tonumber(h)
            
            local full_path = resolve_path(map_file_path, path)
            -- Remove "mods/" prefix if present, assuming data/mods structure?
            -- Or just try to load. DSEngine usually loads from data/. 
            -- Flare paths are weird. "tileset=tilesetdefs/tileset_grassland.txt"
            -- Wait, the map file had "tileset=tilesetdefs/tileset_grassland.txt" in [header]
            -- BUT also [tilesets] section with actual images.
            -- Flare maps can include external tileset defs or embed them.
            -- frontier_outpost.txt has explicit [tilesets] section.
            
            local texture = Texture2D.LoadFromFile(full_path)
            if texture then
                local tex_w = texture:width()
                local tex_h = texture:height()
                
                -- Calculate number of tiles
                local cols = math.floor(tex_w / w)
                local rows = math.floor(tex_h / h)
                local count = cols * rows
                
                table.insert(tilesets, {
                    texture = texture,
                    first_gid = current_gid,
                    tile_w = w,
                    tile_h = h,
                    cols = cols,
                    count = count
                })
                
                current_gid = current_gid + count
            else
                print("Failed to load tileset: " .. full_path)
            end
        end
    end
    
    -- Helper to get Sprite from GID
    local function get_sprite_for_gid(gid)
        if gid == 0 then return nil end
        
        -- Find tileset
        -- Iterate in reverse to find the range
        for i = #tilesets, 1, -1 do
            local ts = tilesets[i]
            if gid >= ts.first_gid then
                local local_id = gid - ts.first_gid
                if local_id < ts.count then
                    -- Found it
                    local col = local_id % ts.cols
                    local row = math.floor(local_id / ts.cols)
                    
                    local rect_x = col * ts.tile_w
                    local rect_y = row * ts.tile_h
                    
                    local sprite = Sprite.Create(ts.texture)
                    sprite:set_rect(rect_x, rect_y, ts.tile_w, ts.tile_h)
                    -- Set pivot if needed (usually center bottom for isometric?)
                    -- DSEngine default pivot is center (0.5, 0.5) usually.
                    -- Flare tiles align bottom-center usually.
                    -- Let's try default first.
                    return sprite
                end
            end
        end
        return nil
    end
    
    -- Load Layers
    if data.layers then
        for i, layer_data in ipairs(data.layers) do
            local layer_go = GameObject.new("Layer_" .. (layer_data.type or i))
            layer_go:SetParent(map_go)
            
            -- Add Tilemap
            local tilemap = layer_go:AddComponent(Tilemap)
            local renderer = layer_go:AddComponent(TilemapRenderer)
            
            -- Set Sorting Order
            renderer:set_sorting_layer(0) -- Default layer
            renderer:set_order_in_layer(i) -- Use index as Z-order
            
            -- Parse CSV Data
            if layer_data.data then
                local cell_x = 0
                local cell_y = 0
                -- Assuming map width/height is known from header
                local map_w = tonumber(header.width) or 100 -- Default fallback
                
                -- Split by comma and newlines (gmatch handles this naturally if we match non-separators)
                -- But ConfigParser might have left newlines in the string? 
                -- "0,0,0,\n0,0,0" -> concatenated in my parser logic without separator if they were separate lines?
                -- My parser: multiline_buffer = multiline_buffer .. line
                -- So "0,0,0," .. "0,0,0" becomes "0,0,0,0,0,0" (correct)
                -- But if line didn't end with comma? "0,0,0" .. "0,0,0" -> "0,0,00,0,0" (error!)
                -- I should check ConfigParser concatenation.
                
                for gid_str in layer_data.data:gmatch("([^, \t\r\n]+)") do
                    local gid = tonumber(gid_str)
                    if gid and gid > 0 then
                        local sprite = get_sprite_for_gid(gid)
                        if sprite then
                            tilemap:SetTile(glm.ivec2(cell_x, cell_y), sprite)
                        end
                    end
                    
                    cell_x = cell_x + 1
                    if cell_x >= map_w then
                        cell_x = 0
                        cell_y = cell_y + 1
                    end
                end
            end
        end
    end
    
    return map_go
end

return MapLoader
