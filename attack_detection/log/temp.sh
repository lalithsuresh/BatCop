#!/bin/sh

#src_IP_list=`cat attacks_detected.txt |  awk '{print $2}'`
attacks=`cat attacks_detected.txt`

for attack in $attacks
do
#echo $attack
attack_type=`echo $attack | awk -F',' '{print $1}'`
attack_src=`echo $attack | awk -F',' '{print $2}'`
attack_dst=`echo $attack | awk -F',' '{print $3}'`
if [ "$attack_src" != `./getIP.sh` ];
then
echo $attack_src" is flooding: "$attack_type" (TODO: Preventive measures to be taken here)"
#TODO:fetch the list of scripts for $attack_type from recovery.conf and execute them like below:
#block_icmp.sh $attack_src
#block_tcp.sh $attack_src 
#
fi
done


exit
