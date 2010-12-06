#!/bin/sh
IPaddr=$1
#list_of_IPs=`nmap -sP $1 -oG - | grep "Host:" | awk '{print $2}'`
sudo ping $IPaddr -f -i 0.01
exit

