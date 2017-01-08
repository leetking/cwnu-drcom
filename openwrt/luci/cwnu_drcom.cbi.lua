require("luci.sys")

--[[
    cbi 模块
    用于处理登录
]]--

local ETC_CONFIG = "drcomrc"
local ETC_CONFIG_SECTION = "setting"
local DRCOM_PATH = "/overlay/cwnu-drcom/"
local WR2DRCOMRC = "wr2drcomrc.sh"
local APP_NAME = "Dr.com4CWNU"
local DESCRIPTION = "<a href='https://github.com/leetking/cwnu-drcom'>cwnu-drcom</a><br/>一个为<a href='http://www.cwnu.edu.cn'>西华师范大学</a>开发的第三方dr.com登录客户端."

m = Map(ETC_CONFIG, translate(APP_NAME), translate(DESCRIPTION))

s = m:section(TypedSection, ETC_CONFIG_SECTION, "")
s.addremove = false --不允许添加NamedSection
s.anonymous = true  --匿名显示这个(setting)NamedSection

uname = s:option(Value, "uname", translate("学号"))
pwd = s:option(Value, "pwd", translate("密码"))
pwd.password = true

local apply = luci.http.formvalue()
if apply then
    io.popen(DRCOM_PATH..WR2DRCOMRC.." start")
end

return m
