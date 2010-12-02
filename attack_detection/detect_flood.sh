#!/bin/sh

if [ -z "$1" ];
then
echo "Usage: "$0" 192.168.206.1/24"
exit
fi


# Number of seconds snort should run for
seconds=6
# Scripts to be executed on attack detection
recovery_scripts_file="recovery.conf"

# Removing previous snort files
sudo rm -f log/alert

# Adding the subnetwork value in myrules.conf
subnet=$1
echo "var HOME_NET "$subnet > subnet.txt 
cat subnet.txt myrules.conf > myrules_temp.conf

# Starting snort and then sleeping for a few seconds
sudo snort -d -l ./log -c myrules_temp.conf  -A fast -D
sleep $seconds 

# Killing snort - there should be a better way of doing this
sudo killall snort

# Outputting the snort results
if [ `cat log/alert | wc -l` -eq 0  ];
then
echo "** Boring network, no attacks detected."
else
#cat log/alert | awk '{print $4","substr($9,1,match($9, ":")-1)","substr($11, 1, match($11, ":")-1)}' | sort | uniq > log/attacks_detected.txt
cat log/alert | awk '{print $4","$9","$11}' | sort | uniq > log/attacks_detected.txt
cat log/attacks_detected.txt 
# Taking actions on the attacks if configured in recovery.conf 
attacks=`cat log/attacks_detected.txt`
for attack in $attacks
do
#echo $attack
attack_type=`echo $attack | awk -F',' '{print $1}'`
attack_src=`echo $attack | awk -F',' '{print $2}'`
attack_dst=`echo $attack | awk -F',' '{print $3}'`
if [ "$attack_src" != `./getIP.sh` ];
then
echo $attack_type" detected from "$attack_src

	list_of_scripts=`cat $recovery_scripts_file | grep $attack_type | awk '{print $2}'`
        for script in $list_of_scripts
               do
               ./$script $attack_src
               done

fi
done

fi

# Cleaning up and Exiting
rm -f subnet.txt
rm -f myrules_temp.conf
exit
