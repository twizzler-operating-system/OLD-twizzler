#!/usr/bin/env bash


INSTANCES=$@

QEMU="-cpu host,migratable=false,host-cache-info=true,host-phys-bits -machine
q35,nvdimm=on,kernel-irqchip=on -device intel-iommu,intremap=off,aw-bits=48,x-scalable-mode=true -m
1024,slots=2,maxmem=8G -object memory-backend-file,id=mem1,share=on,mem-path=pmem.img,size=4G
-device nvdimm,id=nvdimm1,memdev=mem1"

if [ ! -f pmem.img ]; then
	touch pmem.img
	truncate -s 4G pmem.img
fi

QEMU_EXTRA='
-device ioh3420,id=root_port1,bus=pcie.0 
-device x3130-upstream,id=upstream1,bus=root_port1 
-device xio3130-downstream,id=downstream1,bus=upstream1,chassis=9 
-device e1000e,bus=downstream1'

#QEMU_EXTRA='
#-device pxb-pcie,id=pcie.1,bus_nr=2,bus=pcie.0
#-device ioh3420,id=pcie_bridge1,bus=pcie.1,chassis=1
#-device e1000,bus=pcie_bridge1'

if [[ "$INSTANCES" == "" ]]; then
	#echo $QEMU
	qemu-system-x86_64  --trace '*ept*' --trace '*kvm_exit*' --trace '*vtx*' --trace '*vmx*' $QEMU -enable-kvm -vnc '0.0.0.0:0' -cdrom boot.iso -serial mon:stdio $QEMU_FLAGS $QEMU_EXTRA
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

	qemu-system-x86_64 $QEMU -enable-kvm -cdrom $BUILDDIR/boot.iso -serial mon:unix:twz_serial_$instance.sock -drive file=$BUILDDIR/us/nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234,share-rw=on $NETFLAGS $QEMU_FLAGS &
	pids+=" $!"
	sleep $waittime
done

wait $pids

