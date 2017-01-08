#!/bin/bash

# 一键部署文件到openwrt路由器
rm -rf ~/.ssh/known_hosts

_DEBUG=
IP=192.168.1.1
_CC=

# 确定架构
read -p "路由器是大端还是小端?(默认是大端,Y)[Y/n] " lorm
case "${lorm}" in
    "N"|"n")
        _CC=mipsel-openwrt-linux-gcc
        ;;
    *)
        _CC=mips-openwrt-linux-gcc
        ;;
esac

# 编译程序
cd ..
make dist-clean
make CC=${_CC} IS_DEBUG=${_DEBUG}
cd openwrt
${_CC} -o random_mac random_mac.c -O2

# 上传到路由器，进行安装的脚本
install_sh=install-$$.sh
cat << EOF > $install_sh
rm -f /etc/rc.d/S98cwnu-drcom
rm -f /etc/rc.d/S99cwnu-drcom

# install cwnu-drcom managerment script
mv /tmp/drcom.sh /etc/init.d/cwnu-drcom
chmod +x /etc/init.d/cwnu-drcom

# install drcom core
mkdir -p /overlay/cwnu-drcom
mv /tmp/drcom /overlay/cwnu-drcom/
mv /tmp/drcomrc.example /overlay/cwnu-drcom/drcomrc

# install daemon script and make it autorun
mv /tmp/drcom-daemon.sh /etc/init.d/drcom-daemon
chmod +x /etc/init.d/drcom-daemon
ln -sf /etc/init.d/drcom-daemon /etc/rc.d/S98drcom-daemon

# install random_mac
mv /tmp/random_mac /bin/random_mac
chmod +x /bin/random_mac

# install ui
mv /tmp/wr2drcomrc.sh /overlay/cwnu-drcom/
chmod +x /overlay/cwnu-drcom/wr2drcomrc.sh
mv /tmp/cwnu_drcom.reg.lua /usr/lib/lua/luci/controller/cwnu_drcom.lua
mv /tmp/cwnu_drcom.cbi.lua /usr/lib/lua/luci/model/cbi/cwnu_drcom.lua
mv /tmp/drcomrc.etc /etc/config/drcomrc

exit 0
EOF

# 上传文件
UPFILES="$install_sh ../drcom ../drcomrc.example \
    scripts/drcom.sh scripts/drcom-daemon.sh scripts/wr2drcomrc.sh \
    luci/cwnu_drcom.cbi.lua luci/cwnu_drcom.reg.lua luci/drcomrc.etc \
    random_mac"
scp ${UPFILES} root@${IP}:/tmp/
ssh root@${IP} "sh /tmp/$install_sh"

rm -f $install_sh

# 开始配置路由器
#./configusr.sh

exit 0
