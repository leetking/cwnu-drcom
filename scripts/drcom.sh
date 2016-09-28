#!/bin/sh /etc/rc.common
# /init.d/cwnu-drcom

CWNU_DRCOM_PATH=/overlay/cwnu-drcom

#writed for openwrt to implement auto login

START=98

start() {
    #dump login-msg to drcom-login.msg file.
    ${CWNU_DRCOM_PATH}/drcom -d
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
