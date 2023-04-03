sudo apt-get update
sudo apt-get install meson python3-pyelftools -y
wget https://content.mellanox.com/ofed/MLNX_OFED-4.9-5.1.0.0/MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.tgz
tar -xvzf MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.tgz
cd MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64
sudo ./mlnxofedinstall --upstream-libs --dpdk
sudo /etc/init.d/openibd restart
ibv_devinfo
cd /mnt/Work/
git clone https://github.com/DPDK/dpdk
cd dpdk
meson build -Dexamples=all
cd build
meson configure
ninja
sudo ninja install
sudo ldconfig
echo 1024 | sudo tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

