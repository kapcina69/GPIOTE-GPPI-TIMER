#!/usr/bin/env python3
"""
UART stress test for NRFX non-blocking sample.
Sends SF;/SW;/SA commands and validates responses. Includes an "intense" mode
which increases iterations and can send bursts and malformed commands to stress
the firmware.

Usage:
  python stress_test_uart.py --port COM3 --baud 115200 --iters 100 --delay 0.2

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

CMD_SET_FREQ = 'SF;{freq}'
CMD_SET_WIDTH = 'SW;{width}'
CMD_SET_AMP = 'SA;{amp}'

MIN_FREQ = 1
MAX_FREQ = 59
MIN_WIDTH = 1
MAX_WIDTH = 10
MIN_AMP = 1
MAX_AMP = 30


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


def test_sequence(ser, iters=100, delay=0.2, verbose=False, burst=1, malformed_pct=0.0, flood=False):
    stats = {'sent': 0, 'ok': 0, 'error': 0, 'responses': []}

    malformed_examples = [
        'XXX', 'SF;;', 'SW;abc', 'SA;-1', 'SF;999999', '', 'SW', 'SA;'
    ]

    for i in range(iters):
        typ = random.choice(['SF', 'SW', 'SA'])

        # build a base command
        if typ == 'SF':
            freq = random.randint(MIN_FREQ, MAX_FREQ)
            base_cmd = CMD_SET_FREQ.format(freq=freq)
        elif typ == 'SW':
            width = random.randint(MIN_WIDTH, MAX_WIDTH)
            base_cmd = CMD_SET_WIDTH.format(width=width)
        else:
            amp = random.randint(MIN_AMP, MAX_AMP)
            base_cmd = CMD_SET_AMP.format(amp=amp)

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

            # heuristics
            if 'OK:' in resp or 'OK' in resp or 'Frequency set' in resp:
                stats['ok'] += 1
            elif 'ERROR' in resp or 'unknown' in resp.lower():
                stats['error'] += 1
            else:
                if resp.strip() == '':
                    stats['error'] += 1
                else:
                    stats['ok'] += 1

    return stats


def main():
    parser = argparse.ArgumentParser(description='UART command stress test')
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
        print(f"Intense mode: iters={args.iters}, delay={args.delay}, burst={args.burst}, malformed_pct={args.malformed_pct}")

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

    stats = test_sequence(ser, iters=args.iters, delay=args.delay, verbose=args.verbose,
                          burst=args.burst, malformed_pct=args.malformed_pct, flood=args.flood)

    print('\n=== RESULTS ===')
    print(f"Total sent: {stats['sent']}")
    print(f"OK approx.: {stats['ok']}")
    print(f"Errors: {stats['error']}")
    print('Sample responses (first 20):')
    for cmd, resp in stats['responses'][:20]:
        print('->', cmd)
        print('<-', resp.replace('\r', '\\r').replace('\n', '\\n'))

    ser.close()


if __name__ == '__main__':
    main()
