sudo mkfs.ext4 /dev/nvme0n1p4
sudo mkdir -p /mnt/Work
sudo mount /dev/nvme0n1p4 /mnt/Work/
sudo chmod 777 -R /mnt/Work/
ssh-keygen -t rsa
cat ~/.ssh/id_rsa.pub

cd /mnt/Work/
git clone git@github.com:cs15b047/Networks.git