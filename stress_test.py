#!/usr/bin/env python3
"""
BLE Stress Test for nRF52833 Pulse Generation System
Tests rapid parameter changes, edge cases, and system stability
"""

import asyncio
import time
from bleak import BleakClient, BleakScanner
import random

# Nordic UART Service UUIDs
NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # Write to device
NUS_RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # Notifications from device

# Test parameters
DEVICE_NAME = "Nordic_Timer"
TEST_DELAY = 0.1  # Delay between commands (seconds)

class StressTest:
    def __init__(self):
        self.client = None
        self.responses = []
        self.command_count = 0
        self.error_count = 0
        
    def notification_handler(self, sender, data):
        """Handle notifications from device"""
        message = data.decode('utf-8', errors='ignore')
        self.responses.append(message)
        print(f"  ← {message}")
        
    async def send_command(self, command: str, wait_response=True):
        """Send BLE command and optionally wait for response"""
        self.command_count += 1
        print(f"\n[{self.command_count}] → {command}")
        
        try:
            await self.client.write_gatt_char(NUS_TX_CHAR_UUID, command.encode('utf-8'))
            if wait_response:
                await asyncio.sleep(TEST_DELAY)
        except Exception as e:
            print(f"  ✗ Error: {e}")
            self.error_count += 1
            
    async def test_frequency_range(self):
        """Test entire frequency range"""
        print("\n" + "="*60)
        print("TEST 1: Frequency Range (1-100 Hz)")
        print("="*60)
        
        frequencies = [1, 5, 10, 25, 50, 75, 100]
        for freq in frequencies:
            await self.send_command(f"SF;{freq}")
            
    async def test_pulse_width_range(self):
        """Test pulse width range"""
        print("\n" + "="*60)
        print("TEST 2: Pulse Width Range (100-10000 µs)")
        print("="*60)
        
        pulse_widths = [1, 5, 10, 20, 50, 100]  # Units of 100µs
        for pw in pulse_widths:
            await self.send_command(f"SW;{pw}")
            
    async def test_rapid_frequency_changes(self):
        """Rapidly change frequency"""
        print("\n" + "="*60)
        print("TEST 3: Rapid Frequency Changes (20 iterations)")
        print("="*60)
        
        for i in range(20):
            freq = random.randint(1, 100)
            await self.send_command(f"SF;{freq}", wait_response=False)
            await asyncio.sleep(0.05)  # 50ms between commands
            
    async def test_rapid_pulse_width_changes(self):
        """Rapidly change pulse width"""
        print("\n" + "="*60)
        print("TEST 4: Rapid Pulse Width Changes (20 iterations)")
        print("="*60)
        
        for i in range(20):
            pw = random.randint(1, 100)
            await self.send_command(f"SW;{pw}", wait_response=False)
            await asyncio.sleep(0.05)
            
    async def test_alternating_parameters(self):
        """Alternate between frequency and pulse width changes"""
        print("\n" + "="*60)
        print("TEST 5: Alternating Parameter Changes (30 iterations)")
        print("="*60)
        
        for i in range(30):
            if i % 2 == 0:
                freq = random.randint(10, 80)
                await self.send_command(f"SF;{freq}", wait_response=False)
            else:
                pw = random.randint(1, 50)
                await self.send_command(f"SW;{pw}", wait_response=False)
            await asyncio.sleep(0.05)
            
    async def test_edge_cases(self):
        """Test edge cases and limits"""
        print("\n" + "="*60)
        print("TEST 6: Edge Cases (min, max, invalid)")
        print("="*60)
        
        edge_cases = [
            ("SF;1", "Min frequency"),
            ("SF;100", "Max frequency"),
            ("SF;150", "Invalid frequency (too high)"),
            ("SF;0", "Invalid frequency (zero)"),
            ("SW;1", "Min pulse width"),
            ("SW;100", "Max pulse width"),
            ("SW;150", "Invalid pulse width (too high)"),
            ("SW;0", "Invalid pulse width (zero)"),
        ]
        
        for cmd, desc in edge_cases:
            print(f"\n  Test: {desc}")
            await self.send_command(cmd)
            
    async def test_impossible_combinations(self):
        """Test physically impossible frequency/pulse width combinations"""
        print("\n" + "="*60)
        print("TEST 7: Impossible Combinations")
        print("="*60)
        
        # Set high pulse width, then try high frequency
        print("\n  Test: High pulse width (5000µs) + High frequency (100Hz)")
        await self.send_command("SW;50")  # 5000µs
        await asyncio.sleep(0.2)
        await self.send_command("SF;100")  # Should fail
        
        print("\n  Test: Very high pulse width (10000µs) + Medium frequency (50Hz)")
        await self.send_command("SW;100")  # 10000µs
        await asyncio.sleep(0.2)
        await self.send_command("SF;50")  # Should fail
        
        # Test auto-adjustment
        print("\n  Test: Auto-adjustment when reducing pulse width")
        await self.send_command("SF;10")  # Low frequency first
        await asyncio.sleep(0.2)
        await self.send_command("SW;50")  # High pulse width
        await asyncio.sleep(0.2)
        await self.send_command("SF;50")  # Try higher frequency - should auto-adjust
        
    async def test_pause_resume(self):
        """Test pause/resume functionality"""
        print("\n" + "="*60)
        print("TEST 8: Pause/Resume Control")
        print("="*60)
        
        await self.send_command("SP;1")  # Pause
        await asyncio.sleep(0.5)
        await self.send_command("SF;25")  # Change params while paused
        await asyncio.sleep(0.2)
        await self.send_command("SW;10")
        await asyncio.sleep(0.2)
        await self.send_command("SP;0")  # Resume
        
    async def test_sustained_load(self):
        """Sustained random commands for extended period"""
        print("\n" + "="*60)
        print("TEST 9: Sustained Load (60 seconds)")
        print("="*60)
        
        start_time = time.time()
        iteration = 0
        
        while time.time() - start_time < 60:
            iteration += 1
            
            # Random command selection
            cmd_type = random.choice(['freq', 'width', 'pause'])
            
            if cmd_type == 'freq':
                freq = random.randint(1, 100)
                await self.send_command(f"SF;{freq}", wait_response=False)
            elif cmd_type == 'width':
                pw = random.randint(1, 100)
                await self.send_command(f"SW;{pw}", wait_response=False)
            else:
                pause = random.choice([0, 1])
                await self.send_command(f"SP;{pause}", wait_response=False)
                
            await asyncio.sleep(random.uniform(0.05, 0.2))
            
            if iteration % 50 == 0:
                elapsed = time.time() - start_time
                print(f"\n  Progress: {elapsed:.1f}s / 60s ({iteration} commands)")
                
        print(f"\n  Completed: {iteration} commands in 60 seconds")
        
    async def run_all_tests(self):
        """Run all stress tests"""
        print("\n" + "="*60)
        print("BLE STRESS TEST - nRF52833 Pulse Generation System")
        print("="*60)
        
        # Find device
        print("\nScanning for device...")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
        
        if not device:
            print(f"✗ Device '{DEVICE_NAME}' not found!")
            return
            
        print(f"✓ Found device: {device.name} ({device.address})")
        
        # Connect
        print("\nConnecting...")
        async with BleakClient(device) as client:
            self.client = client
            print("✓ Connected")
            
            # Subscribe to notifications
            await client.start_notify(NUS_RX_CHAR_UUID, self.notification_handler)
            print("✓ Notifications enabled")
            
            # Run tests
            await self.test_frequency_range()
            await asyncio.sleep(1)
            
            await self.test_pulse_width_range()
            await asyncio.sleep(1)
            
            await self.test_edge_cases()
            await asyncio.sleep(1)
            
            await self.test_impossible_combinations()
            await asyncio.sleep(1)
            
            await self.test_rapid_frequency_changes()
            await asyncio.sleep(1)
            
            await self.test_rapid_pulse_width_changes()
            await asyncio.sleep(1)
            
            await self.test_alternating_parameters()
            await asyncio.sleep(1)
            
            await self.test_pause_resume()
            await asyncio.sleep(1)
            
            await self.test_sustained_load()
            
            # Summary
            print("\n" + "="*60)
            print("TEST SUMMARY")
            print("="*60)
            print(f"Total commands sent: {self.command_count}")
            print(f"Errors encountered: {self.error_count}")
            print(f"Responses received: {len(self.responses)}")
            print(f"Success rate: {((self.command_count - self.error_count) / self.command_count * 100):.1f}%")
            
            # Stop notifications
            await client.stop_notify(NUS_RX_CHAR_UUID)
            
        print("\n✓ Stress test completed!")

async def main():
    test = StressTest()
    try:
        await test.run_all_tests()
    except KeyboardInterrupt:
        print("\n\n✗ Test interrupted by user")
    except Exception as e:
        print(f"\n\n✗ Test failed with error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    asyncio.run(main())
