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
	echo $attack_type" detected from "$attack_src
	list_of_scripts=`cat recovery.conf | grep $attack_type | awk '{print $2}'`
	for script in $list_of_scripts
		do
		$script $attack_src
		done
	fi
done


exit
