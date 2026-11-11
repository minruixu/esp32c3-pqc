"""Print the MD5 of the whole SPI flash, computed on the chip (read-only)."""
import sys
from esptool.cmds import detect_chip

port, size = sys.argv[1], int(sys.argv[2], 0)
esp = detect_chip(port=port, baud=115200, connect_mode="default_reset")
print("chip:", esp.get_chip_description())
esp = esp.run_stub()
esp.flash_set_parameters(size)
print("flash_md5:", esp.flash_md5sum(0, size))
esp.hard_reset()
esp._port.close()
