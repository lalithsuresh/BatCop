#!/bin/sh

if [ -z "$1" ];
then
echo "Usage: "$0" 192.168.206.1/24"
exit
fi


# Number of seconds snort should run for
seconds=6

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
cat log/alert | grep "Abnormal_TCP_Upload" | awk '{print $4,substr($9,1,match($9, ":")-1),substr($11, 1, match($11, ":")-1)}' | sort | uniq > log/attacks_detected.txt
fi

# Taking actions on the attacks if configured in recovery.conf 

#src_IP_list=`cat log/attacks_detected.txt |  awk '{print $2}'`
#for IPaddr in $src_IP_list
#do
#if [ $IPaddr != `./getIP.sh` ]; then #this comparison is to prevent adding ourselves to IP
#echo $IPaddr
##block_icmp.sh $IPaddr
#fi
#done

attacks=`cat log/attacks_detected.txt`
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



# Cleaning up and Exiting
rm -f subnet.txt
rm -f myrules_temp.conf
exit
