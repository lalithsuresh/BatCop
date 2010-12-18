#!/bin/sh

## Script for detecting if there are any network floods going on - (uses snort rules)

cd attack_detection
logfile="/var/log/batcop/detect_flood.log"
interface=wlan0
sudo touch $logfile

#reading the subnet value from conf file
subnetCIDR=`cat ../conf/subnetCIDR.conf`

# Number of seconds snort should run for
seconds=6
# Scripts to be executed on attack detection
recovery_scripts_file="../conf/recovery.conf"

# Removing previous snort files
sudo rm -f log/alert

# Adding the current subnet from "subnetCIDR.conf" to the snort rules file dynamically
subnet=$subnetCIDR
snortrules="../conf/myrules.conf"
echo "var HOME_NET "$subnet > subnet.txt 
cat subnet.txt $snortrules > myrules_temp.conf

# Starting snort in daemon mode and then sleeping for a few seconds
sudo snort -d -l ./log -c myrules_temp.conf  -A fast -D -i $interface
sleep $seconds 

# Killing snort after sleeping for $seconds
sudo killall snort

# Outputting the snort results
if [ `cat log/alert | wc -l` -eq 0  ];
then
echo [`date`]" Boring network, no attacks detected." | tee -a $logfile
else
	#commented to fix uniq problem cat log/alert | awk '{print $4","$9","$11}' | sort | uniq > log/attacks_detected.txt
	#cat log/alert | awk '{print $4","substr($9,1,index($9,":")-1)","substr($11,1,index($11,":")-1)}' | sort | uniq > log/attacks_detected.txt
	cat log/alert | awk '{print $4","$9","$11}' | awk -F',' '{
         flag = match($2, ":")
         if (flag)
           print $1","substr($2,1,index($2,":")-1)","substr($3,1,index($3,":")-1)
        else
          print $1","$2","$3
	}' | sort | uniq > log/attacks_detected.txt
	sudo cat log/attacks_detected.txt | awk '{print d,$0}' "d=$(date +%s)" >> $logfile

	# Taking actions on the attacks if configured in recovery.conf 
	attacks=`cat log/attacks_detected.txt`
	for attack in $attacks
		do
		#echo $attack
		attack_type=`echo $attack | awk -F',' '{print $1}'`
		attack_src=`echo $attack | awk -F',' '{print $2}' | awk -F':' '{print $1}'`
		attack_dst=`echo $attack | awk -F',' '{print $3}' | awk -F':' '{print $1}'`
		if [ "$attack_src" != `./getIP.sh` ];
		then
			sudo echo `date +%s`" "$attack_type" detected from "$attack_src >> $logfile
			list_of_scripts=`cat $recovery_scripts_file | grep $attack_type | awk '{print $2}'`
	       	for script in $list_of_scripts
               	do
	       		sudo ./$script $attack_src >> $logfile
              	done
		fi
		done
fi

# Cleaning up and Exiting
rm -f subnet.txt
rm -f myrules_temp.conf
exit
