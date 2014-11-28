#!/bin/sh
#
# file:        q1test.sh
# description: command-line read-only tests for CS5600 Homework 3
#
# Peter Desnoyers, Northeastern CCIS, 2012
# $Id: q1test.sh 83 2009-12-07 02:53:34Z pjd $
#

if [ x$1 = x-v ] ; then
    verbose=t; shift
fi

cmd=./homework
origdisk=disk1.img.orig
disk=/tmp/$USER-disk1.img

for f in $cmd $origdisk; do
  if [ ! -f $f ] ; then
      echo "Unable to access: $f"
      exit
  fi
done
cp $origdisk $disk

echo Question 1 - cmdline mode, read-only

$cmd --cmdline $disk > /tmp/output-$$ <<EOF
ls
ls-l file.txt
show file.txt
show another-file
cd home
ls-l small-2
show small-1
cd ..
cd work/dir-1
ls
pwd
show small-3
statfs
EOF
echo >> /tmp/output-$$
[ "$verbose" ] && echo wrote /tmp/output-$$

diff - /tmp/output-$$ <<EOF
read/write block size: 1000
cmd> ls
another-file
dir_other
file.txt
home
work
cmd> ls-l file.txt
/file.txt -rw-r--r-- 1800 2
cmd> show file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
file.txt file.txt file.txt file.txt file.txt file.txt file.txt file.txt
cmd> show another-file
another another another another another another another another another
another another another another another another another another another
another another another another another another another another another
another another another another another another another another another
another another another another another another another another another
another another another another another
cmd> cd home
cmd> ls-l small-2
/home/small-2 -rw-r--r-- 144 1
cmd> show small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1 small-1
cmd> cd ..
cmd> cd work/dir-1
cmd> ls
small-3
small-4
small-5
cmd> pwd
/work/dir-1
cmd> show small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3
cmd> statfs
max name length: 43
block size: 1024
cmd> 
EOF

if [ $? != 0 ] ; then
    echo 'Tests may have failed - see above output for details'
else
    echo 'Tests passed - output matches exactly'
fi
[ "$verbose" ] || rm -f /tmp/output-$$
