echo "Spawning 10 senders"
parallel -j0 ./sender -f ../1GB.zip ::1 5555 ::: {0..2}