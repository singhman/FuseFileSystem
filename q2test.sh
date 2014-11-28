#!/bin/sh
#
# file:        q2test.sh
# description: command-line read-write tests for CS5600 Homework 3
#
# Peter Desnoyers, Northeastern CCIS, 2012
# $Id: q2test.sh 83 2009-12-07 02:53:34Z pjd $
#

[ "x$1" = x-save ] && save=1

cmd=./homework
origdisk=disk1.img.orig
disk=/tmp/disk-$$.img

for f in $cmd $origdisk; do
  if [ ! -f $f ] ; then
      echo "Unable to access: $f"
      exit
  fi
done

echo Question 2 - cmdline mode, read/write

cp $origdisk $disk

yes 'test file 1' | head -20 | fmt > /tmp/$$-file1

$cmd --cmdline $disk > /tmp/output-$$ <<EOF
mkdir dir1
cd dir1
put /tmp/$$-file1 file1.txt
show file1.txt
cd ..
rm file.txt
ls
cd work
rmdir dir-1
rmdir dir-2
ls
quit
EOF
echo >> /tmp/output-$$
diff -c /tmp/output-$$ - <<EOF
read/write block size: 1000
cmd> mkdir dir1
cmd> cd dir1
cmd> put /tmp/$$-file1 file1.txt
cmd> show file1.txt
test file 1 test file 1 test file 1 test file 1 test file 1 test file
1 test file 1 test file 1 test file 1 test file 1 test file 1 test file
1 test file 1 test file 1 test file 1 test file 1 test file 1 test file
1 test file 1 test file 1
cmd> cd ..
cmd> rm file.txt
cmd> ls
another-file
dir1
dir_other
home
work
cmd> cd work
cmd> rmdir dir-1
error: Directory not empty
cmd> rmdir dir-2
cmd> ls
dir-1
cmd> quit

EOF
if [ $? != 0 ] ; then
    echo 'Tests may have failed - see above output for details'
else
    echo 'Tests passed - output matches exactly'
fi
if [ "$save" ] ; then
    echo disk: $disk
    echo output: /tmp/output-$$
else
    rm -f /tmp/output-$$ $disk
fi
