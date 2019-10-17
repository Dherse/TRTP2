echo "Spawning 10 senders"
parallel -j0 ./sender -f /media/ramdisk/100MB.zip ::1 5555 ::: {0..99}