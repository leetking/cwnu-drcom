--[[
    drcom for cwnu
    注册模块
    位于 /usr/lib/lua/luci/controller/Drcom4CWNU.lua
]]--
require("luci.sys")

local APP_NAME = "Dr.com4CWNU"

module("luci.controller.Drcom4CWNU", package.seeall)
function index()
    if not nixio.fs.access("/etc/config/drcomrc") then
        return
    end
    entry({"admin", "services", "Drcom4CWNU"}, cbi("Drcom4CWNU"), _(APP_NAME), 10)
end
