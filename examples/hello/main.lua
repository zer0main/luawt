local test = require 'luawt.test'

local module_file = io.open('./examples/hello/hello.lua', 'r')
local code = module_file:read('*all')
module_file:close()

local ip = '0.0.0.0'
local port = 12346
local wt_config = test.baseConfig()
local server = test.createServer(code, ip, port, wt_config)

server:start()
server:waitForShutdown()
