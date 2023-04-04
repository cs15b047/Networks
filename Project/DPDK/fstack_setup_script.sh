ifname=ens1f0
FSTACK_DIR=/mnt/Work/f-stack

echo 1024 | tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
mount -t hugetlbfs nodev /mnt/huge
echo 0 > /proc/sys/kernel/randomize_va_space

modprobe uio
modprobe hwmon
insmod ${FSTACK_DIR}/dpdk/build/kernel/linux/igb_uio/igb_uio.ko
insmod ${FSTACK_DIR}/dpdk/build/kernel/linux/kni/rte_kni.ko carrier=on

export myaddr=`ifconfig $ifname | grep "inet" | grep -v ":" | awk -F ' '  '{print $2}'`
export mymask=`ifconfig $ifname | grep "netmask" | awk -F ' ' '{print $4}'`
export mybc=`ifconfig $ifname | grep "broadcast" | awk -F ' ' '{print $6}'`
export myhw=`ifconfig $ifname | grep "ether" | awk -F ' ' '{print $2}'`
export mygw=`route -n | grep 0.0.0.0 | grep $ifname | grep UG | awk -F ' ' '{print $2}'`

sed "s/addr=192.168.1.2/addr=${myaddr}/" -i ${FSTACK_DIR}/config.ini
sed "s/netmask=255.255.255.0/netmask=${mymask}/" -i ${FSTACK_DIR}/config.ini
sed "s/broadcast=192.168.1.255/broadcast=${mybc}/" -i ${FSTACK_DIR}/config.ini
sed "s/gateway=192.168.1.1/gateway=${mygw}/" -i ${FSTACK_DIR}/config.ini

sed "s/#\[kni\]/\[kni\]/" -i ${FSTACK_DIR}/config.ini
sed "s/#enable=1/enable=1/" -i ${FSTACK_DIR}/config.ini
sed "s/#method=reject/method=reject/" -i ${FSTACK_DIR}/config.ini
sed "s/#tcp_port=80/tcp_port=80/" -i ${FSTACK_DIR}/config.ini
sed "s/#vlanstrip=1/vlanstrip=1/" -i ${FSTACK_DIR}/config.ini

export FF_PATH=${FSTACK_DIR}


ifconfig $ifname down
python3 $FSTACK_DIR/dpdk/usertools/dpdk-devbind.py --bind=igb_uio $ifname
cd $FSTACK_DIR
./start.sh &

# start kni
# sleep 2
# echo $myaddr $mymask $mybc $myhw $mygw

# modprobe dummy
# ip link add $vifname type dummy
# ip link show $vifname
# ifconfig $vifname ${myaddr}  netmask ${mymask}  broadcast ${mybc} hw ether ${myhw}
# ip link set dev $vifname up
# route add -net 0.0.0.0 gw ${mygw} dev $vifname
# echo 1 > /sys/class/net/$vifname/carrier # if `carrier=on` not set while `insmod rte_kni.ko`.