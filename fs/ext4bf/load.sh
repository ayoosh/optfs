umount /mnt/mydisk
modprobe -r ext4bf
#cd ..
make
make modules_install
cp ./ext4bf.ko /lib/modules/3.2.0vijaycbf-gbde0b1a-dirty/.
echo "Loading module"
modprobe ext4bf
echo "Mounting fs"
mount -t ext4bf -o nodelalloc,nobarrier,nouser_xattr,noacl,data=journal  /dev/sdb1 /mnt/mydisk
echo "Fs contents"
ls /mnt/mydisk/
