/* Added after the Pico SDK link script. The Pico SDK defines
 * __flash_binary_end after all loadable flash sections. */
ASSERT(__flash_binary_end <= 0x101fe000,
       "application image overlaps Feature 005 persistent region")
