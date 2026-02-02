#!/usr/bin/env python3
"""
UART stress test for NRFX non-blocking sample.
Sends SF;/SW;/SA/SON/SOFF commands and validates responses. 
Focus on SON/SOFF (start/stop) commands with mixed parameter commands.
Includes an "intense" mode which increases iterations and can send bursts 
and malformed commands to stress the firmware.

Usage:
  python stress_test_uart.py --port COM3 --baud 115200 --iters 100 --delay 0.2
  python stress_test_uart.py --port COM3 --focus-startstop   # Focus on SON/SOFF
  python stress_test_uart.py --port COM3 --rapid-toggle      # Rapid SON/SOFF toggling

Requires: pyserial (`pip install pyserial`)
"""

import argparse
import random
import sys
import time

try:
    import serial
except Exception:
    print("pyserial is required: pip install pyserial")
    raise

PROMPT = '>'
READ_TIMEOUT = 1.0

# Command templates
CMD_SET_FREQ = 'SF;{freq}'
CMD_SET_WIDTH = 'SW;{width}'
CMD_SET_AMP = 'SA;{amp}'
CMD_START = 'SON'
CMD_STOP = 'SOFF'

MIN_FREQ = 1
MAX_FREQ = 59
MIN_WIDTH = 1
MAX_WIDTH = 10
MIN_AMP = 1
MAX_AMP = 30

# Weight for command selection (higher = more frequent)
CMD_WEIGHTS = {
    'SF': 1,
    'SW': 1,
    'SA': 1,
    'SON': 3,   # Higher weight for start/stop focus
    'SOFF': 3,
}


def read_until_prompt(ser, timeout=READ_TIMEOUT):
    end = time.time() + timeout
    resp = []
    while time.time() < end:
        data = ser.read(ser.in_waiting or 1)
        if not data:
            time.sleep(0.002)
            continue
        try:
            s = data.decode('utf-8', errors='ignore')
        except Exception:
            s = str(data)
        resp.append(s)
        if PROMPT in s:
            break
    return ''.join(resp)


def send_command(ser, cmd, wait=0.05):
    # allow sending empty/malformed commands
    ser.write((cmd + '\r').encode('utf-8'))
    ser.flush()
    if wait:
        time.sleep(wait)
    resp = read_until_prompt(ser, timeout=1.0)
    return resp


def test_sequence(ser, iters=100, delay=0.2, verbose=False, burst=1, malformed_pct=0.0, flood=False, 
                   focus_startstop=False, rapid_toggle=False):
    """
    Run test sequence with various command types.
    
    Args:
        focus_startstop: If True, increase weight of SON/SOFF commands
        rapid_toggle: If True, send rapid SON/SOFF alternations
    """
    stats = {'sent': 0, 'ok': 0, 'error': 0, 'responses': [], 
             'start_ok': 0, 'stop_ok': 0, 'start_err': 0, 'stop_err': 0}

    malformed_examples = [
        'XXX', 'SF;;', 'SW;abc', 'SA;-1', 'SF;999999', '', 'SW', 'SA;',
        'SONN', 'SOOF', 'SON;1', 'SOFF;0', 'son', 'soff'  # malformed start/stop
    ]

    # Build weighted command list
    if focus_startstop:
        cmd_pool = ['SON'] * 5 + ['SOFF'] * 5 + ['SF', 'SW', 'SA']
    elif rapid_toggle:
        cmd_pool = ['SON', 'SOFF']  # Only toggle commands
    else:
        cmd_pool = []
        for cmd, weight in CMD_WEIGHTS.items():
            cmd_pool.extend([cmd] * weight)

    system_running = True  # Track expected system state

    for i in range(iters):
        # For rapid toggle mode, alternate SON/SOFF
        if rapid_toggle:
            typ = 'SOFF' if system_running else 'SON'
        else:
            typ = random.choice(cmd_pool)

        # Build base command
        if typ == 'SF':
            freq = random.randint(MIN_FREQ, MAX_FREQ)
            base_cmd = CMD_SET_FREQ.format(freq=freq)
        elif typ == 'SW':
            width = random.randint(MIN_WIDTH, MAX_WIDTH)
            base_cmd = CMD_SET_WIDTH.format(width=width)
        elif typ == 'SA':
            amp = random.randint(MIN_AMP, MAX_AMP)
            base_cmd = CMD_SET_AMP.format(amp=amp)
        elif typ == 'SON':
            base_cmd = CMD_START
        elif typ == 'SOFF':
            base_cmd = CMD_STOP
        else:
            base_cmd = typ

        # determine if this iteration will use a malformed command
        is_malformed = (random.random() < malformed_pct)

        for b in range(burst):
            if is_malformed:
                cmd = random.choice(malformed_examples)
            else:
                cmd = base_cmd

            stats['sent'] += 1
            if verbose:
                print(f"[{i+1}/{iters}] (burst {b+1}/{burst}) -> {cmd}")

            # flood mode: minimize wait between commands
            if flood:
                wait = 0
            else:
                # no wait for intermediate burst commands, wait only after last
                wait = 0 if b < burst - 1 else delay

            resp = send_command(ser, cmd, wait=wait)
            stats['responses'].append((cmd, resp))

            if verbose:
                print(f"  <- {resp!r}\n")

            # Analyze response
            resp_upper = resp.upper()
            is_ok = 'OK' in resp_upper or 'STARTED' in resp_upper or 'STOPPED' in resp_upper
            is_already = 'ALREADY' in resp_upper
            is_error = 'ERROR' in resp_upper or 'UNKNOWN' in resp_upper.lower()

            # Track start/stop specific stats
            if cmd == 'SON':
                if is_ok or is_already:
                    stats['start_ok'] += 1
                    stats['ok'] += 1
                    system_running = True
                else:
                    stats['start_err'] += 1
                    stats['error'] += 1
            elif cmd == 'SOFF':
                if is_ok or is_already:
                    stats['stop_ok'] += 1
                    stats['ok'] += 1
                    system_running = False
                else:
                    stats['stop_err'] += 1
                    stats['error'] += 1
            elif is_ok:
                stats['ok'] += 1
            elif is_error:
                stats['error'] += 1
            else:
                if resp.strip() == '':
                    stats['error'] += 1
                else:
                    stats['ok'] += 1

    return stats


def test_startstop_sequences(ser, delay=0.2, verbose=False):
    """
    Dedicated test for SON/SOFF command sequences.
    Tests various scenarios to validate start/stop behavior.
    """
    print("\n=== START/STOP SEQUENCE TESTS ===")
    tests_passed = 0
    tests_total = 0
    
    def run_test(name, commands, expected_responses):
        nonlocal tests_passed, tests_total
        tests_total += 1
        print(f"\nTest: {name}")
        results = []
        for cmd in commands:
            resp = send_command(ser, cmd, wait=delay)
            results.append((cmd, resp))
            if verbose:
                print(f"  {cmd} -> {resp.strip()}")
        
        # Check expected patterns in responses
        all_ok = True
        for (cmd, resp), expected in zip(results, expected_responses):
            if expected not in resp.upper():
                all_ok = False
                print(f"  FAIL: Expected '{expected}' in response to '{cmd}'")
                print(f"        Got: {resp.strip()}")
        
        if all_ok:
            print(f"  PASS")
            tests_passed += 1
        return all_ok
    
    # Test 1: Basic stop
    run_test("Basic SOFF", ['SOFF'], ['STOPPED', 'ALREADY'])
    
    # Test 2: Double stop (should say already stopped)
    run_test("Double SOFF", ['SOFF', 'SOFF'], ['OK', 'ALREADY'])
    
    # Test 3: Start after stop
    run_test("SON after SOFF", ['SOFF', 'SON'], ['OK', 'STARTED'])
    
    # Test 4: Double start (should say already running)
    run_test("Double SON", ['SON', 'SON'], ['OK', 'ALREADY'])
    
    # Test 5: Toggle sequence
    run_test("Toggle sequence", ['SOFF', 'SON', 'SOFF', 'SON'], 
             ['OK', 'OK', 'OK', 'OK'])
    
    # Test 6: Rapid toggle
    print(f"\nTest: Rapid toggle (10x)")
    tests_total += 1
    rapid_ok = True
    for i in range(10):
        resp_off = send_command(ser, 'SOFF', wait=0.05)
        resp_on = send_command(ser, 'SON', wait=0.05)
        if 'ERROR' in resp_off.upper() or 'ERROR' in resp_on.upper():
            rapid_ok = False
            print(f"  FAIL at iteration {i}")
            break
    if rapid_ok:
        print(f"  PASS")
        tests_passed += 1
    
    # Test 7: Parameter change while stopped
    print(f"\nTest: Parameter change while stopped")
    tests_total += 1
    send_command(ser, 'SOFF', wait=delay)
    resp_sf = send_command(ser, 'SF;5', wait=delay)
    resp_sw = send_command(ser, 'SW;3', wait=delay)
    resp_sa = send_command(ser, 'SA;10', wait=delay)
    send_command(ser, 'SON', wait=delay)
    
    if all('OK' in r.upper() for r in [resp_sf, resp_sw, resp_sa]):
        print(f"  PASS")
        tests_passed += 1
    else:
        print(f"  FAIL: Parameter commands while stopped should work")
    
    # Test 8: Burst start/stop
    print(f"\nTest: Burst commands (no delay)")
    tests_total += 1
    burst_ok = True
    for _ in range(5):
        send_command(ser, 'SOFF', wait=0)
        send_command(ser, 'SON', wait=0)
    time.sleep(0.5)  # Let firmware catch up
    resp = send_command(ser, 'SOFF', wait=delay)
    if 'OK' in resp.upper() or 'ALREADY' in resp.upper():
        print(f"  PASS")
        tests_passed += 1
    else:
        print(f"  FAIL")
        burst_ok = False
    
    # Ensure system is running at end
    send_command(ser, 'SON', wait=delay)
    
    print(f"\n=== SEQUENCE TESTS: {tests_passed}/{tests_total} passed ===")
    return tests_passed, tests_total


def main():
    parser = argparse.ArgumentParser(description='UART command stress test with SON/SOFF focus')
    parser.add_argument('--port', required=True, help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--iters', type=int, default=200, help='Number of command iterations')
    parser.add_argument('--delay', type=float, default=0.2, help='Delay between command and read (s)')
    parser.add_argument('--burst', type=int, default=1, help='Send N commands back-to-back per iteration')
    parser.add_argument('--malformed-pct', type=float, default=0.0, help='Fraction (0..1) of iterations to send malformed commands')
    parser.add_argument('--flood', action='store_true', help='Minimize waits between burst commands')
    parser.add_argument('--intense', action='store_true', help='Enable intense defaults (high iters, bursts, malformed)')
    parser.add_argument('--slow', action='store_true', help='Multiply delay by 5 for slower testing')
    parser.add_argument('--verbose', action='store_true', help='Show full responses')
    parser.add_argument('--reset-delay', type=float, default=1.0, help='Delay after opening port before starting')
    
    # New SON/SOFF focused options
    parser.add_argument('--focus-startstop', action='store_true', 
                        help='Focus testing on SON/SOFF commands (higher weight)')
    parser.add_argument('--rapid-toggle', action='store_true',
                        help='Rapid SON/SOFF toggling test only')
    parser.add_argument('--sequence-tests', action='store_true',
                        help='Run dedicated SON/SOFF sequence tests')
    parser.add_argument('--full-test', action='store_true',
                        help='Run all tests: sequence tests + stress test with focus on SON/SOFF')

    args = parser.parse_args()

    # apply slow/intense presets
    if args.slow:
        args.delay *= 5
        print(f"Slow mode enabled: effective delay={args.delay}s")

    if args.intense:
        args.iters = max(args.iters, 5000)
        args.delay = min(args.delay, 0.01)
        args.burst = max(args.burst, 8)
        args.malformed_pct = max(args.malformed_pct, 0.12)
        args.flood = True
        args.focus_startstop = True  # Also focus on start/stop in intense mode
        print(f"Intense mode: iters={args.iters}, delay={args.delay}, burst={args.burst}, malformed_pct={args.malformed_pct}")

    # full-test implies both sequence tests and focus on start/stop
    if args.full_test:
        args.sequence_tests = True
        args.focus_startstop = True

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0)
    except Exception as e:
        print('Failed to open serial port:', e)
        sys.exit(2)

    print(f'Opened {args.port} @ {args.baud}, waiting {args.reset_delay}s')
    time.sleep(args.reset_delay)

    banner = read_until_prompt(ser, timeout=1.0)
    if args.verbose:
        print('Banner:', banner)

    # Run sequence tests if requested
    if args.sequence_tests:
        seq_passed, seq_total = test_startstop_sequences(ser, delay=args.delay, verbose=args.verbose)
        print()
    
    # Run main stress test (unless only sequence tests requested)
    if not args.sequence_tests or args.full_test:
        print("=== STRESS TEST ===")
        if args.focus_startstop:
            print("Mode: Focus on SON/SOFF commands")
        elif args.rapid_toggle:
            print("Mode: Rapid SON/SOFF toggle only")
        else:
            print("Mode: Mixed commands (SF/SW/SA/SON/SOFF)")
        
        stats = test_sequence(ser, iters=args.iters, delay=args.delay, verbose=args.verbose,
                              burst=args.burst, malformed_pct=args.malformed_pct, flood=args.flood,
                              focus_startstop=args.focus_startstop, rapid_toggle=args.rapid_toggle)

        print('\n=== STRESS TEST RESULTS ===')
        print(f"Total sent: {stats['sent']}")
        print(f"OK approx.: {stats['ok']}")
        print(f"Errors: {stats['error']}")
        print(f"\nSON/SOFF specific:")
        print(f"  SON OK: {stats['start_ok']}, Errors: {stats['start_err']}")
        print(f"  SOFF OK: {stats['stop_ok']}, Errors: {stats['stop_err']}")
        print('\nSample responses (first 20):')
        for cmd, resp in stats['responses'][:20]:
            print('->', cmd)
            print('<-', resp.replace('\r', '\\r').replace('\n', '\\n'))

    # Ensure system is running at end
    print("\nEnsuring system is running...")
    send_command(ser, 'SON', wait=args.delay)
    
    ser.close()
    print("\nDone.")


if __name__ == '__main__':
    main()
