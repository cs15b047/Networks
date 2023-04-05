sudo apt-get update
sudo apt-get install meson python3-pyelftools -y

pushd /mnt/Work/
wget https://content.mellanox.com/ofed/MLNX_OFED-4.9-5.1.0.0/MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.tgz
tar -xvzf MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.tgz
pushd MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64
sudo ./mlnxofedinstall --upstream-libs --dpdk
sudo /etc/init.d/openibd restart
ibv_devinfo
popd
popd
