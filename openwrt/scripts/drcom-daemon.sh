#!/bin/sh /etc/rc.common

# cwnu-drcom deamon process
# GPL v2
# (C) leetking <li_Tking@163.com>

PATH=$PATH:/etc/init.d/

CWNU_DRCOM_PATH=/etc/init.d/cwnu-drcom

#record status
SLEEPTIME=60
RECONCNT=0

isconnect() {
    local url=baidu.com

    ping -q -c 3 ${url} > /dev/null 2> /dev/null
    local status=$?
    return ${status}
}

_empty() {
    return 0
}

cwnu_daemon() {
    if isconnect; then
        SLEEPTIME=60
        RECONCNT=0
        _empty
        #sleep 7 mins
        sleep 420
    else
        RECONCNT=`expr ${RECONCNT} + 1`
        echo "${RECONCNT}-th restarting cwnu-drcom..."
        ${CWNU_DRCOM_PATH} start
        echo "sleep ${SLEEPTIME}s..."
        sleep ${SLEEPTIME}
        if [ ${RECONCNT} -gt 2 -a ${SLEEPTIME} -lt 86400 ]; then
            SLEEPTIME=`expr ${SLEEPTIME} + ${SLEEPTIME}`
        fi
    fi
}

_pass() {
    while true; do
        cwnu_daemon
    done > /tmp/cwnu_daemon.log &
}

# generate a random mac address
_set_random_mac() {
    local networkrc=/etc/config/network
    # `random_mac' is a one writed by my using c.
    local mac=`random_mac`
    echo "mac is $mac"
    sed -i "/'wan'/,/^\s*$/{/.*macaddr.*/d}; /'wan'/a\	option macaddr '${mac}'" ${networkrc}
}

start() {
    _set_random_mac > /tmp/_random_mac.log
    network restart
    _pass
}

