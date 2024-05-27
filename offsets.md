 16 bit /  2 B ino
128 bit / 16 B inode

size    member      offset
  16    ino             16
   8    uid             24
   8    gid             32
  16    type + mode     48
  32    size            80
  32    time           112
  16    block_p        128

 16 bit /  2 B ino
256 bit / 32 B inode

size    member      offset
  16    ino             16
   8    uid             24
   8    gid             32
  16    type + mode     48
  16    refs            64
  32    size            96
  32    time           128
  16    block_0        144
  16    block_1        160
  16    block_2        176
  16    block_3        192
  16    block_p        208
  16    block_pp       224
  16    block_ppp0     240
  16    block_ppp1     256

 32 bit /  4 B ino
256 bit / 32 B inode

size    member      offset
  32    ino             32
   8    uid             38
   8    gid             46
  16    type + mode     64
  32    size            96
  32    time           128
  32    block          160
  32    block_p        192
  32    block_pp       224
  32    block_ppp      256
