local luawt = require 'luawt'

local app, env = ...

function mainApp(...)
    local text = luawt.WText('Your name, please?')
    local name_edit = luawt.WLineEdit(app:root())
    name_edit:setFocus()
    name_edit:setInline(false)
    local button = luawt.WPushButton("Greet me", app:root())
    local greeting = luawt.WText(app:root())
    button:clicked():connect(function()
        greeting:setText("Hello there, " .. name_edit:text())
    end)
end

mainApp()
