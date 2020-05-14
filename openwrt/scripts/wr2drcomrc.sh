#!/bin/sh /etc/rc.common

# post `username' and `password' from /etc/config/drcomrc to `/overlay/Drcom4CWNU/drcomrc'
# GPL v2
# (C) leetking <li_Tking@163.com>
# locate at /overlay/Drcom4CWNU/wr2drcomrc.sh

CONFIG_NAME=drcomrc
SECTION_NAME=setting
DRCOMRC=/overlay/Drcom4CWNU/drcomrc

parse_conf() {
    local username
    local paswd
    config_get username $1 username
    config_get paswd $1 password
    echo "username: $username"
    echo "password  : $paswd"
    if [ -e ${DRCOMRC} ]; then
        sed -i "/username/s/\"[^\"]*\"/\"${username}\"/" ${DRCOMRC}
        sed -i "/password/s/\"[^\"]*\"/\"${paswd}\"/" ${DRCOMRC}
    fi
}

start() {
    config_load $CONFIG_NAME
    config_foreach parse_conf $SECTION_NAME
}
