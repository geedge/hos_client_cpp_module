#########################################################################
# File Name: c.sh
# Author: pxz
# Created Time: Tue 13 Oct 2020 06:28:57 PM CST
#########################################################################
#!/bin/bash
size=("1k" "10k" "100k" "1M" "2M" "3M" "4M" "5M" "10M" "20M" "30M")

num=0

while((${num} < 11))
do
    truncate -s ${size[$num]} ${size[$num]}.data
    let "num++"
done
