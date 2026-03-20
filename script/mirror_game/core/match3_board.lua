local Match3Board = {}
Match3Board.__index = Match3Board

Match3Board.GemType = {
    EMPTY = 0,
    RED = 1,
    GREEN = 2,
    BLUE = 3,
    YELLOW = 4,
    PURPLE = 5
}

function Match3Board.new(width, height)
    local self = setmetatable({}, Match3Board)
    self.width = width or 8
    self.height = height or 8
    self.grid = {}
    self.selected_gem = nil -- {x, y}
    self.is_processing = false
    self.on_match_callback = nil
    
    self:InitGrid()
    return self
end

function Match3Board:SetMatchCallback(callback)
    self.on_match_callback = callback
end

function Match3Board:InitGrid()
    for y = 1, self.height do
        self.grid[y] = {}
        for x = 1, self.width do
            -- Fill with random gems, ensuring no initial matches
            local gem_type
            repeat
                gem_type = math.random(1, 5) -- 1 to 5 types
            until not self:CheckMatchAt(x, y, gem_type)
            
            self.grid[y][x] = {
                type = gem_type,
                game_object = nil -- To be linked with visual object
            }
        end
    end
end

function Match3Board:CheckMatchAt(x, y, type)
    -- Check horizontal
    if x > 2 then
        if self.grid[y][x-1].type == type and self.grid[y][x-2].type == type then
            return true
        end
    end
    -- Check vertical
    if y > 2 then
        if self.grid[y-1][x].type == type and self.grid[y-2][x].type == type then
            return true
        end
    end
    return false
end

function Match3Board:GetGem(x, y)
    if x >= 1 and x <= self.width and y >= 1 and y <= self.height then
        return self.grid[y][x]
    end
    return nil
end

function Match3Board:SwapGems(x1, y1, x2, y2)
    local gem1 = self:GetGem(x1, y1)
    local gem2 = self:GetGem(x2, y2)
    
    if gem1 and gem2 then
        -- Swap data
        local temp_type = gem1.type
        gem1.type = gem2.type
        gem2.type = temp_type
        
        -- In a real implementation, we would also swap/animate the game_objects here
        -- For now, we assume the Scene will handle the visual update based on data
        return true
    end
    return false
end

function Match3Board:FindMatches()
    local matches = {}
    local marked = {}

    local function mark(x, y)
        local key = y .. "_" .. x
        if not marked[key] then
            marked[key] = true
            table.insert(matches, {x = x, y = y})
        end
    end
    
    -- Horizontal
    for y = 1, self.height do
        for x = 1, self.width - 2 do
            local type = self.grid[y][x].type
            if type ~= Match3Board.GemType.EMPTY then
                if self.grid[y][x+1].type == type and self.grid[y][x+2].type == type then
                    mark(x, y)
                    mark(x + 1, y)
                    mark(x + 2, y)
                    local k = x + 3
                    while k <= self.width and self.grid[y][k].type == type do
                        mark(k, y)
                        k = k + 1
                    end
                end
            end
        end
    end
    
    -- Vertical
    for x = 1, self.width do
        for y = 1, self.height - 2 do
            local type = self.grid[y][x].type
            if type ~= Match3Board.GemType.EMPTY then
                if self.grid[y+1][x].type == type and self.grid[y+2][x].type == type then
                    mark(x, y)
                    mark(x, y + 1)
                    mark(x, y + 2)
                    local k = y + 3
                    while k <= self.height and self.grid[k][x].type == type do
                        mark(x, k)
                        k = k + 1
                    end
                end
            end
        end
    end
    
    return matches
end

function Match3Board:RemoveMatches(matches)
    for _, pos in ipairs(matches) do
        self.grid[pos.y][pos.x].type = Match3Board.GemType.EMPTY
        -- Trigger visual destruction here
    end
end

function Match3Board:ApplyGravity()
    -- Simple gravity implementation
    for x = 1, self.width do
        local write_y = 1
        for y = 1, self.height do
            if self.grid[y][x].type ~= Match3Board.GemType.EMPTY then
                if y ~= write_y then
                    self.grid[write_y][x].type = self.grid[y][x].type
                    self.grid[y][x].type = Match3Board.GemType.EMPTY
                end
                write_y = write_y + 1
            end
        end
        
        -- Fill top with new random gems
        for y = write_y, self.height do
            self.grid[y][x].type = math.random(1, 5)
        end
    end
end

return Match3Board
