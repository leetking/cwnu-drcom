#!/bin/bash

# 自动配置drcom和wifi
DRCOMRC=/overlay/cwnu-drcom/drcomrc
WIFIRC=/etc/config/wireless
NETWORKRC=/etc/config/network

IP=192.168.1.1

read -p "输入drcom账户: " drcom_name
if [ ! ${drcom_name} ]; then
    drcom_name=201413640731
fi
read -p "输入drcom密码: " drcom_pwd
if [ ! ${drcom_pwd} ]; then
    drcom_pwd=testpwd
fi

read -p "输入wifi名字: " wifi_ssid
if [ ! ${wifi_ssid} ]; then
    wifi_ssid=vmbox709
fi
read -p "输入wifi密码(默认是drcom账户后8位): " wifi_pwd
read -p "是否开启隐藏wifi名字?[N/y]: " wifi_ishide
if [ ! ${wifi_pwd} ]; then
    wifi_pwd=${drcom_name:4}
fi
case ${wifi_ishide} in
    Y|y)
        wifi_ishide=1
        ;;
    *)
        wifi_ishide=0
        ;;
esac

config_sh=drcom-config-$$.sh
cat << EOF > $config_sh
PATH=\$PATH:/bin/:/etc/init.d/
#sed -i "/uname/s/\"[^\"]*\"/\"${drcom_name}\"/" ${DRCOMRC}
#sed -i "/pwd/s/\"[^\"]*\"/\"${drcom_pwd}\"/" ${DRCOMRC}
sed -i "s/option country .*/option country 'CN'/" ${WIFIRC}
#sed -i "s/option ssid .*/option ssid '${wifi_ssid}'/" ${WIFIRC}
#sed -i "s/option hidden .*/option hidden '${wifi_ishide}'/" ${WIFIRC}
sed -i "s/option encryption .*/option encryption 'psk2'/" ${WIFIRC}
sed -i "s/option key .*/option key '${wifi_pwd}'/" ${WIFIRC}
#sed -i "/'wan'/,/\toption macaddr.*/s/option macaddr .*/option macaddr '${mac}'/" ${NETWORKRC}
sed -i  "/option macaddr.*/d" ${NETWORKRC}
sed -i "/'wan'/,/\toption macaddr.*/s/option proto .*/option proto 'dhcp'/" ${NETWORKRC}
#echo "restarting network..."
#/etc/init.d/network restart
EOF

scp $config_sh root@${IP}:/tmp
ssh root@${IP} "sh /tmp/$config_sh"

rm -f $config_sh

exit 0
