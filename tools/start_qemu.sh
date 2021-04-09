#!/usr/bin/env bash


INSTANCES=$@

QEMU="qemu-system-x86_64 -cpu host,migratable=false,host-cache-info=true,host-phys-bits -machine
q35,nvdimm,kernel-irqchip=on -device intel-iommu,intremap=off,aw-bits=48,x-scalable-mode=true -m
1024,slots=2,maxmem=8G -object memory-backend-file,id=mem1,share=on,mem-path=pmem.img,size=4G
-device nvdimm,id=nvdimm1,memdev=mem1"

if [ ! -f pmem.img ]; then
	touch pmem.img
	truncate -s 4G pmem.img
fi

if [[ "$INSTANCES" == "" ]]; then
	#echo $QEMU
	$QEMU -enable-kvm -vnc '0.0.0.0:0' -cdrom boot.iso -serial mon:stdio $QEMU_FLAGS
	#-device nvme,drive=D22,serial=1234,share-rw=on $QEMU_FLAGS
	#-drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 
	exit
fi

pids=""

instance=0
for i in $INSTANCES; do 
	instance=$((instance + 1))
	IFS=',' read -a cmd <<< "$i"
	waittime="0"
	NET=""
	mac=""
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

	$QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial mon:unix:twz_serial_$instance.sock -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $NETFLAGS $QEMU_FLAGS &
	pids+=" $!"
	sleep $waittime
done

wait $pids

