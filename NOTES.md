

rmmod aesdcircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.kocircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.kocircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.kocircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.kocircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.kodcircular || true && make -C aesd-char-driver/ && insmod aesd-char-driver/aesdcircular.ko



make clean && make && ./aesdchar_unload && ./aesdchar_load && echo "abcd" > /dev/aesdchar &&  dd if=/dev/aesdchar skip=3 of=/home/btardio/assignment-9-btardio/aesd_char/tmpoutfile bs=1


