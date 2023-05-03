#!/bin/bash

if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root" 1>&2
    exit 1
fi

IF_NAME=ens1f0
FSTACK_DIR=/mnt/Work/f-stack
CONF_FILE=/mnt/Work/Networks/Project/DPDK/app/config.ini

echo 1024 | tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
mount -t hugetlbfs nodev /mnt/huge
echo 0 > /proc/sys/kernel/randomize_va_space

modprobe uio
modprobe hwmon
insmod ${FSTACK_DIR}/dpdk/build/kernel/linux/igb_uio/igb_uio.ko
insmod ${FSTACK_DIR}/dpdk/build/kernel/linux/kni/rte_kni.ko carrier=on

export myaddr=`ifconfig $IF_NAME | grep "inet" | grep -v ":" | awk -F ' '  '{print $2}'`
export mymask=`ifconfig $IF_NAME | grep "netmask" | awk -F ' ' '{print $4}'`
export mybc=`ifconfig $IF_NAME | grep "broadcast" | awk -F ' ' '{print $6}'`
export myhw=`ifconfig $IF_NAME | grep "ether" | awk -F ' ' '{print $2}'`
export mygw=`route -n | grep 0.0.0.0 | grep $IF_NAME | grep U | awk -F ' ' '{print $2}'`

sed "s/addr=192.168.1.2/addr=${myaddr}/" -i ${CONF_FILE}
sed "s/netmask=255.255.255.0/netmask=${mymask}/" -i ${CONF_FILE}
sed "s/broadcast=192.168.1.255/broadcast=${mybc}/" -i ${CONF_FILE}
sed "s/gateway=192.168.1.1/gateway=${mygw}/" -i ${CONF_FILE}

sed "s/#\[kni\]/\[kni\]/" -i ${CONF_FILE}
sed "s/#enable=1/enable=1/" -i ${CONF_FILE}
sed "s/#method=reject/method=reject/" -i ${CONF_FILE}
sed "s/#tcp_port=80/tcp_port=80/" -i ${CONF_FILE}
sed "s/#vlan_strip=1/vlan_strip=1/" -i ${CONF_FILE}

echo "Updated Config file: ${CONF_FILE}"