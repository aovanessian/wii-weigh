Small C program to use a Wii Balance Board as a scale connected to your Linux machine.

Prerequisites:
- Wii Balance Board (obviously)
- Bluetooth adapter on your Linux machine.

Pairing (One-Time):
- Open terminal
- Run `sudo bluetoothctl`
- Run `power on`
- Run `agent on`
- Run `scan on`
- Press the red sync button (inside the battery compartment of the balance board)
- Wait for the balance board to be discovered and note its MAC address
- `pair <MAC>`
- `connect <MAC>`
- `trust <MAC>` (this will allow future connections without re-pairing).

Run wii-weigh
