"""Read the USB serial console without resetting the chip, until PQC_DONE."""
import sys, time
import serial

port, out_path, limit = sys.argv[1], sys.argv[2], float(sys.argv[3])
ser = serial.serial_for_url(port, 115200, do_not_open=True)
ser.timeout = 0.2
ser.rts = True   # same line states the OS asserts on open, so nothing toggles
ser.dtr = True
ser.open()
ser.rts = False  # release RTS first to avoid a reset pulse
ser.dtr = False
buf = b""
t0 = time.time()
done_at = None
while time.time() - t0 < limit:
    chunk = ser.read(4096)
    if chunk:
        buf += chunk
        if done_at is None and b"PQC_DONE" in buf:
            done_at = time.time()
    if done_at and time.time() - done_at > 1.0:
        break
ser.close()
open(out_path, "wb").write(buf)
sys.stdout.write(buf.decode("utf-8", "replace"))
print("\n[capture: %d bytes in %.1f s, done=%s]" % (len(buf), time.time() - t0, bool(done_at)))
