--西华师范大学drcom客户端协议分析插件for wireshark
--write by leetking <li_Tking@163.com>

do
    local drcom_cwnu = Proto("drcom-cwnu", "Dr.com for cwnu")
    local pf = drcom_cwnu.fields

    --包类型
    local pkttype = {
        [0x01] = "Start Request",
        [0x02] = "Start Response",
        [0x03] = "Login Auth",
        [0x04] = "Success",
        [0x05] = "Failure",
        [0x06] = "Logout Auth",
        --本校基本全是这个类型的包0x07
        [0x07] = "NORMAL",    --通用常用操作，包括认证和心跳
        [0x08] = "Unknown, Attention！！",
        [0x09] = "New Password",
        [0x4d] = "Message", --通知消息
        [0xfe] = "Alive 4 client",
        [0xff] = "Alive, Client to Server per 20s",
    }
    pf.pkttype = ProtoField.uint8("drcom.pkttype", "pkttype", base.HEX, pkttype)
    pf.normalcnt = ProtoField.uint8("drcom.normalcnt", "cnt")
    pf.pktlen = ProtoField.uint16("drcom.pktlen", "ptklen")
    local normalauthtype = {
        [0x01] = "Request Keep Alive",
        [0x02] = "Response Keep Alive",
        [0x03] = "Send Identity",
        [0x04] = "Verity Success",
        [0x0b] = "Keep Aliveing",
        [0x06] = "Keep Aliveing 2",
    }
    pf.normalauthtype = ProtoField.uint8("drcom.normalauthtype", "type", base.HEX, normalauthtype)
    pf.usrlen = ProtoField.uint8("drcom.usrlen", "usrlen")
    pf.nusrlen = ProtoField.uint8("drcom.nusrlen", "n*usrlen")
    pf.clientMac = ProtoField.ether("drcom.clientMac", "clientMac")
    pf.clientIP = ProtoField.ipv4("drcom.clientIP", "clientIP")
    pf.serverIP = ProtoField.ipv4("drcom.serverIP", "serverIP")
    pf.fixed03220008 = ProtoField.uint32("drcom.f_fixed03220008", "fixed", base.HEX)
    pf.recvflux = ProtoField.uint32("drcom.recvflux", "recvflux", base.HEX)
    pf.verify = ProtoField.uint32("drcom.verify", "verify", base.HEX)
    pf.zeros =	ProtoField.bytes("drcom.zeros","zeros")
    pf.user = ProtoField.string("drcom.user", "user")
    pf.hostname = ProtoField.string("drcom.hostname", "hostname")
    pf.dns = ProtoField.ipv4("drcom.dns", "dns")
    local msgtype = {
        [0x56] = "msg",
        [0x3a] = "msg2",
    }
    pf.msgtype = ProtoField.uint8("drcom.msgtype", "msgtype", base.HEX)
    pf.kpstep = ProtoField.uint8("drcom.kpstep", "kpstep", base.HEX)
    pf.kpid = ProtoField.uint16("drcom.kpid", "kpid")
    pf.kpfix = ProtoField.uint16("drcom.kpfix", "kpfix(0x02d6)", base.HEX)
    pf.kpsercnt = ProtoField.uint32("drcom.kpsercnt", "servcnt")
    pf.kpcksum = ProtoField.uint32("drcom.kpcksum", "kpcksum", base.HEX)
    pf.md5a = ProtoField.bytes("drcom.md5a", "MD5A=md5(code +type +challenge +password)") --ubytes
    pf.drco = ProtoField.string("drcom.drco", "drco")
    pf.time = ProtoField.uint16("drcom.time", "time")
    pf.x06sercnt = ProtoField.uint32("drcom.x06sercnt", "sercnt")


    --为数据包解析的函数
    function drcom_cwnu.dissector(buf, pkt, root)
        pkt.cols.protocol= drcom_cwnu.name		--覆盖协议栏为DRCOM-CWNU
        local subtree = root:add(drcom_cwnu, buf())
        subtree:append_text(" "..buf():len().." bytes")

        --开始解析
        local pktid = buf(0, 1):uint()
        subtree:add(pf.pkttype, buf(0,1))
        pkt.cols.info = pkttype[pktid] or "Unknown"

        --0x07 NORMAL
        if (pktid == 0x07) then
            subtree:add(pf.normalcnt, buf(1, 1))
            subtree:add_le(pf.pktlen, buf(2, 2))
            subtree:add(pf.normalauthtype, buf(4, 1))
            local step = buf(4, 1):uint()
            pkt.cols.info:append(", "..normalauthtype[step])
            --normal包第一个，请求开始心跳
            if (step == 0x01) then

            --服务器回应
            elseif (step == 0x02) then
                subtree:add(pf.recvflux, buf(8, 4))
                subtree:add(pf.clientIP, buf(12, 4))

            --发送信息
            elseif (step == 0x03) then
                subtree:add(pf.usrlen, buf(5, 1))
                subtree:add(pf.clientMac, buf(6, 6))
                subtree:add(pf.clientIP, buf(12, 4))
                subtree:add(pf.fixed03220008, buf(16, 4))
                subtree:add(pf.recvflux, buf(20, 4))        --复制回应服务器
                subtree:add(pf.verify, buf(24,4))           --!校验值
                subtree:add(pf.zeros, buf(28, 4))           --00000000
                subtree:add(pf.user, buf(32, buf(5,1):uint()))
                local nextoff = 32+buf(5, 1):uint()
                subtree:add(pf.hostname, buf(nextoff, 16))
                subtree:add(pf.dns, buf(60, 4))
                subtree:add(pf.zeros, buf(64, 4))
                subtree:add(pf.dns, buf(68, 4))
                subtree:add(pf.zeros, buf(72, 4))

            --确认信息成功
            elseif (step == 0x04) then
                subtree:add(pf.usrlen, buf(5, 1))
                --subtree:add(pf.serverIP, buf(20, 4))
                
            --特殊心跳的一部分
            elseif (step == 0x06) then
                subtree:add_le(pf.nusrlen, buf(6, 2))       --每次加上usrlen
                subtree:add_le(pf.x06sercnt, buf(8, 4))
                subtree:add(pf.clientIP, buf(12, 4))
                subtree:add_le(pf.nusrlen, buf(32, 2))       --每次加上usrlen

            --进行心跳
            elseif (step == 0x0b) then
                local _kpstep = buf(5,1):uint()
                subtree:add(pf.kpstep, buf(5, 1))       --心跳类型
                subtree:add_le(pf.kpfix, buf(6, 2))     --fix
                subtree:add(pf.kpid, buf(8, 4))         --keep avlie id，客户端的随机数
                subtree:add(pf.zeros, buf(12, 4))
                subtree:add(pf.kpsercnt, buf(16, 4))
                subtree:add(pf.zeros, buf(20, 4))
                if (_kpstep == 0x01) then
                    pkt.cols.info:append(", c -> s");
                elseif (_kpstep == 0x02) then
                    pkt.cols.info:append(", c <- s");
                elseif (_kpstep == 0x03) then
                    pkt.cols.info:append(", c -> s");
                    subtree:add(pf.kpcksum, buf(24, 4))
                    subtree:add(pf.clientIP, buf(28 ,4))
                elseif (_kpstep == 0x04) then
                    pkt.cols.info:append(", c <- s (finish a keep alive crycle.)")
                end
            end

        --0x4d 消息通知
        elseif (pktid == 0x4d) then
            subtree:add(pf.msgtype, buf(1, 1))

        --0xff 特殊心跳，每10个普通心跳后就有一个
        elseif (pktid == 0xff) then
            --subtree:add(
            subtree:add(pf.md5a, buf(1,16))
            subtree:add(pf.zeros, buf(17,3))
            subtree:add(pf.drco, buf(20, 4))    --Drco
            subtree:add(pf.serverIP, buf(24, 4))
            --28, 2
            subtree:add(pf.clientIP, buf(30, 4))
            subtree:add_le(pf.time, buf(36, 2))
        end
    end

    local wtap_encap_table = DissectorTable.get("wtap_encap")
    local udp_encap_table =	DissectorTable.get("udp.port")
    wtap_encap_table:add(wtap.USER15,drcom_cwnu)
    wtap_encap_table:add(wtap.USER12,drcom_cwnu)
    udp_encap_table:add(61440, drcom_cwnu)
end
--[[
# IMPORTANT
本插件只是针对**西华师范大学**（cwnu）的drcom登录包验证有效!

# 参考
- 参考了Lixiang, douniwan5788的`Wireshark dr.com协议插件`
- 参考了CuberL，对协议的部分破解(http://cuberl.com/2016/09/17/make-a-drcom-client-by-yourself/)
]]
