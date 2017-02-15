#!/bin/sh /etc/rc.common

# post `wifissid' and `wifipwd' from /etc/config/drcomrc to `/etc/config/wireless'
# GPL v2
# (C) leetking <li_Tking@163.com>
# locate at /overlay/Drcom4CWNU/wr2wireless.sh

DRCOMRC=drcomrc
SECTION_NAME=setting
PATH=$PATH:/sbin/

parse_conf() {
    local wifissid
    local wifipwd
    config_get wifissid $1 wifissid
    config_get wifipwd $1 wifipwd
    echo "wifissid: '$wifissid'"
    echo "wifipwd : '$wifipwd'"
    uci set wireless.@wifi-iface[0].ssid="${wifissid}"
    uci set wireless.@wifi-iface[0].key="${wifipwd}"
    uci set wireless.@wifi-iface[0].hidden="0"
    uci set wireless.@wifi-iface[0].country="CN"
    uci set wireless.@wifi-iface[0].encryption="psk2"
    uci commit wireless
    echo "modify \`wireless\' success!\n"
    uci export wireless
}

start() {
    config_load $DRCOMRC
    config_foreach parse_conf $SECTION_NAME
}
