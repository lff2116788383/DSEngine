-- Config Parser for Flare-style INI files
-- Supports:
-- [section]
-- key=value
-- multi-key=value1
-- multi-key=value2
-- INCLUDE path/to/file
-- data=csv,values,spanning,multiple,lines

local ConfigParser = {}

function ConfigParser.parse(file_path)
    local file = io.open(file_path, "r")
    if not file then
        print("Error: Could not open file " .. file_path)
        return nil
    end

    local data = {}
    local current_section = nil
    local current_key = nil
    local is_reading_multiline = false
    local multiline_buffer = ""

    local function add_value(section_data, key, value)
        if section_data[key] then
            if type(section_data[key]) ~= "table" then
                section_data[key] = {section_data[key]}
            end
            table.insert(section_data[key], value)
        else
            section_data[key] = value
        end
    end

    for line in file:lines() do
        -- Trim whitespace
        line = line:match("^%s*(.-)%s*$")

        -- Skip comments and empty lines
        if line == "" or line:sub(1, 1) == "#" then
            goto continue
        end

        -- Handle Includes (Basic support)
        if line:sub(1, 7) == "INCLUDE" then
            local include_path = line:sub(9)
            -- Resolve path relative to current file (simplified)
            local dir = file_path:match("(.*/)") or ""
            local full_include_path = dir .. include_path
            -- print("Including: " .. full_include_path)
            local included_data = ConfigParser.parse(full_include_path)
            
            -- Merge included data (Special logic for items which are lists of [item])
            -- For now, just append top-level arrays if they exist, or merge sections
            if included_data then
                for k, v in pairs(included_data) do
                    if type(v) == "table" and data[k] and type(data[k]) == "table" then
                         -- Merge list-like tables (like items array)
                         for _, item in ipairs(v) do
                             table.insert(data[k], item)
                         end
                    else
                        data[k] = v
                    end
                end
            end
            goto continue
        end

        -- Section Header
        local section_name = line:match("^%[(.-)%]$")
        if section_name then
            -- Special handling for repeating sections like [item], [layer], [tileset] (though tileset is usually a key in header)
            -- Flare uses [layer] multiple times.
            if section_name == "item" or section_name == "enemy" or section_name == "power" or section_name == "layer" then
                if not data[section_name.."s"] then data[section_name.."s"] = {} end
                current_section = {}
                table.insert(data[section_name.."s"], current_section)
            else
                -- For unique sections like [header], [tilesets] (if unique)
                -- If it already exists, merge or overwrite?
                -- Flare [tilesets] appears once.
                -- Let's stick to overwrite for others unless we find duplicates.
                if data[section_name] then
                    -- If duplicate section encountered and not in special list, maybe convert to list?
                    -- For now, just overwrite to keep simple, but logging might help.
                    -- print("Warning: Overwriting section " .. section_name)
                end
                current_section = {}
                data[section_name] = current_section
            end
            is_reading_multiline = false
            goto continue
        end

        -- Multiline data handling (for map data)
        if is_reading_multiline then
            if line:find("=") then
                -- New key found, end multiline
                if current_section and current_key then
                    current_section[current_key] = multiline_buffer
                end
                is_reading_multiline = false
                multiline_buffer = ""
            else
                multiline_buffer = multiline_buffer .. line .. "," -- Add comma to ensure separation
                goto continue
            end
        end

        -- Key-Value pair
        local key, value = line:match("^(.-)=(.*)$")
        if key then
            if current_section then
                -- Check for empty value indicating start of multiline (like data=)
                if value == "" then
                    current_key = key
                    is_reading_multiline = true
                    multiline_buffer = ""
                else
                    add_value(current_section, key, value)
                end
            else
                -- Global properties
                add_value(data, key, value)
            end
        end

        ::continue::
    end
    
    -- Finish last multiline if exists
    if is_reading_multiline and current_section and current_key then
        current_section[current_key] = multiline_buffer
    end

    file:close()
    return data
end

return ConfigParser
