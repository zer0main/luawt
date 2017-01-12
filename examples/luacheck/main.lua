local test = require 'luawt.test'

local code = [[
    local app, env = ...
    local luacheck = require 'luacheck'
    local luawt = require 'luawt'
    local textarea = luawt.WTextArea(app:root())
    local button = luawt.WPushButton(app:root())
    button:setText("Check Lua syntax")
    local result = luawt.WText(app:root())
    result:setStyleClass("luacheck-results")
    button:clicked():connect(function()
        local c = luacheck.check_strings({textarea:text()})
        if c.fatals + c.errors + c.warnings > 0 then
            local error_msg = ''
            for i = 1, #c[1] do
                error_msg = error_msg .. luacheck.get_message(c[1][i]) .. '\n'
            end
            result:setText("\nErrors: \n" .. error_msg)
        else
            result:setText("No errors")
        end
    end)
]]

local port = 12345
local wt_config = test.baseConfig()
local server = test.createServer(code, port, wt_config)

server:start()
