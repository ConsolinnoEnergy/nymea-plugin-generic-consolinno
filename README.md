# nymea-plugin-generic-consolinno

Plugin for Consolinno devices

## Build

Download and install cross compiler for armhf: (see also https://gitlab.consolinno-it.de/leafletfirmware/nymea/dockcross-nymea)

```bash
docker login registry.consolinno-it.de # Login with your GitLab credentials
mkdir -p ~/.local/bin
docker run --rm --pull always registry.consolinno-it.de/leafletfirmware/nymea/dockcross-nymea:dc-armv7-nymea-d10-latest > ~/.local/bin/dc-armv7-nymea-d10
chmod +x ~/.local/bin/dc-armv7-nymea-d10
```

Build:

```bash
git clone git@gitlab.consolinno-it.de:leafletfirmware/nymea/nymea-plugin-generic-consolinno.git
cd nymea-plugin-generic-consolinno
sudo ~/.local/bin/dc-armv7-nymea-d10 make clean
sudo ~/.local/bin/dc-armv7-nymea-d10 make distclean
sudo ~/.local/bin/dc-armv7-nymea-d10 bash -c "sudo apt update && sudo apt install libjsonrpccpp-dev:armhf libcurl4-openssl-dev:armhf -y && qmake && make -j6"
```