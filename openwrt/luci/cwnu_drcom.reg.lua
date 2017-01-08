require("luci.sys")

--[[
    drcom for cwnu
    注册模块
    NOTE require貌似必须放到第一行，不然没法识别？
]]--

local APP_NAME = "Dr.com4CWNU"

module("luci.controller.cwnu_drcom", package.seeall)
function index()
    if not nixio.fs.access("/etc/config/drcomrc") then
        return
    end
    local e
    e = entry({"admin", "services", "cwnu_drcom"}, cbi("cwnu_drcom"), _(APP_NAME), 10)
end
