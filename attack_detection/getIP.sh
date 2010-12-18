#!/bin/sh
OS=`uname`
interface=wlan0
IO="" # store IP
case $OS in
   Linux) IP=`ifconfig  | grep 'inet addr:'| grep -v '127.0.0.1' | cut -d: -f2 | awk '{ print $1}'`;;
   *) IP="Unknown";;
esac
echo "$IP"
