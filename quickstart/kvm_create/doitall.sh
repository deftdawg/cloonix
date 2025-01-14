#!/bin/bash
set -e

LIST="fedora39 \
      centos9  \
      jammy    \
      bookworm \
      opensuse155"

sudo echo "sudoer rights given"
mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.qcow2 ]; then
    echo /var/lib/cloonix/bulk/${i}.qcow2
    echo already exists
  else
    echo BEGIN ${i} 
    sudo ./${i}
    echo END ${i}  
  fi
done
