#!/bin/sh /etc/rc.common

# post `uname' and `pwd' from /etc/config/drcom to `/overlay/Drcom4CWNU/drcomrc'
# GPL v2
# (C) leetking <li_Tking@163.com>
# locate at /overlay/Drcom4CWNU/wr2drcomrc.sh

CONFIG_NAME=drcomrc
SECTION_NAME=setting
DRCOMRC=/overlay/Drcom4CWNU/drcomrc

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
