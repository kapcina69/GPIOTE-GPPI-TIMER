# BLE Stress Test

Python stress test suite za testiranje stabilnosti BLE komunikacije i parametara sistema.

## Instalacija

```bash
pip install bleak
```

## Pokretanje

```bash
python stress_test.py
```

## Testovi

### 1. **Frequency Range Test**
- Testira validne frekvencije: 1, 5, 10, 25, 50, 75, 100 Hz
- Validira odgovore sistema

### 2. **Pulse Width Range Test**
- Testira validne širine pulsa: 100µs do 10000µs
- Proverava auto-adjustment frekvencije

### 3. **Rapid Frequency Changes**
- 20 random frekvencija u 50ms intervalima
- Testira brzinu odziva sistema

### 4. **Rapid Pulse Width Changes**
- 20 random širina pulsa u 50ms intervalima
- Testira stabilnost pri brzim promenama

### 5. **Alternating Parameters**
- 30 naizmeničnih promena frekvencije i pulse width
- Simulira realnu upotrebu

### 6. **Edge Cases**
- Min/max vrednosti
- Invalide vrednosti (0, >100)
- Testira error handling

### 7. **Impossible Combinations**
- Fizički nemoguće kombinacije (npr. 100Hz + 10000µs pulse)
- Testira validation logiku
- Proverava auto-adjustment

### 8. **Pause/Resume**
- Pauzira sistem
- Menja parametre tokom pauze
- Nastavlja rad

### 9. **Sustained Load**
- 60 sekundi kontinualnih random komandi
- Testira long-term stabilnost
- Proverava memory leak-ove

## Očekivani Rezultati

```
BLE STRESS TEST - nRF52833 Pulse Generation System
============================================================
✓ Found device: Nordic_Timer (XX:XX:XX:XX:XX:XX)
✓ Connected
✓ Notifications enabled

TEST 1: Frequency Range (1-100 Hz)
============================================================
[1] → SF;1
  ← Frequency set to 1 Hz (pause will be: 991 ms)
[2] → SF;5
  ← Frequency set to 5 Hz (pause will be: 191 ms)
...

TEST SUMMARY
============================================================
Total commands sent: 200+
Errors encountered: 0
Responses received: 200+
Success rate: 100.0%

✓ Stress test completed!
```

## Analiza Problema

Ako se test ne prolazi:
1. **Connection timeout** - Proveri da li je device powered i u advertise modu
2. **Command errors** - Proveri BLE UART Service UUID
3. **No responses** - Proveri da li su notifikacije enable-ovane
4. **Validation errors** - Očekivano za TEST 6 i TEST 7 (edge cases)

## Modifikacije

Za custom testove, izmeni:

```python
TEST_DELAY = 0.1  # Delay između komandi
DEVICE_NAME = "Nordic_Timer"  # Ime uređaja

# Dodaj custom test:
async def test_custom(self):
    # Tvoj kod
    pass
```

## Performanse

- **Commands/sec**: ~10-20 (sa 50-100ms delay)
- **BLE latency**: ~50ms od komande do hardverskog update-a
- **Error rate**: <0.1% na stabilnoj BLE vezi

## Napomene

- Test zahteva aktivan BLE connection
- Ne prekidati test tokom sustained load faze
- Log fajl se može dodati za detaljnu analizu
