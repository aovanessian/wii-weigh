# wii-weigh

Small C program to use a **Wii Balance Board** as a scale on Linux.

---

## Prerequisites

- Wii Balance Board
- Bluetooth adapter on your Linux machine

---

## Pairing (one-time setup)

1. Open a terminal.
2. Run `bluetoothctl` (use `sudo bluetoothctl` if needed).
3. Enable Bluetooth and agent:
`power on`
`agent on`
`scan on`
4. Press the **red sync button** inside the battery compartment on the Wii Balance Board.
5. Wait for the board to be discovered and note its MAC address.
6. Pair and connect:
`pair <MAC>`
`connect <MAC>`
`trust <MAC>`

The board should now automatically connect whenever you press its front button.

---

## Build

```sh
make        # real board version (wii-weigh)
make sim    # simulator version (sim-weigh)
```

## Usage
./wii-weigh [-a adjust] [-w]

Options

-a <adjust> : Adjust final weight (to account for clothing, weigh luggage, etc.)

-w : Just print the weight, no prompts (to be used for scripting)


## Example
```
$ ./wii-weigh -a 3.3
Waiting for balance board...
Balance board found, please step on.
Done, weight: 75.83kg.

$ ./wii-weigh -w
72.53
```
