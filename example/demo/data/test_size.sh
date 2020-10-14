#########################################################################
# File Name: test_times.sh
# Author: pxz
# Created Time: Mon 21 Sep 2020 04:43:35 PM CST
#########################################################################
#!/bin/bash
test_size=("1k" "10k" "100k" "1M" "2M" "3M" "4M" "5M" "10M" "20M" "30M")
#test_size=("1k" "2k" "3k" "4k" "5k" "6k" "7k" "8k" "9k" "10k" "11k" "12k" "13k" "14k" "15k")
num=0

#while((${num} < 15))
while((${num} < 11))
do
    echo ./hos_write_complete mybucket ${test_size[$num]}.data 1000
    ./hos_write_complete mybucket ${test_size[$num]}.data 1000
    #echo ./hos_upload_complete mybucket ${test_size[$num]}.data 1000
    #./hos_upload_complete mybucket ${test_size[$num]}.data 1000
    let "num++"
done

