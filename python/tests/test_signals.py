# Jimmy Paputto 2026
#
# Signals and blocking calls of the Python GnssHat, on a real HAT. Usage:
#   GNSSHAT_HW_TESTS=1 python3 test_signals.py <path to gnsshat .so>
# Exits with 77, which ctest reports as skipped, without GNSSHAT_HW_TESTS.

import os
import signal
import subprocess
import sys
import time
import unittest

SKIPPED = 77

# Every case runs in a process of its own: only one GnssHat may use the
# receiver at a time, and the signals must not reach the test runner.
PRELUDE = """
import importlib.machinery, importlib.util, signal, sys, threading, time
loader = importlib.machinery.ExtensionFileLoader("gnsshat", sys.argv[1])
spec = importlib.util.spec_from_loader("gnsshat", loader)
gnsshat = importlib.util.module_from_spec(spec)
loader.exec_module(gnsshat)
# A process started in the background inherits SIGINT ignored.
signal.signal(signal.SIGINT, signal.default_int_handler)
"""

# Upper bound for a signal to end a wait: one 100 ms slice plus process noise.
SIGNAL_DEADLINE_S = 1.0


def run_case(library, body, sig=None):
    """Runs `body` until it prints 'ready', then sends `sig` and returns
    (seconds from the signal to the exit, return code, stdout)."""
    child = subprocess.Popen(
        [sys.executable, "-c", PRELUDE + body, library],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    try:
        lines = []
        for line in child.stdout:
            lines.append(line)
            if line.strip() == "ready":
                break
        # Let the wait block for a moment.
        time.sleep(0.3)
        start = time.monotonic()
        if sig is not None:
            child.send_signal(sig)
        rest, _ = child.communicate(timeout=10)
        return time.monotonic() - start, child.returncode, "".join(lines) + rest
    except subprocess.TimeoutExpired:
        child.kill()
        child.communicate()
        raise AssertionError("the process did not end after the signal")


class Signals(unittest.TestCase):
    library = None

    def assert_ended(self, result, expected_output):
        seconds, code, output = result
        self.assertEqual(code, 0, output)
        self.assertIn(expected_output, output)
        self.assertLess(seconds, SIGNAL_DEADLINE_S, output)

    def test_ctrl_c_ends_a_wait_without_data(self):
        # Not started, so no epoch ever arrives.
        self.assert_ended(run_case(self.library, """
with gnsshat.GnssHat() as hat:
    print("ready", flush=True)
    try:
        hat.wait_and_get_fresh_navigation()
    except KeyboardInterrupt:
        print("interrupted", flush=True)
""", signal.SIGINT), "interrupted")

    def test_ctrl_c_ends_a_timepulse_loop(self):
        self.assert_ended(run_case(self.library, """
with gnsshat.GnssHat() as hat:
    hat.enable_timepulse()
    print("ready", flush=True)
    try:
        while hat.timepulse():
            pass
    except KeyboardInterrupt:
        print("interrupted", flush=True)
""", signal.SIGINT), "interrupted")

    def test_sigterm_can_end_a_wait_like_ctrl_c(self):
        self.assert_ended(run_case(self.library, """
signal.signal(signal.SIGTERM, signal.default_int_handler)
with gnsshat.GnssHat() as hat:
    print("ready", flush=True)
    try:
        hat.wait_and_get_fresh_navigation()
    except KeyboardInterrupt:
        print("interrupted", flush=True)
""", signal.SIGTERM), "interrupted")

    def test_handler_may_call_shutdown(self):
        self.assert_ended(run_case(self.library, """
with gnsshat.GnssHat() as hat:
    signal.signal(signal.SIGINT, lambda *_: hat.shutdown())
    print("ready", flush=True)
    print("result", hat.wait_and_get_fresh_navigation(), flush=True)
""", signal.SIGINT), "result None")

    def test_handler_may_call_close(self):
        self.assert_ended(run_case(self.library, """
hat = gnsshat.GnssHat()
signal.signal(signal.SIGINT, lambda *_: hat.close())
print("ready", flush=True)
print("result", hat.wait_and_get_fresh_navigation(), hat.closed, flush=True)
""", signal.SIGINT), "result None True")

    def test_close_from_a_thread_ends_a_wait_in_main(self):
        seconds, code, output = run_case(self.library, """
hat = gnsshat.GnssHat()
threading.Timer(0.3, hat.close).start()
print("ready", flush=True)
print("result", hat.wait_and_get_fresh_navigation(), flush=True)
""")
        self.assertEqual(code, 0, output)
        self.assertIn("result None", output)

    def test_ctrl_c_ends_a_gpsd_join(self):
        if os.geteuid() != 0:
            self.skipTest("gpsd forwarding needs root")
        self.assert_ended(run_case(self.library, """
with gnsshat.GnssHat() as hat:
    hat.start_forward_for_gpsd()
    print("ready", flush=True)
    try:
        hat.join_forward_for_gpsd()
    except KeyboardInterrupt:
        print("interrupted", flush=True)
""", signal.SIGINT), "interrupted")

    def test_blocking_calls_release_the_gil(self):
        seconds, code, output = run_case(self.library, """
ticks = 0
def tick():
    global ticks
    while True:
        ticks += 1
        time.sleep(0.01)
threading.Thread(target=tick, daemon=True).start()
with gnsshat.GnssHat() as hat:
    print("ready", flush=True)
    before = ticks
    hat.soft_reset_hot_start()
    print("ticks", ticks - before, flush=True)
""")
        self.assertEqual(code, 0, output)
        ticks = int(output.split("ticks")[1].split()[0])
        # The reset blocks for over a second, so about 100 ticks fit in it.
        self.assertGreater(ticks, 20, output)

    def test_leaving_an_ntrip_with_block_lets_threads_log(self):
        # The handler of a connected client logs while it is being joined.
        seconds, code, output = run_case(self.library, """
import socket
print("ready", flush=True)
with gnsshat.NtripCaster("127.0.0.1", 21199, "X") as caster:
    caster.set_log_level(3)
    caster.set_log_callback(lambda level, message: None)
    caster.start()
    client = socket.create_connection(("127.0.0.1", 21199))
    client.sendall(b"GET /X HTTP/1.1\\r\\nHost: localhost\\r\\n"
                   b"Ntrip-Version: Ntrip/2.0\\r\\n"
                   b"User-Agent: NTRIP test\\r\\n\\r\\n")
    time.sleep(0.1)
client.close()
print("left", flush=True)
""")
        self.assertEqual(code, 0, output)
        self.assertIn("left", output)

    def test_taken_timepulse_line_raises(self):
        seconds, code, output = run_case(self.library, """
print("ready", flush=True)
with gnsshat.GnssHat() as first, gnsshat.GnssHat() as second:
    first.enable_timepulse()
    try:
        second.enable_timepulse()
    except RuntimeError as error:
        print("raised", error, flush=True)
""")
        self.assertEqual(code, 0, output)
        self.assertIn("raised", output)


if __name__ == "__main__":
    if os.environ.get("GNSSHAT_HW_TESTS") is None:
        print("set GNSSHAT_HW_TESTS=1 to run on a real HAT")
        sys.exit(SKIPPED)
    if not os.path.exists("/proc/device-tree/hat/product"):
        print("no HAT detected")
        sys.exit(SKIPPED)
    Signals.library = sys.argv.pop(1)
    unittest.main()
