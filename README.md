# sfs

A simple, small file system aimed at embedded devices.
Design heavily inspired by UNIX file systems.

## Features

Some features include:
- Users and groups
- UNIX-style permissions
- Timestamps
- Links
- FUSE driver
- No OS required

## Usage

See `Makefile`.

## Todo

- Improve inode block references
- Improve error logging
- Improve error handling
- Implement permissions
- Implement move
- Implement links
- Support for inlining small files into inode
