#!/bin/sh


INSTANCES=$@

if [[ "$INSTANCES" == "" ]]; then
	$QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial stdio -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $QEMU_FLAGS
	exit
fi

pids=""

instance=0
for i in $INSTANCES; do 
	instance=$((instance + 1))
	echo $instance $i
	IFS=':' read -a cmd <<< "$i"
	waittime="0"
	NET=""
	case ${cmd[0]} in 
		s)
			if [[ "${cmd[1]}" == "l" ]]; then
				NET="-netdev socket,id=net0,listen=:${cmd[2]}"
				waittime="0.5"
			else
				NET="-netdev socket,id=net0,connect=:${cmd[2]}"
			fi
		;;
		t)
			if [[ "${cmd[1]}" == "l" ]]; then
				NET="-netdev socket,id=net0,listen=:${cmd[2]}"
				waittime="0.5"
			else
				NET="-netdev socket,id=net0,connect=:${cmd[2]}"
			fi
		;;
	esac
	NETFLAGS="$NET -device e1000e,netdev=net0"
	echo $NETFLAGS
	echo $QEMU
	echo $QEMU_FLAGS
	$QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial file:twz_serial_$instance -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $NETFLAGS $QEMU_FLAGS &
	pids+=" $!"
	sleep $waittime
done

wait $pids

