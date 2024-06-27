```
rake compare_speed
```

```
measure the distance (32*32*2000 times):
                                       user     system      total        real
hamming                            0.949597   0.000000   0.949597 (  0.949625)
distance                           1.398479   0.000000   1.398479 (  1.398506)
distance3_bdigit                   0.198673   0.000000   0.198673 (  0.198672)
distance3_pack_bit_logic           0.868502   0.000000   0.868502 (  0.868512)
distance3_pack_algo                0.373779   0.000000   0.373779 (  0.373777)
distance3_popcount_c               0.961717   0.000000   0.961717 (  0.961730)
distance3_popcount_twiddle         0.859967   0.000000   0.859967 (  0.859983)
distance3_ruby                     1.824285   0.000000   1.824285 (  1.824315)
distance 4                         4.441611   0.000000   4.441611 (  4.441687)
```
