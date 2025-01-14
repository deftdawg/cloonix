#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
#----------------------------------------------------------------------#
VMNAME=ams
DIST=bookworm
QCOW2=dns_dhcp.qcow2
BIN_STORE="${HERE}/bin_store"
REPO="http://1.1.1.1/debian/${DIST}"
REPO="http://172.17.0.2/debian/${DIST}"
#######################################################################
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
cp -vf /var/lib/cloonix/bulk/${DIST}.qcow2 /var/lib/cloonix/bulk/${QCOW2}
sync
sleep 1
sync
#######################################################################
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
#######################################################################
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DIST} main contrib non-free non-free-firmware
EOF"
#-----------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
cloonix_ssh ${NET} ${VMNAME} "dhclient eth0"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "DEBIAN_FRONTEND=noninteractive \
                        DEBCONF_NONINTERACTIVE_SEEN=true \
                        apt-get -o Dpkg::Options::=--force-confdef \
                        -o Acquire::Check-Valid-Until=false \
                        --allow-unauthenticated --assume-yes update"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         busybox rsyslog monit bind9-utils bind9 \
                         bind9-dnsutils gcc zip isc-dhcp-server"
#----------------------------------------------------------------------#
cloonix_scp ${NET} -r ${HERE}/build_zip ${VMNAME}:~
cloonix_ssh ${NET} ${VMNAME} "cd /root/build_zip; gcc -o cplibdep lddlist.c"
cloonix_ssh ${NET} ${VMNAME} "cd /root/build_zip; ./create_zip.sh"
cloonix_scp ${NET} ${VMNAME}:/root/dns_dhcp.zip /var/lib/cloonix/bulk
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "poweroff"
sleep 5
set +e
cloonix_cli ${NET} kil
sleep 1
echo END ################################################"

