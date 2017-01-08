#!/bin/sh /etc/rc.common

# 读取/etc/config/drcomrc里的openwrt格式信息
# 把里面的用户名(uanme), 密码(pwd)写到cwnu-drcom的配置文件里(/overlay/cwnu-drcom/drcomrc)

CONFIG_NAME=drcomrc
SECTION_NAME=setting
DRCOMRC=/overlay/cwnu-drcom/drcomrc

parse_conf() {
    local uname
    local paswd
    config_get uname $1 uname
    config_get paswd $1 pwd
    echo "uname: $uname"
    echo "pwd  : $paswd"
    if [ -e ${DRCOMRC} ]; then
        sed -i "/uname/s/\"[^\"]*\"/\"${uname}\"/" ${DRCOMRC}
        sed -i "/pwd/s/\"[^\"]*\"/\"${paswd}\"/" ${DRCOMRC}
    fi
}

start() {
    config_load $CONFIG_NAME
    config_foreach parse_conf $SECTION_NAME
}
