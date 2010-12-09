#!/bin/sh
IPrange=`cat ../conf/subnetCIDR.conf`
list_of_IPs=`nmap -sP $IPrange -oG - | grep "Status: Up" | awk '{print $2}'`
for i in $list_of_IPs
do
echo $i
done 
exit
