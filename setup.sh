sudo mount -t tmpfs -o size=48G tmpfs /media/ramdisk 

sudo ip link set lo txqueuelen 10000
sudo sysctl -w net.core.rmem_max=12582912
sudo sysctl -w net.core.wmem_max=12582912
sudo sysctl -w net.core.netdev_max_backlog=5000
