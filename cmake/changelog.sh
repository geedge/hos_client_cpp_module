#########################################################################
# File Name: changelog.sh
# Author: pxz
# Created Time: Mon 31 Aug 2020 10:06:56 AM CST
#########################################################################
#!/bin/bash
branch=`git status | grep branch | awk '{print $NF}'`
git log --branches=$branch --no-merges --date=local --show-signature --pretty="* %ad %an %ae %nhash: %H%ncommit:%n%B"  | awk -F"-" '{print "- "$0}' | sed 's/- \*/\*/g' | sed 's/- $//g' | sed 's/-/  -/g' | sed 's/[0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\}//g' > changelog.txt

