pushd /mnt/Work

git clone https://github.com/F-Stack/f-stack.git
sudo apt install libnuma-dev python-is-python3 python3-pip meson -y
pip3 install pyelftools --upgrade
pushd f-stack

cd dpdk
meson -Denable_kmods=true build
cd build
meson configure
ninja
sudo ninja install
sudo ldconfig
echo 1024 | sudo tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

sudo mkdir /mnt/huge
sudo mount -t hugetlbfs nodev /mnt/huge

sudo su
echo 0 > /proc/sys/kernel/randomize_va_space
exit

sudo modprobe uio
sudo insmod /mnt/Work/f-stack/dpdk/build/kernel/linux/igb_uio/igb_uio.ko
sudo insmod /mnt/Work/f-stack/dpdk/build/kernel/linux/kni/rte_kni.ko carrier=on
python /mnt/Work/dpdk/usertools/dpdk-devbind.py --status
ifconfig eth0 down
python /mnt/Work/dpdk/usertools/dpdk-devbind.py --bind=igb_uio eth0 
# Some stuff missing in between

sudo apt-get install gawk
sudo apt install gcc make libssl-dev -y

export FF_PATH=/mnt/Work/f-stack
# export PKG_CONFIG_PATH=/usr/lib64/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib/pkgconfig

cd /mnt/Work/f-stack/lib/
make -j10
sudo make install