cd /mnt/Work
git clone git@github.com:F-Stack/f-stack.git
sudo apt-get install libnuma-dev -y
sudo apt install python3-pip -y
pip3 install pyelftools --upgrade
sudo apt install python -y
cd f-stack/
cd dpdk/
meson -Denable_kmods=true build
ninja -C build
sudo ninja -C build install
sudo mkdir /mnt/huge
sudo mount -t hugetlbfs nodev /mnt/huge

sudo su
echo 0 > /proc/sys/kernel/randomize_va_space
exit

sudo modprobe uio
sudo insmod ./build/kernel/linux/igb_uio/igb_uio.ko
sudo insmod /mnt/Work/f-stack/dpdk/build/kernel/linux/kni/rte_kni.ko carrier=on

# Some stuff missing in between

sudo apt-get install gawk
sudo apt install gcc make libssl-dev -y

export FF_PATH=/mnt/Work/f-stack
export PKG_CONFIG_PATH=/usr/lib64/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib/pkgconfig

cd /mnt/Work/f-stack/lib/
make
sudo make install