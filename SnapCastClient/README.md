# SnapcastClient

Build

```bash
mkdir build
cd build
cmake ..
make
```

Run

Put LightningCastDeviceSample and SnapCastClient in the same directory and execute:(hw:0,0 specifies the ALSA output device)

```bash
./SnapCastClient hw:0,0 &
./LightningCastDeviceSample
```
