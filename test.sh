#!/bin/bash

. assert.sh/assert.sh

# make fix
make run > /dev/null 2>&1 &

sleep 0.5      # timeout

Xs=$(printf 'X%.0s' {1..1000})

assert_raises "echo test123 > mnt/file1"
assert        "cat mnt/file1" "test123"
assert_end read_write

assert_raises "echo $Xs > mnt/b"
assert        "cat mnt/b" "$Xs"
assert_end read_write_long  # TODO even bigger

assert_raises "echo test789 > mnt/file1"
assert        "cat mnt/file1" "test789"
assert_end overwrite

assert_raises "ln mnt/file1 mnt/c"
assert        "cat mnt/c" "test789"
assert_end ln

assert        "mkdir mnt/dir1"
assert        "mkdir mnt/dir2"
assert_raises "ls -la mnt/dir1 | grep \\.."
assert        "mkdir mnt/dir1/dir2"
assert_end mkdir

assert_raises "rm mnt/c"
assert_raises "cat mnt/c" 1
assert        "cat mnt/file1" "test789"
assert_raises "rm mnt/dir1" 1
assert_end rm

assert_raises "rmdir mnt/x" 1       # does not exist
assert_raises "rmdir mnt/b" 1       # not dir
assert_raises "rmdir mnt/dir1" 1    # not empty
assert_raises "rmdir mnt/dir1/dir2"
assert_raises "rmdir mnt/dir1"
assert_end rmdir

assert_raises "ls -la mnt/dir1 | grep \\.\\." 1
assert_raises "ls -la mnt/dir2 | grep \\.\\."
assert_end ls

assert_raises "chmod 777 mnt/file1"
assert_raises "ls -la mnt/file1 | grep -- -rwxrwxrwx" 0
assert_raises "chmod 000 mnt/file1"
assert_raises "ls -la mnt/file1 | grep -- ----------" 0
assert_raises "chmod 124 mnt/file1"
assert_raises "ls -la mnt/file1 | grep -- ---x-w-r--" 0
assert_raises "chmod 777 mnt/x" 1   # does not exist
assert_end chmod

assert_raises "df -ha mnt"
assert_end df

# chown

killall main
