--[[
    cbi 模块
    用于处理登录
    位于 /usr/lib/lua/luci/model/cbi/Drcom4CWNU.lua
]]--

require("luci.sys")

local ETC_CONFIG = "drcomrc"
local ETC_CONFIG_SECTION = "setting"
local DRCOM_PATH  = "/overlay/Drcom4CWNU/"
local WR2DRCOMRC  = "wr2drcomrc.sh"
local WR2WIRELESS = "wr2wireless.sh"
--local APP_NAME    = "Dr.com4CWNU"
local APP_NAME    = "Dr.com"
--local DESCRIPTION = "项目所在<a href='https://github.com/leetking/cwnu-drcom'>cwnu-drcom</a><br/>"..
--           "一个为<a href='http://www.cwnu.edu.cn'>西华师范大学</a>开发的第三方dr.com登录客户端."
local DESCRIPTION = "项目所在<a href='https://github.com/leetking/cwnu-drcom'>cwnu-drcom</a><br/>"..
            "一个为<a href='http://www.cwnu.edu.cn'>本校</a>开发的第三方dr.com登录客户端."

m = Map(ETC_CONFIG, translate(APP_NAME), translate(DESCRIPTION))

s = m:section(TypedSection, ETC_CONFIG_SECTION, "")
s.addremove = false --不允许添加NamedSection
s.anonymous = true  --匿名显示这个(setting)NamedSection

username = s:option(Value, "username", translate("学号"))
password = s:option(Value, "password", translate("密码"))
password.password = true
-- 把wifi配置移到这里来吧，简化本校他们的操作
wifissid = s:option(Value, "wifissid", translate("WIFI名字"))
wifipwd  = s:option(Value, "wifipwd", translate("WIFI密码"))
wifipwd.password = true

local apply = luci.http.formvalue()
if apply then
    io.popen(DRCOM_PATH..WR2DRCOMRC.." start")
    io.popen(DRCOM_PATH..WR2WIRELESS.." start")
end

return m
