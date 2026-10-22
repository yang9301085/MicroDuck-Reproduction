"""仅使用 PTY 和临时普通文件，不枚举或打开实体串口。"""
import os
import pty
import select
import subprocess
import sys
import tempfile
import termios
import time
import unittest

BINARY = os.path.abspath(sys.argv.pop(1))

class SerialTests(unittest.TestCase):
    def start(self, tx=None, timeout=400):
        master, slave = pty.openpty()
        self.addCleanup(os.close, slave)
        self.addCleanup(lambda: os.close(master) if master not in self.closed else None)
        args = [BINARY, os.ttyname(slave), '115200', str(timeout), '64']
        if tx is not None:
            args.append(tx)
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: proc.kill() if proc.poll() is None else None)
        self.addCleanup(proc.stdout.close)
        self.addCleanup(proc.stderr.close)
        self.assertTrue(select.select([proc.stdout], [], [], 2)[0], 'missing READY')
        self.assertEqual(proc.stdout.readline(), b'READY\n')
        return master, slave, proc

    def setUp(self):
        self.closed = set()

    def test_binary_roundtrip_and_configuration(self):
        master, slave, proc = self.start('00 FF 11 13 0A 0D 80')
        attrs = termios.tcgetattr(slave)
        self.assertFalse(attrs[3] & (termios.ICANON | termios.ECHO | termios.ISIG))
        self.assertFalse(attrs[0] & (termios.IXON | termios.IXOFF | termios.ICRNL))
        self.assertEqual(attrs[4:6], [termios.B115200, termios.B115200])
        self.assertTrue(select.select([master], [], [], 2)[0])
        self.assertEqual(os.read(master, 64), bytes.fromhex('00 FF 11 13 0A 0D 80'))
        os.write(master, bytes.fromhex('80 00 FF 13 0D 0A'))
        out, err = proc.communicate(timeout=2)
        self.assertEqual(proc.returncode, 0, err)
        self.assertIn(b'TX: 00 FF 11 13 0A 0D 80', out)
        self.assertIn(b'RX: 80 00 FF 13 0D 0A', out)

    def test_receive_only_sends_nothing(self):
        master, _, proc = self.start()
        self.assertFalse(select.select([master], [], [], 0.05)[0])
        os.write(master, b'\x00\xff')
        out, err = proc.communicate(timeout=2)
        self.assertEqual(proc.returncode, 0, err)
        self.assertNotIn(b'TX:', out)
        self.assertIn(b'RX: 00 FF', out)

    def test_timeout(self):
        begin = time.monotonic()
        _, _, proc = self.start(timeout=150)
        out, err = proc.communicate(timeout=2)
        elapsed = time.monotonic() - begin
        self.assertEqual(proc.returncode, 2, err)
        self.assertIn(b'RX timeout', out)
        self.assertGreaterEqual(elapsed, 0.12)
        self.assertLess(elapsed, 1.5)

    def test_disconnect(self):
        master, _, proc = self.start()
        os.close(master)
        self.closed.add(master)
        _, err = proc.communicate(timeout=2)
        self.assertEqual(proc.returncode, 1)
        self.assertIn(b'receive:', err)

    def test_missing_device(self):
        with tempfile.TemporaryDirectory() as folder:
            p = subprocess.run([BINARY, folder+'/missing', '115200', '100', '64'], capture_output=True)
        self.assertEqual(p.returncode, 1)
        self.assertIn(b'open/configure:', p.stderr)

    def test_not_a_tty(self):
        with tempfile.NamedTemporaryFile() as f:
            p = subprocess.run([BINARY, f.name, '115200', '100', '64'], capture_output=True)
        self.assertEqual(p.returncode, 1)
        self.assertIn(b'open/configure:', p.stderr)

    def test_invalid_arguments_before_open(self):
        for baud, timeout, size, tx in [('12345','100','64','00'), ('115200','0','64','00'),
                                        ('115200','100','0','00'), ('115200','100','64','GG'),
                                        ('115200','100','64','0'), ('115200','100','64','')]:
            with self.subTest(baud=baud, timeout=timeout, size=size, tx=tx):
                p = subprocess.run([BINARY, '/not-opened', baud, timeout, size, tx], capture_output=True)
                self.assertEqual(p.returncode, 1)
                self.assertIn(b'arguments:', p.stderr)
                self.assertNotIn(b'open/configure:', p.stderr)

    def test_no_arguments(self):
        p = subprocess.run([BINARY], capture_output=True)
        self.assertEqual(p.returncode, 1)
        self.assertIn(b'Usage:', p.stderr)

if __name__ == '__main__':
    unittest.main(verbosity=2)
