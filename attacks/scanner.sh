#!/bin/sh
IPrange=$1
if [ -z "$IPrange" ];
then
echo "Usage: "$0" IPrange"
else
#echo "[INFO] Carrying out nmap ping scan on "$IPrange > 2
list_of_IPs=`nmap -sP $1 -oG - | grep "Host:" | awk '{print $2}'`
#echo "[INFO] Following hosts identified"
for i in $list_of_IPs
do
echo $i
done 
exit
fi
