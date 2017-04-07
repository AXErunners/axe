=====================
Axe Core
==========

This is the official reference wallet for AXE digital currency and comprises the backbone of the AXE peer-to-peer network. You can [download AXE Core](https://axerunners.com/) or [build it yourself](#building) using the guides below.

Running
---------------------
The following are some helpful notes on how to run Axe on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/axe-qt` (GUI) or
- `bin/axed` (headless)

### Windows

Unpack the files into a directory, and then run axe-qt.exe.

### OS X

Drag Axe-Qt to your applications folder, and then run Axe-Qt.

### Need Help?

* See the [Axe documentation](https://github.com/AXErunners/axe/wiki)
for help and more information.
* Ask for help on [Discord](https://discordapp.com/invite/RKE5PD9) on Support channel.

Building
---------------------
The following are developer notes on how to build Axe Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OS X Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The Axe Core repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- Source Code Documentation ***TODO***
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss on [Reddit](https://www.reddit.com/r/AXErunners/)
* Discuss on [Discord](https://discordapp.com/invite/BqhteaU)
* Discuss on [Slack](https://axe-slack.herokuapp.com/)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
