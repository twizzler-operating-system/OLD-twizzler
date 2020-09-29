#!/usr/bin/env bash


INSTANCES=$@

if [ ! -f $BUILDDIR/pmem.img ]; then
	touch $BUILDDIR/pmem.img
	truncate -s 4G $BUILDDIR/pmem.img
fi

if [[ "$INSTANCES" == "" ]]; then
	echo $QEMU
	$QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial mon:stdio -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $QEMU_FLAGS
	exit
fi

pids=""

instance=0
for i in $INSTANCES; do 
	instance=$((instance + 1))
	echo $instance $i
	IFS=',' read -a cmd <<< "$i"
	waittime="0"
	NET=""
	mac=""
	echo $cmd
	case ${cmd[0]} in 
		's')
			mac=${cmd[3]}
			if [[ "${cmd[1]}" == "l" ]]; then
				NET="-netdev socket,id=net0,listen=:${cmd[2]}"
				waittime="0.5"
			else
				NET="-netdev socket,id=net0,connect=:${cmd[2]}"
			fi
		;;
		't')
			mac=${cmd[2]}
			NET="-netdev tap,id=net0,ifname=${cmd[1]},script=no,downscript=no,vnet_hdr=off"
		;;
	esac
	if [[ "$mac" == "" ]]; then
		NETFLAGS="$NET -device e1000e,netdev=net0"
	else
		NETFLAGS="$NET -device e1000e,netdev=net0,mac=$mac"
	fi
	NETFLAGS="$NETFLAGS -object filter-dump,id=filter0,netdev=net0,file=twz_packetdump_$instance.dat"
	#echo $NETFLAGS
	#echo $QEMU
	#echo $QEMU_FLAGS

	$QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial file:twz_serial_$instance.txt -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $NETFLAGS $QEMU_FLAGS &
	pids+=" $!"
	sleep $waittime
done

wait $pids

