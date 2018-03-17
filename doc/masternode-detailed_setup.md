# Masternode - Detailed setup
To start AXE masternode on a dedicated server you will need:
* 1000 AXE collateral
* server with Ubuntu (16.04) and static IP ([Vultr](https://www.vultr.com/?ref=7231821))
* Axe Core on local machine
* basic Linux skills

## Setup VPS

Switch to dedicated user AXERUNNER:
```
adduser axerunner
usermod -aG sudo axerunner
su axerunner
```
### Prepare the system:
```
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install git python-virtualenv virtualenv fail2ban ufw
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
sudo apt-get install libboost-all-dev
sudo add-apt-repository ppa:bitcoin/bitcoin
sudo apt-get update
sudo apt-get install libdb4.8-dev libdb4.8++-dev
sudo apt-get install libminiupnpc-dev libzmq3-dev
sudo ufw allow ssh/tcp
sudo ufw limit ssh/tcp
sudo ufw allow 9937/tcp
sudo ufw allow 9936/tcp
sudo ufw allow 9337/tcp
sudo ufw logging on
sudo ufw disable
sudo ufw enable
fallocate -l 4G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
nano /etc/fstab
```
enter the following file at the end: <br />
`/swapfile none swap sw 0 0`

### Install AXE on VPS
```
git clone https://github.com/axerunners/axe
cd axe && cd depends && make
cd .. && ./autogen.sh && ./configure --without-gui && make && sudo make install
axed -daemon
axe-cli stop
```
## Build AXE on local machine
```
sudo apt-get install git build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
sudo apt-get install libminiupnpc-dev
sudo apt-get install libzmq3-dev
sudo apt-get install libboost-all-dev
sudo add-apt-repository ppa:bitcoin/bitcoin
sudo apt-get update
sudo apt-get install libdb4.8-dev libdb4.8++-dev
git clone https://github.com/axerunners/axe
cd axe && cd depends && make
cd .. && ./autogen.sh && ./configure && make && sudo make install
axed -daemon
axe-cli stop
```
You could also download [binaries](https://github.com/AXErunners/axe/releases) instead of building the source.<br />
Open your local Axe wallet (`axe-qt`)and let it sync with the network. After downloading the blockchain you can create masternode address with the key. Open debug console to enter `masternode genkey` and `getaccountaddress mn`. Send 1000 AXE to just generated `mn` address.

### Edit `axe.conf` in your datadir
```
cd ~ && cd .axecore
nano axe.conf
```
Insert following:
```
#- - - -
rpcuser=XXX
rpcpassword=XXX
rpcallowip=127.0.0.1
rpcport=9337
#- - - -
listen=1
server=1
daemon=1
#- - - -
masternode=1
masternodeprivkey=XXX
externalip=XXX.XXX.XXX.XXX
```
* `rpcuser/password` might be random <br />
* `externalip` is your VPS static address<br />
* `masternodeprivkey` is generated with `masternode genkey`

### Finish local wallet setup
Go to your local datadir and edit masternode.conf to make it look like example below
```
mn1 127.0.0.2:9937 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 7603c20a05258c208b58b0a0d77603b9fc93d47cfa403035f87f3ce0af814566 0
mn2 127.0.0.4:9937 92Da1aYg6sbenP6uwskJgEY2XWB5LwJ7bXRqc3UPeShtHWJDjDv 5d898e78244f3206e0105f421cdb071d95d111a51cd88eb5511fc0dbf4bfd95f 1
```
* `mn1` is the alias of your masternode
* `127.0.0.2:9937` is the IP address of your VPS and Axe RPC port `9337`
* `93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg` is your masternode private key
* `5d898e78244f3206e0105f421cdb071d95d111a51cd88eb5511fc0dbf4bfd95f 1` is the mn transaction output from `masternode outputs`

## Install Sentinel on the VPS
```
sudo apt-get update
sudo apt-get install python-virtualenv
sudo apt-get install virtualenv
cd ~ && cd .axecore
git clone https://github.com/AXErunners/sentinel.git
cd sentinel
virtualenv venv
venv/bin/pip install -r requirements.txt
```
Now you need to add Sentinel to cron. Enter `crontab -e` and insert following line:
```
* * * * * cd /home/axerunner/.axecore/sentinel && ./venv/bin/python bin/sentinel.py 2>&1 >> sentinel-cron.log
```
### Check Sentinel:
```
venv/bin/python bin/sentinel.py
```
If the node still syncing - you will see "axed not synced with network! awaiting full sync before running Sentinel.".

Wait until the process is finished (check status with axe-cli mnsync status). After the wallet will download the blockchain, test Sentinel again with venv/bin/python bin/sentinel.py. If nothing's returned - Sentinel is working properly.

## Start your masternode
Go to your local wallet's masternode tab (on/off switch in Options) and start your masternode.
