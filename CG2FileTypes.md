# MSM6xxx #
  * **0x01000000** - CDMA2000 Bootloader (~128 kb)
  * **0x04000000** - Flex Data (files, seems)
  * **0x07000000** - Phone Code
  * **0x08000000** - Qualcomm Mini Bootloader (~7 kb)
  * **0x09000000** - Monster EFS Image
  * **0x10000000** - ua.bin (I think this is to facilitate over-the-air updates!)
  * **0x12000000** - Flex Data (Specific to VZW V9m flash? Intermixed with **0x04000000** data)

# ZN4 #
  * **0x24000000** - Bootloader Header, ARM entry points (ZN4 Upgrade)
  * **0x25000000** - Bootloader in ELF (ZN4 Upgrade)
  * **0x26000000** - Phone Code Header, ARM entry points (ZN4 Upgrade)
  * **0x27000000** - Phone Code in ELF (ZN4 Upgrade)

# Questionable #
  * **0x117440512** - Phone Code (Alltel V9m) (Not 32 bits. What?)
  * **0x67108864** - Flex Data (Alltel V9m)