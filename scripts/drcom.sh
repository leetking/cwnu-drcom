#!/bin/sh /etc/rc.common
# /init.d/cwnu-drcom

CWNU_DRCOM_PATH=/overlay/cwnu-drcom

#writed for openwrt to implement auto login

START=99

start() {
    ${CWNU_DRCOM_PATH}/drcom
}
restart() {
    ${CWNU_DRCOM_PATH}/drcom -r
}
stop() {
    ${CWNU_DRCOM_PATH}/drcom -l
}
status() {
    ${CWNU_DRCOM_PATH}/drcom -s
}
