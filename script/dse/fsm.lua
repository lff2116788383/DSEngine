--- @module dse.fsm
--- @brief Lua 状态机 (FSM) 框架，用于游戏逻辑和 AI

local FSM = {}
FSM.__index = FSM

--- 创建一个新的状态机
--- @param target table|nil 状态机要操作的目标对象
--- @return table
function FSM.new(target)
    local self = setmetatable({}, FSM)
    self.target = target
    self.states = {}
    self.current_state = nil
    self.current_state_name = nil
    return self
end

--- 注册一个状态
--- @param name string 状态名称
--- @param state table 状态表，包含 on_enter, on_update, on_exit 函数
function FSM:add_state(name, state)
    self.states[name] = state
end

--- 切换到新状态
--- @param name string 新状态名称
function FSM:change_state(name)
    if self.current_state_name == name then
        return
    end

    local next_state = self.states[name]
    if not next_state then
        print("FSM Error: State not found: " .. tostring(name))
        return
    end

    if self.current_state and self.current_state.on_exit then
        self.current_state.on_exit(self.target)
    end

    self.current_state_name = name
    self.current_state = next_state

    if self.current_state.on_enter then
        self.current_state.on_enter(self.target)
    end
end

--- 每帧更新
--- @param dt number 增量时间
function FSM:update(dt)
    if self.current_state and self.current_state.on_update then
        self.current_state.on_update(self.target, dt)
    end
end

--- 获取当前状态名称
--- @return string|nil
function FSM:get_current_state()
    return self.current_state_name
end

return FSM
