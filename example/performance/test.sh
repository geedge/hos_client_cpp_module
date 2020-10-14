#########################################################################
# File Name: test.sh
# Author: pxz
# Created Time: Tue 13 Oct 2020 02:28:31 PM CST
#########################################################################
#!/bin/bash
size=(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 200, 300, 400, 500)

num=0

while((${num} < 37))
do
    #./HosClientPerformance -a ${size[$num]}
    echo "./HosClientPerformance -a ${size[$num]} -A -f ./file/1M.data"
    ./HosClientPerformance -a ${size[$num]} -A -f ./file/1M.data
    let "num++"
done
