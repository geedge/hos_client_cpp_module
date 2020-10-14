#########################################################################
# File Name: test_times.sh
# Author: pxz
# Created Time: Mon 21 Sep 2020 04:43:35 PM CST
#########################################################################
#!/bin/bash
test_size=("1k" "10k" "100k" "1M" "2M" "3M" "4M")
num=0
echo ${test_size[${num}]}

while((${num} < 7))
do
    ./hos_write_complete mybucket ./data/${test_size[$num]}.data 1000
    let "num++"
done

